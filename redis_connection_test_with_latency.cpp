#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <csignal>
#include <hiredis/hiredis.h>
#include <mutex>

std::atomic<bool> running(true);
std::mutex cout_mutex;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received. Stopping..." << std::endl;
    running = false;
}

void workerThread(const char* host, int port, int desiredRate, std::atomic<int>& totalConnections, std::atomic<double>& totalLatency) {
    std::chrono::steady_clock::time_point start;
    int connections = 0;

    while (running) {
        start = std::chrono::steady_clock::now();
        connections = 0;

        while (connections < desiredRate && running) {
            auto requestStart = std::chrono::high_resolution_clock::now();

            redisContext* context = redisConnect(host, port);
            if (context == nullptr || context->err) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            redisReply* reply = (redisReply*)redisCommand(context, "GET testkey");
            if (reply) {
                freeReplyObject(reply);
            }

            redisFree(context);

            auto requestEnd = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(requestEnd - requestStart).count();

            connections++;
            totalConnections++;

            // Updated atomic addition for totalLatency
            double current = totalLatency.load();
            double desired;
            do {
                desired = current + latency;
            } while (!totalLatency.compare_exchange_weak(current, desired));

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            if (elapsed >= 1) {
                break;
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (elapsed < 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 - elapsed));
        }
    }
}

void testConnection(const char* host, int port, int desiredRate, int numThreads) {
    std::vector<std::thread> threads;
    std::atomic<int> totalConnections(0);
    std::atomic<double> totalLatency(0);

    int ratePerThread = desiredRate / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, host, port, ratePerThread, std::ref(totalConnections), std::ref(totalLatency));
    }

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        int connectionsPerSecond = totalConnections.exchange(0);
        double avgLatency = totalLatency.exchange(0) / connectionsPerSecond;

        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Connections in last second: " << connectionsPerSecond << std::endl;
        std::cout << "Average latency: " << avgLatency << " microseconds" << std::endl;
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <desired_rate> <num_threads>" << std::endl;
        return 1;
    }

    const char* host = argv[1];
    int port = std::stoi(argv[2]);
    int desiredRate = std::stoi(argv[3]);
    int numThreads = std::stoi(argv[4]);

    signal(SIGINT, signalHandler);

    testConnection(host, port, desiredRate, numThreads);

    return 0;
}

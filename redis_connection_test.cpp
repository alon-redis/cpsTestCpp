// TUNE = ulimit -n 1000000; sysctl -w net.ipv4.tcp_fin_timeout=10; sysctl -w net.ipv4.tcp_tw_reuse=1
// INSTALL = apt install g++ libhiredis-dev
// CREATE = pico redis_connection_test.cpp
// COMPILE = g++ -std=c++11 -o redis_connection_test redis_connection_test.cpp -lhiredis -pthread
// USAGE = ./redis_connection_test 10.0.101.127 10000 10000 4


#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <csignal>
#include <hiredis/hiredis.h>
#include <mutex>

const int POOL_SIZE = 10;
std::atomic<bool> running(true);
std::mutex cout_mutex;

class ConnectionPool {
private:
    std::vector<redisContext*> pool;
    const char* host;
    int port;

public:
    ConnectionPool(const char* h, int p, int size) : host(h), port(p) {
        for (int i = 0; i < size; ++i) {
            redisContext* context = redisConnect(host, port);
            if (context == nullptr || context->err) {
                std::cerr << "Error creating connection " << i << ": "
                          << (context ? context->errstr : "Can't allocate Redis context") << std::endl;
            } else {
                pool.push_back(context);
            }
        }
    }

    ~ConnectionPool() {
        for (auto context : pool) {
            redisFree(context);
        }
    }

    redisContext* getConnection() {
        if (pool.empty()) {
            return nullptr;
        }
        redisContext* context = pool.back();
        pool.pop_back();
        return context;
    }

    void returnConnection(redisContext* context) {
        if (context != nullptr) {
            pool.push_back(context);
        }
    }
};

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received. Stopping..." << std::endl;
    running = false;
}

void workerThread(const char* host, int port, int desiredRate, std::atomic<int>& totalConnections) {
    std::chrono::steady_clock::time_point start;
    int connections = 0;

    while (running) {
        start = std::chrono::steady_clock::now();
        connections = 0;

        while (connections < desiredRate && running) {
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
            connections++;
            totalConnections++;

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

    int ratePerThread = desiredRate / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, host, port, ratePerThread, std::ref(totalConnections));
    }

    int seconds = 0;
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        seconds++;

        int connectionsPerSecond = totalConnections.exchange(0);

        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Connections in last second: " << connectionsPerSecond << std::endl;
        std::cout << "Average connections per second: " << (connectionsPerSecond / seconds) << std::endl;
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

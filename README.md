# Redis Connection Test

## Description
This C++ application tests the rate of connection establishment and simple GET operations with a Redis server. It uses multi-threading to achieve high connection rates and provides real-time statistics on connections per second.

## Features
- Multi-threaded connection testing
- Configurable connection rate and thread count
- Real-time statistics output
- Graceful shutdown with Ctrl+C

## Prerequisites
- C++11 compatible compiler and hiredis library
- Redis server
```
apt install g++ libhiredis-dev
```

## OS tuning
- tune the OS configuration
```
sudo ulimit -n 1000000
sudo sysctl -w net.ipv4.tcp_fin_timeout=10
sudo sysctl -w net.ipv4.tcp_tw_reuse=1
```
  
## Compilation
Compile the application using the following command:

```
g++ -std=c++11 -o redis_connection_test redis_connection_test.cpp -lhiredis -pthread
```

## Usage
Run the compiled program with the following command-line arguments:

```
./redis_connection_test    
```

- ``: Redis server hostname or IP address
- ``: Redis server port
- ``: Target number of connections per second
- ``: Number of worker threads to use

## Example
```
./redis_connection_test localhost 6379 10000 4
```

This will attempt to establish 10,000 connections per second to a Redis server running on localhost:6379, using 4 worker threads.

## Output
The program will continuously output:
- Number of connections established in the last second

Press Ctrl+C to stop the test and exit the program.

## Note
This application creates and closes a new connection for each operation, which may impact performance. It's designed for testing connection establishment rates rather than optimal Redis usage in production environments.
```

This README provides a concise overview of the application, its features, how to compile and use it, and what to expect from its output. It also includes a note about the connection handling strategy used in the application.

---

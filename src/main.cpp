#include "zmq_server.hpp"
#include "deps/cppzmq/zmq.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sys/eventfd.h>
#include <unistd.h>
#ifdef ENABLE_TRACY
#include <tracy/Tracy.hpp>
#endif

int g_shutdown_fd = -1;

void signal_handler(int) {
    if (g_shutdown_fd != -1) {
        uint64_t val = 1;
        write(g_shutdown_fd, &val, sizeof(val));
    }
}

int main(int argc, char* argv[]) {
#ifdef ENABLE_TRACY
    ZoneScoped;
#endif
    // Create eventfd for shutdown signaling
    g_shutdown_fd = eventfd(0, EFD_NONBLOCK);
    if (g_shutdown_fd == -1) {
        std::cerr << "Failed to create eventfd: " << strerror(errno) << std::endl;
        return 1;
    }

    // Install signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    bool benchmark = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--benchmark") {
            benchmark = true;
        }
    }

    try {
        // Configuration from environment variables
        const char* pub_endpoint = std::getenv("PUB_ENDPOINT") ? std::getenv("PUB_ENDPOINT") : "tcp://*:5556";
        const char* sub_endpoint = std::getenv("SUB_ENDPOINT") ? std::getenv("SUB_ENDPOINT") : "tcp://localhost:5555";

        // Initialize and run the server
        ZmqServer server(pub_endpoint, sub_endpoint, benchmark, g_shutdown_fd);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        close(g_shutdown_fd);
        return 1;
    }

    close(g_shutdown_fd);
    std::cout << "Server shut down gracefully" << std::endl;
    return 0;
}
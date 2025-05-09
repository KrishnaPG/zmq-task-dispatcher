#include "headers.hpp"

volatile bool gbShouldExit = false;
static zmq::socket_t g_shutdown_signaler;

bool shouldExit() { return  gbShouldExit; }

void setup_shutdown_handlers(zmq::context_t& zmq_ctx)
{
    TRACY_ZONE;

    // Create pair socket for shutdown signaling
    g_shutdown_signaler = zmq::socket_t(zmq_ctx, ZMQ_PAIR);
    g_shutdown_signaler.connect(SHUTDOWN_INPROC_ADDR);
    g_shutdown_signaler.set(zmq::sockopt::linger, 0);

    // Install signal handlers
    std::signal(SIGABRT, request_shutdown);
    std::signal(SIGINT, request_shutdown);
    std::signal(SIGSEGV, request_shutdown);
    std::signal(SIGTERM, request_shutdown);
}

void request_shutdown(int sig)
{
    std::cout << "Signal [" << sig << "] received" << std::endl;
    if (gbShouldExit == false)
    {
        gbShouldExit = true;
        g_shutdown_signaler.send(zmq::message_t("1", 1), zmq::send_flags::dontwait);
        g_shutdown_signaler.close();
    }
}
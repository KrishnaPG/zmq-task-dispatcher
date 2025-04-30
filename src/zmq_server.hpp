#pragma once
#include "message_dispatcher.hpp"
#include "deps/cppzmq/zmq.hpp"
#include "deps/thread-pool/BS_thread_pool.hpp"
#include <memory>
#include <string>

class ZmqServer {
public:
    ZmqServer(const std::string& pub_endpoint, const std::string& sub_endpoint, bool benchmark, int shutdown_fd);
    void run();
    using TThreadPool = BS::thread_pool<>;
private:
    zmq::context_t context_;
    zmq::socket_t pub_socket_;
    zmq::socket_t sub_socket_;
    std::unique_ptr<TThreadPool> thread_pool_;
    std::unique_ptr<MessageDispatcher> dispatcher_;
    bool benchmark_;
    int shutdown_fd_;
};
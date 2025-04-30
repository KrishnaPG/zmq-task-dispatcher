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
    zmq::context_t m_zmqCtx;
    zmq::socket_t m_zmqPub;
    zmq::socket_t m_zmqSub;
    std::unique_ptr<TThreadPool> m_spThreadPool;
    std::unique_ptr<MessageDispatcher> m_spDispatcher;
    bool benchmark_;
    int m_fdShutdown;
};
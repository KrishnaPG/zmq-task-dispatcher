#pragma once
#include "deps/cppzmq/zmq.hpp"
#include <vector>
#include <thread>
#include <functional>
#include <stdexcept>

class WorkerPool {
public:
    WorkerPool(zmq::context_t& ctx, size_t num_workers);
    ~WorkerPool();
    void dispatch(Message&& message);

private:
    void worker_thread();

    zmq::context_t& m_zmqCtx;
    zmq::socket_t m_zmqRouter;
    std::vector<std::thread> m_workerThreads;
    volatile bool m_bShouldExit{false};
};
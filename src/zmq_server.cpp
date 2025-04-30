#include "zmq_server.hpp"
#include "deps/simdjson-3.10.1/singleheader/simdjson.h"
#include <io.h>
#include <fcntl.h>
#ifdef ENABLE_TRACY
#include <tracy/Tracy.hpp>
#endif

ZmqServer::ZmqServer(const std::string& pub_endpoint, const std::string& sub_endpoint, bool benchmark, int shutdown_fd)
    : m_zmqCtx(1), m_zmqPub(m_zmqCtx, ZMQ_PUB), m_zmqSub(m_zmqCtx, ZMQ_SUB), benchmark_(benchmark), m_fdShutdown(shutdown_fd) {
#ifdef ENABLE_TRACY
    ZoneScoped;
#endif
    // Configure sockets
    m_zmqPub.bind(pub_endpoint);
    m_zmqSub.connect(sub_endpoint);
    m_zmqSub.set(zmq::sockopt::subscribe, "");

    // Initialize thread pool and dispatcher
    m_spThreadPool = std::make_unique<TThreadPool>(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4);
    m_spDispatcher = std::make_unique<MessageDispatcher>(m_zmqPub, benchmark_);
}

// we prefer zmq::poll to wait indefinitely. But that is not supported on Windows.
// Hence for windows, we use timeout. For Linux system, the poll() waits indefinitely.
#ifdef _WIN32
#define TIMEOUT std::chrono::milliseconds(1000)
#else 
#define TIMEOUT -1
#endif

void ZmqServer::run() {
#ifdef ENABLE_TRACY
    ZoneScoped;
#endif
    zmq::pollitem_t items[] = {
        {m_zmqSub, 0, ZMQ_POLLIN, 0},
        {nullptr, m_fdShutdown, ZMQ_POLLIN, 0}
    };
    simdjson::ondemand::parser parser;

    while (true) {
        try {        
            // Waits indefinitely or till timeout
            zmq::poll(items, 2, TIMEOUT);
        } catch (const zmq::error_t& e) {
            m_spDispatcher->send_error(-1, -32000, "Poll error: " + std::string(e.what()));
            continue;
        }

        // Check for shutdown
        if (items[1].revents & ZMQ_POLLIN) {
            uint64_t val;
            _read(m_fdShutdown, &val, sizeof(val));
            break;
        }

        // Process incoming message
        if (items[0].revents & ZMQ_POLLIN) {
#ifdef ENABLE_TRACY
            ZoneScopedN("ReceiveMessage");
#endif
            zmq::message_t msg;
            if (!m_zmqSub.recv(msg)) {
                m_spDispatcher->send_error(-1, -32000, "Receive error");
                continue;
            }

            std::string_view data(static_cast<const char*>(msg.data()), msg.size());
            simdjson::padded_string padded(data);
            simdjson::ondemand::document doc;
            if (parser.iterate(padded).get(doc)) {
                m_spDispatcher->send_error(-1, -32700, "Parse error");
                continue;
            }

            m_spThreadPool->submit_task([this, doc = std::move(doc)]() mutable {
                m_spDispatcher->process_request(std::move(doc));
            });
        }
    }

    // Graceful shutdown: wait for all tasks to complete
    m_spThreadPool->wait();
}
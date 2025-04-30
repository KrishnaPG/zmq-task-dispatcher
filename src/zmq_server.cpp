#include "zmq_server.hpp"
#include "deps/simdjson-3.10.1/singleheader/simdjson.h"
#include <io.h>
#include <fcntl.h>
#ifdef ENABLE_TRACY
#include <tracy/Tracy.hpp>
#endif

ZmqServer::ZmqServer(const std::string& pub_endpoint, const std::string& sub_endpoint, bool benchmark, int shutdown_fd)
    : context_(1), pub_socket_(context_, ZMQ_PUB), sub_socket_(context_, ZMQ_SUB), benchmark_(benchmark), shutdown_fd_(shutdown_fd) {
#ifdef ENABLE_TRACY
    ZoneScoped;
#endif
    // Configure sockets
    pub_socket_.bind(pub_endpoint);
    sub_socket_.connect(sub_endpoint);
    sub_socket_.set(zmq::sockopt::subscribe, "");

    // Initialize thread pool and dispatcher
    thread_pool_ = std::make_unique<TThreadPool>(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4);
    dispatcher_ = std::make_unique<MessageDispatcher>(pub_socket_, benchmark_);
}

void ZmqServer::run() {
#ifdef ENABLE_TRACY
    ZoneScoped;
#endif
    zmq::pollitem_t items[] = {
        {sub_socket_, 0, ZMQ_POLLIN, 0},
        {nullptr, shutdown_fd_, ZMQ_POLLIN, 0}
    };
    simdjson::ondemand::parser parser;

    while (true) {
        // Poll indefinitely
        try {
            zmq::poll(items, 2, -1);
        } catch (const zmq::error_t& e) {
            dispatcher_->send_error(-1, -32000, "Poll error: " + std::string(e.what()));
            continue;
        }

        // Check for shutdown
        if (items[1].revents & ZMQ_POLLIN) {
            uint64_t val;
            read(shutdown_fd_, &val, sizeof(val));
            break;
        }

        // Process incoming message
        if (items[0].revents & ZMQ_POLLIN) {
#ifdef ENABLE_TRACY
            ZoneScopedN("ReceiveMessage");
#endif
            zmq::message_t msg;
            if (!sub_socket_.recv(msg)) {
                dispatcher_->send_error(-1, -32000, "Receive error");
                continue;
            }

            std::string_view data(static_cast<const char*>(msg.data()), msg.size());
            simdjson::padded_string padded(data);
            simdjson::ondemand::document doc;
            if (parser.iterate(padded).get(doc)) {
                dispatcher_->send_error(-1, -32700, "Parse error");
                continue;
            }

            thread_pool_->submit_task([this, doc = std::move(doc)]() mutable {
                dispatcher_->process_request(doc);
            });
        }
    }

    // Graceful shutdown: wait for all tasks to complete
    thread_pool_->wait();
}
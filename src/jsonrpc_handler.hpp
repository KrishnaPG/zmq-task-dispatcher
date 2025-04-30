#pragma once
#include "deps/cppzmq/zmq.hpp"
#include "deps/simdjson-3.10.1/singleheader/simdjson.h"
#include <string>

class JsonRpcHandler {
public:
    JsonRpcHandler(zmq::socket_t& pub_socket, bool benchmark);
    void handle_launch_pipeline(simdjson::ondemand::value params);
    void handle_stop_pipeline(simdjson::ondemand::value params);
    void send_error(int64_t id, int code, const std::string& message);
    void send_log(const std::string& level, const std::string& message);

private:
    void send_response(int64_t id, const std::string& result);
    zmq::socket_t& m_zmqPub;
    bool benchmark_;
};
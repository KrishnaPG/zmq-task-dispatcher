#pragma once
#include "jsonrpc_handler.hpp"
#include "deps/cppzmq/zmq.hpp"
#include "deps/simdjson-3.10.1/singleheader/simdjson.h"
#include <unordered_map>
#include <string_view>
#include <functional>

class MessageDispatcher {
public:
    MessageDispatcher(zmq::socket_t& pub_socket, bool benchmark);
    void process_request(simdjson::ondemand::document& request);
    void send_error(int64_t id, int code, const std::string& message);

private:
    using HandlerFunc = std::function<void(simdjson::ondemand::document&, JsonRpcHandler&)>;
    static const std::unordered_map<std::string_view, HandlerFunc> handlers_;
    JsonRpcHandler handler_;
    bool benchmark_;
};
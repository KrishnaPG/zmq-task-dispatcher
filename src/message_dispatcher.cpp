#include "message_dispatcher.hpp"
#include "utils.hpp"
#ifdef ENABLE_TRACY
#include <tracy/Tracy.hpp>
#endif

const std::unordered_map<std::string_view, MessageDispatcher::HandlerFunc> MessageDispatcher::handlers_ = {
    {"launchPipeline", [](simdjson::ondemand::document& request, JsonRpcHandler& handler) {
        simdjson::ondemand::value params = request.find_field("params");
        handler.handle_launch_pipeline(params);
    }},
    {"stopPipeline", [](simdjson::ondemand::document& request, JsonRpcHandler& handler) {
        simdjson::ondemand::value params = request.find_field("params");
        handler.handle_stop_pipeline(params);
    }}
    // Add more handlers here
};

MessageDispatcher::MessageDispatcher(zmq::socket_t& pub_socket, bool benchmark)
    : handler_(pub_socket, benchmark), benchmark_(benchmark) {}

void MessageDispatcher::process_request(simdjson::ondemand::document& request) {
#ifdef ENABLE_TRACY
    ZoneScopedN("ProcessRequest");
#endif
    // Validate JSONRPC
    std::string_view jsonrpc;
    if (request["jsonrpc"].get_string().get(jsonrpc) || jsonrpc != "2.0") {
        send_error(-1, -32600, "Invalid JSONRPC version");
        return;
    }

    // Get method and ID
    std::string_view method;
    int64_t id;
    if (request["method"].get_string().get(method) || request["id"].get_int64().get(id)) {
        send_error(-1, -32600, "Missing method or ID");
        return;
    }

    // Dispatch to handler
    auto it = handlers_.find(method);
    if (it != handlers_.end()) {
        try {
            it->second(request, handler_);
        } catch (const std::exception& e) {
            send_error(id, -32000, "Handler error: " + std::string(e.what()));
        }
    } else {
        send_error(id, -32601, "Method not found");
    }
}

void MessageDispatcher::send_error(int64_t id, int code, const std::string& message) {
    handler_.send_error(id, code, message);
}
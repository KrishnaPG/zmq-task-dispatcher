#include "jsonrpc_handler.hpp"
#include "utils.hpp"
#include "deps/simdjson-3.10.1/singleheader/simdjson.h"
#include <chrono>
#include <sstream>
#ifdef ENABLE_TRACY
#include <tracy/Tracy.hpp>
#endif

using namespace std::chrono;

JsonRpcHandler::JsonRpcHandler(zmq::socket_t& pub_socket, bool benchmark)
    : pub_socket_(pub_socket), benchmark_(benchmark)
{ }

void JsonRpcHandler::handle_launch_pipeline(simdjson::ondemand::value params)
{
#ifdef ENABLE_TRACY
    ZoneScopedN("LaunchPipeline");
#endif
    auto start = steady_clock::now();
    int64_t id;
    params["id"].get_int64().get(id);
    std::string_view pipeline, transport, streamId;
    params["pipeline"].get_string().get(pipeline);
    params["transport"].get_string().get(transport);
    params["streamId"].get_string().get(streamId);

    // Process pipeline
    if (transport != "zeromq")
    {
        send_error(id, -32000, "Unsupported transport: " + std::string(transport));
        return;
    }
    send_log("INFO", "Launching pipeline: " + std::string(pipeline) + " for stream " + std::string(streamId));
    std::ostringstream oss;
    oss << "{\"status\":\"success\",\"streamId\":\"" << streamId << "\",\"details\":\"Pipeline launched\"}";
    send_response(id, oss.str());

    if (benchmark_)
    {
        auto duration = duration_cast<microseconds>(steady_clock::now() - start).count();
        send_log("INFO", "launchPipeline latency: " + std::to_string(duration) + "us");
    }
}

void JsonRpcHandler::handle_stop_pipeline(simdjson::ondemand::value params)
{
#ifdef ENABLE_TRACY
    ZoneScopedN("StopPipeline");
#endif
    auto start = steady_clock::now();
    int64_t id;
    params["id"].get_int64().get(id);
    std::string_view streamId;
    params["streamId"].get_string().get(streamId);

    // Process pipeline
    send_log("INFO", "Stopping pipeline for stream " + std::string(streamId));
    std::ostringstream oss;
    oss << "{\"status\":\"success\",\"streamId\":\"" << streamId << "\",\"details\":\"Pipeline stopped\"}";
    send_response(id, oss.str());

    if (benchmark_)
    {
        auto duration = duration_cast<microseconds>(steady_clock::now() - start).count();
        send_log("INFO", "stopPipeline latency: " + std::to_string(duration) + "us");
    }
}

void JsonRpcHandler::send_response(int64_t id, const std::string& result)
{
#ifdef ENABLE_TRACY
    ZoneScopedN("SendResponse");
#endif
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"result\":" << result << "}";
    utils::publish_message(pub_socket_, oss.str());
}

void JsonRpcHandler::send_error(int64_t id, int code, const std::string& message)
{
#ifdef ENABLE_TRACY
    ZoneScopedN("SendError");
#endif
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"error\":{\"code\":" << code << ",\"message\":\"" << message << "\"}}";
    utils::publish_message(pub_socket_, oss.str());
}

void JsonRpcHandler::send_log(const std::string& level, const std::string& message)
{
#ifdef ENABLE_TRACY
    ZoneScopedN("SendLog");
#endif
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\",\"method\":\"log\",\"params\":{\"level\":\"" << level << "\",\"message\":\"" << message << "\"}}";
    utils::publish_message(pub_socket_, oss.str());
}
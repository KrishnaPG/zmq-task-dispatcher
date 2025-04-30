#pragma once
#include "deps/cppzmq/zmq.hpp"
#include <functional>
#include <chrono>
#include <thread>

namespace utils
{
    inline void publish_message(zmq::socket_t& socket, const std::string& data)
    {
        zmq::message_t msg(data);
        socket.send(msg, zmq::send_flags::none);
    }

    template <typename Func>
    std::optional<std::string> retry(Func&& func, const std::function<void(const std::string&)>& log_error)
    {
        constexpr int max_attempts = 3;
        constexpr std::chrono::milliseconds base_delay(1);
        for (int attempt = 1; attempt <= max_attempts; ++attempt)
        {
            if (auto result = func())
            {
                return result;
            }
            else
            {
                std::string error = result.value_or("Unknown error");
                log_error("Attempt " + std::to_string(attempt) + " failed: " + error);
                if (attempt == max_attempts)
                {
                    return std::nullopt;
                }
                auto delay = base_delay * (1 << (attempt - 1)); // Exponential backoff
                std::this_thread::sleep_for(delay);
            }
        }
        return std::nullopt;
    }
}
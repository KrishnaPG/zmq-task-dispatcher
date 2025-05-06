#pragma once
#include <zmq.hpp>
#include <string_view>
#include <variant>

/**
    These are sample message structures. You can define your own.
 */

enum class MessageType : uint8_t
{
    AUDIO,
    VIDEO,
    CONTROL,
    SHUTDOWN
};

struct AudioPayload
{
    int32_t sample_rate = 0;
    std::string_view data; // variable length payload should come last

    // Zero-copy construction from zmq::message_t
    static AudioPayload from_zmq_msg(const zmq::message_t& msg)
    {
        constexpr size_t METADATA_SIZE = sizeof(AudioPayload::sample_rate);
        assert(msg.size() >= METADATA_SIZE);  // Optional runtime check

        const char* ptr = static_cast<const char*>(msg.data());
        return {
            *std::bit_cast<decltype(AudioPayload::sample_rate)*>(ptr),
            std::string_view(ptr + METADATA_SIZE, msg.size() - METADATA_SIZE),
        };
    }
};

struct VideoPayload
{
    int32_t width;
    int32_t height;
    std::string_view data;  // variable length payload should come last

    static VideoPayload from_zmq_msg(const zmq::message_t& msg)
    {
        constexpr size_t METADATA_SIZE = sizeof(VideoPayload::width) + sizeof(VideoPayload::height);
        assert(msg.size() >= METADATA_SIZE);  // Optional runtime check

        const char* ptr = static_cast<const char*>(msg.data());
        return {
            *std::bit_cast<decltype(VideoPayload::width)*>(ptr),
            *std::bit_cast<decltype(VideoPayload::height)*>(ptr + sizeof(VideoPayload::width)),
            std::string_view(ptr + METADATA_SIZE, msg.size() - METADATA_SIZE),
        };
    }
};

struct ControlPayload
{
    std::string_view command;

    static ControlPayload from_zmq_msg(const zmq::message_t& msg)
    {
        return {
            std::string_view(static_cast<const char*>(msg.data()), msg.size())
        };
    }
};

// Message variant
using PayloadVariant = std::variant<AudioPayload, VideoPayload, ControlPayload>;

struct Message
{
    MessageType type;
    PayloadVariant payload;
    zmq::message_t raw_msg; // Maintains ownership for zero-copy
};

Message parseMessage(zmq::message_t&& msg);
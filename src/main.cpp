#include <string_view>
#include <iostream>
#include <variant>
#include <stdexcept>
#include <zmq.hpp>
#include "shutdown.hpp"
#include "tracer.hpp"

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
    int32_t sample_rate;
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

// Function prototypes (inline for performance)
inline void processAudio(const AudioPayload& payload)
{
     std::cout << "Processing audio: sampleRate=" << payload.sample_rate
        << ", data=" << payload.data << '\n';
}

inline void processVideo(const VideoPayload& payload)
{
    std::cout << "Processing video: width=" << payload.width
        << ", height=" << payload.height
        << ", data=" << payload.data << '\n';
}

inline void processControl(const ControlPayload& payload)
{
    std::cout << "Processing control: command=" << payload.command << '\n';
}

// Parse message with zero-copy
Message parseMessage(zmq::message_t&& msg)
{
    const char* buffer = static_cast<const char*>(msg.data());
    size_t size = msg.size();

    if (size < 1)
    {
        throw std::runtime_error("Invalid message: too short");
    }

    MessageType type = static_cast<MessageType>(buffer[0]);
    return Message { type, {}, std::move(msg) };

    //switch (type)
    //{
    //case MessageType::AUDIO:
    //{
    //    if (size < 5) throw std::runtime_error("Invalid audio message");
    //    int32_t sampleRate(buffer + 1, sizeof(int32_t));
    //    int32_t data(buffer + 5, size - 5);
    //    return { type, AudioPayload{sampleRate, data}, std::move(msg) };
    //}
    //case MessageType::VIDEO:
    //{
    //    if (size < 9) throw std::runtime_error("Invalid video message");
    //    std::string_view width(buffer + 1, sizeof(int32_t));
    //    std::string_view height(buffer + 5, sizeof(int32_t));
    //    std::string_view data(buffer + 9, size - 9);
    //    return { type, VideoPayload{width, height, data}, std::move(msg) };
    //}
    //case MessageType::CONTROL:
    //{
    //    std::string_view command(buffer + 1, size - 1);
    //    return { type, ControlPayload{command}, std::move(msg) };
    //}
    //default:
    //    throw std::runtime_error("Unknown message type");
    //}
}

int main()
{
    TRACY_ZONE;

    // Initialize ZeroMQ zmq_ctx with single IO thread
    zmq::context_t zmq_ctx { 1 };

    // setup shutdown signaling
    setup_shutdown_handlers(zmq_ctx);
    
    // setup shutdown signal listener
    zmq::socket_t shutdown_listener(zmq_ctx, ZMQ_PAIR);
    shutdown_listener.bind(SHUTDOWN_INPROC_ADDR);
    shutdown_listener.set(zmq::sockopt::linger, 0);

    // setup Command listener
    zmq::socket_t subscriber(zmq_ctx, ZMQ_SUB);
    // Set socket options for performance
    subscriber.set(zmq::sockopt::rcvbuf, 1024 * 1024);  // 1MB receive buffer
    subscriber.set(zmq::sockopt::rcvhwm, 1000);         // High-water mark
    subscriber.set(zmq::sockopt::linger, 0);            // after close, die immediately
    subscriber.connect("tcp://localhost:5555");
    subscriber.set(zmq::sockopt::subscribe, "");        // receive all topics

    std::cout << "Server started listening for commands" << std::endl;

    // Polling items
    std::vector<zmq::pollitem_t> items = { 
        {subscriber, 0, ZMQ_POLLIN, 0}, 
        {shutdown_listener, 0, ZMQ_POLLIN, 0},
    };

    while (shouldExit() == false)
    {
        try
        {
            // Wait indefinitely either till a message or SIG event received
            zmq::poll(items, std::chrono::milliseconds { -1 });

            // Check for shutdown
            if (items[1].revents & ZMQ_POLLIN)
                break;

            if (items[0].revents & ZMQ_POLLIN)
            {
                // Process all available messages
                while (shouldExit() == false)
                {
                    zmq::message_t msg;
                    auto result = subscriber.recv(msg, zmq::recv_flags::dontwait);
                    if (shouldExit() || !result.has_value())
                    {
                        break; // No more messages
                    }
                    // Parse and dispatch with zero-copy
                    Message message = parseMessage(std::move(msg)); 
                    std::cout << "Received type " << message.raw_msg << std::endl;

                    // TODO: 
                    //  1. send ACK to the sender that we received the message.
                    //  2. send the message to thread pool to get the work done.
                }
            }
        }
        catch (const zmq::error_t& e)
        {
            std::cerr << "ZeroMQ error: " << e.what() << '\n';
            break;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << '\n';
        }
    }

    std::cout << "Server has shut down" << std::endl;
    return 0;
}

/** 
    // Compile-time handler dispatch

    constexpr auto dispatch = [](const Message& message) {
        std::visit(
            [](const auto& payload) {
                if constexpr (std::is_same_v<std::decay_t<decltype(payload)>, AudioPayload>) {
                    processAudio(payload);
                } else if constexpr (std::is_same_v<std::decay_t<decltype(payload)>, VideoPayload>) {
                    processVideo(payload);
                } else {
                    processControl(payload);
                }
            },
            message.payload
        );
    };
*/
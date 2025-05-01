#include <string_view>
#include <iostream>
#include <variant>
#include <stdexcept>
#include "deps/cppzmq/zmq.hpp"
#include "shutdown.hpp"
#include "tracer.hpp"

// Message types
enum class MessageType : uint8_t
{
    AUDIO,
    VIDEO,
    CONTROL
};

// Payload structures (zero-copy)
struct AudioPayload
{
    std::string_view data;      // Variable-length data
    std::string_view sampleRate; // 4-byte field
};

struct VideoPayload
{
    std::string_view data;   // Variable-length data
    std::string_view width;  // 4-byte field
    std::string_view height; // 4-byte field
};

struct ControlPayload
{
    std::string_view command; // Variable-length data
};

// Message variant
using PayloadVariant = std::variant<AudioPayload, VideoPayload, ControlPayload>;

struct Message
{
    MessageType type;
    PayloadVariant payload;
    zmq::message_t raw_msg; // Keep buffer alive
};

// Function prototypes (inline for performance)
inline void processAudio(const AudioPayload& payload)
{
    // Interpret sampleRate as int32_t directly
    if (payload.sampleRate.size() != sizeof(int32_t))
    {
        throw std::runtime_error("Invalid sampleRate size");
    }
    int32_t sampleRate = *reinterpret_cast<const int32_t*>(payload.sampleRate.data());
    std::cout << "Processing audio: sampleRate=" << sampleRate
        << ", data=" << payload.data << '\n';
}

inline void processVideo(const VideoPayload& payload)
{
    // Interpret width and height as int32_t directly
    if (payload.width.size() != sizeof(int32_t) || payload.height.size() != sizeof(int32_t))
    {
        throw std::runtime_error("Invalid width/height size");
    }
    int32_t width = *reinterpret_cast<const int32_t*>(payload.width.data());
    int32_t height = *reinterpret_cast<const int32_t*>(payload.height.data());
    std::cout << "Processing video: width=" << width
        << ", height=" << height
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

    switch (type)
    {
    case MessageType::AUDIO:
    {
        if (size < 5) throw std::runtime_error("Invalid audio message");
        std::string_view sampleRate(buffer + 1, sizeof(int32_t));
        std::string_view data(buffer + 5, size - 5);
        return { type, AudioPayload{data, sampleRate}, std::move(msg) };
    }
    case MessageType::VIDEO:
    {
        if (size < 9) throw std::runtime_error("Invalid video message");
        std::string_view width(buffer + 1, sizeof(int32_t));
        std::string_view height(buffer + 5, sizeof(int32_t));
        std::string_view data(buffer + 9, size - 9);
        return { type, VideoPayload{data, width, height}, std::move(msg) };
    }
    case MessageType::CONTROL:
    {
        std::string_view command(buffer + 1, size - 1);
        return { type, ControlPayload{command}, std::move(msg) };
    }
    default:
        throw std::runtime_error("Unknown message type");
    }
}

int main()
{
    TRACY_ZONE;

    // Initialize ZeroMQ zmq_ctx with single IO thread
    zmq::context_t zmq_ctx { 1 };

    // Create eventfd for shutdown signaling
    setup_shutdown_handlers(zmq_ctx);
    
    // setup shutdown signal listener
    zmq::socket_t shutdown_listener(zmq_ctx, ZMQ_PAIR);
    shutdown_listener.bind(SHUTDOWN_INPROC_ADDR);
    shutdown_listener.set(zmq::sockopt::linger, 0);

    // setup JSONRPC Command listener
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
                    if (shouldExit() || !result.has_value()) break; // No more messages

                    // Parse and dispatch with zero-copy
                    Message message = parseMessage(std::move(msg));
                    //dispatch(message);
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
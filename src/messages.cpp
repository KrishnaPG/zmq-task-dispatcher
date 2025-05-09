#include "headers.hpp"

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

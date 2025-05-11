#pragma once

enum class GSPipelineOp : uint8_t
{
    Start, 
    Pause,
    Resume,
    Stop,
    List,
    StopAll,
    Unknown // dummy sentinel for validation (value < GSPipelineOp::Unknown)
};

struct GStreamPipelineFn
{
    GSPipelineOp op;
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
    //MessageType type;
    PayloadVariant payload;
    zmq::message_t raw_msg; // Maintains ownership for zero-copy
};


// parses and runs the messages received on ZMQ socket from clients.
// Automatically closes the publisher socket and waits for pending 
// tasks at the time of destruction.
class MessageHandler
{
    BS::thread_pool<> m_threadPool;
    zmq::socket_t m_publisher;
public:
    inline MessageHandler(zmq::socket_t&& publisher):
        m_threadPool(std::thread::hardware_concurrency()), 
        m_publisher(std::move(publisher))
    { }
    void handle_incoming_message(zmq::message_t&& msg);
    void sendAck(const ParamsBase*);
    void sendError(const ParamsBase*, zmq::error_t&& err);
    void publish_outgoing_messages();
protected:
    // TODO: replace these with object pool based buffers
    static fmt::memory_buffer ackBuf;
    static fmt::memory_buffer errBuf;
};
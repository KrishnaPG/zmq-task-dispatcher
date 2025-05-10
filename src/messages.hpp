#pragma once
/**
    These are sample message structures. You can define your own.
 */

typedef std::uint64_t   TReqID;
typedef std::uint8_t    TMethodID;
typedef std::uint32_t   TPipelineID;

struct ParamsBase
{
    TReqID req_id;
    TMethodID method_id;
};

struct ParamsEnd
{
    zmq::message_t raw_msg; // Maintains ownership for zero-copy
};

enum class MethodID : TMethodID
{
    GStreamer_Pipeline_Start,
    GStreamer_Pipeline_Pause,
    GStreamer_Pipeline_Resume,
    GStreamer_Pipeline_Stop,
    AUDIO,
    VIDEO,
    CONTROL,
    SHUTDOWN,
    Unknown // dummy sentinel for validation (value < Methods::Unknown)
};

template<MethodID MID = MethodID::Unknown>
struct Payload { };

template<>
struct Payload<MethodID::GStreamer_Pipeline_Start>
{
    std::string_view pipeline_config;
};

template<>
struct Payload<MethodID::GStreamer_Pipeline_Stop>
{
    TPipelineID pipeline_id;
};
template<>
struct Payload<MethodID::GStreamer_Pipeline_Pause>
{
    TPipelineID pipeline_id;
};
template<>
struct Payload<MethodID::GStreamer_Pipeline_Resume>
{
    TPipelineID pipeline_id;
};

template<MethodID MID = MethodID::Unknown>
struct MethodParams : public Payload<MID>, ParamsEnd { };

template<MethodID MID = MethodID::Unknown>
void handleMethod(const MethodParams<MID>& params) { std::cerr << "Unknown Method" << std::endl; }


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
};
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

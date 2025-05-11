#pragma once
/**
    These are sample message structures. You can define your own.
 */

typedef std::uint64_t   TReqID;
typedef std::uint8_t    TMethodID;
typedef std::uint32_t   TPipelineID;

#pragma pack(push, 1) // prevent padding
struct ParamsBase
{
    TReqID req_id;
    TMethodID method_id;
};
#pragma pack(pop) // Resets to default packing

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
void handleMethod(const MethodParams<MID>& params);

// Compile-time checks
static_assert(sizeof(ParamsBase) == 9, "ParamsBase must be exactly 9 bytes");
static_assert(offsetof(ParamsBase, req_id) == 0, "req_id must be at offset 0");
static_assert(offsetof(ParamsBase, method_id) == 8, "method_id must be at offset 8");
// Type size sanity
static_assert(sizeof(uint8_t) == 1, "uint8_t must be 1 byte");
static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 byte");

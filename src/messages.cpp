#include "headers.hpp"

#define RUN_TASK_IN_POOL  \
    std::move_only_function<void()> task = [method_params = std::move(params)]() noexcept \
        { handleMethod(method_params);  };  \
    m_threadPool.detach_task(std::move(task)); // fire and forget


template<>
void handleMethod<MethodID::GStreamer_Pipeline_Start>(const MethodParams<MethodID::GStreamer_Pipeline_Start>& params)
{
    std::cout << "GStreamer_Pipeline_Start" << std::endl;
}

template<>
void handleMethod<MethodID::GStreamer_Pipeline_Stop>(const MethodParams<MethodID::GStreamer_Pipeline_Stop>& params)
{
    std::cout << "GStreamer_Pipeline_Stop" << std::endl;
}

// Parse params with zero-copy and dispatch to the thread-pool for execution
void MessageHandler::handle_incoming_message(zmq::message_t&& msg)
{
    size_t msgSize = msg.size();
    assert(msgSize > sizeof(ParamsBase) && "Message too small");

    const ParamsBase* pParamsBase = static_cast<const ParamsBase*>(msg.data());
    assert(pParamsBase->req_id && "Request ID cannot be NULL");
    assert(pParamsBase->method_id < (TMethodID)MethodID::Unknown && "Invalid Method ID");

    const char* pBuffer = static_cast<const char*>(msg.data());
    const char* payload_start = pBuffer + sizeof(ParamsBase);
    const size_t payload_size = msgSize - sizeof(ParamsBase);

    // TODO: 
    //  1. send ACK to the sender that we received the message.
    this->m_publisher.send();
    //  2. send the message to thread pool to get the work done.
    
    // Create a dispatcher that calls the appropriate handle method
    switch (static_cast<MethodID>(pParamsBase->method_id))
    {
        case MethodID::GStreamer_Pipeline_Start:
        {
            MethodParams<MethodID::GStreamer_Pipeline_Start> params { 
                std::string_view(payload_start, payload_size), 
                std::move(msg)
            };
            RUN_TASK_IN_POOL
            break;
        }
        case MethodID::GStreamer_Pipeline_Stop:
        {
            MethodParams<MethodID::GStreamer_Pipeline_Stop> params {
                *reinterpret_cast<const TPipelineID*>(payload_start),
                std::move(msg)
            };
            RUN_TASK_IN_POOL
                break;
        }
        case MethodID::GStreamer_Pipeline_Pause:
        {
            MethodParams<MethodID::GStreamer_Pipeline_Pause> params {
                *reinterpret_cast<const TPipelineID*>(payload_start),
                std::move(msg)
            };
            RUN_TASK_IN_POOL
                break;
        }
        case MethodID::GStreamer_Pipeline_Resume:
        {
            MethodParams<MethodID::GStreamer_Pipeline_Resume> params {
                *reinterpret_cast<const TPipelineID*>(payload_start),
                std::move(msg)
            };
            RUN_TASK_IN_POOL
                break;
        }
        default:
            assert(false && "Unknown method ID");
            break;
    }
    
    return;
}

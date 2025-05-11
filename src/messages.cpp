#include "headers.hpp"

#define RUN_TASK_IN_POOL  \
    std::move_only_function<void()> task = [method_params = std::move(params)]() noexcept \
        { handleMethod(method_params);  };  \
    m_threadPool.detach_task(std::move(task)); // fire and forget

// static member variables
fmt::memory_buffer MessageHandler::ackBuf;
fmt::memory_buffer MessageHandler::errBuf;

// Parse params with zero-copy and dispatch to the thread-pool for execution
void MessageHandler::handle_incoming_message(zmq::message_t&& msg)
{
    size_t msgSize = msg.size();
    assert(msgSize >= sizeof(ParamsBase) && "Message too small");

    const ParamsBase* pParamsBase = reinterpret_cast<const ParamsBase*>(msg.data());
    assert(pParamsBase->req_id && "Request ID cannot be NULL");
    assert(pParamsBase->method_id < (TMethodID)MethodID::Unknown && "Invalid Method ID");

    const char* pBuffer = static_cast<const char*>(msg.data());
    const char* payload_start = pBuffer + sizeof(ParamsBase);
    const size_t payload_size = msgSize - sizeof(ParamsBase);

    // Steps: 
    //  1. send ACK to the sender that we received the message.
    this->sendAck(pParamsBase);
    //  2. send the message to thread pool to get the work done.

    // TODO: use MPSC + Lock-free Object Pool to send logs/responses from 
    // inside the threads to the main thread.
    // The static buffers will not work across threads.
    
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

void MessageHandler::publish_outgoing_messages()
{
    // TODO: use MPSC + ObjectPool to pump responses from inside
    // the thread pool tasks to here on main thread (to be published
    // on ZMQ_PUB)
}

void no_delete(void* data, void* hint)
{
    // Custom free function that does nothing, preventing deallocation
}

void MessageHandler::sendAck(const ParamsBase* pParamsBase)
{
    // reuse buffer
    MessageHandler::ackBuf.clear();
    fmt::format_to(std::back_inserter(MessageHandler::ackBuf),
        R"({{"jsonrpc":"2.0","ack":1,"id":{}}})",
        pParamsBase->req_id
    );
    // reuse the ackBuf for the zmq::message_t
    zmq::message_t ack(ackBuf.data(), ackBuf.size(), no_delete, nullptr);
    // zero-copy call with async fire and forget mode
    this->m_publisher.send(std::move(ack), zmq::send_flags::dontwait);
}

void MessageHandler::sendError(const ParamsBase* pParamsBase, zmq::error_t&& err)
{
    // reuse buffer
    MessageHandler::errBuf.clear();
    fmt::format_to(std::back_inserter(MessageHandler::errBuf),
        R"({{"jsonrpc":"2.0","id":{},"error":{{ code:{}, message:"{}" }} }})",
        pParamsBase->req_id,
        err.num(),
        err.what()
    );
    // reuse the ackBuf for the zmq::message_t
    zmq::message_t errMsg(errBuf.data(), errBuf.size(), no_delete, nullptr);
    // zero-copy call with async fire and forget mode
    this->m_publisher.send(std::move(errMsg), zmq::send_flags::dontwait);
}
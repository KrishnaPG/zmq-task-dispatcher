#include "worker-pool.hpp"
#include "tracer.hpp"

#define WORKER_POOL_INPROC_ADDR "inproc://workers"

WorkerPool::WorkerPool(zmq::context_t& ctx, size_t num_workers)
    : m_zmqCtx(ctx), m_zmqRouter(m_zmqCtx, ZMQ_ROUTER) {
    TRACY_ZONE;
    m_zmqRouter.set(zmq::sockopt::linger, 0);
    m_zmqRouter.set(zmq::sockopt::sndhwm, 1000); // High-water mark for outbound messages
    m_zmqRouter.bind(WORKER_POOL_INPROC_ADDR);

    for (size_t i = 0; i < num_workers; ++i) {
        m_workerThreads.emplace_back(&WorkerPool::worker_thread, this);
    }
}

WorkerPool::~WorkerPool() {
    TRACY_ZONE;
    m_bShouldExit = true;
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

bool WorkerPool::dispatch(Message&& message) {
    TRACY_ZONE;
    // Non-blocking send (zero-copy using raw_msg)
    zmq::message_t identity(16); // Placeholder for worker identity
    auto res = m_zmqRouter.send(identity, zmq::send_flags::sndmore | zmq::send_flags::dontwait);
    if (!res) {
        std::cerr << "Dispatch error: Failed to send identity, queue full\n";
        return false;
    }
    res = m_zmqRouter.send(zmq::message_t(&message.type, sizeof(message.type)), zmq::send_flags::sndmore | zmq::send_flags::dontwait);
    if (!res) {
        std::cerr << "Dispatch error: Failed to send type, queue full\n";
        return false;
    }
    res = m_zmqRouter.send(message.raw_msg, zmq::send_flags::dontwait);
    if (!res) {
        std::cerr << "Dispatch error: Failed to send payload, queue full\n";
        return false;
    }
    return true;
}

void WorkerPool::worker_thread() {
    TRACY_ZONE;

    zmq::socket_t dealer_socket(m_zmqCtx, ZMQ_DEALER);
    dealer_socket.set(zmq::sockopt::linger, 0);
    dealer_socket.connect(WORKER_POOL_INPROC_ADDR);

    zmq::message_t identity;
    zmq::message_t type_msg;
    zmq::message_t payload_msg;

    while (m_bShouldExit==false) 
    {
        // Receive multi-part message with blocking receives
        auto res = dealer_socket.recv(identity, zmq::recv_flags::none);
        if (!res) {
            std::cerr << "Worker error: Failed to receive identity, retrying\n";
            continue; // Retry on failure
        }
        if(m_bShouldExit) break; // Exit on shutdown

        res = dealer_socket.recv(type_msg, zmq::recv_flags::none);
        if (!res || m_bShouldExit) {
            std::cerr << "Worker error: Received identity but no type, discarding partial message\n";
            continue;
        }
        if(m_bShouldExit) break; // Exit on shutdown

        res = dealer_socket.recv(payload_msg, zmq::recv_flags::none);
        if (!res || m_bShouldExit) {
            std::cerr << "Worker error: Received identity and type but no payload, discarding partial message\n";
            continue;
        }
        if(m_bShouldExit) break; // Exit on shutdown

        // Reconstruct Message
        if (type_msg.size() != sizeof(MessageType)) {
            std::cerr << "Worker error: Invalid type size, discarding message\n";
            continue;
        }
        if(m_bShouldExit) break; // Exit on shutdown

        MessageType type = *static_cast<MessageType*>(type_msg.data());
        Message message{type, {}, std::move(payload_msg)};

        // Re-parse payload (zero-copy)
        switch (type) {
        case MessageType::AUDIO: {
            const char* buffer = static_cast<const char*>(message.raw_msg.data());
            if (message.raw_msg.size() < 5) {
                std::cerr << "Worker error: Invalid audio message size, discarding\n";
                break;
            }
            std::string_view sampleRate(buffer + 1, sizeof(int32_t));
            std::string_view data(buffer + 5, message.raw_msg.size() - 5);
            message.payload = AudioPayload{data, sampleRate};
            processAudio(std::get<AudioPayload>(message.payload));
            break;
        }
        case MessageType::VIDEO: {
            const char* buffer = static_cast<const char*>(message.raw_msg.data());
            if (message.raw_msg.size() < 9) {
                std::cerr << "Worker error: Invalid video message size, discarding\n";
                break;
            }
            std::string_view width(buffer + 1, sizeof(int32_t));
            std::string_view height(buffer + 5, sizeof(int32_t));
            std::string_view data(buffer + 9, message.raw_msg.size() - 9);
            message.payload = VideoPayload{data, width, height};
            processVideo(std::get<VideoPayload>(message.payload));
            break;
        }
        case MessageType::CONTROL: {
            const char* buffer = static_cast<const char*>(message.raw_msg.data());
            if (message.raw_msg.size() < 1) {
                std::cerr << "Worker error: Invalid control message size, discarding\n";
                break;
            }
            std::string_view command(buffer + 1, message.raw_msg.size() - 1);
            message.payload = ControlPayload{command};
            processControl(std::get<ControlPayload>(message.payload));
            break;
        }
        default:
            std::cerr << "Worker error: Unknown message type, discarding\n";
            continue;
        }
    }
}
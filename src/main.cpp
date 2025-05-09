
#include "custom-memory.hpp"  // should come first
#include "headers.hpp"

// Globals
const char* szCmdSubAddress = "tcp://localhost:5555";
const char* szLogPubAddress = "tcp://localhost:5556";

// Initialize ZeroMQ zmq_ctx with single IO thread
static zmq::context_t zmq_ctx { 1 };

// Publishers for Acks, Results etc.. one for each thread
static std::map<decltype(std::this_thread::get_id()), zmq::socket_t> gThread_pub_sockets;

zmq::socket_t create_pub_socket(zmq::context_t& ctx, const char* address, bool bBind)
{
    zmq::socket_t publisher(ctx, ZMQ_PUB);
    // set socket options for performance
    publisher.set(zmq::sockopt::sndbuf, 1024 * 1024);  // 1MB send buffer
    publisher.set(zmq::sockopt::sndhwm, 1000);         // High-water mark
    publisher.set(zmq::sockopt::linger, 0);            // after close, die immediately
    publisher.set(zmq::sockopt::immediate, 1);         // drop messages if client is not fully connected
    bBind ? publisher.bind(address) : publisher.connect(address); // for main thread, we bind, and for other threads we connect
    return std::move(publisher);
}

#define ONEGB	((uint64_t)1 << 30)

int main()
{
    TRACY_ZONE;
    // reserve memory 
    mi_reserve_os_memory(ONEGB, false /*commit*/, true /*allow large*/);

    // setup shutdown signaling
    setup_shutdown_handlers(zmq_ctx);
    
    // setup shutdown signal listener
    zmq::socket_t shutdown_listener(zmq_ctx, ZMQ_PAIR);
    shutdown_listener.bind(SHUTDOWN_INPROC_ADDR);
    shutdown_listener.set(zmq::sockopt::linger, 0);

    // setup Command listener
    zmq::socket_t zmq_cmd_listener(zmq_ctx, ZMQ_SUB);
    // Set socket options for performance
    zmq_cmd_listener.set(zmq::sockopt::rcvbuf, 1024 * 1024);  // 1MB receive buffer
    zmq_cmd_listener.set(zmq::sockopt::rcvhwm, 1000);         // High-water mark
    zmq_cmd_listener.set(zmq::sockopt::linger, 0);            // after close, die immediately
    zmq_cmd_listener.bind(szCmdSubAddress);
    zmq_cmd_listener.set(zmq::sockopt::subscribe, "");        // receive all topics

    // setup Publisher for Acks, Results, Logs and Notifications for the main thread
    zmq::socket_t zmq_ack_publisher = create_pub_socket(zmq_ctx, szLogPubAddress, true);

    // Initialize thread pool with number of hardware threads
    BS::thread_pool pool(1, [](){
        // thread initialization function, runs once for each thread
        gThread_pub_sockets[std::this_thread::get_id()] = std::move(create_pub_socket(zmq_ctx, szLogPubAddress, false));
    });

    std::cout << "Server started listening for commands" << std::endl;

    // Polling items
    std::vector<zmq::pollitem_t> items = { 
        {zmq_cmd_listener, 0, ZMQ_POLLIN, 0}, 
        {shutdown_listener, 0, ZMQ_POLLIN, 0},
    };

    while (shouldExit() == false)
    {
        try
        {
            // Wait indefinitely either till a message or SIG event received
            zmq::poll(items, std::chrono::milliseconds { 1000 });  //zmq_ack_publisher.send(zmq::message_t("From main"), zmq::send_flags::dontwait);

            pool.detach_task([]()
                {
                    // retrieve this thread's pub socket
                    auto iter = gThread_pub_sockets.find(std::this_thread::get_id());
                    assert(iter != gThread_pub_sockets.end());
                    zmq::socket_t& zmq_ack_publisher = iter->second;

                    zmq_ack_publisher.send(zmq::message_t("inside the threAD"), zmq::send_flags::dontwait);
                }); // fire and forget

            // Check for shutdown
            if (items[1].revents & ZMQ_POLLIN)
                break;

            if (items[0].revents & ZMQ_POLLIN)
            {
                // Process all available messages
                while (shouldExit() == false)
                {
                    zmq::message_t msg;
                    auto result = zmq_cmd_listener.recv(msg, zmq::recv_flags::dontwait);
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
                    std::move_only_function<void()> task = [msg = std::move(message)]() noexcept
                        {
                            TRACY_ZONE_NAMED("thread pool task");

                            // retrieve this thread's pub socket
                            auto iter = gThread_pub_sockets.find(std::this_thread::get_id());
                            assert(iter != gThread_pub_sockets.end());
                            zmq::socket_t& zmq_ack_publisher = iter->second;

                            try
                            {
                                std::cout << "Inside Task" << std::endl;
                                zmq_ack_publisher.send(zmq::message_t("Inside Task"), zmq::send_flags::dontwait);
                            }
                            catch (const zmq::error_t& e)
                            {
                                std::cerr << "ZeroMQ error: " << e.what() << '\n';
                            }
                            catch (const std::exception& e)
                            {
                                std::cerr << "Error processing the message: " << e.what() << std::endl;
                            }
                        };
                    try
                    {
                        pool.detach_task(std::move(task)); // fire and forget
                    }
                    catch (const std::exception& e)
                    {
                        std::cerr << "Failed to submit task to thread pool: " << e.what() << std::endl;
                    }
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

    std::cout << "Shutting down, waiting for thread pool to complete" << std::endl;
    pool.wait();

    // print memory usage statistics
    mi_stats_print_out(NULL, NULL);

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
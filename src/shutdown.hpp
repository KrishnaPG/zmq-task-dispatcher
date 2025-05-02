#pragma once
#include <cstdint>
#include <zmq.hpp>

// ZMQ_PAIR sockets connect on this address to signal shutdown
#define SHUTDOWN_INPROC_ADDR "inproc://shutdown"

// returns true if global shutdown signal has been set to TRUE
bool shouldExit();

// sets up the SIG_TERM HANDLERS
void setup_shutdown_handlers(zmq::context_t&);

/**
* Turns the shutdown signal to be ON. 
* The way we handle this: we make ZQM::Poll() wait indefinitely and expect
* an explicit command through PUB/SUB be sent for shutdown (by executing a
* shutdown script externally, which sends the CMD_SHUTDOWN request to this app).
* Forcing a kill through SIGTERM etc., we do not recommend and cannot guarantee
* graceful shutdown, though we have handlers for them. The problem is: based on
* the host operating system the SIG events may or may not be handled on a 
* different thread. If they are not, then there is no one to wakeup the current
* thread to trigger the SEG handler methods (since poll() is blocked). If another
* thread triggers these SIG handlers of ours, then a graceful shutdown will 
* happen. 
* 
* On Linux system we observe when an interrupt is received, ZQM will throw exception
* not giving much chance for the sig handlers' effect to take place (such as shouldExi()
* to kick-in).
* 
* We are optimizing for usual network messages, instead of being ready to be killed in 10ms.
* When someone prefers killing this app, there is nothing to be graceful about it.
*/
void request_shutdown(int);
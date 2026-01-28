#include <cppzmqzoltanext/actor.h>
#include <cppzmqzoltanext/helpers.h>
#include <cppzmqzoltanext/interrupt.h>
#include <cppzmqzoltanext/loop.h>
#include <cppzmqzoltanext/signal.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <zmq.hpp>

using namespace zmqzext;

/**
 * Actor child socket handler
 *
 */
bool actor_socket_handler(loop_t& loop, zmq::socket_ref socket) {
    try {
        // Receive message from parent
        zmq::message_t msg;
        auto result = recv_retry_on_eintr(socket, msg, zmq::recv_flags::dontwait);
        if (!result) {
            return true;  // Continue loop
        }

        auto signal = signal_t::check_signal(msg);
        if (signal && signal->is_stop()) {
            return false;  // Exit loop
        }

        std::cout << "[Actor] Received: " << msg.to_string_view() << std::endl;

        // Echo the message back to parent
        send_retry_on_eintr(socket, msg, zmq::send_flags::none);
    } catch (...) {
        // Ignore exceptions and continue, but usually should communicate them to parent
    }

    return true;  // Continue loop
}

/**
 * Actor user function that runs in a separate thread.
 * Creates its own event loop to handle incoming requests and stop signals.
 */
bool actor_runner(zmq::socket_t& socket) {
    zmq::context_t actor_context;
    loop_t loop;

    std::cout << "[Actor] Started" << std::endl;

    // Register the actor socket to receive messages from parent
    loop.add(socket, actor_socket_handler);

    // Send success signal to parent
    send_retry_on_eintr(socket, signal_t::create_success(), zmq::send_flags::none);

    // Run the event loop
    try {
        loop.run(false);  // Non-interruptible mode, so the actor is stopped only by parent stop signal
    } catch (...) {
        // As the application exceptions were already caught in the socket handler,
        // the exceptions left here should be only ZMQ related. So, we still could
        // try to comunicate the failure to parent and wait to be stopped or return
        // if that fails.
    }

    std::cout << "[Actor] Finished" << std::endl;

    return false;
}

bool parent_api_socket_handler(loop_t& loop, zmq::socket_ref socket, zmq::socket_ref actor_socket) {
    try {
        zmq::message_t msg;

        // Receive echoed message from actor
        auto result = socket.recv(msg, zmq::recv_flags::dontwait);
        if (!result) {
            return false;  // Exit loop
        }

        std::cout << "[Main] Received request. Delivering it to actor: " << msg.to_string_view() << std::endl;

        // Forward the message to the actor
        auto send_result = actor_socket.send(msg, zmq::send_flags::none);
        if (!send_result) {
            return false;  // Exit loop
        }

        // Send reply back to API client
        auto reply_msg = zmq::message_t{std::string{"Ok"}};
        send_result = socket.send(reply_msg, zmq::send_flags::none);
        if (!send_result) {
            return false;  // Exit loop
        }
    } catch (...) {
        return false;  // Exit loop
    }
    return true;  // Continue loop
}

bool parent_actors_socket_handler(loop_t& loop, zmq::socket_ref socket) {
    try {
        zmq::message_t msg;

        // Receive echoed message from actor
        auto result = socket.recv(msg, zmq::recv_flags::dontwait);
        if (!result) {
            return false;  // Exit loop
        }

        std::cout << "[Main] Received from actor: " << msg.to_string_view() << std::endl;
    } catch (...) {
        return false;  // Exit loop
    }
    return true;  // Continue loop
}

int main() {
    std::cout << "[Main] Starting application" << std::endl;

    // Install interrupt handler for graceful shutdown
    install_interrupt_handler();

    try {
        // Create ZMQ context
        zmq::context_t context;

        // Create and start the actor
        std::cout << "[Main] Creating and starting actor" << std::endl;
        actor_t actor(context);
        actor.start(actor_runner);

        // Create a REP socket for request-reply communication
        zmq::socket_t rep_socket(context, zmq::socket_type::rep);
        rep_socket.bind("tcp://127.0.0.1:5555");

        // Create the main event loop
        loop_t loop;

        // Register actor socket handler
        loop.add(actor.socket(), parent_actors_socket_handler);

        // Register REP socket handler
        loop.add(rep_socket, std::bind(parent_api_socket_handler, std::placeholders::_1, std::placeholders::_2,
                                       zmq::socket_ref(actor.socket())));

        // Register a timer to print status every 2 seconds
        loop.add_timer(std::chrono::milliseconds(2000),
                       0,  // Infinite occurrences
                       [](loop_t&, timer_id_t) {
                           std::cout << "[Main] Timer event - application is running" << std::endl;
                           return true;  // Recurring timer
                       });

        std::cout << "[Main] Running loop" << std::endl;

        // Run the event loop
        // The loop will continue until:
        // - A handler returns false
        // - An interrupt signal (Ctrl+C) is received
        // On Windows, we must assure the loop will check for interrupts on a regular basis
        // On linux, the loop will be interrupted by signals automatically when waiting on zmq::poll
        loop.run(true, std::chrono::milliseconds{500});  // interruptible mode with 500ms check interval

        std::cout << "[Main] Loop finished" << std::endl;

        // No need to stop the actor explicitly, as its destructor will handle it
        std::cout << "[Main] Stopping actor" << std::endl;
    } catch (...) {
        return 1;
    }

    std::cout << "[Main] Actor stopped" << std::endl;
    std::cout << "[Main] Application finished" << std::endl;

    return 0;
}

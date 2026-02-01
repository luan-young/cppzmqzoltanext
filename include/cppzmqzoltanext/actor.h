/*
MIT License

Copyright (c) 2025 Luan Young

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/**
 * @file actor.h
 * @brief Actor pattern implementation using ZeroMQ PAIR sockets
 *
 * This module provides the actor_t class, which implements the Actor pattern
 * for concurrent programming. An actor is an independent execution unit
 * (running in its own thread) that runs a user-defined function and
 * communicates with the parent through ZeroMQ PAIR sockets.
 *
 * Thread Safety
 *
 * The actor_t class is NOT thread-safe for most operations. The design aims to
 * avoid memory sharing between the parent and child threads, except for the child
 * socket passed to the user function. The actor_t object and its parent socket
 * should only be accessed from the thread that created the actor. The child
 * socket is passed to the user function and runs in the actor thread. Additional
 * parameters can be passed to the user function via captures in lambdas or
 * functors, but the lifetime of passed parameters must be managed carefully.
 * Usually, parameters should be copied or moved into the user function to avoid
 * dangling references. Pointers where the ownership is transferred to the user
 * function are also acceptable.
 *
 * Initialization Synchronization
 *
 * The start() method blocks until the user function sends either a success or
 * failure signal. This ensures initialization of the actor is synchronized
 * with the calling thread.
 *
 * Exception Handling
 *
 * If an exception is thrown in the user function (other than zmq::error_t)
 * before sending the success signal, it will be captured and re-thrown in
 * the start() method, allowing the parent to handle initialization errors.
 * After sending the success signal, its the user's responsibility to handle
 * exceptions within the user function. Still, the actor_t class will catch
 * unhandled exceptions and silently exit the thread to avoid crashing the application.
 *
 * Finalization Synchronization
 *
 * The user function finalization is requested by the stop() method which can be called
 * explicitly or is implicitly by the destructor. The stop() method sends a stop request and waits
 * for a response signal, with a configurable timeout. It is the user's responsibility
 * to handle the stop request in the user function and exit immediately.
 * Usually, the user function communicates to the parent application that it has finished
 * its work (either by its own logic or by a previous request) and then the parent
 * application destroys the actor, starting the stop synchronization.
 *
 * @details
 * Key features:
 * - Thread-safe concurrent execution with minimal synchronization
 * - Isolated computational units that don't share memory
 * - Message-based communication between parent and child threads
 * - Exception handling and propagation from child to parent during initialization
 * - Automatic cleanup and resource management
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2025 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <zmq.hpp>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Alias for a function type used to define actor behaviors.
 *
 * This type represents a callable object (such as a lambda, function pointer,
 * or functor) that takes a reference to a zmq::socket_t and returns a boolean
 * value.
 *
 * The actor should send a success signal through the provided socket as soon
 * as it has completed its initialization successfully. If initialization fails,
 * it may throw an exception, which will be rethrown in the parent thread, or
 * just return false, which will throw a std::runtime_error in the parent thread.
 *
 * After the actor sends the success signal, it should monitor for stop requests
 * and return immediately when a stop request is received. The return value has
 * no meaning after initialization.
 *
 * @param socket Reference to a ZeroMQ socket used by the actor for
 * communication.
 * @return true to finish with a success signal, false to finish with a failure
 * signal.
 */
using actor_fn_t = std::function<bool(zmq::socket_t&)>;

/**
 * @brief Class that implements the Actor pattern using ZMQ PAIR sockets
 *
 * The Actor provides the functionality of running a user-provided function in a
 * new thread and keeping a pair of zmq sockets for communication between the
 * parent thread and the child thread. It synchronizes the start of the function
 * execution in its start method and the stop of the thread in its stop method.
 */
class CZZE_EXPORT actor_t {
public:
    /**
     * @brief Constructs a new actor_t object
     *
     * Creates a pair of zmq::sockets of type pair, one for the parent side and
     * other for the child. The parent socket is bound to an automatic
     * self-generated address that is unique for each instance. The child socket
     * is connected to the parent's bound address.
     *
     * @param context The ZMQ context to be used for creating the sockets
     */
    explicit actor_t(zmq::context_t& context);

    /**
     * @brief Copy constructor is deleted
     *
     * Actors cannot be copied because they maintain exclusive ownership of the
     * communication sockets and represent a unique execution thread. Copying
     * would violate the single ownership principle and lead to resource
     * conflicts. Use move semantics instead to transfer actor ownership.
     */
    actor_t(actor_t const&) = delete;

    /**
     * @brief Copy assignment operator is deleted
     *
     * Actors cannot be copy-assigned because they maintain exclusive ownership
     * of the communication sockets and represent a unique execution thread.
     * Copying would violate the single ownership principle and lead to resource
     * conflicts. Use move semantics instead to transfer actor ownership.
     */
    actor_t& operator=(actor_t const&) = delete;

    /**
     * @brief Move constructor
     *
     * Transfers ownership of the actor's communication sockets and execution
     * state from the source actor to this newly constructed actor. The actor object
     * can be moved before or after being started. If moved before start(),
     * the new actor can be started normally. If called after start(), the new
     * actor continues the execution of the original actor's thread.
     * The source actor is left in a valid but empty state where is_started() and
     * is_stopped() return true. Calling stop() on the moved-from actor is a no-op
     * so it can be called in ins destructor.
     *
     * @param other The source actor to move from. After the move, other is in
     * a valid but empty state.
     * @note Move operations are noexcept and do not throw exceptions
     */
    actor_t(actor_t&& other) noexcept;

    /**
     * @brief Move assignment operator
     *
     * Transfers ownership of the actor's communication sockets and execution
     * state from the source actor to this actor. The actor object
     * can be moved before or after being started. If moved before start(),
     * the new actor can be started normally. If called after start(), the new
     * actor continues the execution of the original actor's thread.
     * The source actor is left in a valid but empty state where is_started() and
     * is_stopped() return true. Calling stop() on the moved-from actor is a no-op
     * so it can be called in ins destructor.
     *
     * @param other The source actor to move from. After the move, other is in
     * a valid but empty state.
     * @return Reference to this actor
     * @note Move operations are noexcept and do not throw exceptions
     */
    actor_t& operator=(actor_t&& other) noexcept;

    /**
     * @brief Destroys the actor_t object
     *
     * Calls stop with the configurable destructor timeout to avoid blocking forever.
     */
    ~actor_t() noexcept;

    /**
     * @brief Starts the actor thread with the provided function
     *
     * Launches a new thread executing the provided function and blocks until
     * receiving a success or failure signal. On success signal, returns
     * normally. On failure signal, rethrows any saved exception from the
     * execute method or throws std::runtime_error.
     *
     * @param func The function to be executed in the new thread. Must take a
     * zmq::socket_t& and return bool
     * @throws std::runtime_error If the thread was already started or if the
     * function signals failure during function initialization
     * @throws Rethrows any exception caught during function initialization
     */
    void start(actor_fn_t func);

    /**
     * @brief Stops the actor thread
     *
     * Sends a stop request message and waits for a response signal using the
     * provided timeout. If timeout is 0, returns immediately after trying to
     * send the stop request. If timeout is negative, blocks indefinitely
     * waiting for the response.
     *
     * @param timeout_ms The timeout in milliseconds. 0 for non-blocking,
     * negative for infinite
     * @return true if successfully stopped, wasn't started or was already stopped,
     * false if timed out
     */
    bool stop(std::chrono::milliseconds timeout = std::chrono::milliseconds{-1});

    /**
     * @brief Gets the parent socket for external communication
     *
     * @return zmq::socket_t& Reference to the parent socket
     */
    zmq::socket_t& socket() noexcept { return _parent_socket; }

    /**
     * @brief Checks if the actor thread was started
     *
     * @return true if started, false otherwise
     */
    bool is_started() const noexcept { return _started; }

    /**
     * @brief Checks if the actor thread was stopped
     *
     * @return true if stopped, false otherwise
     */
    bool is_stopped() const noexcept { return _stopped; }

    /**
     * @brief Sets the timeout value used in the destructor
     * @param timeout The timeout value in milliseconds
     */
    void set_destructor_timeout(std::chrono::milliseconds timeout) noexcept { _timeout_on_destructor = timeout; }

    /**
     * @brief Gets the current timeout value used in the destructor
     * @return The current timeout value in milliseconds
     */
    std::chrono::milliseconds get_destructor_timeout() const noexcept { return _timeout_on_destructor; }

private:
    /**
     * @brief Default timeout value for destructor in milliseconds
     */
    static constexpr std::chrono::milliseconds DEFAULT_DESTRUCTOR_TIMEOUT{100};

    /**
     * @brief Struct containing exception-related state shared between actor and
     * its execution thread
     */
    struct SharedExceptionState {
        std::mutex exception_mutex;
        std::exception_ptr saved_exception;
    };

    /**
     * @brief Executes the user function and handles signals
     *
     * Calls the user function with the child socket. Monitors execution and
     * sends appropriate signals. Saves any exception thrown during
     * initialization. Takes ownership of the socket and closes it when function
     * finishes.
     *
     * @param func The user function to execute
     * @param socket The socket to use for communication, takes ownership
     * @param exception_state The shared exception state for error handling
     */
    void execute(actor_fn_t func, std::unique_ptr<zmq::socket_t> socket,
                 std::shared_ptr<SharedExceptionState> exception_state) noexcept;

    /**
     * @brief Binds the parent socket to a unique address
     *
     * Attempts to bind the parent socket to a unique address. If the address is
     * already in use, it will retry with a different random suffix until
     * successful.
     *
     * @return The bound address
     */
    std::string bind_to_unique_address();

    zmq::socket_t _parent_socket;
    std::unique_ptr<zmq::socket_t> _child_socket;
    std::shared_ptr<SharedExceptionState> _exception_state;
    bool _started;
    bool _stopped;
    std::chrono::milliseconds _timeout_on_destructor{DEFAULT_DESTRUCTOR_TIMEOUT};
};

}  // namespace zmqzext

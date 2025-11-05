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
 * The boolean return value indicates that the actor should finish with a
 * success signal while false indicates it should finish with a failure signal.
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
     * @brief Destroys the actor_t object
     *
     * Calls stop with a timeout to avoid blocking forever.
     */
    ~actor_t();

    /**
     * @brief Starts the actor thread with the provided function
     *
     * Launches a new thread executing the provided function and blocks until
     * receiving a success or failure signal. On success signal, returns
     * normally. On failure signal, rethrows any saved exception from the
     * execute method or throws a specific exception.
     *
     * @param func The function to be executed in the new thread. Must take a
     * zmq::socket_t& and return bool
     * @throws std::runtime_error If the thread was already started
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
     * @return true if successfully stopped, false if timed out or wasn't
     * started
     */
    bool stop(std::chrono::milliseconds timeout = std::chrono::milliseconds{
                  -1});

    /**
     * @brief Gets the parent socket for external communication
     *
     * @return zmq::socket_t& Reference to the parent socket
     */
    zmq::socket_t& socket();

    /**
     * @brief Checks if the actor thread was started
     *
     * @return true if started, false otherwise
     */
    bool is_started() const;

    /**
     * @brief Checks if the actor thread was stopped
     *
     * @return true if stopped, false otherwise
     */
    bool is_stopped() const;

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
                 std::shared_ptr<SharedExceptionState> exception_state);

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
    std::chrono::milliseconds _timeout_on_destructor{
        DEFAULT_DESTRUCTOR_TIMEOUT};

public:
    /**
     * @brief Sets the timeout value used in the destructor
     * @param timeout The timeout value in milliseconds
     */
    void set_destructor_timeout(std::chrono::milliseconds timeout);

    /**
     * @brief Gets the current timeout value used in the destructor
     * @return The current timeout value in milliseconds
     */
    std::chrono::milliseconds get_destructor_timeout() const;
};

}  // namespace zmqzext

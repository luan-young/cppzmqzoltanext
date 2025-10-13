#include "cppzmqzoltanext/actor.h"

#include <cerrno>
#include <random>
#include <thread>

#include "cppzmqzoltanext/signal.h"

namespace zmqzext {

actor_t::actor_t(zmq::context_t& context)
    : _parent_socket(context, ZMQ_PAIR),
      _child_socket(std::make_unique<zmq::socket_t>(context, ZMQ_PAIR)),
      _exception_state(std::make_shared<SharedExceptionState>()),
      _started(false),
      _stopped(false) {
    // Bind parent socket and connect child socket
    std::string address = bind_to_unique_address();
    _child_socket->connect(address);
}

actor_t::~actor_t() { stop(_timeout_on_destructor); }

void actor_t::start(actor_fn_t func) {
    // if (_started) {
    //     throw std::runtime_error("Actor already started");
    // }

    _started = true;

    // Start the thread and execute the function with shared exception state
    std::thread thread([this, exception_state = _exception_state, func,
                        socket = std::move(_child_socket)]() mutable {
        this->execute(func, std::move(socket), exception_state);
    });
    thread.detach();  // Thread will run independently

    // Wait for success/failure signal
    zmq::message_t msg;
    if (_parent_socket.recv(msg, zmq::recv_flags::none)) {  // blocking
        auto signal = signal_t::check_signal(msg);
        if (signal && signal->is_success()) {
            return;  // Success case
        }
        // Failure case - get exception if any
        _stopped = true;
        std::exception_ptr saved_ex;
        {
            std::lock_guard<std::mutex> lock(_exception_state->exception_mutex);
            saved_ex = _exception_state->saved_exception;
        }
        if (saved_ex) {
            std::rethrow_exception(saved_ex);
        }
        throw std::runtime_error("Actor initialization failed");
    }
    throw std::runtime_error("Failed to receive initialization signal");
}

bool actor_t::stop(
    std::chrono::milliseconds timeout /* = std::chrono::milliseconds{-1}*/) {
    if (!_started || _stopped) {
        return false;
    }

    auto msg_send = signal_t::create_stop();
    auto const result_send =
        _parent_socket.send(msg_send, zmq::send_flags::dontwait);
    if (!result_send) {
        _stopped = true;
        return true;
    }

    // set socket option rcvtimeout
    int timeout_ms =
        (timeout.count() < 0) ? -1 : static_cast<int>(timeout.count());
    _parent_socket.set(zmq::sockopt::rcvtimeo, timeout_ms);

    // Wait for response with timeout
    zmq::message_t msg_recv;
    auto const result_recv =
        _parent_socket.recv(msg_recv, zmq::recv_flags::none);
    if (!result_recv) {
        _stopped = true;
        return false;
    }

    _stopped = true;
    return true;
}

zmq::socket_t& actor_t::socket() { return _parent_socket; }

bool actor_t::is_started() const { return _started; }

bool actor_t::is_stopped() const { return _stopped; }

void actor_t::execute(actor_fn_t func, std::unique_ptr<zmq::socket_t> socket,
                      std::shared_ptr<SharedExceptionState> exception_state) {
    try {
        // Run the user function and monitor its execution
        auto const success = func(*socket);

        // Send success or failure signal based on return value
        auto signal =
            success ? signal_t::create_success() : signal_t::create_failure();
        socket->send(signal, zmq::send_flags::none);
    } catch (zmq::error_t const&) {
    } catch (...) {
        // Save exception to be rethrown in start() if needed
        {
            std::lock_guard<std::mutex> lock(_exception_state->exception_mutex);
            _exception_state->saved_exception = std::current_exception();
        }

        // Send failure signal
        try {
            auto signal = signal_t::create_failure();
            socket->send(signal, zmq::send_flags::none);
        } catch (...) {
            // Ignore exceptions during send in exception handler
        }
    }
    // Always close socket when done
    socket->close();
}

std::string actor_t::bind_to_unique_address() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    std::string base_address =
        "inproc://zmqzext-actor-" +
        std::to_string(reinterpret_cast<uintptr_t>(this));

    while (true) {
        try {
            std::string address = base_address + "-" + std::to_string(dis(gen));
            _parent_socket.bind(address);
            return address;
        } catch (const zmq::error_t& e) {
            if (e.num() != EADDRINUSE) {
                throw;  // Rethrow if it's not an address-in-use error
            }
            // If address is in use, loop will continue and try another random
            // suffix
        }
    }
}
}  // namespace zmqzext

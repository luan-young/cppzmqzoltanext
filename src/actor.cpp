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
 * @file actor.cpp
 * @brief Actor pattern implementation using ZeroMQ PAIR sockets
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2025 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */
#include "cppzmqzoltanext/actor.h"

#include <algorithm>
#include <cerrno>
#include <limits>
#include <random>
#include <thread>

#include "cppzmqzoltanext/helpers.h"
#include "cppzmqzoltanext/signal.h"

namespace zmqzext {

actor_t::actor_t(zmq::context_t& context)
    : _parent_socket(context, ZMQ_PAIR),
      _child_socket(std::make_unique<zmq::socket_t>(context, ZMQ_PAIR)),
      _exception_state(std::make_shared<SharedExceptionState>()),
      _started(false),
      _stopped(false) {
    std::string address = bind_to_unique_address();
    _child_socket->connect(address);
}

actor_t::actor_t(actor_t&& other) noexcept { *this = std::move(other); }

actor_t& actor_t::operator=(actor_t&& other) noexcept {
    _parent_socket = std::move(other._parent_socket);
    _child_socket = std::move(other._child_socket);
    _exception_state = std::move(other._exception_state);
    _started = other._started;
    _stopped = other._stopped;
    _timeout_on_destructor = other._timeout_on_destructor;

    other._started = true;
    other._stopped = true;

    return *this;
}

actor_t::~actor_t() noexcept {
    try {
        stop(_timeout_on_destructor);
    } catch (...) {
    }
}

void actor_t::start(actor_fn_t func) {
    if (_started) {
        throw std::runtime_error("Actor already started");
    }

    std::thread thread([this, exception_state = _exception_state, func, socket = std::move(_child_socket)]() mutable {
        this->execute(func, std::move(socket), exception_state);
    });
    thread.detach();  // Thread will run independently

    _started = true;

    zmq::message_t msg;
    if (recv_retry_on_eintr(_parent_socket, msg, zmq::recv_flags::none)) {  // blocking
        auto signal = signal_t::check_signal(msg);
        if (signal && signal->is_success()) {
            return;  // Success case
        }
        // Failure case - get exception if any
        _stopped = true;
        _parent_socket.close();
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
    _stopped = true;
    _parent_socket.close();
    throw std::runtime_error("Failed to receive initialization signal");
}

bool actor_t::stop(std::chrono::milliseconds timeout /* = std::chrono::milliseconds{-1}*/) {
    if (!_started || _stopped) {
        return true;
    }

    auto msg_send = signal_t::create_stop();
    auto const result_send = send_retry_on_eintr(_parent_socket, msg_send, zmq::send_flags::dontwait);
    if (!result_send) {
        _stopped = true;
        _parent_socket.close();
        return true;
    }

    auto const confined_timeout = std::clamp(timeout, std::chrono::milliseconds{std::numeric_limits<int>::min()},
                                             std::chrono::milliseconds{std::numeric_limits<int>::max()});
    int timeout_ms = (confined_timeout.count() < 0) ? -1 : static_cast<int>(confined_timeout.count());
    auto const start_time = std::chrono::steady_clock::now();

    zmq::message_t msg_recv;
    while (true) {
        _parent_socket.set(zmq::sockopt::rcvtimeo, timeout_ms);
        auto const result_recv = recv_retry_on_eintr(_parent_socket, msg_recv, zmq::recv_flags::none);  // blocking
        if (!result_recv) {
            _stopped = true;
            _parent_socket.close();
            return false;
        }
        if (signal_t::check_signal(msg_recv)) {
            break;
        }
        if (confined_timeout.count() >= 0) {
            auto const time_left = std::chrono::ceil<std::chrono::milliseconds>(
                confined_timeout - (std::chrono::steady_clock::now() - start_time));
            timeout_ms = std::max(static_cast<int>(time_left.count()), 0);
        }
    }

    _stopped = true;
    _parent_socket.close();
    return true;
}

void actor_t::execute(actor_fn_t func, std::unique_ptr<zmq::socket_t> socket,
                      std::shared_ptr<SharedExceptionState> exception_state) noexcept {
    try {
        auto const success = func(*socket);

        auto signal = success ? signal_t::create_success() : signal_t::create_failure();
        send_retry_on_eintr(*socket, signal, zmq::send_flags::none);  // blocking
    } catch (...) {
        // Save exception to be rethrown in start() if needed
        {
            std::lock_guard<std::mutex> lock(_exception_state->exception_mutex);
            _exception_state->saved_exception = std::current_exception();
        }

        try {
            auto signal = signal_t::create_failure();
            send_retry_on_eintr(*socket, signal, zmq::send_flags::none);  // blocking
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
    std::string base_address = "inproc://zmqzext-actor-" + std::to_string(reinterpret_cast<uintptr_t>(this));

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

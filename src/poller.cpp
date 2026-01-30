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
 * @file poller.cpp
 * @brief Event polling for monitoring multiple ZMQ sockets
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2025 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */
#include "cppzmqzoltanext/poller.h"

#include <algorithm>
#include <stdexcept>

#include "cppzmqzoltanext/interrupt.h"

namespace zmqzext {

void poller_t::add(zmq::socket_ref socket) {
    if (!socket) {
        throw std::invalid_argument("Cannot add null socket to poller");
    }

    if (has_socket(socket.handle())) {
        throw std::invalid_argument("Socket already exists in poller");
    }

    _poll_items.push_back({socket.handle(), 0, ZMQ_POLLIN, 0});
}

void poller_t::remove(zmq::socket_ref socket) {
    auto handle = socket.handle();
    _poll_items.erase(std::remove_if(_poll_items.begin(), _poll_items.end(),
                                     [handle](zmq::pollitem_t const& item) { return item.socket == handle; }),
                      _poll_items.end());
}

zmq::socket_ref poller_t::wait(std::chrono::milliseconds timeout /*= std::chrono::milliseconds{-1}*/) {
    if (is_interrupted() && is_interruptible()) {
        _terminated = true;
        return zmq::socket_ref{};
    }
    _terminated = false;
    try {
        auto const n_items = zmq::poll(_poll_items, timeout);
        // interrupt may have happened between is_interrupted() and poll() calls
        // in that case, the poll does not throw with EINTR
        // then, we check if interrupted before processing results
        if (is_interrupted() && is_interruptible()) {
            _terminated = true;
            return zmq::socket_ref{};
        }
        if (n_items > 0) {
            for (std::size_t i = 0; i < _poll_items.size(); ++i) {
                if (_poll_items[i].revents == ZMQ_POLLIN) {
                    return zmq::socket_ref{zmq::from_handle, _poll_items[i].socket};
                }
            }
        }
    } catch (zmq::error_t const& e) {
        auto const error = e.num();
        if (error == EINTR) {
            if (is_interruptible()) {
                _terminated = true;
            }
        } else if (error == ETERM) {
            _terminated = true;
        } else {
            throw;
        }
    }
    return zmq::socket_ref{};
}

std::vector<zmq::socket_ref> poller_t::wait_all(std::chrono::milliseconds timeout /*= std::chrono::milliseconds{-1}*/) {
    std::vector<zmq::socket_ref> result{};
    if (is_interrupted() && is_interruptible()) {
        _terminated = true;
        return result;
    }
    _terminated = false;
    try {
        auto const n_items = zmq::poll(_poll_items, timeout);
        // interrupt may have happened between is_interrupted() and poll() calls
        // in that case, the poll does not throw with EINTR
        // then, we check if interrupted before processing results
        if (is_interrupted() && is_interruptible()) {
            _terminated = true;
            return result;
        }
        if (n_items > 0) {
            result.reserve(n_items);
            for (std::size_t i = 0; i < _poll_items.size(); ++i) {
                if (_poll_items[i].revents == ZMQ_POLLIN) {
                    result.emplace_back(zmq::socket_ref{zmq::from_handle, _poll_items[i].socket});
                }
            }
        }
    } catch (zmq::error_t const& e) {
        auto const error = e.num();
        if (error == EINTR) {
            if (is_interruptible()) {
                _terminated = true;
            }
        } else if (error == ETERM) {
            _terminated = true;
        } else {
            throw;
        }
    }
    return result;
}

bool poller_t::has_socket(void* socket_handle) const {
    return std::any_of(_poll_items.begin(), _poll_items.end(),
                       [socket_handle](const zmq::pollitem_t& item) { return item.socket == socket_handle; });
}

}  // namespace zmqzext

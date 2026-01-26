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
 * @file helpers.cpp
 * @brief Helper utilities for robust ZMQ message sending and receiving
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2025 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */
#include "cppzmqzoltanext/helpers.h"

#include <cerrno>

namespace zmqzext {

template <typename T>
zmq::send_result_t send_retry_on_eintr(T& socket, zmq::const_buffer const& buf,
                                       zmq::send_flags flags /* = zmq::send_flags::none*/) {
    zmq::send_result_t result;
    while (true) {
        try {
            result = socket.send(buf, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

template <typename T>
zmq::send_result_t send_retry_on_eintr(T& socket, zmq::message_t& msg,
                                       zmq::send_flags flags /* = zmq::send_flags::none*/) {
    zmq::send_result_t result;
    while (true) {
        try {
            result = socket.send(msg, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

template <typename T>
zmq::send_result_t send_retry_on_eintr(T& socket, zmq::message_t&& msg,
                                       zmq::send_flags flags /* = zmq::send_flags::none*/) {
    return send_retry_on_eintr(socket, msg, flags);
}

template <typename T>
zmq::recv_buffer_result_t recv_retry_on_eintr(T& socket, zmq::mutable_buffer const& buf,
                                              zmq::recv_flags flags /*= zmq::recv_flags::none*/) {
    zmq::recv_buffer_result_t result;
    while (true) {
        try {
            result = socket.recv(buf, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

template <typename T>
zmq::recv_result_t recv_retry_on_eintr(T& socket, zmq::message_t& msg,
                                       zmq::recv_flags flags /*= zmq::recv_flags::none*/) {
    zmq::recv_result_t result;
    while (true) {
        try {
            result = socket.recv(msg, flags);
            break;
        } catch (zmq::error_t const& e) {
            if (e.num() == EINTR) {
                continue;
            }
            throw;
        }
    }
    return result;
}

template CZZE_EXPORT zmq::send_result_t send_retry_on_eintr<zmq::socket_t>(
    zmq::socket_t& socket, zmq::const_buffer const& buf, zmq::send_flags flags = zmq::send_flags::none);
template CZZE_EXPORT zmq::send_result_t send_retry_on_eintr<zmq::socket_t>(
    zmq::socket_t& socket, zmq::message_t& msg, zmq::send_flags flags = zmq::send_flags::none);
template CZZE_EXPORT zmq::send_result_t send_retry_on_eintr<zmq::socket_t>(
    zmq::socket_t& socket, zmq::message_t&& msg, zmq::send_flags flags = zmq::send_flags::none);
template CZZE_EXPORT zmq::recv_buffer_result_t recv_retry_on_eintr<zmq::socket_t>(
    zmq::socket_t& socket, zmq::mutable_buffer const& buf, zmq::recv_flags flags = zmq::recv_flags::none);
template CZZE_EXPORT zmq::recv_result_t recv_retry_on_eintr<zmq::socket_t>(
    zmq::socket_t& socket, zmq::message_t& msg, zmq::recv_flags flags = zmq::recv_flags::none);

template CZZE_EXPORT zmq::send_result_t send_retry_on_eintr<zmq::socket_ref>(
    zmq::socket_ref& socket, zmq::const_buffer const& buf, zmq::send_flags flags = zmq::send_flags::none);
template CZZE_EXPORT zmq::send_result_t send_retry_on_eintr<zmq::socket_ref>(
    zmq::socket_ref& socket, zmq::message_t& msg, zmq::send_flags flags = zmq::send_flags::none);
template CZZE_EXPORT zmq::send_result_t send_retry_on_eintr<zmq::socket_ref>(
    zmq::socket_ref& socket, zmq::message_t&& msg, zmq::send_flags flags = zmq::send_flags::none);
template CZZE_EXPORT zmq::recv_buffer_result_t recv_retry_on_eintr<zmq::socket_ref>(
    zmq::socket_ref& socket, zmq::mutable_buffer const& buf, zmq::recv_flags flags = zmq::recv_flags::none);
template CZZE_EXPORT zmq::recv_result_t recv_retry_on_eintr<zmq::socket_ref>(
    zmq::socket_ref& socket, zmq::message_t& msg, zmq::recv_flags flags = zmq::recv_flags::none);

}  // namespace zmqzext

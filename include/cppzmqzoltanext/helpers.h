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
 * @file helpers.h
 * @brief Helper utilities for robust ZMQ message sending and receiving
 *
 * This header provides utility functions that wrap ZMQ send and receive
 * operations with automatic retry logic for handling EINTR errors.
 * These helpers ensure that signal interruptions do not
 * prematurely terminate socket operations.
 *
 * The module implements retrying operations that are interrupted by signals,
 * allowing applications to safely use signal handlers without compromising message delivery.
 *
 * @details
 * Key features:
 * - Transparent EINTR handling for send operations
 * - Transparent EINTR handling for receive operations
 * - Support for multiple buffer and message types
 * - Configurable send/receive flags
 * - Returns original ZMQ result types for integration
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

#include <zmq.hpp>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Send a message buffer with automatic retry on EINTR
 *
 * Sends data from a const buffer through a ZMQ socket, automatically retrying
 * if the operation is interrupted by a signal.
 *
 * @param socket The ZMQ socket to send data through
 * @param buf The constant buffer containing data to send
 * @param flags Optional ZMQ send flags (default: none)
 * @return The send result containing the number of bytes sent
 *
 * @throw zmq::error_t if an error occurs (other than EINTR)
 * @note Overloads are provided for socket parameter types zmq::socket_t and zmq::socket_ref
 * @see send_retry_on_eintr(T&, zmq::message_t&, zmq::send_flags)
 * @see send_retry_on_eintr(T&, zmq::message_t&&, zmq::send_flags)
 */
template <typename T>
CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(T& socket, zmq::const_buffer const& buf,
                                                   zmq::send_flags flags = zmq::send_flags::none);

/**
 * @brief Send a ZMQ message with automatic retry on EINTR
 *
 * Sends a ZMQ message object through a socket, automatically retrying if
 * the operation is interrupted by a signal.
 *
 * @param socket The ZMQ socket to send data through
 * @param msg Reference to the message to send
 * @param flags Optional ZMQ send flags (default: none)
 * @return The send result containing the number of bytes sent
 *
 * @throw zmq::error_t if an error occurs (other than EINTR)
 * @note Overloads are provided for socket parameter types zmq::socket_t and zmq::socket_ref
 * @see send_retry_on_eintr(T&, zmq::const_buffer const&, zmq::send_flags)
 * @see send_retry_on_eintr(T&, zmq::message_t&&, zmq::send_flags)
 */
template <typename T>
CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(T& socket, zmq::message_t& msg,
                                                   zmq::send_flags flags = zmq::send_flags::none);

/**
 * @brief Send a ZMQ message (rvalue) with automatic retry on EINTR
 *
 * Sends a temporary ZMQ message through a socket, automatically retrying if
 * the operation is interrupted by a signal.
 *
 * @param socket The ZMQ socket to send data through
 * @param msg Rvalue reference to the message to send
 * @param flags Optional ZMQ send flags (default: none)
 * @return The send result containing the number of bytes sent
 *
 * @throw zmq::error_t if an error occurs (other than EINTR)
 * @note Overloads are provided for socket parameter types zmq::socket_t and zmq::socket_ref
 * @see send_retry_on_eintr(T&, zmq::const_buffer const&, zmq::send_flags)
 * @see send_retry_on_eintr(T&, zmq::message_t&, zmq::send_flags)
 */
template <typename T>
CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(T& socket, zmq::message_t&& msg,
                                                   zmq::send_flags flags = zmq::send_flags::none);

/**
 * @brief Receive data into a buffer with automatic retry on EINTR
 *
 * Receives data from a ZMQ socket into a mutable buffer, automatically
 * retrying if the operation is interrupted by a signal.
 *
 * @param socket The ZMQ socket to receive data from
 * @param buf The mutable buffer to receive data into
 * @param flags Optional ZMQ receive flags (default: none)
 * @return The receive result containing the number of bytes received and
 *         the actual buffer used
 *
 * @throw zmq::error_t if an error occurs (other than EINTR)
 * @note Overloads are provided for socket parameter types zmq::socket_t and zmq::socket_ref
 * @see recv_retry_on_eintr(T&, zmq::message_t&, zmq::recv_flags)
 */
template <typename T>
CZZE_EXPORT zmq::recv_buffer_result_t recv_retry_on_eintr(T& socket, zmq::mutable_buffer const& buf,
                                                          zmq::recv_flags flags = zmq::recv_flags::none);

/**
 * @brief Receive a message with automatic retry on EINTR
 *
 * Receives a ZMQ message from a socket, automatically retrying if the
 * operation is interrupted by a signal. The message object is populated
 * with the received data.
 *
 * @param socket The ZMQ socket to receive data from
 * @param msg Reference to the message object to populate with received data
 * @param flags Optional ZMQ receive flags (default: none)
 * @return The receive result containing the number of bytes received
 *
 * @throw zmq::error_t if an error occurs (other than EINTR)
 * @note Overloads are provided for socket parameter types zmq::socket_t and zmq::socket_ref
 * @see recv_retry_on_eintr(T&, zmq::mutable_buffer const&, zmq::recv_flags)
 */
template <typename T>
CZZE_EXPORT zmq::recv_result_t recv_retry_on_eintr(T& socket, zmq::message_t& msg,
                                                   zmq::recv_flags flags = zmq::recv_flags::none);

}  // namespace zmqzext

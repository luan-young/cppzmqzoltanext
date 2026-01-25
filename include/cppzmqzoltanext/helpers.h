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
 * @author CppZmqZoltanExt Contributors
 * @date 2025
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
 * @see send_retry_on_eintr(zmq::socket_t&, zmq::message_t&, zmq::send_flags)
 * @see send_retry_on_eintr(zmq::socket_t&, zmq::message_t&&, zmq::send_flags)
 */
CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::const_buffer const& buf,
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
 * @see send_retry_on_eintr(zmq::socket_t&, zmq::const_buffer const&, zmq::send_flags)
 * @see send_retry_on_eintr(zmq::socket_t&, zmq::message_t&&, zmq::send_flags)
 */
CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::message_t& msg,
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
 * @see send_retry_on_eintr(zmq::socket_t&, zmq::const_buffer const&, zmq::send_flags)
 * @see send_retry_on_eintr(zmq::socket_t&, zmq::message_t&, zmq::send_flags)
 */
CZZE_EXPORT zmq::send_result_t send_retry_on_eintr(zmq::socket_t& socket, zmq::message_t&& msg,
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
 * @see recv_retry_on_eintr(zmq::socket_t&, zmq::message_t&, zmq::recv_flags)
 */
CZZE_EXPORT zmq::recv_buffer_result_t recv_retry_on_eintr(zmq::socket_t& socket, zmq::mutable_buffer const& buf,
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
 * @see recv_retry_on_eintr(zmq::socket_t&, zmq::mutable_buffer const&, zmq::recv_flags)
 */
CZZE_EXPORT zmq::recv_result_t recv_retry_on_eintr(zmq::socket_t& socket, zmq::message_t& msg,
                                                   zmq::recv_flags flags = zmq::recv_flags::none);

}  // namespace zmqzext

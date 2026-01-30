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
 * @file signal.h
 * @brief Signal definitions and utilities for actor inter-thread communication
 *
 * This header provides the signal_t class for managing standardized signals
 * used in communication through ZMQ (ZeroMQ) messaging. Signals
 * are lightweight messages that convey state information in
 * a distributed or multi-threaded system.
 *
 * The module defines three primary signal types:
 * - success: Indicates successful completion of an operation
 * - failure: Indicates failed completion of an operation
 * - stop: Indicates a request to terminate or stop execution
 *
 * @note Signals are serialized as single-byte ZMQ messages for efficiency.
 * @see zmq::message_t for ZMQ message types
 *
 * @details
 * Key features:
 * - Type-safe signal enumeration
 * - Factory methods for creating signal messages
 * - Integration with zmq::message_t
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

#include <array>
#include <optional>
#include <zmq.hpp>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Class representing signals for ZMQ communication
 *
 * The signal_t class encapsulates signal types. It provides factory methods
 * to create a zmq::message_t representing different signal states and utilities
 * to parse and validate incoming messages as signals.
 *
 * @note This class is immutable after construction and can be instantiated only by
 *       the check_signal(const zmq::message_t& msg) static factory method.
 * @note Static methods are provided to create zmq::message_t instances encoding a signal_t.
 */
class CZZE_EXPORT signal_t {
public:
    /**
     * @brief Enumeration of possible signal types
     *
     * Defines the three fundamental signal states.
     * Each signal type represents a distinct condition or request in the
     * communication protocol.
     */
    enum class type_t : uint8_t {
        success = 1,  ///< Operation completed successfully
        failure = 2,  ///< Operation failed
        stop = 3,     ///< Stop/terminate request
    };

    /**
     * @brief Get the type of the signal
     *
     * @return The signal type signal_t::type_t
     * @note This function is thread-safe.
     * @see type_t
     */
    type_t type() const noexcept { return _type; }

    /**
     * @brief Check if this signal is a success signal
     *
     * @return true if the signal type is success, false otherwise
     * @note This function is thread-safe.
     * @see is_failure()
     * @see is_stop()
     */
    bool is_success() const noexcept { return _type == type_t::success; }

    /**
     * @brief Check if this signal is a failure signal
     *
     * @return true if the signal type is failure, false otherwise
     * @note This function is thread-safe.
     * @see is_success()
     * @see is_stop()
     */
    bool is_failure() const noexcept { return _type == type_t::failure; }

    /**
     * @brief Check if this signal is a stop signal
     *
     * @return true if the signal type is stop, false otherwise
     * @note This function is thread-safe.
     * @see is_success()
     * @see is_failure()
     */
    bool is_stop() const noexcept { return _type == type_t::stop; }

    /**
     * @brief Create a success signal message
     *
     * Constructs a zmq::message_t representing a success signal.
     * The message contains the encoded success signal type.
     *
     * @return A zmq::message_t containing the success signal
     * @see create_failure()
     * @see create_stop()
     */
    static zmq::message_t create_success();

    /**
     * @brief Create a failure signal message
     *
     * Constructs a zmq::message_t representing a failure signal.
     * The message contains the encoded failure signal type.
     *
     * @return A zmq::message_t containing the failure signal
     * @see create_success()
     * @see create_stop()
     */
    static zmq::message_t create_failure();

    /**
     * @brief Create a stop signal message
     *
     * Constructs a zmq::message_t representing a stop/terminate signal.
     * The message contains the encoded stop signal type.
     *
     * @return A zmq::message_t containing the stop signal
     * @see create_success()
     * @see create_failure()
     */
    static zmq::message_t create_stop();

    /**
     * @brief Check if a zmq::message_t contains a valid signal
     *
     * Validates whether a given zmq::message_t contains a properly formatted
     * signal and, if valid, parses it into a signal_t object. This function
     * is useful for receiving and interpreting signals from ZMQ sockets.
     *
     * @param msg The zmq::message_t to validate and parse
     * @return std::optional<signal_t> containing a signal_t object if the
     *         message is a valid signal, std::nullopt otherwise
     *
     * @note This function is thread-safe and does not modify the input message.
     * @see create_success()
     * @see create_failure()
     * @see create_stop()
     */
    static std::optional<signal_t> check_signal(const zmq::message_t& msg) noexcept;

private:
    explicit signal_t(type_t type) noexcept : _type(type) {}
    type_t _type;
};

}  // namespace zmqzext

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
 * @file poller.h
 * @brief Event polling for monitoring multiple ZMQ sockets
 *
 * This header provides the poller_t class for efficient polling of multiple
 * ZMQ sockets. The poller monitors a set of sockets for readiness to receive
 * data, allowing applications to manage multiple concurrent socket
 * operations with a single wait operation.
 *
 * The module implements ZMQ's poll mechanism with support for timeout control,
 * interruption handling, and termination detection. This enables event-driven
 * architectures where multiple sockets are monitored concurrently.
 *
 * @details
 * Key features:
 * - Dynamic socket registration and deregistration
 * - Wait for single or multiple ready sockets
 * - Configurable timeout values
 * - Interruptible polling for signal handling
 * - Termination detection
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
#include <vector>
#include <zmq.hpp>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Class for efficient polling of multiple ZMQ sockets
 *
 * The poller_t class provides a convenient wrapper around ZMQ's polling
 * mechanism. It allows applications to monitor multiple sockets simultaneously
 * and wait for data availability on any or all of them.
 *
 * The poller supports adding and removing sockets to be monitored at any time.
 *
 * When used in conjunction with the interrupt handling module and the application receives a SIINT
 * or SIGTERM signal, the poller will return early from wait operations, allowing the application
 * to handle the interrupt by checking if the poller was terminated.
 *
 * The set_interruptible() method can be used to enable or disable interrupt checking.
 * When set to false (the default is true), the poller will still return early on
 * interrupt signals, but the terminated() method will always return false.
 * This behavior may be desirable on actors when they should continue processing all
 * events before receiving a stop request from the main application. So the main application
 * can perform a graceful shutdown without the actors loosing any messages that are already in their queues.
 *
 * @note This class is not thread-safe.
 * @note On Windows, the waiting calls to ZMQ functions do not return early on signals,
 * no matter if the signal handlers are installed or not. Still, the interrupt flag
 * is set and can be checked by the poller. Then, it is very important to call the wait
 * methods with an appropriate timeout to allow the poller to detect the interrupt
 * within a reasonable time.
 */
class CZZE_EXPORT poller_t {
public:
    /**
     * @brief Add a socket to the polling set
     *
     * Registers a socket with the poller for monitoring. The socket will be
     * polled in subsequent wait operations to detect readiness for receive operation.
     *
     * @param socket The ZMQ socket reference to add
     * @throws std::invalid_argument if the socket is invalid or already added
     * @see remove()
     */
    void add(zmq::socket_ref socket);

    /**
     * @brief Remove a socket from the polling set
     *
     * Unregisters a socket from the poller. The socket will no longer be
     * monitored in wait operations.
     *
     * @param socket The ZMQ socket reference to remove
     * @note Removing a socket that was not added is a no-op
     * @see add()
     */
    void remove(zmq::socket_ref socket);

    /**
     * @brief Set whether polling should be interruptible
     *
     * Controls whether the poller will check for interrupt signals during
     * wait operations.
     *
     * When enabled (default), the poller wait operations will return
     * immediately if an interrupt signal was already received, or return early if an interrupt
     * signal is received during the wait operations. The is_terminated() will then return true.
     *
     * When interruptible is disabled, the wait operations are allowed to wait for incoming
     * messages in the monitored sockets even if an interrupt signal was already received. Still,
     * the wait operations will return early if an interrupt signal is received during the wait.
     * The is_terminated() method will always return false, even if an interrupt signal was received.
     * This behavior may be desirable on actors when they should continue processing all
     * events before receiving a stop request from the main application. So the main application
     * can perform a graceful shutdown without the actors loosing any messages that are already in their queues.
     *
     * @param interruptible true to enable interrupt checking, false otherwise
     * @note Interrupt checking requires install_interrupt_handler() to be called
     * @note Default is true
     * @note Setting to false does not disable early return on interrupts
     * @see is_interruptible()
     * @see install_interrupt_handler()
     */
    void set_interruptible(bool interruptible) noexcept { _interruptible = interruptible; }

    /**
     * @brief Check if polling is interruptible
     *
     * @return true if the poller will check for interrupt signals, false otherwise
     * @see set_interruptible()
     */
    bool is_interruptible() const noexcept { return _interruptible; }

    /**
     * @brief Get the number of sockets in the polling set
     *
     * @return The count of registered sockets
     */
    std::size_t size() const noexcept { return _poll_items.size(); }

    /**
     * @brief Check if the poller has been terminated during the last wait operation
     *
     * Returns whether a termination condition has been detected during the last wait operation.
     * The termination condition occurs when an interrupt signal is received and the poller is
     * interruptible or when the context associated with any of the monitored
     * sockets is terminated.
     *
     * The terminated state is reset on each wait operation so a new wait can
     * be performed after receiving an interrupt signal when the interruptible
     * mode is disabled.
     *
     * @return true if the poller is in a terminated state, false otherwise
     * @see set_interruptible()
     */
    bool terminated() const noexcept { return _terminated; }

    /**
     * @brief Wait for any socket to become ready for receiving
     *
     * Blocks until at least one socket becomes ready for receiving, the timeout
     * expires, an interrupt signal is received or the context associated with any of the monitored
     * sockets is terminated. Returns the first ready socket found.
     *
     * Sockets are checked in the order they were added to the poller. If multiple
     * sockets are ready, the first one is returned. If the same socket is always
     * ready, it may starve other sockets.
     * For fairness, consider using wait_all() instead.
     *
     * @param timeout Maximum wait duration in milliseconds
     *                (default: -1 for infinite timeout)
     * @return A reference to the first socket that is ready for I/O
     *
     * @throws zmq::error_t if a ZMQ error occurs
     * @note When interrupted, it returns early, no matter the interruptible setting
     * @see wait_all()
     */
    zmq::socket_ref wait(std::chrono::milliseconds timeout = std::chrono::milliseconds{-1});

    /**
     * @brief Wait for at least one socket to become ready for receiving and return all ready
     *
     * Blocks until at least one socket becomes ready for receiving, the timeout
     * expires, an interrupt signal is received or the context associated with any of the monitored
     * sockets is terminated. Returns all ready sockets at the time of the check.
     *
     * Sockets are checked in the order they were added to the poller. If multiple
     * sockets are ready, all of them are returned in the order they were added.
     *
     * @param timeout Maximum wait duration in milliseconds
     *                (default: -1 for infinite timeout)
     * @return A vector of references to all ready sockets
     *
     * @throw zmq::error_t if a ZMQ error occurs
     * @note When interrupted, it returns early, no matter the interruptible setting
     * @see wait()
     */
    std::vector<zmq::socket_ref> wait_all(std::chrono::milliseconds timeout = std::chrono::milliseconds{-1});

private:
    /**
     * @brief Check if a socket is already registered in the poll set
     *
     * @param socket_handle The raw socket handle to search for
     * @return true if the socket is registered, false otherwise
     */
    bool has_socket(void* socket_handle) const;

private:
    std::vector<zmq::pollitem_t> _poll_items;  ///< Vector of poll items for ZMQ polling
    bool _interruptible{true};                 ///< Whether interrupt signals are considered as termination
    bool _terminated{false};                   ///< Termination state flag
};

}  // namespace zmqzext

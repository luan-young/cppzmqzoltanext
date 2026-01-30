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
 * @file loop.h
 * @brief Event loop for managing sockets and timers
 *
 * This header provides the loop_t class, which implements a complete event
 * loop for managing socket I/O and timer-based events. The event loop combines
 * socket polling (via poller_t) with timer management to create a reactive
 * event-driven architecture.
 *
 * The event loop monitors registered sockets for readiness and fires callbacks
 * when sockets become ready. Timers can be registered to trigger callbacks
 * at specified intervals, either once or repeatedly. This enables building complex
 * applications with concurrent I/O operations and time-based scheduling.
 *
 * @note The event loop runs synchronously and blocks until terminated.
 *       Use callbacks that return false to stop finish the loop.
 * @see poller_t for underlying socket polling mechanism
 * @see install_interrupt_handler() for signal handling integration
 *
 * @details
 * Key features:
 * - Socket registration with I/O callbacks
 * - One-shot and recurring timer support
 * - Event loop with interruptible operation
 * - Configurable interrupt checking intervals
 * - Automatic timer management and expiration
 * - Integration with interrupt signal handling
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
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <zmq.hpp>

#include "cppzmqzoltanext/czze_export.h"
#include "poller.h"

namespace zmqzext {

class loop_t;

/// Unique identifier for timer instances
using timer_id_t = std::size_t;

/**
 * @brief Socket event handler callback type
 *
 * Function signature for socket event handlers. The handler is called when
 * a registered socket becomes ready for receiving. Returning false
 * finishes the loop; returning true continues processing.
 *
 * @param loop Reference to the event loop
 * @param socket The socket that is ready for receiving
 * @return false to finish the loop, true to continue
 */
using fn_socket_handler_t = std::function<bool(loop_t&, zmq::socket_ref)>;

/**
 * @brief Timer event handler callback type
 *
 * Function signature for timer event handlers. The handler is called when
 * a registered timer expires. Returning false finishes the loop;
 * returning true continues processing.
 *
 * @param loop Reference to the event loop
 * @param timer_id The unique identifier of the timer that expired
 * @return false to finish the loop, true to continue
 */
using fn_timer_handler_t = std::function<bool(loop_t&, timer_id_t)>;

/**
 * @brief Event loop for managing socket and timer events
 *
 * The loop_t class provides a reactive event loop that monitors multiple
 * sockets for I/O readiness and manages scheduled timers. It uses a poller_t
 * internally to efficiently monitor multiple sockets simultaneously, and
 * maintains a collection of timers with expiration tracking.
 *
 * The event loop integrates with the interrupt handling system, allowing
 * graceful shutdown in response to signals like SIGINT or SIGTERM.
 *
 * @note The loop runs in the calling thread and blocks until terminated.
 * @note This class is not thread-safe.
 * @note Interruptible behavior: when disabled, the loop ignores interrupt signals
 *       and continues running. This behavior may be desirable on actors when they should continue processing all
 *       events before receiving a stop request from the main application. So the main application
 *       can perform a graceful shutdown without the actors loosing any messages that are already in their queues.
 * @note InterruptCheckInterval: On Windows, the waiting calls to ZMQ functions
 *       do not return early on interrupt signals. Then, on an interrupt signal
 *       arrival, the loop would keep blocked indefinitely unless a socket becomes ready
 *       or a timer expires. Setting a finite InterruptCheckInterval allows the loop
 *       to periodically wake up and check the interrupt flag, enabling timely
 *       shutdown even on Windows. It is important to set this interval to a reasonable
 *       value, and also to set appropriate timeouts on all ZMQ calls (send and receive).
 * @note Interrupt checking requires install_interrupt_handler() to be called
 * @see poller_t
 * @see install_interrupt_handler()
 */
class CZZE_EXPORT loop_t {
private:
    using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;
    using time_milliseconds_t = std::chrono::milliseconds;

    /**
     * @brief Internal timer state structure
     */
    struct timer_t {
        timer_id_t id;                      ///< Unique timer identifier
        std::chrono::milliseconds timeout;  ///< Timer interval duration
        std::size_t occurences;             ///< Remaining occurrences (0 for infinite)
        time_point_t next_occurence;        ///< Next scheduled expiration time
        fn_timer_handler_t handler;         ///< Callback function for timer events
        bool removed;                       ///< Flag indicating timer is marked for removal
    };

public:
    /**
     * @brief Register a socket with an I/O handler
     *
     * Adds a socket to the event loop with an associated callback function.
     * The callback is invoked whenever the socket becomes ready for receiving
     * (data available to receive).
     *
     * @param socket The ZMQ socket to register
     * @param fn Callback function to invoke when socket is ready
     * @throws std::invalid_argument if the socket is invalid or already added
     * @see remove()
     */
    void add(zmq::socket_ref socket, fn_socket_handler_t fn);

    /**
     * @brief Register a timer with an expiration handler
     *
     * Adds a timer to the event loop that will expire at regular intervals.
     * The callback is invoked when the timer expires. Timers can be one-shot
     * (occurences=1) or recurring (occurences > 1 or 0 for infinite).
     *
     * @param timeout Duration between timer expirations in milliseconds
     * @param occurences Number of times the timer should fire (0 for infinite)
     * @param fn Callback function to invoke when timer expires
     * @return Unique timer identifier for later removal or reference
     * @throws std::runtime_error if no unique timer ID can be generated
     *
     * @see remove_timer()
     */
    timer_id_t add_timer(std::chrono::milliseconds timeout, std::size_t occurences, fn_timer_handler_t fn);

    /**
     * @brief Unregister a socket from the event loop
     *
     * Removes a socket from event monitoring. The socket's handler callback
     * will no longer be invoked.
     *
     * @param socket The ZMQ socket to remove
     * @note Removing a socket that was not registered is a no-op
     * @note It is safe to remove a socket within its own handler callback or from another callback
     * @see add()
     */
    void remove(zmq::socket_ref socket);

    /**
     * @brief Unregister a timer from the event loop
     *
     * Removes a timer from the event loop. The timer's handler callback will
     * no longer be invoked.
     *
     * @param timer_id The unique identifier of the timer to remove
     * @note Removing a timer that was not registered is a no-op
     * @note It is safe to remove a timer within its own handler callback or from another callback
     * @see add_timer()
     */
    void remove_timer(timer_id_t timer_id);

    /**
     * @brief Run the event loop
     *
     * Starts the event loop, which continuously monitors sockets and timers,
     * invoking their respective callbacks when events occur. The loop blocks
     * until terminated via signal interrupt, the termination of the context
     * associated with any socket, callback return value is false, becomes
     * empty (no sockets or timers registered anymore).
     *
     * @param interruptible Whether to check for interrupt signals during loop
     *                      execution (default is true) to finish the loop.
     *                      When false, the loop will ignore any interrupt signals
     *                      and will continue running (useful in actors orchestrated
     *                      by a main application). See class documentation notes on
     *                      interruptible behavior.
     * @param interruptCheckInterval Duration between interrupt checks (when enabled) in
     *                               milliseconds (default: -1 for checking only when
     *                               interrupted by a signal, socket ready, or timer expiry).
     *                               See class documentation notes on interrupt checking.
     *
     * @note This function blocks until the loop is terminated
     * @see install_interrupt_handler()
     */
    void run(bool interruptible = true,
             std::chrono::milliseconds interruptCheckInterval = std::chrono::milliseconds{-1});

    /**
     * @brief Check if the event loop has been terminated by interrupt signal or context termination
     *
     * @return true if the loop is in a terminated state, false otherwise
     * @see run()
     */
    bool terminated() const noexcept { return _poller.terminated(); }

private:
    /**
     * @brief Get the current steady clock time
     * @return Current time point
     */
    time_point_t now();

    /**
     * @brief Calculate timeout for next poll operation
     *
     * Determines the appropriate timeout for the next polling operation based
     * on the next scheduled timer expiration and _interruptCheckInterval.
     *
     * @param actual_time Current time
     * @return Timeout duration for polling, or -1 for no timeout
     */
    time_milliseconds_t find_next_timeout(time_point_t const& actual_time);

    /**
     * @brief Clean up timers marked for removal
     *
     * Removes timer entries that have been flagged for removal, typically
     * called after handlers complete to safely remove timers.
     */
    void removeFlagedTimers();

    /**
     * @brief Generate unique timer identifier
     *
     * Creates a new unique ID for timer registration, handling wraparound
     * of the timer ID counter.
     *
     * @return New unique timer ID
     */
    timer_id_t generate_unique_timer_id();

private:
    poller_t _poller;                                                 ///< Socket polling mechanism
    std::map<zmq::socket_ref, fn_socket_handler_t> _socket_handlers;  ///< Socket handler registry
    std::list<timer_t> _timer_handlers;                               ///< Timer registry
    timer_id_t _last_timer_id{0};                                     ///< Last allocated timer ID
    bool _timer_id_has_overflowed{false};                             ///< Flag indicating timer ID wraparound
    time_milliseconds_t _interruptCheckInterval{-1};                  ///< Interval for interrupt checking
};

}  // namespace zmqzext

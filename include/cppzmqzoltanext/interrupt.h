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
 * @file interrupt.h
 * @brief Signal interrupt handling for graceful application shutdown
 *
 * This header provides signal handling utilities for managing SIGINT (Ctrl+C)
 * and SIGTERM signals. It establishes a mechanism for detecting and responding
 * to interrupt signals, enabling graceful shutdown of applications.
 *
 * The module manages signal handlers through a pair of installation and
 * restoration functions, allowing applications to safely install the signal handler
 * provided by this module and restore previous handlers when needed.
 *
 * The signal handler provided by this module sets an atomic flag that
 * tracks the interrupt state, which can be checked by the application to
 * determine when shutdown should be initiated.
 *
 * The atomic flag is monitored by the poller_t and loop_t classes to allow
 * them to detect interrupt conditions and return early from polling or loop
 * operations.
 *
 * Typically, an application will call install_interrupt_handler() during
 * application initialization and perform a clean shutdown when its main
 * poller_t or loop_t instance indicates that an interrupt has occurred.
 *
 * @note If no signal handlers are installed and the application receives a SIINT
 * or SIGTERM signal, any waiting call to a ZMQ function
 * will be interrupted and the application will terminate abruptly. On the
 * other hand, if the signal handlers are installed using this module, the
 * waiting calls to ZMQ functions will be interrupted and return EINTR,
 * allowing the application to handle the interrupt.
 *
 * @note On Windows, the waiting calls to ZMQ functions do not return early on interrupt signals,
 * no matter if the signal handlers are installed or not. Still, the interrupt flag
 * is set and can be checked by the application after the ZMQ wait operation. Then,
 * it is very important to set appropriate timeouts on all ZMQ calls.
 *
 * Key features:
 * - Atomic flag for thread-safe interrupt detection
 * - Automatic storage and restoration of previous signal handlers
 * - Support for SIGINT (Ctrl+C) and SIGTERM signals
 * - Non-blocking interrupt checking
 * - Manual interrupt flag reset capability
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

#include <atomic>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Install signal handlers for SIGINT and SIGTERM
 *
 * Installs a signal handler for SIGINT (Ctrl+C) and SIGTERM signals.
 * When either signal is received, a global atomic flag is set to true,
 * indicating that an interrupt has been requested. Other parts of the
 * application can check this flag via is_interrupted() to implement
 * graceful shutdown logic.
 *
 * The function preserves the current signal handlers before installing new
 * ones on the first call or after a call to restore_interrupt_handler().
 * This allows these original handlers to be restored later via
 * restore_interrupt_handler().
 *
 * @note This function is not thread-safe; it should be called during
 *       application initialization before multiple threads are spawned.
 * @note Multiple calls without an intervening call to restore_interrupt_handler()
 *       will not save additional handler states.
 * @see restore_interrupt_handler()
 * @see is_interrupted()
 * @see reset_interrupted()
 */
CZZE_EXPORT void install_interrupt_handler() noexcept;

/**
 * @brief Restore the previously stored signal handlers
 *
 * Restores the signal handlers that were active before the first call to
 * install_interrupt_handler(). This function does nothing if
 * install_interrupt_handler() was never called or if the handlers were
 * already restored.
 *
 * After calling this function, a subsequent call to install_interrupt_handler()
 * will save the current handlers (which are now the original handlers) before
 * installing new custom signal handlers.
 *
 * @note This function is not thread-safe.
 * @note The interrupt flag state is not affected by this operation.
 * @see install_interrupt_handler()
 */
CZZE_EXPORT void restore_interrupt_handler() noexcept;

/**
 * @brief Check if a program interrupt was received
 *
 * Checks whether a SIGINT (Ctrl+C) or SIGTERM signals have been received
 * since the interrupt handlers were installed or since the last call to
 * reset_interrupted().
 *
 * @return true if an interrupt signal has been received, false otherwise
 * @note This function is thread-safe and non-blocking.
 * @see reset_interrupted()
 * @see install_interrupt_handler()
 */
CZZE_EXPORT bool is_interrupted() noexcept;

/**
 * @brief Reset the interrupt flag to false
 *
 * Resets the atomic interrupt flag to false, allowing the application to
 * continue monitoring for new interrupt signals. This function is useful
 * when you want to handle an interrupt and then continue execution while
 * maintaining the ability to detect future interrupts.
 *
 * @note This function is thread-safe.
 * @note Resetting the flag does not affect the signal handlers.
 * @see is_interrupted()
 * @see install_interrupt_handler()
 */
CZZE_EXPORT void reset_interrupted() noexcept;

}  // namespace zmqzext

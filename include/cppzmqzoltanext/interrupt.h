#pragma once

#include <atomic>

#include "cppzmqzoltanext/czze_export.h"

namespace zmqzext {

/**
 * @brief Install signal handlers for SIGINT and SIGTERM
 *
 * This function installs signal handlers for SIGINT (Ctrl+C) and SIGTERM
 * signals. When either signal is received, a global flag
 * is set to true. The flag can be checked by other parts of the code to
 * implement graceful shutdown.
 *
 * If this is the first call or if called after restore_interrupt_handler(),
 * it will store the current signal handlers before installing new ones.
 */
CZZE_EXPORT void install_interrupt_handler();

/**
 * @brief Restore the previously stored signal handlers
 *
 * This function restores the signal handlers that were active before the first
 * call to install_interrupt_handler(). Does nothing if
 * install_interrupt_handler() was never called or if the handlers were already
 * restored.
 *
 * After this call, the next call to install_interrupt_handler() will store
 * the current handlers again.
 */
CZZE_EXPORT void restore_interrupt_handler();

/**
 * @brief Check if a program interrupt was received (SIGINT or SIGTERM)
 * @return true if an interrupt was received, false otherwise
 */
CZZE_EXPORT bool is_interrupted();

/**
 * @brief Reset the interrupt flag to false
 *
 * This function resets the interrupt flag to false, allowing the program to
 * continue monitoring for new interrupts. This is useful when you want to
 * handle an interrupt and then continue execution while still monitoring
 * for future interrupts.
 */
CZZE_EXPORT void reset_interrupted();

}  // namespace zmqzext

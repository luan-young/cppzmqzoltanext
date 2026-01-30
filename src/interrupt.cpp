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
 * @file interrupt.cpp
 * @brief Signal interrupt handling for graceful application shutdown
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2025 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */
#include "cppzmqzoltanext/interrupt.h"

#if !defined(WIN32)
#include <signal.h>
#else
#include <csignal>
#endif

#include <atomic>

namespace zmqzext {

namespace {
static std::atomic<bool> zmqzext_interrupted{false};
static bool handlers_stored{false};
#if !defined(WIN32)
static struct sigaction stored_sigint;
static struct sigaction stored_sigterm;
#else
static void (*stored_sigint_handler)(int) = nullptr;
static void (*stored_sigterm_handler)(int) = nullptr;
#endif

void signal_handler(int /*signal*/) { zmqzext_interrupted.store(true, std::memory_order_relaxed); }

#if !defined(WIN32)
void store_signal_handlers() noexcept {
    sigaction(SIGINT, nullptr, &stored_sigint);
    sigaction(SIGTERM, nullptr, &stored_sigterm);
    handlers_stored = true;
}

void setup_signal_handlers() noexcept {
    struct sigaction action;
    action.sa_handler = signal_handler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
}
#endif

}  // namespace

#if !defined(WIN32)
void install_interrupt_handler() noexcept {
    // Store current handlers only if not already stored or after a restore
    if (!handlers_stored) {
        store_signal_handlers();
    }
    setup_signal_handlers();
}

void restore_interrupt_handler() noexcept {
    if (handlers_stored) {
        sigaction(SIGINT, &stored_sigint, nullptr);
        sigaction(SIGTERM, &stored_sigterm, nullptr);
        handlers_stored = false;
    }
}
#else
void install_interrupt_handler() noexcept {
    if (!handlers_stored) {
        stored_sigint_handler = std::signal(SIGINT, signal_handler);
        stored_sigterm_handler = std::signal(SIGTERM, signal_handler);
        handlers_stored = true;
    } else {
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
    }
}

void restore_interrupt_handler() noexcept {
    if (handlers_stored) {
        std::signal(SIGINT, stored_sigint_handler);
        std::signal(SIGTERM, stored_sigterm_handler);
        handlers_stored = false;
    }
}
#endif

bool is_interrupted() noexcept { return zmqzext_interrupted.load(std::memory_order_relaxed); }

void reset_interrupted() noexcept { zmqzext_interrupted.store(false, std::memory_order_relaxed); }

}  // namespace zmqzext

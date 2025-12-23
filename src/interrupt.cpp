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

void signal_handler(int /*signal*/) {
    zmqzext_interrupted.store(true, std::memory_order_relaxed);
}

#if !defined(WIN32)
void store_signal_handlers() {
    sigaction(SIGINT, nullptr, &stored_sigint);
    sigaction(SIGTERM, nullptr, &stored_sigterm);
    handlers_stored = true;
}

void setup_signal_handlers() {
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
void install_interrupt_handler() {
    // Store current handlers only if not already stored or after a restore
    if (!handlers_stored) {
        store_signal_handlers();
    }
    setup_signal_handlers();
}

void restore_interrupt_handler() {
    if (handlers_stored) {
        sigaction(SIGINT, &stored_sigint, nullptr);
        sigaction(SIGTERM, &stored_sigterm, nullptr);
        handlers_stored = false;
    }
}
#else
void install_interrupt_handler() {
    if (!handlers_stored) {
        stored_sigint_handler = std::signal(SIGINT, signal_handler);
        stored_sigterm_handler = std::signal(SIGTERM, signal_handler);
        handlers_stored = true;
    } else {
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
    }
}

void restore_interrupt_handler() {
    if (handlers_stored) {
        std::signal(SIGINT, stored_sigint_handler);
        std::signal(SIGTERM, stored_sigterm_handler);
        handlers_stored = false;
    }
}
#endif

bool is_interrupted() {
    return zmqzext_interrupted.load(std::memory_order_relaxed);
}

void reset_interrupted() {
    zmqzext_interrupted.store(false, std::memory_order_relaxed);
}

}  // namespace zmqzext

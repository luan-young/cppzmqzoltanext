#include "cppzmqzoltanext/interrupt.h"

#include <signal.h>

#include <atomic>

namespace zmqzext {

namespace {
static std::atomic<bool> zmqzext_interrupted{false};
static bool handlers_stored{false};
static struct sigaction stored_sigint;
static struct sigaction stored_sigterm;

void signal_handler(int /*signal*/) {
    zmqzext_interrupted.store(true, std::memory_order_relaxed);
}

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

}  // namespace

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

bool is_interrupted() {
    return zmqzext_interrupted.load(std::memory_order_relaxed);
}

void reset_interrupt() {
    zmqzext_interrupted.store(false, std::memory_order_relaxed);
}

}  // namespace zmqzext

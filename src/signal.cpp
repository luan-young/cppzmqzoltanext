#include "cppzmqzoltanext/signal.h"

namespace zmqzext {

namespace {

constexpr uint64_t SIGNAL_PREFIX = 0x7766554433221100ULL;

zmq::message_t create_signal_message(signal_t::type_t signal_type) {
    uint64_t data = SIGNAL_PREFIX | static_cast<uint8_t>(signal_type);
    return zmq::message_t{&data, sizeof(uint64_t)};
}

}  // namespace

zmq::message_t signal_t::create_success() { return create_signal_message(type_t::success); }

zmq::message_t signal_t::create_failure() { return create_signal_message(type_t::failure); }

zmq::message_t signal_t::create_stop() { return create_signal_message(type_t::stop); }

std::optional<signal_t> signal_t::check_signal(const zmq::message_t& msg) noexcept {
    if (msg.size() != sizeof(uint64_t)) {
        return std::nullopt;
    }

    if ((*msg.data<uint64_t>() & ~0xFFULL) != SIGNAL_PREFIX) {
        return std::nullopt;
    }

    uint8_t signal_byte = *msg.data<uint64_t>() & 0xFF;
    switch (signal_byte) {
        case static_cast<uint8_t>(type_t::success):
            return signal_t(type_t::success);
        case static_cast<uint8_t>(type_t::failure):
            return signal_t(type_t::failure);
        case static_cast<uint8_t>(type_t::stop):
            return signal_t(type_t::stop);
        default:
            return std::nullopt;
    }
}

}  // namespace zmqzext

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
 * @file signal.cpp
 * @brief Signal definitions and utilities for actor inter-thread communication
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2025 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */
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

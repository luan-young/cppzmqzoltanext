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
 * @file loop.cpp
 * @brief Event loop for managing sockets and timers
 *
 * @authors
 * Luan Young (luanpy@gmail.com)
 *
 * @copyright 2025 Luan Young
 *
 * Distributed under the MIT License (MIT) (See accompanying file LICENSE
 * or copy at http://opensource.org/licenses/MIT)
 */
#include "cppzmqzoltanext/loop.h"

#include <algorithm>

namespace zmqzext {

void loop_t::add(zmq::socket_ref socket, fn_socket_handler_t fn) {
    _poller.add(socket);
    try {
        _socket_handlers.emplace(socket, fn);
    } catch (...) {
        _poller.remove(socket);
        throw;
    }
}

timer_id_t loop_t::add_timer(std::chrono::milliseconds timeout, std::size_t occurences, fn_timer_handler_t fn) {
    auto const timer_id = generate_unique_timer_id();
    auto const next_occurence = now() + timeout;
    _timer_handlers.push_back(timer_t{timer_id, timeout, occurences, next_occurence, fn, false});
    return timer_id;
}

void loop_t::remove(zmq::socket_ref socket) {
    _poller.remove(socket);
    auto const socket_handler_it = _socket_handlers.find(socket);
    if (socket_handler_it == _socket_handlers.end()) {
        return;
    }
    _socket_handlers.erase(socket_handler_it);
}

void loop_t::remove_timer(timer_id_t timer_id) {
    auto timer_it = std::find_if(_timer_handlers.begin(), _timer_handlers.end(),
                                 [timer_id](timer_t const& timer) { return timer.id == timer_id; });
    if (timer_it == _timer_handlers.end()) {
        return;
    }
    timer_it->removed = true;
}

void loop_t::run(bool interruptible /* = true*/,
                 std::chrono::milliseconds interruptCheckInterval /* = std::chrono::milliseconds{-1}*/) {
    _poller.set_interruptible(interruptible);
    _interruptCheckInterval = interruptCheckInterval;
    auto should_continue = true;
    while (should_continue) {
        removeFlagedTimers();
        if (_poller.size() == 0 && _timer_handlers.size() == 0) {
            return;
        }
        auto const initial_time = now();
        auto const next_timeout = find_next_timeout(initial_time);
        auto sockets_ready = _poller.wait_all(next_timeout);
        if (_poller.terminated()) {
            return;
        }
        auto const current_time = now();
        for (auto timer_it = _timer_handlers.begin(); timer_it != _timer_handlers.end();) {
            if (timer_it->removed == false && current_time >= timer_it->next_occurence) {
                should_continue = timer_it->handler(*this, timer_it->id);
                if (!should_continue) {
                    break;
                }
                if (timer_it->occurences > 0 && --timer_it->occurences == 0) {
                    timer_it = _timer_handlers.erase(timer_it);
                    continue;
                } else {
                    timer_it->next_occurence += timer_it->timeout;
                }
            }
            ++timer_it;
        }
        if (!should_continue) {
            break;
        }
        for (auto& socket : sockets_ready) {
            auto const socket_handler_it = _socket_handlers.find(socket);
            if (socket_handler_it != _socket_handlers.end()) {
                should_continue = socket_handler_it->second(*this, socket_handler_it->first);
                if (!should_continue) {
                    break;
                }
            }
        }
    }
}

loop_t::time_point_t loop_t::now() { return std::chrono::steady_clock::now(); }

loop_t::time_milliseconds_t loop_t::find_next_timeout(time_point_t const& actual_time) {
    auto const next_expiring_timer_it =
        std::min_element(_timer_handlers.cbegin(), _timer_handlers.cend(),
                         [](timer_t const& a, timer_t const& b) { return a.next_occurence < b.next_occurence; });
    if (next_expiring_timer_it == _timer_handlers.cend()) {
        if (_interruptCheckInterval > time_milliseconds_t{0}) {
            return _interruptCheckInterval;
        }
        return time_milliseconds_t{-1};
    }
    auto time_left = next_expiring_timer_it->next_occurence - actual_time;
    if (_interruptCheckInterval > time_milliseconds_t{0} && time_left > _interruptCheckInterval) {
        return _interruptCheckInterval;
    }
    return std::max(time_milliseconds_t{0}, std::chrono::ceil<time_milliseconds_t>(time_left));
}

void loop_t::removeFlagedTimers() {
    _timer_handlers.remove_if([](timer_t const& timer) { return timer.removed; });
}

timer_id_t loop_t::generate_unique_timer_id() {
    timer_id_t timer_id = ++_last_timer_id;
    if (_last_timer_id == 0) {
        _timer_id_has_overflowed = true;
        timer_id = ++_last_timer_id;
    }
    if (_timer_id_has_overflowed) {
        while (std::any_of(_timer_handlers.begin(), _timer_handlers.end(),
                           [timer_id](auto const& t) { return t.id == timer_id; })) {
            timer_id = ++_last_timer_id;
            if (_last_timer_id == 0) {
                throw std::runtime_error("Unable to generate unique timer ID: all IDs are in use.");
            }
        }
    }
    return timer_id;
}

}  // namespace zmqzext

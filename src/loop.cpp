#include "cppzmqzoltanext/loop.h"

namespace zmqzext {

void loop_t::add(zmq::socket_ref socket, fn_socket_handler_t fn) {
    _socket_handlers.emplace(socket, fn);
    _poller.add(socket);
}

timer_id_t loop_t::add_timer(std::chrono::milliseconds timeout,
                             std::size_t occurences, fn_timer_handler_t fn) {
    auto const timer_id =
        ++_last_timer_id;  // TODO: check timer id is unique and keep iterating
                           // until finds a unique one
    auto const next_occurence = now() + timeout;
    _timer_handlers.push_back(
        timer_t{timer_id, timeout, occurences, next_occurence, fn, false});
    return timer_id;
}

void loop_t::remove(zmq::socket_ref socket) {
    auto const socket_handler_it = _socket_handlers.find(socket);
    if (socket_handler_it == _socket_handlers.end()) {
        return;
    }
    _socket_handlers.erase(socket_handler_it);
    _poller.remove(socket);
}

void loop_t::remove_timer(timer_id_t timer_id) {
    auto timer_it = std::find_if(
        _timer_handlers.begin(), _timer_handlers.end(),
        [timer_id](timer_t const& timer) { return timer.id == timer_id; });
    if (timer_it == _timer_handlers.end()) {
        return;
    }
    timer_it->removed = true;
}

void loop_t::run() {
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
        for (auto timer_it = _timer_handlers.begin();
             timer_it != _timer_handlers.end();) {
            if (timer_it->removed == false &&
                current_time >= timer_it->next_occurence) {
                should_continue = timer_it->handler(*this, timer_it->id);
                if (timer_it->occurences > 0 && --timer_it->occurences == 0) {
                    timer_it = _timer_handlers.erase(timer_it);
                } else {
                    timer_it->next_occurence += timer_it->timeout;
                }
            }
            ++timer_it;
        }
        for (auto& socket : sockets_ready) {
            auto const socket_handler_it = _socket_handlers.find(socket);
            if (socket_handler_it != _socket_handlers.end()) {
                should_continue =
                    socket_handler_it->second(*this, socket_handler_it->first);
                if (!should_continue) {
                    break;
                }
            }
        }
    }
}

loop_t::time_point_t loop_t::now() { return std::chrono::steady_clock::now(); }

loop_t::time_milliseconds_t loop_t::find_next_timeout(
    time_point_t const& actual_time) {
    auto const next_expiring_timer_it =
        std::min_element(_timer_handlers.cbegin(), _timer_handlers.cend(),
                         [](timer_t const& a, timer_t const& b) {
                             return a.next_occurence < b.next_occurence;
                         });
    if (next_expiring_timer_it == _timer_handlers.cend()) {
        return time_milliseconds_t{-1};
    }
    auto const time_left = next_expiring_timer_it->next_occurence - actual_time;
    return std::max(time_milliseconds_t{0}, ceil_to_milliseconds(time_left));
}

void loop_t::removeFlagedTimers() {
    _timer_handlers.remove_if(
        [](timer_t const& timer) { return timer.removed; });
}

template <class Rep, class Period>
loop_t::time_milliseconds_t loop_t::ceil_to_milliseconds(
    std::chrono::duration<Rep, Period> const& duration) {
    time_milliseconds_t t =
        std::chrono::duration_cast<time_milliseconds_t>(duration);
    if (t < duration) {
        return t + time_milliseconds_t{1};
    }
    return t;
}

}  // namespace zmqzext

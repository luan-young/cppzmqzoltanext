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

using timer_id_t = std::size_t;
using fn_socket_handler_t = std::function<bool(loop_t &, zmq::socket_ref)>;
using fn_timer_handler_t = std::function<bool(loop_t &, timer_id_t)>;

class CZZE_EXPORT loop_t {
private:
    using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;
    using time_milliseconds_t = std::chrono::milliseconds;
    struct timer_t {
        timer_id_t id;
        std::chrono::milliseconds timeout;
        std::size_t occurences;
        time_point_t next_occurence;
        fn_timer_handler_t handler;
        bool removed;
    };

public:
    void add(zmq::socket_ref socket, fn_socket_handler_t fn);
    timer_id_t add_timer(std::chrono::milliseconds timeout,
                         std::size_t occurences, fn_timer_handler_t fn);
    void remove(zmq::socket_ref socket);
    void remove_timer(timer_id_t timer_id);
    void run(bool interruptible = true);
    bool terminated() const { return _poller.terminated(); }

private:
    time_point_t now();
    time_milliseconds_t find_next_timeout(time_point_t const &actual_time);
    void removeFlagedTimers();
    template <class Rep, class Period>
    time_milliseconds_t ceil_to_milliseconds(
        std::chrono::duration<Rep, Period> const &duration);

private:
    poller_t _poller;
    std::map<zmq::socket_ref, fn_socket_handler_t> _socket_handlers;
    std::list<timer_t> _timer_handlers;
    timer_id_t _last_timer_id{0};
};

}  // namespace zmqzext

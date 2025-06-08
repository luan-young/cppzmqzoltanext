#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <vector>

#include <zmq.hpp>

#include "poller.h"

namespace zmqzext
{

class loop_t;

using timer_id_t = std::size_t;
using fn_socket_handler_t = std::function<bool(loop_t&, zmq::socket_ref)>;
using fn_timer_handler_t = std::function<bool(loop_t&, timer_id_t)>;

class loop_t
{
public:
    void add(zmq::socket_t &socket, fn_socket_handler_t fn);
    void remove(zmq::socket_ref socket);
    void run();

private:
    std::map<zmq::socket_ref, fn_socket_handler_t> _socket_handlers;
    poller_t _poller;

private:
    using time_point_t = std::chrono::time_point<std::chrono::steady_clock>;
    struct timer_t{
        timer_id_t id;
        std::chrono::milliseconds timeout;
        std::size_t times;
        time_point_t nextOccurence;
        fn_timer_handler_t handler;
        bool removed;
    };
};

} // namespace zmqzext

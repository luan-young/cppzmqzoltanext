#include "cppzmqzoltanext/loop.h"

namespace zmqzext
{

void loop_t::add(zmq::socket_t &socket, fn_socket_handler_t fn)
{
    _socket_handlers.emplace(socket, fn);
    _poller.add(socket);
}

void loop_t::remove(zmq::socket_ref socket)
{
    auto const socket_handler_it = _socket_handlers.find(socket);
    if (socket_handler_it == _socket_handlers.end()) {
        return;
    }
    _socket_handlers.erase(socket_handler_it);
    _poller.remove(socket);
}

void loop_t::run()
{
    auto should_continue = true;
    while (should_continue) {
        if (_poller.size() == 0) {
            return;
        }
        auto sockets_ready = _poller.wait_all();
        if (_poller.terminated()) {
            return;
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

}

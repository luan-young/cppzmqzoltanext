#include "cppzmqzoltanext/poller.h"

#include <algorithm>

namespace zmqzext
{

void poller_t::add(zmq::socket_ref socket)
{
    _poll_items.emplace_back(zmq::pollitem_t{socket.handle(), 0, ZMQ_POLLIN, 0});
}

void poller_t::remove(zmq::socket_ref socket)
{
    _poll_items.erase(std::remove_if(_poll_items.begin(),
                                     _poll_items.end(),
                                     [&socket](zmq::pollitem_t const &item) { return item.socket == socket.handle(); }),
                      _poll_items.end());
}

zmq::socket_ref poller_t::wait(std::chrono::milliseconds timeout /*= std::chrono::milliseconds{-1}*/)
{
    _terminated = false;
    try {
        auto const n_items = zmq::poll(_poll_items, timeout);
        if (n_items > 0) {
            for (std::size_t i = 0; i < _poll_items.size(); ++i) {
                if (_poll_items[i].revents == ZMQ_POLLIN) {
                    return zmq::socket_ref{zmq::from_handle, _poll_items[i].socket};
                }
            }
        }
    }
    catch(zmq::error_t const&){
        _terminated = true;
    }
    return zmq::socket_ref{};
}

std::vector<zmq::socket_ref> poller_t::wait_all(std::chrono::milliseconds timeout /*= std::chrono::milliseconds{-1}*/)
{
    _terminated = false;
    std::vector<zmq::socket_ref> result{};
    try {
        auto const n_items = zmq::poll(_poll_items, timeout);
        if (n_items > 0) {
            result.reserve(n_items);
            for (std::size_t i = 0; i < _poll_items.size(); ++i) {
                if (_poll_items[i].revents == ZMQ_POLLIN) {
                    result.emplace_back(zmq::socket_ref{zmq::from_handle, _poll_items[i].socket});
                }
            }
        }
    }
    catch(zmq::error_t const&){
        _terminated = true;
    }
    return result;
}

} // namespace zmqzext

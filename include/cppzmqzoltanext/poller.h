#pragma once

#include <chrono>
#include <vector>
#include <zmq.hpp>

namespace zmqzext {

class poller_t {
public:
    void add(zmq::socket_ref socket);
    void remove(zmq::socket_ref socket);
    std::size_t size() const { return _poll_items.size(); }
    bool terminated() const { return _terminated; }

    zmq::socket_ref wait(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{-1});
    std::vector<zmq::socket_ref> wait_all(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{-1});

private:
    std::vector<zmq::pollitem_t> _poll_items;
    bool _terminated{false};
};

}  // namespace zmqzext

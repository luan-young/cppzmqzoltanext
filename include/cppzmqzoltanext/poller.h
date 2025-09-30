#pragma once

#include <chrono>
#include <vector>
#include <zmq.hpp>

#include "czze_export.h"

namespace zmqzext {

class CZZE_EXPORT poller_t {
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
    bool has_socket(void* socket_handle) const;

private:
    std::vector<zmq::pollitem_t> _poll_items;
    bool _terminated{false};
};

}  // namespace zmqzext

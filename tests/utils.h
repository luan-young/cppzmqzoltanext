#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <cppzmqzoltanext/loop.h>

namespace
{

class eagain_send_exception : public std::runtime_error
{
public:
    eagain_send_exception() : std::runtime_error{"Send returned EAGAIN"}
    {
    }
};

class eagain_recv_exception : public std::runtime_error
{
public:
    eagain_recv_exception() : std::runtime_error{"Recv returned EAGAIN"}
    {
    }
};

void send_now_or_throw(zmq::socket_ref socket, std::string const &msg)
{
    auto const result = socket.send(zmq::buffer(msg), zmq::send_flags::dontwait);
    if (!result)
    {
        throw eagain_send_exception{};
    }
}

zmq::message_t recv_now_or_throw(zmq::socket_ref socket)
{
    zmq::message_t msg;
    auto const result = socket.recv(msg, zmq::recv_flags::dontwait);
    if (!result)
    {
        throw eagain_recv_exception{};
    }
    return msg;
}

void waitSocketHaveMsg(zmq::socket_ref socket, std::chrono::milliseconds timeout)
{
    std::vector<zmq::pollitem_t> _poll_items;
    _poll_items.emplace_back(zmq::pollitem_t{socket.handle(), 0, ZMQ_POLLIN, 0});
    auto const nItems = zmq::poll(_poll_items, timeout);
    if (nItems <= 0) {
        throw std::runtime_error{"Socket has not msg ready to receive in timeout."};
    }
}

std::thread shutdown_ctx_after_time(zmq::context_t & ctx, std::chrono::milliseconds time)
{
    return std::thread([&ctx = ctx, time]() {
        std::this_thread::sleep_for(time);
        ctx.shutdown();
    });
}

struct ConnectedSocketsPullAndPush
{
    zmq::socket_t socketPull;
    zmq::socket_t socketPush;

    ConnectedSocketsPullAndPush(zmq::context_t &ctx)
        : socketPull{ctx, zmq::socket_type::pull}, socketPush{ctx, zmq::socket_type::push}
    {
        socketPull.set(zmq::sockopt::linger, 0);
        socketPush.set(zmq::sockopt::linger, 0);

        socketPull.bind("tcp://localhost:*");
        auto const addr = socketPull.get(zmq::sockopt::last_endpoint);
        socketPush.connect(addr);
    }
};

struct ConnectedSocketsWithHandlers
{
    std::unique_ptr<zmq::socket_t> socketPull;
    std::unique_ptr<zmq::socket_t> socketPush;
    std::unique_ptr<zmq::socket_t> socketPull2;
    std::unique_ptr<zmq::socket_t> socketPush2;
    std::size_t maxMsgs{0};
    std::vector<zmq::message_t> messages;

    ConnectedSocketsWithHandlers(zmq::context_t &ctx)
        : socketPull{std::make_unique<zmq::socket_t>(ctx, zmq::socket_type::pull)},
          socketPush{std::make_unique<zmq::socket_t>(ctx, zmq::socket_type::push)},
          socketPull2{std::make_unique<zmq::socket_t>(ctx, zmq::socket_type::pull)},
          socketPush2{std::make_unique<zmq::socket_t>(ctx, zmq::socket_type::push)}
    {
        socketPull->set(zmq::sockopt::linger, 0);
        socketPush->set(zmq::sockopt::linger, 0);
        socketPull2->set(zmq::sockopt::linger, 0);
        socketPush2->set(zmq::sockopt::linger, 0);

        socketPull->bind("tcp://localhost:*");
        auto const addr = socketPull->get(zmq::sockopt::last_endpoint);
        socketPush->connect(addr);

        socketPull2->bind("tcp://localhost:*");
        auto const addr2 = socketPull2->get(zmq::sockopt::last_endpoint);
        socketPush2->connect(addr2);
    }

    bool socketHandlerReceiveMaxMessages(zmqzext::loop_t &, zmq::socket_ref socket)
    {
        assert(messages.size() < maxMsgs);
        auto msg = recv_now_or_throw(socket);
        messages.emplace_back(std::move(msg));
        if (messages.size() >= maxMsgs)
            return false;
        return true;
    }

    bool socketHandlerRemoveSocketBeingHandled(zmqzext::loop_t &loop, zmq::socket_ref socket)
    {
        auto msg = recv_now_or_throw(socket);
        messages.emplace_back(std::move(msg));
        if (socketPull && *socketPull == socket) {
            loop.remove(*socketPull);
            socketPull.reset();
        }
        else if (socketPull2 && *socketPull2 == socket) {
            loop.remove(*socketPull2);
            socketPull2.reset();
        }
        return true;
    }

    bool socketHandlerRemoveOtherSocket(zmqzext::loop_t &loop, zmq::socket_ref socket)
    {
        auto msg = recv_now_or_throw(socket);
        messages.emplace_back(std::move(msg));
        if (socketPull && *socketPull != socket) {
            loop.remove(*socketPull);
            socketPull.reset();
        }
        else if (socketPull2 && *socketPull2 != socket) {
            loop.remove(*socketPull2);
            socketPull2.reset();
        }
        return true;
    }
};

} // namespace

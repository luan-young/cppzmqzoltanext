#include <chrono>
#include <functional>
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

    bool socketHandlerAddOtherSocket(zmqzext::loop_t &loop, zmq::socket_ref socket, zmq::socket_ref otherSocket)
    {
        using std::placeholders::_1;
        using std::placeholders::_2;
        auto msg = recv_now_or_throw(socket);
        messages.emplace_back(std::move(msg));
        loop.add(otherSocket, std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, this, _1, _2));
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

    bool socketHandlerAddTimer(zmqzext::loop_t &loop, zmq::socket_ref socket, zmqzext::fn_timer_handler_t handler)
    {
        std::size_t const timerOcurrences{1};
        std::chrono::milliseconds timerTimeout{1};
        auto msg = recv_now_or_throw(socket);
        messages.emplace_back(std::move(msg));
        auto const timer = loop.add_timer(timerTimeout, timerOcurrences, handler);
        return true;
    }

    bool socketHandlerRemoveTimer(zmqzext::loop_t &loop, zmq::socket_ref socket, zmqzext::timer_id_t const& timerToRevmove)
    {
        auto msg = recv_now_or_throw(socket);
        messages.emplace_back(std::move(msg));
        loop.remove_timer(timerToRevmove);
        return true;
    }
};

struct TimersHandlers
{
    std::vector<zmqzext::timer_id_t> timersHandled;
    std::vector<zmqzext::timer_id_t> timersAdded;

    bool timerHandler(zmqzext::loop_t&, zmqzext::timer_id_t timerId)
    {
        timersHandled.push_back(timerId);
        return true;
    }

    bool timerHandlerAddTimer(zmqzext::loop_t& loop, zmqzext::timer_id_t timerId)
    {
        using std::placeholders::_1;
        using std::placeholders::_2;
        timersHandled.push_back(timerId);
        timersAdded.push_back(
            loop.add_timer(std::chrono::milliseconds{2}, 1, std::bind(&TimersHandlers::timerHandler, this, _1, _2)));
        return true;
    }

    bool timerHandlerRemoveTimer(zmqzext::loop_t& loop, zmqzext::timer_id_t timerId, zmqzext::timer_id_t const& timerToRevmove)
    {
        timersHandled.push_back(timerId);
        loop.remove_timer(timerToRevmove);
        return true;
    }

    bool timerHandlerAddSocket(zmqzext::loop_t& loop, zmqzext::timer_id_t timerId, zmq::socket_ref socket, zmqzext::fn_socket_handler_t handler)
    {
        timersHandled.push_back(timerId);
        loop.add(socket, handler);
        return true;
    }

    bool timerHandlerRemoveSocket(zmqzext::loop_t& loop, zmqzext::timer_id_t timerId, std::unique_ptr<zmq::socket_t>& pSocket)
    {
        timersHandled.push_back(timerId);
        loop.remove(*pSocket);
        pSocket.reset();
        return true;
    }

    bool timerHandlerSendFromSocket(zmqzext::loop_t& loop, zmqzext::timer_id_t timerId, zmq::socket_ref socket)
    {
        std::string const msgStrToSend{"Message from timer"};
        timersHandled.push_back(timerId);
        send_now_or_throw(socket, msgStrToSend);
        return true;
    }
};

} // namespace

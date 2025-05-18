#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <zmq.hpp>

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

struct ConnectedSocketsPullAndPush
{
    zmq::socket_t socketPull;
    zmq::socket_t socketPush;

    ConnectedSocketsPullAndPush(zmq::context_t &ctx)
        : socketPull{ctx, zmq::socket_type::pull}, socketPush{ctx, zmq::socket_type::push}
    {
        // socketPull.set(zmq::sockopt::linger, 0);
        // socketPush.set(zmq::sockopt::linger, 0);

        socketPull.bind("tcp://localhost:*");
        auto const addr = socketPull.get(zmq::sockopt::last_endpoint);
        socketPush.connect(addr);
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

std::thread shutdown_ctx_after_time(zmq::context_t & ctx, std::chrono::milliseconds time)
{
    return std::thread([&ctx = ctx, time]() {
        std::this_thread::sleep_for(time);
        ctx.shutdown();
    });
}

} // namespace

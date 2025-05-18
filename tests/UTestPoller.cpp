#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <cppzmqzoltanext/poller.h>

#include "utils.h"

namespace zmqzext
{
class UTestPoller : public ::testing::Test
{
public:
    poller_t poller;
    zmq::context_t ctx;
};

TEST_F(UTestPoller, ReturnsTheSocketReadyToReceive)
{
    ConnectedSocketsPullAndPush sockets{ctx};
    zmq::socket_t unconnectedSocket{ctx, zmq::socket_type::pull};
    std::string const msgStrToSend{"Test message"};

    poller.add(unconnectedSocket);
    poller.add(sockets.socketPull);

    send_now_or_throw(sockets.socketPush, msgStrToSend);

    auto socket = poller.wait();
    ASSERT_EQ(sockets.socketPull, socket);

    auto const recvMsg = recv_now_or_throw(socket);
    EXPECT_EQ(msgStrToSend, recvMsg.to_string());
}

TEST_F(UTestPoller, ReturnsNullSocketWhenNotReadyToReceiveInTimeout)
{
    ConnectedSocketsPullAndPush sockets{ctx};
    zmq::socket_t unconnectedSocket{ctx, zmq::socket_type::pull};
    zmq::socket_t nullSocket{};

    poller.add(unconnectedSocket);
    poller.add(sockets.socketPull);

    auto socket = poller.wait(std::chrono::milliseconds{10});

    EXPECT_EQ(nullSocket, socket);
}

TEST_F(UTestPoller, ReturnsNullSocketWhenReadySocketWasRemoved)
{
    ConnectedSocketsPullAndPush sockets{ctx};
    zmq::socket_t unconnectedSocket{ctx, zmq::socket_type::pull};
    std::string const msgStrToSend{"Test message"};

    poller.add(unconnectedSocket);
    poller.add(sockets.socketPull);

    send_now_or_throw(sockets.socketPush, msgStrToSend);

    poller.remove(sockets.socketPull);

    auto socket = poller.wait(std::chrono::milliseconds{10});

    EXPECT_EQ(nullptr, socket);
}

TEST_F(UTestPoller, ReturnsAllSocketsReadyToReceive)
{
    ConnectedSocketsPullAndPush sockets1{ctx};
    ConnectedSocketsPullAndPush sockets2{ctx};
    zmq::socket_t unconnectedSocket{ctx, zmq::socket_type::pull};
    std::string const msgStrToSend1{"Test message 1"};
    std::string const msgStrToSend2{"Test message 2"};

    poller.add(sockets1.socketPull);
    poller.add(unconnectedSocket);
    poller.add(sockets2.socketPull);

    send_now_or_throw(sockets1.socketPush, msgStrToSend1);
    send_now_or_throw(sockets2.socketPush, msgStrToSend2);

    // give time to allow all msgs be ready to the in sockets
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    auto sockets = poller.wait_all();

    ASSERT_EQ(2, sockets.size());
    ASSERT_EQ(sockets1.socketPull, sockets[0]);
    ASSERT_EQ(sockets2.socketPull, sockets[1]);

    auto const recvMsg1 = recv_now_or_throw(sockets[0]);
    EXPECT_EQ(msgStrToSend1, recvMsg1.to_string());

    auto const recvMsg2 = recv_now_or_throw(sockets[1]);
    EXPECT_EQ(msgStrToSend2, recvMsg2.to_string());
}

TEST_F(UTestPoller, WaitCallLingersForGivenTimeoutWhenNotReadyToReceive)
{
    std::chrono::milliseconds timeOut{10};
    ConnectedSocketsPullAndPush sockets{ctx};
    zmq::socket_t unconnectedSocket{ctx, zmq::socket_type::pull};

    poller.add(unconnectedSocket);
    poller.add(sockets.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto socket = poller.wait(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_GE(elapsedTime, timeOut);
}

TEST_F(UTestPoller, WaitCallLingersForGivenTimeoutWhenPollerIsEmpty)
{
    std::chrono::milliseconds timeOut{10};

    auto const startTime = std::chrono::steady_clock::now();
    auto socket = poller.wait(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_GE(elapsedTime, timeOut);
}

TEST_F(UTestPoller, WaitAllCallLingersForGivenTimeoutWhenPollerIsEmpty)
{
    std::chrono::milliseconds timeOut{10};

    auto const startTime = std::chrono::steady_clock::now();
    auto sockets = poller.wait_all(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_GE(elapsedTime, timeOut);
}

TEST_F(UTestPoller, WaitCallIsInterruptedOnContextShutdown)
{
    zmq::socket_t socket{ctx, zmq::socket_type::rep};

    poller.add(socket);

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    auto socketReady = poller.wait();

    t.join();

    EXPECT_EQ(nullptr, socketReady);
}

TEST_F(UTestPoller, WaitCallIsNotInterruptedOnContextShutdownWhenPollerIsEmpty)
{
    std::chrono::milliseconds timeOut{100};
    auto const startTime = std::chrono::steady_clock::now();

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    auto socketReady = poller.wait(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    t.join();

    EXPECT_GE(elapsedTime, timeOut);
}

} // namespace zmqzext

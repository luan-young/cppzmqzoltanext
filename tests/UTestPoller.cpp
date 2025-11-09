#include <cppzmqzoltanext/interrupt.h>
#include <cppzmqzoltanext/poller.h>
#include <gtest/gtest.h>

#include <cassert>
#include <chrono>
#include <csignal>
#include <string>
#include <thread>
#include <zmq.hpp>

#include "utils.h"

namespace zmqzext {
class UTestPoller : public ::testing::Test {
public:
    poller_t poller;
    zmq::context_t ctx;
};

class UTestPollerWithInterruptHandler : public UTestPoller {
public:
    void SetUp() override { install_interrupt_handler(); }

    void TearDown() override {
        restore_interrupt_handler();
        reset_interrupt();
    }

    std::thread raise_interrupt_after_time(std::chrono::milliseconds time) {
        return std::thread([time]() {
            std::this_thread::sleep_for(time);
            kill(getpid(), SIGINT);
        });
    }
};

TEST_F(UTestPoller, ReturnsTheSocketReadyToReceive) {
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

TEST_F(UTestPoller, ReturnsNullSocketWhenNotReadyToReceiveInTimeout) {
    ConnectedSocketsPullAndPush sockets{ctx};
    zmq::socket_t unconnectedSocket{ctx, zmq::socket_type::pull};
    zmq::socket_t nullSocket{};

    poller.add(unconnectedSocket);
    poller.add(sockets.socketPull);

    auto socket = poller.wait(std::chrono::milliseconds{10});

    EXPECT_EQ(nullSocket, socket);
}

TEST_F(UTestPoller, ReturnsNullSocketWhenReadySocketWasRemoved) {
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

TEST_F(UTestPoller, ReturnsAllSocketsReadyToReceive) {
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

TEST_F(UTestPoller, WaitAllReturnsEmptyVectorWhenNotReadyToReceiveInTimeout) {
    ConnectedSocketsPullAndPush sockets{ctx};
    poller.add(sockets.socketPull);

    auto const readySockets = poller.wait_all(std::chrono::milliseconds{10});

    EXPECT_TRUE(readySockets.empty());
}

TEST_F(UTestPoller, WaitCallLingersForGivenTimeoutWhenNotReadyToReceive) {
    std::chrono::milliseconds timeOut{10};
    std::chrono::milliseconds timeErrorBound{1};
    ConnectedSocketsPullAndPush sockets{ctx};
    zmq::socket_t unconnectedSocket{ctx, zmq::socket_type::pull};

    poller.add(unconnectedSocket);
    poller.add(sockets.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto socket = poller.wait(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_GE(elapsedTime + timeErrorBound, timeOut);
}

TEST_F(UTestPoller, WaitCallLingersForGivenTimeoutWhenPollerIsEmpty) {
    std::chrono::milliseconds timeOut{10};
    std::chrono::milliseconds timeErrorBound{1};

    auto const startTime = std::chrono::steady_clock::now();
    auto socket = poller.wait(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_GE(elapsedTime + timeErrorBound, timeOut);
}

TEST_F(UTestPoller, WaitAllCallLingersForGivenTimeoutWhenPollerIsEmpty) {
    std::chrono::milliseconds timeOut{10};
    std::chrono::milliseconds timeErrorBound{1};

    auto const startTime = std::chrono::steady_clock::now();
    auto sockets = poller.wait_all(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_GE(elapsedTime + timeErrorBound, timeOut);
}

TEST_F(UTestPoller, WaitCallIsInterruptedOnContextShutdown) {
    zmq::socket_t socket{ctx, zmq::socket_type::rep};

    poller.add(socket);

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    auto socketReady = poller.wait();

    t.join();

    EXPECT_EQ(nullptr, socketReady);
    EXPECT_TRUE(poller.terminated());
}

TEST_F(UTestPoller,
       WaitCallIsNotInterruptedOnContextShutdownWhenPollerIsEmpty) {
    std::chrono::milliseconds timeOut{100};
    std::chrono::milliseconds timeErrorBound{1};
    auto const startTime = std::chrono::steady_clock::now();

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    // it will wait forever here if not timeout is given, as nothing will
    // interrupt it
    auto socketReady = poller.wait(timeOut);
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    t.join();

    EXPECT_GE(elapsedTime + timeErrorBound, timeOut);
    EXPECT_FALSE(poller.terminated());
}

TEST_F(UTestPoller, ThrowsWhenAddingNullSocket) {
    zmq::socket_t nullSocket{};
    EXPECT_THROW(poller.add(nullSocket), std::invalid_argument);
}

TEST_F(UTestPoller, ThrowsWhenAddingSameSocketTwice) {
    ConnectedSocketsPullAndPush sockets{ctx};
    poller.add(sockets.socketPull);
    EXPECT_THROW(poller.add(sockets.socketPull), std::invalid_argument);
}

TEST_F(UTestPoller, RemovingNonExistingSocketHasNoEffect) {
    ConnectedSocketsPullAndPush sockets{ctx};
    EXPECT_NO_THROW(poller.remove(sockets.socketPull));
}

TEST_F(UTestPoller, MultipleSocketRemovalsMaintainConsistency) {
    ConnectedSocketsPullAndPush sockets1{ctx};
    ConnectedSocketsPullAndPush sockets2{ctx};

    poller.add(sockets1.socketPull);
    poller.add(sockets2.socketPull);

    poller.remove(sockets1.socketPull);
    poller.remove(sockets2.socketPull);

    send_now_or_throw(sockets1.socketPush, "test");
    auto readySocket = poller.wait(std::chrono::milliseconds{10});
    EXPECT_EQ(nullptr, readySocket);
}

TEST_F(UTestPollerWithInterruptHandler, WaitCallIsTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto socket = poller.wait(std::chrono::milliseconds{1000});

    EXPECT_EQ(nullptr, socket);
    EXPECT_TRUE(poller.terminated());

    t.join();
}

TEST_F(UTestPollerWithInterruptHandler,
       WaitCallIsTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    ASSERT_TRUE(kill(getpid(), SIGINT) == 0);
    auto socket = poller.wait(std::chrono::milliseconds{1000});

    EXPECT_EQ(nullptr, socket);
    EXPECT_TRUE(poller.terminated());
}

TEST_F(UTestPollerWithInterruptHandler,
       WaitCallInNotInterruptibleModeIsNotTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto socket = poller.wait(std::chrono::milliseconds{100});

    EXPECT_EQ(nullptr, socket);
    EXPECT_FALSE(poller.terminated());

    t.join();
}

TEST_F(UTestPollerWithInterruptHandler,
       WaitCallInNotInterruptibleModeIsNotTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    ASSERT_TRUE(kill(getpid(), SIGINT) == 0);
    auto socket = poller.wait(std::chrono::milliseconds{10});

    EXPECT_EQ(nullptr, socket);
    EXPECT_FALSE(poller.terminated());
}

TEST_F(UTestPollerWithInterruptHandler,
       WaitAllCallIsTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto sockets = poller.wait_all(std::chrono::milliseconds{1000});

    EXPECT_TRUE(sockets.empty());
    EXPECT_TRUE(poller.terminated());

    t.join();
}

TEST_F(UTestPollerWithInterruptHandler,
       WaitAllCallIsTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    ASSERT_TRUE(kill(getpid(), SIGINT) == 0);
    auto sockets = poller.wait_all(std::chrono::milliseconds{1000});

    EXPECT_TRUE(sockets.empty());
    EXPECT_TRUE(poller.terminated());
}

TEST_F(UTestPollerWithInterruptHandler,
       WaitAllCallInNotInterruptibleModeIsNotTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto sockets = poller.wait_all(std::chrono::milliseconds{100});

    EXPECT_TRUE(sockets.empty());
    EXPECT_FALSE(poller.terminated());

    t.join();
}

TEST_F(UTestPollerWithInterruptHandler,
       WaitAllCallInNotInterruptibleModeIsNotTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    ASSERT_TRUE(kill(getpid(), SIGINT) == 0);
    auto sockets = poller.wait_all(std::chrono::milliseconds{10});

    EXPECT_TRUE(sockets.empty());
    EXPECT_FALSE(poller.terminated());
}

}  // namespace zmqzext

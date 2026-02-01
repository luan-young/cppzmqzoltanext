#include <cppzmqzoltanext/interrupt.h>
#include <cppzmqzoltanext/poller.h>
#include <gtest/gtest.h>

#include <cassert>
#include <chrono>
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
        reset_interrupted();
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
    ASSERT_TRUE(sockets.socketPull == socket);

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

    EXPECT_TRUE(nullSocket == socket);
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
    ASSERT_TRUE(sockets1.socketPull == sockets[0]);
    ASSERT_TRUE(sockets2.socketPull == sockets[1]);

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

TEST_F(UTestPoller, WaitCallIsNotInterruptedOnContextShutdownWhenPollerIsEmpty) {
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

#if !defined(WIN32)
TEST_F(UTestPollerWithInterruptHandler, WaitCallIsTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto socket = poller.wait(std::chrono::milliseconds{1000});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_EQ(nullptr, socket);
    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_TRUE(poller.terminated());

    t.join();
}
#else
// On Windows, signals do not interrupt the zmq polling call.
// So, on Windows the polling strategy may be loop with timeout to check for
// interrupt flag.
TEST_F(UTestPollerWithInterruptHandler, WaitCallIsTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto elapsedTime = std::chrono::steady_clock::now() - startTime;
    while (elapsedTime < std::chrono::milliseconds{1000} && !poller.terminated()) {
        auto socket = poller.wait(std::chrono::milliseconds{5});
        elapsedTime = std::chrono::steady_clock::now() - startTime;
    };

    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_TRUE(poller.terminated());

    t.join();
}
#endif

TEST_F(UTestPollerWithInterruptHandler, WaitCallIsTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    raise_interrupt_signal();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});  // ensure signal is handled
    auto const startTime = std::chrono::steady_clock::now();
    auto socket = poller.wait(std::chrono::milliseconds{10});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_EQ(nullptr, socket);
    EXPECT_LT(elapsedTime, std::chrono::milliseconds{5}) << "Poller shouldn't wait";
    EXPECT_TRUE(poller.terminated());
}

#if !defined(WIN32)
TEST_F(UTestPollerWithInterruptHandler, WaitCallInNotInterruptibleModeIsNotTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto socket = poller.wait(std::chrono::milliseconds{1000});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_EQ(nullptr, socket);
    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_FALSE(poller.terminated());

    t.join();
}
#else
// On Windows, signals do not interrupt the zmq polling call.
// So, on Windows the polling strategy may be loop with timeout to check for
// interrupt flag.
TEST_F(UTestPollerWithInterruptHandler, WaitCallInNotInterruptibleModeIsNotTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto elapsedTime = std::chrono::steady_clock::now() - startTime;
    while (elapsedTime < std::chrono::milliseconds{1000} && zmqzext::is_interrupted() == false) {
        auto socket = poller.wait(std::chrono::milliseconds{5});
        elapsedTime = std::chrono::steady_clock::now() - startTime;
    };

    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_FALSE(poller.terminated());

    t.join();
}
#endif

TEST_F(UTestPollerWithInterruptHandler, WaitCallInNotInterruptibleModeIsNotTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    raise_interrupt_signal();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});  // ensure signal is handled
    auto const startTime = std::chrono::steady_clock::now();
    auto socket = poller.wait(std::chrono::milliseconds{10});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_EQ(nullptr, socket);
    EXPECT_GE(elapsedTime, std::chrono::milliseconds{9}) << "Poller was interrupted";
    EXPECT_FALSE(poller.terminated());
}

#if !defined(WIN32)
TEST_F(UTestPollerWithInterruptHandler, WaitAllCallIsTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto sockets = poller.wait_all(std::chrono::milliseconds{1000});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_TRUE(sockets.empty());
    EXPECT_TRUE(poller.terminated());

    t.join();
}
#else
// On Windows, signals do not interrupt the zmq polling call.
// So, on Windows the polling strategy may be loop with timeout to check for
// interrupt flag.
TEST_F(UTestPollerWithInterruptHandler, WaitAllCallIsTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto elapsedTime = std::chrono::steady_clock::now() - startTime;
    auto sockets = std::vector<zmq::socket_ref>{};
    while (elapsedTime < std::chrono::milliseconds{1000} && !poller.terminated()) {
        sockets = poller.wait_all(std::chrono::milliseconds{5});
        elapsedTime = std::chrono::steady_clock::now() - startTime;
    };

    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_TRUE(sockets.empty());
    EXPECT_TRUE(poller.terminated());

    t.join();
}
#endif

TEST_F(UTestPollerWithInterruptHandler, WaitAllCallIsTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.add(sockets1.socketPull);

    raise_interrupt_signal();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});  // ensure signal is handled
    auto const startTime = std::chrono::steady_clock::now();
    auto sockets = poller.wait_all(std::chrono::milliseconds{10});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_LT(elapsedTime, std::chrono::milliseconds{5}) << "Poller shouldn't wait";
    EXPECT_TRUE(sockets.empty());
    EXPECT_TRUE(poller.terminated());
}

#if !defined(WIN32)
TEST_F(UTestPollerWithInterruptHandler, WaitAllCallInNotInterruptibleModeIsNotTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto sockets = poller.wait_all(std::chrono::milliseconds{1000});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_TRUE(sockets.empty());
    EXPECT_FALSE(poller.terminated());

    t.join();
}
#else
// On Windows, signals do not interrupt the zmq polling call.
// So, on Windows the polling strategy may be loop with timeout to check for
// interrupt flag.
TEST_F(UTestPollerWithInterruptHandler, WaitAllCallInNotInterruptibleModeIsNotTerminatedWhenInterrupted) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    auto const startTime = std::chrono::steady_clock::now();
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    auto elapsedTime = std::chrono::steady_clock::now() - startTime;
    auto sockets = std::vector<zmq::socket_ref>{};
    while (elapsedTime < std::chrono::milliseconds{1000} && zmqzext::is_interrupted() == false) {
        sockets = poller.wait_all(std::chrono::milliseconds{5});
        elapsedTime = std::chrono::steady_clock::now() - startTime;
    };

    EXPECT_LT(elapsedTime, std::chrono::milliseconds{100}) << "Not interrupted in time";
    EXPECT_TRUE(sockets.empty());
    EXPECT_FALSE(poller.terminated());

    t.join();
}
#endif

TEST_F(UTestPollerWithInterruptHandler, WaitAllCallInNotInterruptibleModeIsNotTerminatedWhenInterruptedBefore) {
    ConnectedSocketsPullAndPush sockets1{ctx};

    poller.set_interruptible(false);
    poller.add(sockets1.socketPull);

    raise_interrupt_signal();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});  // ensure signal is handled
    auto const startTime = std::chrono::steady_clock::now();
    auto sockets = poller.wait_all(std::chrono::milliseconds{10});
    auto const elapsedTime = std::chrono::steady_clock::now() - startTime;

    EXPECT_GE(elapsedTime, std::chrono::milliseconds{9}) << "Poller was interrupted";
    EXPECT_TRUE(sockets.empty());
    EXPECT_FALSE(poller.terminated());
}

TEST_F(UTestPoller, IsCopyConstructible) {
    ConnectedSocketsPullAndPush sockets{ctx};
    poller.add(sockets.socketPull);
    poller.set_interruptible(false);

    auto poller_copy = poller;

    EXPECT_EQ(poller.size(), poller_copy.size());
    EXPECT_EQ(poller.is_interruptible(), poller_copy.is_interruptible());

    poller.set_interruptible(true);
    poller.remove(sockets.socketPull);

    EXPECT_FALSE(poller_copy.is_interruptible());
    EXPECT_EQ(1U, poller_copy.size());
}

TEST_F(UTestPoller, IsCopyAssignable) {
    ConnectedSocketsPullAndPush sockets{ctx};

    poller.add(sockets.socketPull);
    poller.set_interruptible(false);

    poller_t poller2;
    poller2 = poller;

    EXPECT_EQ(poller.size(), poller2.size());
    EXPECT_EQ(poller.is_interruptible(), poller2.is_interruptible());

    poller.set_interruptible(true);
    poller.remove(sockets.socketPull);

    EXPECT_FALSE(poller2.is_interruptible());
    EXPECT_EQ(1U, poller2.size());
}

TEST_F(UTestPoller, CopyAssignmentSelfAssignmentIsNoop) {
    ConnectedSocketsPullAndPush sockets{ctx};
    poller.add(sockets.socketPull);

    // Self-assignment should not cause issues
    poller = poller;

    EXPECT_EQ(1U, poller.size());
}

TEST_F(UTestPoller, IsMoveConstructible) {
    ConnectedSocketsPullAndPush sockets{ctx};

    poller.add(sockets.socketPull);
    poller.set_interruptible(false);

    auto poller_moved = std::move(poller);

    // Verify the move worked
    EXPECT_EQ(0U, poller.size());
    EXPECT_EQ(1U, poller_moved.size());
    EXPECT_FALSE(poller_moved.is_interruptible());
}

TEST_F(UTestPoller, IsMoveAssignable) {
    ConnectedSocketsPullAndPush sockets{ctx};

    poller_t poller_temp;
    poller_temp.add(sockets.socketPull);
    poller_temp.set_interruptible(false);

    poller = std::move(poller_temp);

    // Verify the move assignment worked
    EXPECT_EQ(0U, poller_temp.size());
    EXPECT_EQ(1U, poller.size());
    EXPECT_FALSE(poller.is_interruptible());
}

TEST_F(UTestPoller, MoveAssignmentSelfAssignmentIsNotWhatYouShouldExpect) {
    ConnectedSocketsPullAndPush sockets{ctx};
    poller.add(sockets.socketPull);

    // Self move-assignment: YOU PROBABLY SHOULD NOT DO THIS
    poller = std::move(poller);

    EXPECT_EQ(0U, poller.size());
}

TEST_F(UTestPoller, CopiedPollerIsIndependent) {
    ConnectedSocketsPullAndPush sockets1{ctx};
    ConnectedSocketsPullAndPush sockets2{ctx};

    poller_t poller_copy;
    poller.add(sockets1.socketPull);
    poller_copy = poller;

    // Add another socket to original
    poller.add(sockets2.socketPull);

    // Copy should not be affected
    EXPECT_EQ(1U, poller_copy.size());
    EXPECT_EQ(2U, poller.size());
}

TEST_F(UTestPoller, CopiedPollerCanBeUsedIndependently) {
    ConnectedSocketsPullAndPush sockets{ctx};
    std::string const msgStrToSend{"Test message"};
    zmq::socket_t nullSocket{};

    poller_t poller_copy;
    poller.add(sockets.socketPull);
    poller_copy = poller;

    send_now_or_throw(sockets.socketPush, msgStrToSend);
    waitSocketHaveMsg(sockets.socketPull, std::chrono::milliseconds{2});

    // Both pollers should be able to detect the ready socket
    auto socket1 = poller.wait(std::chrono::milliseconds{1});
    auto socket2 = poller_copy.wait(std::chrono::milliseconds{1});
    EXPECT_EQ(sockets.socketPull, socket1);
    EXPECT_EQ(sockets.socketPull, socket2);

    // Read the message through only one returned socket
    EXPECT_NO_THROW(recv_now_or_throw(socket1));
    EXPECT_THROW(recv_now_or_throw(socket2), eagain_recv_exception);

    // Both pollers should have no more messages to receive now
    socket1 = poller.wait(std::chrono::milliseconds{1});
    socket2 = poller_copy.wait(std::chrono::milliseconds{1});
    EXPECT_EQ(nullSocket, socket1);
    EXPECT_EQ(nullSocket, socket2);
}

}  // namespace zmqzext

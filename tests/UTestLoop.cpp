#include <cppzmqzoltanext/interrupt.h>
#include <cppzmqzoltanext/loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <zmq.hpp>

#include "utils.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

using ::testing::ElementsAre;

namespace zmqzext {

class UTestLoop : public ::testing::Test {
public:
    loop_t loop;
    zmq::context_t ctx;
};

class UTestLoopWithInterruptHandler : public UTestLoop {
public:
    void SetUp() override { install_interrupt_handler(); }

    void TearDown() override {
        restore_interrupt_handler();
        reset_interrupted();
    }
};

TEST_F(UTestLoop, SocketHandlerIsCalled) {
    size_t const maxMsgs = 1;
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop.run();

    ASSERT_EQ(maxMsgs, sockets.messages.size());
    EXPECT_EQ(msgStrToSend, sockets.messages[0].to_string());
}

TEST_F(UTestLoop, KeepsRunningLoopUntilHandlerReturnsFalse) {
    size_t const maxMsgs = 2;
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    // send maxMsgs + 1 messages
    for (size_t i = 0; i < maxMsgs + 1; ++i) {
        send_now_or_throw(*sockets.socketPush, msgStrToSend);
    }

    loop.run();

    EXPECT_EQ(maxMsgs, sockets.messages.size());
}

TEST_F(UTestLoop, StopsRunningIfEmpty) { loop.run(); }

TEST_F(UTestLoop, StopsRunningWhenSocketContextIsShutdown) {
    ConnectedSocketsWithHandlers sockets{ctx};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    loop.run();

    t.join();
}

TEST_F(UTestLoop, HandlerFromEachSocketIsCalled) {
    size_t const maxMsgs = 2;
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));
    loop.add(*sockets.socketPull2,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    send_now_or_throw(*sockets.socketPush, msgStrToSend);
    send_now_or_throw(*sockets.socketPush2, msgStrToSend);

    loop.run();

    EXPECT_EQ(maxMsgs, sockets.messages.size());
}

TEST_F(UTestLoop, SupportsAddingOtherSocketWhileExecutingSocketHandler) {
    size_t const maxMsgs = 2;
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull, std::bind(&ConnectedSocketsWithHandlers::socketHandlerAddOtherSocket, &sockets, _1,
                                            _2, zmq::socket_ref{*sockets.socketPull2}));

    send_now_or_throw(*sockets.socketPush2,
                      msgStrToSend);  // socket 2 will only receive if added by
                                      // socket 1 handler
    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop.run();

    EXPECT_EQ(maxMsgs, sockets.messages.size());
}

TEST_F(UTestLoop, SupportsRemovingTheSocketWhileItsHandlerIsExecuting) {
    size_t const totalMsgsToSend = 2;
    size_t const totalMsgsShouldReceive = 1;
    ConnectedSocketsWithHandlers sockets{ctx};
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerRemoveSocketBeingHandled, &sockets, _1, _2));

    for (size_t i = 0; i < totalMsgsToSend; ++i) {
        send_now_or_throw(*sockets.socketPush, msgStrToSend);
    }

    // shall stop when socket is removed as the loop will become empty
    loop.run();

    EXPECT_EQ(totalMsgsShouldReceive, sockets.messages.size());
    EXPECT_EQ(nullptr, sockets.socketPull.get());
}

TEST_F(UTestLoop, SupportsRemovingTheSocketWhileItsHandlerIsExecuting_MoreSocketOnLoop) {
    size_t const totalMsgsToSend = 2;
    size_t const totalMsgsShouldReceive = 1;
    ConnectedSocketsWithHandlers sockets{ctx};
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerRemoveSocketBeingHandled, &sockets, _1, _2));
    loop.add(*sockets.socketPull2,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    for (size_t i = 0; i < totalMsgsToSend; ++i) {
        send_now_or_throw(*sockets.socketPush, msgStrToSend);
    }

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    // won't stop as second socket never receives any msg
    loop.run();

    t.join();

    EXPECT_EQ(totalMsgsShouldReceive, sockets.messages.size());
    EXPECT_EQ(nullptr, sockets.socketPull.get());
}

TEST_F(UTestLoop, SupportsRemovingASocketReadyToReceiveWhileHandlingOtherSocket) {
    size_t const totalMsgsToSend = 2;
    size_t const totalMsgsShouldReceive = 1;
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = totalMsgsToSend;
    std::string const msgStrToSend{"Test message"};

    // sockets must be added in this order so the first handler is processed
    // first
    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerRemoveOtherSocket, &sockets, _1, _2));
    loop.add(*sockets.socketPull2,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    send_now_or_throw(*sockets.socketPush, msgStrToSend);
    send_now_or_throw(*sockets.socketPush2, msgStrToSend);

    // must wait both sockets are ready to receive so the first handler is
    // processed first and the second gets ignored after the second socket is
    // removed from the loop
    waitSocketHaveMsg(*sockets.socketPull, std::chrono::milliseconds{2});
    waitSocketHaveMsg(*sockets.socketPull2, std::chrono::milliseconds{2});

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    // won't stop as the second message shall not be received after the second
    // socket is removed from the loop
    loop.run();

    t.join();

    EXPECT_EQ(totalMsgsShouldReceive, sockets.messages.size());
    EXPECT_EQ(nullptr, sockets.socketPull2.get());
}

TEST_F(UTestLoop, TimerHandlerFromOneTimerIsCalledManyTimes) {
    std::size_t const timerOcurrences{3};
    std::chrono::milliseconds timerTimeout{2};
    TimersHandlers timersHandlers{};

    auto const timerId = loop.add_timer(timerTimeout, timerOcurrences,
                                        std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    loop.run();

    ASSERT_EQ(timerOcurrences, timersHandlers.timersHandled.size());
    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerId, timerId, timerId));
}

TEST_F(UTestLoop, ManyTimerHandlersAreCalledManyTimes) {
    std::size_t const timer1Ocurrences{2};
    std::chrono::milliseconds timer1Timeout{50};
    std::size_t const timer2Ocurrences{4};
    std::chrono::milliseconds timer2Timeout{20};
    TimersHandlers timersHandlers{};

    auto const timerId1 = loop.add_timer(timer1Timeout, timer1Ocurrences,
                                         std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));
    auto const timerId2 = loop.add_timer(timer2Timeout, timer2Ocurrences,
                                         std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    loop.run();

    ASSERT_EQ(timer1Ocurrences + timer2Ocurrences, timersHandlers.timersHandled.size());
    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerId2, timerId2, timerId1, timerId2, timerId2, timerId1));
}

TEST_F(UTestLoop, KeepsRunningLoopUntilTimerHandlerReturnsFalse) {
    std::size_t const timerOcurrences{10};
    std::chrono::milliseconds timerTimeout{1};
    TimersHandlers timersHandlers{};

    auto const timerId = loop.add_timer(timerTimeout, timerOcurrences,
                                        std::bind(&TimersHandlers::timerHandlerReturnsFalse, &timersHandlers, _1, _2));

    loop.run();

    ASSERT_EQ(1, timersHandlers.timersHandled.size());
    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerId));
}

TEST_F(UTestLoop, KeepsRunningLoopUntilTimerHandlerReturnsFalse2) {
    std::size_t const timer1Ocurrences{10};
    std::chrono::milliseconds timer1Timeout{1};
    std::size_t const timer2Ocurrences{10};
    std::chrono::milliseconds timer2Timeout{1};
    TimersHandlers timersHandlers{};

    auto const timerId1 = loop.add_timer(timer1Timeout, timer1Ocurrences,
                                         std::bind(&TimersHandlers::timerHandlerReturnsFalse, &timersHandlers, _1, _2));
    auto const timerId2 = loop.add_timer(timer2Timeout, timer2Ocurrences,
                                         std::bind(&TimersHandlers::timerHandlerReturnsFalse, &timersHandlers, _1, _2));

    loop.run();

    ASSERT_EQ(1, timersHandlers.timersHandled.size());
    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerId1));
}

TEST_F(UTestLoop, TimerHandlerWithZeroOccurencesIsCalledForever) {
    std::size_t const timerOcurrences{0};
    std::chrono::milliseconds timerTimeout{1};
    std::chrono::milliseconds delayToInterrupt{20};
    std::size_t const minExpectedOccurences = (delayToInterrupt / timerTimeout) / 2;
    TimersHandlers timersHandlers{};

    // must add at least one socket so the ctx shutdown interrupts the loop
    ConnectedSocketsWithHandlers socketsToInterrupt{ctx};
    loop.add(*socketsToInterrupt.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &socketsToInterrupt, _1, _2));

    auto const timerId = loop.add_timer(timerTimeout, timerOcurrences,
                                        std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    auto t = shutdown_ctx_after_time(ctx, delayToInterrupt);

    loop.run();

    t.join();

    ASSERT_GT(timersHandlers.timersHandled.size(), minExpectedOccurences);
}

TEST_F(UTestLoop, SupportsAddingATimerInATimerHandler) {
    std::size_t const timerOcurrences{1};
    std::chrono::milliseconds timerTimeout{1};
    TimersHandlers timersHandlers{};

    auto const timerId = loop.add_timer(timerTimeout, timerOcurrences,
                                        std::bind(&TimersHandlers::timerHandlerAddTimer, &timersHandlers, _1, _2));

    loop.run();

    ASSERT_EQ(1, timersHandlers.timersAdded.size());
    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerId, timersHandlers.timersAdded[0]));
}

TEST_F(UTestLoop, SupportsRemovingTheTimerWhileItsHandlerIsExecuting) {
    std::size_t const timer1Ocurrences{2};
    std::chrono::milliseconds timer1Timeout{2};
    std::size_t const timer2Ocurrences{2};
    std::chrono::milliseconds timer2Timeout{4};
    TimersHandlers timersHandlers{};
    zmqzext::timer_id_t timerIdToRemove{0};

    timerIdToRemove = loop.add_timer(
        timer1Timeout, timer1Ocurrences,
        std::bind(&TimersHandlers::timerHandlerRemoveTimer, &timersHandlers, _1, _2, std::ref(timerIdToRemove)));
    auto const timerId2 = loop.add_timer(timer2Timeout, timer2Ocurrences,
                                         std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    loop.run();

    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerIdToRemove, timerId2, timerId2));
}

TEST_F(UTestLoop, SupportsRemovingATimerWhileOtherTimerHandlerIsExecuting) {
    std::size_t const timer1Ocurrences{2};
    std::chrono::milliseconds timer1Timeout{5};
    std::size_t const timer2Ocurrences{2};
    std::chrono::milliseconds timer2Timeout{8};
    TimersHandlers timersHandlers{};
    zmqzext::timer_id_t timerIdNotRemove{0};

    auto const timerIdToRemove = loop.add_timer(timer1Timeout, timer1Ocurrences,
                                                std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));
    auto const timerId2 = loop.add_timer(
        timer2Timeout, timer2Ocurrences,
        std::bind(&TimersHandlers::timerHandlerRemoveTimer, &timersHandlers, _1, _2, std::ref(timerIdToRemove)));

    loop.run();

    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerIdToRemove, timerId2, timerId2));
}

TEST_F(UTestLoop, TimerCanNotBeFiredWhenRemovedAndIsExpired) {
    std::size_t const timer1Ocurrences{1};
    std::chrono::milliseconds timer1Timeout{2};
    std::size_t const timer2Ocurrences{1};
    std::chrono::milliseconds timer2Timeout{2};
    TimersHandlers timersHandlers{};
    zmqzext::timer_id_t timerIdToRemove{0};

    auto const timerId1 = loop.add_timer(
        timer1Timeout, timer1Ocurrences,
        std::bind(&TimersHandlers::timerHandlerRemoveTimer, &timersHandlers, _1, _2, std::ref(timerIdToRemove)));

    timerIdToRemove = loop.add_timer(timer2Timeout, timer2Ocurrences,
                                     std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    loop.run();

    EXPECT_THAT(timersHandlers.timersHandled, ElementsAre(timerId1));
}

TEST_F(UTestLoop, SupportsAddingASocketInATimerHandler) {
    size_t const maxMsgs = 1;
    std::size_t const timerOcurrences{1};
    std::chrono::milliseconds timerTimeout{1};
    TimersHandlers timersHandlers{};
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    zmqzext::fn_socket_handler_t socketHandler =
        std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2);
    zmqzext::fn_timer_handler_t timerHandler = std::bind(&TimersHandlers::timerHandlerAddSocket, &timersHandlers, _1,
                                                         _2, zmq::socket_ref{*sockets.socketPull}, socketHandler);

    auto const timerId = loop.add_timer(timerTimeout, timerOcurrences, timerHandler);

    send_now_or_throw(*sockets.socketPush,
                      msgStrToSend);  // socket will only receive if added by timer handler

    loop.run();

    EXPECT_EQ(maxMsgs, sockets.messages.size());
}

TEST_F(UTestLoop, SupportsRemovingASocketInATimerHandler) {
    TimersHandlers timersHandlers{};
    std::size_t const timerSenderOcurrences{2};
    std::chrono::milliseconds timerSenderTimeout{40};
    std::size_t const timerRemoverOcurrences{1};
    std::chrono::milliseconds timerRemoverTimeout{60};
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = 2;

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    auto const timerSender = loop.add_timer(timerSenderTimeout, timerSenderOcurrences,
                                            std::bind(&TimersHandlers::timerHandlerSendFromSocket, &timersHandlers, _1,
                                                      _2, zmq::socket_ref{*sockets.socketPush}));

    auto const timerRemoverPull = loop.add_timer(
        timerRemoverTimeout, timerRemoverOcurrences,
        std::bind(&TimersHandlers::timerHandlerRemoveSocket, &timersHandlers, _1, _2, std::ref(sockets.socketPull)));

    loop.run();

    EXPECT_EQ(1, sockets.messages.size());
}

TEST_F(UTestLoop, SupportsAddingATimerInASocketHandler) {
    ConnectedSocketsWithHandlers sockets{ctx};
    TimersHandlers timersHandlers{};
    std::string const msgStrToSend{"Test message"};

    zmqzext::fn_timer_handler_t timerHandler = std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2);
    zmqzext::fn_socket_handler_t socketHandler =
        std::bind(&ConnectedSocketsWithHandlers::socketHandlerAddTimer, &sockets, _1, _2, timerHandler);

    loop.add(*sockets.socketPull, socketHandler);

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    loop.run();

    t.join();

    EXPECT_EQ(1, timersHandlers.timersHandled.size());
}

TEST_F(UTestLoop, SupportsRemovingATimerInASocketHandler) {
    ConnectedSocketsWithHandlers sockets{ctx};
    TimersHandlers timersHandlers{};
    std::size_t const timerOcurrences{10};
    std::chrono::milliseconds timerTimeout{2};
    zmqzext::timer_id_t timerIdToRemove{0};

    loop.add(*sockets.socketPull, std::bind(&ConnectedSocketsWithHandlers::socketHandlerRemoveTimer, &sockets, _1, _2,
                                            std::ref(timerIdToRemove)));

    send_now_or_throw(*sockets.socketPush, std::string{"Test message"});

    // must wait the scoket be ready to receive to assure the socket handler is
    // fired before the timer expires
    waitSocketHaveMsg(*sockets.socketPull, std::chrono::milliseconds{2});

    timerIdToRemove = loop.add_timer(timerTimeout, timerOcurrences,
                                     std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    loop.run();

    t.join();

    EXPECT_EQ(0, timersHandlers.timersHandled.size());
}

TEST_F(UTestLoop, HandlesConcurrentTimerRemovalAndAddition) {
    TimersHandlers timersHandlers{};
    std::size_t const timerOcurrences{1};

    auto timerId = loop.add_timer(std::chrono::milliseconds{1}, timerOcurrences, [&](loop_t& l, timer_id_t id) {
        // Remove self and add new timer
        l.remove_timer(id);
        l.add_timer(std::chrono::milliseconds{1}, 1, std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));
        return true;
    });

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});
    loop.run();
    t.join();

    EXPECT_EQ(1, timersHandlers.timersHandled.size());
}

TEST_F(UTestLoop, HandlesZeroTimeoutTimer) {
    TimersHandlers timersHandlers{};
    std::size_t const timerOcurrences{5};

    loop.add_timer(std::chrono::milliseconds{0}, timerOcurrences,
                   std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    loop.run();

    EXPECT_EQ(timerOcurrences, timersHandlers.timersHandled.size());
}

TEST_F(UTestLoop, HandlesTimerWithMaximumTimeout) {
    TimersHandlers timersHandlers{};
    auto maxTimeout = std::chrono::milliseconds::max();

    auto timerId = loop.add_timer(maxTimeout, 1, std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    // Should not block indefinitely
    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});
    loop.run();
    t.join();
}

TEST_F(UTestLoop, HandlesMultipleSocketAndTimerRemovals) {
    ConnectedSocketsWithHandlers sockets{ctx};
    TimersHandlers timersHandlers{};

    // Add multiple sockets and timers
    auto timerId1 = loop.add_timer(std::chrono::milliseconds{5}, 1,
                                   std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));
    auto timerId2 = loop.add_timer(std::chrono::milliseconds{5}, 1,
                                   std::bind(&TimersHandlers::timerHandler, &timersHandlers, _1, _2));

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));
    loop.add(*sockets.socketPull2,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    // Remove all at once
    loop.remove_timer(timerId1);
    loop.remove_timer(timerId2);
    loop.remove(*sockets.socketPull);
    loop.remove(*sockets.socketPull2);

    loop.run();  // Should exit immediately as nothing to handle
}

#if !defined(_WIN32)
TEST_F(UTestLoopWithInterruptHandler, StopsRunningWhenInterrupted) {
    ConnectedSocketsWithHandlers sockets{ctx};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    loop.run();
    t.join();
}
#else
TEST_F(UTestLoopWithInterruptHandler, StopsRunningWhenInterrupted) {
    ConnectedSocketsWithHandlers sockets{ctx};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});
    loop.run(true, std::chrono::milliseconds{5});
    t.join();
}
#endif

TEST_F(UTestLoopWithInterruptHandler, StopsRunningWhenInterruptedBeforeRun) {
    ConnectedSocketsWithHandlers sockets{ctx};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    raise_interrupt_signal();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});  // ensure signal is handled
    loop.run();
}

TEST_F(UTestLoopWithInterruptHandler, IgnoresInterruptionWhenSetToNotInterruptibleMode) {
    ConnectedSocketsWithHandlers sockets{ctx};
    bool timerRun = false;
    auto timerHandlerToFinishLoop = [&timerRun](zmqzext::loop_t&, zmqzext::timer_id_t) -> bool {
        timerRun = true;
        return false;
    };

    loop.add_timer(std::chrono::milliseconds{20}, 1, timerHandlerToFinishLoop);
    auto t = raise_interrupt_after_time(std::chrono::milliseconds{10});

    loop.run(false);
    t.join();

    EXPECT_TRUE(timerRun);
}

// Copyability Tests
TEST_F(UTestLoop, IsCopyConstructible) {
    ConnectedSocketsWithHandlers sockets{ctx};
    size_t const maxMsgs = 1;
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    // Copy construct from loop
    auto loop_copy = loop;

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop_copy.run();

    ASSERT_EQ(maxMsgs, sockets.messages.size());
    EXPECT_EQ(msgStrToSend, sockets.messages[0].to_string());
}

TEST_F(UTestLoop, IsCopyAssignable) {
    ConnectedSocketsWithHandlers sockets{ctx};
    size_t const maxMsgs = 1;
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop_t loop2;
    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    // Copy assign loop to loop2
    loop2 = loop;

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop2.run();

    ASSERT_EQ(maxMsgs, sockets.messages.size());
    EXPECT_EQ(msgStrToSend, sockets.messages[0].to_string());
}

TEST_F(UTestLoop, CopyAssignmentSelfAssignmentIsNoop) {
    ConnectedSocketsWithHandlers sockets{ctx};
    size_t const maxMsgs = 1;
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    // Self-assignment should not cause issues
    loop = loop;

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop.run();

    ASSERT_EQ(maxMsgs, sockets.messages.size());
    EXPECT_EQ(msgStrToSend, sockets.messages[0].to_string());
}

TEST_F(UTestLoop, CopiedLoopIsIndependent) {
    ConnectedSocketsWithHandlers sockets{ctx};
    ConnectedSocketsWithHandlers sockets2{ctx};
    size_t const maxMsgs = 1;
    sockets.maxMsgs = maxMsgs;
    sockets2.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop_t loop_copy;
    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));
    loop_copy = loop;

    // Remove from copy
    loop_copy.remove(*sockets.socketPull);
    // Add to copy
    loop_copy.add(*sockets2.socketPull,
                  std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets2, _1, _2));

    send_now_or_throw(*sockets.socketPush, msgStrToSend);
    send_now_or_throw(*sockets2.socketPush, msgStrToSend);

    loop.run();

    ASSERT_EQ(maxMsgs, sockets.messages.size());
    EXPECT_EQ(msgStrToSend, sockets.messages[0].to_string());

    loop_copy.run();

    ASSERT_EQ(maxMsgs, sockets2.messages.size());
    EXPECT_EQ(msgStrToSend, sockets2.messages[0].to_string());
}

// Moveability Tests
TEST_F(UTestLoop, IsMoveConstructible) {
    ConnectedSocketsWithHandlers sockets{ctx};
    size_t const maxMsgs = 1;
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop_t loop_temp;
    loop_temp.add(*sockets.socketPull,
                  std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    // Move construct from loop_temp
    auto loop_moved = std::move(loop_temp);

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop.run();  // original loop is empty, so should return immediately
    ASSERT_EQ(0, sockets.messages.size());

    loop_moved.run();

    ASSERT_EQ(maxMsgs, sockets.messages.size());
    EXPECT_EQ(msgStrToSend, sockets.messages[0].to_string());
}

TEST_F(UTestLoop, IsMoveAssignable) {
    ConnectedSocketsWithHandlers sockets{ctx};
    ConnectedSocketsWithHandlers sockets2{ctx};
    size_t const maxMsgs = 1;
    sockets.maxMsgs = maxMsgs;
    sockets2.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop_t loop_temp;
    loop_temp.add(*sockets.socketPull,
                  std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));
    loop.add(*sockets2.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets2, _1, _2));

    // Move assign loop_temp to loop
    loop = std::move(loop_temp);

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop_temp.run();  // original loop is empty, so should return immediately
    ASSERT_EQ(0, sockets.messages.size());

    loop.run();

    ASSERT_EQ(maxMsgs, sockets.messages.size());
    EXPECT_EQ(msgStrToSend, sockets.messages[0].to_string());
}

TEST_F(UTestLoop, MoveAssignmentSelfAssignmentIsNotWhatYouShouldExpect) {
    ConnectedSocketsWithHandlers sockets{ctx};
    size_t const maxMsgs = 1;
    sockets.maxMsgs = maxMsgs;
    std::string const msgStrToSend{"Test message"};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    // Self move-assignment: YOU PROBABLY SHOULD NOT DO THIS
    loop = std::move(loop);

    send_now_or_throw(*sockets.socketPush, msgStrToSend);

    loop.run();  // loop is empty, so should return immediately

    ASSERT_EQ(0, sockets.messages.size());
}

// Test multiple timers with identical timeouts firing simultaneously
// Test invalid socket references
// Test removing non-existent sockets and timers

}  // namespace zmqzext

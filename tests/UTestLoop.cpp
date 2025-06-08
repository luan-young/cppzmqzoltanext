#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <zmq.hpp>

#include <cppzmqzoltanext/loop.h>

#include "utils.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace zmqzext
{

bool socketHandlerReceiveMaxMessages(loop_t&, zmq::socket_ref socket, std::vector<zmq::message_t>& messages, std::size_t maxMsgs)
{
    assert(messages.size() < maxMsgs);
    auto msg = recv_now_or_throw(socket);
    messages.emplace_back(std::move(msg));
    if (messages.size() >= maxMsgs)
        return false;
    return true;
}

class UTestLoop : public ::testing::Test
{
public:
    loop_t loop;
    zmq::context_t ctx;
};

TEST_F(UTestLoop, SocketHandlerIsCalled)
{
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

TEST_F(UTestLoop, KeepsRunningLoopUntilHandlerReturnsFalse)
{
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

TEST_F(UTestLoop, StopsRunningIfEmpty)
{
    loop.run();
}

TEST_F(UTestLoop, StopsRunningWhenSocketContextIsShutdown)
{
    ConnectedSocketsWithHandlers sockets{ctx};

    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});

    loop.run();

    t.join();
}

TEST_F(UTestLoop, HandlerFromEachSocketIsCalled)
{
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

TEST_F(UTestLoop, SupportsRemovingTheSocketWhileItsHandlerIsExecuting)
{
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

TEST_F(UTestLoop, SupportsRemovingTheSocketWhileItsHandlerIsExecuting_MoreSocketOnLoop)
{
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

    EXPECT_EQ(totalMsgsShouldReceive, sockets.messages.size());
    EXPECT_EQ(nullptr, sockets.socketPull.get());

    t.join();
}

TEST_F(UTestLoop, SupportsRemovingASocketReadyToReceiveWhileHandlingOtherSocket)
{
    size_t const totalMsgsToSend = 2;
    size_t const totalMsgsShouldReceive = 1;
    ConnectedSocketsWithHandlers sockets{ctx};
    sockets.maxMsgs = totalMsgsToSend;
    std::string const msgStrToSend{"Test message"};

    // sockets must be added in this order so the first handler is processed first
    loop.add(*sockets.socketPull,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerRemoveOtherSocket, &sockets, _1, _2));
    loop.add(*sockets.socketPull2,
             std::bind(&ConnectedSocketsWithHandlers::socketHandlerReceiveMaxMessages, &sockets, _1, _2));

    send_now_or_throw(*sockets.socketPush, msgStrToSend);
    send_now_or_throw(*sockets.socketPush2, msgStrToSend);

    // must wait both sockets are ready to receive so the first handler is processed first and the second gets
    // ignored after the second socket is removed from the loop
    waitSocketHaveMsg(*sockets.socketPull, std::chrono::milliseconds{2});
    waitSocketHaveMsg(*sockets.socketPull2, std::chrono::milliseconds{2});

    auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{10});
    // auto t = shutdown_ctx_after_time(ctx, std::chrono::milliseconds{100000});

    // won't stop as the second message shall not be received after the second socket is removed from the loop
    loop.run();

    EXPECT_EQ(totalMsgsShouldReceive, sockets.messages.size());
    EXPECT_EQ(nullptr, sockets.socketPull2.get());

    t.join();
}

} // namespace zmqzext

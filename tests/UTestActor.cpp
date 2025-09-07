#include <cppzmqzoltanext/actor.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <zmq.hpp>

#include "utils.h"

using namespace std::chrono_literals;
using ::testing::ElementsAre;

namespace zmqzext {

class user_exception : public std::runtime_error {
public:
    explicit user_exception()
        : std::runtime_error("Specific error occurred") {}
};

class UTestActor : public ::testing::Test {
public:
    zmq::context_t ctx;

    // TODO: replace signals strings with a signal class
    std::string success_msg = "success";
    std::string failure_msg = "failure";
    std::string stop_msg = "stop";

    bool simpleActorFunction(zmq::socket_t& socket) {
        // Send success signal
        socket.send(zmq::message_t{success_msg.data(), success_msg.size()},
                    zmq::send_flags::none);

        // process messages until receiving stop message
        while (true) {
            try {
                zmq::message_t msg;
                auto result = socket.recv(msg, zmq::recv_flags::none);
                if (result && msg.to_string() == stop_msg) {
                    return true;
                }
            } catch (...) {
                // Ignore exceptions and continue
            }
        }
        return false;
    }

    bool busyActorFunction(zmq::socket_t& socket, std::chrono::milliseconds busy_time) {
        // Send success signal
        socket.send(zmq::message_t{success_msg.data(), success_msg.size()},
                    zmq::send_flags::none);

        // Simulate being busy for a while before checking for stop message
        std::this_thread::sleep_for(busy_time);

        // process messages until receiving stop message
        while (true) {
            try {
                zmq::message_t msg;
                auto result = socket.recv(msg, zmq::recv_flags::none);
                if (result && msg.to_string() == stop_msg) {
                    return true;
                }
            } catch (...) {
                // Ignore exceptions and continue
            }
        }
        return false;
    }

    bool failingDuringInitializationActorFunction(zmq::socket_t& socket) {
        // Return false before sending success signal to indicate failure during initialization
        return false;
    }

    bool throwingDuringInitializationActorFunction(zmq::socket_t&) {
        // Throw before sending success signal
        throw user_exception();
    }

    bool badActorFunctionThatReturnsWithoutBeingRequested(zmq::socket_t& socket) {
        // Send success signal
        socket.send(zmq::message_t{success_msg.data(), success_msg.size()}, zmq::send_flags::none);

        // Simulate work being done
        std::this_thread::sleep_for(10ms);

        // Return false to indicate failure during operation
        return false;
    }

    // bool nonRespondingActorFunction(zmq::socket_t& socket) {
    //     // Never sends success signal
    //     std::this_thread::sleep_for(1s);
    //     return true;
    // }
};

TEST_F(UTestActor, NormalExecution) {
    actor_t actor{ctx};

    EXPECT_FALSE(actor.is_started());
    EXPECT_FALSE(actor.is_stopped());

    actor.start(std::bind(&UTestActor::simpleActorFunction, this, std::placeholders::_1));

    EXPECT_TRUE(actor.is_started());
    EXPECT_FALSE(actor.is_stopped());

    EXPECT_TRUE(actor.stop());

    EXPECT_TRUE(actor.is_started());
    EXPECT_TRUE(actor.is_stopped());
}

TEST_F(UTestActor, FailureDuringStart) {
    actor_t actor{ctx};

    EXPECT_THROW(actor.start(std::bind(&UTestActor::failingDuringInitializationActorFunction, this,
                                       std::placeholders::_1)),
                 std::runtime_error);

    EXPECT_TRUE(actor.is_started());
    EXPECT_TRUE(actor.is_stopped());
}

TEST_F(UTestActor, ExceptionDuringStart) {
    actor_t actor{ctx};

    EXPECT_THROW(actor.start(std::bind(&UTestActor::throwingDuringInitializationActorFunction, this,
                                       std::placeholders::_1)),
                 user_exception);

    EXPECT_TRUE(actor.is_started());
    EXPECT_TRUE(actor.is_stopped());
}

TEST_F(UTestActor, StopWithInsufficientTimeout) {
    actor_t actor{ctx};

    actor.start(
        std::bind(&UTestActor::busyActorFunction, this, std::placeholders::_1, 100ms));

    // Try to stop with a very short timeout
    EXPECT_FALSE(actor.stop(10ms));

    EXPECT_TRUE(actor.is_stopped());
}

TEST_F(UTestActor, StopWithSufficientTimeout) {
    actor_t actor{ctx};

    actor.start(
        std::bind(&UTestActor::busyActorFunction, this, std::placeholders::_1, 10ms));

    // Try to stop with a sufficient timeout
    EXPECT_TRUE(actor.stop(100ms));

    EXPECT_TRUE(actor.is_stopped());
    // std::cout << "Actor stopped successfully with sufficient timeout\n";
}

TEST_F(UTestActor, DestructorWithRunningActor) {
    actor_t actor{ctx};

    EXPECT_FALSE(actor.is_started());
    EXPECT_FALSE(actor.is_stopped());

    actor.start(std::bind(&UTestActor::simpleActorFunction, this, std::placeholders::_1));
}

TEST_F(UTestActor, DestructorWithFailureDuringOperation) {
    {
        actor_t actor{ctx};
        actor.start(std::bind(&UTestActor::badActorFunctionThatReturnsWithoutBeingRequested,
                            this, std::placeholders::_1));

        // receive message from actor
        zmq::message_t msg;
        auto result = actor.socket().recv(msg, zmq::recv_flags::none);
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(failure_msg, msg.to_string());
        // Destructor should call stop in not blocking mode
    }
}

TEST_F(UTestActor, StopWithFailureDuringOperation_MayBlockForever) {

    bool blockedOnce = false;
    for (int i = 0; i < 10; ++i) {
        actor_t actor{ctx};
        actor.start(std::bind(&UTestActor::badActorFunctionThatReturnsWithoutBeingRequested,
                            this, std::placeholders::_1));

        // receive message from actor
        zmq::message_t msg;
        auto result = actor.socket().recv(msg, zmq::recv_flags::none);
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(failure_msg, msg.to_string());

        // Try to stop - could block forever if given infinite timeout
        auto const initTime = std::chrono::steady_clock::now();
        auto const stopResult = actor.stop(std::chrono::milliseconds{10});
        auto const endTime = std::chrono::steady_clock::now();
        if (endTime - initTime >= 10ms) {
            blockedOnce = true;
            break;
        }
    }
    EXPECT_TRUE(blockedOnce);
}

// // Test socket communication
// TEST_F(UTestActor, SocketCommunication) {
//     actor_t actor{ctx};

//     bool messageReceived = false;
//     auto communicatingActorFunction = [&](zmq::socket_t& socket) {
//         socket.send(zmq::message_t{success_msg.data(), success_msg.size()},
//                     zmq::send_flags::none);

//         // Wait for test message
//         zmq::message_t msg;
//         auto result = socket.recv(msg, zmq::recv_flags::none);
//         if (result.has_value() && msg.to_string() == "test") {
//             socket.send(zmq::message_t{"reply", 5}, zmq::send_flags::none);
//         }

//         // Wait for stop message
//         result = socket.recv(msg, zmq::recv_flags::none);
//         return result.has_value() && msg.to_string() == stop_msg;
//     };

//     actor.start(communicatingActorFunction);

//     // Send test message through parent socket
//     actor.socket().send(zmq::message_t{"test", 4}, zmq::send_flags::none);

//     // Receive reply
//     zmq::message_t reply;
//     auto result = actor.socket().recv(reply, zmq::recv_flags::none);
//     EXPECT_TRUE(result.has_value());
//     EXPECT_EQ("reply", reply.to_string());

//     EXPECT_TRUE(actor.stop());
// }

// // Test starting already started actor
// TEST_F(UTestActor, StartTwice) {
//     actor_t actor{ctx};

//     actor.start(std::bind(&UTestActor::simpleActorFunction, this,
//                           std::placeholders::_1));

//     EXPECT_THROW(actor.start(std::bind(&UTestActor::simpleActorFunction, this,
//                                        std::placeholders::_1)),
//                  std::runtime_error);
// }

// // Test stopping non-started actor
// TEST_F(UTestActor, StopNonStartedActor) {
//     actor_t actor{ctx};
//     EXPECT_FALSE(actor.stop());
// }

// // Test stopping already stopped actor
// TEST_F(UTestActor, StopTwice) {
//     actor_t actor{ctx};

//     actor.start(std::bind(&UTestActor::simpleActorFunction, this,
//                           std::placeholders::_1));

//     EXPECT_TRUE(actor.stop());
//     EXPECT_FALSE(actor.stop());
// }

} // namespace zmqzext

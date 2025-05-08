#include <cppzmqzoltanext/poller.h>
#include <gtest/gtest.h>

namespace zmqzext
{
class UTestPoller : public ::testing::Test
{
    poller_t poller;
};

TEST_F(UTestPoller, Instantiates)
{
    ASSERT_TRUE(true);
}
} // namespace zmqzext

#include "gtest/gtest.h"

#include "misa/tetris_gem.h"

namespace AI {
    using namespace std::literals::string_literals;

    class SampleTest : public ::testing::Test {
    };

    TEST_F(SampleTest, case1) {
        auto gem = AI::GEMTYPE_NULL;
        EXPECT_EQ(gem, 0);
    }
}

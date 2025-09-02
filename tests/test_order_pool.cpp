#include <gtest/gtest.h>
#include "order_pool.h"

class OrderPoolTest: public::testing::Test {
    protected:
        Matching::OrderPool pool;
};

TEST_F(OrderPoolTest, addOrder) {
    auto index = pool.add_order(123, 100, true, 456);
    ASSERT_TRUE(index);
    EXPECT_EQ(index, 1);
}

TEST_F(OrderPoolTest, markDead) {
    auto index = pool.add_order(123, 100, true, 456);
    ASSERT_TRUE(index);
    ASSERT_TRUE(pool.is_active(*index));
    pool.mark_dead(*index);
    ASSERT_FALSE(pool.is_active(*index));

}
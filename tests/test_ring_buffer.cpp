#include <gtest/gtest.h>
#include "ring_buffer.h"
#include "order_pool.h"

class RingBufferTest: public::testing::Test {
    protected:
        RingBuffer rb;
        Matching::OrderPool pool;
};

TEST_F(RingBufferTest, EnqueueDequeue) {
    EXPECT_TRUE(rb.is_empty());
    EXPECT_FALSE(rb.is_full());

    auto index = pool.add_order(123, 100, true, 456);
    ASSERT_TRUE(index);
    EXPECT_TRUE(rb.enqueue(*index));
    auto result = rb.dequeue(pool);
    EXPECT_TRUE(result);
    EXPECT_EQ(*result, *index);
    EXPECT_TRUE(rb.is_empty());
}

TEST_F(RingBufferTest, DequeueDeadOrders) {
    for (int i = 0; i < 10; ++i) {
        auto index = pool.add_order(i, 100, true, 456);
        ASSERT_TRUE(index);
        if (i % 2 == 0) pool.mark_dead(*index); // 50% dead
        rb.enqueue(*index);
    }
    int live_count = 0;
    while (rb.dequeue(pool)) ++live_count;
    EXPECT_EQ(live_count, 5); // 50% live
}
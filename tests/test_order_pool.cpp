#include "order_pool.h"
#include "telemetry.h"
#include <gtest/gtest.h>

class OrderPoolTest : public ::testing::Test {
protected:
  Telemetry telemetry_;
  Matching::OrderPool pool_{telemetry_, 8};
};

TEST_F(OrderPoolTest, AllocateAndFindOrder) {
  auto *o1 = pool_.allocate(1001, 50, true, 42);
  ASSERT_NE(o1, nullptr);
  EXPECT_EQ(o1->order_id, 1001);
  EXPECT_EQ(o1->quantity, 50);
  EXPECT_EQ(o1->side, Side::Bid);
  EXPECT_EQ(o1->account_id, 42);

  auto *found = pool_.find(1001);
  EXPECT_EQ(found, o1);
}

TEST_F(OrderPoolTest, DeallocateOrder) {
  auto *o1 = pool_.allocate(1001, 50, false, 40);
  auto *found = pool_.find(1001);

  ASSERT_NE(o1, nullptr);
  EXPECT_EQ(found, o1);

  pool_.deallocate(1001);
  auto *find2 = pool_.find(1001);
  EXPECT_EQ(find2, nullptr);
}

TEST_F(OrderPoolTest, DeallocateAndReuseSlot) {
  auto *o1 = pool_.allocate(1, 10, true, 101);
  pool_.allocate(2, 10, false, 101);
  pool_.deallocate(1);

  auto *o3 = pool_.allocate(3, 5, false, 102);
  EXPECT_EQ(o1, o3);
  EXPECT_EQ(pool_.find(1), nullptr);
  EXPECT_EQ(pool_.find(3), o3);
}

TEST_F(OrderPoolTest, SlabExpansion) {
  for (int i = 0; i < 9; i++) {
    pool_.allocate(i, 1, true, i);
  }
  EXPECT_GE(telemetry_.total_allocs.load(), 9u);
}

TEST_F(OrderPoolTest, DoubleDeallocateDoesNotCrash) {
  auto *o1 = pool_.allocate(1, 100, true, 1);
  ASSERT_NE(o1, nullptr);

  pool_.deallocate(1);
  EXPECT_EQ(pool_.find(1), nullptr);

  // Second call should be ignored
  pool_.deallocate(1);
  EXPECT_EQ(pool_.find(1), nullptr);
}

TEST_F(OrderPoolTest, ReuseFollowsLIFOOrder) {
  auto _ = pool_.allocate(1, 1, true, 1);
  auto *o2 = pool_.allocate(2, 1, true, 1);
  pool_.deallocate(1);
  pool_.deallocate(2);
  auto *o3 = pool_.allocate(3, 1, true, 1);
  EXPECT_EQ(o3, o2); // LIFO expected
}

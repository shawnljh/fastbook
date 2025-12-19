#include <bitset_book.h>
#include <cstdint>
#include <gtest/gtest.h>

class BitsetTest : public ::testing::Test {
protected:
  bitsetbook book;
  void SetUp() override { book = bitsetbook(); }
};

// Verify that BIDS prefer HIGHER indices (clz check)
TEST_F(BitsetTest, BidOrdering_HighVsLow) {
  // Scenario: Bids at idx 2 and idx 3
  book.add_level(2);
  book.add_level(3);

  // Search from 4 downwards. Should pick 3 (Highest), not 2 (Lowest).
  uint64_t best_bid = book.get_next_lowest_setbit(4);

  EXPECT_EQ(best_bid, 3) << "Bid should return highest index (clz logic)";

  // find next from 3
  best_bid = book.get_next_lowest_setbit(3);
  EXPECT_EQ(best_bid, 2);
}

// Verify that ASKS prefer LOWER indices (ctz check)
TEST_F(BitsetTest, AskOrdering_LowVsHigh) {
  // Scenario: Asks at idx 2 and idx 3
  book.add_level(2);
  book.add_level(3);

  // Search from 1 upwards. Should pick 2 (Lowest), not 3 (Highest).
  uint64_t best_ask = book.get_next_highest_setbit(1);

  EXPECT_EQ(best_ask, 2) << "Ask should return lowest index (ctz logic)";

  // find next from 2
  best_ask = book.get_next_highest_setbit(2);
  EXPECT_EQ(best_ask, 3)
      << "Ask should return lowest index not including current";
}

// Verify Strict Masking (Prevent Infinite Loop)
TEST_F(BitsetTest, StrictMask_NoInfiniteLoop) {
  book.add_level(10);

  // Looking for bid strictly < 10. Should NOT return 10.
  EXPECT_EQ(book.get_next_lowest_setbit(10), UINT64_MAX);

  // Looking for ask strictly > 10. Should NOT return 10.
  EXPECT_EQ(book.get_next_lowest_setbit(10), UINT64_MAX);
}

// Verify Crossing 64-bit boundaries (L2 Blocks)
TEST_F(BitsetTest, BoundaryCheck_L2_Crossing) {
  // 63 is end of Block 0. 64 is start of Block 1.
  book.add_level(63);
  book.add_level(64);

  // Bid Search Down: From 65 -> Should find 64 (Block 1) -> 63 (Block 0)
  EXPECT_EQ(book.get_next_lowest_setbit(65), 64);
  EXPECT_EQ(book.get_next_lowest_setbit(64), 63);

  // Ask Search Up: From 62 -> Should find 63 (Block 0) -> 64 (Block 1)
  EXPECT_EQ(book.get_next_highest_setbit(62), 63);
  EXPECT_EQ(book.get_next_highest_setbit(63), 64);
}

// Verify Crossing 4096-bit boundaries (L1 Blocks)
TEST_F(BitsetTest, BoundaryCheck_L1_Crossing) {
  // 4095 is end of L1[0]. 4096 is start of L1[1].
  book.add_level(4095);
  book.add_level(4096);

  // Crossing UP
  EXPECT_EQ(book.get_next_highest_setbit(4095), 4096);

  // Crossing DOWN
  EXPECT_EQ(book.get_next_lowest_setbit(4096), 4095);
}

// Verify Sparse Skips (The whole point of bitsets)
TEST_F(BitsetTest, Performance_SparseSkip) {
  book.add_level(0);
  book.add_level(16000); // Far away

  // Should jump instantly from 0 to 16000 without looping
  EXPECT_EQ(book.get_next_highest_setbit(0), 16000);

  // Should jump instantly from 16000 to 0
  EXPECT_EQ(book.get_next_lowest_setbit(16000), 0);
}

// Verify Add/Remove Logic
TEST_F(BitsetTest, AddRemove_Consistency) {
  book.add_level(50);
  EXPECT_EQ(book.get_next_lowest_setbit(0), UINT64_MAX);

  book.remove_level(50);
  EXPECT_EQ(book.get_next_highest_setbit(0), UINT64_MAX);

  // Re-add to ensure L1/L2 clean state wasn't broken
  book.add_level(50);
  EXPECT_EQ(book.get_next_highest_setbit(0), 50);
}

TEST_F(BitsetTest, BoundsCheck_EdgeCases) {
  // A. Zero Boundary
  // If we are at 0, we cannot find a lower bit.
  EXPECT_EQ(book.get_next_lowest_setbit(0), UINT64_MAX);

  // B. Max Boundary
  // If we are at the very last tick, we cannot find a higher bit.
  uint64_t max_tick = bitsetbook::TICK_COUNT - 1;
  EXPECT_EQ(book.get_next_highest_setbit(max_tick), UINT64_MAX);

  // C. Out of Bounds (OOB) Input
  // Inputs >= TICK_COUNT should be rejected immediately.
  EXPECT_EQ(book.get_next_lowest_setbit(bitsetbook::TICK_COUNT), UINT64_MAX);
  EXPECT_EQ(book.get_next_highest_setbit(bitsetbook::TICK_COUNT), UINT64_MAX);
  EXPECT_EQ(book.get_next_lowest_setbit(99999999), UINT64_MAX); // Massive OOB
}

TEST_F(BitsetTest, BoundsCheck_AddRemoveOOB) {
  // Ensure adding/removing OOB indices doesn't crash or corrupt memory
  book.add_level(bitsetbook::TICK_COUNT);      // Should be ignored
  book.add_level(bitsetbook::TICK_COUNT + 50); // Should be ignored

  // Verify book is still empty
  EXPECT_EQ(book.get_next_highest_setbit(0), UINT64_MAX);

  // Ensure removing OOB is safe
  book.remove_level(bitsetbook::TICK_COUNT + 50);
}

#include "orderbook.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

class OrderBookMarketTest : public ::testing::Test {
protected:
  Orderbook book = Orderbook();

  void SetUp() override {
    book.addOrder(1, 100, 50, false, 101); // ask @100, qty 50
    book.addOrder(2, 101, 30, false, 102); // ask @101, qty 30
    book.addOrder(3, 99, 40, true, 103);   // bid @99, qty 40
    book.addOrder(4, 98, 20, true, 104);   // bid @98, qty 20
  }
};

TEST_F(OrderBookMarketTest, MarketBuyConsumesAsk) {
  // Market BUY for 60 units
  uint64_t remaining = book.matchMarketOrder(true, 60);

  // Expect to have consumed 50 @100 and 10 @101
  EXPECT_EQ(remaining, 0);
  EXPECT_EQ(book.asks().size(), 1); // 101 level partially remains
  EXPECT_EQ(book.asks().back()->price, 101);
  EXPECT_EQ(book.asks().back()->volume, 20); // 30 - 10 = 20
}

TEST_F(OrderBookMarketTest, MarketSellConsumesBids) {
  // market SELL order for 50 units
  uint64_t remaining = book.matchMarketOrder(false, 50);

  // Expect to consume all 40 @99 and 10 @98 (partial)
  EXPECT_EQ(remaining, 0);
  EXPECT_EQ(book.bids().size(), 1); // 98 partially remains
  EXPECT_EQ(book.bids().back()->price, 98);
  EXPECT_EQ(book.bids().back()->volume, 10); // 20 - 10 = 10
}

TEST_F(OrderBookMarketTest, MarketOrderPartialFill) {
  // create SELL order for 70
  uint64_t remaining = book.matchMarketOrder(false, 70);

  // Should consume all available bids (60)
  EXPECT_GT(remaining, 10);
  EXPECT_EQ(book.bids().size(), 0); // all asks consumed
  EXPECT_EQ(book.asks().size(), 2); // unchanged
}

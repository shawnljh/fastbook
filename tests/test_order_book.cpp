#include "orderbook.h"
#include <gtest/gtest.h>

class OrderBookTest : public ::testing::Test {
protected:
  Orderbook book = Orderbook();
};

TEST_F(OrderBookTest, InsertBidAndAskSetsBestPrices) {
  auto [beforeBestBid, beforeBestAsk] = book.getBestPrices();
  ASSERT_FALSE(beforeBestBid.has_value());
  ASSERT_FALSE(beforeBestAsk.has_value());

  // bid
  book.addOrder(1, 100, 10, true, 1);

  // ask
  book.addOrder(2, 105, 5, false, 2);

  auto [bestBid, bestAsk] = book.getBestPrices();

  ASSERT_TRUE(bestBid.has_value());
  ASSERT_TRUE(bestAsk.has_value());
  auto [bestBidPrice, bestBidQuantity] = bestBid.value();
  auto [bestAskPrice, bestAskQuantity] = bestAsk.value();
  ASSERT_EQ(bestBidPrice, 100);
  ASSERT_EQ(bestBidQuantity, 10);
  ASSERT_EQ(bestAskPrice, 105);
  ASSERT_EQ(bestAskQuantity, 5);
}

TEST_F(OrderBookTest, CrossedOrdersMatchImmediately) {
  book.addOrder(1, 100, 10, true, 1);
  book.addOrder(2, 99, 10, false, 2);

  auto [bestBid, bestAsk] = book.getBestPrices();

  EXPECT_FALSE(bestBid.has_value());
  EXPECT_FALSE(bestAsk.has_value());
}

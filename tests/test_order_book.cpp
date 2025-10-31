#include "orderbook.h"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <vector>

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

TEST_F(OrderBookTest, RemainingCrossShouldRest) {
  book.addOrder(1, 100, 10, true, 1);
  book.addOrder(2, 99, 11, false, 2);

  auto [bestBid, bestAsk] = book.getBestPrices();

  EXPECT_FALSE(bestBid.has_value());
  EXPECT_TRUE(bestAsk.has_value());
  auto [bestAskPrice, bestAskQuantity] = bestAsk.value();
  EXPECT_EQ(bestAskPrice, 99);
  EXPECT_EQ(bestAskQuantity, 1);
}

TEST_F(OrderBookTest, PartialFillLeavesRemainingVolume) {
  book.addOrder(1, 100, 10, true, 1);
  book.addOrder(2, 99, 4, false, 2);

  auto [bestBid, bestAsk] = book.getBestPrices();
  ASSERT_TRUE(bestBid.has_value());
  ASSERT_EQ(bestBid->second, 6); // 10 - 4
  EXPECT_FALSE(bestAsk.has_value());
}

TEST_F(OrderBookTest, MaintainsSortedPriceLevels) {
  book.addOrder(1, 100, 5, true, 1);
  book.addOrder(2, 99, 5, true, 2);
  book.addOrder(3, 101, 5, true, 3);

  book.addOrder(4, 200, 5, false, 1);
  book.addOrder(5, 150, 5, false, 2);
  book.addOrder(6, 250, 5, false, 3);
  EXPECT_EQ(book.bids().front()->price, 99);
  EXPECT_EQ(book.bids().back()->price, 101);
  EXPECT_EQ(book.asks().front()->price, 250);
  EXPECT_EQ(book.asks().back()->price, 150);
}

TEST_F(OrderBookTest, VolumeAndSizeIncrementsCorrectly) {
  book.addOrder(1, 100, 3, true, 1);
  auto &bid_level = *book.bids().back();
  EXPECT_EQ(bid_level.size, 1);
  EXPECT_EQ(bid_level.volume, 3);

  book.addOrder(2, 100, 5, true, 2);
  EXPECT_EQ(bid_level.size, 2);
  EXPECT_EQ(bid_level.volume, 8);

  book.addOrder(3, 100, 7, true, 3);
  EXPECT_EQ(bid_level.size, 3);
  EXPECT_EQ(bid_level.volume, 15);

  book.addOrder(4, 101, 3, false, 1);
  auto &ask_level = *book.asks().back();
  EXPECT_EQ(ask_level.size, 1);
  EXPECT_EQ(ask_level.volume, 3);

  book.addOrder(5, 101, 5, false, 2);
  EXPECT_EQ(ask_level.size, 2);
  EXPECT_EQ(ask_level.volume, 8);

  book.addOrder(6, 101, 7, false, 3);
  EXPECT_EQ(ask_level.size, 3);
  EXPECT_EQ(ask_level.volume, 15);
}

TEST_F(OrderBookTest, RemoveOrderRemovesLevel) {
  book.addOrder(1, 100, 10, true, 1);
  book.addOrder(2, 200, 10, false, 2);

  ASSERT_EQ(book.bids().size(), 1);
  ASSERT_EQ(book.asks().size(), 1);

  book.removeOrder(1);
  book.removeOrder(2);

  EXPECT_TRUE(book.bids().empty());
  EXPECT_TRUE(book.asks().empty());
}

TEST_F(OrderBookTest, RemoveMiddleOrderMaintainsListIntegrity) {
  book.addOrder(1, 100, 5, true, 1);
  book.addOrder(2, 100, 5, true, 2);
  book.addOrder(3, 100, 5, true, 3);

  book.removeOrder(2);

  auto &level = *book.bids().back();
  EXPECT_EQ(level.size, 2);
  EXPECT_EQ(level.volume, 10);
  EXPECT_EQ(level.sentinel.next->order_id, 1);
  EXPECT_EQ(level.sentinel.prev->order_id, 3);
}

TEST_F(OrderBookTest, MatchExactlyRemovesLevel) {
  book.addOrder(1, 100, 10, true, 1);
  book.addOrder(2, 100, 10, false, 2);

  EXPECT_TRUE(book.bids().empty());
  EXPECT_TRUE(book.asks().empty());
}

TEST_F(OrderBookTest, MultipleOpposingLevelsMatchInSequence) {
  book.addOrder(1, 101, 5, false, 1);
  book.addOrder(2, 100, 5, false, 2);
  book.addOrder(3, 99, 5, false, 3);

  // One big buyer crosses all three
  book.addOrder(10, 102, 20, true, 9);

  EXPECT_TRUE(book.asks().empty());
  auto [bestBid, bestAsk] = book.getBestPrices();
  EXPECT_TRUE(bestAsk.has_value() == false);
  EXPECT_TRUE(bestBid.has_value());
  EXPECT_EQ(bestBid->second, 5); // Remaining 5 unfilled
}

TEST_F(OrderBookTest, LevelPointerRemainsStable) {
  book.addOrder(1, 100, 5, true, 1);
  auto levelPtr = book.bids().back().get();

  book.addOrder(2, 99, 5, true, 2);
  book.addOrder(3, 101, 5, true, 3);

  EXPECT_EQ(book.bids()[1].get(), levelPtr); // 100 still same address
}

TEST_F(OrderBookTest, CancelNonexistentOrderSafe) {
  book.addOrder(1, 100, 5, true, 1);
  EXPECT_NO_THROW(book.removeOrder(9999));
  EXPECT_EQ(book.bids().size(), 1);
  book.removeOrder(1);
  EXPECT_NO_THROW(book.removeOrder(1));
}

TEST_F(OrderBookTest, TelemetryTracksCounts) {
  auto startAllocs = book.telemetry_.total_allocs.load();
  book.addOrder(1, 100, 5, true, 1);
  EXPECT_GT(book.telemetry_.total_allocs.load(), startAllocs);

  book.addOrder(2, 99, 5, false, 2);
  EXPECT_GT(book.telemetry_.matched_orders.load(), 0u);
}

TEST_F(OrderBookTest, RemoveTailMaintainsListIntegrity) {
  book.addOrder(1, 100, 5, true, 1);
  book.addOrder(2, 100, 5, true, 2);
  book.addOrder(3, 100, 5, true, 3);

  book.removeOrder(3);
  auto &level = *book.bids().back();
  EXPECT_EQ(level.sentinel.next->order_id, 1);
  EXPECT_EQ(level.sentinel.prev->order_id, 2);
  EXPECT_EQ(level.size, 2);
}

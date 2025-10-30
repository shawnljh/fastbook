#include "order.h"
#include "types.h"
#include <arpa/inet.h>
#include <cstring>
#include <gtest/gtest.h>

TEST(OrderTest, ParseOrder) {
  std::vector<unsigned char> payload(26);
  payload[0] = 0; // Buy
  uint64_t price = Client::swap_uint64(100);
  std::memcpy(&payload[1], &price, sizeof(price));
  uint64_t quantity = Client::swap_uint64(50);
  std::memcpy(&payload[9], &quantity, sizeof(quantity));
  uint64_t account_id = Client::swap_uint64(456);
  std::memcpy(&payload[17], &account_id, sizeof(account_id));
  payload[25] = 0; // Limit

  auto order = Client::parse_order(payload);
  EXPECT_EQ(order.side, Side::Bid);
  EXPECT_EQ(order.price, 100);
  EXPECT_EQ(order.quantity, 50);
  EXPECT_EQ(order.account_id, 456);
  EXPECT_EQ(order.order_type, OrderType::Limit);
}

#include <gtest/gtest.h>
#include <order.h>
#include <arpa/inet.h>
#include <cstring>


TEST(OrderTest, ParseOrder) {
    std::vector<unsigned char> payload(25);
    payload[0] = 0; // Buy
    uint64_t price = Client::swap_uint64(100);
    std::memcpy(&payload[1], &price, sizeof(price));
    uint64_t quantity = Client::swap_uint64(50);
    std::memcpy(&payload[9], &quantity, sizeof(quantity));
    uint64_t account_id = Client::swap_uint64(456);
    std::memcpy(&payload[17], &account_id, sizeof(account_id));

    auto order = Client::parse_order(payload);
    EXPECT_EQ(order.side, Client::Order::Side::Buy);
    EXPECT_EQ(order.price, 100);
    EXPECT_EQ(order.quantity, 50);
    EXPECT_EQ(order.account_id, 456);
}


#pragma once

#include <cstdint>
#include <vector>
#include <string>

struct Order {
    enum class Side: uint8_t {Buy = 0, Sell = 1};
    Side side;
    uint64_t price;
    uint64_t quantity;
};

// Parses an Order from a binary payload
Order parse_order(const std::vector<uint8_t>& payload);

std::string side_to_string(Order::Side side);
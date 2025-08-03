#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace Client {
    struct alignas(32) Order {
        enum class Side: uint8_t {Buy = 0, Sell = 1};
        uint64_t price;
        uint64_t quantity;
        uint64_t account_id;
        Side side;
        char padding[7];
    };
    static_assert(sizeof(Order) == 32, "Client::Order size is not 32 bytes");
    static_assert(alignof(Order) == 32, "Client::Order alignment is not 16 bytes");

    // Parses an Order from a binary payload
    Order parse_order(const std::vector<uint8_t>& payload);

    uint64_t swap_uint64(uint64_t val);

    std::string side_to_string(Order::Side side);
}
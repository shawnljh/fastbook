#include "order.h"
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>

double parse_double(const uint8_t* bytes) {
    uint8_t swapped[8];
    for (int i = 0; i < 8; ++i) {
        swapped[i] = bytes[7 - i];
    }
    double result;
    std::memcpy(&result, swapped, sizeof(result));
    return result;
}

double swap_uint64(uint64_t val) {
    uint64_t swapped = 0;
    for (int i = 0; i < 8; ++i) {
        swapped = (swapped << 8) | (val & 0xFF);
        val >>= 8;
    }
    return swapped;
}

Order parse_order(const std::vector<uint8_t>& payload) {
    if (payload.size() != 17) {
        throw std::runtime_error("Invalid payload size for Order");
    }

    Order order;
    order.side = static_cast<Order::Side>(payload[0]);
    uint64_t price_net;
    std::memcpy(&price_net, &payload[1], sizeof(price_net));
    order.price = swap_uint64(price_net);

    uint64_t qty_net;
    std::memcpy(&qty_net, &payload[9], sizeof(qty_net));
    order.quantity = swap_uint64(qty_net);

    return order;
}

std::string side_to_string(Order::Side side) {
    return (side == Order::Side::Buy) ? "Buy" : "Sell";
}
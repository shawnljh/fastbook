#pragma once

#include "types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Client {

#pragma pack(push, 1)
struct Order {
  Side side;
  OrderType order_type;
  char padding[2];
  uint32_t account_id;
  uint64_t price;
  uint64_t quantity;
  uint64_t order_id; //
};
#pragma pack(pop)

static_assert(sizeof(Order) == 32, "Client::Order size is not 32 bytes");
static_assert(alignof(Order) == 1, "Client::Order alignment is not 1 bytes");

// Parses an Order from a binary payload
Order parse_order(const std::vector<uint8_t> &payload);

std::string side_to_string(Side side);
}; // namespace Client

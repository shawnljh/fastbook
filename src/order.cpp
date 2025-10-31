#include "order.h"
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

double parse_double(const uint8_t *bytes) {
  uint8_t swapped[8];
  for (int i = 0; i < 8; ++i) {
    swapped[i] = bytes[7 - i];
  }
  double result;
  std::memcpy(&result, swapped, sizeof(result));
  return result;
}

uint64_t Client::swap_uint64(uint64_t val) {
  uint64_t swapped = 0;
  for (int i = 0; i < 8; ++i) {
    swapped = (swapped << 8) | (val & 0xFF);
    val >>= 8;
  }
  return swapped;
}

Client::Order Client::parse_order(const std::vector<uint8_t> &payload) {
  if (payload.size() != 34) {
    // 1 + 1 + 8 + 8 + 8 Bytes

    std::string error_msg =
        "Invalid payload size for Order, expected: " + std::to_string(34) +
        ", was: " + std::to_string(payload.size());
    throw std::runtime_error(error_msg);
  }

  Client::Order order;

  order.order_type = static_cast<OrderType>(payload[0]);
  order.side = static_cast<Side>(payload[1]);
  uint64_t price_net;
  std::memcpy(&price_net, &payload[2], sizeof(price_net));
  order.price = Client::swap_uint64(price_net);

  uint64_t qty_net;
  std::memcpy(&qty_net, &payload[10], sizeof(qty_net));
  order.quantity = Client::swap_uint64(qty_net);

  uint64_t account_id_net;
  std::memcpy(&account_id_net, &payload[18], sizeof(account_id_net));
  order.account_id = Client::swap_uint64(account_id_net);

  // we only use this for cancel for now
  uint64_t order_id_net;
  std::memcpy(&order_id_net, &payload[26], sizeof(order_id_net));
  order.order_id = Client::swap_uint64(order_id_net);

  return order;
}

std::string Client::side_to_string(Side side) {
  return (side == Side::Bid) ? "Bid" : "Ask";
}

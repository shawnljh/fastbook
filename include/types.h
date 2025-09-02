#pragma once

using Price = uint64_t;
using Volume = uint64_t;
using OrderId = uint64_t;
using AccountId = uint64_t;
using Tick = uint32_t;

enum class Side : uint8_t {Bid = 0, Ask = 1};
inline bool is_bid(Side s) { return s == Side::Bid;}
inline Side opposite(Side s) { return is_bid(s) ? Side::Ask : Side::Bid; }
static_asset(sizeOf(Side) == 1, "Side must be 1 byte");
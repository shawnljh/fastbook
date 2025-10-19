#pragma once
#include "order_pool.h"
#include "types.h"
#include <cstdint>
#include <optional>
#include <sys/types.h>
#include <utility>
#include <vector>

struct MatchResult {
  uint64_t total_traded;
  Volume remaining;
};

struct Level {
  Price price{};
  Volume volume{};
  uint32_t size{0};
  Matching::Order sentinel;

  Level() {
    sentinel.prev = &sentinel;
    sentinel.next = &sentinel;
    sentinel.type = Matching::NodeType::Sentinel;
  }

  Level(Price p) : price(p) {
    sentinel.prev = &sentinel;
    sentinel.next = &sentinel;
    sentinel.type = Matching::NodeType::Sentinel;
  }

  Level(Price p, Volume v) : price(p), volume(v) {
    sentinel.prev = &sentinel;
    sentinel.next = &sentinel;
    sentinel.type = Matching::NodeType::Sentinel;
  }

  bool empty() const { return sentinel.next == &sentinel; }

  void push_back(Matching::Order *o);
  void pop(Matching::Order *o);
  Matching::Order *front() const { return empty() ? nullptr : sentinel.next; };
};

using BestLevel = std::optional<std::pair<Price, Volume>>;

struct Orderbook {
  Matching::OrderPool orderpool;

  Orderbook() = default;

  // Adds to orderbook
  void addOrder(uint64_t orderId, Price price, uint64_t quantity, bool is_buy,
                uint64_t account_id);

  void removeOrder(uint64_t orderId);

  uint64_t matchOrder(Matching::Order *incoming, Price price);

  [[nodiscard]] std::pair<BestLevel, BestLevel> getBestPrices() const;

  [[nodiscard]] BestLevel bestBid() const;

  [[nodiscard]] BestLevel bestAsk() const;

  [[nodiscard]] inline Volume totalBidVolume() const noexcept {
    Volume v = 0;
    for (auto &lvl : mBidLevels)
      v += lvl.volume;
    return v;
  }

  [[nodiscard]] inline Volume totalAskVolume() const noexcept {
    Volume v = 0;
    for (auto &lvl : mAskLevels)
      v += lvl.volume;
    return v;
  }

private:
  //   bids sorted ASC (best at index -1), asks sorted DESC (best at index
  //   -1).
  std::vector<Level> mBidLevels;
  std::vector<Level> mAskLevels;

  // Adds to the specific orderbook side
  void addToLevel(Level &level, Matching::Order *order);

  inline bool crossed(Price incoming, Price resting, Side s) noexcept {
    return (s == Side::Bid) ? (incoming >= resting) : (incoming <= resting);
  }

  // finds the nearest or equal price level
  std::vector<Level>::iterator findBidPos(Price price);
  std::vector<Level>::iterator findAskPos(Price price);
};

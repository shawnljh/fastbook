#include "orderbook.h"
#include "order_pool.h"
#include "types.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>

// Level methods
void Level::push_back(Matching::Order *o) {
  o->level = this;
  o->next = &sentinel;
  o->prev = sentinel.prev;
  sentinel.prev->next = o;
  sentinel.prev = o;
  size++;
  volume += o->quantity_remaining;
};

void Level::pop(Matching::Order *o) {
  assert(o->type == Matching::NodeType::Order);
  o->prev->next = o->next;
  o->next->prev = o->prev;
  o->next = nullptr;
  o->prev = nullptr;
  size--;
  volume -= o->quantity_remaining;
  o->level = nullptr;
};

std::string Level::toString() const {
  std::ostringstream oss;
  oss << "Level(price=" << price << ", size=" << size << ", volume=" << volume
      << ")\n";

  const Matching::Order *curr = sentinel.next;
  while (curr != &sentinel) {
    oss << "  Order{id=" << curr->order_id
        << ", qty_rem=" << curr->quantity_remaining
        << ", side=" << (curr->side == Side::Bid ? "Bid" : "Ask")
        << ", acct=" << curr->account_id << "}\n";
    curr = curr->next;
  }
  return oss.str();
}

void Orderbook::addOrder(uint64_t orderId, Price price, uint64_t quantity,
                         bool is_buy, uint64_t account_id) {
  Matching::Order *order =
      orderpool_.allocate(orderId, quantity, is_buy, account_id);

  uint64_t quantity_remaining = matchLimitOrder(order, price);
  order->quantity_remaining = quantity_remaining;

  if (quantity_remaining == 0) {
    // Order fully filled
    orderpool_.deallocate(orderId);
    return;
  }

  Side side = order->side;
  auto &sideOfBook = (side == Side::Bid) ? mBidLevels : mAskLevels;

  auto possibleLevel =
      (side == Side::Bid) ? findBidPos(price) : findAskPos(price);

  if (possibleLevel != sideOfBook.end() &&
      possibleLevel->get()->price == price) {
    addToLevel(*possibleLevel->get(), order);
  } else {
    auto newLevel = std::make_unique<Level>(price);
    addToLevel(*newLevel, order);
    sideOfBook.insert(possibleLevel, std::move(newLevel));
  }
};

uint64_t Orderbook::matchLimitOrder(Matching::Order *incoming, Price price) {
  Side side = incoming->side;
  auto &opposingLevels = (side == Side::Bid) ? mAskLevels : mBidLevels;
  uint64_t quantity_remaining = incoming->quantity_remaining;
  bool recorded = false;
  // iterate from best opposite (back)
  while (quantity_remaining > 0 && !opposingLevels.empty()) {
    Level &bestOpp = *opposingLevels.back();

    bool crossed = Orderbook::crossed(price, bestOpp.price, side);

    if (!crossed)
      break;

    if (!recorded) {
      recorded = true;
      telemetry_.record_match();
    }

    Matching::Order *resting = bestOpp.sentinel.next;
    while (quantity_remaining > 0 && resting != &bestOpp.sentinel) {
      uint64_t traded =
          std::min(quantity_remaining, resting->quantity_remaining);
      quantity_remaining -= traded;
      resting->quantity_remaining -= traded;
      bestOpp.volume -= traded;

      Matching::Order *next = resting->next; // prefetch

      if (resting->quantity_remaining == 0) {
        bestOpp.pop(resting);
        orderpool_.deallocate(resting->order_id);
      }

      resting = next;
    }

    if (bestOpp.size == 0) {
      opposingLevels.pop_back();
    }
  }

  return quantity_remaining;
}

uint64_t Orderbook::matchMarketOrder(bool is_buy, uint64_t quantity) {
  auto &opposingLevels = (is_buy) ? mAskLevels : mBidLevels;
  uint64_t quantity_remaining = quantity;
  bool recorded = false;
  // iterate from best opposite (back)
  while (quantity_remaining > 0 && !opposingLevels.empty()) {
    Level &bestOpp = *opposingLevels.back();

    if (!recorded) {
      recorded = true;
      telemetry_.record_match();
    }

    Matching::Order *resting = bestOpp.sentinel.next;
    while (quantity_remaining > 0 && resting != &bestOpp.sentinel) {
      uint64_t traded =
          std::min(quantity_remaining, resting->quantity_remaining);
      quantity_remaining -= traded;
      resting->quantity_remaining -= traded;
      bestOpp.volume -= traded;

      Matching::Order *next = resting->next; // prefetch

      if (resting->quantity_remaining == 0) {
        bestOpp.pop(resting);
        orderpool_.deallocate(resting->order_id);
      }

      resting = next;
    }

    if (bestOpp.size == 0) {
      opposingLevels.pop_back();
    }
  }

  return quantity_remaining;
}

void Orderbook::removeOrder(uint64_t order_id) {
  auto *order = orderpool_.find(order_id);
  if (order == nullptr) {
    telemetry_.record_stale_cancel();
    return;
  }

  telemetry_.record_cancel();
  Level *level = order->level;
  level->pop(order);
  Side side = order->side;
  orderpool_.deallocate(order_id);

  if (level->size > 0)
    return;

  // remove empty level
  auto &sideOfBook = (side == Side::Bid) ? mBidLevels : mAskLevels;

  auto it = std::find_if(
      sideOfBook.begin(), sideOfBook.end(),
      [level](const std::unique_ptr<Level> &p) { return p.get() == level; });

  if (it != sideOfBook.end()) {
    sideOfBook.erase(it);
  }
}

std::pair<BestLevel, BestLevel> Orderbook::getBestPrices() const {
  BestLevel bestBid, bestAsk;
  if (!mBidLevels.empty()) {
    bestBid =
        std::make_pair(mBidLevels.back()->price, mBidLevels.back()->volume);
  }
  if (!mAskLevels.empty()) {
    bestAsk =
        std::make_pair(mAskLevels.back()->price, mAskLevels.back()->volume);
  }

  return {bestBid, bestAsk};
};

Orderbook::BidIt Orderbook::findBidPos(Price price) {
  // bids: ascending, best at back
  return std::lower_bound(mBidLevels.begin(), mBidLevels.end(), price,
                          [](const std::unique_ptr<Level> &L,
                             const Price price) { return L->price < price; });
};

Orderbook::AskIt Orderbook::findAskPos(Price price) {
  // Asks: descending, best at back
  return std::lower_bound(mAskLevels.begin(), mAskLevels.end(), price,
                          [](const std::unique_ptr<Level> &L,
                             const Price price) { return L->price > price; });
};

void Orderbook::addToLevel(Level &level, Matching::Order *order) {
  assert(order->level == nullptr && "Order already belongs to a level");
  level.push_back(order);
}

BestLevel Orderbook::bestBid() const {
  return mBidLevels.empty()
             ? std::nullopt
             : std::make_optional(std::make_pair(mBidLevels.back()->price,
                                                 mBidLevels.back()->volume));
}

BestLevel Orderbook::bestAsk() const {
  return mAskLevels.empty()
             ? std::nullopt
             : std::make_optional(std::make_pair(mAskLevels.back()->price,
                                                 mAskLevels.back()->volume));
}

std::string Orderbook::toString() const {
  std::ostringstream oss;

  oss << "=== ORDERBOOK ===\n";

  // --- Asks (print best last, since stored descending) ---
  oss << "[ASKS]\n";
  if (mAskLevels.empty()) {
    oss << "  <empty>\n";
  } else {
    for (auto it = mAskLevels.rbegin(); it != mAskLevels.rend(); ++it) {
      const Level &L = **it;
      oss << "  Price: " << L.price << " | Size: " << L.size
          << " | Vol: " << L.volume << '\n';
      const Matching::Order *curr = L.sentinel.next;
      while (curr != &L.sentinel) {
        oss << "    → id=" << curr->order_id
            << " qty=" << curr->quantity_remaining
            << " acct=" << curr->account_id
            << " side=" << (curr->side == Side::Bid ? "Bid" : "Ask") << '\n';
        curr = curr->next;
      }
    }
  }

  // --- Bids (stored ascending) ---
  oss << "[BIDS]\n";
  if (mBidLevels.empty()) {
    oss << "  <empty>\n";
  } else {
    for (auto it = mBidLevels.rbegin(); it != mBidLevels.rend(); ++it) {
      const Level &L = **it;
      oss << "  Price: " << L.price << " | Size: " << L.size
          << " | Vol: " << L.volume << '\n';
      const Matching::Order *curr = L.sentinel.next;
      while (curr != &L.sentinel) {
        oss << "    → id=" << curr->order_id
            << " qty=" << curr->quantity_remaining
            << " acct=" << curr->account_id
            << " side=" << (curr->side == Side::Bid ? "Bid" : "Ask") << '\n';
        curr = curr->next;
      }
    }
  }

  oss << "=================\n";
  return oss.str();
}

void Orderbook::dump_shape(const std::string &path, uint64_t bin_size) const {
  if (mBidLevels.empty() || mAskLevels.empty()) {
    return;
  }

  uint64_t best_bid = mBidLevels.back().get()->price;
  uint64_t best_ask = mAskLevels.back().get()->price;

  double mid = (best_bid + best_ask) / 2.0;

  std::unordered_map<int64_t, std::pair<uint64_t, uint64_t>> bins;

  for (auto &it : mBidLevels) {
    auto lvl = it.get();
    int64_t dist = static_cast<int64_t>((mid - lvl->price) / bin_size);
    bins[dist].first += lvl->volume;
  }

  for (auto &it : mAskLevels) {
    auto lvl = it.get();
    int64_t dist = static_cast<int64_t>((lvl->price - mid) / bin_size);
    bins[dist].second += lvl->volume;
  }

  std::vector<int64_t> keys;
  keys.reserve(bins.size());
  for (const auto &[k, _] : bins)
    keys.push_back(k);
  std::sort(keys.begin(), keys.end());

  std::ofstream out(path);
  out << "delta_ticks,bid_qty,ask_qty\n";
  for (auto k : keys) {
    auto [bq, aq] = bins[k];
    out << k * static_cast<int64_t>(bin_size) << "," << bq << "," << aq << "\n";
  }
}

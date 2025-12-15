#pragma once

#include "telemetry.h"
#include "types.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

struct Level; // forward declaration

namespace Matching {

enum class NodeType : uint8_t {
  Sentinel,
  Order,
};

struct alignas(64) Order {
  uint64_t quantity;           // 8 Bytes Order quantity
  uint64_t quantity_remaining; // 8 Bytes Order quantity remaining
  AccountId account_id;        // 8 Bytes Account identifier
  uint64_t order_id;           // 8 bytes Unique order ID

  Order *next = nullptr; // 8 bytes
  Order *prev = nullptr; // 8 bytes

  Level *level = nullptr; // 8 bytes

  Side side;                      // 1 byte Buy or sell side
  NodeType type{NodeType::Order}; // 1 byte
  OrderType order_type;

  char padding[5];
};

static_assert(alignof(Order) == 64, "Order struct alignment is not 64 bytes");
static_assert(sizeof(Order) == 64, "Order struct size is not 64 bytes");

class OrderPool {
  Telemetry &telemetry_;
  size_t slab_size_;
  size_t slab_offset_;
  uint64_t next_index_;
  std::vector<std::unique_ptr<Order[]>> slabs_;
  std::unordered_map<uint64_t, uint64_t> id_to_index_;
  std::vector<uint64_t> free_list_;

private:
  inline Order &get(uint64_t idx) noexcept {
    size_t slab_idx = idx / slab_size_;
    size_t offset = idx & (slab_size_ - 1); // fast mod
    return slabs_[slab_idx][offset];
  }

  inline void allocate_slab() noexcept {
    slabs_.push_back(std::make_unique<Order[]>(slab_size_));
    slab_offset_ = 0;
  }

public:
  explicit OrderPool(Telemetry &telemetry, size_t slab_size = 1 << 17) // 131072
      : telemetry_(telemetry), slab_size_(slab_size), next_index_(0) {
    assert((slab_size & (slab_size - 1)) == 0 &&
           "Slab size should be power of 2");
    allocate_slab();
  }

  // Allocate a new order (from freelist or bump)
  Order *allocate(uint64_t order_id, uint64_t quantity, bool is_buy,
                  uint64_t account_id) {
    uint64_t idx;
    auto existingOrder = this->find(order_id);
    if (existingOrder != nullptr) {
      return existingOrder;
    }

    if (!free_list_.empty()) {
      // reuse slot from free list
      telemetry_.record_alloc(false);
      idx = free_list_.back();
      free_list_.pop_back();
    } else {
      telemetry_.record_alloc(true);
      // Bump allocate from slab
      if (slab_offset_ == slab_size_) {
        allocate_slab();
      }
      idx = next_index_++;
      slab_offset_++;
    }

    Order &o = get(idx);
    o.quantity = quantity;
    o.quantity_remaining = quantity;
    o.side = is_buy ? Side::Bid : Side::Ask;
    o.account_id = account_id;
    o.order_id = order_id;

    id_to_index_[order_id] = idx;
    return &o;
  }

  // Lookup by external ID
  Order *find(uint64_t order_id) {
    auto it = id_to_index_.find(order_id);
    if (it == id_to_index_.end())
      return nullptr;
    return &get(it->second);
  }

  void deallocate(uint64_t order_id) {
    auto it = id_to_index_.find(order_id);
    if (it == id_to_index_.end())
      return;

    uint64_t idx = it->second;
    id_to_index_.erase(it);

    free_list_.push_back(idx);
  }
};
}; // namespace Matching

#pragma once

#include "telemetry.h"
#include "types.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
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
  OrderType order_type;           // 1 byte

  char padding[5];
};

static_assert(alignof(Order) == 64, "Order struct alignment is not 64 bytes");
static_assert(sizeof(Order) == 64, "Order struct size is not 64 bytes");

class OrderPool {
  static constexpr uint64_t EMPTY = UINT64_MAX;
  static constexpr uint64_t TOMBSTONE = UINT64_MAX - 1;

  struct alignas(8) MapEntry {
    uint64_t order_id = EMPTY;
    uint64_t pool_idx = EMPTY;
  };

  constexpr static uint8_t default_slab_size = 22;
  Telemetry &telemetry_;
  size_t slab_shift_;
  size_t slab_size_;
  size_t slab_offset_;
  uint64_t next_index_;

  std::vector<std::unique_ptr<Order[]>> slabs_;
  std::vector<uint64_t> free_list_;

  std::vector<MapEntry> lookup_table_;
  size_t lookup_mask_;
  // Consists of occupied + tombstoned indexes
  size_t lookup_capacity_;
  size_t lookup_size;

private:
  inline Order &get_from_slab(size_t pool_idx) noexcept {
    size_t slab_idx = pool_idx >> slab_shift_;
    // slab_size_ is pow of 2
    size_t offset = pool_idx & (slab_size_ - 1);
    return slabs_[slab_idx][offset];
  }

  inline void allocate_slab() noexcept {
    slabs_.push_back(std::make_unique<Order[]>(slab_size_));
    slab_offset_ = 0;
  }

  /**
   * Returns the ptr to the entry for the order for first tombstoned
   * entry.
   *
   * returns a nullptr if not able to find a tombstoned entry or order's entry
   */
  MapEntry *lookup(uint64_t order_id) noexcept {
    size_t hash_idx = std::hash<uint64_t>{}(order_id) & (lookup_size - 1);

    MapEntry *entry = &lookup_table_[hash_idx];
    // Keep track of "TOMBSTONED" index
    MapEntry *freed_entry = nullptr;
    uint64_t start_idx = hash_idx;

    while (entry->order_id != EMPTY) {
      if (entry->order_id == order_id) {
        // order already allocated
        return entry;
      }
      if (freed_entry == nullptr && entry->order_id == TOMBSTONE) {
        freed_entry = entry;
      }
      // linear probing (should be okay since id's are sequential)
      hash_idx = (hash_idx + 1) & (lookup_size - 1);
      entry = &lookup_table_[hash_idx];

      if (hash_idx == start_idx) {
        return freed_entry;
      }
    }
    return freed_entry != nullptr ? freed_entry : entry;
  }

public:
  explicit OrderPool(Telemetry &telemetry,
                     uint8_t slab_shift_ = default_slab_size) // 131072
      : telemetry_(telemetry), slab_shift_(slab_shift_), next_index_(0) {

    slab_size_ = 1 << slab_shift_;

    // lookup table to 2x slab size at 50% load
    lookup_size = 1 << (slab_shift_ + 2);
    assert((lookup_size & (lookup_size - 1)) == 0 &&
           "lookup table size should be a power for 2");

    lookup_table_.resize(lookup_size);
    free_list_.reserve(1 << slab_shift_);

    lookup_capacity_ = 0;
    allocate_slab();
  }

  // Allocate a new order (from freelist or bump)
  Order *allocate(uint64_t order_id, uint64_t quantity, bool is_buy,
                  uint64_t account_id) {
    // resize if load > 50% (slow ms);
    // this preserves the pointers in Order struct
    if ((lookup_capacity_ * 2) > lookup_table_.capacity()) {
      resize_map();
    }

    MapEntry *entry = lookup(order_id);

    if (entry == nullptr) [[unlikely]] {
      telemetry_.record_error();
      return nullptr;
    }

    if (entry->order_id == order_id) {
      return &get_from_slab(entry->pool_idx);
    }

    // Use order from freed slab if possible
    if (!free_list_.empty()) {
      // reuse slot from free list
      telemetry_.record_alloc(false);
      entry->pool_idx = free_list_.back();
      free_list_.pop_back();
    } else {
      telemetry_.record_alloc(true);
      // Bump allocate from slab
      if (slab_offset_ == slab_size_) {
        allocate_slab();
      }
      entry->pool_idx = next_index_++;
      slab_offset_++;
      lookup_capacity_++;
    }

    entry->order_id = order_id;

    Order &o = get_from_slab(entry->pool_idx);
    o.quantity = quantity;
    o.quantity_remaining = quantity;
    o.side = is_buy ? Side::Bid : Side::Ask;
    o.account_id = account_id;
    o.order_id = order_id;

    return &o;
  }

  // Lookup by order ID
  Order *find(uint64_t order_id) {
    MapEntry *entry = lookup(order_id);
    if (entry->order_id != order_id) {
      return nullptr;
    }
    return &get_from_slab(entry->pool_idx);
  }

  void deallocate(uint64_t order_id) {
    MapEntry *entry = lookup(order_id);
    if (entry->order_id != order_id) {
      return;
    }

    free_list_.push_back(entry->pool_idx);

    entry->order_id = TOMBSTONE;
    entry->pool_idx = EMPTY;
  }

  // Returns a dangling order, only use for cleanup
  Order *find_and_deallocate(uint64_t order_id) {
    MapEntry *entry = lookup(order_id);
    if (entry->order_id != order_id) {
      return nullptr;
    }

    Order *order = &get_from_slab(entry->pool_idx);
    free_list_.push_back(entry->pool_idx);

    entry->order_id = TOMBSTONE;
    entry->pool_idx = EMPTY;

    return order;
  }

  void resize_map();
};
}; // namespace Matching

#include "order_pool.h"
#include <cstddef>
#include <cstdint>

using namespace Matching;

void OrderPool::resize_map() {
  size_t old_size = lookup_size;
  lookup_size <<= 1;
  std::vector<MapEntry> new_lookup(lookup_size);

  // Need to recalculate the hash for everything in the table
  for (size_t i = 0; i < old_size; i++) {
    OrderPool::MapEntry *old_entry = &lookup_table_[i];
    if (old_entry->order_id == EMPTY || old_entry->order_id == TOMBSTONE) {
      continue;
    }

    size_t hash_idx =
        std::hash<uint64_t>{}(old_entry->order_id) & (lookup_size - 1);

    MapEntry *entry = &new_lookup[hash_idx];
    while (entry->order_id != EMPTY) {
      hash_idx = (hash_idx + 1) & (lookup_size - 1);
      entry = &new_lookup[hash_idx];
    }
    entry->order_id = old_entry->order_id;
    entry->pool_idx = old_entry->pool_idx;
  }

  lookup_table_ = std::move(new_lookup);
}

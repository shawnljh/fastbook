#pragma once
#include <cstdint>
#include <orderbook.h>
#include <strings.h>
#include <sys/types.h>

// GCC/Clang Intrinsics
#define ctz(x) __builtin_ctzll(x) // Count Trailing Zeros (Find LSBit)
#define clz(x) __builtin_clzll(x) // Count Leading Zeros (Find MSBit)

struct bitsetbook {
  /*
   * BIT LAYOUT
   * ==========================
   * We map a 64-bit integer (Block) to 64 price ticks.
   *
   * Binary Representation:
   * [ MSB (Bit 63) ........................................ LSB (Bit 0) ]
   *
   * Index Value:    HIGH (63)  <------------------------->  LOW (0)
   * Price Value:    HIGHEST    <------------------------->  LOWEST
   *
   *
   * SEARCH DIRECTIONS (TRAVERSAL)
   * =============================
   * Assume 'C' is the Current Bit (Limit Price just matched).
   *
   * State: [ ... 0 0 1 1 C 1 1 0 0 ... ]
   * ^
   *
   * 1. Find next best ASK:
   * - Goal: Find Lowest Price > Current.
   * - Direction: Move UP (Left towards MSB).
   * - Intrinsic: ctz (Count Trailing Zeros) -> Finds LSB of the upper mask.
   * - Logic: We want the "bottom" of the upper range.
   *
   * 2. Find next best BID (Selling to Buyers):
   * - Goal: Find Highest Price < Current.
   * - Direction: Move DOWN (Right towards LSB).
   * - Intrinsic: clz (Count Leading Zeros) -> Finds MSB of the lower mask.
   * - Logic: We want the "top" of the lower range.
   */

  static constexpr int TICK_COUNT = 16'384;
  // L1: Summary layer (4 * 64 = 256 bits) -> Maps to 256 L2 blocks
  uint64_t l1[4] = {0};
  // L2: Leaf layer (256 * 64 = 16384 bits) -> Maps to actual ticks
  uint64_t l2[256] = {0};

  void add_level(uint64_t index) {
    if (index < TICK_COUNT) [[likely]] {

      uint64_t l2_index = index >> 6;     // index / 64;
      uint64_t l2_offset = index & 63;    // index % 64;
      uint64_t l1_index = index >> 12;    // l2_index / 64;
      uint64_t l1_offset = l2_index & 63; // l2_index % 64;

      // set L2 bit to 1
      l2[l2_index] |= (1ull << l2_offset);
      // set L1 bit to 1
      l1[l1_index] |= (1ull << l1_offset);
    }
  }

  void remove_level(uint64_t index) {
    if (index < TICK_COUNT) [[likely]] {

      uint64_t l2_index = index >> 6;
      uint64_t l2_offset = index & 63;

      l2[l2_index] &= ~(1ull << l2_offset);

      // check if L2 block is empty
      if (l2[l2_index] == 0) {
        int l1_index = l2_index >> 6;
        int l1_offset = l2_index & 63;
        // Set L1 bit to empty
        l1[l1_index] &= ~(1ull << l1_offset);
      }
    }
  }

  // Returns Highest set index < index
  uint64_t get_next_lowest_setbit(uint64_t index) {
    if (index == 0 || index >= TICK_COUNT) [[unlikely]]
      return UINT64_MAX;

    uint64_t l2_index = index >> 6;
    uint64_t l2_offset = index & 63;

    // Check L2 block
    // We want everything from offset to 0 position to be 0
    uint64_t mask = ((1uLL << (l2_offset)) - 1);
    uint64_t val = l2[l2_index] & mask;

    if (val != 0) {
      return (l2_index << 6) + (63 - clz(val));
    }

    uint64_t l1_index = l2_index >> 6;
    uint64_t l1_offset = l2_index & 63;

    // Get the next lowest bit in the current L1 block (excluding the current
    // bit)
    mask = (1uLL << (l1_offset)) - 1;
    val = l1[l1_index] & mask;

    if (val != 0) {
      uint64_t next_l2 = (l1_index << 6) + (63 - clz(val));
      return (next_l2 << 6) + (63 - clz(l2[next_l2]));
    }

    // 3. Scan remaining L1 integers downwards
    for (int i = l1_index - 1; i >= 0; i--) {
      if (l1[i] != 0) {
        uint64_t next_l2 = (i << 6) + (63 - clz(l1[i]));
        return (next_l2 << 6) + (63 - clz(l2[next_l2]));
      }
    }

    return UINT64_MAX;
  }

  // Returns lowest set index > index
  uint64_t get_next_highest_setbit(uint64_t index) {
    if (index >= (TICK_COUNT - 1)) [[unlikely]]
      return UINT64_MAX;

    uint64_t l2_index = index >> 6;
    uint64_t l2_offset = index & 63;

    // Check L2 block
    // We want bits < offset bit
    uint64_t mask = (l2_offset == 63) ? 0uLL : ~((1uLL << (l2_offset + 1)) - 1);
    uint64_t val = l2[l2_index] & mask;

    if (val != 0) {
      return (l2_index << 6) + ctz(val);
    }

    uint64_t l1_index = l2_index >> 6;
    uint64_t l1_offset = l2_index & 63;

    // Get the next highest bit in the current L1 block (excluding the current
    // bit)
    mask = (l1_offset == 63) ? 0uLL : ~((1uLL << (l1_offset + 1)) - 1);
    val = l1[l1_index] & mask;

    if (val != 0) {
      uint64_t next_l2 = (l1_index << 6) + (ctz(val));
      return (next_l2 << 6) + (ctz(l2[next_l2]));
    }

    // Scan remaining L1 integers upwards
    for (int i = l1_index + 1; i < 4; i++) {
      if (l1[i] != 0) {
        uint64_t next_l2 = (i << 6) + (ctz(l1[i]));
        return (next_l2 << 6) + ctz(l2[next_l2]);
      }
    }

    return UINT64_MAX;
  }
};

#pragma once
#include <array>
#include <atomic>
#include <optional>

using namespace std;

template <typename T, size_t Size> class SPSCQueue {
  static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");

  array<T, Size> buffer;
  alignas(64) atomic<size_t> head{0};
  alignas(64) atomic<size_t> tail{0};

public:
  bool enqueue(const T &item) {
    size_t current_head = head.load(memory_order_relaxed);
    size_t next_head = (current_head + 1) & (Size - 1);

    if (next_head == tail.load(memory_order_acquire)) {
      // Queue full
      return false;
    }

    buffer[current_head] = item;
    head.store(next_head, memory_order_release);
    return true;
  }

  optional<T> dequeue() {
    size_t current_tail = tail.load(memory_order_relaxed);

    if (current_tail == head.load(memory_order_acquire)) {
      // Queue empty
      return nullopt;
    }

    T item = buffer[current_tail];
    tail.store((current_tail + 1) & (Size - 1), memory_order_release);
    return item;
  }
};

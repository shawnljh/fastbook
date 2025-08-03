#pragma once
#include <array>
#include <optional>
#include <order.h>
#include <order_pool.h>

#define CACHE_LINE_SIZE 64

struct alignas(CACHE_LINE_SIZE) RingBuffer {
    static constexpr size_t CAPACITY = 1024;
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "CAPACITY must be a power of 2");

    static constexpr size_t MAX_INCREMENT = 10;

    std::array<uint64_t, CAPACITY> data;
    size_t head;
    size_t tail;
    size_t size;
    size_t capacity;

    RingBuffer();

    inline bool enqueue(uint64_t order_index) {
        if (size == capacity) {
            return false;
        }
        data[head] = order_index;
        head = (head + 1) & (CAPACITY - 1);
        size++;
        return true;
    };

    inline std::optional<uint64_t> dequeue(Matching::OrderPool& order_pool) {
    // Do unbounded loop for now
        while (size > 0 && !order_pool.is_active(data[tail])) {
            // tail points to a dead order
            tail = (tail + 1) & (CAPACITY - 1);
            size--;
        }

        if (size == 0) return std::nullopt;

        uint64_t index = data[tail];

        tail = (tail + 1) & (CAPACITY - 1);
        size--;

        return index;
    }

    inline size_t get_size() const {
        return size;
    };

    inline bool is_empty() const {
        return size == 0;
    }

    inline bool is_full() const {
        return size >= CAPACITY;
    }
};


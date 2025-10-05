#pragma once

#include <order.h>
#include <order_pool.h>

struct alignas(24) IntrusiveListNode {
    uint64_t orderId;
    IntrusiveListNode* next;
    IntrusiveListNode* prev;

    IntrusiveListNode(uint64_t n): orderId(n), next(nullptr), prev(nullptr) {}
};


struct alignas(64) IntrusiveList {
    uint32_t size;
    IntrusiveListNode head;
    IntrusiveListNode tail;

    IntrusiveList(): size(0), head(0), tail(0) {
        head.next = &tail;
        tail.prev = &head;
    }

    public:
        void enqueue(IntrusiveListNode* node);
        void remove(IntrusiveListNode* node);
        IntrusiveListNode* dequeue();

    
    inline uint32_t get_size() const {
        return size;
    };

    inline bool is_empty() const {
        return size == 0;
    }
};
#include "intrusive_list.h"

void IntrusiveList::enqueue(IntrusiveListNode* node) {
    node->prev = tail.prev;
    node->next = &tail;
    tail.prev->next = node;
    tail.prev = node;
    ++size;
}

void IntrusiveList::remove(IntrusiveListNode* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev; 
    node->next = nullptr;
    node->prev = nullptr;
    --size;
}

IntrusiveListNode* IntrusiveList::dequeue() {
    if (size == 0) return nullptr;
    IntrusiveListNode* node = head.next;
    remove(node);
    return node;
}
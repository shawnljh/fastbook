#include "ring_buffer.h"
#include "optional"
#include "order_pool.h"


RingBuffer::RingBuffer(): data{}, head(0), tail(0), size(0), capacity(CAPACITY) {
}

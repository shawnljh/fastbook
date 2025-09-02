#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <cstddef>
#include <cassert>
#include <types.h>
#include <intrusive_list.h>


namespace Matching{

struct alignas(64) Order {
    uint64_t quantity;    // Order quantity
    uint64_t quantity_remaining; // Order quantity remaining
    Side side;              // Buy or sell side
    bool is_active;       // True if order is live, false if cancelled
    AccountId account_id;  // Account identifier
    uint64_t order_id;    // Unique order ID
    IntrusiveListNode node; // Node lives in order; 
    char padding[6];
};

static_assert(alignof(Order) == 64, "Order struct alignment is not 64 bytes");
static_assert(sizeof(Order) == 64, "Order struct size is not 64 bytes");


class OrderPool {
public:
    static constexpr size_t MAX_ORDERS = 1'000'000;
    
    OrderPool();

    inline std::optional<uint64_t> add_order(uint64_t order_id, uint64_t quantity, bool is_buy, uint64_t account_id) {
        if (next_index == MAX_ORDERS) {
            return std::nullopt;
        }
        orders[next_index] =  {order_id, quantity, account_id, is_buy, true};
        uint64_t order_index = next_index++;
        return order_index;
    }

    inline void mark_dead(uint64_t order_index) {
        assert(order_index < next_index); // reduce branching in hot path
        orders[order_index].is_active = false;
    }

    inline bool is_active(uint64_t order_index) const {
        assert(order_index < next_index); // reduce branching in hot path
        return orders[order_index].is_active;
    }

private:
    alignas(64) std::array<Order, MAX_ORDERS> orders;
    uint64_t next_index;
};
};
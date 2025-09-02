#pragma once
#include <vector>
#include <types.h>
#include <ring_buffer.h>
#include <intrusive_list.h>

struct Level {
    Price price {};
    Volume volume {};
    IntrusiveList orders; 
    uint32_t count_live{0};
    uint32_t count_dead{0};

    Level() = default; // default ctor
    Level(Price p, Volume v); // custom ctor
}

struct Orderbook {
    Orderbook() = default;

    // Adds to orderbook
    void addOrder(Side side, Price price, Volume volume, OrderId orderId);

    std::pair<std::optional<std::pair<Price, Volume>>, std::optional<std::pair<Price, Volume>>> getBestPrices() const;

private:
    //   bids sorted ASC (best at index -1), asks sorted DESC (best at index -1).
    std::vector<Level> mBidLevels;
    std::vector<Level> mAskLevels;

    // Adds to the specific orderbook side
    void addToLevel(Level& level, Volume volume, OrderId orderId);
   
    // finds the nearest or equal price level
    std::vector<Level>::iterator findBidPos(Price price);
    std::vector<Level>::iterator findAskPos(Price price);

};


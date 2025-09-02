#include "orderbook.h"


Level::Level(Price p, Volume v):
    price(p),
    volume(v),
    orders(),
    count_live(0),
    count_dead(0) {}

using BestLevel = std::optional<std::pair<Price, Volume>>;

void addOrder(Side side, Price price, Volume volume, OrderId orderId) {

};

std::pair<std::optional<std::pair<Price, Volume>>, std::optional<std::pair<Price, Volume>>> getBestPrices() const {
    BestLevel bestBid, BestAsk;

    if (!mBid)
};

std::vector<Level>::iterator Orderbook::findBidPos(Price price) {
    // bids: ASC - find first element with level.price >= price
    return std::find_if(mBidLevels.begin(), mBidLevels.end(),
                         [price](const Level& L){return L.price >= price;})
}

std::vector<Level>::iterator Orderbook::findAskPos(Price price) {
    // Asks: DESC - find first element with level.price <= price
    return std::find_if(mAskLevels.begin(), mAskLevels.end(),
                        [price](const Level &L){return L.price <= price;})
}


void addToLevel(Level& level, Volume volume, OrderId orderId) {

}
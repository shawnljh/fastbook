#include "order.h"
#include "server.h"
#include "spsc_queue.h"
#include "types.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <orderbook.h>
#include <thread>

using namespace std;

SPSCQueue<Client::Order, 1024> order_queue;
Orderbook book;
uint64_t order_id = 1;

void matching_loop() {
  uint64_t processed = 0;
  chrono::steady_clock::time_point start;
  bool started = false;

  while (true) {
    auto maybe_order = order_queue.dequeue();
    if (!maybe_order) {
      std::this_thread::sleep_for(chrono::microseconds(500));
      continue;
    }

    ScopedTimer t(book.telemetry_);
    book.telemetry_.record_order();

    if (!started) {
      started = true;
      start = chrono::steady_clock::now();
    }

    const auto &order = *maybe_order;
    bool is_buy = (order.side == Side::Bid);

    if (order.order_type == OrderType::Limit) {
      book.addOrder(order_id++, order.price, order.quantity, is_buy,
                    order.account_id);
    } else if (order.order_type == OrderType::Market) {

      book.matchMarketOrder(is_buy, order.quantity);
    } else {
      book.removeOrder(order.order_id);
    }

    processed++;
    if (processed % 1'000'000 == 0) {
      auto now = chrono::steady_clock::now();
      double elapsed = chrono::duration<double>(now - start).count();
      std::cout << processed << " processed in " << elapsed << "s ("
                << processed / elapsed << " orders/sec)" << "\n";
      book.telemetry_.dump(elapsed);
      std::printf("active_levels=%zu resting_orders=%zu\n",
                  book.active_levels(), book.resting_orders());
    }
  }
}

int main() {
  thread matcher(matching_loop);
  start_tcp_server();
  matcher.join();
  return 0;
}

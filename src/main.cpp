#include "order.h"
#include "server.h"
#include "spsc_queue.h"
#include "types.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <orderbook.h>
#include <thread>

using namespace std;

SPSCQueue<Client::Order, 262144> order_queue;
Orderbook book;
uint64_t order_id = 1;

std::atomic<bool> *p_stop_flag = nullptr;

void handle_signal(int sig) {
  if (p_stop_flag) {
    std::cerr << "\n [Signal Caught] " << sig << ", shutting down.\n";
    p_stop_flag->store(true);
  }
}

void matching_loop(std::atomic<bool> &stop_flag) {
  uint64_t processed = 0;
  chrono::steady_clock::time_point start;
  bool started = false;

  while (!stop_flag.load()) {
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

  book.dump_shape("final_shape.csv", 10);
  cout << "processed: " << processed << '\n';
}

int main() {
  std::atomic<bool> stop_flag{false};
  p_stop_flag = &stop_flag;

  std::signal(SIGINT, handle_signal);
  thread matcher(matching_loop, ref(stop_flag));
  start_tcp_server(stop_flag);
  matcher.join();

  std::cerr << "[Main] Graceful termination.\n";
  return 0;
}

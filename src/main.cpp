#include "order.h"
#include "server.h"
#include "SPSCQueue.h"
#include <thread>
#include <iostream>

using namespace std;

SPSCQueue<Order, 1024> order_queue;

void matching_loop() {
    while (true) {
        auto maybe_order = order_queue.dequeue();
        if (maybe_order) {
            Order order = maybe_order.value();
            cout << "Order dequeue" << "\n";
            cout << order.price << "\n";
        }
    }
}


int main() {
    thread matcher(matching_loop);
    start_tcp_server();
    matcher.join();
    return 0;
}


#include "ingress_telemetry.h"
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <order.h>
#include <poll.h>
#include <server.h>
#include <spsc_queue.h>
#include <thread>
#include <types.h>
#include <unistd.h>

extern SPSCQueue<Client::Order, 262144> order_queue;

constexpr int PORT = 8080;

ssize_t read_exact(int fd, void *buffer, size_t bytes,
                   std::atomic<bool> &stop_flag) {
  size_t total_read = 0;
  uint8_t *buf = static_cast<uint8_t *>(buffer);

  while (total_read < bytes) {
    if (stop_flag.load())
      return -3;

    ssize_t n = read(fd, buf + total_read, bytes - total_read);

    if (n > 0) {
      total_read += n;
      continue;
    }
    if (n == 0) {
      // client closed connection
      return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // no data available yet
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      continue;
    }
    return -1; // real error
  }
  return total_read;
}

void start_tcp_server(std::atomic<bool> &stop_flag) {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  Ingress_Telemetry ingress_tel;
  bool started = false;
  chrono::steady_clock::time_point t0{};

  int enqueued = 0;
  int not_queued = 0;

  socklen_t addrlen = sizeof(address);

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  // Forcefully attaching socket to the port 8080
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  // Bind the socket to the network address and the port
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // Start listening for incoming connections
  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  cout << "Server listening on port " << PORT << endl;
  // Accept incoming connection

  int flags = fcntl(server_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl(F_GETFL)");
    exit(EXIT_FAILURE);
  }

  if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl(F_SETFL)");
    exit(EXIT_FAILURE);
  }

  while (!stop_flag.load()) {
    new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    if (new_socket >= 0) {
      break;
    }

    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    perror("accept");
    exit(EXIT_FAILURE);
  }

  if (new_socket < 0) {
    std::cerr << "[Server] Shutting down before accepting connection.\n";
    close(server_fd);
    return;
  }

  // Set client socket to non-blocking
  cout << "Client socket connected\n";
  flags = fcntl(new_socket, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl(F_GETFL) new_socket");
    close(new_socket);
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  if (fcntl(new_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl(F_SETFL) new_socket");
    close(new_socket);
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  while (!stop_flag.load()) {
    auto start = std::chrono::high_resolution_clock::now();
    if (!started) {
      t0 = chrono::steady_clock::now();
    }

    uint32_t length_prefix_net;
    ssize_t n = read_exact(new_socket, &length_prefix_net,
                           sizeof(length_prefix_net), stop_flag);

    if (n == 0) {
      std::cout << "Client disconnected\n";
      break;
    }
    if (n == -1) {
      perror("read error");
      break;
    }
    if (n == -3) {
      // Sigint
      break;
    }

    uint32_t payload_size = ntohl(length_prefix_net);
    std::vector<unsigned char> payload(payload_size);

    if (read_exact(new_socket, payload.data(), payload_size, stop_flag) !=
        payload_size) {
      cout << "Failed to read payload\n";
      break;
    }

    try {
      // cout << "Payload size: " << payload_size << "\n";

      Client::Order order = Client::parse_order(payload);

      // cout << "Order Parsed:\n";
      // cout << "  Side: " << (order.side == Side::Bid ? "Bid" : "Ask") <<
      // "\n"; cout << "  Price: " << order.price << "\n"; cout << "  Quantity:
      // " << order.quantity << "\n";
      //
      if (order_queue.enqueue(order)) {
        enqueued++;
        if (!started) {
          started = true;
        }
        auto end = std::chrono::high_resolution_clock::now();
        uint64_t ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        ingress_tel.record_latency(ns);
      } else {
        not_queued++;
      }

    } catch (const std::exception &e) {
      cerr << "Parse error: " << e.what() << "\n";
    }
  }

  double elapsed_s = 0.0;
  if (started)
    elapsed_s =
        std::chrono::duration<double>(chrono::steady_clock::now() - t0).count();
  ingress_tel.dump(elapsed_s);
  cout << "Enqueued: " << enqueued << '\n';
  cout << "Not queued: " << not_queued << '\n';

  // close the socket
  close(new_socket);
  close(server_fd);
}

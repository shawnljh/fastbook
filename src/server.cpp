
#include <iostream>
#include <netinet/in.h>
#include <order.h>
#include <server.h>
#include <spsc_queue.h>
#include <types.h>
#include <unistd.h>

extern SPSCQueue<Client::Order, 1024> order_queue;

constexpr int PORT = 8080;

ssize_t read_exact(int fd, void *buffer, size_t bytes) {
  size_t total_read = 0;
  uint8_t *buf = static_cast<uint8_t *>(buffer);

  while (total_read < bytes) {
    ssize_t n = read(fd, buf + total_read, bytes - total_read);
    if (n <= 0)
      return n; // error or EOF
    total_read += n;
  }

  return total_read;
}

void start_tcp_server() {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
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
  if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) <
      0) {
    perror("accept");
    exit(EXIT_FAILURE);
  }

  while (true) {
    uint32_t length_prefix_net;
    if (read_exact(new_socket, &length_prefix_net, sizeof(length_prefix_net)) !=
        sizeof(length_prefix_net)) {
      cout << "Client disconnected\n";
      break;
    }

    uint32_t payload_size = ntohl(length_prefix_net);
    std::vector<unsigned char> payload(payload_size);

    if (read_exact(new_socket, payload.data(), payload_size) != payload_size) {
      cout << "Failed to read payload\n";
      break;
    }

    try {
      cout << "Payload size: " << payload_size << "\n";
      Client::Order order = Client::parse_order(payload);

      cout << "Order Parsed:\n";
      cout << "  Side: " << (order.side == Side::Bid ? "Bid" : "Ask") << "\n";
      cout << "  Price: " << order.price << "\n";
      cout << "  Quantity: " << order.quantity << "\n";

      if (order_queue.enqueue(order)) {
        cout << "Order enqueued" << "\n";
      }

    } catch (const std::exception &e) {
      cerr << "Parse error: " << e.what() << "\n";
    }
  }

  // close the socket
  close(new_socket);
  close(server_fd);
}

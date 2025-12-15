
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  int fd = open("client/orders.bin", O_RDONLY);
  struct stat st{};
  fstat(fd, &st);
  size_t size = st.st_size;

  char *data = (char *)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);

  int sock = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  connect(sock, (sockaddr *)&addr, sizeof(addr));

  const size_t CHUNK = 4096; // tuned later
  size_t offset = 0;

  auto t0 = std::chrono::steady_clock::now();

  while (offset < size) {
    size_t chunk = std::min(CHUNK, size - offset);
    ssize_t n = send(sock, data + offset, chunk, 0);
    offset += n;
  }

  auto t1 = std::chrono::steady_clock::now();

  double seconds = std::chrono::duration<double>(t1 - t0).count();
  size_t N = size / 25;

  std::cout << "Replayed " << N << " orders in " << seconds << "s → "
            << (N / seconds) << " orders/sec\n";

  close(sock);
  munmap(data, size);
}

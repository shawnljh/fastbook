
#pragma once

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace std;
class SocketBuffer {
private:
  vector<uint8_t> buf_;
  size_t head_; // read cursor
  size_t tail_; // write cursor

public:
  explicit SocketBuffer(size_t capacity = 65536)
      : buf_(capacity), head_(0), tail_(0) {}

  ssize_t read_exact(int fd, void *dest, size_t bytes_needed,
                     std::atomic<bool> &stop_flag) {
    uint8_t *out = static_cast<uint8_t *>(dest);
    size_t bytes_fulfilled = 0;

    while (bytes_fulfilled < bytes_needed) {
      if (stop_flag.load(memory_order::relaxed))
        return -3;

      size_t available = tail_ - head_;
      size_t to_copy = bytes_needed - bytes_fulfilled;

      [[likely]]
      if (available >= to_copy) {
        // Fastpath: read directly from buffer
        memcpy(out + bytes_fulfilled, buf_.data() + head_, to_copy);
        head_ += to_copy;
        return bytes_needed;
      }

      // Need to syscall to read more data
      if (available > 0) {
        memcpy(out + bytes_fulfilled, buf_.data() + head_, available);
        bytes_fulfilled += available;
      }

      // We know the buffer is empty because we just drained it. Can reuse
      // buffer
      head_ = 0;
      tail_ = 0;

      // Syscall to fill the bufer
      ssize_t n = read(fd, buf_.data(), buf_.size());

      if (n > 0) {
        tail_ = n;
        continue;
      }
      if (n == 0) {
        // connection closed
        return 0;
      }

      // If we were to block, we spin instead
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        _mm_pause(); // spin wait
        continue;
      }

      // Real socket error
      return -1;
    }

    return bytes_fulfilled;
  }
};

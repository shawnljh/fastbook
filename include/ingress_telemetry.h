#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

struct Ingress_Telemetry {
  std::atomic<uint64_t> total_msgs{0};
  std::atomic<uint64_t> total_latency_ns{0};

  static constexpr uint64_t BIN_WIDTH_NS = 100;
  static constexpr uint64_t MAX_TRACK_NS = 10'000'000;
  static constexpr uint64_t NUM_BINS = MAX_TRACK_NS / BIN_WIDTH_NS + 1;
  std::array<std::atomic<uint64_t>, NUM_BINS> hist{};

  void record_latency(uint64_t ns) noexcept {
    size_t idx = std::min<size_t>(ns / BIN_WIDTH_NS, NUM_BINS - 1);
    hist[idx].fetch_add(1, std::memory_order_relaxed);
    total_latency_ns.fetch_add(ns, std ::memory_order_relaxed);
    total_msgs.fetch_add(1, std::memory_order_relaxed);
  }

  double avg_latency_ns() const noexcept {
    auto total = total_msgs.load();
    return total ? double(total_latency_ns.load()) / total : 0.0;
  }

  void dump(double elapsed_s) const noexcept {
    std::printf("[Ingress Telemetry]\n");
    std::printf("messages=%lu avg_latency=%.2f ns throughput=%.2f msg/s\n",
                total_msgs.load(), avg_latency_ns(),
                total_msgs.load() / elapsed_s);
  }
};

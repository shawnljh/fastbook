#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>

struct Telemetry {
  // Counters
  std::atomic<uint64_t> total_orders{0};
  std::atomic<uint64_t> matched_orders{0};
  std::atomic<uint64_t> cancelled_orders{0};
  std::atomic<uint64_t> stale_cancels{0};
  std::atomic<uint64_t> total_latency_ns{0};

  std::atomic<uint64_t> total_allocs{0};
  std::atomic<uint64_t> reused_allocs{0};

  static constexpr uint64_t BIN_WIDTH_NS = 25;        // each bin = 100 ns
  static constexpr uint64_t MAX_TRACK_NS = 5'000'000; // 5 ms cap
  static constexpr size_t NUM_BINS = MAX_TRACK_NS / BIN_WIDTH_NS + 1;
  std::array<std::atomic<uint64_t>, NUM_BINS> hist{};

  void record_order() noexcept {
    total_orders.fetch_add(1, std::memory_order_relaxed);
  }

  void record_match() noexcept {
    matched_orders.fetch_add(1, std::memory_order_relaxed);
  }

  void record_cancel() noexcept {
    cancelled_orders.fetch_add(1, std::memory_order_relaxed);
  }

  void record_stale_cancel() noexcept {
    stale_cancels.fetch_add(1, std::memory_order_relaxed);
  }

  void record_alloc(bool reused) {
    total_allocs.fetch_add(1, std::memory_order_relaxed);
    if (reused)
      reused_allocs.fetch_add(1, std::memory_order_relaxed);
  }

  void record_latency(uint64_t ns) noexcept {
    size_t idx = std::min<size_t>(ns / BIN_WIDTH_NS, NUM_BINS - 1);
    hist[idx].fetch_add(1, std::memory_order_relaxed);

    total_latency_ns.fetch_add(ns, std::memory_order_relaxed);
  }

  double avg_latency_ns() const noexcept {
    auto total = total_orders.load(std::memory_order_relaxed);
    return total ? double(total_latency_ns.load(std::memory_order_relaxed)) /
                       total
                 : 0.0;
  }

  double reuse_ratio() const noexcept {
    auto total = total_allocs.load(std::memory_order_relaxed);
    return total ? 100.0 * reused_allocs.load(std::memory_order_relaxed) / total
                 : 0.0;
  }

  void dump_percentiles() const noexcept {
    uint64_t total = 0;
    for (auto &h : hist)
      total += h.load(std::memory_order_relaxed);
    if (total == 0)
      return;

    auto find_percentile = [&](double target) {
      uint64_t cumulative = 0;
      for (size_t i = 0; i < NUM_BINS; ++i) {
        cumulative += hist[i].load(std::memory_order_relaxed);
        if (double(cumulative) / total >= target)
          return i * BIN_WIDTH_NS;
      }
      return (NUM_BINS - 1) * BIN_WIDTH_NS;
    };

    auto p50 = find_percentile(0.50);
    auto p90 = find_percentile(0.90);
    auto p99 = find_percentile(0.99);
    auto p999 = find_percentile(0.999);

    std::printf("p50=%lu ns  p90=%lu ns  p99=%lu ns  p999=%lu ns\n", p50, p90,
                p99, p999);
  }

  void dump(double elapsed_s) const noexcept {
    double throughput = total_orders.load() / elapsed_s;
    std::printf("[FastBook Telemetry]\n");
    std::printf("orders=%lu matched=%lu cancelled=%lu stale cancels=%lu\n",
                total_orders.load(), matched_orders.load(),
                cancelled_orders.load(), stale_cancels.load());
    std::printf("avg_latency=%.2f ns, total_latency= %lu ns\n",
                avg_latency_ns(), total_latency_ns.load());
    std::printf("throughput=%.2f ops/s\n", throughput);
    std::printf("allocations=%lu reused=%.2f%%\n", total_allocs.load(),
                reuse_ratio());
    dump_percentiles();
  }
};

// Per-order latency measurement
struct ScopedTimer {
  Telemetry &tel;
  std::chrono::high_resolution_clock::time_point start;
  explicit ScopedTimer(Telemetry &t) noexcept
      : tel(t), start(std::chrono::high_resolution_clock::now()) {}

  ~ScopedTimer() noexcept {
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                  .count();
    tel.record_latency(ns);
  }
};

#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Telemetry {
  // Counters
  std::atomic<uint64_t> total_orders{0};
  std::atomic<uint64_t> matched_orders{0};
  std::atomic<uint64_t> cancelled_orders{0};
  std::atomic<uint64_t> total_latency_ns{0};
  std::atomic<uint64_t> max_latency_ns{0};

  std::atomic<uint64_t> total_allocs{0};
  std::atomic<uint64_t> reused_allocs{0};
  std::vector<uint64_t> latencies;

  void record_order() noexcept {
    total_orders.fetch_add(1, std::memory_order_relaxed);
  }

  void record_match() noexcept {
    matched_orders.fetch_add(1, std::memory_order_relaxed);
  }

  void record_cancel() noexcept {
    cancelled_orders.fetch_add(1, std::memory_order_relaxed);
  }

  void record_alloc(bool reused) {
    total_allocs.fetch_add(1, std::memory_order_relaxed);
    if (reused)
      reused_allocs.fetch_add(1, std::memory_order_relaxed);
  }

  void record_latency(uint64_t ns) noexcept {
    latencies.push_back(ns);
    total_latency_ns.fetch_add(ns, std::memory_order_relaxed);
    uint64_t prev = max_latency_ns.load(std::memory_order_relaxed);
    while (prev < ns && !max_latency_ns.compare_exchange_weak(
                            prev, ns, std::memory_order_relaxed)) {
    }
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
    if (latencies.empty())
      return;
    std::sort(latencies.begin(), latencies.end());
    auto get = [&](double p) {
      size_t idx = std::min<size_t>(latencies.size() - 1, p * latencies.size());
      return latencies[idx];
    };

    printf("p50=%lu ns  p90=%lu ns  p99=%lu ns  p999=%lu ns\n", get(0.50),
           get(0.90), get(0.99), get(0.999));
  }

  void dump(double elapsed_s) const noexcept {
    double throughput = total_orders.load() / elapsed_s;
    std::printf("[FastBook Telemetry]\n");
    std::printf("orders=%lu matched=%lu cancelled=%lu\n", total_orders.load(),
                matched_orders.load(), cancelled_orders.load());
    std::printf("avg_latency=%.2f ns  max_latency=%lu ns\n", avg_latency_ns(),
                max_latency_ns.load());
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

#pragma once

#include <chrono>
#include <cstdint>
#include <emmintrin.h>
#include <iostream>
#include <stdint.h>
#include <thread>
#include <x86intrin.h>

using namespace std;

class TSCClock {
private:
  double nanoseconds_per_cycle_;

public:
  TSCClock() {
    cout << "TSC: Calibrating hardware clock... (1 second)\n";

    auto os_start = chrono::high_resolution_clock::now();
    // pipeline fence
    _mm_lfence();

    uint64_t tsc_start = __rdtsc();
    this_thread::sleep_for(std::chrono::seconds(1));
    auto os_end = chrono::high_resolution_clock::now();

    unsigned int aux;
    uint64_t tsc_end = __rdtscp(&aux);

    uint64_t os_nanoseconds =
        chrono::duration_cast<chrono::nanoseconds>(os_end - os_start).count();
    uint64_t elapsed_cycles = tsc_end - tsc_start;

    nanoseconds_per_cycle_ =
        static_cast<double>(os_nanoseconds) / elapsed_cycles;

    cout << "TSC: Calibration completed: 1 cycle = " << nanoseconds_per_cycle_
         << " nanoseconds\n";
  }

  inline __attribute__((always_inline)) uint64_t start() const {
    _mm_lfence();
    return __rdtsc();
  }

  inline __attribute__((always_inline)) uint64_t stop() const {
    unsigned int aux;
    return __rdtscp(&aux);
  }
  inline uint64_t cycles_to_nanoseconds(uint64_t cycles) const {
    return static_cast<uint64_t>(cycles * nanoseconds_per_cycle_);
  }
};

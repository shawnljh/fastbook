#pragma once

#include "TSCClock.h"
#include <atomic>
void start_tcp_server(std::atomic<bool> &stop_flag, TSCClock hardware_clock);

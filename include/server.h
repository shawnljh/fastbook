#pragma once

#include <atomic>
void start_tcp_server(std::atomic<bool> &stop_flag);

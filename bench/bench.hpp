#pragma once

#include <stdint.h>

// The only debugger write is this scalar case selector, before setup runs.
extern "C" volatile uint8_t bench_case;
extern "C" void bench_select();
void bench_setup();
uint8_t bench_count();

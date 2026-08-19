#ifndef TCMIPS_C_TCM_UTIL_H
#define TCMIPS_C_TCM_UTIL_H

#include <dev/syscall.h>

#include <cstdint>

static inline void tcm_delay_ms(uint32_t ms) {
  uint32_t start_s = tcm_syscall_get_timestamp();
  uint32_t start_m = tcm_syscall_get_timestamp_milli();
  while (true) {
    uint32_t s = tcm_syscall_get_timestamp();
    uint32_t m = tcm_syscall_get_timestamp_milli();
    uint32_t elapsed =
        (s - start_s) * 1000u + (m >= start_m ? m - start_m : m + 1000u - start_m);
    if (elapsed >= ms)
      return;
  }
}

static inline uint32_t tcm_rng_next(uint32_t &state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

static inline uint32_t tcm_rand_seed() {
  return tcm_syscall_get_timestamp() ^ (tcm_syscall_get_timestamp_micro() << 7) ^
         (tcm_syscall_get_timestamp_nano() << 13);
}

#endif
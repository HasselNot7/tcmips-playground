#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>

#include <cstdint>
#include <cstdio>

#include "tcm_util.h"

int main() {
  uint32_t rng_state = tcm_rand_seed();

  tcm_ascii_console_init();
  tcm_ascii_console_mode(false, true);
  tcm_ascii_console_clear();
  tcm_seven_segment_display_clear();

  printf("===== Reaction Time Test =====\n");
  printf("Wait for NOW! then press any key.\n");
  printf("Result in ms on 7-seg. Q: quit\n\n");

  while (true) {
    while (tcm_keyboard_get_code() != 0) {
    }

    printf("Get ready...\n");
    uint32_t wait = 1000 + tcm_rng_next(rng_state) % 3500;
    tcm_delay_ms(wait);

    if (tcm_keyboard_get_code() != 0) {
      printf("Too early! Press any key to retry.\n");
      while (tcm_keyboard_get_code() == 0) {
      }
      continue;
    }

    printf("NOW! PRESS ANY KEY!\n");
    uint32_t s0 = tcm_syscall_get_timestamp();
    uint32_t us0 = tcm_syscall_get_timestamp_micro();
    while (tcm_keyboard_get_code() == 0) {
    }
    uint32_t s1 = tcm_syscall_get_timestamp();
    uint32_t us1 = tcm_syscall_get_timestamp_micro();

    uint32_t dt_us = (s1 - s0) * 1000000u +
                     (us1 >= us0 ? us1 - us0 : us1 + 1000000u - us0);
    uint32_t ms = dt_us / 1000;

    printf("Your time: %u ms (%u us)\n\n", ms, dt_us);
    tcm_seven_segment_display_upper_decimal(ms);
    tcm_seven_segment_display_lower_decimal(dt_us % 1000000);

    uint32_t code = 0;
    while (code == 0) {
      code = tcm_keyboard_get_code();
    }
    if (code == 'q' || code == 'Q')
      break;
  }

  tcm_ascii_console_clear();
  printf("Bye! Average human reaction is ~250ms.\n");
  return 0;
}
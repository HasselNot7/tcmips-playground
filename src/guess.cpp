#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "tcm_util.h"

int main() {
  uint32_t rng_state = tcm_rand_seed();

  tcm_ascii_console_init();
  tcm_ascii_console_mode(true, true);
  tcm_seven_segment_display_clear();

  while (true) {
    tcm_ascii_console_clear();
    printf("===== Guess The Number =====\n");
    printf("I picked a number between 0 and 9999.\n");
    printf("Try to guess it! (7-seg shows tries)\n\n");

    uint32_t target = tcm_rng_next(rng_state) % 10000;
    uint32_t tries = 0;

    while (true) {
      printf("Your guess: ");
      char buf[16];
      int n = tcm_ascii_console_read_string(buf, sizeof(buf));
      if (n == 0)
        continue;
      int guess = atoi(buf);
      tries++;
      tcm_seven_segment_display_upper_decimal(tries);

      if (guess < (int)target) {
        printf("  %d is too SMALL. Try higher.\n", guess);
      } else if (guess > (int)target) {
        printf("  %d is too BIG. Try lower.\n", guess);
      } else {
        printf("  BINGO! %d is correct in %u tries!\n", guess, tries);
        tcm_seven_segment_display_lower_decimal(target);
        break;
      }
    }

    printf("\nPlay again? (y/n) ");
    char again[4];
    int m = tcm_ascii_console_read_string(again, sizeof(again));
    if (m == 0 || (again[0] != 'y' && again[0] != 'Y'))
      break;
  }

  tcm_ascii_console_clear();
  printf("Thanks for playing! Bye.\n");
  return 0;
}
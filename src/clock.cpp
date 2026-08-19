#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <cstdint>
#include <cstdio>

int main() {
  tcm_ascii_console_init();
  tcm_ascii_console_clear();
  tcm_seven_segment_display_clear();

  printf("===== TCM Clock =====\n");
  printf("Upper 7-seg: days since epoch\n");
  printf("Lower 7-seg: HH:MM:SS\n");
  printf("Press Q to exit\n\n");

  uint32_t last_sec = 0xFFFFFFFF;
  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    if (code == 'q' || code == 'Q')
      break;

    uint32_t sec = tcm_syscall_get_timestamp();
    if (sec != last_sec) {
      uint32_t days = sec / 86400u;
      uint32_t t = sec % 86400u;
      uint32_t hh = t / 3600u;
      uint32_t mm = (t % 3600u) / 60u;
      uint32_t ss = t % 60u;

      tcm_seven_segment_display_upper_decimal(days);
      tcm_seven_segment_display_lower_decimal(hh * 10000u + mm * 100u + ss);

      printf("%02u:%02u:%02u   day %u (epoch)\n", hh, mm, ss, days);
      last_sec = sec;
    }
  }

  tcm_ascii_console_clear();
  printf("Clock stopped. Bye!\n");
  return 0;
}
/* Minimal embedded-file read test on device */
#include <stdio.h>
#include <string.h>
#include <dev/console.h>

void tcm_embed_init(void);
extern int open(const char *, int, ...);
extern ssize_t read(int, void *, size_t);
extern int close(int);

int main(void) {
    tcm_ascii_console_init();
    tcm_ascii_console_clear();
    extern const struct { const char *name; const unsigned char *data; unsigned int size; } tcm_nh_files[];
    extern const int tcm_nh_file_count;
    {
        char msg[80];
        snprintf(msg, sizeof msg, "count=%d\r\n", tcm_nh_file_count);
        tcm_ascii_console_write_string(msg);
        snprintf(msg, sizeof msg, "name0='%s'\r\n",
                 tcm_nh_file_count > 0 ? tcm_nh_files[0].name : "?");
        tcm_ascii_console_write_string(msg);
        if (tcm_nh_file_count > 0) {
            snprintf(msg, sizeof msg, "size0=%u\r\n", tcm_nh_files[0].size);
            tcm_ascii_console_write_string(msg);
        }
        snprintf(msg, sizeof msg, "name70='%s'\r\n",
                 tcm_nh_file_count > 70 ? tcm_nh_files[70].name : "?");
        tcm_ascii_console_write_string(msg);
    }
    tcm_embed_init();
    {
        extern int tcm_fs_diag(char *, int);
        char msg[128];
        tcm_fs_diag(msg, sizeof msg);
        tcm_ascii_console_write_string(msg);
        tcm_ascii_console_write_string("\r\n");
        extern volatile int tcm_fs_open_calls;
        snprintf(msg, sizeof msg, "opencalls=%d\r\n", tcm_fs_open_calls);
        tcm_ascii_console_write_string(msg);
    }

    tcm_ascii_console_write_string("T3 open test\r\n");

    /* test 1: bare open - which implementation wins the link? */
    int fd = open("dungeon.lua", 0 /* O_RDONLY */);
    char msg[64];
    snprintf(msg, sizeof msg, "open=%d\r\n", fd);
    tcm_ascii_console_write_string(msg);
    if (fd >= 0) {
        unsigned char buf[16];
        long n = read(fd, buf, sizeof buf);
        snprintf(msg, sizeof msg, "read=%ld first=%02x %02x\r\n",
                 n, (unsigned) buf[0], (unsigned) buf[1]);
        tcm_ascii_console_write_string(msg);
        close(fd);
    }

    /* test 2: fopen path (picolibc stdio) */
    FILE *f = fopen("dungeon.lua", "r");
    snprintf(msg, sizeof msg, "fopen=%p\r\n", (void *) f);
    tcm_ascii_console_write_string(msg);
    if (f) {
        int c = fgetc(f);
        snprintf(msg, sizeof msg, "fgetc=%d\r\n", c);
        tcm_ascii_console_write_string(msg);
        fclose(f);
    }

    tcm_ascii_console_write_string("T3 done\r\n");
    for (;;) {
        volatile unsigned char *v = (volatile unsigned char *) 0x03C00000;
        for (volatile int d = 0; d < 300000; d++) {}
        v[0] = v[0] ? 0 : 0xFF;
    }
    return 0;
}

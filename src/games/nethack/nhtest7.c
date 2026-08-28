/* T7: RAM file layer test (creat/write/close/open/read) + heap headroom */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dev/console.h>

static char tb[96];

static void
pl(const char *s)
{
    tcm_ascii_console_write_string(s);
    tcm_ascii_console_write_string("\r\n");
}

int
main(void)
{
    tcm_ascii_console_init();
    tcm_ascii_console_clear();
    pl("T7 fs test");

    snprintf(tb, sizeof tb, "brk=%lx", (unsigned long) sbrk(0));
    pl(tb);

    int fd = creat("1lock.0", 0644);
    snprintf(tb, sizeof tb, "creat=%d", fd);
    pl(tb);
    if (fd < 0) {
        snprintf(tb, sizeof tb, "errno=%d", errno);
        pl(tb);
        goto end;
    }
    {
        char hdr[16] = "TESTDATA1234567";
        long w = write(fd, hdr, 16);
        snprintf(tb, sizeof tb, "write=%ld", (long) w);
        pl(tb);
        int c = close(fd);
        snprintf(tb, sizeof tb, "close=%d", c);
        pl(tb);
    }

    {
        int rd = open("1lock.0", O_RDONLY);
        snprintf(tb, sizeof tb, "open=%d", rd);
        pl(tb);
        if (rd >= 0) {
            char buf[32];
            memset(buf, 0, sizeof buf);
            long n = read(rd, buf, 16);
            snprintf(tb, sizeof tb, "read=%ld '%s'", (long) n, buf);
            pl(tb);
            close(rd);
        }
    }

    {
        /* second open + rewrite like the game does */
        int w2 = open("1lock.0", O_WRONLY | O_CREAT | O_TRUNC);
        snprintf(tb, sizeof tb, "open2=%d", w2);
        pl(tb);
        if (w2 >= 0) {
            write(w2, "ABCDEFGH", 8);
            close(w2);
        }
        int rd2 = open("1lock.0", O_RDONLY);
        snprintf(tb, sizeof tb, "open3=%d", rd2);
        pl(tb);
        if (rd2 >= 0) {
            char buf[32];
            memset(buf, 0, sizeof buf);
            long n = read(rd2, buf, 8);
            snprintf(tb, sizeof tb, "read3=%ld '%s'", (long) n, buf);
            pl(tb);
            close(rd2);
        }
    }

end:
    snprintf(tb, sizeof tb, "brk2=%lx", (unsigned long) sbrk(0));
    pl(tb);
    pl("T7 done");
    for (;;) {}
}

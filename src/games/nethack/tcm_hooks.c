/* TCMIPS shared port hooks for NetHack (host + device). */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "tcm_port.h"
#include "hack.h"

#ifndef TCM_HOST
#include <dev/syscall.h>
#include <dev/console.h>
#endif

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

unsigned long tcm_ms_base;

#ifndef TCM_HOST
unsigned long tcm_boot_ms; /* wall time at main() entry */

unsigned long
tcm_wall_ms(void)
{
    return (unsigned long) tcm_syscall_get_timestamp() * 1000UL
           + (unsigned long) tcm_syscall_get_timestamp_milli();
}
#endif

clock_t
clock(void)
{
    return (clock_t) (tcm_ms_base / 1000);
}

#ifndef TCM_HOST
void
tcm_vmp_probe(const char *tag, long a, long b)
{
    char tb[64];
    snprintf(tb, sizeof tb, "%s %ld %ld\r\n", tag, a, b);
    tcm_ascii_console_write_string(tb);
}
#endif

unsigned long
tcm_napms_tick(void)
{
    return tcm_ms_base;
}

void
tcm_napms(int ms)
{
#ifdef TCM_HOST
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (long) ms * 1000000L;
    nanosleep(&ts, 0);
#else
    uint32_t start, elapsed = 0, now;
    if (ms <= 0)
        return;
    start = tcm_syscall_get_timestamp_micro();
    while (elapsed < (uint32_t) ms * 1000u) {
        now = tcm_syscall_get_timestamp_micro();
        elapsed = now - start;
        if ((long) elapsed < 0)
            break;
    }
#endif
    tcm_ms_base += (unsigned long) ms;
}

long
tcm_rand(void)
{
    static unsigned long s0, s1;
    unsigned long x, y;

    if (!s0 && !s1) {
        s0 = (unsigned long) time(0) | 1;
        s1 = (unsigned long) &tcm_ms_base ^ tcm_ms_base ^ 0x9e3779b9UL;
    }
    x = s0;
    y = s1;
    s0 = y;
    x ^= x << 23;
    s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
    return (long) (s1 & 0x7fffffffL);
}

/* port hook used by core/rnd.c */
unsigned long
sys_random_seed(void)
{
    return ((unsigned long) time(0) * 1103515245UL + 12345UL)
           ^ (unsigned long) tcm_ms_base ^ (unsigned long) &tcm_rand;
}

/* port hook: mangle illegal filename characters */
void
regularize(char *s)
{
    for (; *s; s++)
#if 'Z' - 'A' == 25
        if (*s >= 'A' && *s <= 'Z')
            continue;
        else
#endif
            if (!(*s >= 'a' && *s <= 'z') && !(*s >= '0' && *s <= '9')
                && *s != '.' && *s != '-' && *s != '_')
                *s = '_';
}

/* interrupt enable/disable: no signals on this platform */
void
intron(void)
{
}

void
introff(void)
{
}

/* ^Z suspend: not possible here */
int
dosuspend(void)
{
    pline("Suspension is not possible.");
    return ECMD_OK;
}

/* directory handling: single flat filesystem, nothing to do */
void
chdirx(const char *dir UNUSED, boolean wr UNUSED)
{
}

/* port hook from unixunix.c: establish the single-player lock.
 * Upstream getlock() creat()s an empty <lockname>.0 level-0 lock file and
 * writes the raw hackpid into it; savestateinlock() reopens level 0 for
 * read-modify-write at every checkpoint and treats a missing file as
 * tampering (done(TRICKED)), so this file must exist before newgame(). */
void
getlock(void)
{
    char *tf;
    int fd, pid;

    set_savefile_name(TRUE);
    tf = strrchr(gl.lock, '.');
    if (!tf)
        tf = eos(gl.lock);
    Sprintf(tf, ".0");
    fd = open(gl.lock, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if (fd >= 0) {
        pid = (int) getpid();
        (void) write(fd, (genericptr_t) &pid, sizeof pid);
        (void) close(fd);
    }
}

/* NHUUID support: no persistent uuid on this platform */
void
get_nhuuid(void)
{
}

void
free_nhuuid(void)
{
    svn.nhuuid[0] = '\0';
}

/* fatal startup error (normally provided by the port's main module) */
void
error(const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSZ];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
#ifdef TCM_HOST
    fprintf(stderr, "%s\n", buf);
#else
    tcm_vterm_raw("NetHack error: ");
    tcm_vterm_raw(buf);
    tcm_vterm_raw("\n");
    tcm_vterm_flush();
#endif
    exit(EXIT_FAILURE);
}

boolean
authorize_wizard_mode(void)
{
    return FALSE; /* no debug mode on the console */
}

boolean
authorize_explore_mode(void)
{
    return TRUE; /* explore mode allowed if ever requested */
}

/* port hook from unixmain.c: user-name lookup, skipped on this platform */
boolean
whoami(void)
{
    return FALSE;
}

/* port hook: does a regular file exist? */
boolean
file_exists(const char *path)
{
    struct stat sb;
    return stat(path, &sb) == 0 && S_ISREG(sb.st_mode);
}

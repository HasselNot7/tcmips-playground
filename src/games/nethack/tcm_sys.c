/* TCMIPS system layer for NetHack: RAM filesystem, libc overrides,
 * random, timing, environment. */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

static unsigned long tcm_ms_base;

#ifndef TCM_HOST
#include <dev/console.h>
#include <dev/syscall.h>
#endif

#include "tcm_port.h"
#include "nhdata.h"
#include "hack.h"

#ifndef TCM_HOST
/* ---------------- RAM filesystem ---------------- */

#define RM_MAXFILES 96
#define RM_MAXFD 24
#define EMBED_FD 1000

typedef struct {
    int used;
    char *name;
    unsigned char *data;
    long size, cap;
} rfile;

typedef struct {
    int used;
    rfile *f;
    long pos;
} rfd;

static rfile rfiles[RM_MAXFILES];
static rfd rfds[RM_MAXFD];

/* embedded file cursor (single active at a time) */
static const unsigned char *emb_ptr;
static unsigned int emb_len, emb_off;

/*
 * The device cannot reliably read very large const arrays placed in
 * flash/rodata (wolf3d lesson: >~500KB reads back zeroed), so copy the
 * whole file table and every blob into heap RAM once at startup.
 */
static tcm_nh_file_t efiles[192];
static int efile_count;

void
tcm_embed_init(void)
{
    int i;
    int n = tcm_nh_file_count;

    if (n > (int) (sizeof(efiles) / sizeof(efiles[0])))
        n = (int) (sizeof(efiles) / sizeof(efiles[0]));
    for (i = 0; i < n; i++) {
        unsigned int sz = tcm_nh_files[i].size;
        unsigned char *ram = (unsigned char *) malloc(sz ? sz : 1);
        if (!ram)
            continue;
        memcpy(ram, tcm_nh_files[i].data, sz);
        efiles[i].name = tcm_nh_files[i].name;
        efiles[i].data = ram;
        efiles[i].size = sz;
    }
    efile_count = n;
}

static const char *
baseof(const char *path)
{
    const char *b = strrchr(path, '/');
    return b ? b + 1 : path;
}

static rfile *
rm_find(const char *path)
{
    const char *b = baseof(path);
    for (int i = 0; i < RM_MAXFILES; i++)
        if (rfiles[i].used && !strcmp(rfiles[i].name, b))
            return &rfiles[i];
    return 0;
}

static rfile *
rm_alloc(const char *path)
{
    const char *b = baseof(path);
    for (int i = 0; i < RM_MAXFILES; i++) {
        if (!rfiles[i].used) {
            rfiles[i].used = 1;
            rfiles[i].name = strdup(b);
            rfiles[i].data = (unsigned char *) malloc(256);
            rfiles[i].cap = 256;
            rfiles[i].size = 0;
            if (!rfiles[i].name || !rfiles[i].data)
                return 0;
            return &rfiles[i];
        }
    }
    return 0;
}

static int
rm_grow(rfile *f, long need)
{
    if (need <= f->cap)
        return 1;
    long nc = f->cap ? f->cap : 256;
    while (nc < need)
        nc *= 2;
    unsigned char *nd = (unsigned char *) realloc(f->data, (size_t) nc);
    if (!nd)
        return 0;
    f->data = nd;
    f->cap = nc;
    return 1;
}

int
open(const char *path, int flags, ...)
{
    int wr = (flags & (O_WRONLY | O_RDWR)) != 0;
    rfile *f;
    int fd, i;

    if (!(flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC))) {
        /* read access: serve embedded data if known */
        const char *b = baseof(path);
        for (int j = 0; j < efile_count; j++) {
            if (!strcmp(efiles[j].name, b)) {
                emb_ptr = efiles[j].data;
                emb_len = efiles[j].size;
                emb_off = 0;
                return EMBED_FD;
            }
        }
    }
    f = rm_find(path);
    if (!f) {
        if (!wr && !(flags & O_CREAT))
            return -1;
        f = rm_alloc(path);
        if (!f) {
            errno = ENFILE;
            return -1;
        }
    } else if ((flags & O_EXCL) && (flags & O_CREAT))
        return -1;
    else if (wr && !(flags & O_APPEND))
        f->size = 0;
    for (i = 3; i < RM_MAXFD; i++) {
        if (!rfds[i].used)
            break;
    }
    if (i >= RM_MAXFD) {
        errno = EMFILE;
        return -1;
    }
    rfds[i].used = 1;
    rfds[i].f = f;
    rfds[i].pos = ((flags & O_APPEND) && wr) ? f->size : 0;
    fd = i;
    return fd;
}

int
close(int fd)
{
    if (fd == EMBED_FD)
        return 0;
    if (fd < 0 || fd >= RM_MAXFD || !rfds[fd].used)
        return -1;
    rfds[fd].used = 0;
    return 0;
}

ssize_t
read(int fd, void *buf, size_t n)
{
    if (fd == EMBED_FD) {
        long avail = (long) emb_len - (long) emb_off;
        if (avail <= 0 || n == 0)
            return 0;
        if ((long) n > avail)
            n = (size_t) avail;
        memcpy(buf, emb_ptr + emb_off, n);
        emb_off += (unsigned int) n;
        return (ssize_t) n;
    }
    if (fd == 0) {
        unsigned char *p = (unsigned char *) buf;
        for (size_t i = 0; i < n; i++)
            p[i] = (unsigned char) tcm_getch();
        return (ssize_t) n;
    }
    if (fd < 0 || fd >= RM_MAXFD || !rfds[fd].used)
        return -1;
    {
        rfile *f = rfds[fd].f;
        long avail = f->size - rfds[fd].pos;
        if (avail <= 0)
            return 0;
        if ((long) n > avail)
            n = (size_t) avail;
        memcpy(buf, f->data + rfds[fd].pos, n);
        rfds[fd].pos += (long) n;
        return (ssize_t) n;
    }
}

ssize_t
write(int fd, const void *buf, size_t n)
{
    if (fd == EMBED_FD)
        return -1;
    if (fd == 1 || fd == 2 || fd == 0) {
#ifdef TCM_HOST
        fwrite(buf, 1, n, stdout);
        fflush(stdout);
#else
        /* render via vterm: never touch the driver's scrolling cursor */
        {
            const char *s = (const char *) buf;
            size_t i;
            for (i = 0; i < n; i++) {
                char tmp[2] = { s[i], 0 };
                tcm_vterm_raw(tmp);
            }
            tcm_vterm_flush();
        }
#endif
        return (ssize_t) n;
    }
    if (fd < 0 || fd >= RM_MAXFD || !rfds[fd].used)
        return -1;
    {
        rfile *f = rfds[fd].f;
        if (!rm_grow(f, rfds[fd].pos + (long) n))
            return -1;
        memcpy(f->data + rfds[fd].pos, buf, n);
        rfds[fd].pos += (long) n;
        if (rfds[fd].pos > f->size)
            f->size = rfds[fd].pos;
        return (ssize_t) n;
    }
}

off_t
lseek(int fd, off_t off, int whence)
{
    if (fd == EMBED_FD) {
        switch (whence) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            off += (off_t) emb_off;
            break;
        case SEEK_END:
            off += (off_t) emb_len;
            break;
        default:
            return -1;
        }
        if (off < 0)
            return -1;
        emb_off = (unsigned int) off;
        return off;
    }
    if (fd < 0 || fd >= RM_MAXFD || !rfds[fd].used)
        return -1;
    {
        rfile *f = rfds[fd].f;
        switch (whence) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            off += (off_t) rfds[fd].pos;
            break;
        case SEEK_END:
            off += (off_t) f->size;
            break;
        default:
            return -1;
        }
        if (off < 0)
            return -1;
        if ((long) off > f->size) {
            if (!rm_grow(f, (long) off))
                return -1;
            memset(f->data + f->size, 0, (size_t) ((long) off - f->size));
            f->size = (long) off;
        }
        rfds[fd].pos = (long) off;
        return off;
    }
}

int
unlink(const char *path)
{
    rfile *f = rm_find(path);
    if (!f)
        return -1;
    free(f->name);
    free(f->data);
    f->used = 0;
    return 0;
}

int
link(const char *oldp, const char *newp)
{
    rfile *src = rm_find(oldp), *dst;
    if (!src)
        return -1;
    if (rm_find(newp))
        return -1;
    dst = rm_alloc(newp);
    if (!dst)
        return -1;
    if (!rm_grow(dst, src->size ? src->size : 1))
        return -1;
    memcpy(dst->data, src->data, (size_t) src->size);
    dst->size = src->size;
    return 0;
}

int
creat(const char *path, mode_t mode)
{
    (void) mode;
    return open(path, O_WRONLY | O_CREAT | O_TRUNC);
}

int
chmod(const char *path, mode_t mode)
{
    (void) path;
    (void) mode;
    return 0;
}

int
stat(const char *path, struct stat *st)
{
    const char *b = baseof(path);
    for (int j = 0; j < efile_count; j++) {
        if (!strcmp(efiles[j].name, b)) {
            memset(st, 0, sizeof(*st));
            st->st_size = (off_t) efiles[j].size;
            st->st_mode = S_IFREG | 0444;
            return 0;
        }
    }
    {
        rfile *f = rm_find(path);
        if (!f)
            return -1;
        memset(st, 0, sizeof(*st));
        st->st_size = (off_t) f->size;
        st->st_mode = S_IFREG | 0644;
        return 0;
    }
}

int
fstat(int fd, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;
    if (fd == EMBED_FD) {
        st->st_mode = S_IFREG | 0444;
        st->st_size = (off_t) emb_len;
        return 0;
    }
    if (fd >= 0 && fd <= 2)
        return 0;
    if (fd < 0 || fd >= RM_MAXFD || !rfds[fd].used)
        return -1;
    st->st_mode = S_IFREG | 0644;
    st->st_size = (off_t) rfds[fd].f->size;
    return 0;
}

int
isatty(int fd)
{
    return (fd >= 0 && fd <= 2);
}

int
access(const char *path, int amode)
{
    (void) amode;
    {
        const char *b = baseof(path);
        for (int j = 0; j < efile_count; j++)
            if (!strcmp(efiles[j].name, b))
                return 0;
    }
    return rm_find(path) ? 0 : -1;
}

int
rename(const char *oldp, const char *newp)
{
    rfile *src = rm_find(oldp), *dst;
    if (!src)
        return -1;
    dst = rm_find(newp);
    if (dst) {
        free(dst->name);
        free(dst->data);
        dst->used = 0;
    }
    return link(oldp, newp) || unlink(oldp) ? -1 : 0;
}

int
fcntl(int fd, int cmd, ...)
{
    (void) fd;
    (void) cmd;
    return 0; /* file locking is a no-op on the RAM filesystem */
}

/* ---------------- time ---------------- */

time_t
time(time_t *t)
{
    time_t v;
#ifdef TCM_HOST
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    v = (time_t) ts.tv_sec;
#else
    v = (time_t) tcm_syscall_get_timestamp();
#endif
    if (t)
        *t = v;
    return v;
}

clock_t
clock(void)
{
    return (clock_t) (tcm_ms_base / 1000);
}

unsigned long
tcm_napms_tick(void)
{
    return tcm_ms_base;
}


/* ---------------- environment ---------------- */

static const struct {
    const char *name;
    const char *val;
} tcm_env[] = {
    { "NETHACKOPTIONS",
      "name:Hero,role:Valkyrie,race:Dwarf,gender:female,align:lawful" },
    { "TERM", "tcmips" },
    { "HACKDIR", "/" },
};

char *
getenv(const char *name)
{
    /* return a writable copy: option parsers mutate the buffer */
    for (unsigned i = 0; i < sizeof(tcm_env) / sizeof(tcm_env[0]); i++)
        if (!strcmp(name, tcm_env[i].name)) {
            static char copied[4][512];
            static unsigned slot;
            char *dst = copied[slot & 3];
            unsigned len = (unsigned) strlen(tcm_env[i].val);
            if (len >= sizeof copied[0])
                len = sizeof copied[0] - 1;
            ++slot;
            memcpy(dst, tcm_env[i].val, len);
            dst[len] = '\0';
            return dst;
        }
    return 0;
}

int
putenv(char *s UNUSED)
{
    return 0;
}

int
setenv(const char *n UNUSED, const char *v UNUSED, int o UNUSED)
{
    return 0;
}

/* ---------------- misc libc bits some ports expect ---------------- */

uid_t
getuid(void)
{
    return 1000;
}
uid_t
geteuid(void)
{
    return 1000;
}
gid_t
getgid(void)
{
    return 1000;
}
pid_t
getpid(void)
{
    return 42;
}
int
kill(pid_t p UNUSED, int sig UNUSED)
{
    return 0;
}

/* process control: never actually used (msghandler is unset) */
pid_t
fork(void)
{
    return -1;
}
int
execv(const char *path UNUSED, char *const argv[] UNUSED)
{
    return 0;
}
pid_t
waitpid(pid_t pid UNUSED, int *status UNUSED, int opts UNUSED)
{
    return 0;
}
int
setuid(uid_t u UNUSED)
{
    return 0;
}
int
setgid(gid_t g UNUSED)
{
    return 0;
}

char *
getcwd(char *buf, size_t size)
{
    if (!buf || size < 3)
        return 0;
    strcpy(buf, "/");
    return buf;
}

#endif /* !TCM_HOST */

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

unsigned int
sleep(unsigned int seconds)
{
    tcm_napms((int) seconds * 1000);
    return 0;
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

/* directory handling: single flat RAM filesystem, nothing to do */
void
chdirx(const char *dir UNUSED, boolean wr UNUSED)
{
}

/* port hook from unixunix.c: establish single-player lock.
 * The RAM filesystem is private to the console, so just clear any
 * stale lock left behind by an earlier session. */
void
getlock(void)
{
    char lockbuf[BUFSZ];
    const char *b;

    set_savefile_name(TRUE);
    b = strrchr(gs.SAVEF, '/');
    Sprintf(lockbuf, "%s.lock", b ? b + 1 : gs.SAVEF);
    (void) unlink(lockbuf); /* clear stale lock */
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
    {
        char ebuf[BUFSZ + 32];
        Snprintf(ebuf, sizeof ebuf, "NetHack error: %s\n", buf);
        tcm_vterm_raw(ebuf);
        tcm_vterm_flush();
    }
#endif
    exit(EXIT_FAILURE);
}


/* ---------------- timing / random (shared) ---------------- */

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
           ^ (unsigned long) tcm_ms_base ^ (unsigned long) &srandom;
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

/* TCMIPS device filesystem for NetHack: RAM filesystem serving the
 * embedded dat/ blobs plus libc file-call overrides. Device-only. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef TCM_HOST
#include <dev/console.h>
#include <dev/syscall.h>
#endif

#include "tcm_port.h"
#include "nhdata.h"

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

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

static const unsigned char *emb_ptr;
static unsigned int emb_len, emb_off;

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

static tcm_nh_file_t efiles[192];
static int efile_count;

void
tcm_embed_init(void)
{
    int n = tcm_nh_file_count;
    if (n > (int) (sizeof(efiles) / sizeof(efiles[0])))
        n = (int) (sizeof(efiles) / sizeof(efiles[0]));
    for (int i = 0; i < n; i++) {
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

/* diagnostic: report embed state (device bring-up debugging) */
int
tcm_fs_diag(char *out, int maxlen)
{
    int found = -1;
    for (int i = 0; i < efile_count; i++)
        if (!strcmp(efiles[i].name, "dungeon.lua")) {
            found = i;
            break;
        }
    return snprintf(out, (size_t) maxlen,
                     "efile_count=%d dun_idx=%d", efile_count, found);
}

volatile int tcm_fs_open_calls;
volatile int tcm_fs_read_calls;

int
open(const char *path, int flags, ...)
{
    tcm_fs_open_calls++;
    int wr = (flags & (O_WRONLY | O_RDWR)) != 0;
    rfile *f;
    int i;

    if (!(flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC))) {
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
    } else if ((flags & O_EXCL) && (flags & O_CREAT)) {
        return -1;
    } else if (wr && !(flags & O_APPEND)) {
        f->size = 0;
    }
    for (i = 3; i < RM_MAXFD; i++)
        if (!rfds[i].used)
            break;
    if (i >= RM_MAXFD) {
        errno = EMFILE;
        return -1;
    }
    rfds[i].used = 1;
    rfds[i].f = f;
    rfds[i].pos = ((flags & O_APPEND) && wr) ? f->size : 0;
    return i;
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
    if (fd >= 0 && fd <= 2) {
        const char *s = (const char *) buf;
        for (size_t i = 0; i < n; i++) {
            char tmp[2] = { s[i], 0 };
            tcm_vterm_raw(tmp);
        }
        tcm_vterm_flush();
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
        default:
            off += (off_t) (whence == SEEK_END ? emb_len : emb_off);
            break;
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
        default:
            off += (off_t) f->size;
            break;
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

static int
open_common(const char *path, int flags)
{
    return open(path, flags);
}

int
creat(const char *path, mode_t mode)
{
    (void) mode;
    return open_common(path, O_WRONLY | O_CREAT | O_TRUNC);
}

int
creat64(const char *path, mode_t mode)
{
    return creat(path, mode);
}

int
open64(const char *path, int flags, ...)
{
    return open_common(path, flags);
}

int
openat(int dirfd, const char *path, int flags, ...)
{
    (void) dirfd;
    return open_common(path, flags);
}

int
openat64(int dirfd, const char *path, int flags, ...)
{
    (void) dirfd;
    return open_common(path, flags);
}

int
chmod(const char *path UNUSED, mode_t mode UNUSED)
{
    return 0;
}

int
fcntl(int fd, int cmd, ...)
{
    (void) fd;
    (void) cmd;
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
access(const char *path, int amode UNUSED)
{
    const char *b = baseof(path);
    for (int j = 0; j < efile_count; j++)
        if (!strcmp(efiles[j].name, b))
            return 0;
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

char *
getcwd(char *buf, size_t size)
{
    if (!buf || size < 3)
        return 0;
    strcpy(buf, "/");
    return buf;
}

unsigned int
sleep(unsigned int seconds)
{
    tcm_napms((int) seconds * 1000);
    return 0;
}

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

#ifndef TCM_HOST
time_t
time(time_t *t)
{
    time_t v = (time_t) tcm_syscall_get_timestamp();
    if (t)
        *t = v;
    return v;
}
#endif

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
            static char copied[4][256];
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
/* libc.bc has internal references to the original POSIX names; provide
 * strong aliases so those resolve too regardless of link order. */
#ifndef TCM_HOST
#undef open
#undef read
#undef write
#undef close
#undef lseek
#undef unlink
#undef link
#undef creat
#undef stat
#undef fstat
#undef isatty
#undef access
#undef rename
#undef fcntl
#undef getcwd
#undef chmod
#undef open64
#undef creat64
#undef openat
#undef openat64

int open(const char *p, int f, ...)  { return tcm_fs_open(p, f); }
ssize_t read(int f, void *b, size_t n)  { return tcm_fs_read(f, b, n); }
ssize_t write(int f, const void *b, size_t n)  { return tcm_fs_write(f, b, n); }
int close(int f)  { return tcm_fs_close(f); }
off_t lseek(int f, off_t o, int w)  { return tcm_fs_lseek(f, o, w); }
int unlink(const char *p)  { return tcm_fs_unlink(p); }
int link(const char *a, const char *b)  { return tcm_fs_link(a, b); }
int creat(const char *p, mode_t m)  { return tcm_fs_creat(p, m); }
int stat(const char *p, struct stat *s)  { return tcm_fs_stat(p, s); }
int fstat(int f, struct stat *s)  { return tcm_fs_fstat(f, s); }
int isatty(int f)  { return tcm_fs_isatty(f); }
int access(const char *p, int m)  { return tcm_fs_access(p, m); }
int rename(const char *a, const char *b)  { return tcm_fs_rename(a, b); }
int fcntl(int f, int c, ...)  { return tcm_fs_fcntl(f, c); }
char *getcwd(char *b, size_t n)  { return tcm_fs_getcwd(b, n); }
int chmod(const char *p, mode_t m)  { return tcm_fs_chmod(p, m); }
int open64(const char *p, int f, ...)  { return tcm_fs_open64(p, f); }
int creat64(const char *p, mode_t m)  { return tcm_fs_creat64(p, m); }
int openat(int d, const char *p, int f, ...)  { return tcm_fs_openat(d, p, f); }
int openat64(int d, const char *p, int f, ...)  { return tcm_fs_openat64(d, p, f); }
#endif /* !TCM_HOST */

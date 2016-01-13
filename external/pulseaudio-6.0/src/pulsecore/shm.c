/***
    This file is part of PulseAudio.

    Copyright 2006 Lennart Poettering
    Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

    PulseAudio is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of the
    License, or (at your option) any later version.

    PulseAudio is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

/* This is deprecated on glibc but is still used by FreeBSD */
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
# define MAP_ANONYMOUS MAP_ANON
#endif

#include <pulse/xmalloc.h>
#include <pulse/gccmacro.h>

#include <pulsecore/core-error.h>
#include <pulsecore/log.h>
#include <pulsecore/random.h>
#include <pulsecore/core-util.h>
#include <pulsecore/macro.h>
#include <pulsecore/atomic.h>

#include "shm.h"

#if defined(__linux__) && !defined(MADV_REMOVE)
#define MADV_REMOVE 9
#endif

/* 1 GiB at max */
#define MAX_SHM_SIZE (PA_ALIGN(1024*1024*1024))

#ifdef __linux__
/* On Linux we know that the shared memory blocks are files in
 * /dev/shm. We can use that information to list all blocks and
 * cleanup unused ones */
#define SHM_PATH "/dev/shm/"
#define SHM_ID_LEN 10
#elif defined(__sun)
#define SHM_PATH "/tmp"
#define SHM_ID_LEN 15
#else
#undef SHM_PATH
#undef SHM_ID_LEN
#endif

#define SHM_MARKER ((int) 0xbeefcafe)

/* We now put this SHM marker at the end of each segment. It's
 * optional, to not require a reboot when upgrading, though. Note that
 * on multiarch systems 32bit and 64bit processes might access this
 * region simultaneously. The header fields need to be independent
 * from the process' word with */
struct shm_marker {
    pa_atomic_t marker; /* 0xbeefcafe */
    pa_atomic_t pid;
    uint64_t _reserved1;
    uint64_t _reserved2;
    uint64_t _reserved3;
    uint64_t _reserved4;
} PA_GCC_PACKED;

#define SHM_MARKER_SIZE PA_ALIGN(sizeof(struct shm_marker))

#ifdef HAVE_SHM_OPEN
static char *segment_name(char *fn, size_t l, unsigned id) {
    pa_snprintf(fn, l, "/pulse-shm-%u", id);
    return fn;
}
#endif

int pa_shm_create_rw(pa_shm *m, size_t size, bool shared, mode_t mode) {
#ifdef HAVE_SHM_OPEN
    char fn[32];
    int fd = -1;
#endif

    pa_assert(m);
    pa_assert(size > 0);
    pa_assert(size <= MAX_SHM_SIZE);
    pa_assert(!(mode & ~0777));
    pa_assert(mode >= 0600);

    /* Each time we create a new SHM area, let's first drop all stale
     * ones */
    pa_shm_cleanup();

    /* Round up to make it page aligned */
    size = PA_PAGE_ALIGN(size);

    if (!shared) {
        m->id = 0;
        m->size = size;

#ifdef MAP_ANONYMOUS
        if ((m->ptr = mmap(NULL, m->size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, (off_t) 0)) == MAP_FAILED) {
            pa_log("mmap() failed: %s", pa_cstrerror(errno));
            goto fail;
        }
#elif defined(HAVE_POSIX_MEMALIGN)
        {
            int r;

            if ((r = posix_memalign(&m->ptr, PA_PAGE_SIZE, size)) < 0) {
                pa_log("posix_memalign() failed: %s", pa_cstrerror(r));
                goto fail;
            }
        }
#else
        m->ptr = pa_xmalloc(m->size);
#endif

        m->do_unlink = false;

    } else {
#ifdef HAVE_SHM_OPEN
        struct shm_marker *marker;

        pa_random(&m->id, sizeof(m->id));
        segment_name(fn, sizeof(fn), m->id);

        if ((fd = shm_open(fn, O_RDWR|O_CREAT|O_EXCL, mode)) < 0) {
            pa_log("shm_open() failed: %s", pa_cstrerror(errno));
            goto fail;
        }

        m->size = size + SHM_MARKER_SIZE;

        if (ftruncate(fd, (off_t) m->size) < 0) {
            pa_log("ftruncate() failed: %s", pa_cstrerror(errno));
            goto fail;
        }

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

        if ((m->ptr = mmap(NULL, PA_PAGE_ALIGN(m->size), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_NORESERVE, fd, (off_t) 0)) == MAP_FAILED) {
            pa_log("mmap() failed: %s", pa_cstrerror(errno));
            goto fail;
        }

        /* We store our PID at the end of the shm block, so that we
         * can check for dead shm segments later */
        marker = (struct shm_marker*) ((uint8_t*) m->ptr + m->size - SHM_MARKER_SIZE);
        pa_atomic_store(&marker->pid, (int) getpid());
        pa_atomic_store(&marker->marker, SHM_MARKER);

        pa_assert_se(pa_close(fd) == 0);
        m->do_unlink = true;
#else
        goto fail;
#endif
    }

    m->shared = shared;

    return 0;

fail:

#ifdef HAVE_SHM_OPEN
    if (fd >= 0) {
        shm_unlink(fn);
        pa_close(fd);
    }
#endif

    return -1;
}

void pa_shm_free(pa_shm *m) {
    pa_assert(m);
    pa_assert(m->ptr);
    pa_assert(m->size > 0);

#ifdef MAP_FAILED
    pa_assert(m->ptr != MAP_FAILED);
#endif

    if (!m->shared) {
#ifdef MAP_ANONYMOUS
        if (munmap(m->ptr, m->size) < 0)
            pa_log("munmap() failed: %s", pa_cstrerror(errno));
#elif defined(HAVE_POSIX_MEMALIGN)
        free(m->ptr);
#else
        pa_xfree(m->ptr);
#endif
    } else {
#ifdef HAVE_SHM_OPEN
        if (munmap(m->ptr, PA_PAGE_ALIGN(m->size)) < 0)
            pa_log("munmap() failed: %s", pa_cstrerror(errno));

        if (m->do_unlink) {
            char fn[32];

            segment_name(fn, sizeof(fn), m->id);

            if (shm_unlink(fn) < 0)
                pa_log(" shm_unlink(%s) failed: %s", fn, pa_cstrerror(errno));
        }
#else
        /* We shouldn't be here without shm support */
        pa_assert_not_reached();
#endif
    }

    pa_zero(*m);
}

void pa_shm_punch(pa_shm *m, size_t offset, size_t size) {
    void *ptr;
    size_t o;

    pa_assert(m);
    pa_assert(m->ptr);
    pa_assert(m->size > 0);
    pa_assert(offset+size <= m->size);

#ifdef MAP_FAILED
    pa_assert(m->ptr != MAP_FAILED);
#endif

    /* You're welcome to implement this as NOOP on systems that don't
     * support it */

    /* Align the pointer up to multiples of the page size */
    ptr = (uint8_t*) m->ptr + offset;
    o = (size_t) ((uint8_t*) ptr - (uint8_t*) PA_PAGE_ALIGN_PTR(ptr));

    if (o > 0) {
        size_t delta = PA_PAGE_SIZE - o;
        ptr = (uint8_t*) ptr + delta;
        size -= delta;
    }

    /* Align the size down to multiples of page size */
    size = (size / PA_PAGE_SIZE) * PA_PAGE_SIZE;

#ifdef MADV_REMOVE
    if (madvise(ptr, size, MADV_REMOVE) >= 0)
        return;
#endif

#ifdef MADV_FREE
    if (madvise(ptr, size, MADV_FREE) >= 0)
        return;
#endif

#ifdef MADV_DONTNEED
    madvise(ptr, size, MADV_DONTNEED);
#elif defined(POSIX_MADV_DONTNEED)
    posix_madvise(ptr, size, POSIX_MADV_DONTNEED);
#endif
}

#ifdef HAVE_SHM_OPEN

int pa_shm_attach(pa_shm *m, unsigned id, bool writable) {
    char fn[32];
    int fd = -1;
    int prot;
    struct stat st;

    pa_assert(m);

    segment_name(fn, sizeof(fn), m->id = id);

    if ((fd = shm_open(fn, writable ? O_RDWR : O_RDONLY, 0)) < 0) {
        if (errno != EACCES && errno != ENOENT)
            pa_log("shm_open() failed: %s", pa_cstrerror(errno));
        goto fail;
    }

    if (fstat(fd, &st) < 0) {
        pa_log("fstat() failed: %s", pa_cstrerror(errno));
        goto fail;
    }

    if (st.st_size <= 0 ||
        st.st_size > (off_t) (MAX_SHM_SIZE+SHM_MARKER_SIZE) ||
        PA_ALIGN((size_t) st.st_size) != (size_t) st.st_size) {
        pa_log("Invalid shared memory segment size");
        goto fail;
    }

    m->size = (size_t) st.st_size;

    prot = writable ? PROT_READ | PROT_WRITE : PROT_READ;
    if ((m->ptr = mmap(NULL, PA_PAGE_ALIGN(m->size), prot, MAP_SHARED, fd, (off_t) 0)) == MAP_FAILED) {
        pa_log("mmap() failed: %s", pa_cstrerror(errno));
        goto fail;
    }

    m->do_unlink = false;
    m->shared = true;

    pa_assert_se(pa_close(fd) == 0);

    return 0;

fail:
    if (fd >= 0)
        pa_close(fd);

    return -1;
}

#else /* HAVE_SHM_OPEN */

int pa_shm_attach(pa_shm *m, unsigned id, bool writable) {
    return -1;
}

#endif /* HAVE_SHM_OPEN */

int pa_shm_cleanup(void) {

#ifdef HAVE_SHM_OPEN
#ifdef SHM_PATH
    DIR *d;
    struct dirent *de;

    if (!(d = opendir(SHM_PATH))) {
        pa_log_warn("Failed to read "SHM_PATH": %s", pa_cstrerror(errno));
        return -1;
    }

    while ((de = readdir(d))) {
        pa_shm seg;
        unsigned id;
        pid_t pid;
        char fn[128];
        struct shm_marker *m;

#if defined(__sun)
        if (strncmp(de->d_name, ".SHMDpulse-shm-", SHM_ID_LEN))
#else
        if (strncmp(de->d_name, "pulse-shm-", SHM_ID_LEN))
#endif
            continue;

        if (pa_atou(de->d_name + SHM_ID_LEN, &id) < 0)
            continue;

        if (pa_shm_attach(&seg, id, false) < 0)
            continue;

        if (seg.size < SHM_MARKER_SIZE) {
            pa_shm_free(&seg);
            continue;
        }

        m = (struct shm_marker*) ((uint8_t*) seg.ptr + seg.size - SHM_MARKER_SIZE);

        if (pa_atomic_load(&m->marker) != SHM_MARKER) {
            pa_shm_free(&seg);
            continue;
        }

        if (!(pid = (pid_t) pa_atomic_load(&m->pid))) {
            pa_shm_free(&seg);
            continue;
        }

        if (kill(pid, 0) == 0 || errno != ESRCH) {
            pa_shm_free(&seg);
            continue;
        }

        pa_shm_free(&seg);

        /* Ok, the owner of this shms segment is dead, so, let's remove the segment */
        segment_name(fn, sizeof(fn), id);

        if (shm_unlink(fn) < 0 && errno != EACCES && errno != ENOENT)
            pa_log_warn("Failed to remove SHM segment %s: %s\n", fn, pa_cstrerror(errno));
    }

    closedir(d);
#endif /* SHM_PATH */
#endif /* HAVE_SHM_OPEN */

    return 0;
}

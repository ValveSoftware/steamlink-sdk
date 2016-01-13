/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering
  Copyright 2006-2007 Pierre Ossman <ossman@cendio.se> for Cendio AB

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#ifdef __linux__
#include <linux/sockios.h>
#endif

#include <pulse/pulseaudio.h>
#include <pulse/gccmacro.h>
#include <pulsecore/llist.h>
#include <pulsecore/core-util.h>

/* On some systems SIOCINQ isn't defined, but FIONREAD is just an alias */
#if !defined(SIOCINQ) && defined(FIONREAD)
# define SIOCINQ FIONREAD
#endif

/* make sure gcc doesn't redefine open and friends as macros */
#undef open
#undef open64

typedef enum {
    FD_INFO_MIXER,
    FD_INFO_STREAM,
} fd_info_type_t;

typedef struct fd_info fd_info;

struct fd_info {
    pthread_mutex_t mutex;
    int ref;
    int unusable;

    fd_info_type_t type;
    int app_fd, thread_fd;

    pa_sample_spec sample_spec;
    size_t fragment_size;
    unsigned n_fragments;

    pa_threaded_mainloop *mainloop;
    pa_context *context;
    pa_stream *play_stream;
    pa_stream *rec_stream;
    int play_precork;
    int rec_precork;

    pa_io_event *io_event;
    pa_io_event_flags_t io_flags;

    void *buf;
    size_t rec_offset;

    int operation_success;

    pa_cvolume sink_volume, source_volume;
    uint32_t sink_index, source_index;
    int volume_modify_count;

    int optr_n_blocks;

    PA_LLIST_FIELDS(fd_info);
};

static int dsp_drain(fd_info *i);
static void fd_info_remove_from_list(fd_info *i);

static pthread_mutex_t fd_infos_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t func_mutex = PTHREAD_MUTEX_INITIALIZER;

static PA_LLIST_HEAD(fd_info, fd_infos) = NULL;

static int (*_ioctl)(int, int, void*) = NULL;
static int (*_close)(int) = NULL;
static int (*_open)(const char *, int, mode_t) = NULL;
static int (*___open_2)(const char *, int) = NULL;
static FILE* (*_fopen)(const char *path, const char *mode) = NULL;
static int (*_stat)(const char *, struct stat *) = NULL;
#ifdef _STAT_VER
static int (*___xstat)(int, const char *, struct stat *) = NULL;
#endif
#ifdef HAVE_OPEN64
static int (*_open64)(const char *, int, mode_t) = NULL;
static int (*___open64_2)(const char *, int) = NULL;
static FILE* (*_fopen64)(const char *path, const char *mode) = NULL;
static int (*_stat64)(const char *, struct stat64 *) = NULL;
#ifdef _STAT_VER
static int (*___xstat64)(int, const char *, struct stat64 *) = NULL;
#endif
#endif
static int (*_fclose)(FILE *f) = NULL;
static int (*_access)(const char *, int) = NULL;

/* dlsym() violates ISO C, so confide the breakage into this function to
 * avoid warnings. */
typedef void (*fnptr)(void);
static inline fnptr dlsym_fn(void *handle, const char *symbol) {
    return (fnptr) (long) dlsym(handle, symbol);
}

#define LOAD_IOCTL_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_ioctl)  \
        _ioctl = (int (*)(int, int, void*)) dlsym_fn(RTLD_NEXT, "ioctl"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_OPEN_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_open) \
        _open = (int (*)(const char *, int, mode_t)) dlsym_fn(RTLD_NEXT, "open"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD___OPEN_2_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!___open_2) \
        ___open_2 = (int (*)(const char *, int)) dlsym_fn(RTLD_NEXT, "__open_2"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_OPEN64_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_open64) \
        _open64 = (int (*)(const char *, int, mode_t)) dlsym_fn(RTLD_NEXT, "open64"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD___OPEN64_2_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!___open64_2) \
        ___open64_2 = (int (*)(const char *, int)) dlsym_fn(RTLD_NEXT, "__open64_2"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_CLOSE_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_close) \
        _close = (int (*)(int)) dlsym_fn(RTLD_NEXT, "close"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_ACCESS_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_access) \
        _access = (int (*)(const char*, int)) dlsym_fn(RTLD_NEXT, "access"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_STAT_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_stat) \
        _stat = (int (*)(const char *, struct stat *)) dlsym_fn(RTLD_NEXT, "stat"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_STAT64_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_stat64) \
        _stat64 = (int (*)(const char *, struct stat64 *)) dlsym_fn(RTLD_NEXT, "stat64"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_XSTAT_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!___xstat) \
        ___xstat = (int (*)(int, const char *, struct stat *)) dlsym_fn(RTLD_NEXT, "__xstat"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_XSTAT64_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!___xstat64) \
        ___xstat64 = (int (*)(int, const char *, struct stat64 *)) dlsym_fn(RTLD_NEXT, "__xstat64"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_FOPEN_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_fopen) \
        _fopen = (FILE* (*)(const char *, const char*)) dlsym_fn(RTLD_NEXT, "fopen"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_FOPEN64_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_fopen64) \
        _fopen64 = (FILE* (*)(const char *, const char*)) dlsym_fn(RTLD_NEXT, "fopen64"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define LOAD_FCLOSE_FUNC() \
do { \
    pthread_mutex_lock(&func_mutex); \
    if (!_fclose) \
        _fclose = (int (*)(FILE *)) dlsym_fn(RTLD_NEXT, "fclose"); \
    pthread_mutex_unlock(&func_mutex); \
} while(0)

#define CONTEXT_CHECK_DEAD_GOTO(i, label) do { \
if (!(i)->context || pa_context_get_state((i)->context) != PA_CONTEXT_READY) { \
    debug(DEBUG_LEVEL_NORMAL, __FILE__": Not connected: %s\n", (i)->context ? pa_strerror(pa_context_errno((i)->context)) : "NULL"); \
    goto label; \
} \
} while(0)

#define PLAYBACK_STREAM_CHECK_DEAD_GOTO(i, label) do { \
if (!(i)->context || pa_context_get_state((i)->context) != PA_CONTEXT_READY || \
    !(i)->play_stream || pa_stream_get_state((i)->play_stream) != PA_STREAM_READY) { \
    debug(DEBUG_LEVEL_NORMAL, __FILE__": Not connected: %s\n", (i)->context ? pa_strerror(pa_context_errno((i)->context)) : "NULL"); \
    goto label; \
} \
} while(0)

#define RECORD_STREAM_CHECK_DEAD_GOTO(i, label) do { \
if (!(i)->context || pa_context_get_state((i)->context) != PA_CONTEXT_READY || \
    !(i)->rec_stream || pa_stream_get_state((i)->rec_stream) != PA_STREAM_READY) { \
    debug(DEBUG_LEVEL_NORMAL, __FILE__": Not connected: %s\n", (i)->context ? pa_strerror(pa_context_errno((i)->context)) : "NULL"); \
    goto label; \
} \
} while(0)

static void debug(int level, const char *format, ...) PA_GCC_PRINTF_ATTR(2,3);

#define DEBUG_LEVEL_ALWAYS                0
#define DEBUG_LEVEL_NORMAL                1
#define DEBUG_LEVEL_VERBOSE               2

static void debug(int level, const char *format, ...) {
    va_list ap;
    const char *dlevel_s;
    int dlevel;

    dlevel_s = getenv("PADSP_DEBUG");
    if (!dlevel_s)
        return;

    dlevel = atoi(dlevel_s);

    if (dlevel < level)
        return;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

static int padsp_disabled(void) {
    static int *sym;
    static int sym_resolved = 0;

    /* If the current process has a symbol __padsp_disabled__ we use
     * it to detect whether we should enable our stuff or not. A
     * program needs to be compiled with -rdynamic for this to work!
     * The symbol must be an int containing a three bit bitmask: bit 1
     * -> disable /dev/dsp emulation, bit 2 -> disable /dev/sndstat
     * emulation, bit 3 -> disable /dev/mixer emulation. Hence a value
     * of 7 disables padsp entirely. */

    pthread_mutex_lock(&func_mutex);
    if (!sym_resolved) {
        sym = (int*) dlsym(RTLD_DEFAULT, "__padsp_disabled__");
        sym_resolved = 1;
    }
    pthread_mutex_unlock(&func_mutex);

    if (!sym)
        return 0;

    return *sym;
}

static int dsp_cloak_enable(void) {
    if (padsp_disabled() & 1)
        return 0;

    if (getenv("PADSP_NO_DSP") || getenv("PULSE_INTERNAL"))
        return 0;

    return 1;
}

static int sndstat_cloak_enable(void) {
    if (padsp_disabled() & 2)
        return 0;

    if (getenv("PADSP_NO_SNDSTAT") || getenv("PULSE_INTERNAL"))
        return 0;

    return 1;
}

static int mixer_cloak_enable(void) {
    if (padsp_disabled() & 4)
        return 0;

    if (getenv("PADSP_NO_MIXER") || getenv("PULSE_INTERNAL"))
        return 0;

    return 1;
}
static pthread_key_t recursion_key;

static void recursion_key_alloc(void) {
    pthread_key_create(&recursion_key, NULL);
}

static int function_enter(void) {
    /* Avoid recursive calls */
    static pthread_once_t recursion_key_once = PTHREAD_ONCE_INIT;
    pthread_once(&recursion_key_once, recursion_key_alloc);

    if (pthread_getspecific(recursion_key))
        return 0;

    pthread_setspecific(recursion_key, (void*) 1);
    return 1;
}

static void function_exit(void) {
    pthread_setspecific(recursion_key, NULL);
}

static void fd_info_free(fd_info *i) {
    assert(i);

    debug(DEBUG_LEVEL_NORMAL, __FILE__": freeing fd info (fd=%i)\n", i->app_fd);

    dsp_drain(i);

    if (i->mainloop)
        pa_threaded_mainloop_stop(i->mainloop);

    if (i->play_stream) {
        pa_stream_disconnect(i->play_stream);
        pa_stream_unref(i->play_stream);
    }

    if (i->rec_stream) {
        pa_stream_disconnect(i->rec_stream);
        pa_stream_unref(i->rec_stream);
    }

    if (i->context) {
        pa_context_disconnect(i->context);
        pa_context_unref(i->context);
    }

    if (i->mainloop)
        pa_threaded_mainloop_free(i->mainloop);

    if (i->app_fd >= 0) {
        LOAD_CLOSE_FUNC();
        _close(i->app_fd);
    }

    if (i->thread_fd >= 0) {
        LOAD_CLOSE_FUNC();
        _close(i->thread_fd);
    }

    free(i->buf);

    pthread_mutex_destroy(&i->mutex);
    free(i);
}

static fd_info *fd_info_ref(fd_info *i) {
    assert(i);

    pthread_mutex_lock(&i->mutex);
    assert(i->ref >= 1);
    i->ref++;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": ref++, now %i\n", i->ref);
    pthread_mutex_unlock(&i->mutex);

    return i;
}

static void fd_info_unref(fd_info *i) {
    int r;
    pthread_mutex_lock(&i->mutex);
    assert(i->ref >= 1);
    r = --i->ref;
    debug(DEBUG_LEVEL_VERBOSE, __FILE__": ref--, now %i\n", i->ref);
    pthread_mutex_unlock(&i->mutex);

    if (r <= 0)
        fd_info_free(i);
}

static void context_state_cb(pa_context *c, void *userdata) {
    fd_info *i = userdata;
    assert(c);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(i->mainloop, 0);
            break;

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
    }
}

static void reset_params(fd_info *i) {
    assert(i);

    i->sample_spec.format = PA_SAMPLE_U8;
    i->sample_spec.channels = 1;
    i->sample_spec.rate = 8000;
    i->fragment_size = 0;
    i->n_fragments = 0;
}

static const char *client_name(char *buf, size_t n) {
    char *p;
    const char *e;

    if ((e = getenv("PADSP_CLIENT_NAME")))
        return e;

    if ((p = pa_get_binary_name_malloc())) {
        snprintf(buf, n, "OSS Emulation[%s]", p);
        pa_xfree(p);
    } else
        snprintf(buf, n, "OSS");

    return buf;
}

static const char *stream_name(void) {
    const char *e;

    if ((e = getenv("PADSP_STREAM_NAME")))
        return e;

    return "Audio Stream";
}

static void atfork_prepare(void) {
    fd_info *i;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": atfork_prepare() enter\n");

    function_enter();

    pthread_mutex_lock(&fd_infos_mutex);

    for (i = fd_infos; i; i = i->next) {
        pthread_mutex_lock(&i->mutex);
        pa_threaded_mainloop_lock(i->mainloop);
    }

    pthread_mutex_lock(&func_mutex);

    debug(DEBUG_LEVEL_NORMAL, __FILE__": atfork_prepare() exit\n");
}

static void atfork_parent(void) {
    fd_info *i;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": atfork_parent() enter\n");

    pthread_mutex_unlock(&func_mutex);

    for (i = fd_infos; i; i = i->next) {
        pa_threaded_mainloop_unlock(i->mainloop);
        pthread_mutex_unlock(&i->mutex);
    }

    pthread_mutex_unlock(&fd_infos_mutex);

    function_exit();

    debug(DEBUG_LEVEL_NORMAL, __FILE__": atfork_parent() exit\n");
}

static void atfork_child(void) {
    fd_info *i;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": atfork_child() enter\n");

    /* We do only the bare minimum to get all fds closed */
    pthread_mutex_init(&func_mutex, NULL);
    pthread_mutex_init(&fd_infos_mutex, NULL);

    for (i = fd_infos; i; i = i->next) {
        pthread_mutex_init(&i->mutex, NULL);

        if (i->context) {
            pa_context_disconnect(i->context);
            pa_context_unref(i->context);
            i->context = NULL;
        }

        if (i->play_stream) {
            pa_stream_unref(i->play_stream);
            i->play_stream = NULL;
        }

        if (i->rec_stream) {
            pa_stream_unref(i->rec_stream);
            i->rec_stream = NULL;
        }

        if (i->app_fd >= 0) {
            LOAD_CLOSE_FUNC();
            _close(i->app_fd);
            i->app_fd = -1;
        }

        if (i->thread_fd >= 0) {
            LOAD_CLOSE_FUNC();
            _close(i->thread_fd);
            i->thread_fd = -1;
        }

        i->unusable = 1;
    }

    function_exit();

    debug(DEBUG_LEVEL_NORMAL, __FILE__": atfork_child() exit\n");
}

static void install_atfork(void) {
    pthread_atfork(atfork_prepare, atfork_parent, atfork_child);
}

static void stream_success_cb(pa_stream *s, int success, void *userdata) {
    fd_info *i = userdata;

    assert(s);
    assert(i);

    i->operation_success = success;
    pa_threaded_mainloop_signal(i->mainloop, 0);
}

static void context_success_cb(pa_context *c, int success, void *userdata) {
    fd_info *i = userdata;

    assert(c);
    assert(i);

    i->operation_success = success;
    pa_threaded_mainloop_signal(i->mainloop, 0);
}

static fd_info* fd_info_new(fd_info_type_t type, int *_errno) {
    fd_info *i;
    int sfds[2] = { -1, -1 };
    char name[64];
    static pthread_once_t install_atfork_once = PTHREAD_ONCE_INIT;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": fd_info_new()\n");

    signal(SIGPIPE, SIG_IGN); /* Yes, ugly as hell */

    pthread_once(&install_atfork_once, install_atfork);

    if (!(i = malloc(sizeof(fd_info)))) {
        *_errno = ENOMEM;
        goto fail;
    }

    i->app_fd = i->thread_fd = -1;
    i->type = type;

    i->mainloop = NULL;
    i->context = NULL;
    i->play_stream = NULL;
    i->rec_stream = NULL;
    i->play_precork = 0;
    i->rec_precork = 0;
    i->io_event = NULL;
    i->io_flags = 0;
    pthread_mutex_init(&i->mutex, NULL);
    i->ref = 1;
    i->buf = NULL;
    i->rec_offset = 0;
    i->unusable = 0;
    pa_cvolume_reset(&i->sink_volume, 2);
    pa_cvolume_reset(&i->source_volume, 2);
    i->volume_modify_count = 0;
    i->sink_index = (uint32_t) -1;
    i->source_index = (uint32_t) -1;
    i->optr_n_blocks = 0;
    PA_LLIST_INIT(fd_info, i);

    reset_params(i);

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sfds) < 0) {
        *_errno = errno;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": socket() failed: %s\n", strerror(errno));
        goto fail;
    }

    i->app_fd = sfds[0];
    i->thread_fd = sfds[1];

    if (!(i->mainloop = pa_threaded_mainloop_new())) {
        *_errno = EIO;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_threaded_mainloop_new() failed\n");
        goto fail;
    }

    if (!(i->context = pa_context_new(pa_threaded_mainloop_get_api(i->mainloop), client_name(name, sizeof(name))))) {
        *_errno = EIO;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_context_new() failed\n");
        goto fail;
    }

    pa_context_set_state_callback(i->context, context_state_cb, i);

    if (pa_context_connect(i->context, NULL, 0, NULL) < 0) {
        *_errno = ECONNREFUSED;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_context_connect() failed: %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    pa_threaded_mainloop_lock(i->mainloop);

    if (pa_threaded_mainloop_start(i->mainloop) < 0) {
        *_errno = EIO;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_threaded_mainloop_start() failed\n");
        goto unlock_and_fail;
    }

    /* Wait until the context is ready */
    pa_threaded_mainloop_wait(i->mainloop);

    if (pa_context_get_state(i->context) != PA_CONTEXT_READY) {
        *_errno = ECONNREFUSED;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_context_connect() failed: %s\n", pa_strerror(pa_context_errno(i->context)));
        goto unlock_and_fail;
    }

    pa_threaded_mainloop_unlock(i->mainloop);
    return i;

unlock_and_fail:

    pa_threaded_mainloop_unlock(i->mainloop);

fail:

    if (i)
        fd_info_unref(i);

    return NULL;
}

static void fd_info_add_to_list(fd_info *i) {
    assert(i);

    pthread_mutex_lock(&fd_infos_mutex);
    PA_LLIST_PREPEND(fd_info, fd_infos, i);
    pthread_mutex_unlock(&fd_infos_mutex);

    fd_info_ref(i);
}

static void fd_info_remove_from_list(fd_info *i) {
    assert(i);

    pthread_mutex_lock(&fd_infos_mutex);
    PA_LLIST_REMOVE(fd_info, fd_infos, i);
    pthread_mutex_unlock(&fd_infos_mutex);

    fd_info_unref(i);
}

static fd_info* fd_info_find(int fd) {
    fd_info *i;

    pthread_mutex_lock(&fd_infos_mutex);

    for (i = fd_infos; i; i = i->next)
        if (i->app_fd == fd && !i->unusable) {
            fd_info_ref(i);
            break;
        }

    pthread_mutex_unlock(&fd_infos_mutex);

    return i;
}

static void fix_metrics(fd_info *i) {
    size_t fs;
    char t[PA_SAMPLE_SPEC_SNPRINT_MAX];

    fs = pa_frame_size(&i->sample_spec);

    /* Don't fix things more than necessary */
    if ((i->fragment_size % fs) == 0 &&
        i->n_fragments >= 2 &&
        i->fragment_size > 0)
        return;

    i->fragment_size = (i->fragment_size/fs)*fs;

    /* Number of fragments set? */
    if (i->n_fragments < 2) {
        if (i->fragment_size > 0) {
            i->n_fragments = (unsigned) (pa_bytes_per_second(&i->sample_spec) / 2 / i->fragment_size);
            if (i->n_fragments < 2)
                i->n_fragments = 2;
        } else
            i->n_fragments = 12;
    }

    /* Fragment size set? */
    if (i->fragment_size <= 0) {
        i->fragment_size = pa_bytes_per_second(&i->sample_spec) / 2 / i->n_fragments;
        if (i->fragment_size < 1024)
            i->fragment_size = 1024;
    }

    debug(DEBUG_LEVEL_NORMAL, __FILE__": sample spec: %s\n", pa_sample_spec_snprint(t, sizeof(t), &i->sample_spec));
    debug(DEBUG_LEVEL_NORMAL, __FILE__": fixated metrics to %i fragments, %li bytes each.\n", i->n_fragments, (long)i->fragment_size);
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata) {
    fd_info *i = userdata;
    assert(s);

    if (i->io_event) {
        pa_mainloop_api *api;
        size_t n;

        api = pa_threaded_mainloop_get_api(i->mainloop);

        if (s == i->play_stream) {
            n = pa_stream_writable_size(i->play_stream);
            if (n == (size_t)-1) {
                debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_writable_size(): %s\n",
                    pa_strerror(pa_context_errno(i->context)));
            }

            if (n >= i->fragment_size)
                i->io_flags |= PA_IO_EVENT_INPUT;
            else
                i->io_flags &= ~PA_IO_EVENT_INPUT;
        }

        if (s == i->rec_stream) {
            n = pa_stream_readable_size(i->rec_stream);
            if (n == (size_t)-1) {
                debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_readable_size(): %s\n",
                    pa_strerror(pa_context_errno(i->context)));
            }

            if (n >= i->fragment_size)
                i->io_flags |= PA_IO_EVENT_OUTPUT;
            else
                i->io_flags &= ~PA_IO_EVENT_OUTPUT;
        }

        api->io_enable(i->io_event, i->io_flags);
    }
}

static void stream_latency_update_cb(pa_stream *s, void *userdata) {
    fd_info *i = userdata;
    assert(s);

    pa_threaded_mainloop_signal(i->mainloop, 0);
}

static void fd_info_shutdown(fd_info *i) {
    assert(i);

    if (i->io_event) {
        pa_mainloop_api *api;
        api = pa_threaded_mainloop_get_api(i->mainloop);
        api->io_free(i->io_event);
        i->io_event = NULL;
        i->io_flags = 0;
    }

    if (i->thread_fd >= 0) {
        close(i->thread_fd);
        i->thread_fd = -1;
    }
}

static int fd_info_copy_data(fd_info *i, int force) {
    size_t n;

    if (!i->play_stream && !i->rec_stream)
        return -1;

    if ((i->play_stream) && (pa_stream_get_state(i->play_stream) == PA_STREAM_READY)) {
        n = pa_stream_writable_size(i->play_stream);

        if (n == (size_t)-1) {
            debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_writable_size(): %s\n",
                pa_strerror(pa_context_errno(i->context)));
            return -1;
        }

        while (n >= i->fragment_size || force) {
            ssize_t r;

            if (!i->buf) {
                if (!(i->buf = malloc(i->fragment_size))) {
                    debug(DEBUG_LEVEL_NORMAL, __FILE__": malloc() failed.\n");
                    return -1;
                }
            }

            if ((r = read(i->thread_fd, i->buf, i->fragment_size)) <= 0) {

                if (errno == EAGAIN)
                    break;

                debug(DEBUG_LEVEL_NORMAL, __FILE__": read(): %s\n", r == 0 ? "EOF" : strerror(errno));
                return -1;
            }

            if (pa_stream_write(i->play_stream, i->buf, (size_t) r, free, 0LL, PA_SEEK_RELATIVE) < 0) {
                debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_write(): %s\n", pa_strerror(pa_context_errno(i->context)));
                return -1;
            }

            i->buf = NULL;

            assert(n >= (size_t) r);
            n -= (size_t) r;
        }

        if (n >= i->fragment_size)
            i->io_flags |= PA_IO_EVENT_INPUT;
        else
            i->io_flags &= ~PA_IO_EVENT_INPUT;
    }

    if ((i->rec_stream) && (pa_stream_get_state(i->rec_stream) == PA_STREAM_READY)) {
        n = pa_stream_readable_size(i->rec_stream);

        if (n == (size_t)-1) {
            debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_readable_size(): %s\n",
                pa_strerror(pa_context_errno(i->context)));
            return -1;
        }

        while (n >= i->fragment_size || force) {
            ssize_t r;
            const void *data;
            const char *buf;
            size_t len;

            if (pa_stream_peek(i->rec_stream, &data, &len) < 0) {
                debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_peek(): %s\n", pa_strerror(pa_context_errno(i->context)));
                return -1;
            }

            if (len <= 0)
                break;

            if (!data) {
                /* Maybe we should generate silence here, but I'm lazy and
                 * I'll just skip any holes in the stream. */
                if (pa_stream_drop(i->rec_stream) < 0) {
                    debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_drop(): %s\n", pa_strerror(pa_context_errno(i->context)));
                    return -1;
                }

                assert(n >= len);
                n -= len;
                continue;
            }

            buf = (const char*)data + i->rec_offset;

            if ((r = write(i->thread_fd, buf, len - i->rec_offset)) <= 0) {

                if (errno == EAGAIN)
                    break;

                debug(DEBUG_LEVEL_NORMAL, __FILE__": write(): %s\n", strerror(errno));
                return -1;
            }

            assert((size_t)r <= len - i->rec_offset);
            i->rec_offset += (size_t) r;

            if (i->rec_offset == len) {
                if (pa_stream_drop(i->rec_stream) < 0) {
                    debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_drop(): %s\n", pa_strerror(pa_context_errno(i->context)));
                    return -1;
                }
                i->rec_offset = 0;
            }

            assert(n >= (size_t) r);
            n -= (size_t) r;
        }

        if (n >= i->fragment_size)
            i->io_flags |= PA_IO_EVENT_OUTPUT;
        else
            i->io_flags &= ~PA_IO_EVENT_OUTPUT;
    }

    if (i->io_event) {
        pa_mainloop_api *api;

        api = pa_threaded_mainloop_get_api(i->mainloop);
        api->io_enable(i->io_event, i->io_flags);
    }

    /* So, we emptied the socket now, let's tell dsp_empty_socket()
     * about this */
    pa_threaded_mainloop_signal(i->mainloop, 0);

    return 0;
}

static void stream_state_cb(pa_stream *s, void * userdata) {
    fd_info *i = userdata;
    assert(s);

    switch (pa_stream_get_state(s)) {

        case PA_STREAM_READY:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": stream established.\n");
            break;

        case PA_STREAM_FAILED:
            if (s == i->play_stream) {
                debug(DEBUG_LEVEL_NORMAL,
                    __FILE__": pa_stream_connect_playback() failed: %s\n",
                    pa_strerror(pa_context_errno(i->context)));
                pa_stream_unref(i->play_stream);
                i->play_stream = NULL;
            } else if (s == i->rec_stream) {
                debug(DEBUG_LEVEL_NORMAL,
                    __FILE__": pa_stream_connect_record() failed: %s\n",
                    pa_strerror(pa_context_errno(i->context)));
                pa_stream_unref(i->rec_stream);
                i->rec_stream = NULL;
            }
            fd_info_shutdown(i);
            break;

        case PA_STREAM_TERMINATED:
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

static int create_playback_stream(fd_info *i) {
    pa_buffer_attr attr;
    int n, flags;

    assert(i);

    fix_metrics(i);

    if (!(i->play_stream = pa_stream_new(i->context, stream_name(), &i->sample_spec, NULL))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_new() failed: %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    pa_stream_set_state_callback(i->play_stream, stream_state_cb, i);
    pa_stream_set_write_callback(i->play_stream, stream_request_cb, i);
    pa_stream_set_latency_update_callback(i->play_stream, stream_latency_update_cb, i);

    memset(&attr, 0, sizeof(attr));
    attr.maxlength = (uint32_t) (i->fragment_size * (i->n_fragments+1));
    attr.tlength = (uint32_t) (i->fragment_size * i->n_fragments);
    attr.prebuf = (uint32_t) i->fragment_size;
    attr.minreq = (uint32_t) i->fragment_size;

    flags = PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE|PA_STREAM_EARLY_REQUESTS;
    if (i->play_precork) {
        flags |= PA_STREAM_START_CORKED;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": creating stream corked\n");
    }
    if (pa_stream_connect_playback(i->play_stream, NULL, &attr, flags, NULL, NULL) < 0) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_connect_playback() failed: %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    n = (int) i->fragment_size;
    setsockopt(i->app_fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
    n = (int) i->fragment_size;
    setsockopt(i->thread_fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));

    return 0;

fail:
    return -1;
}

static int create_record_stream(fd_info *i) {
    pa_buffer_attr attr;
    int n, flags;

    assert(i);

    fix_metrics(i);

    if (!(i->rec_stream = pa_stream_new(i->context, stream_name(), &i->sample_spec, NULL))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_new() failed: %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    pa_stream_set_state_callback(i->rec_stream, stream_state_cb, i);
    pa_stream_set_read_callback(i->rec_stream, stream_request_cb, i);
    pa_stream_set_latency_update_callback(i->rec_stream, stream_latency_update_cb, i);

    memset(&attr, 0, sizeof(attr));
    attr.maxlength = (uint32_t) (i->fragment_size * (i->n_fragments+1));
    attr.fragsize = (uint32_t) i->fragment_size;

    flags = PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE;
    if (i->rec_precork) {
        flags |= PA_STREAM_START_CORKED;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": creating stream corked\n");
    }
    if (pa_stream_connect_record(i->rec_stream, NULL, &attr, flags) < 0) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_connect_record() failed: %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    n = (int) i->fragment_size;
    setsockopt(i->app_fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
    n = (int) i->fragment_size;
    setsockopt(i->thread_fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));

    return 0;

fail:
    return -1;
}

static void free_streams(fd_info *i) {
    assert(i);

    if (i->play_stream) {
        pa_stream_disconnect(i->play_stream);
        pa_stream_unref(i->play_stream);
        i->play_stream = NULL;
        i->io_flags |= PA_IO_EVENT_INPUT;
    }

    if (i->rec_stream) {
        pa_stream_disconnect(i->rec_stream);
        pa_stream_unref(i->rec_stream);
        i->rec_stream = NULL;
        i->io_flags |= PA_IO_EVENT_OUTPUT;
    }

    if (i->io_event) {
        pa_mainloop_api *api;

        api = pa_threaded_mainloop_get_api(i->mainloop);
        api->io_enable(i->io_event, i->io_flags);
    }
}

static void io_event_cb(pa_mainloop_api *api, pa_io_event *e, int fd, pa_io_event_flags_t flags, void *userdata) {
    fd_info *i = userdata;

    pa_threaded_mainloop_signal(i->mainloop, 0);

    if (flags & PA_IO_EVENT_INPUT) {

        if (!i->play_stream) {
            if (create_playback_stream(i) < 0)
                goto fail;
        } else {
            if (fd_info_copy_data(i, 0) < 0)
                goto fail;
        }

    } else if (flags & PA_IO_EVENT_OUTPUT) {

        if (!i->rec_stream) {
            if (create_record_stream(i) < 0)
                goto fail;
        } else {
            if (fd_info_copy_data(i, 0) < 0)
                goto fail;
        }

    } else if (flags & (PA_IO_EVENT_HANGUP|PA_IO_EVENT_ERROR))
        goto fail;

    return;

fail:
    /* We can't do anything better than removing the event source */
    fd_info_shutdown(i);
}

static int dsp_open(int flags, int *_errno) {
    fd_info *i;
    pa_mainloop_api *api;
    int ret;
    int f;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": dsp_open()\n");

    if (!(i = fd_info_new(FD_INFO_STREAM, _errno)))
        return -1;

    if ((flags & O_NONBLOCK) == O_NONBLOCK) {
        if ((f = fcntl(i->app_fd, F_GETFL)) >= 0)
            fcntl(i->app_fd, F_SETFL, f|O_NONBLOCK);
    }
    if ((f = fcntl(i->thread_fd, F_GETFL)) >= 0)
        fcntl(i->thread_fd, F_SETFL, f|O_NONBLOCK);

    fcntl(i->app_fd, F_SETFD, FD_CLOEXEC);
    fcntl(i->thread_fd, F_SETFD, FD_CLOEXEC);

    pa_threaded_mainloop_lock(i->mainloop);
    api = pa_threaded_mainloop_get_api(i->mainloop);

    switch (flags & O_ACCMODE) {
    case O_RDONLY:
        i->io_flags = PA_IO_EVENT_OUTPUT;
        shutdown(i->thread_fd, SHUT_RD);
        shutdown(i->app_fd, SHUT_WR);
        break;
    case O_WRONLY:
        i->io_flags = PA_IO_EVENT_INPUT;
        shutdown(i->thread_fd, SHUT_WR);
        shutdown(i->app_fd, SHUT_RD);
        break;
    case O_RDWR:
        i->io_flags = PA_IO_EVENT_INPUT | PA_IO_EVENT_OUTPUT;
        break;
    default:
        return -1;
    }

    if (!(i->io_event = api->io_new(api, i->thread_fd, i->io_flags, io_event_cb, i)))
        goto fail;

    pa_threaded_mainloop_unlock(i->mainloop);

    debug(DEBUG_LEVEL_NORMAL, __FILE__": dsp_open() succeeded, fd=%i\n", i->app_fd);

    fd_info_add_to_list(i);
    ret = i->app_fd;
    fd_info_unref(i);

    return ret;

fail:
    pa_threaded_mainloop_unlock(i->mainloop);

    if (i)
        fd_info_unref(i);

    *_errno = EIO;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": dsp_open() failed\n");

    return -1;
}

static void sink_info_cb(pa_context *context, const pa_sink_info *si, int eol, void *userdata) {
    fd_info *i = userdata;

    if (eol < 0) {
        i->operation_success = 0;
        pa_threaded_mainloop_signal(i->mainloop, 0);
        return;
    }

    if (eol)
        return;

    if (!pa_cvolume_equal(&i->sink_volume, &si->volume))
        i->volume_modify_count++;

    i->sink_volume = si->volume;
    i->sink_index = si->index;

    i->operation_success = 1;
    pa_threaded_mainloop_signal(i->mainloop, 0);
}

static void source_info_cb(pa_context *context, const pa_source_info *si, int eol, void *userdata) {
    fd_info *i = userdata;

    if (eol < 0) {
        i->operation_success = 0;
        pa_threaded_mainloop_signal(i->mainloop, 0);
        return;
    }

    if (eol)
        return;

    if (!pa_cvolume_equal(&i->source_volume, &si->volume))
        i->volume_modify_count++;

    i->source_volume = si->volume;
    i->source_index = si->index;

    i->operation_success = 1;
    pa_threaded_mainloop_signal(i->mainloop, 0);
}

static void subscribe_cb(pa_context *context, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    fd_info *i = userdata;
    pa_operation *o = NULL;

    if (i->sink_index != idx)
        return;

    if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) != PA_SUBSCRIPTION_EVENT_CHANGE)
        return;

    if (!(o = pa_context_get_sink_info_by_index(i->context, i->sink_index, sink_info_cb, i))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": Failed to get sink info: %s", pa_strerror(pa_context_errno(i->context)));
        return;
    }

    pa_operation_unref(o);
}

static int mixer_open(int flags, int *_errno) {
    fd_info *i;
    pa_operation *o = NULL;
    int ret;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": mixer_open()\n");

    if (!(i = fd_info_new(FD_INFO_MIXER, _errno)))
        return -1;

    pa_threaded_mainloop_lock(i->mainloop);

    pa_context_set_subscribe_callback(i->context, subscribe_cb, i);

    if (!(o = pa_context_subscribe(i->context, PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE, context_success_cb, i))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": Failed to subscribe to events: %s", pa_strerror(pa_context_errno(i->context)));
        *_errno = EIO;
        goto fail;
    }

    i->operation_success = 0;
    while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
        pa_threaded_mainloop_wait(i->mainloop);
        CONTEXT_CHECK_DEAD_GOTO(i, fail);
    }

    pa_operation_unref(o);
    o = NULL;

    if (!i->operation_success) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__":Failed to subscribe to events: %s", pa_strerror(pa_context_errno(i->context)));
        *_errno = EIO;
        goto fail;
    }

    /* Get sink info */

    if (!(o = pa_context_get_sink_info_by_name(i->context, NULL, sink_info_cb, i))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": Failed to get sink info: %s", pa_strerror(pa_context_errno(i->context)));
        *_errno = EIO;
        goto fail;
    }

    i->operation_success = 0;
    while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
        pa_threaded_mainloop_wait(i->mainloop);
        CONTEXT_CHECK_DEAD_GOTO(i, fail);
    }

    pa_operation_unref(o);
    o = NULL;

    if (!i->operation_success) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": Failed to get sink info: %s", pa_strerror(pa_context_errno(i->context)));
        *_errno = EIO;
        goto fail;
    }

    /* Get source info */

    if (!(o = pa_context_get_source_info_by_name(i->context, NULL, source_info_cb, i))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": Failed to get source info: %s", pa_strerror(pa_context_errno(i->context)));
        *_errno = EIO;
        goto fail;
    }

    i->operation_success = 0;
    while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
        pa_threaded_mainloop_wait(i->mainloop);
        CONTEXT_CHECK_DEAD_GOTO(i, fail);
    }

    pa_operation_unref(o);
    o = NULL;

    if (!i->operation_success) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": Failed to get source info: %s", pa_strerror(pa_context_errno(i->context)));
        *_errno = EIO;
        goto fail;
    }

    pa_threaded_mainloop_unlock(i->mainloop);

    debug(DEBUG_LEVEL_NORMAL, __FILE__": mixer_open() succeeded, fd=%i\n", i->app_fd);

    fd_info_add_to_list(i);
    ret = i->app_fd;
    fd_info_unref(i);

    return ret;

fail:
    if (o)
        pa_operation_unref(o);

    pa_threaded_mainloop_unlock(i->mainloop);

    if (i)
        fd_info_unref(i);

    *_errno = EIO;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": mixer_open() failed\n");

    return -1;
}

static int sndstat_open(int flags, int *_errno) {
    static const char sndstat[] =
        "Sound Driver:3.8.1a-980706 (PulseAudio Virtual OSS)\n"
        "Kernel: POSIX\n"
        "Config options: 0\n"
        "\n"
        "Installed drivers:\n"
        "Type 255: PulseAudio Virtual OSS\n"
        "\n"
        "Card config:\n"
        "PulseAudio Virtual OSS\n"
        "\n"
        "Audio devices:\n"
        "0: PulseAudio Virtual OSS\n"
        "\n"
        "Synth devices: NOT ENABLED IN CONFIG\n"
        "\n"
        "Midi devices:\n"
        "\n"
        "Timers:\n"
        "\n"
        "Mixers:\n"
        "0: PulseAudio Virtual OSS\n";

    char *fn;
    mode_t u;
    int fd = -1;
    int e;

    fn = pa_sprintf_malloc("%s" PA_PATH_SEP "padsp-sndstat-XXXXXX", pa_get_temp_dir());

    debug(DEBUG_LEVEL_NORMAL, __FILE__": sndstat_open()\n");

    if (flags != O_RDONLY
#ifdef O_LARGEFILE
        && flags != (O_RDONLY|O_LARGEFILE)
#endif
       ) {
        *_errno = EACCES;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": bad access!\n");
        goto fail;
    }

    u = umask(0077);
    fd = mkstemp(fn);
    e = errno;
    umask(u);

    if (fd < 0) {
        *_errno = e;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": mkstemp() failed: %s\n", strerror(errno));
        goto fail;
    }

    unlink(fn);
    pa_xfree(fn);
    fn = NULL;

    if (write(fd, sndstat, sizeof(sndstat) -1) != sizeof(sndstat)-1) {
        *_errno = errno;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": write() failed: %s\n", strerror(errno));
        goto fail;
    }

    if (lseek(fd, SEEK_SET, 0) < 0) {
        *_errno = errno;
        debug(DEBUG_LEVEL_NORMAL, __FILE__": lseek() failed: %s\n", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    pa_xfree(fn);
    if (fd >= 0)
        close(fd);
    return -1;
}

static int real_open(const char *filename, int flags, mode_t mode) {
    int r, _errno = 0;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": open(%s)\n", filename?filename:"NULL");

    if (!function_enter()) {
        LOAD_OPEN_FUNC();
        return _open(filename, flags, mode);
    }

    if (filename && dsp_cloak_enable() && (pa_streq(filename, "/dev/dsp") || pa_streq(filename, "/dev/adsp") || pa_streq(filename, "/dev/audio")))
        r = dsp_open(flags, &_errno);
    else if (filename && mixer_cloak_enable() && pa_streq(filename, "/dev/mixer"))
        r = mixer_open(flags, &_errno);
    else if (filename && sndstat_cloak_enable() && pa_streq(filename, "/dev/sndstat"))
        r = sndstat_open(flags, &_errno);
    else {
        function_exit();
        LOAD_OPEN_FUNC();
        return _open(filename, flags, mode);
    }

    function_exit();

    if (_errno)
        errno = _errno;

    return r;
}

int open(const char *filename, int flags, ...) {
    va_list args;
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_start(args, flags);
        if (sizeof(mode_t) < sizeof(int))
            mode = (mode_t) va_arg(args, int);
        else
            mode = va_arg(args, mode_t);
        va_end(args);
    }

    return real_open(filename, flags, mode);
}

static bool is_audio_device_node(const char *path) {
    return
        pa_streq(path, "/dev/dsp") ||
        pa_streq(path, "/dev/adsp") ||
        pa_streq(path, "/dev/audio") ||
        pa_streq(path, "/dev/sndstat") ||
        pa_streq(path, "/dev/mixer");
}

int __open_2(const char *filename, int flags) {
    debug(DEBUG_LEVEL_VERBOSE, __FILE__": __open_2(%s)\n", filename?filename:"NULL");

    if ((flags & O_CREAT) ||
        !filename ||
        !is_audio_device_node(filename)) {
        LOAD___OPEN_2_FUNC();
        return ___open_2(filename, flags);
    }
    return real_open(filename, flags, 0);
}

static int mixer_ioctl(fd_info *i, unsigned long request, void*argp, int *_errno) {
    int ret = -1;

    switch (request) {
        case SOUND_MIXER_READ_DEVMASK :
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_READ_DEVMASK\n");

            *(int*) argp = SOUND_MASK_PCM | SOUND_MASK_IGAIN;
            break;

        case SOUND_MIXER_READ_RECMASK :
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_READ_RECMASK\n");

            *(int*) argp = SOUND_MASK_IGAIN;
            break;

        case SOUND_MIXER_READ_STEREODEVS:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_READ_STEREODEVS\n");

            pa_threaded_mainloop_lock(i->mainloop);
            *(int*) argp = 0;
            if (i->sink_volume.channels > 1)
                *(int*) argp |= SOUND_MASK_PCM;
            if (i->source_volume.channels > 1)
                *(int*) argp |= SOUND_MASK_IGAIN;
            pa_threaded_mainloop_unlock(i->mainloop);

            break;

        case SOUND_MIXER_READ_RECSRC:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_READ_RECSRC\n");

            *(int*) argp = SOUND_MASK_IGAIN;
            break;

        case SOUND_MIXER_WRITE_RECSRC:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_WRITE_RECSRC\n");
            break;

        case SOUND_MIXER_READ_CAPS:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_READ_CAPS\n");

            *(int*) argp = 0;
            break;

        case SOUND_MIXER_READ_PCM:
        case SOUND_MIXER_READ_IGAIN: {
            pa_cvolume *v;

            if (request == SOUND_MIXER_READ_PCM)
                debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_READ_PCM\n");
            else
                debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_READ_IGAIN\n");

            pa_threaded_mainloop_lock(i->mainloop);

            if (request == SOUND_MIXER_READ_PCM)
                v = &i->sink_volume;
            else
                v = &i->source_volume;

            *(int*) argp =
                ((v->values[0]*100/PA_VOLUME_NORM)) |
                ((v->values[v->channels > 1 ? 1 : 0]*100/PA_VOLUME_NORM) << 8);

            pa_threaded_mainloop_unlock(i->mainloop);

            break;
        }

        case SOUND_MIXER_WRITE_PCM:
        case SOUND_MIXER_WRITE_IGAIN: {
            pa_cvolume v, *pv;

            if (request == SOUND_MIXER_WRITE_PCM)
                debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_WRITE_PCM\n");
            else
                debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_WRITE_IGAIN\n");

            pa_threaded_mainloop_lock(i->mainloop);

            if (request == SOUND_MIXER_WRITE_PCM) {
                v = i->sink_volume;
                pv = &i->sink_volume;
            } else {
                v = i->source_volume;
                pv = &i->source_volume;
            }

            pv->values[0] = ((*(int*) argp & 0xFF)*PA_VOLUME_NORM)/100;
            pv->values[1] = ((*(int*) argp >> 8)*PA_VOLUME_NORM)/100;

            if (!pa_cvolume_equal(pv, &v)) {
                pa_operation *o;

                if (request == SOUND_MIXER_WRITE_PCM)
                    o = pa_context_set_sink_volume_by_index(i->context, i->sink_index, pv, context_success_cb, i);
                else
                    o = pa_context_set_source_volume_by_index(i->context, i->source_index, pv, context_success_cb, i);

                if (!o)
                    debug(DEBUG_LEVEL_NORMAL, __FILE__":Failed set volume: %s", pa_strerror(pa_context_errno(i->context)));
                else {

                    i->operation_success = 0;
                    while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
                        CONTEXT_CHECK_DEAD_GOTO(i, exit_loop);

                        pa_threaded_mainloop_wait(i->mainloop);
                    }
                exit_loop:

                    if (!i->operation_success)
                        debug(DEBUG_LEVEL_NORMAL, __FILE__": Failed to set volume: %s\n", pa_strerror(pa_context_errno(i->context)));

                    pa_operation_unref(o);
                }

                /* We don't wait for completion here */
                i->volume_modify_count++;
            }

            pa_threaded_mainloop_unlock(i->mainloop);

            break;
        }

        case SOUND_MIXER_INFO: {
            mixer_info *mi = argp;

            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_MIXER_INFO\n");

            memset(mi, 0, sizeof(mixer_info));
            strncpy(mi->id, "PULSEAUDIO", sizeof(mi->id));
            strncpy(mi->name, "PulseAudio Virtual OSS", sizeof(mi->name));
            pa_threaded_mainloop_lock(i->mainloop);
            mi->modify_counter = i->volume_modify_count;
            pa_threaded_mainloop_unlock(i->mainloop);
            break;
        }

        default:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": unknown ioctl 0x%08lx\n", request);

            *_errno = EINVAL;
            goto fail;
    }

    ret = 0;

fail:

    return ret;
}

static int map_format(int *fmt, pa_sample_spec *ss) {

    switch (*fmt) {
        case AFMT_MU_LAW:
            ss->format = PA_SAMPLE_ULAW;
            break;

        case AFMT_A_LAW:
            ss->format = PA_SAMPLE_ALAW;
            break;

        case AFMT_S8:
            *fmt = AFMT_U8;
            /* fall through */
        case AFMT_U8:
            ss->format = PA_SAMPLE_U8;
            break;

        case AFMT_U16_BE:
            *fmt = AFMT_S16_BE;
            /* fall through */
        case AFMT_S16_BE:
            ss->format = PA_SAMPLE_S16BE;
            break;

        case AFMT_U16_LE:
            *fmt = AFMT_S16_LE;
            /* fall through */
        case AFMT_S16_LE:
            ss->format = PA_SAMPLE_S16LE;
            break;

        default:
            ss->format = PA_SAMPLE_S16NE;
            *fmt = AFMT_S16_NE;
            break;
    }

    return 0;
}

static int map_format_back(pa_sample_format_t format) {
    switch (format) {
        case PA_SAMPLE_S16LE: return AFMT_S16_LE;
        case PA_SAMPLE_S16BE: return AFMT_S16_BE;
        case PA_SAMPLE_ULAW: return AFMT_MU_LAW;
        case PA_SAMPLE_ALAW: return AFMT_A_LAW;
        case PA_SAMPLE_U8: return AFMT_U8;
        default:
            abort();
    }
}

static int dsp_flush_fd(int fd) {
#ifdef SIOCINQ
    int l;

    if (ioctl(fd, SIOCINQ, &l) < 0) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": SIOCINQ: %s\n", strerror(errno));
        return -1;
    }

    while (l > 0) {
        char buf[1024];
        size_t k;

        k = (size_t) l > sizeof(buf) ? sizeof(buf) : (size_t) l;
        if (read(fd, buf, k) < 0)
            debug(DEBUG_LEVEL_NORMAL, __FILE__": read(): %s\n", strerror(errno));
        l -= k;
    }

    return 0;
#else
# warning "Your platform does not support SIOCINQ, something might not work as intended."
    return 0;
#endif
}

static int dsp_flush_socket(fd_info *i) {
    int res = 0;

    if ((i->thread_fd < 0) && (i->app_fd < 0))
        return -1;

    if (i->thread_fd >= 0)
        res = dsp_flush_fd(i->thread_fd);

    if (res < 0)
        return res;

    if (i->app_fd >= 0)
        res = dsp_flush_fd(i->app_fd);

    if (res < 0)
        return res;

    return 0;
}

static int dsp_empty_socket(fd_info *i) {
#ifdef SIOCINQ
    int ret = -1;

    /* Empty the socket */
    for (;;) {
        int l;

        if (i->thread_fd < 0)
            break;

        if (ioctl(i->thread_fd, SIOCINQ, &l) < 0) {
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SIOCINQ: %s\n", strerror(errno));
            break;
        }

        if (!l) {
            ret = 0;
            break;
        }

        pa_threaded_mainloop_wait(i->mainloop);
    }

    return ret;
#else
# warning "Your platform does not support SIOCINQ, something might not work as intended."
    return 0;
#endif
}

static int dsp_drain(fd_info *i) {
    pa_operation *o = NULL;
    int r = -1;

    if (!i->mainloop)
        return 0;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": Draining.\n");

    pa_threaded_mainloop_lock(i->mainloop);

    if (dsp_empty_socket(i) < 0)
        goto fail;

    if (!i->play_stream)
        goto fail;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": Really draining.\n");

    if (!(o = pa_stream_drain(i->play_stream, stream_success_cb, i))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_drain(): %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    i->operation_success = 0;
    while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
        PLAYBACK_STREAM_CHECK_DEAD_GOTO(i, fail);

        pa_threaded_mainloop_wait(i->mainloop);
    }

    if (!i->operation_success) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_drain() 2: %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    r = 0;

fail:

    if (o)
        pa_operation_unref(o);

    pa_threaded_mainloop_unlock(i->mainloop);

    return r;
}

static int dsp_trigger(fd_info *i) {
    pa_operation *o = NULL;
    int r = -1;

    if (!i->play_stream)
        return 0;

    pa_threaded_mainloop_lock(i->mainloop);

    if (dsp_empty_socket(i) < 0)
        goto fail;

    debug(DEBUG_LEVEL_NORMAL, __FILE__": Triggering.\n");

    if (!(o = pa_stream_trigger(i->play_stream, stream_success_cb, i))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_trigger(): %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    i->operation_success = 0;
    while (!pa_operation_get_state(o) != PA_OPERATION_DONE) {
        PLAYBACK_STREAM_CHECK_DEAD_GOTO(i, fail);

        pa_threaded_mainloop_wait(i->mainloop);
    }

    if (!i->operation_success) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_trigger(): %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    r = 0;

fail:

    if (o)
        pa_operation_unref(o);

    pa_threaded_mainloop_unlock(i->mainloop);

    return r;
}

static int dsp_cork(fd_info *i, pa_stream *s, int b) {
    pa_operation *o = NULL;
    int r = -1;

    pa_threaded_mainloop_lock(i->mainloop);

    if (!(o = pa_stream_cork(s, b, stream_success_cb, i))) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_cork(): %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    i->operation_success = 0;
    while (!pa_operation_get_state(o) != PA_OPERATION_DONE) {
        if (s == i->play_stream)
            PLAYBACK_STREAM_CHECK_DEAD_GOTO(i, fail);
        else if (s == i->rec_stream)
            RECORD_STREAM_CHECK_DEAD_GOTO(i, fail);

        pa_threaded_mainloop_wait(i->mainloop);
    }

    if (!i->operation_success) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_cork(): %s\n", pa_strerror(pa_context_errno(i->context)));
        goto fail;
    }

    r = 0;

fail:

    if (o)
        pa_operation_unref(o);

    pa_threaded_mainloop_unlock(i->mainloop);

    return r;
}

static int dsp_ioctl(fd_info *i, unsigned long request, void*argp, int *_errno) {
    int ret = -1;

    if (i->thread_fd == -1) {
        /*
         * We've encountered some fatal error and are just waiting
         * for a close.
         */
        debug(DEBUG_LEVEL_NORMAL, __FILE__": got ioctl 0x%08lx in fatal error state\n", request);
        *_errno = EIO;
        return -1;
    }

    switch (request) {
        case SNDCTL_DSP_SETFMT: {
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_SETFMT: %i\n", *(int*) argp);

            pa_threaded_mainloop_lock(i->mainloop);

            if (*(int*) argp == AFMT_QUERY)
                *(int*) argp = map_format_back(i->sample_spec.format);
            else {
                map_format((int*) argp, &i->sample_spec);
                free_streams(i);
            }

            pa_threaded_mainloop_unlock(i->mainloop);
            break;
        }

        case SNDCTL_DSP_SPEED: {
            pa_sample_spec ss;
            int valid;
            char t[256];

            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_SPEED: %i\n", *(int*) argp);

            pa_threaded_mainloop_lock(i->mainloop);

            ss = i->sample_spec;
            ss.rate = *(int*) argp;

            if ((valid = pa_sample_spec_valid(&ss))) {
                i->sample_spec = ss;
                free_streams(i);
            }

            debug(DEBUG_LEVEL_NORMAL, __FILE__": ss: %s\n", pa_sample_spec_snprint(t, sizeof(t), &i->sample_spec));

            pa_threaded_mainloop_unlock(i->mainloop);

            if (!valid) {
                *_errno = EINVAL;
                goto fail;
            }

            break;
        }

        case SNDCTL_DSP_STEREO:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_STEREO: %i\n", *(int*) argp);

            pa_threaded_mainloop_lock(i->mainloop);

            i->sample_spec.channels = *(int*) argp ? 2 : 1;
            free_streams(i);

            pa_threaded_mainloop_unlock(i->mainloop);
            return 0;

        case SNDCTL_DSP_CHANNELS: {
            pa_sample_spec ss;
            int valid;

            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_CHANNELS: %i\n", *(int*) argp);

            pa_threaded_mainloop_lock(i->mainloop);

            ss = i->sample_spec;
            ss.channels = *(int*) argp;

            if ((valid = pa_sample_spec_valid(&ss))) {
                i->sample_spec = ss;
                free_streams(i);
            }

            pa_threaded_mainloop_unlock(i->mainloop);

            if (!valid) {
                *_errno = EINVAL;
                goto fail;
            }

            break;
        }

        case SNDCTL_DSP_GETBLKSIZE:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_GETBLKSIZE\n");

            pa_threaded_mainloop_lock(i->mainloop);

            fix_metrics(i);
            *(int*) argp = i->fragment_size;

            pa_threaded_mainloop_unlock(i->mainloop);

            break;

        case SNDCTL_DSP_SETFRAGMENT:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_SETFRAGMENT: 0x%08x\n", *(int*) argp);

            pa_threaded_mainloop_lock(i->mainloop);

            i->fragment_size = 1 << ((*(int*) argp) & 31);
            i->n_fragments = (*(int*) argp) >> 16;

            /* 0x7FFF means that we can set whatever we like */
            if (i->n_fragments == 0x7FFF)
                i->n_fragments = 12;

            free_streams(i);

            pa_threaded_mainloop_unlock(i->mainloop);

            break;

        case SNDCTL_DSP_GETCAPS:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_CAPS\n");

            *(int*) argp = DSP_CAP_DUPLEX | DSP_CAP_TRIGGER
#ifdef DSP_CAP_MULTI
              | DSP_CAP_MULTI
#endif
              ;
            break;

        case SNDCTL_DSP_GETODELAY: {
            int l;

            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_GETODELAY\n");

            pa_threaded_mainloop_lock(i->mainloop);

            *(int*) argp = 0;

            for (;;) {
                pa_usec_t usec;

                PLAYBACK_STREAM_CHECK_DEAD_GOTO(i, exit_loop);

                if (pa_stream_get_latency(i->play_stream, &usec, NULL) >= 0) {
                    *(int*) argp = pa_usec_to_bytes(usec, &i->sample_spec);
                    break;
                }

                if (pa_context_errno(i->context) != PA_ERR_NODATA) {
                    debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_get_latency(): %s\n", pa_strerror(pa_context_errno(i->context)));
                    break;
                }

                pa_threaded_mainloop_wait(i->mainloop);
            }

        exit_loop:

#ifdef SIOCINQ
            if (ioctl(i->thread_fd, SIOCINQ, &l) < 0)
                debug(DEBUG_LEVEL_NORMAL, __FILE__": SIOCINQ failed: %s\n", strerror(errno));
            else
                *(int*) argp += l;
#else
# warning "Your platform does not support SIOCINQ, something might not work as intended."
#endif

            pa_threaded_mainloop_unlock(i->mainloop);

            debug(DEBUG_LEVEL_NORMAL, __FILE__": ODELAY: %i\n", *(int*) argp);

            break;
        }

        case SNDCTL_DSP_RESET: {
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_RESET\n");

            pa_threaded_mainloop_lock(i->mainloop);

            free_streams(i);
            dsp_flush_socket(i);

            i->optr_n_blocks = 0;

            pa_threaded_mainloop_unlock(i->mainloop);
            break;
        }

        case SNDCTL_DSP_GETFMTS: {
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_GETFMTS\n");

            *(int*) argp = AFMT_MU_LAW|AFMT_A_LAW|AFMT_U8|AFMT_S16_LE|AFMT_S16_BE;
            break;
        }

        case SNDCTL_DSP_POST:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_POST\n");

            if (dsp_trigger(i) < 0)
                *_errno = EIO;
            break;

        case SNDCTL_DSP_GETTRIGGER:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_GETTRIGGER\n");

            *(int*) argp = 0;
            if (!i->play_precork)
                *(int*) argp |= PCM_ENABLE_OUTPUT;
            if (!i->rec_precork)
                *(int*) argp |= PCM_ENABLE_INPUT;

            break;

        case SNDCTL_DSP_SETTRIGGER:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_SETTRIGGER: 0x%08x\n", *(int*) argp);

            if (!i->io_event) {
                *_errno = EIO;
                break;
            }

            i->play_precork = !((*(int*) argp) & PCM_ENABLE_OUTPUT);

            if (i->play_stream) {
                if (dsp_cork(i, i->play_stream, !((*(int*) argp) & PCM_ENABLE_OUTPUT)) < 0)
                    *_errno = EIO;
                if (dsp_trigger(i) < 0)
                    *_errno = EIO;
            }

            i->rec_precork = !((*(int*) argp) & PCM_ENABLE_INPUT);

            if (i->rec_stream) {
                if (dsp_cork(i, i->rec_stream, !((*(int*) argp) & PCM_ENABLE_INPUT)) < 0)
                    *_errno = EIO;
            }

            break;

        case SNDCTL_DSP_SYNC:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_SYNC\n");

            if (dsp_drain(i) < 0)
                *_errno = EIO;

            break;

        case SNDCTL_DSP_GETOSPACE:
        case SNDCTL_DSP_GETISPACE: {
            audio_buf_info *bi = (audio_buf_info*) argp;
            int l = 0;
            size_t k = 0;

            if (request == SNDCTL_DSP_GETOSPACE)
                debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_GETOSPACE\n");
            else
                debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_GETISPACE\n");

            pa_threaded_mainloop_lock(i->mainloop);

            fix_metrics(i);

            if (request == SNDCTL_DSP_GETOSPACE) {
                if (i->play_stream) {
                    if ((k = pa_stream_writable_size(i->play_stream)) == (size_t) -1)
                        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_writable_size(): %s\n", pa_strerror(pa_context_errno(i->context)));
                } else
                    k = i->fragment_size * i->n_fragments;

#ifdef SIOCINQ
                if (ioctl(i->thread_fd, SIOCINQ, &l) < 0) {
                    debug(DEBUG_LEVEL_NORMAL, __FILE__": SIOCINQ failed: %s\n", strerror(errno));
                    l = 0;
                }
#else
# warning "Your platform does not dsp_flush_fd, something might not work as intended."
#endif

                bi->bytes = k > (size_t) l ? k - l : 0;
            } else {
                if (i->rec_stream) {
                    if ((k = pa_stream_readable_size(i->rec_stream)) == (size_t) -1)
                        debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_readable_size(): %s\n", pa_strerror(pa_context_errno(i->context)));
                } else
                    k = 0;

#ifdef SIOCINQ
                if (ioctl(i->app_fd, SIOCINQ, &l) < 0) {
                    debug(DEBUG_LEVEL_NORMAL, __FILE__": SIOCINQ failed: %s\n", strerror(errno));
                    l = 0;
                }
#else
# warning "Your platform does not dsp_flush_fd, something might not work as intended."
#endif
                bi->bytes = k + l;
            }

            bi->fragsize = i->fragment_size;
            bi->fragstotal = i->n_fragments;
            bi->fragments = bi->bytes / bi->fragsize;

            pa_threaded_mainloop_unlock(i->mainloop);

            debug(DEBUG_LEVEL_NORMAL, __FILE__": fragsize=%i, fragstotal=%i, bytes=%i, fragments=%i\n", bi->fragsize, bi->fragstotal, bi->bytes, bi->fragments);

            break;
        }

        case SOUND_PCM_READ_RATE:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_PCM_READ_RATE\n");

            pa_threaded_mainloop_lock(i->mainloop);
            *(int*) argp = i->sample_spec.rate;
            pa_threaded_mainloop_unlock(i->mainloop);
            break;

        case SOUND_PCM_READ_CHANNELS:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_PCM_READ_CHANNELS\n");

            pa_threaded_mainloop_lock(i->mainloop);
            *(int*) argp = i->sample_spec.channels;
            pa_threaded_mainloop_unlock(i->mainloop);
            break;

        case SOUND_PCM_READ_BITS:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SOUND_PCM_READ_BITS\n");

            pa_threaded_mainloop_lock(i->mainloop);
            *(int*) argp = pa_sample_size(&i->sample_spec)*8;
            pa_threaded_mainloop_unlock(i->mainloop);
            break;

        case SNDCTL_DSP_GETOPTR: {
            count_info *info;

            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_GETOPTR\n");

            info = (count_info*) argp;
            memset(info, 0, sizeof(*info));

            pa_threaded_mainloop_lock(i->mainloop);

            for (;;) {
                pa_usec_t usec;

                PLAYBACK_STREAM_CHECK_DEAD_GOTO(i, exit_loop2);

                if (pa_stream_get_time(i->play_stream, &usec) >= 0) {
                    size_t k = pa_usec_to_bytes(usec, &i->sample_spec);
                    int m;

                    info->bytes = (int) k;
                    m = k / i->fragment_size;
                    info->blocks = m - i->optr_n_blocks;
                    i->optr_n_blocks = m;

                    break;
                }

                if (pa_context_errno(i->context) != PA_ERR_NODATA) {
                    debug(DEBUG_LEVEL_NORMAL, __FILE__": pa_stream_get_latency(): %s\n", pa_strerror(pa_context_errno(i->context)));
                    break;
                }

                pa_threaded_mainloop_wait(i->mainloop);
            }

        exit_loop2:

            pa_threaded_mainloop_unlock(i->mainloop);

            debug(DEBUG_LEVEL_NORMAL, __FILE__": GETOPTR bytes=%i, blocks=%i, ptr=%i\n", info->bytes, info->blocks, info->ptr);

            break;
        }

        case SNDCTL_DSP_GETIPTR:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": invalid ioctl SNDCTL_DSP_GETIPTR\n");
            goto inval;

        case SNDCTL_DSP_SETDUPLEX:
            debug(DEBUG_LEVEL_NORMAL, __FILE__": SNDCTL_DSP_SETDUPLEX\n");
            /* this is a no-op */
            break;

        default:
            /* Mixer ioctls are valid on /dev/dsp as well */
            return mixer_ioctl(i, request, argp, _errno);

inval:
            *_errno = EINVAL;
            goto fail;
    }

    ret = 0;

fail:

    return ret;
}

#ifdef sun
int ioctl(int fd, int request, ...) {
#else
int ioctl(int fd, unsigned long request, ...) {
#endif
    fd_info *i;
    va_list args;
    void *argp;
    int r, _errno = 0;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": ioctl()\n");

    va_start(args, request);
    argp = va_arg(args, void *);
    va_end(args);

    if (!function_enter()) {
        LOAD_IOCTL_FUNC();
        return _ioctl(fd, request, argp);
    }

    if (!(i = fd_info_find(fd))) {
        function_exit();
        LOAD_IOCTL_FUNC();
        return _ioctl(fd, request, argp);
    }

    if (i->type == FD_INFO_MIXER)
        r = mixer_ioctl(i, request, argp, &_errno);
    else
        r = dsp_ioctl(i, request, argp, &_errno);

    fd_info_unref(i);

    if (_errno)
        errno = _errno;

    function_exit();

    return r;
}

int close(int fd) {
    fd_info *i;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": close()\n");

    if (!function_enter()) {
        LOAD_CLOSE_FUNC();
        return _close(fd);
    }

    if (!(i = fd_info_find(fd))) {
        function_exit();
        LOAD_CLOSE_FUNC();
        return _close(fd);
    }

    fd_info_remove_from_list(i);
    fd_info_unref(i);

    function_exit();

    return 0;
}

int access(const char *pathname, int mode) {

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": access(%s)\n", pathname?pathname:"NULL");

    if (!pathname ||
        !is_audio_device_node(pathname)) {
        LOAD_ACCESS_FUNC();
        return _access(pathname, mode);
    }

    if (mode & X_OK) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": access(%s, %x) = EACCESS\n", pathname, mode);
        errno = EACCES;
        return -1;
    }

    debug(DEBUG_LEVEL_NORMAL, __FILE__": access(%s, %x) = OK\n", pathname, mode);

    return 0;
}

int stat(const char *pathname, struct stat *buf) {
#ifdef HAVE_OPEN64
    struct stat64 parent;
#else
    struct stat parent;
#endif
    int ret;

    if (!pathname ||
        !buf ||
        !is_audio_device_node(pathname)) {
        debug(DEBUG_LEVEL_VERBOSE, __FILE__": stat(%s)\n", pathname?pathname:"NULL");
        LOAD_STAT_FUNC();
        return _stat(pathname, buf);
    }

    debug(DEBUG_LEVEL_NORMAL, __FILE__": stat(%s)\n", pathname);

#ifdef _STAT_VER
#ifdef HAVE_OPEN64
    ret = __xstat64(_STAT_VER, "/dev", &parent);
#else
    ret = __xstat(_STAT_VER, "/dev", &parent);
#endif
#else
#ifdef HAVE_OPEN64
    ret = stat64("/dev", &parent);
#else
    ret = stat("/dev", &parent);
#endif
#endif

    if (ret) {
        debug(DEBUG_LEVEL_NORMAL, __FILE__": unable to stat \"/dev\"\n");
        return -1;
    }

    buf->st_dev = parent.st_dev;
    buf->st_ino = 0xDEADBEEF;   /* FIXME: Can we do this in a safe way? */
    buf->st_mode = S_IFCHR | S_IRUSR | S_IWUSR;
    buf->st_nlink = 1;
    buf->st_uid = getuid();
    buf->st_gid = getgid();
    buf->st_rdev = 0x0E03;      /* FIXME: Linux specific */
    buf->st_size = 0;
    buf->st_atime = 1181557705;
    buf->st_mtime = 1181557705;
    buf->st_ctime = 1181557705;
    buf->st_blksize = 1;
    buf->st_blocks = 0;

    return 0;
}

#ifdef HAVE_OPEN64

int stat64(const char *pathname, struct stat64 *buf) {
    struct stat oldbuf;
    int ret;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": stat64(%s)\n", pathname?pathname:"NULL");

    if (!pathname ||
        !buf ||
        !is_audio_device_node(pathname)) {
        LOAD_STAT64_FUNC();
        return _stat64(pathname, buf);
    }

    ret = stat(pathname, &oldbuf);
    if (ret)
        return ret;

    buf->st_dev = oldbuf.st_dev;
    buf->st_ino = oldbuf.st_ino;
    buf->st_mode = oldbuf.st_mode;
    buf->st_nlink = oldbuf.st_nlink;
    buf->st_uid = oldbuf.st_uid;
    buf->st_gid = oldbuf.st_gid;
    buf->st_rdev = oldbuf.st_rdev;
    buf->st_size = oldbuf.st_size;
    buf->st_atime = oldbuf.st_atime;
    buf->st_mtime = oldbuf.st_mtime;
    buf->st_ctime = oldbuf.st_ctime;
    buf->st_blksize = oldbuf.st_blksize;
    buf->st_blocks = oldbuf.st_blocks;

    return 0;
}

int open64(const char *filename, int flags, ...) {
    va_list args;
    mode_t mode = 0;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": open64(%s)\n", filename?filename:"NULL");

    if (flags & O_CREAT) {
        va_start(args, flags);
        if (sizeof(mode_t) < sizeof(int))
            mode = va_arg(args, int);
        else
            mode = va_arg(args, mode_t);
        va_end(args);
    }

    if (!filename ||
        !is_audio_device_node(filename)) {
        LOAD_OPEN64_FUNC();
        return _open64(filename, flags, mode);
    }

    return real_open(filename, flags, mode);
}

int __open64_2(const char *filename, int flags) {
    debug(DEBUG_LEVEL_VERBOSE, __FILE__": __open64_2(%s)\n", filename?filename:"NULL");

    if ((flags & O_CREAT) ||
        !filename ||
        !is_audio_device_node(filename)) {
        LOAD___OPEN64_2_FUNC();
        return ___open64_2(filename, flags);
    }

    return real_open(filename, flags, 0);
}

#endif

#ifdef _STAT_VER

int __xstat(int ver, const char *pathname, struct stat *buf) {
    debug(DEBUG_LEVEL_VERBOSE, __FILE__": __xstat(%s)\n", pathname?pathname:"NULL");

    if (!pathname ||
        !buf ||
        !is_audio_device_node(pathname)) {
        LOAD_XSTAT_FUNC();
        return ___xstat(ver, pathname, buf);
    }

    if (ver != _STAT_VER) {
        errno = EINVAL;
        return -1;
    }

    return stat(pathname, buf);
}

#ifdef HAVE_OPEN64

int __xstat64(int ver, const char *pathname, struct stat64 *buf) {
    debug(DEBUG_LEVEL_VERBOSE, __FILE__": __xstat64(%s)\n", pathname?pathname:"NULL");

    if (!pathname ||
        !buf ||
        !is_audio_device_node(pathname)) {
        LOAD_XSTAT64_FUNC();
        return ___xstat64(ver, pathname, buf);
    }

    if (ver != _STAT_VER) {
        errno = EINVAL;
        return -1;
    }

    return stat64(pathname, buf);
}

#endif

#endif

FILE* fopen(const char *filename, const char *mode) {
    FILE *f = NULL;
    int fd;
    mode_t m;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": fopen(%s)\n", filename?filename:"NULL");

    if (!filename ||
        !mode ||
        !is_audio_device_node(filename)) {
        LOAD_FOPEN_FUNC();
        return _fopen(filename, mode);
    }

    switch (mode[0]) {
    case 'r':
        m = O_RDONLY;
        break;
    case 'w':
    case 'a':
        m = O_WRONLY;
        break;
    default:
        errno = EINVAL;
        return NULL;
    }

    if ((((mode[1] == 'b') || (mode[1] == 't')) && (mode[2] == '+')) || (mode[1] == '+'))
        m = O_RDWR;

    if ((fd = real_open(filename, m, 0)) < 0)
        return NULL;

    if (!(f = fdopen(fd, mode))) {
        close(fd);
        return NULL;
    }

    return f;
}

#ifdef HAVE_OPEN64

FILE *fopen64(const char *filename, const char *mode) {

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": fopen64(%s)\n", filename?filename:"NULL");

    if (!filename ||
        !mode ||
        !is_audio_device_node(filename)) {
        LOAD_FOPEN64_FUNC();
        return _fopen64(filename, mode);
    }

    return fopen(filename, mode);
}

#endif

int fclose(FILE *f) {
    fd_info *i;

    debug(DEBUG_LEVEL_VERBOSE, __FILE__": fclose()\n");

    if (!function_enter()) {
        LOAD_FCLOSE_FUNC();
        return _fclose(f);
    }

    if (!(i = fd_info_find(fileno(f)))) {
        function_exit();
        LOAD_FCLOSE_FUNC();
        return _fclose(f);
    }

    fd_info_remove_from_list(i);

    /* Dirty trick to avoid that the fd is not freed twice, once by us
     * and once by the real fclose() */
    i->app_fd = -1;

    fd_info_unref(i);

    function_exit();

    LOAD_FCLOSE_FUNC();
    return _fclose(f);
}

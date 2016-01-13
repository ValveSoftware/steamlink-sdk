/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2011 George Boutsioukis for Xen

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

#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <time.h>

#include <pulse/xmalloc.h>

#include <pulsecore/core-error.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>

#include <sys/select.h>
#include <sys/mman.h>
#include <xenctrl.h>
#include <xs.h>

#include "module-xenpv-sink-symdef.h"
#include "gntalloc.h"
#include "gntdev.h"

PA_MODULE_AUTHOR("Giorgos Boutsioukis");
PA_MODULE_DESCRIPTION("Xen PV audio sink");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink_name=<name for the sink> "
        "sink_properties=<properties for the sink> "
        "format=<sample format> "
        "rate=<sample rate>"
        "channels=<number of channels> "
        "channel_map=<channel map>");

#define DEFAULT_SINK_NAME "xenpv_output"
#define DEFAULT_FILE_NAME "xenpv_output"

#define STATE_UNDEFINED 9999

int device_id = -1;
enum xenbus_state {
    XenbusStateUnknown      = 0,
    XenbusStateInitialising = 1,
    XenbusStateInitWait     = 2,
    XenbusStateInitialised  = 3,
    XenbusStateConnected    = 4,
    XenbusStateClosing      = 5,
    XenbusStateClosed       = 6,
    XenbusStateReconfiguring = 7,
    XenbusStateReconfigured  = 8
};

static const char* xenbus_names[] = {
    "XenbusStateUnknown",
    "XenbusStateInitialising",
    "XenbusStateInitWait",
    "XenbusStateInitialised",
    "XenbusStateConnected",
    "XenbusStateClosing",
    "XenbusStateClosed",
    "XenbusStateReconfiguring",
    "XenbusStateReconfigured"
};

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_sink *sink;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;

    pa_memchunk memchunk;

    pa_rtpoll_item *rtpoll_item;

    int write_type;
};

pa_sample_spec ss;
pa_channel_map map;

/* just to test non- frame-aligned size */
#define BUFSIZE 2047

struct ring {
    uint32_t cons_indx, prod_indx;
    uint32_t usable_buffer_space; /* kept here for convenience */
    uint8_t buffer[BUFSIZE];
} *ioring;

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "file",
    "format",
    "rate",
    "channels",
    "channel_map",
    NULL
};

/* Xen globals*/
xc_interface* xch;
xc_evtchn* xce;
evtchn_port_or_error_t xen_evtchn_port;
static struct xs_handle *xsh;
struct ioctl_gntalloc_alloc_gref gref;

static int register_backend_state_watch(void);
static int wait_for_backend_state_change(void);
static int alloc_gref(struct ioctl_gntalloc_alloc_gref *gref, void **addr);
static int ring_write(struct ring *r, void *src, int length);
static int publish_spec(pa_sample_spec *ss);
static int read_backend_default_spec(pa_sample_spec *ss);
static int publish_param(const char *paramname, const char *value);
static int publish_param_int(const char *paramname, const int value);
static char* read_param(const char *paramname);

static int set_state(int state) {
    static int current_state = 0;
    pa_log_debug("State transition %s->%s\n",
            xenbus_names[current_state], xenbus_names[state]);

    publish_param_int("state", state);
    current_state = state;
    return state;
}
#define NEGOTIATION_ERROR 2
#define NEGOTIATION_OK 1

/* negotiation callbacks */
static int state_unknown_cb() {
    pa_log_debug("Xen audio sink: Backend state was XenbusStateUnknown\n");
    set_state(XenbusStateInitialising);

    return 0;
}

static int state_initialising_cb() {
    pa_log_debug("Xen audio sink: Backend state was XenbusStateInitialising\n");
    set_state(XenbusStateInitialised);
    return 0;
}

static int state_initwait_cb() {
    pa_log_debug("Xen audio sink: Backend state was XenbusStateInitWait\n");
    return 0;
}

static int state_initialised_cb() {
    pa_log_debug("Xen audio sink: Backend state was XenbusStateInitialised\n");
    /*Remind the backend we are ready*/
    set_state(XenbusStateInitialised);
    return 0;
}

static int state_connected_cb() {
    /* The backend accepted our parameters, sweet! */
    set_state(XenbusStateConnected);
    pa_log_debug("Xen audio sink: Backend state was XenbusStateConnected\n");
    return NEGOTIATION_OK;
}

static int state_closing_cb() {
    pa_log_debug("Xen audio sink: Backend state was XenbusStateClosing\n");
    return 0;
}

static int state_closed_cb() {
    pa_log_debug("Xen audio sink: Backend state was XenbusStateClosed\n");
    return 0;
}

static int state_reconfiguring_cb() {
    /* The backend rejected our sample spec */
    pa_log_debug("Xen audio sink: Backend state was XenbusStateReconfiguring\n");
    /* fall back to the backend's default parameters*/
    read_backend_default_spec(&ss);
    /* backend should accept these now */
    publish_spec(&ss);
    set_state(XenbusStateInitialised);
    return 0;
}

static int state_reconfigured_cb() {
    pa_log_debug("Xen audio sink: Backend state was XenbusStateReconfigured\n");
    return 0;
}

int (*state_callbacks[9])(void) = {
    state_unknown_cb,
    state_initialising_cb,
    state_initwait_cb,
    state_initialised_cb,
    state_connected_cb,
    state_closing_cb,
    state_closed_cb,
    state_reconfiguring_cb,
    state_reconfigured_cb
};

static void xen_cleanup() {
    char keybuf[64];
    /*XXX hardcoded*/
    munmap((void*)gref.index, 4096);

    set_state(XenbusStateClosing);
    /* send one last event to unblock the backend */
    xc_evtchn_notify(xce, xen_evtchn_port);
    /* close xen interfaces */
    xc_evtchn_close(xce);
    xc_interface_close(xch);

    /* delete xenstore keys */
    publish_param_int("state", XenbusStateClosed);
    snprintf(keybuf, sizeof(keybuf), "device/audio/%d", device_id);
    xs_rm(xsh, 0, keybuf);
    xs_daemon_close(xsh);
}

static int sink_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {

        case PA_SINK_MESSAGE_GET_LATENCY: {
            size_t n = 0;

            n += u->memchunk.length;

            *((pa_usec_t*) data) = pa_bytes_to_usec(n, &u->sink->sample_spec);
            return 0;
        }
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

static int process_render(struct userdata *u) {
    pa_assert(u);

    if (u->memchunk.length <= 0)
        pa_sink_render(u->sink, ioring->usable_buffer_space, &u->memchunk);

    pa_assert(u->memchunk.length > 0);

    xc_evtchn_notify(xce, xen_evtchn_port);
    for (;;) {
        ssize_t l;
        void *p;

        p = pa_memblock_acquire(u->memchunk.memblock);
        /* xen: write data to ring buffer & notify backend */
        l = ring_write(ioring, (uint8_t*)p + u->memchunk.index, u->memchunk.length);

        pa_memblock_release(u->memchunk.memblock);

        pa_assert(l != 0);

        if (l < 0) {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
                return 0;
            else {
                pa_log("Failed to write data to FIFO: %s", pa_cstrerror(errno));
                return -1;
            }

        } else {

            u->memchunk.index += (size_t) l;
            u->memchunk.length -= (size_t) l;

            if (u->memchunk.length <= 0) {
                pa_memblock_unref(u->memchunk.memblock);
                pa_memchunk_reset(&u->memchunk);
            }
        }

        return 0;
    }
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);

    pa_log_debug("Thread starting up");

    pa_thread_mq_install(&u->thread_mq);

    for(;;) {
        struct pollfd *pollfd;
        int ret;

        pollfd = pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL);

        if (PA_UNLIKELY(u->sink->thread_info.rewind_requested))
            pa_sink_process_rewind(u->sink, 0);

        /* Render some data and write it to the fifo */
        if (PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
            if (pollfd->revents) {
                if (process_render(u) < 0)
                    goto fail;

                pollfd->revents = 0;
            }
        }

        /* Hmm, nothing to do. Let's sleep */

        pollfd->events = (short) (u->sink->thread_info.state == PA_SINK_RUNNING ? POLLOUT : 0);

        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0)
            goto fail;

        if (ret == 0)
            goto finish;

        pollfd = pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL);

        if (pollfd->revents & ~POLLOUT) {
            pa_log("FIFO shutdown.");
            goto fail;
        }
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);
    pa_log_debug("Shutting down Xen...");
    xen_cleanup();
finish:
    pa_log_debug("Thread shutting down");
}

int pa__init(pa_module*m) {

    struct userdata *u;
    pa_modargs *ma;
    pa_sink_new_data data;
    int backend_state;
    int ret;
    char strbuf[100];

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    ss = m->core->default_sample_spec;
    map = m->core->default_channel_map;

    /* user arguments override these */
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        return 1;
    }

    /* Xen Basic init */
    xsh = xs_domain_open();
    if (xsh==NULL) {
        pa_log("xs_domain_open failed");
        goto fail;
    }
    set_state(XenbusStateUnknown);

    xch = xc_interface_open(NULL, NULL, 0);
    if (xch==0) {
        pa_log("xc_interface_open failed");
        goto fail;
    }

    xce = xc_evtchn_open(NULL, 0);
    if (xce==0) {
        pa_log("xc_evtchn_open failed");
        goto fail;
    }

    /* use only dom0 as the backend for now */
    xen_evtchn_port = xc_evtchn_bind_unbound_port(xce, 0);
    if (xen_evtchn_port == 0) {
        pa_log("xc_evtchn_bind_unbound_port failed");
    }

    /* get grant reference & map locally */
    if (alloc_gref(&gref, (void**)&ioring)) {
       pa_log("alloc_gref failed");
    };
    device_id = 0; /* hardcoded for now */

    if (register_backend_state_watch()) {
        pa_log("Xen sink: register xenstore watch failed");
    };

    publish_param_int("event-channel", xen_evtchn_port);
    publish_param_int("ring-ref", gref.gref_ids[0]);

    /* let's ask for something absurd and deal with rejection */
    ss.rate = 192000;

    publish_spec(&ss);

    ret=0;
    while (!ret) {
        backend_state = wait_for_backend_state_change();
        if (backend_state == STATE_UNDEFINED) {
            pa_log("Xen Backend is taking long to respond, still waiting...");
            continue;
        } else if (backend_state == -1) {
            pa_log("Error while waiting for backend: %s", strerror(errno));
            break;
            goto fail;
        }
        ret = state_callbacks[backend_state]();
    }
    if (ret!=NEGOTIATION_OK) {
        pa_log("Negotiation with Xen backend failed!");
        return 1;
    }

    pa_sample_spec_snprint(strbuf, 100, &ss);
    pa_log_debug("Negotiation ended, the result was: %s", strbuf);

    /* End of Phase 2, begin playback cycle */

    u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    m->userdata = u;
    pa_memchunk_reset(&u->memchunk);
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);
    u->write_type = 0;

    /* init ring buffer */
    ioring->prod_indx = ioring->cons_indx = 0;
    ioring->usable_buffer_space = BUFSIZE - BUFSIZE % pa_frame_size(&ss);

    pa_sink_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    pa_sink_new_data_set_name(&data, pa_modargs_get_value(ma, "sink_name", DEFAULT_SINK_NAME));
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_STRING, "xensink");
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "Xen PV audio sink");
    pa_sink_new_data_set_sample_spec(&data, &ss);
    pa_sink_new_data_set_channel_map(&data, &map);

    if (pa_modargs_get_proplist(ma, "sink_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&data);
        goto fail;
    }

    u->sink = pa_sink_new(m->core, &data, PA_SINK_LATENCY);
    pa_sink_new_data_done(&data);

    if (!u->sink) {
        pa_log("Failed to create sink.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg;
    u->sink->userdata = u;

    pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
    pa_sink_set_rtpoll(u->sink, u->rtpoll);
    pa_sink_set_max_request(u->sink, ioring->usable_buffer_space);
    pa_sink_set_fixed_latency(u->sink, pa_bytes_to_usec(ioring->usable_buffer_space, &u->sink->sample_spec));

    u->rtpoll_item = pa_rtpoll_item_new(u->rtpoll, PA_RTPOLL_NEVER, 1);

    if (!(u->thread = pa_thread_new("xenpv-sink", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    pa_sink_put(u->sink);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return pa_sink_linked_by(u->sink);
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->sink)
        pa_sink_unref(u->sink);

    if (u->memchunk.memblock)
       pa_memblock_unref(u->memchunk.memblock);

    if (u->rtpoll_item)
        pa_rtpoll_item_free(u->rtpoll_item);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    pa_xfree(u);

    xen_cleanup();

}

static int alloc_gref(struct ioctl_gntalloc_alloc_gref *gref_, void **addr) {
    int alloc_fd, dev_fd, rv;

    alloc_fd = open("/dev/xen/gntalloc", O_RDWR);
    if (alloc_fd<=0) {
        perror("Could not open /dev/xen/gntalloc! Have you loaded the xen_gntalloc module?");
        return 1;
    }

    dev_fd = open("/dev/xen/gntdev", O_RDWR);
    if (dev_fd<=0) {
        perror("Could not open /dev/xen/gntdev! Have you loaded the xen_gntdev module?");
        return 1;
    }

    /* use dom0 */
    gref_->domid = 0;
    gref_->flags = GNTALLOC_FLAG_WRITABLE;
    gref_->count = 1;

    rv = ioctl(alloc_fd, IOCTL_GNTALLOC_ALLOC_GREF, gref_);
    if (rv) {
        pa_log_debug("Xen audio sink: src-add error: %s (rv=%d)\n", strerror(errno), rv);
        return rv;
    }

    /*addr=NULL(default),length, prot,             flags,    fd,         offset*/
    *addr = mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, alloc_fd, gref_->index);
    if (*addr == MAP_FAILED) {
        *addr = 0;
        pa_log_debug("Xen audio sink: mmap'ing shared page failed\n");
        return rv;
    }

    pa_log_debug("Xen audio sink: Got grant #%d. Mapped locally at %Ld=%p\n",
            gref_->gref_ids[0], (long long)gref_->index, *addr);

    /* skip this for now
       struct ioctl_gntalloc_unmap_notify uarg = {
       .index = gref->index + offsetof(struct shr_page, notifies[0]),
       .action = UNMAP_NOTIFY_CLEAR_BYTE
       };

       rv = ioctl(a_fd, IOCTL_GNTALLOC_SET_UNMAP_NOTIFY, &uarg);
       if (rv)
       pa_log_debug("gntalloc unmap notify error: %s (rv=%d)\n", strerror(errno), rv);
       */

    close(alloc_fd);
    close(dev_fd);

    return rv;
}

#define RING_FREE_BYTES ((r->usable_buffer_space - (r->prod_indx-r->cons_indx) -1) % r->usable_buffer_space)
static int ring_write(struct ring *r, void *src, int length) {
    int full = 0;
    for (;;) {
        /* free space may be split over the end of the buffer */
        int first_chunk_size = (r->usable_buffer_space-r->prod_indx);
        int second_chunk_size = (r->cons_indx>=r->prod_indx)? (r->cons_indx) : 0;
        int l, fl, sl;

        /* full? */
        if (RING_FREE_BYTES==0) {
            /*XXX hardcoded*/
            if (full>=100) {
                errno = EINTR;
                return -1;
            }
            /*XXX hardcoded */
            usleep(1000);
            /* should return in 100ms max; definitely not midstream */
            full++;
            continue;
        }

        /* calculate lengths in case of a split buffer */
        l = PA_MIN((int)RING_FREE_BYTES, length);
        fl = PA_MIN(l, first_chunk_size);
        sl = PA_MIN(l-fl, second_chunk_size);

        memcpy(r->buffer+r->prod_indx, src, fl);
        if (sl)
            memcpy(r->buffer, ((char*)src)+fl, sl);
        r->prod_indx = (r->prod_indx+fl+sl) % r->usable_buffer_space;

        return sl+fl;
    }
}

static int publish_param(const char *paramname, const char *value) {
    char keybuf[128], valbuf[32];

    snprintf(keybuf, sizeof keybuf, "device/audio/%d/%s", device_id, paramname);
    snprintf(valbuf, sizeof valbuf, "%s", value);
    return xs_write(xsh, 0, keybuf, valbuf, strlen(valbuf));
}

static int publish_param_int(const char *paramname, const int value) {
    char keybuf[128], valbuf[32];
    snprintf(keybuf, sizeof keybuf, "device/audio/%d/%s", device_id, paramname);
    snprintf(valbuf, sizeof valbuf, "%d", value);
    return xs_write(xsh, 0, keybuf, valbuf, strlen(valbuf));
}

static char* read_param(const char *paramname) {
    char keybuf[128];
    unsigned int len;
    int my_domid;

    my_domid = atoi(xs_read(xsh, 0, "domid", &len));
    snprintf(keybuf, sizeof(keybuf), "/local/domain/0/backend/audio/%d/%d/%s", my_domid, device_id, paramname);
    /* remember to free lvalue! */
    return xs_read(xsh, 0, keybuf, &len);
}

static int publish_spec(pa_sample_spec *sample_spec) {
    /* Publish spec and set state to XenbusStateInitWait*/
    int ret;

    ret = publish_param("format", pa_sample_format_to_string(sample_spec->format));
    ret += publish_param_int("rate", sample_spec->rate);
    ret += publish_param_int("channels", sample_spec->channels);

    return ret;
}

static int read_backend_default_spec(pa_sample_spec *sample_spec) {
    /* Read spec from backend */
    char *out;

    out = read_param("default-format");
    sample_spec->format = pa_parse_sample_format(out);
    free(out);

    out = read_param("default-rate");
    sample_spec->rate = atoi(out);
    free(out);

    out = read_param("default-channels");
    sample_spec->channels = atoi(out);
    free(out);

    return 0;
}

static int register_backend_state_watch() {
    char keybuf[128];
    int my_domid;
    unsigned int len;

    my_domid = atoi(xs_read(xsh, 0, "domid", &len));
    snprintf(keybuf, sizeof(keybuf), "/local/domain/0/backend/audio/%d/%d/state", my_domid, device_id);
    if (!xs_watch(xsh, keybuf, "xenpvaudiofrontendsinktoken")) {
        perror("xs_watch failed");
        return -EINVAL;
    }
    return 0;
}

static int wait_for_backend_state_change() {
    char keybuf[128];
    int my_domid;
    unsigned int len;

    int backend_state;
    int seconds;
    char *buf, **vec;
    int ret;

    int xs_fd;
    struct timeval tv;
    fd_set watch_fdset;
    int start, now;

    backend_state = STATE_UNDEFINED;
    xs_fd = xs_fileno(xsh);
    start = now = time(NULL);

    my_domid = atoi(xs_read(xsh, 0, "domid", &len));
    snprintf(keybuf, sizeof(keybuf), "/local/domain/0/backend/audio/%d/%d/state", my_domid, device_id);

    /*XXX: hardcoded */
    seconds = 10;
    do {
        tv.tv_usec = 0;
        tv.tv_sec = (start + seconds) - now;
        FD_ZERO(&watch_fdset);
        FD_SET(xs_fd, &watch_fdset);
        ret=select(xs_fd + 1, &watch_fdset, NULL, NULL, &tv);

        if (ret==-1)
            /* error */
            return -1;
        else if (ret) {

            /* Read the watch to drain the buffer */
            vec = xs_read_watch(xsh, &len);

            buf = xs_read(xsh, XBT_NULL, vec[0], &len);
            if (buf == 0) {
                /* usually means that the backend isn't there yet */
                continue;
            };
            backend_state = atoi(buf);

            free(buf);
            free(vec);
        }
    /* else: timeout */
    } while (backend_state == STATE_UNDEFINED && \
            (now = time(NULL)) < start + seconds);

    return backend_state;
}

/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_X11
#include <xcb/xcb.h>
#endif

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/util.h>
#include <pulse/version.h>
#include <pulse/xmalloc.h>

#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/core-subscribe.h>
#include <pulsecore/pdispatch.h>
#include <pulsecore/pstream.h>
#include <pulsecore/pstream-util.h>
#include <pulsecore/socket-client.h>
#include <pulsecore/time-smoother.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-error.h>
#include <pulsecore/proplist-util.h>
#include <pulsecore/auth-cookie.h>
#include <pulsecore/mcalign.h>
#include <pulsecore/strlist.h>

#ifdef HAVE_X11
#include <pulsecore/x11prop.h>
#endif

#ifdef TUNNEL_SINK
#include "module-tunnel-sink-symdef.h"
#else
#include "module-tunnel-source-symdef.h"
#endif

#define ENV_DEFAULT_SINK "PULSE_SINK"
#define ENV_DEFAULT_SOURCE "PULSE_SOURCE"
#define ENV_DEFAULT_SERVER "PULSE_SERVER"
#define ENV_COOKIE_FILE "PULSE_COOKIE"

#ifdef TUNNEL_SINK
PA_MODULE_DESCRIPTION("Tunnel module for sinks");
PA_MODULE_USAGE(
        "sink_name=<name for the local sink> "
        "sink_properties=<properties for the local sink> "
        "auto=<determine server/sink/cookie automatically> "
        "server=<address> "
        "sink=<remote sink name> "
        "cookie=<filename> "
        "format=<sample format> "
        "channels=<number of channels> "
        "rate=<sample rate> "
        "channel_map=<channel map>");
#else
PA_MODULE_DESCRIPTION("Tunnel module for sources");
PA_MODULE_USAGE(
        "source_name=<name for the local source> "
        "source_properties=<properties for the local source> "
        "auto=<determine server/source/cookie automatically> "
        "server=<address> "
        "source=<remote source name> "
        "cookie=<filename> "
        "format=<sample format> "
        "channels=<number of channels> "
        "rate=<sample rate> "
        "channel_map=<channel map>");
#endif

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);

static const char* const valid_modargs[] = {
    "auto",
    "server",
    "cookie",
    "format",
    "channels",
    "rate",
#ifdef TUNNEL_SINK
    "sink_name",
    "sink_properties",
    "sink",
#else
    "source_name",
    "source_properties",
    "source",
#endif
    "channel_map",
    NULL,
};

#define DEFAULT_TIMEOUT 5

#define LATENCY_INTERVAL (10*PA_USEC_PER_SEC)

#define MIN_NETWORK_LATENCY_USEC (8*PA_USEC_PER_MSEC)

#ifdef TUNNEL_SINK

enum {
    SINK_MESSAGE_REQUEST = PA_SINK_MESSAGE_MAX,
    SINK_MESSAGE_REMOTE_SUSPEND,
    SINK_MESSAGE_UPDATE_LATENCY,
    SINK_MESSAGE_POST
};

#define DEFAULT_TLENGTH_MSEC 150
#define DEFAULT_MINREQ_MSEC 25

#else

enum {
    SOURCE_MESSAGE_POST = PA_SOURCE_MESSAGE_MAX,
    SOURCE_MESSAGE_REMOTE_SUSPEND,
    SOURCE_MESSAGE_UPDATE_LATENCY
};

#define DEFAULT_FRAGSIZE_MSEC 25

#endif

#ifdef TUNNEL_SINK
static void command_request(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
static void command_started(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
#endif
static void command_subscribe_event(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
static void command_stream_killed(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
static void command_overflow_or_underflow(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
static void command_suspended(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
static void command_moved(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
static void command_stream_or_client_event(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);
static void command_stream_buffer_attr_changed(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata);

static const pa_pdispatch_cb_t command_table[PA_COMMAND_MAX] = {
#ifdef TUNNEL_SINK
    [PA_COMMAND_REQUEST] = command_request,
    [PA_COMMAND_STARTED] = command_started,
#endif
    [PA_COMMAND_SUBSCRIBE_EVENT] = command_subscribe_event,
    [PA_COMMAND_OVERFLOW] = command_overflow_or_underflow,
    [PA_COMMAND_UNDERFLOW] = command_overflow_or_underflow,
    [PA_COMMAND_PLAYBACK_STREAM_KILLED] = command_stream_killed,
    [PA_COMMAND_RECORD_STREAM_KILLED] = command_stream_killed,
    [PA_COMMAND_PLAYBACK_STREAM_SUSPENDED] = command_suspended,
    [PA_COMMAND_RECORD_STREAM_SUSPENDED] = command_suspended,
    [PA_COMMAND_PLAYBACK_STREAM_MOVED] = command_moved,
    [PA_COMMAND_RECORD_STREAM_MOVED] = command_moved,
    [PA_COMMAND_PLAYBACK_STREAM_EVENT] = command_stream_or_client_event,
    [PA_COMMAND_RECORD_STREAM_EVENT] = command_stream_or_client_event,
    [PA_COMMAND_CLIENT_EVENT] = command_stream_or_client_event,
    [PA_COMMAND_PLAYBACK_BUFFER_ATTR_CHANGED] = command_stream_buffer_attr_changed,
    [PA_COMMAND_RECORD_BUFFER_ATTR_CHANGED] = command_stream_buffer_attr_changed
};

struct userdata {
    pa_core *core;
    pa_module *module;

    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;
    pa_thread *thread;

    pa_socket_client *client;
    pa_pstream *pstream;
    pa_pdispatch *pdispatch;

    char *server_name;
#ifdef TUNNEL_SINK
    char *sink_name;
    pa_sink *sink;
    size_t requested_bytes;
#else
    char *source_name;
    pa_source *source;
    pa_mcalign *mcalign;
#endif

    pa_auth_cookie *auth_cookie;

    uint32_t version;
    uint32_t ctag;
    uint32_t device_index;
    uint32_t channel;

    int64_t counter, counter_delta;

    bool remote_corked:1;
    bool remote_suspended:1;

    pa_usec_t transport_usec; /* maintained in the main thread */
    pa_usec_t thread_transport_usec; /* maintained in the IO thread */

    uint32_t ignore_latency_before;

    pa_time_event *time_event;

    pa_smoother *smoother;

    char *device_description;
    char *server_fqdn;
    char *user_name;

    uint32_t maxlength;
#ifdef TUNNEL_SINK
    uint32_t tlength;
    uint32_t minreq;
    uint32_t prebuf;
#else
    uint32_t fragsize;
#endif
};

static void request_latency(struct userdata *u);

/* Called from main context */
static void command_stream_or_client_event(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata) {
    pa_log_debug("Got stream or client event.");
}

/* Called from main context */
static void command_stream_killed(pa_pdispatch *pd,  uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(pd);
    pa_assert(t);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    pa_log_warn("Stream killed");
    pa_module_unload_request(u->module, true);
}

/* Called from main context */
static void command_overflow_or_underflow(pa_pdispatch *pd,  uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(pd);
    pa_assert(t);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    pa_log_info("Server signalled buffer overrun/underrun.");
    request_latency(u);
}

/* Called from main context */
static void command_suspended(pa_pdispatch *pd,  uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t channel;
    bool suspended;

    pa_assert(pd);
    pa_assert(t);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    if (pa_tagstruct_getu32(t, &channel) < 0 ||
        pa_tagstruct_get_boolean(t, &suspended) < 0 ||
        !pa_tagstruct_eof(t)) {

        pa_log("Invalid packet.");
        pa_module_unload_request(u->module, true);
        return;
    }

    pa_log_debug("Server reports device suspend.");

#ifdef TUNNEL_SINK
    pa_asyncmsgq_send(u->sink->asyncmsgq, PA_MSGOBJECT(u->sink), SINK_MESSAGE_REMOTE_SUSPEND, PA_UINT32_TO_PTR(suspended), 0, NULL);
#else
    pa_asyncmsgq_send(u->source->asyncmsgq, PA_MSGOBJECT(u->source), SOURCE_MESSAGE_REMOTE_SUSPEND, PA_UINT32_TO_PTR(suspended), 0, NULL);
#endif

    request_latency(u);
}

/* Called from main context */
static void command_moved(pa_pdispatch *pd,  uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t channel, di;
    const char *dn;
    bool suspended;

    pa_assert(pd);
    pa_assert(t);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    if (pa_tagstruct_getu32(t, &channel) < 0 ||
        pa_tagstruct_getu32(t, &di) < 0 ||
        pa_tagstruct_gets(t, &dn) < 0 ||
        pa_tagstruct_get_boolean(t, &suspended) < 0) {

        pa_log_error("Invalid packet.");
        pa_module_unload_request(u->module, true);
        return;
    }

    pa_log_debug("Server reports a stream move.");

#ifdef TUNNEL_SINK
    pa_asyncmsgq_send(u->sink->asyncmsgq, PA_MSGOBJECT(u->sink), SINK_MESSAGE_REMOTE_SUSPEND, PA_UINT32_TO_PTR(suspended), 0, NULL);
#else
    pa_asyncmsgq_send(u->source->asyncmsgq, PA_MSGOBJECT(u->source), SOURCE_MESSAGE_REMOTE_SUSPEND, PA_UINT32_TO_PTR(suspended), 0, NULL);
#endif

    request_latency(u);
}

static void command_stream_buffer_attr_changed(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t channel, maxlength, tlength = 0, fragsize, prebuf, minreq;
    pa_usec_t usec;

    pa_assert(pd);
    pa_assert(t);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    if (pa_tagstruct_getu32(t, &channel) < 0 ||
        pa_tagstruct_getu32(t, &maxlength) < 0) {

        pa_log_error("Invalid packet.");
        pa_module_unload_request(u->module, true);
        return;
    }

    if (command == PA_COMMAND_RECORD_BUFFER_ATTR_CHANGED) {
        if (pa_tagstruct_getu32(t, &fragsize) < 0 ||
            pa_tagstruct_get_usec(t, &usec) < 0) {

            pa_log_error("Invalid packet.");
            pa_module_unload_request(u->module, true);
            return;
        }
    } else {
        if (pa_tagstruct_getu32(t, &tlength) < 0 ||
            pa_tagstruct_getu32(t, &prebuf) < 0 ||
            pa_tagstruct_getu32(t, &minreq) < 0 ||
            pa_tagstruct_get_usec(t, &usec) < 0) {

            pa_log_error("Invalid packet.");
            pa_module_unload_request(u->module, true);
            return;
        }
    }

#ifdef TUNNEL_SINK
    pa_log_debug("Server reports buffer attrs changed. tlength now at %lu, before %lu.", (unsigned long) tlength, (unsigned long) u->tlength);
#endif

    request_latency(u);
}

#ifdef TUNNEL_SINK

/* Called from main context */
static void command_started(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(pd);
    pa_assert(t);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    pa_log_debug("Server reports playback started.");
    request_latency(u);
}

#endif

/* Called from IO thread context */
static void check_smoother_status(struct userdata *u, bool past) {
    pa_usec_t x;

    pa_assert(u);

    x = pa_rtclock_now();

    /* Correct by the time the requested issued needs to travel to the
     * other side.  This is a valid thread-safe access, because the
     * main thread is waiting for us */

    if (past)
        x -= u->thread_transport_usec;
    else
        x += u->thread_transport_usec;

    if (u->remote_suspended || u->remote_corked)
        pa_smoother_pause(u->smoother, x);
    else
        pa_smoother_resume(u->smoother, x, true);
}

/* Called from IO thread context */
static void stream_cork_within_thread(struct userdata *u, bool cork) {
    pa_assert(u);

    if (u->remote_corked == cork)
        return;

    u->remote_corked = cork;
    check_smoother_status(u, false);
}

/* Called from main context */
static void stream_cork(struct userdata *u, bool cork) {
    pa_tagstruct *t;
    pa_assert(u);

    if (!u->pstream)
        return;

    t = pa_tagstruct_new(NULL, 0);
#ifdef TUNNEL_SINK
    pa_tagstruct_putu32(t, PA_COMMAND_CORK_PLAYBACK_STREAM);
#else
    pa_tagstruct_putu32(t, PA_COMMAND_CORK_RECORD_STREAM);
#endif
    pa_tagstruct_putu32(t, u->ctag++);
    pa_tagstruct_putu32(t, u->channel);
    pa_tagstruct_put_boolean(t, cork);
    pa_pstream_send_tagstruct(u->pstream, t);

    request_latency(u);
}

/* Called from IO thread context */
static void stream_suspend_within_thread(struct userdata *u, bool suspend) {
    pa_assert(u);

    if (u->remote_suspended == suspend)
        return;

    u->remote_suspended = suspend;
    check_smoother_status(u, true);
}

#ifdef TUNNEL_SINK

/* Called from IO thread context */
static void send_data(struct userdata *u) {
    pa_assert(u);

    while (u->requested_bytes > 0) {
        pa_memchunk memchunk;

        pa_sink_render(u->sink, u->requested_bytes, &memchunk);
        pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->sink), SINK_MESSAGE_POST, NULL, 0, &memchunk, NULL);
        pa_memblock_unref(memchunk.memblock);

        u->requested_bytes -= memchunk.length;

        u->counter += (int64_t) memchunk.length;
    }
}

/* This function is called from IO context -- except when it is not. */
static int sink_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {

        case PA_SINK_MESSAGE_SET_STATE: {
            int r;

            /* First, change the state, because otherwise pa_sink_render() would fail */
            if ((r = pa_sink_process_msg(o, code, data, offset, chunk)) >= 0) {

                stream_cork_within_thread(u, u->sink->thread_info.state == PA_SINK_SUSPENDED);

                if (PA_SINK_IS_OPENED(u->sink->thread_info.state))
                    send_data(u);
            }

            return r;
        }

        case PA_SINK_MESSAGE_GET_LATENCY: {
            pa_usec_t yl, yr, *usec = data;

            yl = pa_bytes_to_usec((uint64_t) u->counter, &u->sink->sample_spec);
            yr = pa_smoother_get(u->smoother, pa_rtclock_now());

            *usec = yl > yr ? yl - yr : 0;
            return 0;
        }

        case SINK_MESSAGE_REQUEST:

            pa_assert(offset > 0);
            u->requested_bytes += (size_t) offset;

            if (PA_SINK_IS_OPENED(u->sink->thread_info.state))
                send_data(u);

            return 0;

        case SINK_MESSAGE_REMOTE_SUSPEND:

            stream_suspend_within_thread(u, !!PA_PTR_TO_UINT(data));
            return 0;

        case SINK_MESSAGE_UPDATE_LATENCY: {
            pa_usec_t y;

            y = pa_bytes_to_usec((uint64_t) u->counter, &u->sink->sample_spec);

            if (y > (pa_usec_t) offset)
                y -= (pa_usec_t) offset;
            else
                y = 0;

            pa_smoother_put(u->smoother, pa_rtclock_now(), y);

            /* We can access this freely here, since the main thread is waiting for us */
            u->thread_transport_usec = u->transport_usec;

            return 0;
        }

        case SINK_MESSAGE_POST:

            /* OK, This might be a bit confusing. This message is
             * delivered to us from the main context -- NOT from the
             * IO thread context where the rest of the messages are
             * dispatched. Yeah, ugly, but I am a lazy bastard. */

            pa_pstream_send_memblock(u->pstream, u->channel, 0, PA_SEEK_RELATIVE, chunk);

            u->counter_delta += (int64_t) chunk->length;

            return 0;
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int sink_set_state(pa_sink *s, pa_sink_state_t state) {
    struct userdata *u;
    pa_sink_assert_ref(s);
    u = s->userdata;

    switch ((pa_sink_state_t) state) {

        case PA_SINK_SUSPENDED:
            pa_assert(PA_SINK_IS_OPENED(s->state));
            stream_cork(u, true);
            break;

        case PA_SINK_IDLE:
        case PA_SINK_RUNNING:
            if (s->state == PA_SINK_SUSPENDED)
                stream_cork(u, false);
            break;

        case PA_SINK_UNLINKED:
        case PA_SINK_INIT:
        case PA_SINK_INVALID_STATE:
            ;
    }

    return 0;
}

#else

/* This function is called from IO context -- except when it is not. */
static int source_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {

        case PA_SOURCE_MESSAGE_SET_STATE: {
            int r;

            if ((r = pa_source_process_msg(o, code, data, offset, chunk)) >= 0)
                stream_cork_within_thread(u, u->source->thread_info.state == PA_SOURCE_SUSPENDED);

            return r;
        }

        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            pa_usec_t yr, yl, *usec = data;

            yl = pa_bytes_to_usec((uint64_t) u->counter, &PA_SOURCE(o)->sample_spec);
            yr = pa_smoother_get(u->smoother, pa_rtclock_now());

            *usec = yr > yl ? yr - yl : 0;
            return 0;
        }

        case SOURCE_MESSAGE_POST: {
            pa_memchunk c;

            pa_mcalign_push(u->mcalign, chunk);

            while (pa_mcalign_pop(u->mcalign, &c) >= 0) {

                if (PA_SOURCE_IS_OPENED(u->source->thread_info.state))
                    pa_source_post(u->source, &c);

                pa_memblock_unref(c.memblock);

                u->counter += (int64_t) c.length;
            }

            return 0;
        }

        case SOURCE_MESSAGE_REMOTE_SUSPEND:

            stream_suspend_within_thread(u, !!PA_PTR_TO_UINT(data));
            return 0;

        case SOURCE_MESSAGE_UPDATE_LATENCY: {
            pa_usec_t y;

            y = pa_bytes_to_usec((uint64_t) u->counter, &u->source->sample_spec);
            y += (pa_usec_t) offset;

            pa_smoother_put(u->smoother, pa_rtclock_now(), y);

            /* We can access this freely here, since the main thread is waiting for us */
            u->thread_transport_usec = u->transport_usec;

            return 0;
        }
    }

    return pa_source_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int source_set_state(pa_source *s, pa_source_state_t state) {
    struct userdata *u;
    pa_source_assert_ref(s);
    u = s->userdata;

    switch ((pa_source_state_t) state) {

        case PA_SOURCE_SUSPENDED:
            pa_assert(PA_SOURCE_IS_OPENED(s->state));
            stream_cork(u, true);
            break;

        case PA_SOURCE_IDLE:
        case PA_SOURCE_RUNNING:
            if (s->state == PA_SOURCE_SUSPENDED)
                stream_cork(u, false);
            break;

        case PA_SOURCE_UNLINKED:
        case PA_SOURCE_INIT:
        case PA_SOURCE_INVALID_STATE:
            ;
    }

    return 0;
}

#endif

static void thread_func(void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);

    pa_log_debug("Thread starting up");

    pa_thread_mq_install(&u->thread_mq);

    for (;;) {
        int ret;

#ifdef TUNNEL_SINK
        if (PA_UNLIKELY(u->sink->thread_info.rewind_requested))
            pa_sink_process_rewind(u->sink, 0);
#endif

        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0)
            goto fail;

        if (ret == 0)
            goto finish;
    }

fail:
    /* If this was no regular exit from the loop we have to continue
     * processing messages until we received PA_MESSAGE_SHUTDOWN */
    pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("Thread shutting down");
}

#ifdef TUNNEL_SINK
/* Called from main context */
static void command_request(pa_pdispatch *pd, uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t bytes, channel;

    pa_assert(pd);
    pa_assert(command == PA_COMMAND_REQUEST);
    pa_assert(t);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    if (pa_tagstruct_getu32(t, &channel) < 0 ||
        pa_tagstruct_getu32(t, &bytes) < 0) {
        pa_log("Invalid protocol reply");
        goto fail;
    }

    if (channel != u->channel) {
        pa_log("Received data for invalid channel");
        goto fail;
    }

    pa_asyncmsgq_post(u->sink->asyncmsgq, PA_MSGOBJECT(u->sink), SINK_MESSAGE_REQUEST, NULL, bytes, NULL, NULL);
    return;

fail:
    pa_module_unload_request(u->module, true);
}

#endif

/* Called from main context */
static void stream_get_latency_callback(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    pa_usec_t sink_usec, source_usec;
    bool playing;
    int64_t write_index, read_index;
    struct timeval local, remote, now;
    pa_sample_spec *ss;
    int64_t delay;

    pa_assert(pd);
    pa_assert(u);

    if (command != PA_COMMAND_REPLY) {
        if (command == PA_COMMAND_ERROR)
            pa_log("Failed to get latency.");
        else
            pa_log("Protocol error.");
        goto fail;
    }

    if (pa_tagstruct_get_usec(t, &sink_usec) < 0 ||
        pa_tagstruct_get_usec(t, &source_usec) < 0 ||
        pa_tagstruct_get_boolean(t, &playing) < 0 ||
        pa_tagstruct_get_timeval(t, &local) < 0 ||
        pa_tagstruct_get_timeval(t, &remote) < 0 ||
        pa_tagstruct_gets64(t, &write_index) < 0 ||
        pa_tagstruct_gets64(t, &read_index) < 0) {
        pa_log("Invalid reply.");
        goto fail;
    }

#ifdef TUNNEL_SINK
    if (u->version >= 13) {
        uint64_t underrun_for = 0, playing_for = 0;

        if (pa_tagstruct_getu64(t, &underrun_for) < 0 ||
            pa_tagstruct_getu64(t, &playing_for) < 0) {
            pa_log("Invalid reply.");
            goto fail;
        }
    }
#endif

    if (!pa_tagstruct_eof(t)) {
        pa_log("Invalid reply.");
        goto fail;
    }

    if (tag < u->ignore_latency_before) {
        return;
    }

    pa_gettimeofday(&now);

    /* Calculate transport usec */
    if (pa_timeval_cmp(&local, &remote) < 0 && pa_timeval_cmp(&remote, &now)) {
        /* local and remote seem to have synchronized clocks */
#ifdef TUNNEL_SINK
        u->transport_usec = pa_timeval_diff(&remote, &local);
#else
        u->transport_usec = pa_timeval_diff(&now, &remote);
#endif
    } else
        u->transport_usec = pa_timeval_diff(&now, &local)/2;

    /* First, take the device's delay */
#ifdef TUNNEL_SINK
    delay = (int64_t) sink_usec;
    ss = &u->sink->sample_spec;
#else
    delay = (int64_t) source_usec;
    ss = &u->source->sample_spec;
#endif

    /* Add the length of our server-side buffer */
    if (write_index >= read_index)
        delay += (int64_t) pa_bytes_to_usec((uint64_t) (write_index-read_index), ss);
    else
        delay -= (int64_t) pa_bytes_to_usec((uint64_t) (read_index-write_index), ss);

    /* Our measurements are already out of date, hence correct by the     *
     * transport latency */
#ifdef TUNNEL_SINK
    delay -= (int64_t) u->transport_usec;
#else
    delay += (int64_t) u->transport_usec;
#endif

    /* Now correct by what we have have read/written since we requested the update */
#ifdef TUNNEL_SINK
    delay += (int64_t) pa_bytes_to_usec((uint64_t) u->counter_delta, ss);
#else
    delay -= (int64_t) pa_bytes_to_usec((uint64_t) u->counter_delta, ss);
#endif

#ifdef TUNNEL_SINK
    pa_asyncmsgq_send(u->sink->asyncmsgq, PA_MSGOBJECT(u->sink), SINK_MESSAGE_UPDATE_LATENCY, 0, delay, NULL);
#else
    pa_asyncmsgq_send(u->source->asyncmsgq, PA_MSGOBJECT(u->source), SOURCE_MESSAGE_UPDATE_LATENCY, 0, delay, NULL);
#endif

    return;

fail:

    pa_module_unload_request(u->module, true);
}

/* Called from main context */
static void request_latency(struct userdata *u) {
    pa_tagstruct *t;
    struct timeval now;
    uint32_t tag;
    pa_assert(u);

    t = pa_tagstruct_new(NULL, 0);
#ifdef TUNNEL_SINK
    pa_tagstruct_putu32(t, PA_COMMAND_GET_PLAYBACK_LATENCY);
#else
    pa_tagstruct_putu32(t, PA_COMMAND_GET_RECORD_LATENCY);
#endif
    pa_tagstruct_putu32(t, tag = u->ctag++);
    pa_tagstruct_putu32(t, u->channel);

    pa_tagstruct_put_timeval(t, pa_gettimeofday(&now));

    pa_pstream_send_tagstruct(u->pstream, t);
    pa_pdispatch_register_reply(u->pdispatch, tag, DEFAULT_TIMEOUT, stream_get_latency_callback, u, NULL);

    u->ignore_latency_before = tag;
    u->counter_delta = 0;
}

/* Called from main context */
static void timeout_callback(pa_mainloop_api *m, pa_time_event *e, const struct timeval *t, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(m);
    pa_assert(e);
    pa_assert(u);

    request_latency(u);

    pa_core_rttime_restart(u->core, e, pa_rtclock_now() + LATENCY_INTERVAL);
}

/* Called from main context */
static void update_description(struct userdata *u) {
    char *d;
    char un[128], hn[128];
    pa_tagstruct *t;

    pa_assert(u);

    if (!u->server_fqdn || !u->user_name || !u->device_description)
        return;

    d = pa_sprintf_malloc("%s on %s@%s", u->device_description, u->user_name, u->server_fqdn);

#ifdef TUNNEL_SINK
    pa_sink_set_description(u->sink, d);
    pa_proplist_sets(u->sink->proplist, "tunnel.remote.user", u->user_name);
    pa_proplist_sets(u->sink->proplist, "tunnel.remote.fqdn", u->server_fqdn);
    pa_proplist_sets(u->sink->proplist, "tunnel.remote.description", u->device_description);
#else
    pa_source_set_description(u->source, d);
    pa_proplist_sets(u->source->proplist, "tunnel.remote.user", u->user_name);
    pa_proplist_sets(u->source->proplist, "tunnel.remote.fqdn", u->server_fqdn);
    pa_proplist_sets(u->source->proplist, "tunnel.remote.description", u->device_description);
#endif

    pa_xfree(d);

    d = pa_sprintf_malloc("%s for %s@%s", u->device_description,
                          pa_get_user_name(un, sizeof(un)),
                          pa_get_host_name(hn, sizeof(hn)));

    t = pa_tagstruct_new(NULL, 0);
#ifdef TUNNEL_SINK
    pa_tagstruct_putu32(t, PA_COMMAND_SET_PLAYBACK_STREAM_NAME);
#else
    pa_tagstruct_putu32(t, PA_COMMAND_SET_RECORD_STREAM_NAME);
#endif
    pa_tagstruct_putu32(t, u->ctag++);
    pa_tagstruct_putu32(t, u->channel);
    pa_tagstruct_puts(t, d);
    pa_pstream_send_tagstruct(u->pstream, t);

    pa_xfree(d);
}

/* Called from main context */
static void server_info_cb(pa_pdispatch *pd, uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    pa_sample_spec ss;
    pa_channel_map cm;
    const char *server_name, *server_version, *user_name, *host_name, *default_sink_name, *default_source_name;
    uint32_t cookie;

    pa_assert(pd);
    pa_assert(u);

    if (command != PA_COMMAND_REPLY) {
        if (command == PA_COMMAND_ERROR)
            pa_log("Failed to get info.");
        else
            pa_log("Protocol error.");
        goto fail;
    }

    if (pa_tagstruct_gets(t, &server_name) < 0 ||
        pa_tagstruct_gets(t, &server_version) < 0 ||
        pa_tagstruct_gets(t, &user_name) < 0 ||
        pa_tagstruct_gets(t, &host_name) < 0 ||
        pa_tagstruct_get_sample_spec(t, &ss) < 0 ||
        pa_tagstruct_gets(t, &default_sink_name) < 0 ||
        pa_tagstruct_gets(t, &default_source_name) < 0 ||
        pa_tagstruct_getu32(t, &cookie) < 0 ||
        (u->version >= 15 && pa_tagstruct_get_channel_map(t, &cm) < 0)) {

        pa_log("Parse failure");
        goto fail;
    }

    if (!pa_tagstruct_eof(t)) {
        pa_log("Packet too long");
        goto fail;
    }

    pa_xfree(u->server_fqdn);
    u->server_fqdn = pa_xstrdup(host_name);

    pa_xfree(u->user_name);
    u->user_name = pa_xstrdup(user_name);

    update_description(u);

    return;

fail:
    pa_module_unload_request(u->module, true);
}

static int read_ports(struct userdata *u, pa_tagstruct *t) {
    if (u->version >= 16) {
        uint32_t n_ports;
        const char *s;

        if (pa_tagstruct_getu32(t, &n_ports)) {
            pa_log("Parse failure");
            return -PA_ERR_PROTOCOL;
        }

        for (uint32_t j = 0; j < n_ports; j++) {
            uint32_t priority;

            if (pa_tagstruct_gets(t, &s) < 0 || /* name */
                pa_tagstruct_gets(t, &s) < 0 || /* description */
                pa_tagstruct_getu32(t, &priority) < 0) {

                pa_log("Parse failure");
                return -PA_ERR_PROTOCOL;
            }
            if (u->version >= 24 && pa_tagstruct_getu32(t, &priority) < 0) { /* available */
                pa_log("Parse failure");
                return -PA_ERR_PROTOCOL;
            }
        }

        if (pa_tagstruct_gets(t, &s) < 0) { /* active port */
            pa_log("Parse failure");
            return -PA_ERR_PROTOCOL;
        }
    }
    return 0;
}

static int read_formats(struct userdata *u, pa_tagstruct *t) {
    uint8_t n_formats;
    pa_format_info *format;

    if (pa_tagstruct_getu8(t, &n_formats) < 0) { /* no. of formats */
        pa_log("Parse failure");
        return -PA_ERR_PROTOCOL;
    }

    for (uint8_t j = 0; j < n_formats; j++) {
        format = pa_format_info_new();
        if (pa_tagstruct_get_format_info(t, format)) { /* format info */
            pa_format_info_free(format);
            pa_log("Parse failure");
            return -PA_ERR_PROTOCOL;
        }
        pa_format_info_free(format);
    }
    return 0;
}

#ifdef TUNNEL_SINK

/* Called from main context */
static void sink_info_cb(pa_pdispatch *pd, uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t idx, owner_module, monitor_source, flags;
    const char *name, *description, *monitor_source_name, *driver;
    pa_sample_spec ss;
    pa_channel_map cm;
    pa_cvolume volume;
    bool mute;
    pa_usec_t latency;

    pa_assert(pd);
    pa_assert(u);

    if (command != PA_COMMAND_REPLY) {
        if (command == PA_COMMAND_ERROR)
            pa_log("Failed to get info.");
        else
            pa_log("Protocol error.");
        goto fail;
    }

    if (pa_tagstruct_getu32(t, &idx) < 0 ||
        pa_tagstruct_gets(t, &name) < 0 ||
        pa_tagstruct_gets(t, &description) < 0 ||
        pa_tagstruct_get_sample_spec(t, &ss) < 0 ||
        pa_tagstruct_get_channel_map(t, &cm) < 0 ||
        pa_tagstruct_getu32(t, &owner_module) < 0 ||
        pa_tagstruct_get_cvolume(t, &volume) < 0 ||
        pa_tagstruct_get_boolean(t, &mute) < 0 ||
        pa_tagstruct_getu32(t, &monitor_source) < 0 ||
        pa_tagstruct_gets(t, &monitor_source_name) < 0 ||
        pa_tagstruct_get_usec(t, &latency) < 0 ||
        pa_tagstruct_gets(t, &driver) < 0 ||
        pa_tagstruct_getu32(t, &flags) < 0) {

        pa_log("Parse failure");
        goto fail;
    }

    if (u->version >= 13) {
        pa_usec_t configured_latency;

        if (pa_tagstruct_get_proplist(t, NULL) < 0 ||
            pa_tagstruct_get_usec(t, &configured_latency) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (u->version >= 15) {
        pa_volume_t base_volume;
        uint32_t state, n_volume_steps, card;

        if (pa_tagstruct_get_volume(t, &base_volume) < 0 ||
            pa_tagstruct_getu32(t, &state) < 0 ||
            pa_tagstruct_getu32(t, &n_volume_steps) < 0 ||
            pa_tagstruct_getu32(t, &card) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (read_ports(u, t) < 0)
        goto fail;

    if (u->version >= 21 && read_formats(u, t) < 0)
        goto fail;

    if (!pa_tagstruct_eof(t)) {
        pa_log("Packet too long");
        goto fail;
    }

    if (!u->sink_name || !pa_streq(name, u->sink_name))
        return;

    pa_xfree(u->device_description);
    u->device_description = pa_xstrdup(description);

    update_description(u);

    return;

fail:
    pa_module_unload_request(u->module, true);
}

/* Called from main context */
static void sink_input_info_cb(pa_pdispatch *pd, uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t idx, owner_module, client, sink;
    pa_usec_t buffer_usec, sink_usec;
    const char *name, *driver, *resample_method;
    bool mute = false;
    pa_sample_spec sample_spec;
    pa_channel_map channel_map;
    pa_cvolume volume;
    bool b;

    pa_assert(pd);
    pa_assert(u);

    if (command != PA_COMMAND_REPLY) {
        if (command == PA_COMMAND_ERROR)
            pa_log("Failed to get info.");
        else
            pa_log("Protocol error.");
        goto fail;
    }

    if (pa_tagstruct_getu32(t, &idx) < 0 ||
        pa_tagstruct_gets(t, &name) < 0 ||
        pa_tagstruct_getu32(t, &owner_module) < 0 ||
        pa_tagstruct_getu32(t, &client) < 0 ||
        pa_tagstruct_getu32(t, &sink) < 0 ||
        pa_tagstruct_get_sample_spec(t, &sample_spec) < 0 ||
        pa_tagstruct_get_channel_map(t, &channel_map) < 0 ||
        pa_tagstruct_get_cvolume(t, &volume) < 0 ||
        pa_tagstruct_get_usec(t, &buffer_usec) < 0 ||
        pa_tagstruct_get_usec(t, &sink_usec) < 0 ||
        pa_tagstruct_gets(t, &resample_method) < 0 ||
        pa_tagstruct_gets(t, &driver) < 0) {

        pa_log("Parse failure");
        goto fail;
    }

    if (u->version >= 11) {
        if (pa_tagstruct_get_boolean(t, &mute) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (u->version >= 13) {
        if (pa_tagstruct_get_proplist(t, NULL) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (u->version >= 19) {
        if (pa_tagstruct_get_boolean(t, &b) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (u->version >= 20) {
        if (pa_tagstruct_get_boolean(t, &b) < 0 ||
            pa_tagstruct_get_boolean(t, &b) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (u->version >= 21) {
        pa_format_info *format = pa_format_info_new();

        if (pa_tagstruct_get_format_info(t, format) < 0) {
            pa_format_info_free(format);
            pa_log("Parse failure");
            goto fail;
        }
        pa_format_info_free(format);
    }

    if (!pa_tagstruct_eof(t)) {
        pa_log("Packet too long");
        goto fail;
    }

    if (idx != u->device_index)
        return;

    pa_assert(u->sink);

    if ((u->version < 11 || mute == u->sink->muted) &&
        pa_cvolume_equal(&volume, &u->sink->real_volume))
        return;

    pa_sink_volume_changed(u->sink, &volume);

    if (u->version >= 11)
        pa_sink_mute_changed(u->sink, mute);

    return;

fail:
    pa_module_unload_request(u->module, true);
}

#else

/* Called from main context */
static void source_info_cb(pa_pdispatch *pd, uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    uint32_t idx, owner_module, monitor_of_sink, flags;
    const char *name, *description, *monitor_of_sink_name, *driver;
    pa_sample_spec ss;
    pa_channel_map cm;
    pa_cvolume volume;
    bool mute;
    pa_usec_t latency, configured_latency;

    pa_assert(pd);
    pa_assert(u);

    if (command != PA_COMMAND_REPLY) {
        if (command == PA_COMMAND_ERROR)
            pa_log("Failed to get info.");
        else
            pa_log("Protocol error.");
        goto fail;
    }

    if (pa_tagstruct_getu32(t, &idx) < 0 ||
        pa_tagstruct_gets(t, &name) < 0 ||
        pa_tagstruct_gets(t, &description) < 0 ||
        pa_tagstruct_get_sample_spec(t, &ss) < 0 ||
        pa_tagstruct_get_channel_map(t, &cm) < 0 ||
        pa_tagstruct_getu32(t, &owner_module) < 0 ||
        pa_tagstruct_get_cvolume(t, &volume) < 0 ||
        pa_tagstruct_get_boolean(t, &mute) < 0 ||
        pa_tagstruct_getu32(t, &monitor_of_sink) < 0 ||
        pa_tagstruct_gets(t, &monitor_of_sink_name) < 0 ||
        pa_tagstruct_get_usec(t, &latency) < 0 ||
        pa_tagstruct_gets(t, &driver) < 0 ||
        pa_tagstruct_getu32(t, &flags) < 0) {

        pa_log("Parse failure");
        goto fail;
    }

    if (u->version >= 13) {
        if (pa_tagstruct_get_proplist(t, NULL) < 0 ||
            pa_tagstruct_get_usec(t, &configured_latency) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (u->version >= 15) {
        pa_volume_t base_volume;
        uint32_t state, n_volume_steps, card;

        if (pa_tagstruct_get_volume(t, &base_volume) < 0 ||
            pa_tagstruct_getu32(t, &state) < 0 ||
            pa_tagstruct_getu32(t, &n_volume_steps) < 0 ||
            pa_tagstruct_getu32(t, &card) < 0) {

            pa_log("Parse failure");
            goto fail;
        }
    }

    if (read_ports(u, t) < 0)
        goto fail;

    if (u->version >= 22 && read_formats(u, t) < 0)
        goto fail;

    if (!pa_tagstruct_eof(t)) {
        pa_log("Packet too long");
        goto fail;
    }

    if (!u->source_name || !pa_streq(name, u->source_name))
        return;

    pa_xfree(u->device_description);
    u->device_description = pa_xstrdup(description);

    update_description(u);

    return;

fail:
    pa_module_unload_request(u->module, true);
}

#endif

/* Called from main context */
static void request_info(struct userdata *u) {
    pa_tagstruct *t;
    uint32_t tag;
    pa_assert(u);

    t = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(t, PA_COMMAND_GET_SERVER_INFO);
    pa_tagstruct_putu32(t, tag = u->ctag++);
    pa_pstream_send_tagstruct(u->pstream, t);
    pa_pdispatch_register_reply(u->pdispatch, tag, DEFAULT_TIMEOUT, server_info_cb, u, NULL);

#ifdef TUNNEL_SINK
    t = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(t, PA_COMMAND_GET_SINK_INPUT_INFO);
    pa_tagstruct_putu32(t, tag = u->ctag++);
    pa_tagstruct_putu32(t, u->device_index);
    pa_pstream_send_tagstruct(u->pstream, t);
    pa_pdispatch_register_reply(u->pdispatch, tag, DEFAULT_TIMEOUT, sink_input_info_cb, u, NULL);

    if (u->sink_name) {
        t = pa_tagstruct_new(NULL, 0);
        pa_tagstruct_putu32(t, PA_COMMAND_GET_SINK_INFO);
        pa_tagstruct_putu32(t, tag = u->ctag++);
        pa_tagstruct_putu32(t, PA_INVALID_INDEX);
        pa_tagstruct_puts(t, u->sink_name);
        pa_pstream_send_tagstruct(u->pstream, t);
        pa_pdispatch_register_reply(u->pdispatch, tag, DEFAULT_TIMEOUT, sink_info_cb, u, NULL);
    }
#else
    if (u->source_name) {
        t = pa_tagstruct_new(NULL, 0);
        pa_tagstruct_putu32(t, PA_COMMAND_GET_SOURCE_INFO);
        pa_tagstruct_putu32(t, tag = u->ctag++);
        pa_tagstruct_putu32(t, PA_INVALID_INDEX);
        pa_tagstruct_puts(t, u->source_name);
        pa_pstream_send_tagstruct(u->pstream, t);
        pa_pdispatch_register_reply(u->pdispatch, tag, DEFAULT_TIMEOUT, source_info_cb, u, NULL);
    }
#endif
}

/* Called from main context */
static void command_subscribe_event(pa_pdispatch *pd,  uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    pa_subscription_event_type_t e;
    uint32_t idx;

    pa_assert(pd);
    pa_assert(t);
    pa_assert(u);
    pa_assert(command == PA_COMMAND_SUBSCRIBE_EVENT);

    if (pa_tagstruct_getu32(t, &e) < 0 ||
        pa_tagstruct_getu32(t, &idx) < 0) {
        pa_log("Invalid protocol reply");
        pa_module_unload_request(u->module, true);
        return;
    }

    if (e != (PA_SUBSCRIPTION_EVENT_SERVER|PA_SUBSCRIPTION_EVENT_CHANGE) &&
#ifdef TUNNEL_SINK
        e != (PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_CHANGE) &&
        e != (PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_CHANGE)
#else
        e != (PA_SUBSCRIPTION_EVENT_SOURCE|PA_SUBSCRIPTION_EVENT_CHANGE)
#endif
        )
        return;

    request_info(u);
}

/* Called from main context */
static void start_subscribe(struct userdata *u) {
    pa_tagstruct *t;
    pa_assert(u);

    t = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(t, PA_COMMAND_SUBSCRIBE);
    pa_tagstruct_putu32(t, u->ctag++);
    pa_tagstruct_putu32(t, PA_SUBSCRIPTION_MASK_SERVER|
#ifdef TUNNEL_SINK
                        PA_SUBSCRIPTION_MASK_SINK_INPUT|PA_SUBSCRIPTION_MASK_SINK
#else
                        PA_SUBSCRIPTION_MASK_SOURCE
#endif
                        );

    pa_pstream_send_tagstruct(u->pstream, t);
}

/* Called from main context */
static void create_stream_callback(pa_pdispatch *pd, uint32_t command,  uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
#ifdef TUNNEL_SINK
    uint32_t bytes;
#endif

    pa_assert(pd);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    if (command != PA_COMMAND_REPLY) {
        if (command == PA_COMMAND_ERROR)
            pa_log("Failed to create stream.");
        else
            pa_log("Protocol error.");
        goto fail;
    }

    if (pa_tagstruct_getu32(t, &u->channel) < 0 ||
        pa_tagstruct_getu32(t, &u->device_index) < 0
#ifdef TUNNEL_SINK
        || pa_tagstruct_getu32(t, &bytes) < 0
#endif
        )
        goto parse_error;

    if (u->version >= 9) {
#ifdef TUNNEL_SINK
        if (pa_tagstruct_getu32(t, &u->maxlength) < 0 ||
            pa_tagstruct_getu32(t, &u->tlength) < 0 ||
            pa_tagstruct_getu32(t, &u->prebuf) < 0 ||
            pa_tagstruct_getu32(t, &u->minreq) < 0)
            goto parse_error;
#else
        if (pa_tagstruct_getu32(t, &u->maxlength) < 0 ||
            pa_tagstruct_getu32(t, &u->fragsize) < 0)
            goto parse_error;
#endif
    }

    if (u->version >= 12) {
        pa_sample_spec ss;
        pa_channel_map cm;
        uint32_t device_index;
        const char *dn;
        bool suspended;

        if (pa_tagstruct_get_sample_spec(t, &ss) < 0 ||
            pa_tagstruct_get_channel_map(t, &cm) < 0 ||
            pa_tagstruct_getu32(t, &device_index) < 0 ||
            pa_tagstruct_gets(t, &dn) < 0 ||
            pa_tagstruct_get_boolean(t, &suspended) < 0)
            goto parse_error;

#ifdef TUNNEL_SINK
        pa_xfree(u->sink_name);
        u->sink_name = pa_xstrdup(dn);
#else
        pa_xfree(u->source_name);
        u->source_name = pa_xstrdup(dn);
#endif
    }

    if (u->version >= 13) {
        pa_usec_t usec;

        if (pa_tagstruct_get_usec(t, &usec) < 0)
            goto parse_error;

/* #ifdef TUNNEL_SINK */
/*         pa_sink_set_latency_range(u->sink, usec + MIN_NETWORK_LATENCY_USEC, 0); */
/* #else */
/*         pa_source_set_latency_range(u->source, usec + MIN_NETWORK_LATENCY_USEC, 0); */
/* #endif */
    }

    if (u->version >= 21) {
        pa_format_info *format = pa_format_info_new();

        if (pa_tagstruct_get_format_info(t, format) < 0) {
            pa_format_info_free(format);
            goto parse_error;
        }

        pa_format_info_free(format);
    }

    if (!pa_tagstruct_eof(t))
        goto parse_error;

    start_subscribe(u);
    request_info(u);

    pa_assert(!u->time_event);
    u->time_event = pa_core_rttime_new(u->core, pa_rtclock_now() + LATENCY_INTERVAL, timeout_callback, u);

    request_latency(u);

    pa_log_debug("Stream created.");

#ifdef TUNNEL_SINK
    pa_asyncmsgq_post(u->sink->asyncmsgq, PA_MSGOBJECT(u->sink), SINK_MESSAGE_REQUEST, NULL, bytes, NULL, NULL);
#endif

    return;

parse_error:
    pa_log("Invalid reply. (Create stream)");

fail:
    pa_module_unload_request(u->module, true);

}

/* Called from main context */
static void setup_complete_callback(pa_pdispatch *pd, uint32_t command, uint32_t tag, pa_tagstruct *t, void *userdata) {
    struct userdata *u = userdata;
    pa_tagstruct *reply;
    char name[256], un[128], hn[128];
    pa_cvolume volume;

    pa_assert(pd);
    pa_assert(u);
    pa_assert(u->pdispatch == pd);

    if (command != PA_COMMAND_REPLY ||
        pa_tagstruct_getu32(t, &u->version) < 0 ||
        !pa_tagstruct_eof(t)) {

        if (command == PA_COMMAND_ERROR)
            pa_log("Failed to authenticate");
        else
            pa_log("Protocol error.");

        goto fail;
    }

    /* Minimum supported protocol version */
    if (u->version < 8) {
        pa_log("Incompatible protocol version");
        goto fail;
    }

    /* Starting with protocol version 13 the MSB of the version tag
    reflects if shm is enabled for this connection or not. We don't
    support SHM here at all, so we just ignore this. */

    if (u->version >= 13)
        u->version &= 0x7FFFFFFFU;

    pa_log_debug("Protocol version: remote %u, local %u", u->version, PA_PROTOCOL_VERSION);

#ifdef TUNNEL_SINK
    pa_proplist_setf(u->sink->proplist, "tunnel.remote_version", "%u", u->version);
    pa_sink_update_proplist(u->sink, 0, NULL);

    pa_snprintf(name, sizeof(name), "%s for %s@%s",
                u->sink_name,
                pa_get_user_name(un, sizeof(un)),
                pa_get_host_name(hn, sizeof(hn)));
#else
    pa_proplist_setf(u->source->proplist, "tunnel.remote_version", "%u", u->version);
    pa_source_update_proplist(u->source, 0, NULL);

    pa_snprintf(name, sizeof(name), "%s for %s@%s",
                u->source_name,
                pa_get_user_name(un, sizeof(un)),
                pa_get_host_name(hn, sizeof(hn)));
#endif

    reply = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(reply, PA_COMMAND_SET_CLIENT_NAME);
    pa_tagstruct_putu32(reply, u->ctag++);

    if (u->version >= 13) {
        pa_proplist *pl;
        pl = pa_proplist_new();
        pa_proplist_sets(pl, PA_PROP_APPLICATION_ID, "org.PulseAudio.PulseAudio");
        pa_proplist_sets(pl, PA_PROP_APPLICATION_VERSION, PACKAGE_VERSION);
        pa_init_proplist(pl);
        pa_tagstruct_put_proplist(reply, pl);
        pa_proplist_free(pl);
    } else
        pa_tagstruct_puts(reply, "PulseAudio");

    pa_pstream_send_tagstruct(u->pstream, reply);
    /* We ignore the server's reply here */

    reply = pa_tagstruct_new(NULL, 0);

    if (u->version < 13)
        /* Only for older PA versions we need to fill in the maxlength */
        u->maxlength = 4*1024*1024;

#ifdef TUNNEL_SINK
    u->tlength = (uint32_t) pa_usec_to_bytes(PA_USEC_PER_MSEC * DEFAULT_TLENGTH_MSEC, &u->sink->sample_spec);
    u->minreq = (uint32_t) pa_usec_to_bytes(PA_USEC_PER_MSEC * DEFAULT_MINREQ_MSEC, &u->sink->sample_spec);
    u->prebuf = u->tlength;
#else
    u->fragsize = (uint32_t) pa_usec_to_bytes(PA_USEC_PER_MSEC * DEFAULT_FRAGSIZE_MSEC, &u->source->sample_spec);
#endif

#ifdef TUNNEL_SINK
    pa_tagstruct_putu32(reply, PA_COMMAND_CREATE_PLAYBACK_STREAM);
    pa_tagstruct_putu32(reply, tag = u->ctag++);

    if (u->version < 13)
        pa_tagstruct_puts(reply, name);

    pa_tagstruct_put_sample_spec(reply, &u->sink->sample_spec);
    pa_tagstruct_put_channel_map(reply, &u->sink->channel_map);
    pa_tagstruct_putu32(reply, PA_INVALID_INDEX);
    pa_tagstruct_puts(reply, u->sink_name);
    pa_tagstruct_putu32(reply, u->maxlength);
    pa_tagstruct_put_boolean(reply, !PA_SINK_IS_OPENED(pa_sink_get_state(u->sink)));
    pa_tagstruct_putu32(reply, u->tlength);
    pa_tagstruct_putu32(reply, u->prebuf);
    pa_tagstruct_putu32(reply, u->minreq);
    pa_tagstruct_putu32(reply, 0);
    pa_cvolume_reset(&volume, u->sink->sample_spec.channels);
    pa_tagstruct_put_cvolume(reply, &volume);
#else
    pa_tagstruct_putu32(reply, PA_COMMAND_CREATE_RECORD_STREAM);
    pa_tagstruct_putu32(reply, tag = u->ctag++);

    if (u->version < 13)
        pa_tagstruct_puts(reply, name);

    pa_tagstruct_put_sample_spec(reply, &u->source->sample_spec);
    pa_tagstruct_put_channel_map(reply, &u->source->channel_map);
    pa_tagstruct_putu32(reply, PA_INVALID_INDEX);
    pa_tagstruct_puts(reply, u->source_name);
    pa_tagstruct_putu32(reply, u->maxlength);
    pa_tagstruct_put_boolean(reply, !PA_SOURCE_IS_OPENED(pa_source_get_state(u->source)));
    pa_tagstruct_putu32(reply, u->fragsize);
#endif

    if (u->version >= 12) {
        pa_tagstruct_put_boolean(reply, false); /* no_remap */
        pa_tagstruct_put_boolean(reply, false); /* no_remix */
        pa_tagstruct_put_boolean(reply, false); /* fix_format */
        pa_tagstruct_put_boolean(reply, false); /* fix_rate */
        pa_tagstruct_put_boolean(reply, false); /* fix_channels */
        pa_tagstruct_put_boolean(reply, true); /* no_move */
        pa_tagstruct_put_boolean(reply, false); /* variable_rate */
    }

    if (u->version >= 13) {
        pa_proplist *pl;

        pa_tagstruct_put_boolean(reply, false); /* start muted/peak detect*/
        pa_tagstruct_put_boolean(reply, true); /* adjust_latency */

        pl = pa_proplist_new();
        pa_proplist_sets(pl, PA_PROP_MEDIA_NAME, name);
        pa_proplist_sets(pl, PA_PROP_MEDIA_ROLE, "abstract");
        pa_tagstruct_put_proplist(reply, pl);
        pa_proplist_free(pl);

#ifndef TUNNEL_SINK
        pa_tagstruct_putu32(reply, PA_INVALID_INDEX); /* direct on input */
#endif
    }

    if (u->version >= 14) {
#ifdef TUNNEL_SINK
        pa_tagstruct_put_boolean(reply, false); /* volume_set */
#endif
        pa_tagstruct_put_boolean(reply, true); /* early rquests */
    }

    if (u->version >= 15) {
#ifdef TUNNEL_SINK
        pa_tagstruct_put_boolean(reply, false); /* muted_set */
#endif
        pa_tagstruct_put_boolean(reply, false); /* don't inhibit auto suspend */
        pa_tagstruct_put_boolean(reply, false); /* fail on suspend */
    }

#ifdef TUNNEL_SINK
    if (u->version >= 17)
        pa_tagstruct_put_boolean(reply, false); /* relative volume */

    if (u->version >= 18)
        pa_tagstruct_put_boolean(reply, false); /* passthrough stream */
#endif

#ifdef TUNNEL_SINK
    if (u->version >= 21) {
        /* We're not using the extended API, so n_formats = 0 and that's that */
        pa_tagstruct_putu8(reply, 0);
    }
#else
    if (u->version >= 22) {
        /* We're not using the extended API, so n_formats = 0 and that's that */
        pa_tagstruct_putu8(reply, 0);
        pa_cvolume_reset(&volume, u->source->sample_spec.channels);
        pa_tagstruct_put_cvolume(reply, &volume);
        pa_tagstruct_put_boolean(reply, false); /* muted */
        pa_tagstruct_put_boolean(reply, false); /* volume_set */
        pa_tagstruct_put_boolean(reply, false); /* muted_set */
        pa_tagstruct_put_boolean(reply, false); /* relative volume */
        pa_tagstruct_put_boolean(reply, false); /* passthrough stream */
    }
#endif

    pa_pstream_send_tagstruct(u->pstream, reply);
    pa_pdispatch_register_reply(u->pdispatch, tag, DEFAULT_TIMEOUT, create_stream_callback, u, NULL);

    pa_log_debug("Connection authenticated, creating stream ...");

    return;

fail:
    pa_module_unload_request(u->module, true);
}

/* Called from main context */
static void pstream_die_callback(pa_pstream *p, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(p);
    pa_assert(u);

    pa_log_warn("Stream died.");
    pa_module_unload_request(u->module, true);
}

/* Called from main context */
static void pstream_packet_callback(pa_pstream *p, pa_packet *packet, const pa_cmsg_ancil_data *ancil_data, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(p);
    pa_assert(packet);
    pa_assert(u);

    if (pa_pdispatch_run(u->pdispatch, packet, ancil_data, u) < 0) {
        pa_log("Invalid packet");
        pa_module_unload_request(u->module, true);
        return;
    }
}

#ifndef TUNNEL_SINK
/* Called from main context */
static void pstream_memblock_callback(pa_pstream *p, uint32_t channel, int64_t offset, pa_seek_mode_t seek, const pa_memchunk *chunk, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(p);
    pa_assert(chunk);
    pa_assert(u);

    if (channel != u->channel) {
        pa_log("Received memory block on bad channel.");
        pa_module_unload_request(u->module, true);
        return;
    }

    pa_asyncmsgq_send(u->source->asyncmsgq, PA_MSGOBJECT(u->source), SOURCE_MESSAGE_POST, PA_UINT_TO_PTR(seek), offset, chunk);

    u->counter_delta += (int64_t) chunk->length;
}
#endif

/* Called from main context */
static void on_connection(pa_socket_client *sc, pa_iochannel *io, void *userdata) {
    struct userdata *u = userdata;
    pa_tagstruct *t;
    uint32_t tag;

    pa_assert(sc);
    pa_assert(u);
    pa_assert(u->client == sc);

    pa_socket_client_unref(u->client);
    u->client = NULL;

    if (!io) {
        pa_log("Connection failed: %s", pa_cstrerror(errno));
        pa_module_unload_request(u->module, true);
        return;
    }

    u->pstream = pa_pstream_new(u->core->mainloop, io, u->core->mempool);
    u->pdispatch = pa_pdispatch_new(u->core->mainloop, true, command_table, PA_COMMAND_MAX);

    pa_pstream_set_die_callback(u->pstream, pstream_die_callback, u);
    pa_pstream_set_receive_packet_callback(u->pstream, pstream_packet_callback, u);
#ifndef TUNNEL_SINK
    pa_pstream_set_receive_memblock_callback(u->pstream, pstream_memblock_callback, u);
#endif

    t = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(t, PA_COMMAND_AUTH);
    pa_tagstruct_putu32(t, tag = u->ctag++);
    pa_tagstruct_putu32(t, PA_PROTOCOL_VERSION);

    pa_tagstruct_put_arbitrary(t, pa_auth_cookie_read(u->auth_cookie, PA_NATIVE_COOKIE_LENGTH), PA_NATIVE_COOKIE_LENGTH);

#ifdef HAVE_CREDS
{
    pa_creds ucred;

    if (pa_iochannel_creds_supported(io))
        pa_iochannel_creds_enable(io);

    ucred.uid = getuid();
    ucred.gid = getgid();

    pa_pstream_send_tagstruct_with_creds(u->pstream, t, &ucred);
}
#else
    pa_pstream_send_tagstruct(u->pstream, t);
#endif

    pa_pdispatch_register_reply(u->pdispatch, tag, DEFAULT_TIMEOUT, setup_complete_callback, u, NULL);

    pa_log_debug("Connection established, authenticating ...");
}

#ifdef TUNNEL_SINK

/* Called from main context */
static void sink_set_volume(pa_sink *sink) {
    struct userdata *u;
    pa_tagstruct *t;

    pa_assert(sink);
    u = sink->userdata;
    pa_assert(u);

    t = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(t, PA_COMMAND_SET_SINK_INPUT_VOLUME);
    pa_tagstruct_putu32(t, u->ctag++);
    pa_tagstruct_putu32(t, u->device_index);
    pa_tagstruct_put_cvolume(t, &sink->real_volume);
    pa_pstream_send_tagstruct(u->pstream, t);
}

/* Called from main context */
static void sink_set_mute(pa_sink *sink) {
    struct userdata *u;
    pa_tagstruct *t;

    pa_assert(sink);
    u = sink->userdata;
    pa_assert(u);

    if (u->version < 11)
        return;

    t = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(t, PA_COMMAND_SET_SINK_INPUT_MUTE);
    pa_tagstruct_putu32(t, u->ctag++);
    pa_tagstruct_putu32(t, u->device_index);
    pa_tagstruct_put_boolean(t, sink->muted);
    pa_pstream_send_tagstruct(u->pstream, t);
}

#endif

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u = NULL;
    char *server = NULL;
    pa_strlist *server_list = NULL;
    pa_sample_spec ss;
    pa_channel_map map;
    char *dn = NULL;
#ifdef TUNNEL_SINK
    pa_sink_new_data data;
#else
    pa_source_new_data data;
#endif
    bool automatic;
#ifdef HAVE_X11
    xcb_connection_t *xcb = NULL;
#endif
    const char *cookie_path;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->client = NULL;
    u->pdispatch = NULL;
    u->pstream = NULL;
    u->server_name = NULL;
#ifdef TUNNEL_SINK
    u->sink_name = pa_xstrdup(pa_modargs_get_value(ma, "sink", NULL));;
    u->sink = NULL;
    u->requested_bytes = 0;
#else
    u->source_name = pa_xstrdup(pa_modargs_get_value(ma, "source", NULL));;
    u->source = NULL;
#endif
    u->smoother = pa_smoother_new(
            PA_USEC_PER_SEC,
            PA_USEC_PER_SEC*2,
            true,
            true,
            10,
            pa_rtclock_now(),
            false);
    u->ctag = 1;
    u->device_index = u->channel = PA_INVALID_INDEX;
    u->time_event = NULL;
    u->ignore_latency_before = 0;
    u->transport_usec = u->thread_transport_usec = 0;
    u->remote_suspended = u->remote_corked = false;
    u->counter = u->counter_delta = 0;

    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    if (pa_modargs_get_value_boolean(ma, "auto", &automatic) < 0) {
        pa_log("Failed to parse argument \"auto\".");
        goto fail;
    }

    cookie_path = pa_modargs_get_value(ma, "cookie", NULL);
    server = pa_xstrdup(pa_modargs_get_value(ma, "server", NULL));

    if (automatic) {
#ifdef HAVE_X11
        /* Need an X11 connection to get root properties */
        if (getenv("DISPLAY") != NULL) {
            if (!(xcb = xcb_connect(getenv("DISPLAY"), NULL)))
                pa_log("xcb_connect() failed");
            else {
                if (xcb_connection_has_error(xcb)) {
                    pa_log("xcb_connection_has_error() returned true");
                    xcb_disconnect(xcb);
                    xcb = NULL;
                }
            }
        }
#endif

        /* Figure out the cookie the same way a normal client would */
        if (!cookie_path)
            cookie_path = getenv(ENV_COOKIE_FILE);

#ifdef HAVE_X11
        if (!cookie_path && xcb) {
            char t[1024];
            if (pa_x11_get_prop(xcb, 0, "PULSE_COOKIE", t, sizeof(t))) {
                uint8_t cookie[PA_NATIVE_COOKIE_LENGTH];

                if (pa_parsehex(t, cookie, sizeof(cookie)) != sizeof(cookie))
                    pa_log("Failed to parse cookie data");
                else {
                    if (!(u->auth_cookie = pa_auth_cookie_create(u->core, cookie, sizeof(cookie))))
                        goto fail;
                }
            }
        }
#endif

        /* Same thing for the server name */
        if (!server)
            server = pa_xstrdup(getenv(ENV_DEFAULT_SERVER));

#ifdef HAVE_X11
        if (!server && xcb) {
            char t[1024];
            if (pa_x11_get_prop(xcb, 0, "PULSE_SERVER", t, sizeof(t)))
                server = pa_xstrdup(t);
        }
#endif

        /* Also determine the default sink/source on the other server */
#ifdef TUNNEL_SINK
        if (!u->sink_name)
            u->sink_name = pa_xstrdup(getenv(ENV_DEFAULT_SINK));

#ifdef HAVE_X11
        if (!u->sink_name && xcb) {
            char t[1024];
            if (pa_x11_get_prop(xcb, 0, "PULSE_SINK", t, sizeof(t)))
                u->sink_name = pa_xstrdup(t);
        }
#endif
#else
        if (!u->source_name)
            u->source_name = pa_xstrdup(getenv(ENV_DEFAULT_SOURCE));

#ifdef HAVE_X11
        if (!u->source_name && xcb) {
            char t[1024];
            if (pa_x11_get_prop(xcb, 0, "PULSE_SOURCE", t, sizeof(t)))
                u->source_name = pa_xstrdup(t);
        }
#endif
#endif
    }

    if (!cookie_path && !u->auth_cookie)
        cookie_path = PA_NATIVE_COOKIE_FILE;

    if (cookie_path) {
        if (!(u->auth_cookie = pa_auth_cookie_get(u->core, cookie_path, true, PA_NATIVE_COOKIE_LENGTH)))
            goto fail;
    }

    if (server) {
        if (!(server_list = pa_strlist_parse(server))) {
            pa_log("Invalid server specified.");
            goto fail;
        }
    } else {
        char *ufn;

        if (!automatic) {
            pa_log("No server specified.");
            goto fail;
        }

        pa_log("No server address found. Attempting default local sockets.");

        /* The system wide instance via PF_LOCAL */
        server_list = pa_strlist_prepend(server_list, PA_SYSTEM_RUNTIME_PATH PA_PATH_SEP PA_NATIVE_DEFAULT_UNIX_SOCKET);

        /* The user instance via PF_LOCAL */
        if ((ufn = pa_runtime_path(PA_NATIVE_DEFAULT_UNIX_SOCKET))) {
            server_list = pa_strlist_prepend(server_list, ufn);
            pa_xfree(ufn);
        }
    }

    ss = m->core->default_sample_spec;
    map = m->core->default_channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification");
        goto fail;
    }

    for (;;) {
        server_list = pa_strlist_pop(server_list, &u->server_name);

        if (!u->server_name) {
            pa_log("Failed to connect to server '%s'", server);
            goto fail;
        }

        pa_log_debug("Trying to connect to %s...", u->server_name);

        if (!(u->client = pa_socket_client_new_string(m->core->mainloop, true, u->server_name, PA_NATIVE_DEFAULT_PORT))) {
            pa_xfree(u->server_name);
            u->server_name = NULL;
            continue;
        }

        break;
     }

    pa_socket_client_set_callback(u->client, on_connection, u);

#ifdef TUNNEL_SINK

    if (!(dn = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", NULL))))
        dn = pa_sprintf_malloc("tunnel-sink.%s", u->server_name);

    pa_sink_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    data.namereg_fail = false;
    pa_sink_new_data_set_name(&data, dn);
    pa_sink_new_data_set_sample_spec(&data, &ss);
    pa_sink_new_data_set_channel_map(&data, &map);
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "%s%s%s", pa_strempty(u->sink_name), u->sink_name ? " on " : "", u->server_name);
    pa_proplist_sets(data.proplist, "tunnel.remote.server", u->server_name);
    if (u->sink_name)
        pa_proplist_sets(data.proplist, "tunnel.remote.sink", u->sink_name);

    if (pa_modargs_get_proplist(ma, "sink_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&data);
        goto fail;
    }

    u->sink = pa_sink_new(m->core, &data, PA_SINK_NETWORK|PA_SINK_LATENCY);
    pa_sink_new_data_done(&data);

    if (!u->sink) {
        pa_log("Failed to create sink.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg;
    u->sink->userdata = u;
    u->sink->set_state = sink_set_state;
    pa_sink_set_set_volume_callback(u->sink, sink_set_volume);
    pa_sink_set_set_mute_callback(u->sink, sink_set_mute);

    u->sink->refresh_volume = u->sink->refresh_muted = false;

/*     pa_sink_set_latency_range(u->sink, MIN_NETWORK_LATENCY_USEC, 0); */

    pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
    pa_sink_set_rtpoll(u->sink, u->rtpoll);

#else

    if (!(dn = pa_xstrdup(pa_modargs_get_value(ma, "source_name", NULL))))
        dn = pa_sprintf_malloc("tunnel-source.%s", u->server_name);

    pa_source_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    data.namereg_fail = false;
    pa_source_new_data_set_name(&data, dn);
    pa_source_new_data_set_sample_spec(&data, &ss);
    pa_source_new_data_set_channel_map(&data, &map);
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "%s%s%s", pa_strempty(u->source_name), u->source_name ? " on " : "", u->server_name);
    pa_proplist_sets(data.proplist, "tunnel.remote.server", u->server_name);
    if (u->source_name)
        pa_proplist_sets(data.proplist, "tunnel.remote.source", u->source_name);

    if (pa_modargs_get_proplist(ma, "source_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_source_new_data_done(&data);
        goto fail;
    }

    u->source = pa_source_new(m->core, &data, PA_SOURCE_NETWORK|PA_SOURCE_LATENCY);
    pa_source_new_data_done(&data);

    if (!u->source) {
        pa_log("Failed to create source.");
        goto fail;
    }

    u->source->parent.process_msg = source_process_msg;
    u->source->set_state = source_set_state;
    u->source->userdata = u;

/*     pa_source_set_latency_range(u->source, MIN_NETWORK_LATENCY_USEC, 0); */

    pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
    pa_source_set_rtpoll(u->source, u->rtpoll);

    u->mcalign = pa_mcalign_new(pa_frame_size(&u->source->sample_spec));
#endif

    pa_xfree(dn);

    u->time_event = NULL;

    u->maxlength = (uint32_t) -1;
#ifdef TUNNEL_SINK
    u->tlength = u->minreq = u->prebuf = (uint32_t) -1;
#else
    u->fragsize = (uint32_t) -1;
#endif

    if (!(u->thread = pa_thread_new("module-tunnel", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

#ifdef TUNNEL_SINK
    pa_sink_put(u->sink);
#else
    pa_source_put(u->source);
#endif

    if (server)
        pa_xfree(server);

    if (server_list)
        pa_strlist_free(server_list);

#ifdef HAVE_X11
    if (xcb)
        xcb_disconnect(xcb);
#endif

    pa_modargs_free(ma);

    return 0;

fail:
    pa__done(m);

    if (server)
        pa_xfree(server);

    if (server_list)
        pa_strlist_free(server_list);

#ifdef HAVE_X11
    if (xcb)
        xcb_disconnect(xcb);
#endif

    if (ma)
        pa_modargs_free(ma);

    pa_xfree(dn);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

#ifdef TUNNEL_SINK
    if (u->sink)
        pa_sink_unlink(u->sink);
#else
    if (u->source)
        pa_source_unlink(u->source);
#endif

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

#ifdef TUNNEL_SINK
    if (u->sink)
        pa_sink_unref(u->sink);
#else
    if (u->source)
        pa_source_unref(u->source);
#endif

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    if (u->pstream) {
        pa_pstream_unlink(u->pstream);
        pa_pstream_unref(u->pstream);
    }

    if (u->pdispatch)
        pa_pdispatch_unref(u->pdispatch);

    if (u->client)
        pa_socket_client_unref(u->client);

    if (u->auth_cookie)
        pa_auth_cookie_unref(u->auth_cookie);

    if (u->smoother)
        pa_smoother_free(u->smoother);

    if (u->time_event)
        u->core->mainloop->time_free(u->time_event);

#ifndef TUNNEL_SINK
    if (u->mcalign)
        pa_mcalign_free(u->mcalign);
#endif

#ifdef TUNNEL_SINK
    pa_xfree(u->sink_name);
#else
    pa_xfree(u->source_name);
#endif
    pa_xfree(u->server_name);

    pa_xfree(u->device_description);
    pa_xfree(u->server_fqdn);
    pa_xfree(u->user_name);

    pa_xfree(u);
}

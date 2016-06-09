/***
  This file is part of PulseAudio.

  Copyright 2004-2008 Lennart Poettering

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
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>

#include <pulsecore/i18n.h>
#include <pulsecore/macro.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>

#include "module-null-sink-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION(_("Clocked NULL sink"));
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink_name=<name of sink> "
        "sink_properties=<properties for the sink> "
        "format=<sample format> "
        "rate=<sample rate> "
        "channels=<number of channels> "
        "channel_map=<channel map>");

#define DEFAULT_SINK_NAME "null"
#define BLOCK_USEC (PA_USEC_PER_SEC * 2)

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_sink *sink;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;

    pa_usec_t block_usec;
    pa_usec_t timestamp;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "format",
    "rate",
    "channels",
    "channel_map",
    NULL
};

static int sink_process_msg(
        pa_msgobject *o,
        int code,
        void *data,
        int64_t offset,
        pa_memchunk *chunk) {

    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {
        case PA_SINK_MESSAGE_SET_STATE:

            if (pa_sink_get_state(u->sink) == PA_SINK_SUSPENDED || pa_sink_get_state(u->sink) == PA_SINK_INIT) {
                if (PA_PTR_TO_UINT(data) == PA_SINK_RUNNING || PA_PTR_TO_UINT(data) == PA_SINK_IDLE)
                    u->timestamp = pa_rtclock_now();
            }

            break;

        case PA_SINK_MESSAGE_GET_LATENCY: {
            pa_usec_t now;

            now = pa_rtclock_now();
            *((pa_usec_t*) data) = u->timestamp > now ? u->timestamp - now : 0ULL;

            return 0;
        }
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;
    size_t nbytes;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    u->block_usec = pa_sink_get_requested_latency_within_thread(s);

    if (u->block_usec == (pa_usec_t) -1)
        u->block_usec = s->thread_info.max_latency;

    nbytes = pa_usec_to_bytes(u->block_usec, &s->sample_spec);
    pa_sink_set_max_rewind_within_thread(s, nbytes);
    pa_sink_set_max_request_within_thread(s, nbytes);
}

static void process_rewind(struct userdata *u, pa_usec_t now) {
    size_t rewind_nbytes, in_buffer;
    pa_usec_t delay;

    pa_assert(u);

    rewind_nbytes = u->sink->thread_info.rewind_nbytes;

    if (!PA_SINK_IS_OPENED(u->sink->thread_info.state) || rewind_nbytes <= 0)
        goto do_nothing;

    pa_log_debug("Requested to rewind %lu bytes.", (unsigned long) rewind_nbytes);

    if (u->timestamp <= now)
        goto do_nothing;

    delay = u->timestamp - now;
    in_buffer = pa_usec_to_bytes(delay, &u->sink->sample_spec);

    if (in_buffer <= 0)
        goto do_nothing;

    if (rewind_nbytes > in_buffer)
        rewind_nbytes = in_buffer;

    pa_sink_process_rewind(u->sink, rewind_nbytes);
    u->timestamp -= pa_bytes_to_usec(rewind_nbytes, &u->sink->sample_spec);

    pa_log_debug("Rewound %lu bytes.", (unsigned long) rewind_nbytes);
    return;

do_nothing:

    pa_sink_process_rewind(u->sink, 0);
}

static void process_render(struct userdata *u, pa_usec_t now) {
    size_t ate = 0;

    pa_assert(u);

    /* This is the configured latency. Sink inputs connected to us
    might not have a single frame more than the maxrequest value
    queued. Hence: at maximum read this many bytes from the sink
    inputs. */

    /* Fill the buffer up the latency size */
    while (u->timestamp < now + u->block_usec) {
        pa_memchunk chunk;

        pa_sink_render(u->sink, u->sink->thread_info.max_request, &chunk);
        pa_memblock_unref(chunk.memblock);

/*         pa_log_debug("Ate %lu bytes.", (unsigned long) chunk.length); */
        u->timestamp += pa_bytes_to_usec(chunk.length, &u->sink->sample_spec);

        ate += chunk.length;

        if (ate >= u->sink->thread_info.max_request)
            break;
    }

/*     pa_log_debug("Ate in sum %lu bytes (of %lu)", (unsigned long) ate, (unsigned long) nbytes); */
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);

    pa_log_debug("Thread starting up");

    pa_thread_mq_install(&u->thread_mq);

    u->timestamp = pa_rtclock_now();

    for (;;) {
        pa_usec_t now = 0;
        int ret;

        if (PA_SINK_IS_OPENED(u->sink->thread_info.state))
            now = pa_rtclock_now();

        if (PA_UNLIKELY(u->sink->thread_info.rewind_requested))
            process_rewind(u, now);

        /* Render some data and drop it immediately */
        if (PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
            if (u->timestamp <= now)
                process_render(u, now);

            pa_rtpoll_set_timer_absolute(u->rtpoll, u->timestamp);
        } else
            pa_rtpoll_set_timer_disabled(u->rtpoll);

        /* Hmm, nothing to do. Let's sleep */
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

int pa__init(pa_module*m) {
    struct userdata *u = NULL;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma = NULL;
    pa_sink_new_data data;
    size_t nbytes;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    ss = m->core->default_sample_spec;
    map = m->core->default_channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll);

    pa_sink_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    pa_sink_new_data_set_name(&data, pa_modargs_get_value(ma, "sink_name", DEFAULT_SINK_NAME));
    pa_sink_new_data_set_sample_spec(&data, &ss);
    pa_sink_new_data_set_channel_map(&data, &map);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_DESCRIPTION, _("Null Output"));
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_CLASS, "abstract");

    if (pa_modargs_get_proplist(ma, "sink_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&data);
        goto fail;
    }

    u->sink = pa_sink_new(m->core, &data, PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY);
    pa_sink_new_data_done(&data);

    if (!u->sink) {
        pa_log("Failed to create sink object.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg;
    u->sink->update_requested_latency = sink_update_requested_latency_cb;
    u->sink->userdata = u;

    pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
    pa_sink_set_rtpoll(u->sink, u->rtpoll);

    u->block_usec = BLOCK_USEC;
    nbytes = pa_usec_to_bytes(u->block_usec, &u->sink->sample_spec);
    pa_sink_set_max_rewind(u->sink, nbytes);
    pa_sink_set_max_request(u->sink, nbytes);

    if (!(u->thread = pa_thread_new("null-sink", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    pa_sink_set_latency_range(u->sink, 0, BLOCK_USEC);

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

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    pa_xfree(u);
}

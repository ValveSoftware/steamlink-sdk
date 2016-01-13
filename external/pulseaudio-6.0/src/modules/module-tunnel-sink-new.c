/***
    This file is part of PulseAudio.

    Copyright 2013 Alexander Couzens

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

#include <pulse/context.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>
#include <pulse/stream.h>
#include <pulse/mainloop.h>
#include <pulse/introspect.h>
#include <pulse/error.h>

#include <pulsecore/core.h>
#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/sink.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/poll.h>
#include <pulsecore/proplist-util.h>

#include "module-tunnel-sink-new-symdef.h"

PA_MODULE_AUTHOR("Alexander Couzens");
PA_MODULE_DESCRIPTION("Create a network sink which connects via a stream to a remote PulseAudio server");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "server=<address> "
        "sink=<name of the remote sink> "
        "sink_name=<name for the local sink> "
        "sink_properties=<properties for the local sink> "
        "format=<sample format> "
        "channels=<number of channels> "
        "rate=<sample rate> "
        "channel_map=<channel map> "
        "cookie=<cookie file path>"
        );

#define MAX_LATENCY_USEC (200 * PA_USEC_PER_MSEC)
#define TUNNEL_THREAD_FAILED_MAINLOOP 1

static void stream_state_cb(pa_stream *stream, void *userdata);
static void stream_changed_buffer_attr_cb(pa_stream *stream, void *userdata);
static void stream_set_buffer_attr_cb(pa_stream *stream, int success, void *userdata);
static void context_state_cb(pa_context *c, void *userdata);
static void sink_update_requested_latency_cb(pa_sink *s);

struct userdata {
    pa_module *module;
    pa_sink *sink;
    pa_thread *thread;
    pa_thread_mq *thread_mq;
    pa_mainloop *thread_mainloop;
    pa_mainloop_api *thread_mainloop_api;

    pa_context *context;
    pa_stream *stream;

    bool update_stream_bufferattr_after_connect;

    bool connected;

    char *cookie_file;
    char *remote_server;
    char *remote_sink_name;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "server",
    "sink",
    "format",
    "channels",
    "rate",
    "channel_map",
    "cookie",
   /* "reconnect", reconnect if server comes back again - unimplemented */
    NULL,
};

static void cork_stream(struct userdata *u, bool cork) {
    pa_operation *operation;

    pa_assert(u);
    pa_assert(u->stream);

    if (cork) {
        /* When the sink becomes suspended (which is the only case where we
         * cork the stream), we don't want to keep any old data around, because
         * the old data is most likely unrelated to the audio that will be
         * played at the time when the sink starts running again. */
        if ((operation = pa_stream_flush(u->stream, NULL, NULL)))
            pa_operation_unref(operation);
    }

    if ((operation = pa_stream_cork(u->stream, cork, NULL, NULL)))
        pa_operation_unref(operation);
}

static void reset_bufferattr(pa_buffer_attr *bufferattr) {
    pa_assert(bufferattr);
    bufferattr->fragsize = (uint32_t) -1;
    bufferattr->minreq = (uint32_t) -1;
    bufferattr->maxlength = (uint32_t) -1;
    bufferattr->prebuf = (uint32_t) -1;
    bufferattr->tlength = (uint32_t) -1;
}

static pa_proplist* tunnel_new_proplist(struct userdata *u) {
    pa_proplist *proplist = pa_proplist_new();
    pa_assert(proplist);
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, "PulseAudio");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID, "org.PulseAudio.PulseAudio");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_VERSION, PACKAGE_VERSION);
    pa_init_proplist(proplist);

    return proplist;
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;
    pa_proplist *proplist;
    pa_assert(u);

    pa_log_debug("Thread starting up");
    pa_thread_mq_install(u->thread_mq);

    proplist = tunnel_new_proplist(u);
    u->context = pa_context_new_with_proplist(u->thread_mainloop_api,
                                              "PulseAudio",
                                              proplist);
    pa_proplist_free(proplist);

    if (!u->context) {
        pa_log("Failed to create libpulse context");
        goto fail;
    }

    if (u->cookie_file && pa_context_load_cookie_from_file(u->context, u->cookie_file) != 0) {
        pa_log_error("Can not load cookie file!");
        goto fail;
    }

    pa_context_set_state_callback(u->context, context_state_cb, u);
    if (pa_context_connect(u->context,
                           u->remote_server,
                           PA_CONTEXT_NOAUTOSPAWN,
                           NULL) < 0) {
        pa_log("Failed to connect libpulse context");
        goto fail;
    }

    for (;;) {
        int ret;

        if (pa_mainloop_iterate(u->thread_mainloop, 1, &ret) < 0) {
            if (ret == 0)
                goto finish;
            else
                goto fail;
        }

        if (PA_UNLIKELY(u->sink->thread_info.rewind_requested))
            pa_sink_process_rewind(u->sink, 0);

        if (u->connected &&
                pa_stream_get_state(u->stream) == PA_STREAM_READY &&
                PA_SINK_IS_LINKED(u->sink->thread_info.state)) {
            size_t writable;

            writable = pa_stream_writable_size(u->stream);
            if (writable > 0) {
                pa_memchunk memchunk;
                const void *p;

                pa_sink_render_full(u->sink, writable, &memchunk);

                pa_assert(memchunk.length > 0);

                /* we have new data to write */
                p = pa_memblock_acquire(memchunk.memblock);
                /* TODO: Use pa_stream_begin_write() to reduce copying. */
                ret = pa_stream_write(u->stream,
                                      (uint8_t*) p + memchunk.index,
                                      memchunk.length,
                                      NULL,     /**< A cleanup routine for the data or NULL to request an internal copy */
                                      0,        /** offset */
                                      PA_SEEK_RELATIVE);
                pa_memblock_release(memchunk.memblock);
                pa_memblock_unref(memchunk.memblock);

                if (ret != 0) {
                    pa_log_error("Could not write data into the stream ... ret = %i", ret);
                    u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
                }

            }
        }
    }
fail:
    pa_asyncmsgq_post(u->thread_mq->outq, PA_MSGOBJECT(u->module->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq->inq, PA_MESSAGE_SHUTDOWN);

finish:
    if (u->stream) {
        pa_stream_disconnect(u->stream);
        pa_stream_unref(u->stream);
        u->stream = NULL;
    }

    if (u->context) {
        pa_context_disconnect(u->context);
        pa_context_unref(u->context);
        u->context = NULL;
    }

    pa_log_debug("Thread shutting down");
}

static void stream_state_cb(pa_stream *stream, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(u);

    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_FAILED:
            pa_log_error("Stream failed.");
            u->connected = false;
            u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
            break;
        case PA_STREAM_TERMINATED:
            pa_log_debug("Stream terminated.");
            break;
        case PA_STREAM_READY:
            if (PA_SINK_IS_OPENED(u->sink->thread_info.state))
                cork_stream(u, false);

            /* Only call our requested_latency_cb when requested_latency
             * changed between PA_STREAM_CREATING -> PA_STREAM_READY, because
             * we don't want to override the initial tlength set by the server
             * without a good reason. */
            if (u->update_stream_bufferattr_after_connect)
                sink_update_requested_latency_cb(u->sink);
            else
                stream_changed_buffer_attr_cb(stream, userdata);
        case PA_STREAM_CREATING:
        case PA_STREAM_UNCONNECTED:
            break;
    }
}

/* called when remote server changes the stream buffer_attr */
static void stream_changed_buffer_attr_cb(pa_stream *stream, void *userdata) {
    struct userdata *u = userdata;
    const pa_buffer_attr *bufferattr;
    pa_assert(u);

    bufferattr = pa_stream_get_buffer_attr(u->stream);
    pa_sink_set_max_request_within_thread(u->sink, bufferattr->tlength);
}

/* called after we requested a change of the stream buffer_attr */
static void stream_set_buffer_attr_cb(pa_stream *stream, int success, void *userdata) {
    stream_changed_buffer_attr_cb(stream, userdata);
}

static void context_state_cb(pa_context *c, void *userdata) {
    struct userdata *u = userdata;
    pa_assert(u);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
        case PA_CONTEXT_READY: {
            pa_proplist *proplist;
            pa_buffer_attr bufferattr;
            pa_usec_t requested_latency;
            char *username = pa_get_user_name_malloc();
            char *hostname = pa_get_host_name_malloc();
            /* TODO: old tunnel put here the remote sink_name into stream name e.g. 'Null Output for lynxis@lazus' */
            char *stream_name = pa_sprintf_malloc(_("Tunnel for %s@%s"), username, hostname);
            pa_xfree(hostname);
            pa_xfree(username);

            pa_log_debug("Connection successful. Creating stream.");
            pa_assert(!u->stream);

            proplist = tunnel_new_proplist(u);
            u->stream = pa_stream_new_with_proplist(u->context,
                                                    stream_name,
                                                    &u->sink->sample_spec,
                                                    &u->sink->channel_map,
                                                    proplist);
            pa_proplist_free(proplist);
            pa_xfree(stream_name);

            if (!u->stream) {
                pa_log_error("Could not create a stream.");
                u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
                return;
            }

            requested_latency = pa_sink_get_requested_latency_within_thread(u->sink);
            if (requested_latency == (pa_usec_t) -1)
                requested_latency = u->sink->thread_info.max_latency;

            reset_bufferattr(&bufferattr);
            bufferattr.tlength = pa_usec_to_bytes(requested_latency, &u->sink->sample_spec);

            pa_stream_set_state_callback(u->stream, stream_state_cb, userdata);
            pa_stream_set_buffer_attr_callback(u->stream, stream_changed_buffer_attr_cb, userdata);
            if (pa_stream_connect_playback(u->stream,
                                           u->remote_sink_name,
                                           &bufferattr,
                                           PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_DONT_MOVE | PA_STREAM_START_CORKED | PA_STREAM_AUTO_TIMING_UPDATE,
                                           NULL,
                                           NULL) < 0) {
                pa_log_error("Could not connect stream.");
                u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
            }
            u->connected = true;
            break;
        }
        case PA_CONTEXT_FAILED:
            pa_log_debug("Context failed: %s.", pa_strerror(pa_context_errno(u->context)));
            u->connected = false;
            u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
            break;
        case PA_CONTEXT_TERMINATED:
            pa_log_debug("Context terminated.");
            u->connected = false;
            u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
            break;
    }
}

static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;
    pa_operation *operation;
    size_t nbytes;
    pa_usec_t block_usec;
    pa_buffer_attr bufferattr;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    block_usec = pa_sink_get_requested_latency_within_thread(s);
    if (block_usec == (pa_usec_t) -1)
        block_usec = s->thread_info.max_latency;

    nbytes = pa_usec_to_bytes(block_usec, &s->sample_spec);
    pa_sink_set_max_request_within_thread(s, nbytes);

    if (u->stream) {
        switch (pa_stream_get_state(u->stream)) {
            case PA_STREAM_READY:
                if (pa_stream_get_buffer_attr(u->stream)->tlength == nbytes)
                    break;

                reset_bufferattr(&bufferattr);
                bufferattr.tlength = nbytes;
                if ((operation = pa_stream_set_buffer_attr(u->stream, &bufferattr, stream_set_buffer_attr_cb, u)))
                    pa_operation_unref(operation);
                break;
            case PA_STREAM_CREATING:
                /* we have to delay our request until stream is ready */
                u->update_stream_bufferattr_after_connect = true;
                break;
            default:
                break;
        }
    }
}

static int sink_process_msg_cb(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {
        case PA_SINK_MESSAGE_GET_LATENCY: {
            int negative;
            pa_usec_t remote_latency;

            if (!PA_SINK_IS_LINKED(u->sink->thread_info.state)) {
                *((pa_usec_t*) data) = 0;
                return 0;
            }

            if (!u->stream) {
                *((pa_usec_t*) data) = 0;
                return 0;
            }

            if (pa_stream_get_state(u->stream) != PA_STREAM_READY) {
                *((pa_usec_t*) data) = 0;
                return 0;
            }

            if (pa_stream_get_latency(u->stream, &remote_latency, &negative) < 0) {
                *((pa_usec_t*) data) = 0;
                return 0;
            }

            *((pa_usec_t*) data) = remote_latency;
            return 0;
        }
        case PA_SINK_MESSAGE_SET_STATE:
            if (!u->stream || pa_stream_get_state(u->stream) != PA_STREAM_READY)
                break;

            switch ((pa_sink_state_t) PA_PTR_TO_UINT(data)) {
                case PA_SINK_SUSPENDED: {
                    cork_stream(u, true);
                    break;
                }
                case PA_SINK_IDLE:
                case PA_SINK_RUNNING: {
                    cork_stream(u, false);
                    break;
                }
                case PA_SINK_INVALID_STATE:
                case PA_SINK_INIT:
                case PA_SINK_UNLINKED:
                    break;
            }
            break;
    }
    return pa_sink_process_msg(o, code, data, offset, chunk);
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_modargs *ma = NULL;
    pa_sink_new_data sink_data;
    pa_sample_spec ss;
    pa_channel_map map;
    const char *remote_server = NULL;
    const char *sink_name = NULL;
    char *default_sink_name = NULL;

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

    remote_server = pa_modargs_get_value(ma, "server", NULL);
    if (!remote_server) {
        pa_log("No server given!");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;
    u->remote_server = pa_xstrdup(remote_server);
    u->thread_mainloop = pa_mainloop_new();
    if (u->thread_mainloop == NULL) {
        pa_log("Failed to create mainloop");
        goto fail;
    }
    u->thread_mainloop_api = pa_mainloop_get_api(u->thread_mainloop);
    u->cookie_file = pa_xstrdup(pa_modargs_get_value(ma, "cookie", NULL));
    u->remote_sink_name = pa_xstrdup(pa_modargs_get_value(ma, "sink", NULL));

    u->thread_mq = pa_xnew0(pa_thread_mq, 1);
    pa_thread_mq_init_thread_mainloop(u->thread_mq, m->core->mainloop, u->thread_mainloop_api);

    /* Create sink */
    pa_sink_new_data_init(&sink_data);
    sink_data.driver = __FILE__;
    sink_data.module = m;

    default_sink_name = pa_sprintf_malloc("tunnel-sink-new.%s", remote_server);
    sink_name = pa_modargs_get_value(ma, "sink_name", default_sink_name);

    pa_sink_new_data_set_name(&sink_data, sink_name);
    pa_sink_new_data_set_sample_spec(&sink_data, &ss);
    pa_sink_new_data_set_channel_map(&sink_data, &map);

    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_CLASS, "sound");
    pa_proplist_setf(sink_data.proplist,
                     PA_PROP_DEVICE_DESCRIPTION,
                     _("Tunnel to %s/%s"),
                     remote_server,
                     pa_strempty(u->remote_sink_name));

    if (pa_modargs_get_proplist(ma, "sink_properties", sink_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&sink_data);
        goto fail;
    }
    if (!(u->sink = pa_sink_new(m->core, &sink_data, PA_SINK_LATENCY | PA_SINK_DYNAMIC_LATENCY | PA_SINK_NETWORK))) {
        pa_log("Failed to create sink.");
        pa_sink_new_data_done(&sink_data);
        goto fail;
    }

    pa_sink_new_data_done(&sink_data);
    u->sink->userdata = u;
    u->sink->parent.process_msg = sink_process_msg_cb;
    u->sink->update_requested_latency = sink_update_requested_latency_cb;
    pa_sink_set_latency_range(u->sink, 0, MAX_LATENCY_USEC);

    /* set thread message queue */
    pa_sink_set_asyncmsgq(u->sink, u->thread_mq->inq);

    if (!(u->thread = pa_thread_new("tunnel-sink", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    pa_sink_put(u->sink);
    pa_modargs_free(ma);
    pa_xfree(default_sink_name);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    if (default_sink_name)
        pa_xfree(default_sink_name);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq->inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    if (u->thread_mq) {
        pa_thread_mq_done(u->thread_mq);
        pa_xfree(u->thread_mq);
    }

    if (u->thread_mainloop)
        pa_mainloop_free(u->thread_mainloop);

    if (u->cookie_file)
        pa_xfree(u->cookie_file);

    if (u->remote_sink_name)
        pa_xfree(u->remote_sink_name);

    if (u->remote_server)
        pa_xfree(u->remote_server);

    if (u->sink)
        pa_sink_unref(u->sink);

    pa_xfree(u);
}

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
#include <pulsecore/source.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/poll.h>
#include <pulsecore/proplist-util.h>

#include "module-tunnel-source-new-symdef.h"

PA_MODULE_AUTHOR("Alexander Couzens");
PA_MODULE_DESCRIPTION("Create a network source which connects via a stream to a remote PulseAudio server");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "server=<address> "
        "source=<name of the remote source> "
        "source_name=<name for the local source> "
        "source_properties=<properties for the local source> "
        "format=<sample format> "
        "channels=<number of channels> "
        "rate=<sample rate> "
        "channel_map=<channel map> "
        "cookie=<cookie file path>"
        );

#define TUNNEL_THREAD_FAILED_MAINLOOP 1

static void stream_state_cb(pa_stream *stream, void *userdata);
static void stream_read_cb(pa_stream *s, size_t length, void *userdata);
static void context_state_cb(pa_context *c, void *userdata);
static void source_update_requested_latency_cb(pa_source *s);

struct userdata {
    pa_module *module;
    pa_source *source;
    pa_thread *thread;
    pa_thread_mq *thread_mq;
    pa_mainloop *thread_mainloop;
    pa_mainloop_api *thread_mainloop_api;

    pa_context *context;
    pa_stream *stream;

    bool update_stream_bufferattr_after_connect;
    bool connected;
    bool new_data;

    char *cookie_file;
    char *remote_server;
    char *remote_source_name;
};

static const char* const valid_modargs[] = {
    "source_name",
    "source_properties",
    "server",
    "source",
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

static void stream_read_cb(pa_stream *s, size_t length, void *userdata) {
    struct userdata *u = userdata;
    u->new_data = true;
}

/* called from io context to read samples from the stream into our source */
static void read_new_samples(struct userdata *u) {
    const void *p;
    size_t readable = 0;
    pa_memchunk memchunk;

    pa_assert(u);
    u->new_data = false;

    pa_memchunk_reset(&memchunk);

    if (PA_UNLIKELY(!u->connected || pa_stream_get_state(u->stream) != PA_STREAM_READY))
        return;

    readable = pa_stream_readable_size(u->stream);
    while (readable > 0) {
        size_t nbytes = 0;
        if (PA_UNLIKELY(pa_stream_peek(u->stream, &p, &nbytes) != 0)) {
            pa_log("pa_stream_peek() failed: %s", pa_strerror(pa_context_errno(u->context)));
            u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
            return;
        }

        if (PA_LIKELY(p)) {
            /* we have valid data */
            memchunk.memblock = pa_memblock_new_fixed(u->module->core->mempool, (void *) p, nbytes, true);
            memchunk.length = nbytes;
            memchunk.index = 0;

            pa_source_post(u->source, &memchunk);
            pa_memblock_unref_fixed(memchunk.memblock);
        } else {
            size_t bytes_to_generate = nbytes;

            /* we have a hole. generate silence */
            memchunk = u->source->silence;
            pa_memblock_ref(memchunk.memblock);

            while (bytes_to_generate > 0) {
                if (bytes_to_generate < memchunk.length)
                    memchunk.length = bytes_to_generate;

                pa_source_post(u->source, &memchunk);
                bytes_to_generate -= memchunk.length;
            }

            pa_memblock_unref(memchunk.memblock);
        }

        pa_stream_drop(u->stream);
        readable -= nbytes;
    }
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
        pa_log("Failed to connect libpulse context: %s", pa_strerror(pa_context_errno(u->context)));
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

        if (u->new_data)
            read_new_samples(u);
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
            pa_log_error("Stream failed: %s", pa_strerror(pa_context_errno(u->context)));
            u->connected = false;
            u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
            break;
        case PA_STREAM_TERMINATED:
            pa_log_debug("Stream terminated.");
            break;
        case PA_STREAM_READY:
            if (PA_SOURCE_IS_OPENED(u->source->thread_info.state))
                cork_stream(u, false);

            /* Only call our requested_latency_cb when requested_latency
             * changed between PA_STREAM_CREATING -> PA_STREAM_READY, because
             * we don't want to override the initial fragsize set by the server
             * without a good reason. */
            if (u->update_stream_bufferattr_after_connect)
                source_update_requested_latency_cb(u->source);
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
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
            /* TODO: old tunnel put here the remote source_name into stream name e.g. 'Null Output for lynxis@lazus' */
            char *stream_name = pa_sprintf_malloc(_("Tunnel for %s@%s"), username, hostname);
            pa_xfree(username);
            pa_xfree(hostname);

            pa_log_debug("Connection successful. Creating stream.");
            pa_assert(!u->stream);

            proplist = tunnel_new_proplist(u);
            u->stream = pa_stream_new_with_proplist(u->context,
                                                    stream_name,
                                                    &u->source->sample_spec,
                                                    &u->source->channel_map,
                                                    proplist);
            pa_proplist_free(proplist);
            pa_xfree(stream_name);

            if (!u->stream) {
                pa_log_error("Could not create a stream: %s", pa_strerror(pa_context_errno(u->context)));
                u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
                return;
            }

            requested_latency = pa_source_get_requested_latency_within_thread(u->source);
            if (requested_latency == (uint32_t) -1)
                requested_latency = u->source->thread_info.max_latency;

            reset_bufferattr(&bufferattr);
            bufferattr.fragsize = pa_usec_to_bytes(requested_latency, &u->source->sample_spec);

            pa_stream_set_state_callback(u->stream, stream_state_cb, userdata);
            pa_stream_set_read_callback(u->stream, stream_read_cb, userdata);
            if (pa_stream_connect_record(u->stream,
                                         u->remote_source_name,
                                         &bufferattr,
                                         PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_DONT_MOVE|PA_STREAM_AUTO_TIMING_UPDATE|PA_STREAM_START_CORKED) < 0) {
                pa_log_debug("Could not create stream: %s", pa_strerror(pa_context_errno(u->context)));
                u->thread_mainloop_api->quit(u->thread_mainloop_api, TUNNEL_THREAD_FAILED_MAINLOOP);
            }
            u->connected = true;
            break;
        }
        case PA_CONTEXT_FAILED:
            pa_log_debug("Context failed with err %s.", pa_strerror(pa_context_errno(u->context)));
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

static void source_update_requested_latency_cb(pa_source *s) {
    struct userdata *u;
    pa_operation *operation;
    size_t nbytes;
    pa_usec_t block_usec;
    pa_buffer_attr bufferattr;

    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);

    block_usec = pa_source_get_requested_latency_within_thread(s);
    if (block_usec == (pa_usec_t) -1)
        block_usec = s->thread_info.max_latency;

    nbytes = pa_usec_to_bytes(block_usec, &s->sample_spec);

    if (u->stream) {
        switch (pa_stream_get_state(u->stream)) {
            case PA_STREAM_READY:
                if (pa_stream_get_buffer_attr(u->stream)->fragsize == nbytes)
                    break;

                reset_bufferattr(&bufferattr);
                bufferattr.fragsize = nbytes;
                if ((operation = pa_stream_set_buffer_attr(u->stream, &bufferattr, NULL, NULL)))
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

static int source_process_msg_cb(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {
        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            int negative;
            pa_usec_t remote_latency;

            if (!PA_SOURCE_IS_LINKED(u->source->thread_info.state)) {
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

            if (negative)
                *((pa_usec_t*) data) = 0;
            else
                *((pa_usec_t*) data) = remote_latency;

            return 0;
        }
        case PA_SOURCE_MESSAGE_SET_STATE:
            if (!u->stream || pa_stream_get_state(u->stream) != PA_STREAM_READY)
                break;

            switch ((pa_source_state_t) PA_PTR_TO_UINT(data)) {
                case PA_SOURCE_SUSPENDED: {
                    cork_stream(u, true);
                    break;
                }
                case PA_SOURCE_IDLE:
                case PA_SOURCE_RUNNING: {
                    cork_stream(u, false);
                    break;
                }
                case PA_SOURCE_INVALID_STATE:
                case PA_SOURCE_INIT:
                case PA_SOURCE_UNLINKED:
                    break;
            }
            break;
    }
    return pa_source_process_msg(o, code, data, offset, chunk);
}

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_modargs *ma = NULL;
    pa_source_new_data source_data;
    pa_sample_spec ss;
    pa_channel_map map;
    const char *remote_server = NULL;
    const char *source_name = NULL;
    char *default_source_name = NULL;

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
    u->remote_source_name = pa_xstrdup(pa_modargs_get_value(ma, "source", NULL));

    u->thread_mq = pa_xnew0(pa_thread_mq, 1);
    pa_thread_mq_init_thread_mainloop(u->thread_mq, m->core->mainloop, u->thread_mainloop_api);

    /* Create source */
    pa_source_new_data_init(&source_data);
    source_data.driver = __FILE__;
    source_data.module = m;

    default_source_name = pa_sprintf_malloc("tunnel-source-new.%s", remote_server);
    source_name = pa_modargs_get_value(ma, "source_name", default_source_name);

    pa_source_new_data_set_name(&source_data, source_name);
    pa_source_new_data_set_sample_spec(&source_data, &ss);
    pa_source_new_data_set_channel_map(&source_data, &map);

    pa_proplist_sets(source_data.proplist, PA_PROP_DEVICE_CLASS, "sound");
    pa_proplist_setf(source_data.proplist,
                     PA_PROP_DEVICE_DESCRIPTION,
                     _("Tunnel to %s/%s"),
                     remote_server,
                     pa_strempty(u->remote_source_name));

    if (pa_modargs_get_proplist(ma, "source_properties", source_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_source_new_data_done(&source_data);
        goto fail;
    }
    if (!(u->source = pa_source_new(m->core, &source_data, PA_SOURCE_LATENCY | PA_SOURCE_DYNAMIC_LATENCY | PA_SOURCE_NETWORK))) {
        pa_log("Failed to create source.");
        pa_source_new_data_done(&source_data);
        goto fail;
    }

    pa_source_new_data_done(&source_data);
    u->source->userdata = u;
    u->source->parent.process_msg = source_process_msg_cb;
    u->source->update_requested_latency = source_update_requested_latency_cb;

    pa_source_set_asyncmsgq(u->source, u->thread_mq->inq);

    if (!(u->thread = pa_thread_new("tunnel-source", thread_func, u))) {
        pa_log("Failed to create thread.");
        goto fail;
    }

    pa_source_put(u->source);
    pa_modargs_free(ma);
    pa_xfree(default_source_name);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    if (default_source_name)
        pa_xfree(default_source_name);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->source)
        pa_source_unlink(u->source);

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

    if (u->remote_source_name)
        pa_xfree(u->remote_source_name);

    if (u->remote_server)
        pa_xfree(u->remote_server);

    if (u->source)
        pa_source_unref(u->source);

    pa_xfree(u);
}

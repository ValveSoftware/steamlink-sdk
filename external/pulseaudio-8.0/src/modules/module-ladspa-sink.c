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

/* TODO: Some plugins cause latency, and some even report it by using a control
   out port. We don't currently use the latency information. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include <pulse/xmalloc.h>

#include <pulsecore/i18n.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/ltdl-helper.h>

#ifdef HAVE_DBUS
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/dbus-util.h>
#endif

#include "module-ladspa-sink-symdef.h"
#include "ladspa.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION(_("Virtual LADSPA sink"));
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
    _("sink_name=<name for the sink> "
      "sink_properties=<properties for the sink> "
      "master=<name of sink to filter> "
      "format=<sample format> "
      "rate=<sample rate> "
      "channels=<number of channels> "
      "channel_map=<input channel map> "
      "plugin=<ladspa plugin name> "
      "label=<ladspa plugin label> "
      "control=<comma separated list of input control values> "
      "input_ladspaport_map=<comma separated list of input LADSPA port names> "
      "output_ladspaport_map=<comma separated list of output LADSPA port names> "));

#define MEMBLOCKQ_MAXLENGTH (16*1024*1024)

/* PLEASE NOTICE: The PortAudio ports and the LADSPA ports are two different concepts.
They are not related and where possible the names of the LADSPA port variables contains "ladspa" to avoid confusion */

struct userdata {
    pa_module *module;

    pa_sink *sink;
    pa_sink_input *sink_input;

    const LADSPA_Descriptor *descriptor;
    LADSPA_Handle handle[PA_CHANNELS_MAX];
    unsigned long max_ladspaport_count, input_count, output_count, channels;
    LADSPA_Data **input, **output;
    size_t block_size;
    LADSPA_Data *control;
    long unsigned n_control;

    /* This is a dummy buffer. Every port must be connected, but we don't care
    about control out ports. We connect them all to this single buffer. */
    LADSPA_Data control_out;

    pa_memblockq *memblockq;

    bool *use_default;
    pa_sample_spec ss;

#ifdef HAVE_DBUS
    pa_dbus_protocol *dbus_protocol;
    char *dbus_path;
#endif

    bool auto_desc;
};

static const char* const valid_modargs[] = {
    "sink_name",
    "sink_properties",
    "master",
    "format",
    "rate",
    "channels",
    "channel_map",
    "plugin",
    "label",
    "control",
    "input_ladspaport_map",
    "output_ladspaport_map",
    NULL
};

/* The PA_SINK_MESSAGE types that extend the predefined messages. */
enum {
   LADSPA_SINK_MESSAGE_UPDATE_PARAMETERS = PA_SINK_MESSAGE_MAX
};

static int write_control_parameters(struct userdata *u, double *control_values, bool *use_default);
static void connect_control_ports(struct userdata *u);

#ifdef HAVE_DBUS

#define LADSPA_IFACE "org.PulseAudio.Ext.Ladspa1"
#define LADSPA_ALGORITHM_PARAMETERS "AlgorithmParameters"

/* TODO: add a PropertyChanged signal to tell that the algorithm parameters have been changed */

enum ladspa_handler_index {
    LADSPA_HANDLER_ALGORITHM_PARAMETERS,
    LADSPA_HANDLER_MAX
};

static void get_algorithm_parameters(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter, struct_iter;
    unsigned long i;
    double *control;
    dbus_bool_t *use_default;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert_se(u = _u);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));
    dbus_message_iter_init_append(reply, &msg_iter);

    dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);

    /* copying because of the D-Bus type mapping */
    control = pa_xnew(double, u->n_control);
    use_default = pa_xnew(dbus_bool_t, u->n_control);

    for (i = 0; i < u->n_control; i++) {
        control[i] = (double) u->control[i];
        use_default[i] = u->use_default[i];
    }

    pa_dbus_append_basic_array(&struct_iter, DBUS_TYPE_DOUBLE, control, u->n_control);
    pa_dbus_append_basic_array(&struct_iter, DBUS_TYPE_BOOLEAN, use_default, u->n_control);

    dbus_message_iter_close_container(&msg_iter, &struct_iter);

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);
    pa_xfree(control);
    pa_xfree(use_default);
}

static void set_algorithm_parameters(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *_u) {
    struct userdata *u;
    DBusMessageIter array_iter, struct_iter;
    int n_control = 0, n_use_default;
    unsigned n_dbus_control, n_dbus_use_default;
    double *read_values = NULL;
    dbus_bool_t *read_defaults = NULL;
    bool *use_defaults = NULL;
    unsigned long i;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert_se(u = _u);

    /* The property we are expecting has signature (adab), meaning that it's a
       struct of two arrays, the first containing doubles and the second containing
       booleans. The first array has the algorithm configuration values and the
       second array has booleans indicating whether the matching algorithm
       configuration value should use (or try to use) the default value provided by
       the algorithm module. The PulseAudio D-Bus infrastructure will take care of
       checking the argument types for us. */

    dbus_message_iter_recurse(iter, &struct_iter);

    dbus_message_iter_recurse(&struct_iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, &read_values, &n_control);

    dbus_message_iter_next(&struct_iter);
    dbus_message_iter_recurse(&struct_iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, &read_defaults, &n_use_default);

    n_dbus_control = n_control; /* handle the unsignedness */
    n_dbus_use_default = n_use_default;

    if (n_dbus_control != u->n_control || n_dbus_use_default != u->n_control) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Wrong number of array values (expected %lu)", u->n_control);
        return;
    }

    use_defaults = pa_xnew(bool, n_control);
    for (i = 0; i < u->n_control; i++)
        use_defaults[i] = read_defaults[i];

    if (write_control_parameters(u, read_values, use_defaults) < 0) {
        pa_log_warn("Failed writing control parameters");
        goto error;
    }

    pa_asyncmsgq_send(u->sink->asyncmsgq, PA_MSGOBJECT(u->sink), LADSPA_SINK_MESSAGE_UPDATE_PARAMETERS, NULL, 0, NULL);

    pa_dbus_send_empty_reply(conn, msg);

    pa_xfree(use_defaults);
    return;

error:
    pa_xfree(use_defaults);
    pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Internal error");
}

static pa_dbus_property_handler ladspa_property_handlers[LADSPA_HANDLER_MAX] = {
    [LADSPA_HANDLER_ALGORITHM_PARAMETERS] = {
        .property_name = LADSPA_ALGORITHM_PARAMETERS,
        .type = "(adab)",
        .get_cb = get_algorithm_parameters,
        .set_cb = set_algorithm_parameters
    }
};

static void ladspa_get_all(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct userdata *u;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter, dict_iter, dict_entry_iter, variant_iter, struct_iter;
    const char *key = LADSPA_ALGORITHM_PARAMETERS;
    double *control;
    dbus_bool_t *use_default;
    long unsigned i;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert_se(u = _u);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    /* Currently, on this interface, only a single property is returned. */

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));
    pa_assert_se(dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter));
    pa_assert_se(dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &key));

    pa_assert_se(dbus_message_iter_open_container(&dict_entry_iter, DBUS_TYPE_VARIANT, "(adab)", &variant_iter));
    pa_assert_se(dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter));

    control = pa_xnew(double, u->n_control);
    use_default = pa_xnew(dbus_bool_t, u->n_control);

    for (i = 0; i < u->n_control; i++) {
        control[i] = (double) u->control[i];
        use_default[i] = u->use_default[i];
    }

    pa_dbus_append_basic_array(&struct_iter, DBUS_TYPE_DOUBLE, control, u->n_control);
    pa_dbus_append_basic_array(&struct_iter, DBUS_TYPE_BOOLEAN, use_default, u->n_control);

    pa_assert_se(dbus_message_iter_close_container(&variant_iter, &struct_iter));
    pa_assert_se(dbus_message_iter_close_container(&dict_entry_iter, &variant_iter));
    pa_assert_se(dbus_message_iter_close_container(&dict_iter, &dict_entry_iter));
    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
    pa_xfree(control);
    pa_xfree(use_default);
}

static pa_dbus_interface_info ladspa_info = {
    .name = LADSPA_IFACE,
    .method_handlers = NULL,
    .n_method_handlers = 0,
    .property_handlers = ladspa_property_handlers,
    .n_property_handlers = LADSPA_HANDLER_MAX,
    .get_all_properties_cb = ladspa_get_all,
    .signals = NULL,
    .n_signals = 0
};

static void dbus_init(struct userdata *u) {
    pa_assert_se(u);

    u->dbus_protocol = pa_dbus_protocol_get(u->sink->core);
    u->dbus_path = pa_sprintf_malloc("/org/pulseaudio/core1/sink%d", u->sink->index);

    pa_dbus_protocol_add_interface(u->dbus_protocol, u->dbus_path, &ladspa_info, u);
}

static void dbus_done(struct userdata *u) {
    pa_assert_se(u);

    if (!u->dbus_protocol) {
        pa_assert(!u->dbus_path);
        return;
    }

    pa_dbus_protocol_remove_interface(u->dbus_protocol, u->dbus_path, ladspa_info.name);
    pa_xfree(u->dbus_path);
    pa_dbus_protocol_unref(u->dbus_protocol);

    u->dbus_path = NULL;
    u->dbus_protocol = NULL;
}

#endif /* HAVE_DBUS */

/* Called from I/O thread context */
static int sink_process_msg_cb(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {

    case PA_SINK_MESSAGE_GET_LATENCY:

        /* The sink is _put() before the sink input is, so let's
         * make sure we don't access it in that time. Also, the
         * sink input is first shut down, the sink second. */
        if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
                !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state)) {
            *((pa_usec_t*) data) = 0;
            return 0;
        }

        *((pa_usec_t*) data) =

            /* Get the latency of the master sink */
            pa_sink_get_latency_within_thread(u->sink_input->sink) +

            /* Add the latency internal to our sink input on top */
            pa_bytes_to_usec(pa_memblockq_get_length(u->sink_input->thread_info.render_memblockq), &u->sink_input->sink->sample_spec);

        return 0;

    case LADSPA_SINK_MESSAGE_UPDATE_PARAMETERS:

        /* rewind the stream to throw away the previously rendered data */

        pa_log_debug("Requesting rewind due to parameter update.");
        pa_sink_request_rewind(u->sink, -1);

        /* change the sink parameters */
        connect_control_ports(u);

        return 0;
    }

    return pa_sink_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int sink_set_state_cb(pa_sink *s, pa_sink_state_t state) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(state) ||
            !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return 0;

    pa_sink_input_cork(u->sink_input, state == PA_SINK_SUSPENDED);
    return 0;
}

/* Called from I/O thread context */
static void sink_request_rewind_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
            !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    /* Just hand this one over to the master sink */
    pa_sink_input_request_rewind(u->sink_input,
                                 s->thread_info.rewind_nbytes +
                                 pa_memblockq_get_length(u->memblockq), true, false, false);
}

/* Called from I/O thread context */
static void sink_update_requested_latency_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
            !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state))
        return;

    /* Just hand this one over to the master sink */
    pa_sink_input_set_requested_latency_within_thread(
        u->sink_input,
        pa_sink_get_requested_latency_within_thread(s));
}

/* Called from main context */
static void sink_set_mute_cb(pa_sink *s) {
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(pa_sink_get_state(s)) ||
            !PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(u->sink_input)))
        return;

    pa_sink_input_set_mute(u->sink_input, s->muted, s->save_muted);
}

/* Called from I/O thread context */
static int sink_input_pop_cb(pa_sink_input *i, size_t nbytes, pa_memchunk *chunk) {
    struct userdata *u;
    float *src, *dst;
    size_t fs;
    unsigned n, h, c;
    pa_memchunk tchunk;

    pa_sink_input_assert_ref(i);
    pa_assert(chunk);
    pa_assert_se(u = i->userdata);

    /* Hmm, process any rewind request that might be queued up */
    pa_sink_process_rewind(u->sink, 0);

    while (pa_memblockq_peek(u->memblockq, &tchunk) < 0) {
        pa_memchunk nchunk;

        pa_sink_render(u->sink, nbytes, &nchunk);
        pa_memblockq_push(u->memblockq, &nchunk);
        pa_memblock_unref(nchunk.memblock);
    }

    tchunk.length = PA_MIN(nbytes, tchunk.length);
    pa_assert(tchunk.length > 0);

    fs = pa_frame_size(&i->sample_spec);
    n = (unsigned) (PA_MIN(tchunk.length, u->block_size) / fs);

    pa_assert(n > 0);

    chunk->index = 0;
    chunk->length = n*fs;
    chunk->memblock = pa_memblock_new(i->sink->core->mempool, chunk->length);

    pa_memblockq_drop(u->memblockq, chunk->length);

    src = pa_memblock_acquire_chunk(&tchunk);
    dst = pa_memblock_acquire(chunk->memblock);

    for (h = 0; h < (u->channels / u->max_ladspaport_count); h++) {
        for (c = 0; c < u->input_count; c++)
            pa_sample_clamp(PA_SAMPLE_FLOAT32NE, u->input[c], sizeof(float), src+ h*u->max_ladspaport_count + c, u->channels*sizeof(float), n);
        u->descriptor->run(u->handle[h], n);
        for (c = 0; c < u->output_count; c++)
            pa_sample_clamp(PA_SAMPLE_FLOAT32NE, dst + h*u->max_ladspaport_count + c, u->channels*sizeof(float), u->output[c], sizeof(float), n);
    }

    pa_memblock_release(tchunk.memblock);
    pa_memblock_release(chunk->memblock);

    pa_memblock_unref(tchunk.memblock);

    return 0;
}

/* Called from I/O thread context */
static void sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;
    size_t amount = 0;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (u->sink->thread_info.rewind_nbytes > 0) {
        size_t max_rewrite;

        max_rewrite = nbytes + pa_memblockq_get_length(u->memblockq);
        amount = PA_MIN(u->sink->thread_info.rewind_nbytes, max_rewrite);
        u->sink->thread_info.rewind_nbytes = 0;

        if (amount > 0) {
            unsigned c;

            pa_memblockq_seek(u->memblockq, - (int64_t) amount, PA_SEEK_RELATIVE, true);

            pa_log_debug("Resetting plugin");

            /* Reset the plugin */
            if (u->descriptor->deactivate)
                for (c = 0; c < (u->channels / u->max_ladspaport_count); c++)
                    u->descriptor->deactivate(u->handle[c]);
            if (u->descriptor->activate)
                for (c = 0; c < (u->channels / u->max_ladspaport_count); c++)
                    u->descriptor->activate(u->handle[c]);
        }
    }

    pa_sink_process_rewind(u->sink, amount);
    pa_memblockq_rewind(u->memblockq, nbytes);
}

/* Called from I/O thread context */
static void sink_input_update_max_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_memblockq_set_maxrewind(u->memblockq, nbytes);
    pa_sink_set_max_rewind_within_thread(u->sink, nbytes);
}

/* Called from I/O thread context */
static void sink_input_update_max_request_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_max_request_within_thread(u->sink, nbytes);
}

/* Called from I/O thread context */
static void sink_input_update_sink_latency_range_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);
}

/* Called from I/O thread context */
static void sink_input_update_sink_fixed_latency_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);
}

/* Called from I/O thread context */
static void sink_input_detach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_detach_within_thread(u->sink);

    pa_sink_set_rtpoll(u->sink, NULL);
}

/* Called from I/O thread context */
static void sink_input_attach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_rtpoll(u->sink, i->sink->thread_info.rtpoll);
    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);
    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);
    pa_sink_set_max_request_within_thread(u->sink, pa_sink_input_get_max_request(i));

    /* FIXME: Too small max_rewind:
     * https://bugs.freedesktop.org/show_bug.cgi?id=53709 */
    pa_sink_set_max_rewind_within_thread(u->sink, pa_sink_input_get_max_rewind(i));

    pa_sink_attach_within_thread(u->sink);
}

/* Called from main context */
static void sink_input_kill_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* The order here matters! We first kill the sink input, followed
     * by the sink. That means the sink callbacks must be protected
     * against an unconnected sink input! */
    pa_sink_input_unlink(u->sink_input);
    pa_sink_unlink(u->sink);

    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;

    pa_sink_unref(u->sink);
    u->sink = NULL;

    pa_module_unload_request(u->module, true);
}

/* Called from IO thread context */
static void sink_input_state_change_cb(pa_sink_input *i, pa_sink_input_state_t state) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* If we are added for the first time, ask for a rewinding so that
     * we are heard right-away. */
    if (PA_SINK_INPUT_IS_LINKED(state) &&
            i->thread_info.state == PA_SINK_INPUT_INIT) {
        pa_log_debug("Requesting rewind due to state change.");
        pa_sink_input_request_rewind(i, 0, false, true, true);
    }
}

/* Called from main context */
static void sink_input_moving_cb(pa_sink_input *i, pa_sink *dest) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (dest) {
        pa_sink_set_asyncmsgq(u->sink, dest->asyncmsgq);
        pa_sink_update_flags(u->sink, PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY, dest->flags);
    } else
        pa_sink_set_asyncmsgq(u->sink, NULL);

    if (u->auto_desc && dest) {
        const char *z;
        pa_proplist *pl;

        pl = pa_proplist_new();
        z = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "LADSPA Plugin %s on %s",
                         pa_proplist_gets(u->sink->proplist, "device.ladspa.name"), z ? z : dest->name);

        pa_sink_update_proplist(u->sink, PA_UPDATE_REPLACE, pl);
        pa_proplist_free(pl);
    }
}

/* Called from main context */
static void sink_input_mute_changed_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_mute_changed(u->sink, i->muted);
}

static int parse_control_parameters(struct userdata *u, const char *cdata, double *read_values, bool *use_default) {
    unsigned long p = 0;
    const char *state = NULL;
    char *k;

    pa_assert(read_values);
    pa_assert(use_default);
    pa_assert(u);

    pa_log_debug("Trying to read %lu control values", u->n_control);

    if (!cdata && u->n_control > 0)
        return -1;

    pa_log_debug("cdata: '%s'", cdata);

    while ((k = pa_split(cdata, ",", &state)) && p < u->n_control) {
        double f;

        if (*k == 0) {
            pa_log_debug("Read empty config value (p=%lu)", p);
            use_default[p++] = true;
            pa_xfree(k);
            continue;
        }

        if (pa_atod(k, &f) < 0) {
            pa_log_debug("Failed to parse control value '%s' (p=%lu)", k, p);
            pa_xfree(k);
            goto fail;
        }

        pa_xfree(k);

        pa_log_debug("Read config value %f (p=%lu)", f, p);

        use_default[p] = false;
        read_values[p++] = f;
    }

    /* The previous loop doesn't take the last control value into account
       if it is left empty, so we do it here. */
    if (*cdata == 0 || cdata[strlen(cdata) - 1] == ',') {
        if (p < u->n_control)
            use_default[p] = true;
        p++;
    }

    if (p > u->n_control || k) {
        pa_log("Too many control values passed, %lu expected.", u->n_control);
        pa_xfree(k);
        goto fail;
    }

    if (p < u->n_control) {
        pa_log("Not enough control values passed, %lu expected, %lu passed.", u->n_control, p);
        goto fail;
    }

    return 0;

fail:
    return -1;
}

static void connect_control_ports(struct userdata *u) {
    unsigned long p = 0, h = 0, c;
    const LADSPA_Descriptor *d;

    pa_assert(u);
    pa_assert_se(d = u->descriptor);

    for (p = 0; p < d->PortCount; p++) {
        if (!LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p]))
            continue;

        if (LADSPA_IS_PORT_OUTPUT(d->PortDescriptors[p])) {
            for (c = 0; c < (u->channels / u->max_ladspaport_count); c++)
                d->connect_port(u->handle[c], p, &u->control_out);
            continue;
        }

        /* input control port */

        pa_log_debug("Binding %f to port %s", u->control[h], d->PortNames[p]);

        for (c = 0; c < (u->channels / u->max_ladspaport_count); c++)
            d->connect_port(u->handle[c], p, &u->control[h]);

        h++;
    }
}

static int validate_control_parameters(struct userdata *u, double *control_values, bool *use_default) {
    unsigned long p = 0, h = 0;
    const LADSPA_Descriptor *d;
    pa_sample_spec ss;

    pa_assert(control_values);
    pa_assert(use_default);
    pa_assert(u);
    pa_assert_se(d = u->descriptor);

    ss = u->ss;

    /* Iterate over all ports. Check for every control port that 1) it
     * supports default values if a default value is provided and 2) the
     * provided value is within the limits specified in the plugin. */

    for (p = 0; p < d->PortCount; p++) {
        LADSPA_PortRangeHintDescriptor hint = d->PortRangeHints[p].HintDescriptor;

        if (!LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p]))
            continue;

        if (LADSPA_IS_PORT_OUTPUT(d->PortDescriptors[p]))
            continue;

        if (use_default[h]) {
            /* User wants to use default value. Check if the plugin
             * provides it. */
            if (!LADSPA_IS_HINT_HAS_DEFAULT(hint)) {
                pa_log_warn("Control port value left empty but plugin defines no default.");
                return -1;
            }
        }
        else {
            /* Check if the user-provided value is within the bounds. */
            LADSPA_Data lower = d->PortRangeHints[p].LowerBound;
            LADSPA_Data upper = d->PortRangeHints[p].UpperBound;

            if (LADSPA_IS_HINT_SAMPLE_RATE(hint)) {
                upper *= (LADSPA_Data) ss.rate;
                lower *= (LADSPA_Data) ss.rate;
            }

            if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint)) {
                if (control_values[h] > upper) {
                    pa_log_warn("Control value %lu over upper bound: %f (upper bound: %f)", h, control_values[h], upper);
                    return -1;
                }
            }
            if (LADSPA_IS_HINT_BOUNDED_BELOW(hint)) {
                if (control_values[h] < lower) {
                    pa_log_warn("Control value %lu below lower bound: %f (lower bound: %f)", h, control_values[h], lower);
                    return -1;
                }
            }
        }

        h++;
    }

    return 0;
}

static int write_control_parameters(struct userdata *u, double *control_values, bool *use_default) {
    unsigned long p = 0, h = 0, c;
    const LADSPA_Descriptor *d;
    pa_sample_spec ss;

    pa_assert(control_values);
    pa_assert(use_default);
    pa_assert(u);
    pa_assert_se(d = u->descriptor);

    ss = u->ss;

    if (validate_control_parameters(u, control_values, use_default) < 0)
        return -1;

    /* p iterates over all ports, h is the control port iterator */

    for (p = 0; p < d->PortCount; p++) {
        LADSPA_PortRangeHintDescriptor hint = d->PortRangeHints[p].HintDescriptor;

        if (!LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p]))
            continue;

        if (LADSPA_IS_PORT_OUTPUT(d->PortDescriptors[p])) {
            for (c = 0; c < (u->channels / u->max_ladspaport_count); c++)
                d->connect_port(u->handle[c], p, &u->control_out);
            continue;
        }

        if (use_default[h]) {

            LADSPA_Data lower, upper;

            lower = d->PortRangeHints[p].LowerBound;
            upper = d->PortRangeHints[p].UpperBound;

            if (LADSPA_IS_HINT_SAMPLE_RATE(hint)) {
                lower *= (LADSPA_Data) ss.rate;
                upper *= (LADSPA_Data) ss.rate;
            }

            switch (hint & LADSPA_HINT_DEFAULT_MASK) {

            case LADSPA_HINT_DEFAULT_MINIMUM:
                u->control[h] = lower;
                break;

            case LADSPA_HINT_DEFAULT_MAXIMUM:
                u->control[h] = upper;
                break;

            case LADSPA_HINT_DEFAULT_LOW:
                if (LADSPA_IS_HINT_LOGARITHMIC(hint))
                    u->control[h] = (LADSPA_Data) exp(log(lower) * 0.75 + log(upper) * 0.25);
                else
                    u->control[h] = (LADSPA_Data) (lower * 0.75 + upper * 0.25);
                break;

            case LADSPA_HINT_DEFAULT_MIDDLE:
                if (LADSPA_IS_HINT_LOGARITHMIC(hint))
                    u->control[h] = (LADSPA_Data) exp(log(lower) * 0.5 + log(upper) * 0.5);
                else
                    u->control[h] = (LADSPA_Data) (lower * 0.5 + upper * 0.5);
                break;

            case LADSPA_HINT_DEFAULT_HIGH:
                if (LADSPA_IS_HINT_LOGARITHMIC(hint))
                    u->control[h] = (LADSPA_Data) exp(log(lower) * 0.25 + log(upper) * 0.75);
                else
                    u->control[h] = (LADSPA_Data) (lower * 0.25 + upper * 0.75);
                break;

            case LADSPA_HINT_DEFAULT_0:
                u->control[h] = 0;
                break;

            case LADSPA_HINT_DEFAULT_1:
                u->control[h] = 1;
                break;

            case LADSPA_HINT_DEFAULT_100:
                u->control[h] = 100;
                break;

            case LADSPA_HINT_DEFAULT_440:
                u->control[h] = 440;
                break;

            default:
                pa_assert_not_reached();
            }
        }
        else {
            if (LADSPA_IS_HINT_INTEGER(hint)) {
                u->control[h] = roundf(control_values[h]);
            }
            else {
                u->control[h] = control_values[h];
            }
        }

        h++;
    }

    /* set the use_default array to the user data */
    memcpy(u->use_default, use_default, u->n_control * sizeof(u->use_default[0]));

    return 0;
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_sample_spec ss;
    pa_channel_map map;
    pa_modargs *ma;
    char *t;
    pa_sink *master;
    pa_sink_input_new_data sink_input_data;
    pa_sink_new_data sink_data;
    const char *plugin, *label, *input_ladspaport_map, *output_ladspaport_map;
    LADSPA_Descriptor_Function descriptor_func;
    unsigned long input_ladspaport[PA_CHANNELS_MAX], output_ladspaport[PA_CHANNELS_MAX];
    const char *e, *cdata;
    const LADSPA_Descriptor *d;
    unsigned long p, h, j, n_control, c;
    pa_memchunk silence;

    pa_assert(m);

    pa_assert_cc(sizeof(LADSPA_Data) == sizeof(float));

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    if (!(master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "master", NULL), PA_NAMEREG_SINK))) {
        pa_log("Master sink not found");
        goto fail;
    }

    ss = master->sample_spec;
    ss.format = PA_SAMPLE_FLOAT32;
    map = master->channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        pa_log("Invalid sample format specification or channel map");
        goto fail;
    }

    if (!(plugin = pa_modargs_get_value(ma, "plugin", NULL))) {
        pa_log("Missing LADSPA plugin name");
        goto fail;
    }

    if (!(label = pa_modargs_get_value(ma, "label", NULL))) {
        pa_log("Missing LADSPA plugin label");
        goto fail;
    }

    if (!(input_ladspaport_map = pa_modargs_get_value(ma, "input_ladspaport_map", NULL)))
        pa_log_debug("Using default input ladspa port mapping");

    if (!(output_ladspaport_map = pa_modargs_get_value(ma, "output_ladspaport_map", NULL)))
        pa_log_debug("Using default output ladspa port mapping");

    cdata = pa_modargs_get_value(ma, "control", NULL);

    u = pa_xnew0(struct userdata, 1);
    u->module = m;
    m->userdata = u;
    u->max_ladspaport_count = 1; /*to avoid division by zero etc. in pa__done when failing before this value has been set*/
    u->channels = 0;
    u->input = NULL;
    u->output = NULL;
    u->ss = ss;

    if (!(e = getenv("LADSPA_PATH")))
        e = LADSPA_PATH;

    /* FIXME: This is not exactly thread safe */
    t = pa_xstrdup(lt_dlgetsearchpath());
    lt_dlsetsearchpath(e);
    m->dl = lt_dlopenext(plugin);
    lt_dlsetsearchpath(t);
    pa_xfree(t);

    if (!m->dl) {
        pa_log("Failed to load LADSPA plugin: %s", lt_dlerror());
        goto fail;
    }

    if (!(descriptor_func = (LADSPA_Descriptor_Function) pa_load_sym(m->dl, NULL, "ladspa_descriptor"))) {
        pa_log("LADSPA module lacks ladspa_descriptor() symbol.");
        goto fail;
    }

    for (j = 0;; j++) {

        if (!(d = descriptor_func(j))) {
            pa_log("Failed to find plugin label '%s' in plugin '%s'.", label, plugin);
            goto fail;
        }

        if (pa_streq(d->Label, label))
            break;
    }

    u->descriptor = d;

    pa_log_debug("Module: %s", plugin);
    pa_log_debug("Label: %s", d->Label);
    pa_log_debug("Unique ID: %lu", d->UniqueID);
    pa_log_debug("Name: %s", d->Name);
    pa_log_debug("Maker: %s", d->Maker);
    pa_log_debug("Copyright: %s", d->Copyright);

    n_control = 0;
    u->channels = ss.channels;

    /*
    * Enumerate ladspa ports
    * Default mapping is in order given by the plugin
    */
    for (p = 0; p < d->PortCount; p++) {
        if (LADSPA_IS_PORT_AUDIO(d->PortDescriptors[p])) {
            if (LADSPA_IS_PORT_INPUT(d->PortDescriptors[p])) {
                pa_log_debug("Port %lu is input: %s", p, d->PortNames[p]);
                input_ladspaport[u->input_count] = p;
                u->input_count++;
            } else if (LADSPA_IS_PORT_OUTPUT(d->PortDescriptors[p])) {
                pa_log_debug("Port %lu is output: %s", p, d->PortNames[p]);
                output_ladspaport[u->output_count] = p;
                u->output_count++;
            }
        } else if (LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p]) && LADSPA_IS_PORT_INPUT(d->PortDescriptors[p])) {
            pa_log_debug("Port %lu is control: %s", p, d->PortNames[p]);
            n_control++;
        } else
            pa_log_debug("Ignored port %s", d->PortNames[p]);
        /* XXX: Has anyone ever seen an in-place plugin with non-equal number of input and output ports? */
        /* Could be if the plugin is for up-mixing stereo to 5.1 channels */
        /* Or if the plugin is down-mixing 5.1 to two channel stereo or binaural encoded signal */
        if (u->input_count > u->max_ladspaport_count)
            u->max_ladspaport_count = u->input_count;
        else
            u->max_ladspaport_count = u->output_count;
    }

    if (u->channels % u->max_ladspaport_count) {
        pa_log("Cannot handle non-integral number of plugins required for given number of channels");
        goto fail;
    }

    pa_log_debug("Will run %lu plugin instances", u->channels / u->max_ladspaport_count);

    /* Parse data for input ladspa port map */
    if (input_ladspaport_map) {
        const char *state = NULL;
        char *pname;
        c = 0;
        while ((pname = pa_split(input_ladspaport_map, ",", &state))) {
            if (c == u->input_count) {
                pa_log("Too many ports in input ladspa port map");
                pa_xfree(pname);
                goto fail;
            }

            for (p = 0; p < d->PortCount; p++) {
                if (pa_streq(d->PortNames[p], pname)) {
                    if (LADSPA_IS_PORT_AUDIO(d->PortDescriptors[p]) && LADSPA_IS_PORT_INPUT(d->PortDescriptors[p])) {
                        input_ladspaport[c] = p;
                    } else {
                        pa_log("Port %s is not an audio input ladspa port", pname);
                        pa_xfree(pname);
                        goto fail;
                    }
                }
            }
            c++;
            pa_xfree(pname);
        }
    }

    /* Parse data for output port map */
    if (output_ladspaport_map) {
        const char *state = NULL;
        char *pname;
        c = 0;
        while ((pname = pa_split(output_ladspaport_map, ",", &state))) {
            if (c == u->output_count) {
                pa_log("Too many ports in output ladspa port map");
                pa_xfree(pname);
                goto fail;
            }
            for (p = 0; p < d->PortCount; p++) {
                if (pa_streq(d->PortNames[p], pname)) {
                    if (LADSPA_IS_PORT_AUDIO(d->PortDescriptors[p]) && LADSPA_IS_PORT_OUTPUT(d->PortDescriptors[p])) {
                        output_ladspaport[c] = p;
                    } else {
                        pa_log("Port %s is not an output ladspa port", pname);
                        pa_xfree(pname);
                        goto fail;
                    }
                }
            }
            c++;
            pa_xfree(pname);
        }
    }

    u->block_size = pa_frame_align(pa_mempool_block_size_max(m->core->mempool), &ss);

    /* Create buffers */
    if (LADSPA_IS_INPLACE_BROKEN(d->Properties)) {
        u->input = (LADSPA_Data**) pa_xnew(LADSPA_Data*, (unsigned) u->input_count);
        for (c = 0; c < u->input_count; c++)
            u->input[c] = (LADSPA_Data*) pa_xnew(uint8_t, (unsigned) u->block_size);
        u->output = (LADSPA_Data**) pa_xnew(LADSPA_Data*, (unsigned) u->output_count);
        for (c = 0; c < u->output_count; c++)
            u->output[c] = (LADSPA_Data*) pa_xnew(uint8_t, (unsigned) u->block_size);
    } else {
        u->input = (LADSPA_Data**) pa_xnew(LADSPA_Data*, (unsigned) u->max_ladspaport_count);
        for (c = 0; c < u->max_ladspaport_count; c++)
            u->input[c] = (LADSPA_Data*) pa_xnew(uint8_t, (unsigned) u->block_size);
        u->output = u->input;
    }
    /* Initialize plugin instances */
    for (h = 0; h < (u->channels / u->max_ladspaport_count); h++) {
        if (!(u->handle[h] = d->instantiate(d, ss.rate))) {
            pa_log("Failed to instantiate plugin %s with label %s", plugin, d->Label);
            goto fail;
        }

        for (c = 0; c < u->input_count; c++)
            d->connect_port(u->handle[h], input_ladspaport[c], u->input[c]);
        for (c = 0; c < u->output_count; c++)
            d->connect_port(u->handle[h], output_ladspaport[c], u->output[c]);
    }

    u->n_control = n_control;

    if (u->n_control > 0) {
        double *control_values;
        bool *use_default;

        /* temporary storage for parser */
        control_values = pa_xnew(double, (unsigned) u->n_control);
        use_default = pa_xnew(bool, (unsigned) u->n_control);

        /* real storage */
        u->control = pa_xnew(LADSPA_Data, (unsigned) u->n_control);
        u->use_default = pa_xnew(bool, (unsigned) u->n_control);

        if ((parse_control_parameters(u, cdata, control_values, use_default) < 0) ||
            (write_control_parameters(u, control_values, use_default) < 0)) {
            pa_xfree(control_values);
            pa_xfree(use_default);

            pa_log("Failed to parse, validate or set control parameters");

            goto fail;
        }
        connect_control_ports(u);
        pa_xfree(control_values);
        pa_xfree(use_default);
    }

    if (d->activate)
        for (c = 0; c < (u->channels / u->max_ladspaport_count); c++)
            d->activate(u->handle[c]);

    /* Create sink */
    pa_sink_new_data_init(&sink_data);
    sink_data.driver = __FILE__;
    sink_data.module = m;
    if (!(sink_data.name = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", NULL))))
        sink_data.name = pa_sprintf_malloc("%s.ladspa", master->name);
    pa_sink_new_data_set_sample_spec(&sink_data, &ss);
    pa_sink_new_data_set_channel_map(&sink_data, &map);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
    pa_proplist_sets(sink_data.proplist, PA_PROP_DEVICE_CLASS, "filter");
    pa_proplist_sets(sink_data.proplist, "device.ladspa.module", plugin);
    pa_proplist_sets(sink_data.proplist, "device.ladspa.label", d->Label);
    pa_proplist_sets(sink_data.proplist, "device.ladspa.name", d->Name);
    pa_proplist_sets(sink_data.proplist, "device.ladspa.maker", d->Maker);
    pa_proplist_sets(sink_data.proplist, "device.ladspa.copyright", d->Copyright);
    pa_proplist_setf(sink_data.proplist, "device.ladspa.unique_id", "%lu", (unsigned long) d->UniqueID);

    if (pa_modargs_get_proplist(ma, "sink_properties", sink_data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_sink_new_data_done(&sink_data);
        goto fail;
    }

    if ((u->auto_desc = !pa_proplist_contains(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *z;

        z = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(sink_data.proplist, PA_PROP_DEVICE_DESCRIPTION, "LADSPA Plugin %s on %s", d->Name, z ? z : master->name);
    }

    u->sink = pa_sink_new(m->core, &sink_data,
                          (master->flags & (PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY)) | PA_SINK_SHARE_VOLUME_WITH_MASTER);
    pa_sink_new_data_done(&sink_data);

    if (!u->sink) {
        pa_log("Failed to create sink.");
        goto fail;
    }

    u->sink->parent.process_msg = sink_process_msg_cb;
    u->sink->set_state = sink_set_state_cb;
    u->sink->update_requested_latency = sink_update_requested_latency_cb;
    u->sink->request_rewind = sink_request_rewind_cb;
    pa_sink_set_set_mute_callback(u->sink, sink_set_mute_cb);
    u->sink->userdata = u;

    pa_sink_set_asyncmsgq(u->sink, master->asyncmsgq);

    /* Create sink input */
    pa_sink_input_new_data_init(&sink_input_data);
    sink_input_data.driver = __FILE__;
    sink_input_data.module = m;
    pa_sink_input_new_data_set_sink(&sink_input_data, master, false);
    sink_input_data.origin_sink = u->sink;
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_NAME, "LADSPA Stream");
    pa_proplist_sets(sink_input_data.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_sink_input_new_data_set_sample_spec(&sink_input_data, &ss);
    pa_sink_input_new_data_set_channel_map(&sink_input_data, &map);

    pa_sink_input_new(&u->sink_input, m->core, &sink_input_data);
    pa_sink_input_new_data_done(&sink_input_data);

    if (!u->sink_input)
        goto fail;

    u->sink_input->pop = sink_input_pop_cb;
    u->sink_input->process_rewind = sink_input_process_rewind_cb;
    u->sink_input->update_max_rewind = sink_input_update_max_rewind_cb;
    u->sink_input->update_max_request = sink_input_update_max_request_cb;
    u->sink_input->update_sink_latency_range = sink_input_update_sink_latency_range_cb;
    u->sink_input->update_sink_fixed_latency = sink_input_update_sink_fixed_latency_cb;
    u->sink_input->kill = sink_input_kill_cb;
    u->sink_input->attach = sink_input_attach_cb;
    u->sink_input->detach = sink_input_detach_cb;
    u->sink_input->state_change = sink_input_state_change_cb;
    u->sink_input->moving = sink_input_moving_cb;
    u->sink_input->mute_changed = sink_input_mute_changed_cb;
    u->sink_input->userdata = u;

    u->sink->input_to_master = u->sink_input;

    pa_sink_input_get_silence(u->sink_input, &silence);
    u->memblockq = pa_memblockq_new("module-ladspa-sink memblockq", 0, MEMBLOCKQ_MAXLENGTH, 0, &ss, 1, 1, 0, &silence);
    pa_memblock_unref(silence.memblock);

    pa_sink_put(u->sink);
    pa_sink_input_put(u->sink_input);

#ifdef HAVE_DBUS
    dbus_init(u);
#endif

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
    unsigned c;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    /* See comments in sink_input_kill_cb() above regarding
    * destruction order! */

#ifdef HAVE_DBUS
    dbus_done(u);
#endif

    if (u->sink_input)
        pa_sink_input_unlink(u->sink_input);

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->sink_input)
        pa_sink_input_unref(u->sink_input);

    if (u->sink)
        pa_sink_unref(u->sink);

    for (c = 0; c < (u->channels / u->max_ladspaport_count); c++) {
        if (u->handle[c]) {
            if (u->descriptor->deactivate)
                u->descriptor->deactivate(u->handle[c]);
            u->descriptor->cleanup(u->handle[c]);
        }
    }

    if (u->output == u->input) {
        if (u->input != NULL) {
            for (c = 0; c < u->max_ladspaport_count; c++)
                pa_xfree(u->input[c]);
            pa_xfree(u->input);
        }
    } else {
        if (u->input != NULL) {
            for (c = 0; c < u->input_count; c++)
                pa_xfree(u->input[c]);
            pa_xfree(u->input);
        }
        if (u->output != NULL) {
            for (c = 0; c < u->output_count; c++)
                pa_xfree(u->output[c]);
            pa_xfree(u->output);
        }
    }

    if (u->memblockq)
        pa_memblockq_free(u->memblockq);

    pa_xfree(u->control);
    pa_xfree(u->use_default);
    pa_xfree(u);
}

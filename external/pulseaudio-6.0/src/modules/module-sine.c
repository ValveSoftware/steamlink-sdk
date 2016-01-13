/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

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

#include <stdio.h>

#include <pulse/xmalloc.h>

#include <pulsecore/sink-input.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/namereg.h>
#include <pulsecore/log.h>

#include "module-sine-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Sine wave generator");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink=<sink to connect to> "
        "rate=<sample rate> "
        "frequency=<frequency in Hz>");

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_sink_input *sink_input;
    pa_memchunk memchunk;
    size_t peek_index;
};

static const char* const valid_modargs[] = {
    "sink",
    "rate",
    "frequency",
    NULL,
};

static int sink_input_pop_cb(pa_sink_input *i, size_t nbytes, pa_memchunk *chunk) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);
    pa_assert(chunk);

    *chunk = u->memchunk;
    pa_memblock_ref(chunk->memblock);

    chunk->index += u->peek_index;
    chunk->length -= u->peek_index;

    u->peek_index = 0;

    return 0;
}

static void sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    nbytes %= u->memchunk.length;

    if (u->peek_index >= nbytes)
        u->peek_index -= nbytes;
    else
        u->peek_index = u->memchunk.length + u->peek_index - nbytes;
}

static void sink_input_kill_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_input_unlink(u->sink_input);
    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;

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
        i->thread_info.state == PA_SINK_INPUT_INIT)
        pa_sink_input_request_rewind(i, 0, false, true, true);
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    pa_sink *sink;
    pa_sample_spec ss;
    uint32_t frequency;
    pa_sink_input_new_data data;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (!(sink = pa_namereg_get(m->core, pa_modargs_get_value(ma, "sink", NULL), PA_NAMEREG_SINK))) {
        pa_log("No such sink.");
        goto fail;
    }

    ss.format = PA_SAMPLE_FLOAT32;
    ss.rate = sink->sample_spec.rate;
    ss.channels = 1;

    if (pa_modargs_get_sample_rate(ma, &ss.rate) < 0) {
        pa_log("Invalid rate specification");
        goto fail;
    }

    frequency = 440;
    if (pa_modargs_get_value_u32(ma, "frequency", &frequency) < 0 || frequency < 1 || frequency > ss.rate/2) {
        pa_log("Invalid frequency specification");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->sink_input = NULL;

    u->peek_index = 0;
    pa_memchunk_sine(&u->memchunk, m->core->mempool, ss.rate, frequency);

    pa_sink_input_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    pa_sink_input_new_data_set_sink(&data, sink, false);
    pa_proplist_setf(data.proplist, PA_PROP_MEDIA_NAME, "%u Hz Sine", frequency);
    pa_proplist_sets(data.proplist, PA_PROP_MEDIA_ROLE, "abstract");
    pa_proplist_setf(data.proplist, "sine.hz", "%u", frequency);
    pa_sink_input_new_data_set_sample_spec(&data, &ss);

    pa_sink_input_new(&u->sink_input, m->core, &data);
    pa_sink_input_new_data_done(&data);

    if (!u->sink_input)
        goto fail;

    u->sink_input->pop = sink_input_pop_cb;
    u->sink_input->process_rewind = sink_input_process_rewind_cb;
    u->sink_input->kill = sink_input_kill_cb;
    u->sink_input->state_change = sink_input_state_change_cb;
    u->sink_input->userdata = u;

    pa_sink_input_put(u->sink_input);

    pa_modargs_free(ma);
    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);
    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sink_input) {
        pa_sink_input_unlink(u->sink_input);
        pa_sink_input_unref(u->sink_input);
    }

    if (u->memchunk.memblock)
        pa_memblock_unref(u->memchunk.memblock);

    pa_xfree(u);
}

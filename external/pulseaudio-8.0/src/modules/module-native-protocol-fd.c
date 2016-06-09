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
#include <unistd.h>

#include <pulsecore/module.h>
#include <pulsecore/macro.h>
#include <pulsecore/iochannel.h>
#include <pulsecore/modargs.h>
#include <pulsecore/protocol-native.h>
#include <pulsecore/log.h>

#include "module-native-protocol-fd-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Native protocol autospawn helper");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(true);

static const char* const valid_modargs[] = {
    "fd",
    NULL,
};

int pa__init(pa_module*m) {
    pa_iochannel *io;
    pa_modargs *ma;
    int32_t fd = -1;
    int r = -1;
    pa_native_options *options = NULL;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto finish;
    }

    if (pa_modargs_get_value_s32(ma, "fd", &fd) < 0 || fd < 0) {
        pa_log("Invalid file descriptor.");
        goto finish;
    }

    m->userdata = pa_native_protocol_get(m->core);

    io = pa_iochannel_new(m->core->mainloop, fd, fd);

    options = pa_native_options_new();
    options->module = m;
    options->auth_anonymous = true;

    pa_native_protocol_connect(m->userdata, io, options);

    r = 0;

finish:
    if (ma)
        pa_modargs_free(ma);

    if (options)
        pa_native_options_unref(options);

    return r;
}

void pa__done(pa_module*m) {
    pa_assert(m);

    if (m->userdata) {
        pa_native_protocol_disconnect(m->userdata, m);
        pa_native_protocol_unref(m->userdata);
    }
}

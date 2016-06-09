/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering
  Copyright 2011 Colin Guthrie

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

#include <pulse/xmalloc.h>

#include <pulsecore/module.h>
#include <pulsecore/log.h>

#include "module-combine-symdef.h"

PA_MODULE_AUTHOR("Colin Guthrie");
PA_MODULE_DESCRIPTION("Compatibility module (module-combine rename)");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_DEPRECATED("Please use module-combine-sink instead of module-combine!");

struct userdata {
    uint32_t module_index;
};

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_module *module;

    pa_assert(m);
    pa_assert_se(m->userdata = u = pa_xnew0(struct userdata, 1));

    pa_log_warn("We will now load module-combine-sink. Please make sure to remove module-combine from your configuration.");

    module = pa_module_load(m->core, "module-combine-sink", m->argument);
    u->module_index = module ? module->index : PA_INVALID_INDEX;

    return module ? 0 : -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert(m->userdata);

    u = m->userdata;

    if (u && PA_INVALID_INDEX != u->module_index)
        pa_module_unload_by_index(m->core, u->module_index, true);

    pa_xfree(u);
}

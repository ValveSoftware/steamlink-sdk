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

#include <unistd.h>
#include <errno.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>

#include "module-esound-compat-spawnfd-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("ESOUND compatibility module: -spawnfd emulation");
PA_MODULE_LOAD_ONCE(true);
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_USAGE("fd=<file descriptor>");

static const char* const valid_modargs[] = {
    "fd",
    NULL,
};

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    int ret = -1;
    int32_t fd = -1;
    char x = 1;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs)) ||
        pa_modargs_get_value_s32(ma, "fd", &fd) < 0 ||
        fd < 0) {

        pa_log("Failed to parse module arguments");
        goto finish;
    }

    if (pa_loop_write(fd, &x, sizeof(x), NULL) != sizeof(x))
        pa_log_warn("write(%u, 1, 1) failed: %s", fd, pa_cstrerror(errno));

    pa_assert_se(pa_close(fd) == 0);

    pa_module_unload_request(m, true);

    ret = 0;

finish:
    if (ma)
        pa_modargs_free(ma);

    return ret;
}

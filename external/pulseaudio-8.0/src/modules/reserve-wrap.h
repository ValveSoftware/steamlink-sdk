#ifndef fooreservewraphfoo
#define fooreservewraphfoo

/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>

typedef struct pa_reserve_wrapper pa_reserve_wrapper;

pa_reserve_wrapper* pa_reserve_wrapper_get(pa_core *c, const char *device_name);
void pa_reserve_wrapper_unref(pa_reserve_wrapper *r);

pa_hook* pa_reserve_wrapper_hook(pa_reserve_wrapper *r);

void pa_reserve_wrapper_set_application_device_name(pa_reserve_wrapper *r, const char *name);

typedef struct pa_reserve_monitor_wrapper pa_reserve_monitor_wrapper;

pa_reserve_monitor_wrapper* pa_reserve_monitor_wrapper_get(pa_core *c, const char *device_name);
void pa_reserve_monitor_wrapper_unref(pa_reserve_monitor_wrapper *m);

pa_hook* pa_reserve_monitor_wrapper_hook(pa_reserve_monitor_wrapper *m);
bool pa_reserve_monitor_wrapper_busy(pa_reserve_monitor_wrapper *m);

#endif

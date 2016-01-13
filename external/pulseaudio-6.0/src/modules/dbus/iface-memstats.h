#ifndef foodbusifacememstatshfoo
#define foodbusifacememstatshfoo

/***
  This file is part of PulseAudio.

  Copyright 2009 Tanu Kaskinen

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

/* This object implements the D-Bus interface org.PulseAudio.Core1.Memstats.
 *
 * See http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/DBus/Memstats/
 * for the Memstats interface documentation.
 */

#include <pulsecore/core.h>
#include <pulsecore/protocol-dbus.h>

#include "iface-core.h"

#define PA_DBUSIFACE_MEMSTATS_INTERFACE PA_DBUS_CORE_INTERFACE ".Memstats"

typedef struct pa_dbusiface_memstats pa_dbusiface_memstats;

pa_dbusiface_memstats *pa_dbusiface_memstats_new(pa_dbusiface_core *dbus_core, pa_core *core);
void pa_dbusiface_memstats_free(pa_dbusiface_memstats *m);

const char *pa_dbusiface_memstats_get_path(pa_dbusiface_memstats *m);

#endif

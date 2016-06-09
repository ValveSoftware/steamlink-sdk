#ifndef foodbusifacedevicehfoo
#define foodbusifacedevicehfoo

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

/* This object implements the D-Bus interfaces org.PulseAudio.Core1.Device,
 * org.PulseAudio.Core1.Sink and org.PulseAudio.Core1.Source.
 *
 * See http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/DBus/Device/
 * for the interface documentation.
 */

#include <pulsecore/protocol-dbus.h>
#include <pulsecore/sink.h>
#include <pulsecore/source.h>

#include "iface-core.h"

#define PA_DBUSIFACE_DEVICE_INTERFACE PA_DBUS_CORE_INTERFACE ".Device"
#define PA_DBUSIFACE_SINK_INTERFACE PA_DBUS_CORE_INTERFACE ".Sink"
#define PA_DBUSIFACE_SOURCE_INTERFACE PA_DBUS_CORE_INTERFACE ".Source"

typedef struct pa_dbusiface_device pa_dbusiface_device;

pa_dbusiface_device *pa_dbusiface_device_new_sink(pa_dbusiface_core *core, pa_sink *sink);
pa_dbusiface_device *pa_dbusiface_device_new_source(pa_dbusiface_core *core, pa_source *source);
void pa_dbusiface_device_free(pa_dbusiface_device *d);

const char *pa_dbusiface_device_get_path(pa_dbusiface_device *d);

pa_sink *pa_dbusiface_device_get_sink(pa_dbusiface_device *d);
pa_source *pa_dbusiface_device_get_source(pa_dbusiface_device *d);

#endif

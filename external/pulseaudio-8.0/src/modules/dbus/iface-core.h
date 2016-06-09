#ifndef foodbusifacecorehfoo
#define foodbusifacecorehfoo

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

/* This object implements the D-Bus interface org.PulseAudio.Core1.
 *
 * See http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/DBus/Core/
 * for the Core interface documentation.
 */

#include <pulsecore/core.h>

typedef struct pa_dbusiface_core pa_dbusiface_core;

pa_dbusiface_core *pa_dbusiface_core_new(pa_core *core);
void pa_dbusiface_core_free(pa_dbusiface_core *c);

const char *pa_dbusiface_core_get_card_path(pa_dbusiface_core *c, const pa_card *card);
const char *pa_dbusiface_core_get_sink_path(pa_dbusiface_core *c, const pa_sink *sink);
const char *pa_dbusiface_core_get_source_path(pa_dbusiface_core *c, const pa_source *source);
const char *pa_dbusiface_core_get_playback_stream_path(pa_dbusiface_core *c, const pa_sink_input *sink_input);
const char *pa_dbusiface_core_get_record_stream_path(pa_dbusiface_core *c, const pa_source_output *source_output);
const char *pa_dbusiface_core_get_module_path(pa_dbusiface_core *c, const pa_module *module);
const char *pa_dbusiface_core_get_client_path(pa_dbusiface_core *c, const pa_client *client);

/* Returns NULL if there's no sink with the given path. */
pa_sink *pa_dbusiface_core_get_sink(pa_dbusiface_core *c, const char *object_path);

/* Returns NULL if there's no source with the given path. */
pa_source *pa_dbusiface_core_get_source(pa_dbusiface_core *c, const char *object_path);

#endif

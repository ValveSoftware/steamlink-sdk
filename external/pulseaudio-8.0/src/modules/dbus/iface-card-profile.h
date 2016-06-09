#ifndef foodbusifacecardprofilehfoo
#define foodbusifacecardprofilehfoo

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

/* This object implements the D-Bus interface org.PulseAudio.Core1.CardProfile.
 *
 * See http://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/DBus/CardProfile/
 * for the CardProfile interface documentation.
 */

#include <pulsecore/protocol-dbus.h>

#include "iface-card.h"

#define PA_DBUSIFACE_CARD_PROFILE_INTERFACE PA_DBUS_CORE_INTERFACE ".CardProfile"

typedef struct pa_dbusiface_card_profile pa_dbusiface_card_profile;

pa_dbusiface_card_profile *pa_dbusiface_card_profile_new(
        pa_dbusiface_card *card,
        pa_core *core,
        pa_card_profile *profile,
        uint32_t idx);
void pa_dbusiface_card_profile_free(pa_dbusiface_card_profile *p);

const char *pa_dbusiface_card_profile_get_path(pa_dbusiface_card_profile *p);
const char *pa_dbusiface_card_profile_get_name(pa_dbusiface_card_profile *p);
pa_card_profile *pa_dbusiface_card_profile_get_profile(pa_dbusiface_card_profile *p);

#endif

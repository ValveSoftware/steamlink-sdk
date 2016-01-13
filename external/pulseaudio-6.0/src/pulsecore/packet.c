/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <pulse/xmalloc.h>
#include <pulsecore/macro.h>

#include "packet.h"

pa_packet* pa_packet_new(size_t length) {
    pa_packet *p;

    pa_assert(length > 0);

    p = pa_xmalloc(PA_ALIGN(sizeof(pa_packet)) + length);
    PA_REFCNT_INIT(p);
    p->length = length;
    p->data = (uint8_t*) p + PA_ALIGN(sizeof(pa_packet));
    p->type = PA_PACKET_APPENDED;

    return p;
}

pa_packet* pa_packet_new_dynamic(void* data, size_t length) {
    pa_packet *p;

    pa_assert(data);
    pa_assert(length > 0);

    p = pa_xnew(pa_packet, 1);
    PA_REFCNT_INIT(p);
    p->length = length;
    p->data = data;
    p->type = PA_PACKET_DYNAMIC;

    return p;
}

pa_packet* pa_packet_ref(pa_packet *p) {
    pa_assert(p);
    pa_assert(PA_REFCNT_VALUE(p) >= 1);

    PA_REFCNT_INC(p);
    return p;
}

void pa_packet_unref(pa_packet *p) {
    pa_assert(p);
    pa_assert(PA_REFCNT_VALUE(p) >= 1);

    if (PA_REFCNT_DEC(p) <= 0) {
        if (p->type == PA_PACKET_DYNAMIC)
            pa_xfree(p->data);
        pa_xfree(p);
    }
}

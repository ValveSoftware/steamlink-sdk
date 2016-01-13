#ifndef foopackethfoo
#define foopackethfoo

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

#include <sys/types.h>
#include <inttypes.h>

#include <pulsecore/refcnt.h>

typedef struct pa_packet {
    PA_REFCNT_DECLARE;
    enum { PA_PACKET_APPENDED, PA_PACKET_DYNAMIC } type;
    size_t length;
    uint8_t *data;
} pa_packet;

pa_packet* pa_packet_new(size_t length);
pa_packet* pa_packet_new_dynamic(void* data, size_t length);

pa_packet* pa_packet_ref(pa_packet *p);
void pa_packet_unref(pa_packet *p);

#endif

#ifndef foosaphfoo
#define foosaphfoo

/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering

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

#include <inttypes.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pulsecore/memblockq.h>
#include <pulsecore/memchunk.h>

typedef struct pa_sap_context {
    int fd;
    char *sdp_data;

    uint16_t msg_id_hash;
} pa_sap_context;

pa_sap_context* pa_sap_context_init_send(pa_sap_context *c, int fd, char *sdp_data);
void pa_sap_context_destroy(pa_sap_context *c);

int pa_sap_send(pa_sap_context *c, bool goodbye);

pa_sap_context* pa_sap_context_init_recv(pa_sap_context *c, int fd);
int pa_sap_recv(pa_sap_context *c, bool *goodbye);

#endif

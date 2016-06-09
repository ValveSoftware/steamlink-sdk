#ifndef foopulseshmhfoo
#define foopulseshmhfoo

/***
  This file is part of PulseAudio.

  Copyright 2006 Lennart Poettering

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

#include <pulsecore/macro.h>

typedef struct pa_shm {
    unsigned id;
    void *ptr;
    size_t size;
    bool do_unlink:1;
    bool shared:1;
} pa_shm;

int pa_shm_create_rw(pa_shm *m, size_t size, bool shared, mode_t mode);
int pa_shm_attach(pa_shm *m, unsigned id, bool writable);

void pa_shm_punch(pa_shm *m, size_t offset, size_t size);

void pa_shm_free(pa_shm *m);

int pa_shm_cleanup(void);

#endif

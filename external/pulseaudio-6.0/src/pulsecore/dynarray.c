/***
  This file is part of PulseAudio.

  Copyright 2004-2008 Lennart Poettering

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

#include <string.h>
#include <stdlib.h>

#include <pulse/xmalloc.h>
#include <pulsecore/macro.h>

#include "dynarray.h"

struct pa_dynarray {
    void **data;
    unsigned n_allocated, n_entries;
    pa_free_cb_t free_cb;
};

pa_dynarray* pa_dynarray_new(pa_free_cb_t free_cb) {
    pa_dynarray *array;

    array = pa_xnew0(pa_dynarray, 1);
    array->free_cb = free_cb;

    return array;
}

void pa_dynarray_free(pa_dynarray *array) {
    unsigned i;
    pa_assert(array);

    if (array->free_cb)
        for (i = 0; i < array->n_entries; i++)
            array->free_cb(array->data[i]);

    pa_xfree(array->data);
    pa_xfree(array);
}

void pa_dynarray_append(pa_dynarray *array, void *p) {
    pa_assert(array);
    pa_assert(p);

    if (array->n_entries == array->n_allocated) {
        unsigned n = PA_MAX(array->n_allocated * 2, 25U);

        array->data = pa_xrealloc(array->data, sizeof(void *) * n);
        array->n_allocated = n;
    }

    array->data[array->n_entries++] = p;
}

void *pa_dynarray_get(pa_dynarray *array, unsigned i) {
    pa_assert(array);
    pa_assert(i < array->n_entries);

    return array->data[i];
}

void *pa_dynarray_steal_last(pa_dynarray *array) {
    pa_assert(array);

    if (array->n_entries > 0)
        return array->data[--array->n_entries];
    else
        return NULL;
}

unsigned pa_dynarray_size(pa_dynarray *array) {
    pa_assert(array);

    return array->n_entries;
}

#ifndef foopulsecoredynarrayhfoo
#define foopulsecoredynarrayhfoo

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

#include <pulse/def.h>

typedef struct pa_dynarray pa_dynarray;

/* Implementation of a simple dynamically sized array for storing pointers.
 *
 * When the array is created, a free callback can be provided, which will be
 * then used when removing items from the array and when freeing the array. If
 * the free callback is not provided, the memory management of the stored items
 * is the responsibility of the array user. If there is need to remove items
 * from the array without freeing them, while also having the free callback
 * set, the functions with "steal" in their name can be used.
 *
 * Removing items from the middle of the array causes the subsequent items to
 * be moved to fill the gap, so it's not efficient with large arrays. If the
 * order of the array is not important, however, functions with "fast" in their
 * name can be used, in which case the gap is filled by moving only the last
 * item(s). XXX: Currently there are no functions with "fast" in their name,
 * but such functions will be added if they are ever needed.
 *
 * The array doesn't support storing NULL pointers. */

pa_dynarray* pa_dynarray_new(pa_free_cb_t free_cb);
void pa_dynarray_free(pa_dynarray *array);

void pa_dynarray_append(pa_dynarray *array, void *p);
void *pa_dynarray_get(pa_dynarray *array, unsigned i);

/* Returns the removed item, or NULL if the array is empty. */
void *pa_dynarray_steal_last(pa_dynarray *array);

unsigned pa_dynarray_size(pa_dynarray *array);

#endif

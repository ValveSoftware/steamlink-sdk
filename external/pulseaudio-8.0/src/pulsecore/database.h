#ifndef foopulsecoredatabasehfoo
#define foopulsecoredatabasehfoo

/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

/* A little abstraction over simple databases, such as gdbm, tdb, and
 * so on. We only make minimal assumptions about the supported
 * backend: it does not need to support locking, it does not have to
 * be arch independent. */

typedef struct pa_database pa_database;

typedef struct pa_datum {
    void *data;
    size_t size;
} pa_datum;

void pa_datum_free(pa_datum *d);

/* This will append a suffix to the filename */
pa_database* pa_database_open(const char *fn, bool for_write);
void pa_database_close(pa_database *db);

pa_datum* pa_database_get(pa_database *db, const pa_datum *key, pa_datum* data);

int pa_database_set(pa_database *db, const pa_datum *key, const pa_datum* data, bool overwrite);
int pa_database_unset(pa_database *db, const pa_datum *key);

int pa_database_clear(pa_database *db);

signed pa_database_size(pa_database *db);

pa_datum* pa_database_first(pa_database *db, pa_datum *key, pa_datum *data /* may be NULL */);
pa_datum* pa_database_next(pa_database *db, const pa_datum *key, pa_datum *next, pa_datum *data /* may be NULL */);

int pa_database_sync(pa_database *db);

#endif

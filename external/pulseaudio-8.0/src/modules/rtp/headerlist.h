#ifndef foopulseheaderlisthfoo
#define foopulseheaderlisthfoo

/***
  This file is part of PulseAudio.

  Copyright 2008 Colin Guthrie
  Copyright 2007 Lennart Poettering

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

#include <pulsecore/macro.h>

typedef struct pa_headerlist pa_headerlist;

pa_headerlist* pa_headerlist_new(void);
void pa_headerlist_free(pa_headerlist* p);

int pa_headerlist_puts(pa_headerlist *p, const char *key, const char *value);
int pa_headerlist_putsappend(pa_headerlist *p, const char *key, const char *value);

const char *pa_headerlist_gets(pa_headerlist *p, const char *key);

int pa_headerlist_remove(pa_headerlist *p, const char *key);

const char *pa_headerlist_iterate(pa_headerlist *p, void **state);

char *pa_headerlist_to_string(pa_headerlist *p);

int pa_headerlist_contains(pa_headerlist *p, const char *key);

#endif

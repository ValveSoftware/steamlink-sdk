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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <pulse/xmalloc.h>

#include <pulsecore/hashmap.h>
#include <pulsecore/strbuf.h>
#include <pulsecore/core-util.h>

#include "headerlist.h"

struct header {
    char *key;
    void *value;
    size_t nbytes;
};

#define MAKE_HASHMAP(p) ((pa_hashmap*) (p))
#define MAKE_HEADERLIST(p) ((pa_headerlist*) (p))

static void header_free(struct header *hdr) {
    pa_assert(hdr);

    pa_xfree(hdr->key);
    pa_xfree(hdr->value);
    pa_xfree(hdr);
}

pa_headerlist* pa_headerlist_new(void) {
    return MAKE_HEADERLIST(pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) header_free));
}

void pa_headerlist_free(pa_headerlist* p) {
    pa_hashmap_free(MAKE_HASHMAP(p));
}

int pa_headerlist_puts(pa_headerlist *p, const char *key, const char *value) {
    struct header *hdr;
    bool add = false;

    pa_assert(p);
    pa_assert(key);

    if (!(hdr = pa_hashmap_get(MAKE_HASHMAP(p), key))) {
        hdr = pa_xnew(struct header, 1);
        hdr->key = pa_xstrdup(key);
        add = true;
    } else
        pa_xfree(hdr->value);

    hdr->value = pa_xstrdup(value);
    hdr->nbytes = strlen(value)+1;

    if (add)
        pa_hashmap_put(MAKE_HASHMAP(p), hdr->key, hdr);

    return 0;
}

int pa_headerlist_putsappend(pa_headerlist *p, const char *key, const char *value) {
    struct header *hdr;
    bool add = false;

    pa_assert(p);
    pa_assert(key);

    if (!(hdr = pa_hashmap_get(MAKE_HASHMAP(p), key))) {
        hdr = pa_xnew(struct header, 1);
        hdr->key = pa_xstrdup(key);
        hdr->value = pa_xstrdup(value);
        add = true;
    } else {
        void *newval = pa_sprintf_malloc("%s%s", (char*)hdr->value, value);
        pa_xfree(hdr->value);
        hdr->value = newval;
    }
    hdr->nbytes = strlen(hdr->value)+1;

    if (add)
        pa_hashmap_put(MAKE_HASHMAP(p), hdr->key, hdr);

    return 0;
}

const char *pa_headerlist_gets(pa_headerlist *p, const char *key) {
    struct header *hdr;

    pa_assert(p);
    pa_assert(key);

    if (!(hdr = pa_hashmap_get(MAKE_HASHMAP(p), key)))
        return NULL;

    if (hdr->nbytes <= 0)
        return NULL;

    if (((char*) hdr->value)[hdr->nbytes-1] != 0)
        return NULL;

    if (strlen((char*) hdr->value) != hdr->nbytes-1)
        return NULL;

    return (char*) hdr->value;
}

int pa_headerlist_remove(pa_headerlist *p, const char *key) {
    pa_assert(p);
    pa_assert(key);

    return pa_hashmap_remove_and_free(MAKE_HASHMAP(p), key);
}

const char *pa_headerlist_iterate(pa_headerlist *p, void **state) {
    struct header *hdr;

    if (!(hdr = pa_hashmap_iterate(MAKE_HASHMAP(p), state, NULL)))
        return NULL;

    return hdr->key;
}

char *pa_headerlist_to_string(pa_headerlist *p) {
    const char *key;
    void *state = NULL;
    pa_strbuf *buf;

    pa_assert(p);

    buf = pa_strbuf_new();

    while ((key = pa_headerlist_iterate(p, &state))) {

        const char *v;

        if ((v = pa_headerlist_gets(p, key)))
            pa_strbuf_printf(buf, "%s: %s\r\n", key, v);
    }

    return pa_strbuf_to_string_free(buf);
}

int pa_headerlist_contains(pa_headerlist *p, const char *key) {
    pa_assert(p);
    pa_assert(key);

    if (!(pa_hashmap_get(MAKE_HASHMAP(p), key)))
        return 0;

    return 1;
}

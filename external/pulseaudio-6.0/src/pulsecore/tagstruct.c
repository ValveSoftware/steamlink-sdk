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
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <pulse/xmalloc.h>

#include <pulsecore/socket.h>
#include <pulsecore/macro.h>

#include "tagstruct.h"

#define MAX_TAG_SIZE (64*1024)

struct pa_tagstruct {
    uint8_t *data;
    size_t length, allocated;
    size_t rindex;

    bool dynamic;
};

pa_tagstruct *pa_tagstruct_new(const uint8_t* data, size_t length) {
    pa_tagstruct*t;

    pa_assert(!data || (data && length));

    t = pa_xnew(pa_tagstruct, 1);
    t->data = (uint8_t*) data;
    t->allocated = t->length = data ? length : 0;
    t->rindex = 0;
    t->dynamic = !data;

    return t;
}

void pa_tagstruct_free(pa_tagstruct*t) {
    pa_assert(t);

    if (t->dynamic)
        pa_xfree(t->data);
    pa_xfree(t);
}

uint8_t* pa_tagstruct_free_data(pa_tagstruct*t, size_t *l) {
    uint8_t *p;

    pa_assert(t);
    pa_assert(t->dynamic);
    pa_assert(l);

    p = t->data;
    *l = t->length;
    pa_xfree(t);
    return p;
}

static void extend(pa_tagstruct*t, size_t l) {
    pa_assert(t);
    pa_assert(t->dynamic);

    if (t->length+l <= t->allocated)
        return;

    t->data = pa_xrealloc(t->data, t->allocated = t->length+l+100);
}

void pa_tagstruct_puts(pa_tagstruct*t, const char *s) {
    size_t l;
    pa_assert(t);

    if (s) {
        l = strlen(s)+2;
        extend(t, l);
        t->data[t->length] = PA_TAG_STRING;
        strcpy((char*) (t->data+t->length+1), s);
        t->length += l;
    } else {
        extend(t, 1);
        t->data[t->length] = PA_TAG_STRING_NULL;
        t->length += 1;
    }
}

void pa_tagstruct_putu32(pa_tagstruct*t, uint32_t i) {
    pa_assert(t);

    extend(t, 5);
    t->data[t->length] = PA_TAG_U32;
    i = htonl(i);
    memcpy(t->data+t->length+1, &i, 4);
    t->length += 5;
}

void pa_tagstruct_putu8(pa_tagstruct*t, uint8_t c) {
    pa_assert(t);

    extend(t, 2);
    t->data[t->length] = PA_TAG_U8;
    *(t->data+t->length+1) = c;
    t->length += 2;
}

void pa_tagstruct_put_sample_spec(pa_tagstruct *t, const pa_sample_spec *ss) {
    uint32_t rate;

    pa_assert(t);
    pa_assert(ss);

    extend(t, 7);
    t->data[t->length] = PA_TAG_SAMPLE_SPEC;
    t->data[t->length+1] = (uint8_t) ss->format;
    t->data[t->length+2] = ss->channels;
    rate = htonl(ss->rate);
    memcpy(t->data+t->length+3, &rate, 4);
    t->length += 7;
}

void pa_tagstruct_put_arbitrary(pa_tagstruct *t, const void *p, size_t length) {
    uint32_t tmp;

    pa_assert(t);
    pa_assert(p);

    extend(t, 5+length);
    t->data[t->length] = PA_TAG_ARBITRARY;
    tmp = htonl((uint32_t) length);
    memcpy(t->data+t->length+1, &tmp, 4);
    if (length)
        memcpy(t->data+t->length+5, p, length);
    t->length += 5+length;
}

void pa_tagstruct_put_boolean(pa_tagstruct*t, bool b) {
    pa_assert(t);

    extend(t, 1);
    t->data[t->length] = (uint8_t) (b ? PA_TAG_BOOLEAN_TRUE : PA_TAG_BOOLEAN_FALSE);
    t->length += 1;
}

void pa_tagstruct_put_timeval(pa_tagstruct*t, const struct timeval *tv) {
    uint32_t tmp;
    pa_assert(t);

    extend(t, 9);
    t->data[t->length] = PA_TAG_TIMEVAL;
    tmp = htonl((uint32_t) tv->tv_sec);
    memcpy(t->data+t->length+1, &tmp, 4);
    tmp = htonl((uint32_t) tv->tv_usec);
    memcpy(t->data+t->length+5, &tmp, 4);
    t->length += 9;
}

void pa_tagstruct_put_usec(pa_tagstruct*t, pa_usec_t u) {
    uint32_t tmp;

    pa_assert(t);

    extend(t, 9);
    t->data[t->length] = PA_TAG_USEC;
    tmp = htonl((uint32_t) (u >> 32));
    memcpy(t->data+t->length+1, &tmp, 4);
    tmp = htonl((uint32_t) u);
    memcpy(t->data+t->length+5, &tmp, 4);
    t->length += 9;
}

void pa_tagstruct_putu64(pa_tagstruct*t, uint64_t u) {
    uint32_t tmp;

    pa_assert(t);

    extend(t, 9);
    t->data[t->length] = PA_TAG_U64;
    tmp = htonl((uint32_t) (u >> 32));
    memcpy(t->data+t->length+1, &tmp, 4);
    tmp = htonl((uint32_t) u);
    memcpy(t->data+t->length+5, &tmp, 4);
    t->length += 9;
}

void pa_tagstruct_puts64(pa_tagstruct*t, int64_t u) {
    uint32_t tmp;

    pa_assert(t);

    extend(t, 9);
    t->data[t->length] = PA_TAG_S64;
    tmp = htonl((uint32_t) ((uint64_t) u >> 32));
    memcpy(t->data+t->length+1, &tmp, 4);
    tmp = htonl((uint32_t) ((uint64_t) u));
    memcpy(t->data+t->length+5, &tmp, 4);
    t->length += 9;
}

void pa_tagstruct_put_channel_map(pa_tagstruct *t, const pa_channel_map *map) {
    unsigned i;

    pa_assert(t);
    pa_assert(map);
    extend(t, 2 + (size_t) map->channels);

    t->data[t->length++] = PA_TAG_CHANNEL_MAP;
    t->data[t->length++] = map->channels;

    for (i = 0; i < map->channels; i ++)
        t->data[t->length++] = (uint8_t) map->map[i];
}

void pa_tagstruct_put_cvolume(pa_tagstruct *t, const pa_cvolume *cvolume) {
    unsigned i;
    pa_volume_t vol;

    pa_assert(t);
    pa_assert(cvolume);
    extend(t, 2 + cvolume->channels * sizeof(pa_volume_t));

    t->data[t->length++] = PA_TAG_CVOLUME;
    t->data[t->length++] = cvolume->channels;

    for (i = 0; i < cvolume->channels; i ++) {
        vol = htonl(cvolume->values[i]);
        memcpy(t->data + t->length, &vol, sizeof(pa_volume_t));
        t->length += sizeof(pa_volume_t);
    }
}

void pa_tagstruct_put_volume(pa_tagstruct *t, pa_volume_t vol) {
    uint32_t u;
    pa_assert(t);

    extend(t, 5);
    t->data[t->length] = PA_TAG_VOLUME;
    u = htonl((uint32_t) vol);
    memcpy(t->data+t->length+1, &u, 4);
    t->length += 5;
}

void pa_tagstruct_put_proplist(pa_tagstruct *t, pa_proplist *p) {
    void *state = NULL;
    pa_assert(t);
    pa_assert(p);

    extend(t, 1);

    t->data[t->length++] = PA_TAG_PROPLIST;

    for (;;) {
        const char *k;
        const void *d;
        size_t l;

        if (!(k = pa_proplist_iterate(p, &state)))
            break;

        pa_tagstruct_puts(t, k);
        pa_assert_se(pa_proplist_get(p, k, &d, &l) >= 0);
        pa_tagstruct_putu32(t, (uint32_t) l);
        pa_tagstruct_put_arbitrary(t, d, l);
    }

    pa_tagstruct_puts(t, NULL);
}

void pa_tagstruct_put_format_info(pa_tagstruct *t, pa_format_info *f) {
    pa_assert(t);
    pa_assert(f);

    extend(t, 1);

    t->data[t->length++] = PA_TAG_FORMAT_INFO;
    pa_tagstruct_putu8(t, (uint8_t) f->encoding);
    pa_tagstruct_put_proplist(t, f->plist);
}

int pa_tagstruct_gets(pa_tagstruct*t, const char **s) {
    int error = 0;
    size_t n;
    char *c;

    pa_assert(t);
    pa_assert(s);

    if (t->rindex+1 > t->length)
        return -1;

    if (t->data[t->rindex] == PA_TAG_STRING_NULL) {
        t->rindex++;
        *s = NULL;
        return 0;
    }

    if (t->rindex+2 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_STRING)
        return -1;

    error = 1;
    for (n = 0, c = (char*) (t->data+t->rindex+1); t->rindex+1+n < t->length; n++, c++)
        if (!*c) {
            error = 0;
            break;
        }

    if (error)
        return -1;

    *s = (char*) (t->data+t->rindex+1);

    t->rindex += n+2;
    return 0;
}

int pa_tagstruct_getu32(pa_tagstruct*t, uint32_t *i) {
    pa_assert(t);
    pa_assert(i);

    if (t->rindex+5 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_U32)
        return -1;

    memcpy(i, t->data+t->rindex+1, 4);
    *i = ntohl(*i);
    t->rindex += 5;
    return 0;
}

int pa_tagstruct_getu8(pa_tagstruct*t, uint8_t *c) {
    pa_assert(t);
    pa_assert(c);

    if (t->rindex+2 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_U8)
        return -1;

    *c = t->data[t->rindex+1];
    t->rindex +=2;
    return 0;
}

int pa_tagstruct_get_sample_spec(pa_tagstruct *t, pa_sample_spec *ss) {
    pa_assert(t);
    pa_assert(ss);

    if (t->rindex+7 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_SAMPLE_SPEC)
        return -1;

    ss->format = t->data[t->rindex+1];
    ss->channels = t->data[t->rindex+2];
    memcpy(&ss->rate, t->data+t->rindex+3, 4);
    ss->rate = ntohl(ss->rate);

    t->rindex += 7;
    return 0;
}

int pa_tagstruct_get_arbitrary(pa_tagstruct *t, const void **p, size_t length) {
    uint32_t len;

    pa_assert(t);
    pa_assert(p);

    if (t->rindex+5+length > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_ARBITRARY)
        return -1;

    memcpy(&len, t->data+t->rindex+1, 4);
    if (ntohl(len) != length)
        return -1;

    *p = t->data+t->rindex+5;
    t->rindex += 5+length;
    return 0;
}

int pa_tagstruct_eof(pa_tagstruct*t) {
    pa_assert(t);

    return t->rindex >= t->length;
}

const uint8_t* pa_tagstruct_data(pa_tagstruct*t, size_t *l) {
    pa_assert(t);
    pa_assert(t->dynamic);
    pa_assert(l);

    *l = t->length;
    return t->data;
}

int pa_tagstruct_get_boolean(pa_tagstruct*t, bool *b) {
    pa_assert(t);
    pa_assert(b);

    if (t->rindex+1 > t->length)
        return -1;

    if (t->data[t->rindex] == PA_TAG_BOOLEAN_TRUE)
        *b = true;
    else if (t->data[t->rindex] == PA_TAG_BOOLEAN_FALSE)
        *b = false;
    else
        return -1;

    t->rindex +=1;
    return 0;
}

int pa_tagstruct_get_timeval(pa_tagstruct*t, struct timeval *tv) {

    pa_assert(t);
    pa_assert(tv);

    if (t->rindex+9 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_TIMEVAL)
        return -1;

    memcpy(&tv->tv_sec, t->data+t->rindex+1, 4);
    tv->tv_sec = (time_t) ntohl((uint32_t) tv->tv_sec);
    memcpy(&tv->tv_usec, t->data+t->rindex+5, 4);
    tv->tv_usec = (suseconds_t) ntohl((uint32_t) tv->tv_usec);
    t->rindex += 9;
    return 0;
}

int pa_tagstruct_get_usec(pa_tagstruct*t, pa_usec_t *u) {
    uint32_t tmp;

    pa_assert(t);
    pa_assert(u);

    if (t->rindex+9 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_USEC)
        return -1;

    memcpy(&tmp, t->data+t->rindex+1, 4);
    *u = (pa_usec_t) ntohl(tmp) << 32;
    memcpy(&tmp, t->data+t->rindex+5, 4);
    *u |= (pa_usec_t) ntohl(tmp);
    t->rindex +=9;
    return 0;
}

int pa_tagstruct_getu64(pa_tagstruct*t, uint64_t *u) {
    uint32_t tmp;

    pa_assert(t);
    pa_assert(u);

    if (t->rindex+9 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_U64)
        return -1;

    memcpy(&tmp, t->data+t->rindex+1, 4);
    *u = (uint64_t) ntohl(tmp) << 32;
    memcpy(&tmp, t->data+t->rindex+5, 4);
    *u |= (uint64_t) ntohl(tmp);
    t->rindex +=9;
    return 0;
}

int pa_tagstruct_gets64(pa_tagstruct*t, int64_t *u) {
    uint32_t tmp;

    pa_assert(t);
    pa_assert(u);

    if (t->rindex+9 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_S64)
        return -1;

    memcpy(&tmp, t->data+t->rindex+1, 4);
    *u = (int64_t) ((uint64_t) ntohl(tmp) << 32);
    memcpy(&tmp, t->data+t->rindex+5, 4);
    *u |= (int64_t) ntohl(tmp);
    t->rindex +=9;
    return 0;
}

int pa_tagstruct_get_channel_map(pa_tagstruct *t, pa_channel_map *map) {
    unsigned i;

    pa_assert(t);
    pa_assert(map);

    if (t->rindex+2 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_CHANNEL_MAP)
        return -1;

    if ((map->channels = t->data[t->rindex+1]) > PA_CHANNELS_MAX)
        return -1;

    if (t->rindex+2+map->channels > t->length)
        return -1;

    for (i = 0; i < map->channels; i ++)
        map->map[i] = (int8_t) t->data[t->rindex + 2 + i];

    t->rindex += 2 + (size_t) map->channels;
    return 0;
}

int pa_tagstruct_get_cvolume(pa_tagstruct *t, pa_cvolume *cvolume) {
    unsigned i;
    pa_volume_t vol;

    pa_assert(t);
    pa_assert(cvolume);

    if (t->rindex+2 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_CVOLUME)
        return -1;

    if ((cvolume->channels = t->data[t->rindex+1]) > PA_CHANNELS_MAX)
        return -1;

    if (t->rindex+2+cvolume->channels*sizeof(pa_volume_t) > t->length)
        return -1;

    for (i = 0; i < cvolume->channels; i ++) {
        memcpy(&vol, t->data + t->rindex + 2 + i * sizeof(pa_volume_t), sizeof(pa_volume_t));
        cvolume->values[i] = (pa_volume_t) ntohl(vol);
    }

    t->rindex += 2 + cvolume->channels * sizeof(pa_volume_t);
    return 0;
}

int pa_tagstruct_get_volume(pa_tagstruct*t, pa_volume_t *vol) {
    uint32_t u;

    pa_assert(t);
    pa_assert(vol);

    if (t->rindex+5 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_VOLUME)
        return -1;

    memcpy(&u, t->data+t->rindex+1, 4);
    *vol = (pa_volume_t) ntohl(u);

    t->rindex += 5;
    return 0;
}

int pa_tagstruct_get_proplist(pa_tagstruct *t, pa_proplist *p) {
    size_t saved_rindex;

    pa_assert(t);

    if (t->rindex+1 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_PROPLIST)
        return -1;

    saved_rindex = t->rindex;
    t->rindex++;

    for (;;) {
        const char *k;
        const void *d;
        uint32_t length;

        if (pa_tagstruct_gets(t, &k) < 0)
            goto fail;

        if (!k)
            break;

        if (!pa_proplist_key_valid(k))
            goto fail;

        if (pa_tagstruct_getu32(t, &length) < 0)
            goto fail;

        if (length > MAX_TAG_SIZE)
            goto fail;

        if (pa_tagstruct_get_arbitrary(t, &d, length) < 0)
            goto fail;

        if (p)
            pa_assert_se(pa_proplist_set(p, k, d, length) >= 0);
    }

    return 0;

fail:
    t->rindex = saved_rindex;
    return -1;
}

int pa_tagstruct_get_format_info(pa_tagstruct *t, pa_format_info *f) {
    size_t saved_rindex;
    uint8_t encoding;

    pa_assert(t);
    pa_assert(f);

    if (t->rindex+1 > t->length)
        return -1;

    if (t->data[t->rindex] != PA_TAG_FORMAT_INFO)
        return -1;

    saved_rindex = t->rindex;
    t->rindex++;

    if (pa_tagstruct_getu8(t, &encoding) < 0)
        goto fail;

    f->encoding = encoding;

    if (pa_tagstruct_get_proplist(t, f->plist) < 0)
        goto fail;

    return 0;

fail:
    t->rindex = saved_rindex;
    return -1;
}

void pa_tagstruct_put(pa_tagstruct *t, ...) {
    va_list va;
    pa_assert(t);

    va_start(va, t);

    for (;;) {
        int tag = va_arg(va, int);

        if (tag == PA_TAG_INVALID)
            break;

        switch (tag) {
            case PA_TAG_STRING:
            case PA_TAG_STRING_NULL:
                pa_tagstruct_puts(t, va_arg(va, char*));
                break;

            case PA_TAG_U32:
                pa_tagstruct_putu32(t, va_arg(va, uint32_t));
                break;

            case PA_TAG_U8:
                pa_tagstruct_putu8(t, (uint8_t) va_arg(va, int));
                break;

            case PA_TAG_U64:
                pa_tagstruct_putu64(t, va_arg(va, uint64_t));
                break;

            case PA_TAG_SAMPLE_SPEC:
                pa_tagstruct_put_sample_spec(t, va_arg(va, pa_sample_spec*));
                break;

            case PA_TAG_ARBITRARY: {
                void *p = va_arg(va, void*);
                size_t size = va_arg(va, size_t);
                pa_tagstruct_put_arbitrary(t, p, size);
                break;
            }

            case PA_TAG_BOOLEAN_TRUE:
            case PA_TAG_BOOLEAN_FALSE:
                pa_tagstruct_put_boolean(t, va_arg(va, int));
                break;

            case PA_TAG_TIMEVAL:
                pa_tagstruct_put_timeval(t, va_arg(va, struct timeval*));
                break;

            case PA_TAG_USEC:
                pa_tagstruct_put_usec(t, va_arg(va, pa_usec_t));
                break;

            case PA_TAG_CHANNEL_MAP:
                pa_tagstruct_put_channel_map(t, va_arg(va, pa_channel_map *));
                break;

            case PA_TAG_CVOLUME:
                pa_tagstruct_put_cvolume(t, va_arg(va, pa_cvolume *));
                break;

            case PA_TAG_VOLUME:
                pa_tagstruct_put_volume(t, va_arg(va, pa_volume_t));
                break;

            case PA_TAG_PROPLIST:
                pa_tagstruct_put_proplist(t, va_arg(va, pa_proplist *));
                break;

            default:
                pa_assert_not_reached();
        }
    }

    va_end(va);
}

int pa_tagstruct_get(pa_tagstruct *t, ...) {
    va_list va;
    int ret = 0;

    pa_assert(t);

    va_start(va, t);
    while (ret == 0) {
        int tag = va_arg(va, int);

        if (tag == PA_TAG_INVALID)
            break;

        switch (tag) {
            case PA_TAG_STRING:
            case PA_TAG_STRING_NULL:
                ret = pa_tagstruct_gets(t, va_arg(va, const char**));
                break;

            case PA_TAG_U32:
                ret = pa_tagstruct_getu32(t, va_arg(va, uint32_t*));
                break;

            case PA_TAG_U8:
                ret = pa_tagstruct_getu8(t, va_arg(va, uint8_t*));
                break;

            case PA_TAG_U64:
                ret = pa_tagstruct_getu64(t, va_arg(va, uint64_t*));
                break;

            case PA_TAG_SAMPLE_SPEC:
                ret = pa_tagstruct_get_sample_spec(t, va_arg(va, pa_sample_spec*));
                break;

            case PA_TAG_ARBITRARY: {
                const void **p = va_arg(va, const void**);
                size_t size = va_arg(va, size_t);
                ret = pa_tagstruct_get_arbitrary(t, p, size);
                break;
            }

            case PA_TAG_BOOLEAN_TRUE:
            case PA_TAG_BOOLEAN_FALSE:
                ret = pa_tagstruct_get_boolean(t, va_arg(va, bool*));
                break;

            case PA_TAG_TIMEVAL:
                ret = pa_tagstruct_get_timeval(t, va_arg(va, struct timeval*));
                break;

            case PA_TAG_USEC:
                ret = pa_tagstruct_get_usec(t, va_arg(va, pa_usec_t*));
                break;

            case PA_TAG_CHANNEL_MAP:
                ret = pa_tagstruct_get_channel_map(t, va_arg(va, pa_channel_map *));
                break;

            case PA_TAG_CVOLUME:
                ret = pa_tagstruct_get_cvolume(t, va_arg(va, pa_cvolume *));
                break;

            case PA_TAG_VOLUME:
                ret = pa_tagstruct_get_volume(t, va_arg(va, pa_volume_t *));
                break;

            case PA_TAG_PROPLIST:
                ret = pa_tagstruct_get_proplist(t, va_arg(va, pa_proplist *));
                break;

            default:
                pa_assert_not_reached();
        }

    }

    va_end(va);
    return ret;
}

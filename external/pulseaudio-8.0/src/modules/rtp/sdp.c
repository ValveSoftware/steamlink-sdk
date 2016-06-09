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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <pulse/xmalloc.h>
#include <pulse/util.h>

#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/arpa-inet.h>

#include "sdp.h"
#include "rtp.h"

char *pa_sdp_build(int af, const void *src, const void *dst, const char *name, uint16_t port, uint8_t payload, const pa_sample_spec *ss) {
    uint32_t ntp;
    char buf_src[64], buf_dst[64], un[64];
    const char *u, *f;

    pa_assert(src);
    pa_assert(dst);

#ifdef HAVE_IPV6
    pa_assert(af == AF_INET || af == AF_INET6);
#else
    pa_assert(af == AF_INET);
#endif

    pa_assert_se(f = pa_rtp_format_to_string(ss->format));

    if (!(u = pa_get_user_name(un, sizeof(un))))
        u = "-";

    ntp = (uint32_t) time(NULL) + 2208988800U;

    pa_assert_se(inet_ntop(af, src, buf_src, sizeof(buf_src)));
    pa_assert_se(inet_ntop(af, dst, buf_dst, sizeof(buf_dst)));

    return pa_sprintf_malloc(
            PA_SDP_HEADER
            "o=%s %lu 0 IN %s %s\n"
            "s=%s\n"
            "c=IN %s %s\n"
            "t=%lu 0\n"
            "a=recvonly\n"
            "m=audio %u RTP/AVP %i\n"
            "a=rtpmap:%i %s/%u/%u\n"
            "a=type:broadcast\n",
            u, (unsigned long) ntp, af == AF_INET ? "IP4" : "IP6", buf_src,
            name,
            af == AF_INET ? "IP4" : "IP6", buf_dst,
            (unsigned long) ntp,
            port, payload,
            payload, f, ss->rate, ss->channels);
}

static pa_sample_spec *parse_sdp_sample_spec(pa_sample_spec *ss, char *c) {
    unsigned rate, channels;
    pa_assert(ss);
    pa_assert(c);

    if (pa_startswith(c, "L16/")) {
        ss->format = PA_SAMPLE_S16BE;
        c += 4;
    } else if (pa_startswith(c, "L8/")) {
        ss->format = PA_SAMPLE_U8;
        c += 3;
    } else if (pa_startswith(c, "PCMA/")) {
        ss->format = PA_SAMPLE_ALAW;
        c += 5;
    } else if (pa_startswith(c, "PCMU/")) {
        ss->format = PA_SAMPLE_ULAW;
        c += 5;
    } else
        return NULL;

    if (sscanf(c, "%u/%u", &rate, &channels) == 2) {
        ss->rate = (uint32_t) rate;
        ss->channels = (uint8_t) channels;
    } else if (sscanf(c, "%u", &rate) == 2) {
        ss->rate = (uint32_t) rate;
        ss->channels = 1;
    } else
        return NULL;

    if (!pa_sample_spec_valid(ss))
        return NULL;

    return ss;
}

pa_sdp_info *pa_sdp_parse(const char *t, pa_sdp_info *i, int is_goodbye) {
    uint16_t port = 0;
    bool ss_valid = false;

    pa_assert(t);
    pa_assert(i);

    i->origin = i->session_name = NULL;
    i->salen = 0;
    i->payload = 255;

    if (!pa_startswith(t, PA_SDP_HEADER)) {
        pa_log("Failed to parse SDP data: invalid header.");
        goto fail;
    }

    t += sizeof(PA_SDP_HEADER)-1;

    while (*t) {
        size_t l;

        l = strcspn(t, "\n");

        if (l <= 2) {
            pa_log("Failed to parse SDP data: line too short: >%s<.", t);
            goto fail;
        }

        if (pa_startswith(t, "o="))
            i->origin = pa_xstrndup(t+2, l-2);
        else if (pa_startswith(t, "s="))
            i->session_name = pa_xstrndup(t+2, l-2);
        else if (pa_startswith(t, "c=IN IP4 ")) {
            char a[64];
            size_t k;

            k = l-8 > sizeof(a) ? sizeof(a) : l-8;

            pa_strlcpy(a, t+9, k);
            a[strcspn(a, "/")] = 0;

            if (inet_pton(AF_INET, a, &((struct sockaddr_in*) &i->sa)->sin_addr) <= 0) {
                pa_log("Failed to parse SDP data: bad address: >%s<.", a);
                goto fail;
            }

            ((struct sockaddr_in*) &i->sa)->sin_family = AF_INET;
            ((struct sockaddr_in*) &i->sa)->sin_port = 0;
            i->salen = sizeof(struct sockaddr_in);
#ifdef HAVE_IPV6
        } else if (pa_startswith(t, "c=IN IP6 ")) {
            char a[64];
            size_t k;

            k = l-8 > sizeof(a) ? sizeof(a) : l-8;

            pa_strlcpy(a, t+9, k);
            a[strcspn(a, "/")] = 0;

            if (inet_pton(AF_INET6, a, &((struct sockaddr_in6*) &i->sa)->sin6_addr) <= 0) {
                pa_log("Failed to parse SDP data: bad address: >%s<.", a);
                goto fail;
            }

            ((struct sockaddr_in6*) &i->sa)->sin6_family = AF_INET6;
            ((struct sockaddr_in6*) &i->sa)->sin6_port = 0;
            i->salen = sizeof(struct sockaddr_in6);
#endif
        } else if (pa_startswith(t, "m=audio ")) {

            if (i->payload > 127) {
                int _port, _payload;

                if (sscanf(t+8, "%i RTP/AVP %i", &_port, &_payload) == 2) {

                    if (_port <= 0 || _port > 0xFFFF) {
                        pa_log("Failed to parse SDP data: invalid port %i.", _port);
                        goto fail;
                    }

                    if (_payload < 0 || _payload > 127) {
                        pa_log("Failed to parse SDP data: invalid payload %i.", _payload);
                        goto fail;
                    }

                    port = (uint16_t) _port;
                    i->payload = (uint8_t) _payload;

                    if (pa_rtp_sample_spec_from_payload(i->payload, &i->sample_spec))
                        ss_valid = true;
                }
            }
        } else if (pa_startswith(t, "a=rtpmap:")) {

            if (i->payload <= 127) {
                char c[64];
                int _payload;
                int len;

                if (sscanf(t + 9, "%i %n", &_payload, &len) == 1) {
                    if (_payload < 0 || _payload > 127) {
                        pa_log("Failed to parse SDP data: invalid payload %i.", _payload);
                        goto fail;
                    }
                    if (_payload == i->payload) {
                        strncpy(c, t + 9 + len, 63);
                        c[63] = 0;
                        c[strcspn(c, "\n")] = 0;

                        if (parse_sdp_sample_spec(&i->sample_spec, c))
                            ss_valid = true;
                    }
                }
            }
        }

        t += l;

        if (*t == '\n')
            t++;
    }

    if (!i->origin || (!is_goodbye && (!i->salen || i->payload > 127 || !ss_valid || port == 0))) {
        pa_log("Failed to parse SDP data: missing data.");
        goto fail;
    }

    if (((struct sockaddr*) &i->sa)->sa_family == AF_INET)
        ((struct sockaddr_in*) &i->sa)->sin_port = htons(port);
    else
        ((struct sockaddr_in6*) &i->sa)->sin6_port = htons(port);

    return i;

fail:
    pa_xfree(i->origin);
    pa_xfree(i->session_name);

    return NULL;
}

void pa_sdp_info_destroy(pa_sdp_info *i) {
    pa_assert(i);

    pa_xfree(i->origin);
    pa_xfree(i->session_name);
}

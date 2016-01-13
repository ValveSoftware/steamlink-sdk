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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include <pulsecore/core-error.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/core-util.h>
#include <pulsecore/arpa-inet.h>

#include "rtp.h"

pa_rtp_context* pa_rtp_context_init_send(pa_rtp_context *c, int fd, uint32_t ssrc, uint8_t payload, size_t frame_size) {
    pa_assert(c);
    pa_assert(fd >= 0);

    c->fd = fd;
    c->sequence = (uint16_t) (rand()*rand());
    c->timestamp = 0;
    c->ssrc = ssrc ? ssrc : (uint32_t) (rand()*rand());
    c->payload = (uint8_t) (payload & 127U);
    c->frame_size = frame_size;

    pa_memchunk_reset(&c->memchunk);

    return c;
}

#define MAX_IOVECS 16

int pa_rtp_send(pa_rtp_context *c, size_t size, pa_memblockq *q) {
    struct iovec iov[MAX_IOVECS];
    pa_memblock* mb[MAX_IOVECS];
    int iov_idx = 1;
    size_t n = 0;

    pa_assert(c);
    pa_assert(size > 0);
    pa_assert(q);

    if (pa_memblockq_get_length(q) < size)
        return 0;

    for (;;) {
        int r;
        pa_memchunk chunk;

        pa_memchunk_reset(&chunk);

        if ((r = pa_memblockq_peek(q, &chunk)) >= 0) {

            size_t k = n + chunk.length > size ? size - n : chunk.length;

            pa_assert(chunk.memblock);

            iov[iov_idx].iov_base = pa_memblock_acquire_chunk(&chunk);
            iov[iov_idx].iov_len = k;
            mb[iov_idx] = chunk.memblock;
            iov_idx ++;

            n += k;
            pa_memblockq_drop(q, k);
        }

        pa_assert(n % c->frame_size == 0);

        if (r < 0 || n >= size || iov_idx >= MAX_IOVECS) {
            uint32_t header[3];
            struct msghdr m;
            ssize_t k;
            int i;

            if (n > 0) {
                header[0] = htonl(((uint32_t) 2 << 30) | ((uint32_t) c->payload << 16) | ((uint32_t) c->sequence));
                header[1] = htonl(c->timestamp);
                header[2] = htonl(c->ssrc);

                iov[0].iov_base = (void*)header;
                iov[0].iov_len = sizeof(header);

                m.msg_name = NULL;
                m.msg_namelen = 0;
                m.msg_iov = iov;
                m.msg_iovlen = (size_t) iov_idx;
                m.msg_control = NULL;
                m.msg_controllen = 0;
                m.msg_flags = 0;

                k = sendmsg(c->fd, &m, MSG_DONTWAIT);

                for (i = 1; i < iov_idx; i++) {
                    pa_memblock_release(mb[i]);
                    pa_memblock_unref(mb[i]);
                }

                c->sequence++;
            } else
                k = 0;

            c->timestamp += (unsigned) (n/c->frame_size);

            if (k < 0) {
                if (errno != EAGAIN && errno != EINTR) /* If the queue is full, just ignore it */
                    pa_log("sendmsg() failed: %s", pa_cstrerror(errno));
                return -1;
            }

            if (r < 0 || pa_memblockq_get_length(q) < size)
                break;

            n = 0;
            iov_idx = 1;
        }
    }

    return 0;
}

pa_rtp_context* pa_rtp_context_init_recv(pa_rtp_context *c, int fd, size_t frame_size) {
    pa_assert(c);

    c->fd = fd;
    c->frame_size = frame_size;

    pa_memchunk_reset(&c->memchunk);
    return c;
}

int pa_rtp_recv(pa_rtp_context *c, pa_memchunk *chunk, pa_mempool *pool, struct timeval *tstamp) {
    int size;
    struct msghdr m;
    struct cmsghdr *cm;
    struct iovec iov;
    uint32_t header;
    unsigned cc;
    ssize_t r;
    uint8_t aux[1024];
    bool found_tstamp = false;

    pa_assert(c);
    pa_assert(chunk);

    pa_memchunk_reset(chunk);

    if (ioctl(c->fd, FIONREAD, &size) < 0) {
        pa_log_warn("FIONREAD failed: %s", pa_cstrerror(errno));
        goto fail;
    }

    if (size <= 0) {
        /* size can be 0 due to any of the following reasons:
         *
         * 1. Somebody sent us a perfectly valid zero-length UDP packet.
         * 2. Somebody sent us a UDP packet with a bad CRC.
         *
         * It is unknown whether size can actually be less than zero.
         *
         * In the first case, the packet has to be read out, otherwise the
         * kernel will tell us again and again about it, thus preventing
         * reception of any further packets. So let's just read it out
         * now and discard it later, when comparing the number of bytes
         * received (0) with the number of bytes wanted (1, see below).
         *
         * In the second case, recvmsg() will fail, thus allowing us to
         * return the error.
         *
         * Just to avoid passing zero-sized memchunks and NULL pointers to
         * recvmsg(), let's force allocation of at least one byte by setting
         * size to 1.
         */
        size = 1;
    }

    if (c->memchunk.length < (unsigned) size) {
        size_t l;

        if (c->memchunk.memblock)
            pa_memblock_unref(c->memchunk.memblock);

        l = PA_MAX((size_t) size, pa_mempool_block_size_max(pool));

        c->memchunk.memblock = pa_memblock_new(pool, l);
        c->memchunk.index = 0;
        c->memchunk.length = pa_memblock_get_length(c->memchunk.memblock);
    }

    pa_assert(c->memchunk.length >= (size_t) size);

    chunk->memblock = pa_memblock_ref(c->memchunk.memblock);
    chunk->index = c->memchunk.index;

    iov.iov_base = pa_memblock_acquire_chunk(chunk);
    iov.iov_len = (size_t) size;

    m.msg_name = NULL;
    m.msg_namelen = 0;
    m.msg_iov = &iov;
    m.msg_iovlen = 1;
    m.msg_control = aux;
    m.msg_controllen = sizeof(aux);
    m.msg_flags = 0;

    r = recvmsg(c->fd, &m, 0);
    pa_memblock_release(chunk->memblock);

    if (r != size) {
        if (r < 0 && errno != EAGAIN && errno != EINTR)
            pa_log_warn("recvmsg() failed: %s", r < 0 ? pa_cstrerror(errno) : "size mismatch");

        goto fail;
    }

    if (size < 12) {
        pa_log_warn("RTP packet too short.");
        goto fail;
    }

    memcpy(&header, iov.iov_base, sizeof(uint32_t));
    memcpy(&c->timestamp, (uint8_t*) iov.iov_base + 4, sizeof(uint32_t));
    memcpy(&c->ssrc, (uint8_t*) iov.iov_base + 8, sizeof(uint32_t));

    header = ntohl(header);
    c->timestamp = ntohl(c->timestamp);
    c->ssrc = ntohl(c->ssrc);

    if ((header >> 30) != 2) {
        pa_log_warn("Unsupported RTP version.");
        goto fail;
    }

    if ((header >> 29) & 1) {
        pa_log_warn("RTP padding not supported.");
        goto fail;
    }

    if ((header >> 28) & 1) {
        pa_log_warn("RTP header extensions not supported.");
        goto fail;
    }

    cc = (header >> 24) & 0xF;
    c->payload = (uint8_t) ((header >> 16) & 127U);
    c->sequence = (uint16_t) (header & 0xFFFFU);

    if (12 + cc*4 > (unsigned) size) {
        pa_log_warn("RTP packet too short. (CSRC)");
        goto fail;
    }

    chunk->index += 12 + cc*4;
    chunk->length = (size_t) size - 12 + cc*4;

    if (chunk->length % c->frame_size != 0) {
        pa_log_warn("Bad RTP packet size.");
        goto fail;
    }

    c->memchunk.index = chunk->index + chunk->length;
    c->memchunk.length = pa_memblock_get_length(c->memchunk.memblock) - c->memchunk.index;

    if (c->memchunk.length <= 0) {
        pa_memblock_unref(c->memchunk.memblock);
        pa_memchunk_reset(&c->memchunk);
    }

    for (cm = CMSG_FIRSTHDR(&m); cm; cm = CMSG_NXTHDR(&m, cm))
        if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_TIMESTAMP) {
            memcpy(tstamp, CMSG_DATA(cm), sizeof(struct timeval));
            found_tstamp = true;
            break;
        }

    if (!found_tstamp) {
        pa_log_warn("Couldn't find SCM_TIMESTAMP data in auxiliary recvmsg() data!");
        pa_zero(*tstamp);
    }

    return 0;

fail:
    if (chunk->memblock)
        pa_memblock_unref(chunk->memblock);

    return -1;
}

uint8_t pa_rtp_payload_from_sample_spec(const pa_sample_spec *ss) {
    pa_assert(ss);

    if (ss->format == PA_SAMPLE_ULAW && ss->rate == 8000 && ss->channels == 1)
        return 0;
    if (ss->format == PA_SAMPLE_ALAW && ss->rate == 8000 && ss->channels == 1)
        return 8;
    if (ss->format == PA_SAMPLE_S16BE && ss->rate == 44100 && ss->channels == 2)
        return 10;
    if (ss->format == PA_SAMPLE_S16BE && ss->rate == 44100 && ss->channels == 1)
        return 11;

    return 127;
}

pa_sample_spec *pa_rtp_sample_spec_from_payload(uint8_t payload, pa_sample_spec *ss) {
    pa_assert(ss);

    switch (payload) {
        case 0:
            ss->channels = 1;
            ss->format = PA_SAMPLE_ULAW;
            ss->rate = 8000;
            break;

        case 8:
            ss->channels = 1;
            ss->format = PA_SAMPLE_ALAW;
            ss->rate = 8000;
            break;

        case 10:
            ss->channels = 2;
            ss->format = PA_SAMPLE_S16BE;
            ss->rate = 44100;
            break;

        case 11:
            ss->channels = 1;
            ss->format = PA_SAMPLE_S16BE;
            ss->rate = 44100;
            break;

        default:
            return NULL;
    }

    return ss;
}

pa_sample_spec *pa_rtp_sample_spec_fixup(pa_sample_spec * ss) {
    pa_assert(ss);

    if (!pa_rtp_sample_spec_valid(ss))
        ss->format = PA_SAMPLE_S16BE;

    pa_assert(pa_rtp_sample_spec_valid(ss));
    return ss;
}

int pa_rtp_sample_spec_valid(const pa_sample_spec *ss) {
    pa_assert(ss);

    if (!pa_sample_spec_valid(ss))
        return 0;

    return
        ss->format == PA_SAMPLE_U8 ||
        ss->format == PA_SAMPLE_ALAW ||
        ss->format == PA_SAMPLE_ULAW ||
        ss->format == PA_SAMPLE_S16BE;
}

void pa_rtp_context_destroy(pa_rtp_context *c) {
    pa_assert(c);

    pa_assert_se(pa_close(c->fd) == 0);

    if (c->memchunk.memblock)
        pa_memblock_unref(c->memchunk.memblock);
}

const char* pa_rtp_format_to_string(pa_sample_format_t f) {
    switch (f) {
        case PA_SAMPLE_S16BE:
            return "L16";
        case PA_SAMPLE_U8:
            return "L8";
        case PA_SAMPLE_ALAW:
            return "PCMA";
        case PA_SAMPLE_ULAW:
            return "PCMU";
        default:
            return NULL;
    }
}

pa_sample_format_t pa_rtp_string_to_format(const char *s) {
    pa_assert(s);

    if (pa_streq(s, "L16"))
        return PA_SAMPLE_S16BE;
    else if (pa_streq(s, "L8"))
        return PA_SAMPLE_U8;
    else if (pa_streq(s, "PCMA"))
        return PA_SAMPLE_ALAW;
    else if (pa_streq(s, "PCMU"))
        return PA_SAMPLE_ULAW;
    else
        return PA_SAMPLE_INVALID;
}

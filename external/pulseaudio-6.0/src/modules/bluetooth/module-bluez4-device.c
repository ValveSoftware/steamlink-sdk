/***
  This file is part of PulseAudio.

  Copyright 2008-2013 João Paulo Rechi Vita
  Copyright 2011-2013 BMW Car IT GmbH.

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include <math.h>
#include <linux/sockios.h>
#include <arpa/inet.h>

#include <pulse/rtclock.h>
#include <pulse/sample.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>

#include <pulsecore/i18n.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-util.h>
#include <pulsecore/core-error.h>
#include <pulsecore/shared.h>
#include <pulsecore/socket-util.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/poll.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/time-smoother.h>
#include <pulsecore/namereg.h>

#include <sbc/sbc.h>

#include "module-bluez4-device-symdef.h"
#include "a2dp-codecs.h"
#include "rtp.h"
#include "bluez4-util.h"

#define BITPOOL_DEC_LIMIT 32
#define BITPOOL_DEC_STEP 5

PA_MODULE_AUTHOR("João Paulo Rechi Vita");
PA_MODULE_DESCRIPTION("BlueZ 4 Bluetooth audio sink and source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "name=<name for the card/sink/source, to be prefixed> "
        "card_name=<name for the card> "
        "card_properties=<properties for the card> "
        "sink_name=<name for the sink> "
        "sink_properties=<properties for the sink> "
        "source_name=<name for the source> "
        "source_properties=<properties for the source> "
        "address=<address of the device> "
        "profile=<a2dp|hsp|hfgw> "
        "rate=<sample rate> "
        "channels=<number of channels> "
        "path=<device object path> "
        "auto_connect=<automatically connect?> "
        "sco_sink=<SCO over PCM sink name> "
        "sco_source=<SCO over PCM source name>");

/* TODO: not close fd when entering suspend mode in a2dp */

static const char* const valid_modargs[] = {
    "name",
    "card_name",
    "card_properties",
    "sink_name",
    "sink_properties",
    "source_name",
    "source_properties",
    "address",
    "profile",
    "rate",
    "channels",
    "path",
    "auto_connect",
    "sco_sink",
    "sco_source",
    NULL
};

struct a2dp_info {
    sbc_t sbc;                           /* Codec data */
    bool sbc_initialized;                /* Keep track if the encoder is initialized */
    size_t codesize, frame_length;       /* SBC Codesize, frame_length. We simply cache those values here */

    void* buffer;                        /* Codec transfer buffer */
    size_t buffer_size;                  /* Size of the buffer */

    uint16_t seq_num;                    /* Cumulative packet sequence */
    uint8_t min_bitpool;
    uint8_t max_bitpool;
};

struct hsp_info {
    pa_sink *sco_sink;
    void (*sco_sink_set_volume)(pa_sink *s);
    pa_source *sco_source;
    void (*sco_source_set_volume)(pa_source *s);
};

struct bluetooth_msg {
    pa_msgobject parent;
    pa_card *card;
};

typedef struct bluetooth_msg bluetooth_msg;
PA_DEFINE_PRIVATE_CLASS(bluetooth_msg, pa_msgobject);
#define BLUETOOTH_MSG(o) (bluetooth_msg_cast(o))

struct userdata {
    pa_core *core;
    pa_module *module;

    pa_bluez4_device *device;
    pa_hook_slot *uuid_added_slot;
    char *address;
    char *path;
    pa_bluez4_transport *transport;
    bool transport_acquired;
    pa_hook_slot *discovery_slot;
    pa_hook_slot *sink_state_changed_slot;
    pa_hook_slot *source_state_changed_slot;
    pa_hook_slot *transport_state_changed_slot;
    pa_hook_slot *transport_nrec_changed_slot;
    pa_hook_slot *transport_microphone_changed_slot;
    pa_hook_slot *transport_speaker_changed_slot;

    pa_bluez4_discovery *discovery;
    bool auto_connect;

    char *output_port_name;
    char *input_port_name;

    pa_card *card;
    pa_sink *sink;
    pa_source *source;

    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;
    pa_rtpoll_item *rtpoll_item;
    pa_thread *thread;
    bluetooth_msg *msg;

    uint64_t read_index, write_index;
    pa_usec_t started_at;
    pa_smoother *read_smoother;

    pa_memchunk write_memchunk;

    pa_sample_spec sample_spec, requested_sample_spec;

    int stream_fd;

    size_t read_link_mtu;
    size_t read_block_size;

    size_t write_link_mtu;
    size_t write_block_size;

    struct a2dp_info a2dp;
    struct hsp_info hsp;

    pa_bluez4_profile_t profile;

    pa_modargs *modargs;

    int stream_write_type;
};

enum {
    BLUETOOTH_MESSAGE_IO_THREAD_FAILED,
    BLUETOOTH_MESSAGE_MAX
};

#define FIXED_LATENCY_PLAYBACK_A2DP (25*PA_USEC_PER_MSEC)
#define FIXED_LATENCY_RECORD_A2DP (25*PA_USEC_PER_MSEC)
#define FIXED_LATENCY_PLAYBACK_HSP (125*PA_USEC_PER_MSEC)
#define FIXED_LATENCY_RECORD_HSP (25*PA_USEC_PER_MSEC)

#define MAX_PLAYBACK_CATCH_UP_USEC (100*PA_USEC_PER_MSEC)

#define USE_SCO_OVER_PCM(u) (u->profile == PA_BLUEZ4_PROFILE_HSP && (u->hsp.sco_sink && u->hsp.sco_source))

static int init_profile(struct userdata *u);

/* from IO thread */
static void a2dp_set_bitpool(struct userdata *u, uint8_t bitpool) {
    struct a2dp_info *a2dp;

    pa_assert(u);

    a2dp = &u->a2dp;

    if (a2dp->sbc.bitpool == bitpool)
        return;

    if (bitpool > a2dp->max_bitpool)
        bitpool = a2dp->max_bitpool;
    else if (bitpool < a2dp->min_bitpool)
        bitpool = a2dp->min_bitpool;

    a2dp->sbc.bitpool = bitpool;

    a2dp->codesize = sbc_get_codesize(&a2dp->sbc);
    a2dp->frame_length = sbc_get_frame_length(&a2dp->sbc);

    pa_log_debug("Bitpool has changed to %u", a2dp->sbc.bitpool);

    u->read_block_size =
        (u->read_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
        / a2dp->frame_length * a2dp->codesize;

    u->write_block_size =
        (u->write_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
        / a2dp->frame_length * a2dp->codesize;

    pa_sink_set_max_request_within_thread(u->sink, u->write_block_size);
    pa_sink_set_fixed_latency_within_thread(u->sink,
            FIXED_LATENCY_PLAYBACK_A2DP + pa_bytes_to_usec(u->write_block_size, &u->sample_spec));
}

/* from IO thread, except in SCO over PCM */
static void bt_transport_config_mtu(struct userdata *u) {
    /* Calculate block sizes */
    if (u->profile == PA_BLUEZ4_PROFILE_HSP || u->profile == PA_BLUEZ4_PROFILE_HFGW) {
        u->read_block_size = u->read_link_mtu;
        u->write_block_size = u->write_link_mtu;
    } else {
        u->read_block_size =
            (u->read_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
            / u->a2dp.frame_length * u->a2dp.codesize;

        u->write_block_size =
            (u->write_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
            / u->a2dp.frame_length * u->a2dp.codesize;
    }

    if (USE_SCO_OVER_PCM(u))
        return;

    if (u->sink) {
        pa_sink_set_max_request_within_thread(u->sink, u->write_block_size);
        pa_sink_set_fixed_latency_within_thread(u->sink,
                                                (u->profile == PA_BLUEZ4_PROFILE_A2DP ?
                                                 FIXED_LATENCY_PLAYBACK_A2DP : FIXED_LATENCY_PLAYBACK_HSP) +
                                                pa_bytes_to_usec(u->write_block_size, &u->sample_spec));
    }

    if (u->source)
        pa_source_set_fixed_latency_within_thread(u->source,
                                                  (u->profile == PA_BLUEZ4_PROFILE_A2DP_SOURCE ?
                                                   FIXED_LATENCY_RECORD_A2DP : FIXED_LATENCY_RECORD_HSP) +
                                                  pa_bytes_to_usec(u->read_block_size, &u->sample_spec));
}

/* from IO thread, except in SCO over PCM */

static void setup_stream(struct userdata *u) {
    struct pollfd *pollfd;
    int one;

    pa_log_info("Transport %s resuming", u->transport->path);

    bt_transport_config_mtu(u);

    pa_make_fd_nonblock(u->stream_fd);
    pa_make_socket_low_delay(u->stream_fd);

    one = 1;
    if (setsockopt(u->stream_fd, SOL_SOCKET, SO_TIMESTAMP, &one, sizeof(one)) < 0)
        pa_log_warn("Failed to enable SO_TIMESTAMP: %s", pa_cstrerror(errno));

    pa_log_debug("Stream properly set up, we're ready to roll!");

    if (u->profile == PA_BLUEZ4_PROFILE_A2DP)
        a2dp_set_bitpool(u, u->a2dp.max_bitpool);

    u->rtpoll_item = pa_rtpoll_item_new(u->rtpoll, PA_RTPOLL_NEVER, 1);
    pollfd = pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL);
    pollfd->fd = u->stream_fd;
    pollfd->events = pollfd->revents = 0;

    u->read_index = u->write_index = 0;
    u->started_at = 0;

    if (u->source)
        u->read_smoother = pa_smoother_new(
                PA_USEC_PER_SEC,
                PA_USEC_PER_SEC*2,
                true,
                true,
                10,
                pa_rtclock_now(),
                true);
}

static void teardown_stream(struct userdata *u) {
    if (u->rtpoll_item) {
        pa_rtpoll_item_free(u->rtpoll_item);
        u->rtpoll_item = NULL;
    }

    if (u->stream_fd >= 0) {
        pa_close(u->stream_fd);
        u->stream_fd = -1;
    }

    if (u->read_smoother) {
        pa_smoother_free(u->read_smoother);
        u->read_smoother = NULL;
    }

    if (u->write_memchunk.memblock) {
        pa_memblock_unref(u->write_memchunk.memblock);
        pa_memchunk_reset(&u->write_memchunk);
    }

    pa_log_debug("Audio stream torn down");
}

static void bt_transport_release(struct userdata *u) {
    pa_assert(u->transport);

    /* Ignore if already released */
    if (!u->transport_acquired)
        return;

    pa_log_debug("Releasing transport %s", u->transport->path);

    pa_bluez4_transport_release(u->transport);

    u->transport_acquired = false;

    teardown_stream(u);
}

static int bt_transport_acquire(struct userdata *u, bool optional) {
    pa_assert(u->transport);

    if (u->transport_acquired)
        return 0;

    pa_log_debug("Acquiring transport %s", u->transport->path);

    u->stream_fd = pa_bluez4_transport_acquire(u->transport, optional, &u->read_link_mtu, &u->write_link_mtu);
    if (u->stream_fd < 0) {
        if (!optional)
            pa_log("Failed to acquire transport %s", u->transport->path);
        else
            pa_log_info("Failed optional acquire of transport %s", u->transport->path);

        return -1;
    }

    u->transport_acquired = true;
    pa_log_info("Transport %s acquired: fd %d", u->transport->path, u->stream_fd);

    return 0;
}

/* Run from IO thread */
static int sink_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SINK(o)->userdata;
    bool failed = false;
    int r;

    pa_assert(u->sink == PA_SINK(o));
    pa_assert(u->transport);

    switch (code) {

        case PA_SINK_MESSAGE_SET_STATE:

            switch ((pa_sink_state_t) PA_PTR_TO_UINT(data)) {

                case PA_SINK_SUSPENDED:
                    /* Ignore if transition is PA_SINK_INIT->PA_SINK_SUSPENDED */
                    if (!PA_SINK_IS_OPENED(u->sink->thread_info.state))
                        break;

                    /* Stop the device if the source is suspended as well */
                    if (!u->source || u->source->state == PA_SOURCE_SUSPENDED)
                        /* We deliberately ignore whether stopping
                         * actually worked. Since the stream_fd is
                         * closed it doesn't really matter */
                        bt_transport_release(u);

                    break;

                case PA_SINK_IDLE:
                case PA_SINK_RUNNING:
                    if (u->sink->thread_info.state != PA_SINK_SUSPENDED)
                        break;

                    /* Resume the device if the source was suspended as well */
                    if (!u->source || !PA_SOURCE_IS_OPENED(u->source->thread_info.state)) {
                        if (bt_transport_acquire(u, false) < 0)
                            failed = true;
                        else
                            setup_stream(u);
                    }
                    break;

                case PA_SINK_UNLINKED:
                case PA_SINK_INIT:
                case PA_SINK_INVALID_STATE:
                    ;
            }
            break;

        case PA_SINK_MESSAGE_GET_LATENCY: {

            if (u->read_smoother) {
                pa_usec_t wi, ri;

                ri = pa_smoother_get(u->read_smoother, pa_rtclock_now());
                wi = pa_bytes_to_usec(u->write_index + u->write_block_size, &u->sample_spec);

                *((pa_usec_t*) data) = wi > ri ? wi - ri : 0;
            } else {
                pa_usec_t ri, wi;

                ri = pa_rtclock_now() - u->started_at;
                wi = pa_bytes_to_usec(u->write_index, &u->sample_spec);

                *((pa_usec_t*) data) = wi > ri ? wi - ri : 0;
            }

            *((pa_usec_t*) data) += u->sink->thread_info.fixed_latency;
            return 0;
        }
    }

    r = pa_sink_process_msg(o, code, data, offset, chunk);

    return (r < 0 || !failed) ? r : -1;
}

/* Run from IO thread */
static int source_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;
    bool failed = false;
    int r;

    pa_assert(u->source == PA_SOURCE(o));
    pa_assert(u->transport);

    switch (code) {

        case PA_SOURCE_MESSAGE_SET_STATE:

            switch ((pa_source_state_t) PA_PTR_TO_UINT(data)) {

                case PA_SOURCE_SUSPENDED:
                    /* Ignore if transition is PA_SOURCE_INIT->PA_SOURCE_SUSPENDED */
                    if (!PA_SOURCE_IS_OPENED(u->source->thread_info.state))
                        break;

                    /* Stop the device if the sink is suspended as well */
                    if (!u->sink || u->sink->state == PA_SINK_SUSPENDED)
                        bt_transport_release(u);

                    if (u->read_smoother)
                        pa_smoother_pause(u->read_smoother, pa_rtclock_now());
                    break;

                case PA_SOURCE_IDLE:
                case PA_SOURCE_RUNNING:
                    if (u->source->thread_info.state != PA_SOURCE_SUSPENDED)
                        break;

                    /* Resume the device if the sink was suspended as well */
                    if (!u->sink || !PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
                        if (bt_transport_acquire(u, false) < 0)
                            failed = true;
                        else
                            setup_stream(u);
                    }
                    /* We don't resume the smoother here. Instead we
                     * wait until the first packet arrives */
                    break;

                case PA_SOURCE_UNLINKED:
                case PA_SOURCE_INIT:
                case PA_SOURCE_INVALID_STATE:
                    ;
            }
            break;

        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            pa_usec_t wi, ri;

            if (u->read_smoother) {
                wi = pa_smoother_get(u->read_smoother, pa_rtclock_now());
                ri = pa_bytes_to_usec(u->read_index, &u->sample_spec);

                *((pa_usec_t*) data) = (wi > ri ? wi - ri : 0) + u->source->thread_info.fixed_latency;
            } else
                *((pa_usec_t*) data) = 0;

            return 0;
        }

    }

    r = pa_source_process_msg(o, code, data, offset, chunk);

    return (r < 0 || !failed) ? r : -1;
}

/* Called from main thread context */
static int device_process_msg(pa_msgobject *obj, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct bluetooth_msg *u = BLUETOOTH_MSG(obj);

    switch (code) {
        case BLUETOOTH_MESSAGE_IO_THREAD_FAILED: {
            if (u->card->module->unload_requested)
                break;

            pa_log_debug("Switching the profile to off due to IO thread failure.");

            pa_assert_se(pa_card_set_profile(u->card, pa_hashmap_get(u->card->profiles, "off"), false) >= 0);
            break;
        }
    }
    return 0;
}

/* Run from IO thread */
static int hsp_process_render(struct userdata *u) {
    int ret = 0;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUEZ4_PROFILE_HSP || u->profile == PA_BLUEZ4_PROFILE_HFGW);
    pa_assert(u->sink);

    /* First, render some data */
    if (!u->write_memchunk.memblock)
        pa_sink_render_full(u->sink, u->write_block_size, &u->write_memchunk);

    pa_assert(u->write_memchunk.length == u->write_block_size);

    for (;;) {
        ssize_t l;
        const void *p;

        /* Now write that data to the socket. The socket is of type
         * SEQPACKET, and we generated the data of the MTU size, so this
         * should just work. */

        p = (const uint8_t *) pa_memblock_acquire_chunk(&u->write_memchunk);
        l = pa_write(u->stream_fd, p, u->write_memchunk.length, &u->stream_write_type);
        pa_memblock_release(u->write_memchunk.memblock);

        pa_assert(l != 0);

        if (l < 0) {

            if (errno == EINTR)
                /* Retry right away if we got interrupted */
                continue;

            else if (errno == EAGAIN)
                /* Hmm, apparently the socket was not writable, give up for now */
                break;

            pa_log_error("Failed to write data to SCO socket: %s", pa_cstrerror(errno));
            ret = -1;
            break;
        }

        pa_assert((size_t) l <= u->write_memchunk.length);

        if ((size_t) l != u->write_memchunk.length) {
            pa_log_error("Wrote memory block to socket only partially! %llu written, wanted to write %llu.",
                        (unsigned long long) l,
                        (unsigned long long) u->write_memchunk.length);
            ret = -1;
            break;
        }

        u->write_index += (uint64_t) u->write_memchunk.length;
        pa_memblock_unref(u->write_memchunk.memblock);
        pa_memchunk_reset(&u->write_memchunk);

        ret = 1;
        break;
    }

    return ret;
}

/* Run from IO thread */
static int hsp_process_push(struct userdata *u) {
    int ret = 0;
    pa_memchunk memchunk;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUEZ4_PROFILE_HSP || u->profile == PA_BLUEZ4_PROFILE_HFGW);
    pa_assert(u->source);
    pa_assert(u->read_smoother);

    memchunk.memblock = pa_memblock_new(u->core->mempool, u->read_block_size);
    memchunk.index = memchunk.length = 0;

    for (;;) {
        ssize_t l;
        void *p;
        struct msghdr m;
        struct cmsghdr *cm;
        uint8_t aux[1024];
        struct iovec iov;
        bool found_tstamp = false;
        pa_usec_t tstamp;

        memset(&m, 0, sizeof(m));
        memset(&aux, 0, sizeof(aux));
        memset(&iov, 0, sizeof(iov));

        m.msg_iov = &iov;
        m.msg_iovlen = 1;
        m.msg_control = aux;
        m.msg_controllen = sizeof(aux);

        p = pa_memblock_acquire(memchunk.memblock);
        iov.iov_base = p;
        iov.iov_len = pa_memblock_get_length(memchunk.memblock);
        l = recvmsg(u->stream_fd, &m, 0);
        pa_memblock_release(memchunk.memblock);

        if (l <= 0) {

            if (l < 0 && errno == EINTR)
                /* Retry right away if we got interrupted */
                continue;

            else if (l < 0 && errno == EAGAIN)
                /* Hmm, apparently the socket was not readable, give up for now. */
                break;

            pa_log_error("Failed to read data from SCO socket: %s", l < 0 ? pa_cstrerror(errno) : "EOF");
            ret = -1;
            break;
        }

        pa_assert((size_t) l <= pa_memblock_get_length(memchunk.memblock));

        /* In some rare occasions, we might receive packets of a very strange
         * size. This could potentially be possible if the SCO packet was
         * received partially over-the-air, or more probably due to hardware
         * issues in our Bluetooth adapter. In these cases, in order to avoid
         * an assertion failure due to unaligned data, just discard the whole
         * packet */
        if (!pa_frame_aligned(l, &u->sample_spec)) {
            pa_log_warn("SCO packet received of unaligned size: %zu", l);
            break;
        }

        memchunk.length = (size_t) l;
        u->read_index += (uint64_t) l;

        for (cm = CMSG_FIRSTHDR(&m); cm; cm = CMSG_NXTHDR(&m, cm))
            if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SO_TIMESTAMP) {
                struct timeval *tv = (struct timeval*) CMSG_DATA(cm);
                pa_rtclock_from_wallclock(tv);
                tstamp = pa_timeval_load(tv);
                found_tstamp = true;
                break;
            }

        if (!found_tstamp) {
            pa_log_warn("Couldn't find SO_TIMESTAMP data in auxiliary recvmsg() data!");
            tstamp = pa_rtclock_now();
        }

        pa_smoother_put(u->read_smoother, tstamp, pa_bytes_to_usec(u->read_index, &u->sample_spec));
        pa_smoother_resume(u->read_smoother, tstamp, true);

        pa_source_post(u->source, &memchunk);

        ret = l;
        break;
    }

    pa_memblock_unref(memchunk.memblock);

    return ret;
}

/* Run from IO thread */
static void a2dp_prepare_buffer(struct userdata *u) {
    size_t min_buffer_size = PA_MAX(u->read_link_mtu, u->write_link_mtu);

    pa_assert(u);

    if (u->a2dp.buffer_size >= min_buffer_size)
        return;

    u->a2dp.buffer_size = 2 * min_buffer_size;
    pa_xfree(u->a2dp.buffer);
    u->a2dp.buffer = pa_xmalloc(u->a2dp.buffer_size);
}

/* Run from IO thread */
static int a2dp_process_render(struct userdata *u) {
    struct a2dp_info *a2dp;
    struct rtp_header *header;
    struct rtp_payload *payload;
    size_t nbytes;
    void *d;
    const void *p;
    size_t to_write, to_encode;
    unsigned frame_count;
    int ret = 0;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUEZ4_PROFILE_A2DP);
    pa_assert(u->sink);

    /* First, render some data */
    if (!u->write_memchunk.memblock)
        pa_sink_render_full(u->sink, u->write_block_size, &u->write_memchunk);

    pa_assert(u->write_memchunk.length == u->write_block_size);

    a2dp_prepare_buffer(u);

    a2dp = &u->a2dp;
    header = a2dp->buffer;
    payload = (struct rtp_payload*) ((uint8_t*) a2dp->buffer + sizeof(*header));

    frame_count = 0;

    /* Try to create a packet of the full MTU */

    p = (const uint8_t *) pa_memblock_acquire_chunk(&u->write_memchunk);
    to_encode = u->write_memchunk.length;

    d = (uint8_t*) a2dp->buffer + sizeof(*header) + sizeof(*payload);
    to_write = a2dp->buffer_size - sizeof(*header) - sizeof(*payload);

    while (PA_LIKELY(to_encode > 0 && to_write > 0)) {
        ssize_t written;
        ssize_t encoded;

        encoded = sbc_encode(&a2dp->sbc,
                             p, to_encode,
                             d, to_write,
                             &written);

        if (PA_UNLIKELY(encoded <= 0)) {
            pa_log_error("SBC encoding error (%li)", (long) encoded);
            pa_memblock_release(u->write_memchunk.memblock);
            return -1;
        }

/*         pa_log_debug("SBC: encoded: %lu; written: %lu", (unsigned long) encoded, (unsigned long) written); */
/*         pa_log_debug("SBC: codesize: %lu; frame_length: %lu", (unsigned long) a2dp->codesize, (unsigned long) a2dp->frame_length); */

        pa_assert_fp((size_t) encoded <= to_encode);
        pa_assert_fp((size_t) encoded == a2dp->codesize);

        pa_assert_fp((size_t) written <= to_write);
        pa_assert_fp((size_t) written == a2dp->frame_length);

        p = (const uint8_t*) p + encoded;
        to_encode -= encoded;

        d = (uint8_t*) d + written;
        to_write -= written;

        frame_count++;
    }

    pa_memblock_release(u->write_memchunk.memblock);

    pa_assert(to_encode == 0);

    PA_ONCE_BEGIN {
        pa_log_debug("Using SBC encoder implementation: %s", pa_strnull(sbc_get_implementation_info(&a2dp->sbc)));
    } PA_ONCE_END;

    /* write it to the fifo */
    memset(a2dp->buffer, 0, sizeof(*header) + sizeof(*payload));
    header->v = 2;
    header->pt = 1;
    header->sequence_number = htons(a2dp->seq_num++);
    header->timestamp = htonl(u->write_index / pa_frame_size(&u->sample_spec));
    header->ssrc = htonl(1);
    payload->frame_count = frame_count;

    nbytes = (uint8_t*) d - (uint8_t*) a2dp->buffer;

    for (;;) {
        ssize_t l;

        l = pa_write(u->stream_fd, a2dp->buffer, nbytes, &u->stream_write_type);

        pa_assert(l != 0);

        if (l < 0) {

            if (errno == EINTR)
                /* Retry right away if we got interrupted */
                continue;

            else if (errno == EAGAIN)
                /* Hmm, apparently the socket was not writable, give up for now */
                break;

            pa_log_error("Failed to write data to socket: %s", pa_cstrerror(errno));
            ret = -1;
            break;
        }

        pa_assert((size_t) l <= nbytes);

        if ((size_t) l != nbytes) {
            pa_log_warn("Wrote memory block to socket only partially! %llu written, wanted to write %llu.",
                        (unsigned long long) l,
                        (unsigned long long) nbytes);
            ret = -1;
            break;
        }

        u->write_index += (uint64_t) u->write_memchunk.length;
        pa_memblock_unref(u->write_memchunk.memblock);
        pa_memchunk_reset(&u->write_memchunk);

        ret = 1;

        break;
    }

    return ret;
}

static int a2dp_process_push(struct userdata *u) {
    int ret = 0;
    pa_memchunk memchunk;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUEZ4_PROFILE_A2DP_SOURCE);
    pa_assert(u->source);
    pa_assert(u->read_smoother);

    memchunk.memblock = pa_memblock_new(u->core->mempool, u->read_block_size);
    memchunk.index = memchunk.length = 0;

    for (;;) {
        bool found_tstamp = false;
        pa_usec_t tstamp;
        struct a2dp_info *a2dp;
        struct rtp_header *header;
        struct rtp_payload *payload;
        const void *p;
        void *d;
        ssize_t l;
        size_t to_write, to_decode;
        size_t total_written = 0;

        a2dp_prepare_buffer(u);

        a2dp = &u->a2dp;
        header = a2dp->buffer;
        payload = (struct rtp_payload*) ((uint8_t*) a2dp->buffer + sizeof(*header));

        l = pa_read(u->stream_fd, a2dp->buffer, a2dp->buffer_size, &u->stream_write_type);

        if (l <= 0) {

            if (l < 0 && errno == EINTR)
                /* Retry right away if we got interrupted */
                continue;

            else if (l < 0 && errno == EAGAIN)
                /* Hmm, apparently the socket was not readable, give up for now. */
                break;

            pa_log_error("Failed to read data from socket: %s", l < 0 ? pa_cstrerror(errno) : "EOF");
            ret = -1;
            break;
        }

        pa_assert((size_t) l <= a2dp->buffer_size);

        /* TODO: get timestamp from rtp */
        if (!found_tstamp) {
            /* pa_log_warn("Couldn't find SO_TIMESTAMP data in auxiliary recvmsg() data!"); */
            tstamp = pa_rtclock_now();
        }

        p = (uint8_t*) a2dp->buffer + sizeof(*header) + sizeof(*payload);
        to_decode = l - sizeof(*header) - sizeof(*payload);

        d = pa_memblock_acquire(memchunk.memblock);
        to_write = memchunk.length = pa_memblock_get_length(memchunk.memblock);

        while (PA_LIKELY(to_decode > 0)) {
            size_t written;
            ssize_t decoded;

            decoded = sbc_decode(&a2dp->sbc,
                                 p, to_decode,
                                 d, to_write,
                                 &written);

            if (PA_UNLIKELY(decoded <= 0)) {
                pa_log_error("SBC decoding error (%li)", (long) decoded);
                pa_memblock_release(memchunk.memblock);
                pa_memblock_unref(memchunk.memblock);
                return 0;
            }

/*             pa_log_debug("SBC: decoded: %lu; written: %lu", (unsigned long) decoded, (unsigned long) written); */
/*             pa_log_debug("SBC: frame_length: %lu; codesize: %lu", (unsigned long) a2dp->frame_length, (unsigned long) a2dp->codesize); */

            total_written += written;

            /* Reset frame length, it can be changed due to bitpool change */
            a2dp->frame_length = sbc_get_frame_length(&a2dp->sbc);

            pa_assert_fp((size_t) decoded <= to_decode);
            pa_assert_fp((size_t) decoded == a2dp->frame_length);

            pa_assert_fp((size_t) written == a2dp->codesize);

            p = (const uint8_t*) p + decoded;
            to_decode -= decoded;

            d = (uint8_t*) d + written;
            to_write -= written;
        }

        u->read_index += (uint64_t) total_written;
        pa_smoother_put(u->read_smoother, tstamp, pa_bytes_to_usec(u->read_index, &u->sample_spec));
        pa_smoother_resume(u->read_smoother, tstamp, true);

        memchunk.length -= to_write;

        pa_memblock_release(memchunk.memblock);

        pa_source_post(u->source, &memchunk);

        ret = l;
        break;
    }

    pa_memblock_unref(memchunk.memblock);

    return ret;
}

static void a2dp_reduce_bitpool(struct userdata *u) {
    struct a2dp_info *a2dp;
    uint8_t bitpool;

    pa_assert(u);

    a2dp = &u->a2dp;

    /* Check if bitpool is already at its limit */
    if (a2dp->sbc.bitpool <= BITPOOL_DEC_LIMIT)
        return;

    bitpool = a2dp->sbc.bitpool - BITPOOL_DEC_STEP;

    if (bitpool < BITPOOL_DEC_LIMIT)
        bitpool = BITPOOL_DEC_LIMIT;

    a2dp_set_bitpool(u, bitpool);
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;
    unsigned do_write = 0;
    unsigned pending_read_bytes = 0;
    bool writable = false;

    pa_assert(u);
    pa_assert(u->transport);

    pa_log_debug("IO Thread starting up");

    if (u->core->realtime_scheduling)
        pa_make_realtime(u->core->realtime_priority);

    pa_thread_mq_install(&u->thread_mq);

    /* Setup the stream only if the transport was already acquired */
    if (u->transport_acquired)
        setup_stream(u);

    for (;;) {
        struct pollfd *pollfd;
        int ret;
        bool disable_timer = true;

        pollfd = u->rtpoll_item ? pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL) : NULL;

        if (u->source && PA_SOURCE_IS_LINKED(u->source->thread_info.state)) {

            /* We should send two blocks to the device before we expect
             * a response. */

            if (u->write_index == 0 && u->read_index <= 0)
                do_write = 2;

            if (pollfd && (pollfd->revents & POLLIN)) {
                int n_read;

                if (u->profile == PA_BLUEZ4_PROFILE_HSP || u->profile == PA_BLUEZ4_PROFILE_HFGW)
                    n_read = hsp_process_push(u);
                else
                    n_read = a2dp_process_push(u);

                if (n_read < 0)
                    goto io_fail;

                if (n_read > 0) {
                    /* We just read something, so we are supposed to write something, too */
                    pending_read_bytes += n_read;
                    do_write += pending_read_bytes / u->write_block_size;
                    pending_read_bytes = pending_read_bytes % u->write_block_size;
                }
            }
        }

        if (u->sink && PA_SINK_IS_LINKED(u->sink->thread_info.state)) {

            if (PA_UNLIKELY(u->sink->thread_info.rewind_requested))
                pa_sink_process_rewind(u->sink, 0);

            if (pollfd) {
                if (pollfd->revents & POLLOUT)
                    writable = true;

                if ((!u->source || !PA_SOURCE_IS_LINKED(u->source->thread_info.state)) && do_write <= 0 && writable) {
                    pa_usec_t time_passed;
                    pa_usec_t audio_sent;

                    /* Hmm, there is no input stream we could synchronize
                     * to. So let's do things by time */

                    time_passed = pa_rtclock_now() - u->started_at;
                    audio_sent = pa_bytes_to_usec(u->write_index, &u->sample_spec);

                    if (audio_sent <= time_passed) {
                        pa_usec_t audio_to_send = time_passed - audio_sent;

                        /* Never try to catch up for more than 100ms */
                        if (u->write_index > 0 && audio_to_send > MAX_PLAYBACK_CATCH_UP_USEC) {
                            pa_usec_t skip_usec;
                            uint64_t skip_bytes;

                            skip_usec = audio_to_send - MAX_PLAYBACK_CATCH_UP_USEC;
                            skip_bytes = pa_usec_to_bytes(skip_usec, &u->sample_spec);

                            if (skip_bytes > 0) {
                                pa_memchunk tmp;

                                pa_log_warn("Skipping %llu us (= %llu bytes) in audio stream",
                                            (unsigned long long) skip_usec,
                                            (unsigned long long) skip_bytes);

                                pa_sink_render_full(u->sink, skip_bytes, &tmp);
                                pa_memblock_unref(tmp.memblock);
                                u->write_index += skip_bytes;

                                if (u->profile == PA_BLUEZ4_PROFILE_A2DP)
                                    a2dp_reduce_bitpool(u);
                            }
                        }

                        do_write = 1;
                        pending_read_bytes = 0;
                    }
                }

                if (writable && do_write > 0) {
                    int n_written;

                    if (u->write_index <= 0)
                        u->started_at = pa_rtclock_now();

                    if (u->profile == PA_BLUEZ4_PROFILE_A2DP) {
                        if ((n_written = a2dp_process_render(u)) < 0)
                            goto io_fail;
                    } else {
                        if ((n_written = hsp_process_render(u)) < 0)
                            goto io_fail;
                    }

                    if (n_written == 0)
                        pa_log("Broken kernel: we got EAGAIN on write() after POLLOUT!");

                    do_write -= n_written;
                    writable = false;
                }

                if ((!u->source || !PA_SOURCE_IS_LINKED(u->source->thread_info.state)) && do_write <= 0) {
                    pa_usec_t sleep_for;
                    pa_usec_t time_passed, next_write_at;

                    if (writable) {
                        /* Hmm, there is no input stream we could synchronize
                         * to. So let's estimate when we need to wake up the latest */
                        time_passed = pa_rtclock_now() - u->started_at;
                        next_write_at = pa_bytes_to_usec(u->write_index, &u->sample_spec);
                        sleep_for = time_passed < next_write_at ? next_write_at - time_passed : 0;
                        /* pa_log("Sleeping for %lu; time passed %lu, next write at %lu", (unsigned long) sleep_for, (unsigned long) time_passed, (unsigned long)next_write_at); */
                    } else
                        /* drop stream every 500 ms */
                        sleep_for = PA_USEC_PER_MSEC * 500;

                    pa_rtpoll_set_timer_relative(u->rtpoll, sleep_for);
                    disable_timer = false;
                }
            }
        }

        if (disable_timer)
            pa_rtpoll_set_timer_disabled(u->rtpoll);

        /* Hmm, nothing to do. Let's sleep */
        if (pollfd)
            pollfd->events = (short) (((u->sink && PA_SINK_IS_LINKED(u->sink->thread_info.state) && !writable) ? POLLOUT : 0) |
                                      (u->source && PA_SOURCE_IS_LINKED(u->source->thread_info.state) ? POLLIN : 0));

        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0) {
            pa_log_debug("pa_rtpoll_run failed with: %d", ret);
            goto fail;
        }
        if (ret == 0) {
            pa_log_debug("IO thread shutdown requested, stopping cleanly");
            bt_transport_release(u);
            goto finish;
        }

        pollfd = u->rtpoll_item ? pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL) : NULL;

        if (pollfd && (pollfd->revents & ~(POLLOUT|POLLIN))) {
            pa_log_info("FD error: %s%s%s%s",
                        pollfd->revents & POLLERR ? "POLLERR " :"",
                        pollfd->revents & POLLHUP ? "POLLHUP " :"",
                        pollfd->revents & POLLPRI ? "POLLPRI " :"",
                        pollfd->revents & POLLNVAL ? "POLLNVAL " :"");
            goto io_fail;
        }

        continue;

io_fail:
        /* In case of HUP, just tear down the streams */
        if (!pollfd || (pollfd->revents & POLLHUP) == 0)
            goto fail;

        do_write = 0;
        pending_read_bytes = 0;
        writable = false;

        teardown_stream(u);
    }

fail:
    /* If this was no regular exit from the loop we have to continue processing messages until we receive PA_MESSAGE_SHUTDOWN */
    pa_log_debug("IO thread failed");
    pa_asyncmsgq_post(pa_thread_mq_get()->outq, PA_MSGOBJECT(u->msg), BLUETOOTH_MESSAGE_IO_THREAD_FAILED, NULL, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("IO thread shutting down");
}

static pa_available_t transport_state_to_availability(pa_bluez4_transport_state_t state) {
    if (state == PA_BLUEZ4_TRANSPORT_STATE_DISCONNECTED)
        return PA_AVAILABLE_NO;
    else if (state >= PA_BLUEZ4_TRANSPORT_STATE_PLAYING)
        return PA_AVAILABLE_YES;
    else
        return PA_AVAILABLE_UNKNOWN;
}

static pa_direction_t get_profile_direction(pa_bluez4_profile_t p) {
    static const pa_direction_t profile_direction[] = {
        [PA_BLUEZ4_PROFILE_A2DP] = PA_DIRECTION_OUTPUT,
        [PA_BLUEZ4_PROFILE_A2DP_SOURCE] = PA_DIRECTION_INPUT,
        [PA_BLUEZ4_PROFILE_HSP] = PA_DIRECTION_INPUT | PA_DIRECTION_OUTPUT,
        [PA_BLUEZ4_PROFILE_HFGW] = PA_DIRECTION_INPUT | PA_DIRECTION_OUTPUT,
        [PA_BLUEZ4_PROFILE_OFF] = 0
    };

    return profile_direction[p];
}

/* Run from main thread */
static pa_available_t get_port_availability(struct userdata *u, pa_direction_t direction) {
    pa_available_t result = PA_AVAILABLE_NO;
    unsigned i;

    pa_assert(u);
    pa_assert(u->device);

    for (i = 0; i < PA_BLUEZ4_PROFILE_COUNT; i++) {
        pa_bluez4_transport *transport;

        if (!(get_profile_direction(i) & direction))
            continue;

        if (!(transport = u->device->transports[i]))
            continue;

        switch(transport->state) {
            case PA_BLUEZ4_TRANSPORT_STATE_DISCONNECTED:
                continue;

            case PA_BLUEZ4_TRANSPORT_STATE_IDLE:
                if (result == PA_AVAILABLE_NO)
                    result = PA_AVAILABLE_UNKNOWN;

                break;

            case PA_BLUEZ4_TRANSPORT_STATE_PLAYING:
                return PA_AVAILABLE_YES;
        }
    }

    return result;
}

/* Run from main thread */
static void handle_transport_state_change(struct userdata *u, struct pa_bluez4_transport *transport) {
    bool acquire = false;
    bool release = false;
    pa_bluez4_profile_t profile;
    pa_card_profile *cp;
    pa_bluez4_transport_state_t state;
    pa_device_port *port;

    pa_assert(u);
    pa_assert(transport);

    profile = transport->profile;
    state = transport->state;

    /* Update profile availability */
    if (!(cp = pa_hashmap_get(u->card->profiles, pa_bluez4_profile_to_string(profile))))
        return;

    pa_card_profile_set_available(cp, transport_state_to_availability(state));

    /* Update port availability */
    pa_assert_se(port = pa_hashmap_get(u->card->ports, u->output_port_name));
    pa_device_port_set_available(port, get_port_availability(u, PA_DIRECTION_OUTPUT));

    pa_assert_se(port = pa_hashmap_get(u->card->ports, u->input_port_name));
    pa_device_port_set_available(port, get_port_availability(u, PA_DIRECTION_INPUT));

    /* Acquire or release transport as needed */
    acquire = (state == PA_BLUEZ4_TRANSPORT_STATE_PLAYING && u->profile == profile);
    release = (state != PA_BLUEZ4_TRANSPORT_STATE_PLAYING && u->profile == profile);

    if (acquire)
        if (bt_transport_acquire(u, true) >= 0) {
            if (u->source) {
                pa_log_debug("Resuming source %s, because the bluetooth audio state changed to 'playing'.", u->source->name);
                pa_source_suspend(u->source, false, PA_SUSPEND_IDLE|PA_SUSPEND_USER);
            }

            if (u->sink) {
                pa_log_debug("Resuming sink %s, because the bluetooth audio state changed to 'playing'.", u->sink->name);
                pa_sink_suspend(u->sink, false, PA_SUSPEND_IDLE|PA_SUSPEND_USER);
            }
        }

    if (release && u->transport_acquired) {
        /* FIXME: this release is racy, since the audio stream might have
           been set up again in the meantime (but not processed yet by PA).
           BlueZ should probably release the transport automatically, and
           in that case we would just mark the transport as released */

        /* Remote side closed the stream so we consider it PA_SUSPEND_USER */
        if (u->source) {
            pa_log_debug("Suspending source %s, because the remote end closed the stream.", u->source->name);
            pa_source_suspend(u->source, true, PA_SUSPEND_USER);
        }

        if (u->sink) {
            pa_log_debug("Suspending sink %s, because the remote end closed the stream.", u->sink->name);
            pa_sink_suspend(u->sink, true, PA_SUSPEND_USER);
        }
    }
}

/* Run from main thread */
static void sink_set_volume_cb(pa_sink *s) {
    uint16_t gain;
    pa_volume_t volume;
    struct userdata *u;
    char *k;

    pa_assert(s);
    pa_assert(s->core);

    k = pa_sprintf_malloc("bluetooth-device@%p", (void*) s);
    u = pa_shared_get(s->core, k);
    pa_xfree(k);

    pa_assert(u);
    pa_assert(u->sink == s);
    pa_assert(u->profile == PA_BLUEZ4_PROFILE_HSP);
    pa_assert(u->transport);

    gain = (dbus_uint16_t) round((double) pa_cvolume_max(&s->real_volume) * HSP_MAX_GAIN / PA_VOLUME_NORM);
    volume = (pa_volume_t) round((double) gain * PA_VOLUME_NORM / HSP_MAX_GAIN);

    pa_cvolume_set(&s->real_volume, u->sample_spec.channels, volume);

    pa_bluez4_transport_set_speaker_gain(u->transport, gain);
}

/* Run from main thread */
static void source_set_volume_cb(pa_source *s) {
    uint16_t gain;
    pa_volume_t volume;
    struct userdata *u;
    char *k;

    pa_assert(s);
    pa_assert(s->core);

    k = pa_sprintf_malloc("bluetooth-device@%p", (void*) s);
    u = pa_shared_get(s->core, k);
    pa_xfree(k);

    pa_assert(u);
    pa_assert(u->source == s);
    pa_assert(u->profile == PA_BLUEZ4_PROFILE_HSP);
    pa_assert(u->transport);

    gain = (dbus_uint16_t) round((double) pa_cvolume_max(&s->real_volume) * HSP_MAX_GAIN / PA_VOLUME_NORM);
    volume = (pa_volume_t) round((double) gain * PA_VOLUME_NORM / HSP_MAX_GAIN);

    pa_cvolume_set(&s->real_volume, u->sample_spec.channels, volume);

    pa_bluez4_transport_set_microphone_gain(u->transport, gain);
}

/* Run from main thread */
static char *get_name(const char *type, pa_modargs *ma, const char *device_id, const char *profile, bool *namereg_fail) {
    char *t;
    const char *n;

    pa_assert(type);
    pa_assert(ma);
    pa_assert(device_id);
    pa_assert(namereg_fail);

    t = pa_sprintf_malloc("%s_name", type);
    n = pa_modargs_get_value(ma, t, NULL);
    pa_xfree(t);

    if (n) {
        *namereg_fail = true;
        return pa_xstrdup(n);
    }

    if ((n = pa_modargs_get_value(ma, "name", NULL)))
        *namereg_fail = true;
    else {
        n = device_id;
        *namereg_fail = false;
    }

    if (profile)
        return pa_sprintf_malloc("bluez_%s.%s.%s", type, n, profile);
    else
        return pa_sprintf_malloc("bluez_%s.%s", type, n);
}

static int sco_over_pcm_state_update(struct userdata *u, bool changed) {
    pa_assert(u);
    pa_assert(USE_SCO_OVER_PCM(u));

    if (PA_SINK_IS_OPENED(pa_sink_get_state(u->hsp.sco_sink)) ||
        PA_SOURCE_IS_OPENED(pa_source_get_state(u->hsp.sco_source))) {

        if (u->stream_fd >= 0)
            return 0;

        pa_log_debug("Resuming SCO over PCM");
        if (init_profile(u) < 0) {
            pa_log("Can't resume SCO over PCM");
            return -1;
        }

        if (bt_transport_acquire(u, false) < 0)
            return -1;

        setup_stream(u);

        return 0;
    }

    if (changed) {
        if (u->stream_fd < 0)
            return 0;

        pa_log_debug("Closing SCO over PCM");

        bt_transport_release(u);
    }

    return 0;
}

static pa_hook_result_t sink_state_changed_cb(pa_core *c, pa_sink *s, struct userdata *u) {
    pa_assert(c);
    pa_sink_assert_ref(s);
    pa_assert(u);

    if (!USE_SCO_OVER_PCM(u) || s != u->hsp.sco_sink)
        return PA_HOOK_OK;

    sco_over_pcm_state_update(u, true);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_state_changed_cb(pa_core *c, pa_source *s, struct userdata *u) {
    pa_assert(c);
    pa_source_assert_ref(s);
    pa_assert(u);

    if (!USE_SCO_OVER_PCM(u) || s != u->hsp.sco_source)
        return PA_HOOK_OK;

    sco_over_pcm_state_update(u, true);

    return PA_HOOK_OK;
}

static pa_hook_result_t transport_nrec_changed_cb(pa_bluez4_discovery *y, pa_bluez4_transport *t, struct userdata *u) {
    pa_proplist *p;

    pa_assert(t);
    pa_assert(u);

    if (t != u->transport)
        return PA_HOOK_OK;

    p = pa_proplist_new();
    pa_proplist_sets(p, "bluetooth.nrec", t->nrec ? "1" : "0");
    pa_source_update_proplist(u->source, PA_UPDATE_REPLACE, p);
    pa_proplist_free(p);

    return PA_HOOK_OK;
}

static pa_hook_result_t transport_microphone_gain_changed_cb(pa_bluez4_discovery *y, pa_bluez4_transport *t,
                                                             struct userdata *u) {
    pa_cvolume v;

    pa_assert(t);
    pa_assert(u);

    if (t != u->transport)
        return PA_HOOK_OK;

    pa_assert(u->source);

    pa_cvolume_set(&v, u->sample_spec.channels,
                   (pa_volume_t) round((double) t->microphone_gain * PA_VOLUME_NORM / HSP_MAX_GAIN));
    pa_source_volume_changed(u->source, &v);

    return PA_HOOK_OK;
}

static pa_hook_result_t transport_speaker_gain_changed_cb(pa_bluez4_discovery *y, pa_bluez4_transport *t,
                                                          struct userdata *u) {
    pa_cvolume v;

    pa_assert(t);
    pa_assert(u);

    if (t != u->transport)
        return PA_HOOK_OK;

    pa_assert(u->sink);

    pa_cvolume_set(&v, u->sample_spec.channels, (pa_volume_t) round((double) t->speaker_gain * PA_VOLUME_NORM / HSP_MAX_GAIN));
    pa_sink_volume_changed(u->sink, &v);

    return PA_HOOK_OK;
}

static void connect_ports(struct userdata *u, void *sink_or_source_new_data, pa_direction_t direction) {
    pa_device_port *port;

    if (direction == PA_DIRECTION_OUTPUT) {
        pa_sink_new_data *sink_new_data = sink_or_source_new_data;

        pa_assert_se(port = pa_hashmap_get(u->card->ports, u->output_port_name));
        pa_assert_se(pa_hashmap_put(sink_new_data->ports, port->name, port) >= 0);
        pa_device_port_ref(port);
    } else {
        pa_source_new_data *source_new_data = sink_or_source_new_data;

        pa_assert_se(port = pa_hashmap_get(u->card->ports, u->input_port_name));
        pa_assert_se(pa_hashmap_put(source_new_data->ports, port->name, port) >= 0);
        pa_device_port_ref(port);
    }
}

static int sink_set_port_cb(pa_sink *s, pa_device_port *p) {
    return 0;
}

static int source_set_port_cb(pa_source *s, pa_device_port *p) {
    return 0;
}

/* Run from main thread */
static int add_sink(struct userdata *u) {
    char *k;

    pa_assert(u->transport);

    if (USE_SCO_OVER_PCM(u)) {
        pa_proplist *p;

        u->sink = u->hsp.sco_sink;
        p = pa_proplist_new();
        pa_proplist_sets(p, "bluetooth.protocol", pa_bluez4_profile_to_string(u->profile));
        pa_proplist_update(u->sink->proplist, PA_UPDATE_MERGE, p);
        pa_proplist_free(p);
    } else {
        pa_sink_new_data data;
        bool b;

        pa_sink_new_data_init(&data);
        data.driver = __FILE__;
        data.module = u->module;
        pa_sink_new_data_set_sample_spec(&data, &u->sample_spec);
        pa_proplist_sets(data.proplist, "bluetooth.protocol", pa_bluez4_profile_to_string(u->profile));
        if (u->profile == PA_BLUEZ4_PROFILE_HSP)
            pa_proplist_sets(data.proplist, PA_PROP_DEVICE_INTENDED_ROLES, "phone");
        data.card = u->card;
        data.name = get_name("sink", u->modargs, u->address, pa_bluez4_profile_to_string(u->profile), &b);
        data.namereg_fail = b;

        if (pa_modargs_get_proplist(u->modargs, "sink_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
            pa_log("Invalid properties");
            pa_sink_new_data_done(&data);
            return -1;
        }
        connect_ports(u, &data, PA_DIRECTION_OUTPUT);

        if (!u->transport_acquired)
            switch (u->profile) {
                case PA_BLUEZ4_PROFILE_A2DP:
                case PA_BLUEZ4_PROFILE_HSP:
                    pa_assert_not_reached(); /* Profile switch should have failed */
                    break;
                case PA_BLUEZ4_PROFILE_HFGW:
                    data.suspend_cause = PA_SUSPEND_USER;
                    break;
                case PA_BLUEZ4_PROFILE_A2DP_SOURCE:
                case PA_BLUEZ4_PROFILE_OFF:
                    pa_assert_not_reached();
            }

        u->sink = pa_sink_new(u->core, &data, PA_SINK_HARDWARE|PA_SINK_LATENCY);
        pa_sink_new_data_done(&data);

        if (!u->sink) {
            pa_log_error("Failed to create sink");
            return -1;
        }

        u->sink->userdata = u;
        u->sink->parent.process_msg = sink_process_msg;
        u->sink->set_port = sink_set_port_cb;
    }

    if (u->profile == PA_BLUEZ4_PROFILE_HSP) {
        pa_sink_set_set_volume_callback(u->sink, sink_set_volume_cb);
        u->sink->n_volume_steps = 16;

        k = pa_sprintf_malloc("bluetooth-device@%p", (void*) u->sink);
        pa_shared_set(u->core, k, u);
        pa_xfree(k);
    }

    return 0;
}

/* Run from main thread */
static int add_source(struct userdata *u) {
    char *k;

    pa_assert(u->transport);

    if (USE_SCO_OVER_PCM(u)) {
        u->source = u->hsp.sco_source;
        pa_proplist_sets(u->source->proplist, "bluetooth.protocol", pa_bluez4_profile_to_string(u->profile));
    } else {
        pa_source_new_data data;
        bool b;

        pa_source_new_data_init(&data);
        data.driver = __FILE__;
        data.module = u->module;
        pa_source_new_data_set_sample_spec(&data, &u->sample_spec);
        pa_proplist_sets(data.proplist, "bluetooth.protocol", pa_bluez4_profile_to_string(u->profile));
        if (u->profile == PA_BLUEZ4_PROFILE_HSP)
            pa_proplist_sets(data.proplist, PA_PROP_DEVICE_INTENDED_ROLES, "phone");

        data.card = u->card;
        data.name = get_name("source", u->modargs, u->address, pa_bluez4_profile_to_string(u->profile), &b);
        data.namereg_fail = b;

        if (pa_modargs_get_proplist(u->modargs, "source_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
            pa_log("Invalid properties");
            pa_source_new_data_done(&data);
            return -1;
        }

        connect_ports(u, &data, PA_DIRECTION_INPUT);

        if (!u->transport_acquired)
            switch (u->profile) {
                case PA_BLUEZ4_PROFILE_HSP:
                    pa_assert_not_reached(); /* Profile switch should have failed */
                    break;
                case PA_BLUEZ4_PROFILE_A2DP_SOURCE:
                case PA_BLUEZ4_PROFILE_HFGW:
                    data.suspend_cause = PA_SUSPEND_USER;
                    break;
                case PA_BLUEZ4_PROFILE_A2DP:
                case PA_BLUEZ4_PROFILE_OFF:
                    pa_assert_not_reached();
            }

        u->source = pa_source_new(u->core, &data, PA_SOURCE_HARDWARE|PA_SOURCE_LATENCY);
        pa_source_new_data_done(&data);

        if (!u->source) {
            pa_log_error("Failed to create source");
            return -1;
        }

        u->source->userdata = u;
        u->source->parent.process_msg = source_process_msg;
        u->source->set_port = source_set_port_cb;
    }

    if ((u->profile == PA_BLUEZ4_PROFILE_HSP) || (u->profile == PA_BLUEZ4_PROFILE_HFGW)) {
        pa_bluez4_transport *t = u->transport;
        pa_proplist_sets(u->source->proplist, "bluetooth.nrec", t->nrec ? "1" : "0");
    }

    if (u->profile == PA_BLUEZ4_PROFILE_HSP) {
        pa_source_set_set_volume_callback(u->source, source_set_volume_cb);
        u->source->n_volume_steps = 16;

        k = pa_sprintf_malloc("bluetooth-device@%p", (void*) u->source);
        pa_shared_set(u->core, k, u);
        pa_xfree(k);
    }

    return 0;
}

static void bt_transport_config_a2dp(struct userdata *u) {
    const pa_bluez4_transport *t;
    struct a2dp_info *a2dp = &u->a2dp;
    a2dp_sbc_t *config;

    t = u->transport;
    pa_assert(t);

    config = (a2dp_sbc_t *) t->config;

    u->sample_spec.format = PA_SAMPLE_S16LE;

    if (a2dp->sbc_initialized)
        sbc_reinit(&a2dp->sbc, 0);
    else
        sbc_init(&a2dp->sbc, 0);
    a2dp->sbc_initialized = true;

    switch (config->frequency) {
        case SBC_SAMPLING_FREQ_16000:
            a2dp->sbc.frequency = SBC_FREQ_16000;
            u->sample_spec.rate = 16000U;
            break;
        case SBC_SAMPLING_FREQ_32000:
            a2dp->sbc.frequency = SBC_FREQ_32000;
            u->sample_spec.rate = 32000U;
            break;
        case SBC_SAMPLING_FREQ_44100:
            a2dp->sbc.frequency = SBC_FREQ_44100;
            u->sample_spec.rate = 44100U;
            break;
        case SBC_SAMPLING_FREQ_48000:
            a2dp->sbc.frequency = SBC_FREQ_48000;
            u->sample_spec.rate = 48000U;
            break;
        default:
            pa_assert_not_reached();
    }

    switch (config->channel_mode) {
        case SBC_CHANNEL_MODE_MONO:
            a2dp->sbc.mode = SBC_MODE_MONO;
            u->sample_spec.channels = 1;
            break;
        case SBC_CHANNEL_MODE_DUAL_CHANNEL:
            a2dp->sbc.mode = SBC_MODE_DUAL_CHANNEL;
            u->sample_spec.channels = 2;
            break;
        case SBC_CHANNEL_MODE_STEREO:
            a2dp->sbc.mode = SBC_MODE_STEREO;
            u->sample_spec.channels = 2;
            break;
        case SBC_CHANNEL_MODE_JOINT_STEREO:
            a2dp->sbc.mode = SBC_MODE_JOINT_STEREO;
            u->sample_spec.channels = 2;
            break;
        default:
            pa_assert_not_reached();
    }

    switch (config->allocation_method) {
        case SBC_ALLOCATION_SNR:
            a2dp->sbc.allocation = SBC_AM_SNR;
            break;
        case SBC_ALLOCATION_LOUDNESS:
            a2dp->sbc.allocation = SBC_AM_LOUDNESS;
            break;
        default:
            pa_assert_not_reached();
    }

    switch (config->subbands) {
        case SBC_SUBBANDS_4:
            a2dp->sbc.subbands = SBC_SB_4;
            break;
        case SBC_SUBBANDS_8:
            a2dp->sbc.subbands = SBC_SB_8;
            break;
        default:
            pa_assert_not_reached();
    }

    switch (config->block_length) {
        case SBC_BLOCK_LENGTH_4:
            a2dp->sbc.blocks = SBC_BLK_4;
            break;
        case SBC_BLOCK_LENGTH_8:
            a2dp->sbc.blocks = SBC_BLK_8;
            break;
        case SBC_BLOCK_LENGTH_12:
            a2dp->sbc.blocks = SBC_BLK_12;
            break;
        case SBC_BLOCK_LENGTH_16:
            a2dp->sbc.blocks = SBC_BLK_16;
            break;
        default:
            pa_assert_not_reached();
    }

    a2dp->min_bitpool = config->min_bitpool;
    a2dp->max_bitpool = config->max_bitpool;

    /* Set minimum bitpool for source to get the maximum possible block_size */
    a2dp->sbc.bitpool = u->profile == PA_BLUEZ4_PROFILE_A2DP ? a2dp->max_bitpool : a2dp->min_bitpool;
    a2dp->codesize = sbc_get_codesize(&a2dp->sbc);
    a2dp->frame_length = sbc_get_frame_length(&a2dp->sbc);

    pa_log_info("SBC parameters:\n\tallocation=%u\n\tsubbands=%u\n\tblocks=%u\n\tbitpool=%u\n",
                a2dp->sbc.allocation, a2dp->sbc.subbands, a2dp->sbc.blocks, a2dp->sbc.bitpool);
}

static void bt_transport_config(struct userdata *u) {
    if (u->profile == PA_BLUEZ4_PROFILE_HSP || u->profile == PA_BLUEZ4_PROFILE_HFGW) {
        u->sample_spec.format = PA_SAMPLE_S16LE;
        u->sample_spec.channels = 1;
        u->sample_spec.rate = 8000;
    } else
        bt_transport_config_a2dp(u);
}

/* Run from main thread */
static pa_hook_result_t transport_state_changed_cb(pa_bluez4_discovery *y, pa_bluez4_transport *t, struct userdata *u) {
    pa_assert(t);
    pa_assert(u);

    if (t == u->transport && t->state == PA_BLUEZ4_TRANSPORT_STATE_DISCONNECTED)
        pa_assert_se(pa_card_set_profile(u->card, pa_hashmap_get(u->card->profiles, "off"), false) >= 0);

    if (t->device == u->device)
        handle_transport_state_change(u, t);

    return PA_HOOK_OK;
}

/* Run from main thread */
static int setup_transport(struct userdata *u) {
    pa_bluez4_transport *t;

    pa_assert(u);
    pa_assert(!u->transport);
    pa_assert(u->profile != PA_BLUEZ4_PROFILE_OFF);

    /* check if profile has a transport */
    t = u->device->transports[u->profile];
    if (!t || t->state == PA_BLUEZ4_TRANSPORT_STATE_DISCONNECTED) {
        pa_log_warn("Profile has no transport");
        return -1;
    }

    u->transport = t;

    if (u->profile == PA_BLUEZ4_PROFILE_A2DP_SOURCE || u->profile == PA_BLUEZ4_PROFILE_HFGW)
        bt_transport_acquire(u, true); /* In case of error, the sink/sources will be created suspended */
    else if (bt_transport_acquire(u, false) < 0)
        return -1; /* We need to fail here until the interactions with module-suspend-on-idle and alike get improved */

    bt_transport_config(u);

    return 0;
}

/* Run from main thread */
static int init_profile(struct userdata *u) {
    int r = 0;
    pa_assert(u);
    pa_assert(u->profile != PA_BLUEZ4_PROFILE_OFF);

    if (setup_transport(u) < 0)
        return -1;

    pa_assert(u->transport);

    if (u->profile == PA_BLUEZ4_PROFILE_A2DP ||
        u->profile == PA_BLUEZ4_PROFILE_HSP ||
        u->profile == PA_BLUEZ4_PROFILE_HFGW)
        if (add_sink(u) < 0)
            r = -1;

    if (u->profile == PA_BLUEZ4_PROFILE_HSP ||
        u->profile == PA_BLUEZ4_PROFILE_A2DP_SOURCE ||
        u->profile == PA_BLUEZ4_PROFILE_HFGW)
        if (add_source(u) < 0)
            r = -1;

    return r;
}

/* Run from main thread */
static void stop_thread(struct userdata *u) {
    char *k;

    pa_assert(u);

    if (u->sink && !USE_SCO_OVER_PCM(u))
        pa_sink_unlink(u->sink);

    if (u->source && !USE_SCO_OVER_PCM(u))
        pa_source_unlink(u->source);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
        u->thread = NULL;
    }

    if (u->rtpoll_item) {
        pa_rtpoll_item_free(u->rtpoll_item);
        u->rtpoll_item = NULL;
    }

    if (u->rtpoll) {
        pa_thread_mq_done(&u->thread_mq);

        pa_rtpoll_free(u->rtpoll);
        u->rtpoll = NULL;
    }

    if (u->transport) {
        bt_transport_release(u);
        u->transport = NULL;
    }

    if (u->sink) {
        if (u->profile == PA_BLUEZ4_PROFILE_HSP) {
            k = pa_sprintf_malloc("bluetooth-device@%p", (void*) u->sink);
            pa_shared_remove(u->core, k);
            pa_xfree(k);
        }

        pa_sink_unref(u->sink);
        u->sink = NULL;
    }

    if (u->source) {
        if (u->profile == PA_BLUEZ4_PROFILE_HSP) {
            k = pa_sprintf_malloc("bluetooth-device@%p", (void*) u->source);
            pa_shared_remove(u->core, k);
            pa_xfree(k);
        }

        pa_source_unref(u->source);
        u->source = NULL;
    }

    if (u->read_smoother) {
        pa_smoother_free(u->read_smoother);
        u->read_smoother = NULL;
    }
}

/* Run from main thread */
static int start_thread(struct userdata *u) {
    pa_assert(u);
    pa_assert(!u->thread);
    pa_assert(!u->rtpoll);
    pa_assert(!u->rtpoll_item);

    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, u->core->mainloop, u->rtpoll);

    if (USE_SCO_OVER_PCM(u)) {
        if (sco_over_pcm_state_update(u, false) < 0) {
            char *k;

            if (u->sink) {
                k = pa_sprintf_malloc("bluetooth-device@%p", (void*) u->sink);
                pa_shared_remove(u->core, k);
                pa_xfree(k);
                u->sink = NULL;
            }
            if (u->source) {
                k = pa_sprintf_malloc("bluetooth-device@%p", (void*) u->source);
                pa_shared_remove(u->core, k);
                pa_xfree(k);
                u->source = NULL;
            }
            return -1;
        }

        pa_sink_ref(u->sink);
        pa_source_ref(u->source);
        /* FIXME: monitor stream_fd error */
        return 0;
    }

    if (!(u->thread = pa_thread_new("bluetooth", thread_func, u))) {
        pa_log_error("Failed to create IO thread");
        return -1;
    }

    if (u->sink) {
        pa_sink_set_asyncmsgq(u->sink, u->thread_mq.inq);
        pa_sink_set_rtpoll(u->sink, u->rtpoll);
        pa_sink_put(u->sink);

        if (u->sink->set_volume)
            u->sink->set_volume(u->sink);
    }

    if (u->source) {
        pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
        pa_source_set_rtpoll(u->source, u->rtpoll);
        pa_source_put(u->source);

        if (u->source->set_volume)
            u->source->set_volume(u->source);
    }

    return 0;
}

static void save_sco_volume_callbacks(struct userdata *u) {
    pa_assert(u);
    pa_assert(USE_SCO_OVER_PCM(u));

    u->hsp.sco_sink_set_volume = u->hsp.sco_sink->set_volume;
    u->hsp.sco_source_set_volume = u->hsp.sco_source->set_volume;
}

static void restore_sco_volume_callbacks(struct userdata *u) {
    pa_assert(u);
    pa_assert(USE_SCO_OVER_PCM(u));

    pa_sink_set_set_volume_callback(u->hsp.sco_sink, u->hsp.sco_sink_set_volume);
    pa_source_set_set_volume_callback(u->hsp.sco_source, u->hsp.sco_source_set_volume);
}

/* Run from main thread */
static int card_set_profile(pa_card *c, pa_card_profile *new_profile) {
    struct userdata *u;
    pa_bluez4_profile_t *d;

    pa_assert(c);
    pa_assert(new_profile);
    pa_assert_se(u = c->userdata);

    d = PA_CARD_PROFILE_DATA(new_profile);

    if (*d != PA_BLUEZ4_PROFILE_OFF) {
        const pa_bluez4_device *device = u->device;

        if (!device->transports[*d] || device->transports[*d]->state == PA_BLUEZ4_TRANSPORT_STATE_DISCONNECTED) {
            pa_log_warn("Profile not connected, refused to switch profile to %s", new_profile->name);
            return -PA_ERR_IO;
        }
    }

    stop_thread(u);

    if (USE_SCO_OVER_PCM(u))
        restore_sco_volume_callbacks(u);

    u->profile = *d;
    u->sample_spec = u->requested_sample_spec;

    if (USE_SCO_OVER_PCM(u))
        save_sco_volume_callbacks(u);

    if (u->profile != PA_BLUEZ4_PROFILE_OFF)
        if (init_profile(u) < 0)
            goto off;

    if (u->sink || u->source)
        if (start_thread(u) < 0)
            goto off;

    return 0;

off:
    stop_thread(u);

    pa_assert_se(pa_card_set_profile(u->card, pa_hashmap_get(u->card->profiles, "off"), false) >= 0);

    return -PA_ERR_IO;
}

/* Run from main thread */
static void create_card_ports(struct userdata *u, pa_hashmap *ports) {
    pa_device_port *port;
    pa_device_port_new_data port_data;

    const char *name_prefix = NULL;
    const char *input_description = NULL;
    const char *output_description = NULL;

    pa_assert(u);
    pa_assert(ports);
    pa_assert(u->device);

    switch (pa_bluez4_get_form_factor(u->device->class)) {
        case PA_BLUEZ4_FORM_FACTOR_UNKNOWN:
            break;

        case PA_BLUEZ4_FORM_FACTOR_HEADSET:
            name_prefix = "headset";
            input_description = output_description = _("Headset");
            break;

        case PA_BLUEZ4_FORM_FACTOR_HANDSFREE:
            name_prefix = "handsfree";
            input_description = output_description = _("Handsfree");
            break;

        case PA_BLUEZ4_FORM_FACTOR_MICROPHONE:
            name_prefix = "microphone";
            input_description = _("Microphone");
            break;

        case PA_BLUEZ4_FORM_FACTOR_SPEAKER:
            name_prefix = "speaker";
            output_description = _("Speaker");
            break;

        case PA_BLUEZ4_FORM_FACTOR_HEADPHONE:
            name_prefix = "headphone";
            output_description = _("Headphone");
            break;

        case PA_BLUEZ4_FORM_FACTOR_PORTABLE:
            name_prefix = "portable";
            input_description = output_description = _("Portable");
            break;

        case PA_BLUEZ4_FORM_FACTOR_CAR:
            name_prefix = "car";
            input_description = output_description = _("Car");
            break;

        case PA_BLUEZ4_FORM_FACTOR_HIFI:
            name_prefix = "hifi";
            input_description = output_description = _("HiFi");
            break;

        case PA_BLUEZ4_FORM_FACTOR_PHONE:
            name_prefix = "phone";
            input_description = output_description = _("Phone");
            break;
    }

    if (!name_prefix)
        name_prefix = "unknown";

    if (!output_description)
        output_description = _("Bluetooth Output");

    if (!input_description)
        input_description = _("Bluetooth Input");

    u->output_port_name = pa_sprintf_malloc("%s-output", name_prefix);
    u->input_port_name = pa_sprintf_malloc("%s-input", name_prefix);

    pa_device_port_new_data_init(&port_data);
    pa_device_port_new_data_set_name(&port_data, u->output_port_name);
    pa_device_port_new_data_set_description(&port_data, output_description);
    pa_device_port_new_data_set_direction(&port_data, PA_DIRECTION_OUTPUT);
    pa_device_port_new_data_set_available(&port_data, get_port_availability(u, PA_DIRECTION_OUTPUT));
    pa_assert_se(port = pa_device_port_new(u->core, &port_data, 0));
    pa_assert_se(pa_hashmap_put(ports, port->name, port) >= 0);
    pa_device_port_new_data_done(&port_data);

    pa_device_port_new_data_init(&port_data);
    pa_device_port_new_data_set_name(&port_data, u->input_port_name);
    pa_device_port_new_data_set_description(&port_data, input_description);
    pa_device_port_new_data_set_direction(&port_data, PA_DIRECTION_INPUT);
    pa_device_port_new_data_set_available(&port_data, get_port_availability(u, PA_DIRECTION_INPUT));
    pa_assert_se(port = pa_device_port_new(u->core, &port_data, 0));
    pa_assert_se(pa_hashmap_put(ports, port->name, port) >= 0);
    pa_device_port_new_data_done(&port_data);
}

/* Run from main thread */
static pa_card_profile *create_card_profile(struct userdata *u, const char *uuid, pa_hashmap *ports) {
    pa_device_port *input_port, *output_port;
    pa_card_profile *p = NULL;
    pa_bluez4_profile_t *d;

    pa_assert(u->input_port_name);
    pa_assert(u->output_port_name);
    pa_assert_se(input_port = pa_hashmap_get(ports, u->input_port_name));
    pa_assert_se(output_port = pa_hashmap_get(ports, u->output_port_name));

    if (pa_streq(uuid, A2DP_SINK_UUID)) {
        p = pa_card_profile_new("a2dp", _("High Fidelity Playback (A2DP)"), sizeof(pa_bluez4_profile_t));
        p->priority = 10;
        p->n_sinks = 1;
        p->n_sources = 0;
        p->max_sink_channels = 2;
        p->max_source_channels = 0;
        pa_hashmap_put(output_port->profiles, p->name, p);

        d = PA_CARD_PROFILE_DATA(p);
        *d = PA_BLUEZ4_PROFILE_A2DP;
    } else if (pa_streq(uuid, A2DP_SOURCE_UUID)) {
        p = pa_card_profile_new("a2dp_source", _("High Fidelity Capture (A2DP)"), sizeof(pa_bluez4_profile_t));
        p->priority = 10;
        p->n_sinks = 0;
        p->n_sources = 1;
        p->max_sink_channels = 0;
        p->max_source_channels = 2;
        pa_hashmap_put(input_port->profiles, p->name, p);

        d = PA_CARD_PROFILE_DATA(p);
        *d = PA_BLUEZ4_PROFILE_A2DP_SOURCE;
    } else if (pa_streq(uuid, HSP_HS_UUID) || pa_streq(uuid, HFP_HS_UUID)) {
        p = pa_card_profile_new("hsp", _("Telephony Duplex (HSP/HFP)"), sizeof(pa_bluez4_profile_t));
        p->priority = 20;
        p->n_sinks = 1;
        p->n_sources = 1;
        p->max_sink_channels = 1;
        p->max_source_channels = 1;
        pa_hashmap_put(input_port->profiles, p->name, p);
        pa_hashmap_put(output_port->profiles, p->name, p);

        d = PA_CARD_PROFILE_DATA(p);
        *d = PA_BLUEZ4_PROFILE_HSP;
    } else if (pa_streq(uuid, HFP_AG_UUID)) {
        p = pa_card_profile_new("hfgw", _("Handsfree Gateway"), sizeof(pa_bluez4_profile_t));
        p->priority = 20;
        p->n_sinks = 1;
        p->n_sources = 1;
        p->max_sink_channels = 1;
        p->max_source_channels = 1;
        pa_hashmap_put(input_port->profiles, p->name, p);
        pa_hashmap_put(output_port->profiles, p->name, p);

        d = PA_CARD_PROFILE_DATA(p);
        *d = PA_BLUEZ4_PROFILE_HFGW;
    }

    if (p) {
        pa_bluez4_transport *t;

        if ((t = u->device->transports[*d]))
            p->available = transport_state_to_availability(t->state);
    }

    return p;
}

/* Run from main thread */
static int add_card(struct userdata *u) {
    pa_card_new_data data;
    bool b;
    pa_card_profile *p;
    pa_bluez4_profile_t *d;
    pa_bluez4_form_factor_t ff;
    char *n;
    const char *default_profile;
    const pa_bluez4_device *device;
    const pa_bluez4_uuid *uuid;

    pa_assert(u);
    pa_assert(u->device);

    device = u->device;

    pa_card_new_data_init(&data);
    data.driver = __FILE__;
    data.module = u->module;

    n = pa_bluez4_cleanup_name(device->alias);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_DESCRIPTION, n);
    pa_xfree(n);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_STRING, device->address);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_API, "bluez");
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_CLASS, "sound");
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_BUS, "bluetooth");

    if ((ff = pa_bluez4_get_form_factor(device->class)) != PA_BLUEZ4_FORM_FACTOR_UNKNOWN)
        pa_proplist_sets(data.proplist, PA_PROP_DEVICE_FORM_FACTOR, pa_bluez4_form_factor_to_string(ff));

    pa_proplist_sets(data.proplist, "bluez.path", device->path);
    pa_proplist_setf(data.proplist, "bluez.class", "0x%06x", (unsigned) device->class);
    pa_proplist_sets(data.proplist, "bluez.alias", device->alias);
    data.name = get_name("card", u->modargs, device->address, NULL, &b);
    data.namereg_fail = b;

    if (pa_modargs_get_proplist(u->modargs, "card_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        pa_log("Invalid properties");
        pa_card_new_data_done(&data);
        return -1;
    }

    create_card_ports(u, data.ports);

    PA_LLIST_FOREACH(uuid, device->uuids) {
        p = create_card_profile(u, uuid->uuid, data.ports);

        if (!p)
            continue;

        if (pa_hashmap_get(data.profiles, p->name)) {
            pa_card_profile_free(p);
            continue;
        }

        pa_hashmap_put(data.profiles, p->name, p);
    }

    pa_assert(!pa_hashmap_isempty(data.profiles));

    p = pa_card_profile_new("off", _("Off"), sizeof(pa_bluez4_profile_t));
    p->available = PA_AVAILABLE_YES;
    d = PA_CARD_PROFILE_DATA(p);
    *d = PA_BLUEZ4_PROFILE_OFF;
    pa_hashmap_put(data.profiles, p->name, p);

    if ((default_profile = pa_modargs_get_value(u->modargs, "profile", NULL))) {
        if (pa_hashmap_get(data.profiles, default_profile))
            pa_card_new_data_set_profile(&data, default_profile);
        else
            pa_log_warn("Profile '%s' not valid or not supported by device.", default_profile);
    }

    u->card = pa_card_new(u->core, &data);
    pa_card_new_data_done(&data);

    if (!u->card) {
        pa_log("Failed to allocate card.");
        return -1;
    }

    u->card->userdata = u;
    u->card->set_profile = card_set_profile;

    d = PA_CARD_PROFILE_DATA(u->card->active_profile);

    if (*d != PA_BLUEZ4_PROFILE_OFF && (!device->transports[*d] ||
                              device->transports[*d]->state == PA_BLUEZ4_TRANSPORT_STATE_DISCONNECTED)) {
        pa_log_warn("Default profile not connected, selecting off profile");
        u->card->active_profile = pa_hashmap_get(u->card->profiles, "off");
        u->card->save_profile = false;
    }

    d = PA_CARD_PROFILE_DATA(u->card->active_profile);
    u->profile = *d;

    if (USE_SCO_OVER_PCM(u))
        save_sco_volume_callbacks(u);

    return 0;
}

/* Run from main thread */
static pa_bluez4_device* find_device(struct userdata *u, const char *address, const char *path) {
    pa_bluez4_device *d = NULL;

    pa_assert(u);

    if (!address && !path) {
        pa_log_error("Failed to get device address/path from module arguments.");
        return NULL;
    }

    if (path) {
        if (!(d = pa_bluez4_discovery_get_by_path(u->discovery, path))) {
            pa_log_error("%s is not a valid BlueZ audio device.", path);
            return NULL;
        }

        if (address && !(pa_streq(d->address, address))) {
            pa_log_error("Passed path %s address %s != %s don't match.", path, d->address, address);
            return NULL;
        }

    } else {
        if (!(d = pa_bluez4_discovery_get_by_address(u->discovery, address))) {
            pa_log_error("%s is not known.", address);
            return NULL;
        }
    }

    if (d) {
        u->address = pa_xstrdup(d->address);
        u->path = pa_xstrdup(d->path);
    }

    return d;
}

/* Run from main thread */
static pa_hook_result_t uuid_added_cb(pa_bluez4_discovery *y, const struct pa_bluez4_hook_uuid_data *data,
                                      struct userdata *u) {
    pa_card_profile *p;

    pa_assert(data);
    pa_assert(data->device);
    pa_assert(data->uuid);
    pa_assert(u);

    if (data->device != u->device)
        return PA_HOOK_OK;

    p = create_card_profile(u, data->uuid, u->card->ports);

    if (!p)
        return PA_HOOK_OK;

    if (pa_hashmap_get(u->card->profiles, p->name)) {
        pa_card_profile_free(p);
        return PA_HOOK_OK;
    }

    pa_card_add_profile(u->card, p);

    return PA_HOOK_OK;
}

/* Run from main thread */
static pa_hook_result_t discovery_hook_cb(pa_bluez4_discovery *y, const pa_bluez4_device *d, struct userdata *u) {
    pa_assert(u);
    pa_assert(d);

    if (d != u->device)
        return PA_HOOK_OK;

    if (d->dead)
        pa_log_debug("Device %s removed: unloading module", d->path);
    else if (!pa_bluez4_device_any_audio_connected(d))
        pa_log_debug("Unloading module, because device %s doesn't have any audio profiles connected anymore.", d->path);
    else
        return PA_HOOK_OK;

    pa_module_unload(u->core, u->module, true);

    return PA_HOOK_OK;
}

int pa__init(pa_module *m) {
    pa_modargs *ma;
    uint32_t channels;
    struct userdata *u;
    const char *address, *path;
    pa_bluez4_device *device;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log_error("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->module = m;
    u->core = m->core;
    u->stream_fd = -1;
    u->sample_spec = m->core->default_sample_spec;
    u->modargs = ma;

    if (pa_modargs_get_value(ma, "sco_sink", NULL) &&
        !(u->hsp.sco_sink = pa_namereg_get(m->core, pa_modargs_get_value(ma, "sco_sink", NULL), PA_NAMEREG_SINK))) {
        pa_log("SCO sink not found");
        goto fail;
    }

    if (pa_modargs_get_value(ma, "sco_source", NULL) &&
        !(u->hsp.sco_source = pa_namereg_get(m->core, pa_modargs_get_value(ma, "sco_source", NULL), PA_NAMEREG_SOURCE))) {
        pa_log("SCO source not found");
        goto fail;
    }

    if (pa_modargs_get_sample_rate(ma, &u->sample_spec.rate) < 0) {
        pa_log_error("Failed to get rate from module arguments");
        goto fail;
    }

    u->auto_connect = true;
    if (pa_modargs_get_value_boolean(ma, "auto_connect", &u->auto_connect)) {
        pa_log("Failed to parse auto_connect= argument");
        goto fail;
    }

    channels = u->sample_spec.channels;
    if (pa_modargs_get_value_u32(ma, "channels", &channels) < 0 ||
        !pa_channels_valid(channels)) {
        pa_log_error("Failed to get channels from module arguments");
        goto fail;
    }
    u->sample_spec.channels = (uint8_t) channels;
    u->requested_sample_spec = u->sample_spec;

    address = pa_modargs_get_value(ma, "address", NULL);
    path = pa_modargs_get_value(ma, "path", NULL);

    if (!(u->discovery = pa_bluez4_discovery_get(m->core)))
        goto fail;

    if (!(device = find_device(u, address, path)))
        goto fail;

    u->device = device;

    u->discovery_slot =
        pa_hook_connect(pa_bluez4_discovery_hook(u->discovery, PA_BLUEZ4_HOOK_DEVICE_CONNECTION_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) discovery_hook_cb, u);

    u->uuid_added_slot =
        pa_hook_connect(pa_bluez4_discovery_hook(u->discovery, PA_BLUEZ4_HOOK_DEVICE_UUID_ADDED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) uuid_added_cb, u);

    u->sink_state_changed_slot =
        pa_hook_connect(&u->core->hooks[PA_CORE_HOOK_SINK_STATE_CHANGED],
                        PA_HOOK_NORMAL, (pa_hook_cb_t) sink_state_changed_cb, u);

    u->source_state_changed_slot =
        pa_hook_connect(&u->core->hooks[PA_CORE_HOOK_SOURCE_STATE_CHANGED],
                        PA_HOOK_NORMAL, (pa_hook_cb_t) source_state_changed_cb, u);

    u->transport_state_changed_slot =
        pa_hook_connect(pa_bluez4_discovery_hook(u->discovery, PA_BLUEZ4_HOOK_TRANSPORT_STATE_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) transport_state_changed_cb, u);

    u->transport_nrec_changed_slot =
        pa_hook_connect(pa_bluez4_discovery_hook(u->discovery, PA_BLUEZ4_HOOK_TRANSPORT_NREC_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) transport_nrec_changed_cb, u);

    u->transport_microphone_changed_slot =
        pa_hook_connect(pa_bluez4_discovery_hook(u->discovery, PA_BLUEZ4_HOOK_TRANSPORT_MICROPHONE_GAIN_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) transport_microphone_gain_changed_cb, u);

    u->transport_speaker_changed_slot =
        pa_hook_connect(pa_bluez4_discovery_hook(u->discovery, PA_BLUEZ4_HOOK_TRANSPORT_SPEAKER_GAIN_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) transport_speaker_gain_changed_cb, u);

    /* Add the card structure. This will also initialize the default profile */
    if (add_card(u) < 0)
        goto fail;

    if (!(u->msg = pa_msgobject_new(bluetooth_msg)))
        goto fail;

    u->msg->parent.process_msg = device_process_msg;
    u->msg->card = u->card;

    if (u->profile != PA_BLUEZ4_PROFILE_OFF)
        if (init_profile(u) < 0)
            goto off;

    if (u->sink || u->source)
        if (start_thread(u) < 0)
            goto off;

    return 0;

off:
    stop_thread(u);

    pa_assert_se(pa_card_set_profile(u->card, pa_hashmap_get(u->card->profiles, "off"), false) >= 0);

    return 0;

fail:

    pa__done(m);

    return -1;
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return
        (u->sink ? pa_sink_linked_by(u->sink) : 0) +
        (u->source ? pa_source_linked_by(u->source) : 0);
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    stop_thread(u);

    if (u->discovery_slot)
        pa_hook_slot_free(u->discovery_slot);

    if (u->uuid_added_slot)
        pa_hook_slot_free(u->uuid_added_slot);

    if (u->sink_state_changed_slot)
        pa_hook_slot_free(u->sink_state_changed_slot);

    if (u->source_state_changed_slot)
        pa_hook_slot_free(u->source_state_changed_slot);

    if (u->transport_state_changed_slot)
        pa_hook_slot_free(u->transport_state_changed_slot);

    if (u->transport_nrec_changed_slot)
        pa_hook_slot_free(u->transport_nrec_changed_slot);

    if (u->transport_microphone_changed_slot)
        pa_hook_slot_free(u->transport_microphone_changed_slot);

    if (u->transport_speaker_changed_slot)
        pa_hook_slot_free(u->transport_speaker_changed_slot);

    if (USE_SCO_OVER_PCM(u))
        restore_sco_volume_callbacks(u);

    if (u->msg)
        pa_xfree(u->msg);

    if (u->card)
        pa_card_free(u->card);

    if (u->a2dp.buffer)
        pa_xfree(u->a2dp.buffer);

    sbc_finish(&u->a2dp.sbc);

    if (u->modargs)
        pa_modargs_free(u->modargs);

    pa_xfree(u->output_port_name);
    pa_xfree(u->input_port_name);

    pa_xfree(u->address);
    pa_xfree(u->path);

    if (u->discovery)
        pa_bluez4_discovery_unref(u->discovery);

    pa_xfree(u);
}

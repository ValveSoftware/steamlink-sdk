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

#include <errno.h>

#include <arpa/inet.h>
#include <sbc/sbc.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>

#include <pulsecore/core-error.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/poll.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/shared.h>
#include <pulsecore/socket-util.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/time-smoother.h>

#include "a2dp-codecs.h"
#include "bluez5-util.h"
#include "rtp.h"

#include "module-bluez5-device-symdef.h"

PA_MODULE_AUTHOR("João Paulo Rechi Vita");
PA_MODULE_DESCRIPTION("BlueZ 5 Bluetooth audio sink and source");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE("path=<device object path>");

#define MAX_PLAYBACK_CATCH_UP_USEC (100 * PA_USEC_PER_MSEC)
#define FIXED_LATENCY_PLAYBACK_A2DP (25 * PA_USEC_PER_MSEC)
#define FIXED_LATENCY_PLAYBACK_SCO (125 * PA_USEC_PER_MSEC)
#define FIXED_LATENCY_RECORD_A2DP   (25 * PA_USEC_PER_MSEC)
#define FIXED_LATENCY_RECORD_SCO    (25 * PA_USEC_PER_MSEC)

#define BITPOOL_DEC_LIMIT 32
#define BITPOOL_DEC_STEP 5
#define HSP_MAX_GAIN 15

static const char* const valid_modargs[] = {
    "path",
    NULL
};

enum {
    BLUETOOTH_MESSAGE_IO_THREAD_FAILED,
    BLUETOOTH_MESSAGE_STREAM_FD_HUP,
    BLUETOOTH_MESSAGE_MAX
};

typedef struct bluetooth_msg {
    pa_msgobject parent;
    pa_card *card;
} bluetooth_msg;
PA_DEFINE_PRIVATE_CLASS(bluetooth_msg, pa_msgobject);
#define BLUETOOTH_MSG(o) (bluetooth_msg_cast(o))

typedef struct sbc_info {
    sbc_t sbc;                           /* Codec data */
    bool sbc_initialized;                /* Keep track if the encoder is initialized */
    size_t codesize, frame_length;       /* SBC Codesize, frame_length. We simply cache those values here */
    uint16_t seq_num;                    /* Cumulative packet sequence */
    uint8_t min_bitpool;
    uint8_t max_bitpool;

    void* buffer;                        /* Codec transfer buffer */
    size_t buffer_size;                  /* Size of the buffer */
} sbc_info_t;

struct userdata {
    pa_module *module;
    pa_core *core;

    pa_hook_slot *device_connection_changed_slot;
    pa_hook_slot *transport_state_changed_slot;
    pa_hook_slot *transport_speaker_gain_changed_slot;
    pa_hook_slot *transport_microphone_gain_changed_slot;

    pa_bluetooth_discovery *discovery;
    pa_bluetooth_device *device;
    pa_bluetooth_transport *transport;
    bool transport_acquired;

    pa_card *card;
    pa_sink *sink;
    pa_source *source;
    pa_bluetooth_profile_t profile;
    char *output_port_name;
    char *input_port_name;

    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;
    pa_rtpoll_item *rtpoll_item;
    bluetooth_msg *msg;

    int stream_fd;
    int stream_write_type;
    size_t read_link_mtu;
    size_t write_link_mtu;
    size_t read_block_size;
    size_t write_block_size;
    uint64_t read_index;
    uint64_t write_index;
    pa_usec_t started_at;
    pa_smoother *read_smoother;
    pa_memchunk write_memchunk;
    pa_sample_spec sample_spec;
    struct sbc_info sbc_info;
};

typedef enum pa_bluetooth_form_factor {
    PA_BLUETOOTH_FORM_FACTOR_UNKNOWN,
    PA_BLUETOOTH_FORM_FACTOR_HEADSET,
    PA_BLUETOOTH_FORM_FACTOR_HANDSFREE,
    PA_BLUETOOTH_FORM_FACTOR_MICROPHONE,
    PA_BLUETOOTH_FORM_FACTOR_SPEAKER,
    PA_BLUETOOTH_FORM_FACTOR_HEADPHONE,
    PA_BLUETOOTH_FORM_FACTOR_PORTABLE,
    PA_BLUETOOTH_FORM_FACTOR_CAR,
    PA_BLUETOOTH_FORM_FACTOR_HIFI,
    PA_BLUETOOTH_FORM_FACTOR_PHONE,
} pa_bluetooth_form_factor_t;

/* Run from main thread */
static pa_bluetooth_form_factor_t form_factor_from_class(uint32_t class_of_device) {
    unsigned major, minor;
    pa_bluetooth_form_factor_t r;

    static const pa_bluetooth_form_factor_t table[] = {
        [1] = PA_BLUETOOTH_FORM_FACTOR_HEADSET,
        [2] = PA_BLUETOOTH_FORM_FACTOR_HANDSFREE,
        [4] = PA_BLUETOOTH_FORM_FACTOR_MICROPHONE,
        [5] = PA_BLUETOOTH_FORM_FACTOR_SPEAKER,
        [6] = PA_BLUETOOTH_FORM_FACTOR_HEADPHONE,
        [7] = PA_BLUETOOTH_FORM_FACTOR_PORTABLE,
        [8] = PA_BLUETOOTH_FORM_FACTOR_CAR,
        [10] = PA_BLUETOOTH_FORM_FACTOR_HIFI
    };

    /*
     * See Bluetooth Assigned Numbers:
     * https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
     */
    major = (class_of_device >> 8) & 0x1F;
    minor = (class_of_device >> 2) & 0x3F;

    switch (major) {
        case 2:
            return PA_BLUETOOTH_FORM_FACTOR_PHONE;
        case 4:
            break;
        default:
            pa_log_debug("Unknown Bluetooth major device class %u", major);
            return PA_BLUETOOTH_FORM_FACTOR_UNKNOWN;
    }

    r = minor < PA_ELEMENTSOF(table) ? table[minor] : PA_BLUETOOTH_FORM_FACTOR_UNKNOWN;

    if (!r)
        pa_log_debug("Unknown Bluetooth minor device class %u", minor);

    return r;
}

/* Run from main thread */
static const char *form_factor_to_string(pa_bluetooth_form_factor_t ff) {
    switch (ff) {
        case PA_BLUETOOTH_FORM_FACTOR_UNKNOWN:
            return "unknown";
        case PA_BLUETOOTH_FORM_FACTOR_HEADSET:
            return "headset";
        case PA_BLUETOOTH_FORM_FACTOR_HANDSFREE:
            return "hands-free";
        case PA_BLUETOOTH_FORM_FACTOR_MICROPHONE:
            return "microphone";
        case PA_BLUETOOTH_FORM_FACTOR_SPEAKER:
            return "speaker";
        case PA_BLUETOOTH_FORM_FACTOR_HEADPHONE:
            return "headphone";
        case PA_BLUETOOTH_FORM_FACTOR_PORTABLE:
            return "portable";
        case PA_BLUETOOTH_FORM_FACTOR_CAR:
            return "car";
        case PA_BLUETOOTH_FORM_FACTOR_HIFI:
            return "hifi";
        case PA_BLUETOOTH_FORM_FACTOR_PHONE:
            return "phone";
    }

    pa_assert_not_reached();
}

/* Run from main thread */
static void connect_ports(struct userdata *u, void *new_data, pa_direction_t direction) {
    pa_device_port *port;

    if (direction == PA_DIRECTION_OUTPUT) {
        pa_sink_new_data *sink_new_data = new_data;

        pa_assert_se(port = pa_hashmap_get(u->card->ports, u->output_port_name));
        pa_assert_se(pa_hashmap_put(sink_new_data->ports, port->name, port) >= 0);
        pa_device_port_ref(port);
    } else {
        pa_source_new_data *source_new_data = new_data;

        pa_assert_se(port = pa_hashmap_get(u->card->ports, u->input_port_name));
        pa_assert_se(pa_hashmap_put(source_new_data->ports, port->name, port) >= 0);
        pa_device_port_ref(port);
    }
}

/* Run from IO thread */
static int sco_process_render(struct userdata *u) {
    ssize_t l;
    pa_memchunk memchunk;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT ||
                u->profile == PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY);
    pa_assert(u->sink);

    pa_sink_render_full(u->sink, u->write_block_size, &memchunk);

    pa_assert(memchunk.length == u->write_block_size);

    for (;;) {
        const void *p;

        /* Now write that data to the socket. The socket is of type
         * SEQPACKET, and we generated the data of the MTU size, so this
         * should just work. */

        p = (const uint8_t *) pa_memblock_acquire_chunk(&memchunk);
        l = pa_write(u->stream_fd, p, memchunk.length, &u->stream_write_type);
        pa_memblock_release(memchunk.memblock);

        pa_assert(l != 0);

        if (l > 0)
            break;

        if (errno == EINTR)
            /* Retry right away if we got interrupted */
            continue;
        else if (errno == EAGAIN)
            /* Hmm, apparently the socket was not writable, give up for now */
            return 0;

        pa_log_error("Failed to write data to SCO socket: %s", pa_cstrerror(errno));
        return -1;
    }

    pa_assert((size_t) l <= memchunk.length);

    if ((size_t) l != memchunk.length) {
        pa_log_error("Wrote memory block to socket only partially! %llu written, wanted to write %llu.",
                    (unsigned long long) l,
                    (unsigned long long) memchunk.length);
        return -1;
    }

    u->write_index += (uint64_t) memchunk.length;
    pa_memblock_unref(memchunk.memblock);

    return 1;
}

/* Run from IO thread */
static int sco_process_push(struct userdata *u) {
    ssize_t l;
    pa_memchunk memchunk;
    struct cmsghdr *cm;
    struct msghdr m;
    bool found_tstamp = false;
    pa_usec_t tstamp = 0;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT ||
                u->profile == PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY);
    pa_assert(u->source);
    pa_assert(u->read_smoother);

    memchunk.memblock = pa_memblock_new(u->core->mempool, u->read_block_size);
    memchunk.index = memchunk.length = 0;

    for (;;) {
        void *p;
        uint8_t aux[1024];
        struct iovec iov;

        pa_zero(m);
        pa_zero(aux);
        pa_zero(iov);

        m.msg_iov = &iov;
        m.msg_iovlen = 1;
        m.msg_control = aux;
        m.msg_controllen = sizeof(aux);

        p = pa_memblock_acquire(memchunk.memblock);
        iov.iov_base = p;
        iov.iov_len = pa_memblock_get_length(memchunk.memblock);
        l = recvmsg(u->stream_fd, &m, 0);
        pa_memblock_release(memchunk.memblock);

        if (l > 0)
            break;

        if (l < 0 && errno == EINTR)
            /* Retry right away if we got interrupted */
            continue;

        pa_memblock_unref(memchunk.memblock);

        if (l < 0 && errno == EAGAIN)
            /* Hmm, apparently the socket was not readable, give up for now. */
            return 0;

        pa_log_error("Failed to read data from SCO socket: %s", l < 0 ? pa_cstrerror(errno) : "EOF");
        return -1;
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
        pa_memblock_unref(memchunk.memblock);
        return -1;
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
    pa_memblock_unref(memchunk.memblock);

    return l;
}

/* Run from IO thread */
static void a2dp_prepare_buffer(struct userdata *u) {
    size_t min_buffer_size = PA_MAX(u->read_link_mtu, u->write_link_mtu);

    pa_assert(u);

    if (u->sbc_info.buffer_size >= min_buffer_size)
        return;

    u->sbc_info.buffer_size = 2 * min_buffer_size;
    pa_xfree(u->sbc_info.buffer);
    u->sbc_info.buffer = pa_xmalloc(u->sbc_info.buffer_size);
}

/* Run from IO thread */
static int a2dp_process_render(struct userdata *u) {
    struct sbc_info *sbc_info;
    struct rtp_header *header;
    struct rtp_payload *payload;
    size_t nbytes;
    void *d;
    const void *p;
    size_t to_write, to_encode;
    unsigned frame_count;
    int ret = 0;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUETOOTH_PROFILE_A2DP_SINK);
    pa_assert(u->sink);

    /* First, render some data */
    if (!u->write_memchunk.memblock)
        pa_sink_render_full(u->sink, u->write_block_size, &u->write_memchunk);

    pa_assert(u->write_memchunk.length == u->write_block_size);

    a2dp_prepare_buffer(u);

    sbc_info = &u->sbc_info;
    header = sbc_info->buffer;
    payload = (struct rtp_payload*) ((uint8_t*) sbc_info->buffer + sizeof(*header));

    frame_count = 0;

    /* Try to create a packet of the full MTU */

    p = (const uint8_t *) pa_memblock_acquire_chunk(&u->write_memchunk);
    to_encode = u->write_memchunk.length;

    d = (uint8_t*) sbc_info->buffer + sizeof(*header) + sizeof(*payload);
    to_write = sbc_info->buffer_size - sizeof(*header) - sizeof(*payload);

    while (PA_LIKELY(to_encode > 0 && to_write > 0)) {
        ssize_t written;
        ssize_t encoded;

        encoded = sbc_encode(&sbc_info->sbc,
                             p, to_encode,
                             d, to_write,
                             &written);

        if (PA_UNLIKELY(encoded <= 0)) {
            pa_log_error("SBC encoding error (%li)", (long) encoded);
            pa_memblock_release(u->write_memchunk.memblock);
            return -1;
        }

        pa_assert_fp((size_t) encoded <= to_encode);
        pa_assert_fp((size_t) encoded == sbc_info->codesize);

        pa_assert_fp((size_t) written <= to_write);
        pa_assert_fp((size_t) written == sbc_info->frame_length);

        p = (const uint8_t*) p + encoded;
        to_encode -= encoded;

        d = (uint8_t*) d + written;
        to_write -= written;

        frame_count++;
    }

    pa_memblock_release(u->write_memchunk.memblock);

    pa_assert(to_encode == 0);

    PA_ONCE_BEGIN {
        pa_log_debug("Using SBC encoder implementation: %s", pa_strnull(sbc_get_implementation_info(&sbc_info->sbc)));
    } PA_ONCE_END;

    /* write it to the fifo */
    memset(sbc_info->buffer, 0, sizeof(*header) + sizeof(*payload));
    header->v = 2;
    header->pt = 1;
    header->sequence_number = htons(sbc_info->seq_num++);
    header->timestamp = htonl(u->write_index / pa_frame_size(&u->sample_spec));
    header->ssrc = htonl(1);
    payload->frame_count = frame_count;

    nbytes = (uint8_t*) d - (uint8_t*) sbc_info->buffer;

    for (;;) {
        ssize_t l;

        l = pa_write(u->stream_fd, sbc_info->buffer, nbytes, &u->stream_write_type);

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

/* Run from IO thread */
static int a2dp_process_push(struct userdata *u) {
    int ret = 0;
    pa_memchunk memchunk;

    pa_assert(u);
    pa_assert(u->profile == PA_BLUETOOTH_PROFILE_A2DP_SOURCE);
    pa_assert(u->source);
    pa_assert(u->read_smoother);

    memchunk.memblock = pa_memblock_new(u->core->mempool, u->read_block_size);
    memchunk.index = memchunk.length = 0;

    for (;;) {
        bool found_tstamp = false;
        pa_usec_t tstamp;
        struct sbc_info *sbc_info;
        struct rtp_header *header;
        struct rtp_payload *payload;
        const void *p;
        void *d;
        ssize_t l;
        size_t to_write, to_decode;
        size_t total_written = 0;

        a2dp_prepare_buffer(u);

        sbc_info = &u->sbc_info;
        header = sbc_info->buffer;
        payload = (struct rtp_payload*) ((uint8_t*) sbc_info->buffer + sizeof(*header));

        l = pa_read(u->stream_fd, sbc_info->buffer, sbc_info->buffer_size, &u->stream_write_type);

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

        pa_assert((size_t) l <= sbc_info->buffer_size);

        /* TODO: get timestamp from rtp */
        if (!found_tstamp) {
            /* pa_log_warn("Couldn't find SO_TIMESTAMP data in auxiliary recvmsg() data!"); */
            tstamp = pa_rtclock_now();
        }

        p = (uint8_t*) sbc_info->buffer + sizeof(*header) + sizeof(*payload);
        to_decode = l - sizeof(*header) - sizeof(*payload);

        d = pa_memblock_acquire(memchunk.memblock);
        to_write = memchunk.length = pa_memblock_get_length(memchunk.memblock);

        while (PA_LIKELY(to_decode > 0)) {
            size_t written;
            ssize_t decoded;

            decoded = sbc_decode(&sbc_info->sbc,
                                 p, to_decode,
                                 d, to_write,
                                 &written);

            if (PA_UNLIKELY(decoded <= 0)) {
                pa_log_error("SBC decoding error (%li)", (long) decoded);
                pa_memblock_release(memchunk.memblock);
                pa_memblock_unref(memchunk.memblock);
                return 0;
            }

            total_written += written;

            /* Reset frame length, it can be changed due to bitpool change */
            sbc_info->frame_length = sbc_get_frame_length(&sbc_info->sbc);

            pa_assert_fp((size_t) decoded <= to_decode);
            pa_assert_fp((size_t) decoded == sbc_info->frame_length);

            pa_assert_fp((size_t) written == sbc_info->codesize);

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

/* Run from I/O thread */
static void a2dp_set_bitpool(struct userdata *u, uint8_t bitpool) {
    struct sbc_info *sbc_info;

    pa_assert(u);

    sbc_info = &u->sbc_info;

    if (sbc_info->sbc.bitpool == bitpool)
        return;

    if (bitpool > sbc_info->max_bitpool)
        bitpool = sbc_info->max_bitpool;
    else if (bitpool < sbc_info->min_bitpool)
        bitpool = sbc_info->min_bitpool;

    sbc_info->sbc.bitpool = bitpool;

    sbc_info->codesize = sbc_get_codesize(&sbc_info->sbc);
    sbc_info->frame_length = sbc_get_frame_length(&sbc_info->sbc);

    pa_log_debug("Bitpool has changed to %u", sbc_info->sbc.bitpool);

    u->read_block_size =
        (u->read_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
        / sbc_info->frame_length * sbc_info->codesize;

    u->write_block_size =
        (u->write_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
        / sbc_info->frame_length * sbc_info->codesize;

    pa_sink_set_max_request_within_thread(u->sink, u->write_block_size);
    pa_sink_set_fixed_latency_within_thread(u->sink,
            FIXED_LATENCY_PLAYBACK_A2DP + pa_bytes_to_usec(u->write_block_size, &u->sample_spec));
}

/* Run from I/O thread */
static void a2dp_reduce_bitpool(struct userdata *u) {
    struct sbc_info *sbc_info;
    uint8_t bitpool;

    pa_assert(u);

    sbc_info = &u->sbc_info;

    /* Check if bitpool is already at its limit */
    if (sbc_info->sbc.bitpool <= BITPOOL_DEC_LIMIT)
        return;

    bitpool = sbc_info->sbc.bitpool - BITPOOL_DEC_STEP;

    if (bitpool < BITPOOL_DEC_LIMIT)
        bitpool = BITPOOL_DEC_LIMIT;

    a2dp_set_bitpool(u, bitpool);
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

static int transport_acquire(struct userdata *u, bool optional) {
    pa_assert(u->transport);

    if (u->transport_acquired)
        return 0;

    pa_log_debug("Acquiring transport %s", u->transport->path);

    u->stream_fd = u->transport->acquire(u->transport, optional, &u->read_link_mtu, &u->write_link_mtu);
    if (u->stream_fd < 0)
        return -1;

    u->transport_acquired = true;
    pa_log_info("Transport %s acquired: fd %d", u->transport->path, u->stream_fd);

    return 0;
}

static void transport_release(struct userdata *u) {
    pa_assert(u->transport);

    /* Ignore if already released */
    if (!u->transport_acquired)
        return;

    pa_log_debug("Releasing transport %s", u->transport->path);

    u->transport->release(u->transport);

    u->transport_acquired = false;

    teardown_stream(u);
}

/* Run from I/O thread */
static void transport_config_mtu(struct userdata *u) {
    if (u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT || u->profile == PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY) {
        u->read_block_size = u->read_link_mtu;
        u->write_block_size = u->write_link_mtu;
    } else {
        u->read_block_size =
            (u->read_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
            / u->sbc_info.frame_length * u->sbc_info.codesize;

        u->write_block_size =
            (u->write_link_mtu - sizeof(struct rtp_header) - sizeof(struct rtp_payload))
            / u->sbc_info.frame_length * u->sbc_info.codesize;
    }

    if (u->sink) {
        pa_sink_set_max_request_within_thread(u->sink, u->write_block_size);
        pa_sink_set_fixed_latency_within_thread(u->sink,
                                                (u->profile == PA_BLUETOOTH_PROFILE_A2DP_SINK ?
                                                 FIXED_LATENCY_PLAYBACK_A2DP : FIXED_LATENCY_PLAYBACK_SCO) +
                                                pa_bytes_to_usec(u->write_block_size, &u->sample_spec));
    }

    if (u->source)
        pa_source_set_fixed_latency_within_thread(u->source,
                                                  (u->profile == PA_BLUETOOTH_PROFILE_A2DP_SOURCE ?
                                                   FIXED_LATENCY_RECORD_A2DP : FIXED_LATENCY_RECORD_SCO) +
                                                  pa_bytes_to_usec(u->read_block_size, &u->sample_spec));
}

/* Run from I/O thread */
static void setup_stream(struct userdata *u) {
    struct pollfd *pollfd;
    int one;

    pa_log_info("Transport %s resuming", u->transport->path);

    transport_config_mtu(u);

    pa_make_fd_nonblock(u->stream_fd);
    pa_make_socket_low_delay(u->stream_fd);

    one = 1;
    if (setsockopt(u->stream_fd, SOL_SOCKET, SO_TIMESTAMP, &one, sizeof(one)) < 0)
        pa_log_warn("Failed to enable SO_TIMESTAMP: %s", pa_cstrerror(errno));

    pa_log_debug("Stream properly set up, we're ready to roll!");

    if (u->profile == PA_BLUETOOTH_PROFILE_A2DP_SINK)
        a2dp_set_bitpool(u, u->sbc_info.max_bitpool);

    u->rtpoll_item = pa_rtpoll_item_new(u->rtpoll, PA_RTPOLL_NEVER, 1);
    pollfd = pa_rtpoll_item_get_pollfd(u->rtpoll_item, NULL);
    pollfd->fd = u->stream_fd;
    pollfd->events = pollfd->revents = 0;

    u->read_index = u->write_index = 0;
    u->started_at = 0;

    if (u->source)
        u->read_smoother = pa_smoother_new(PA_USEC_PER_SEC, 2*PA_USEC_PER_SEC, true, true, 10, pa_rtclock_now(), true);
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
                        transport_release(u);

                    if (u->read_smoother)
                        pa_smoother_pause(u->read_smoother, pa_rtclock_now());

                    break;

                case PA_SOURCE_IDLE:
                case PA_SOURCE_RUNNING:
                    if (u->source->thread_info.state != PA_SOURCE_SUSPENDED)
                        break;

                    /* Resume the device if the sink was suspended as well */
                    if (!u->sink || !PA_SINK_IS_OPENED(u->sink->thread_info.state)) {
                        if (transport_acquire(u, false) < 0)
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
                    break;
            }

            break;

        case PA_SOURCE_MESSAGE_GET_LATENCY: {
            pa_usec_t wi, ri;

            if (u->read_smoother) {
                wi = pa_smoother_get(u->read_smoother, pa_rtclock_now());
                ri = pa_bytes_to_usec(u->read_index, &u->sample_spec);

                *((pa_usec_t*) data) = FIXED_LATENCY_RECORD_A2DP + wi > ri ? FIXED_LATENCY_RECORD_A2DP + wi - ri : 0;
            } else
                *((pa_usec_t*) data) = 0;

            return 0;
        }

    }

    r = pa_source_process_msg(o, code, data, offset, chunk);

    return (r < 0 || !failed) ? r : -1;
}

/* Run from main thread */
static void source_set_volume_cb(pa_source *s) {
    uint16_t gain;
    pa_volume_t volume;
    struct userdata *u;

    pa_assert(s);
    pa_assert(s->core);

    u = s->userdata;

    pa_assert(u);
    pa_assert(u->source == s);
    pa_assert(u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT);

    if (u->transport->set_microphone_gain == NULL)
      return;

    gain = (pa_cvolume_max(&s->real_volume) * HSP_MAX_GAIN) / PA_VOLUME_NORM;

    if (gain > HSP_MAX_GAIN)
        gain = HSP_MAX_GAIN;

    volume = (pa_volume_t) (gain * PA_VOLUME_NORM / HSP_MAX_GAIN);

    /* increment volume by one to correct rounding errors */
    if (volume < PA_VOLUME_NORM)
        volume++;

    pa_cvolume_set(&s->real_volume, u->sample_spec.channels, volume);

    u->transport->set_microphone_gain(u->transport, gain);
}

/* Run from main thread */
static int add_source(struct userdata *u) {
    pa_source_new_data data;

    pa_assert(u->transport);

    pa_source_new_data_init(&data);
    data.module = u->module;
    data.card = u->card;
    data.driver = __FILE__;
    data.name = pa_sprintf_malloc("bluez_source.%s", u->device->address);
    data.namereg_fail = false;
    pa_proplist_sets(data.proplist, "bluetooth.protocol", pa_bluetooth_profile_to_string(u->profile));
    pa_source_new_data_set_sample_spec(&data, &u->sample_spec);
    if (u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT)
        pa_proplist_sets(data.proplist, PA_PROP_DEVICE_INTENDED_ROLES, "phone");

    connect_ports(u, &data, PA_DIRECTION_INPUT);

    if (!u->transport_acquired)
        switch (u->profile) {
            case PA_BLUETOOTH_PROFILE_A2DP_SOURCE:
            case PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY:
                data.suspend_cause = PA_SUSPEND_USER;
                break;
            case PA_BLUETOOTH_PROFILE_A2DP_SINK:
            case PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT:
            case PA_BLUETOOTH_PROFILE_OFF:
                pa_assert_not_reached();
                break;
        }

    u->source = pa_source_new(u->core, &data, PA_SOURCE_HARDWARE|PA_SOURCE_LATENCY);
    pa_source_new_data_done(&data);
    if (!u->source) {
        pa_log_error("Failed to create source");
        return -1;
    }

    u->source->userdata = u;
    u->source->parent.process_msg = source_process_msg;

    if (u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT) {
        pa_source_set_set_volume_callback(u->source, source_set_volume_cb);
        u->source->n_volume_steps = 16;
    }
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
                        transport_release(u);

                    break;

                case PA_SINK_IDLE:
                case PA_SINK_RUNNING:
                    if (u->sink->thread_info.state != PA_SINK_SUSPENDED)
                        break;

                    /* Resume the device if the source was suspended as well */
                    if (!u->source || !PA_SOURCE_IS_OPENED(u->source->thread_info.state)) {
                        if (transport_acquire(u, false) < 0)
                            failed = true;
                        else
                            setup_stream(u);
                    }

                    break;

                case PA_SINK_UNLINKED:
                case PA_SINK_INIT:
                case PA_SINK_INVALID_STATE:
                    break;
            }

            break;

        case PA_SINK_MESSAGE_GET_LATENCY: {
            pa_usec_t wi, ri;

            if (u->read_smoother) {
                ri = pa_smoother_get(u->read_smoother, pa_rtclock_now());
                wi = pa_bytes_to_usec(u->write_index + u->write_block_size, &u->sample_spec);
            } else {
                ri = pa_rtclock_now() - u->started_at;
                wi = pa_bytes_to_usec(u->write_index, &u->sample_spec);
            }

            *((pa_usec_t*) data) = FIXED_LATENCY_PLAYBACK_A2DP + wi > ri ? FIXED_LATENCY_PLAYBACK_A2DP + wi - ri : 0;

            return 0;
        }
    }

    r = pa_sink_process_msg(o, code, data, offset, chunk);

    return (r < 0 || !failed) ? r : -1;
}

/* Run from main thread */
static void sink_set_volume_cb(pa_sink *s) {
    uint16_t gain;
    pa_volume_t volume;
    struct userdata *u;

    pa_assert(s);
    pa_assert(s->core);

    u = s->userdata;

    pa_assert(u);
    pa_assert(u->sink == s);
    pa_assert(u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT);

    if (u->transport->set_speaker_gain == NULL)
      return;

    gain = (pa_cvolume_max(&s->real_volume) * HSP_MAX_GAIN) / PA_VOLUME_NORM;

    if (gain > HSP_MAX_GAIN)
        gain = HSP_MAX_GAIN;

    volume = (pa_volume_t) (gain * PA_VOLUME_NORM / HSP_MAX_GAIN);

    /* increment volume by one to correct rounding errors */
    if (volume < PA_VOLUME_NORM)
        volume++;

    pa_cvolume_set(&s->real_volume, u->sample_spec.channels, volume);

    u->transport->set_speaker_gain(u->transport, gain);
}

/* Run from main thread */
static int add_sink(struct userdata *u) {
    pa_sink_new_data data;

    pa_assert(u->transport);

    pa_sink_new_data_init(&data);
    data.module = u->module;
    data.card = u->card;
    data.driver = __FILE__;
    data.name = pa_sprintf_malloc("bluez_sink.%s", u->device->address);
    data.namereg_fail = false;
    pa_proplist_sets(data.proplist, "bluetooth.protocol", pa_bluetooth_profile_to_string(u->profile));
    pa_sink_new_data_set_sample_spec(&data, &u->sample_spec);
    if (u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT)
        pa_proplist_sets(data.proplist, PA_PROP_DEVICE_INTENDED_ROLES, "phone");

    connect_ports(u, &data, PA_DIRECTION_OUTPUT);

    if (!u->transport_acquired)
        switch (u->profile) {
            case PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY:
                data.suspend_cause = PA_SUSPEND_USER;
                break;
            case PA_BLUETOOTH_PROFILE_A2DP_SINK:
                /* Profile switch should have failed */
            case PA_BLUETOOTH_PROFILE_A2DP_SOURCE:
            case PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT:
            case PA_BLUETOOTH_PROFILE_OFF:
                pa_assert_not_reached();
                break;
        }

    u->sink = pa_sink_new(u->core, &data, PA_SINK_HARDWARE|PA_SINK_LATENCY);
    pa_sink_new_data_done(&data);
    if (!u->sink) {
        pa_log_error("Failed to create sink");
        return -1;
    }

    u->sink->userdata = u;
    u->sink->parent.process_msg = sink_process_msg;

    if (u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT) {
        pa_sink_set_set_volume_callback(u->sink, sink_set_volume_cb);
        u->sink->n_volume_steps = 16;
    }
    return 0;
}

/* Run from main thread */
static void transport_config(struct userdata *u) {
    if (u->profile == PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT || u->profile == PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY) {
        u->sample_spec.format = PA_SAMPLE_S16LE;
        u->sample_spec.channels = 1;
        u->sample_spec.rate = 8000;
    } else {
        sbc_info_t *sbc_info = &u->sbc_info;
        a2dp_sbc_t *config;

        pa_assert(u->transport);

        u->sample_spec.format = PA_SAMPLE_S16LE;
        config = (a2dp_sbc_t *) u->transport->config;

        if (sbc_info->sbc_initialized)
            sbc_reinit(&sbc_info->sbc, 0);
        else
            sbc_init(&sbc_info->sbc, 0);
        sbc_info->sbc_initialized = true;

        switch (config->frequency) {
            case SBC_SAMPLING_FREQ_16000:
                sbc_info->sbc.frequency = SBC_FREQ_16000;
                u->sample_spec.rate = 16000U;
                break;
            case SBC_SAMPLING_FREQ_32000:
                sbc_info->sbc.frequency = SBC_FREQ_32000;
                u->sample_spec.rate = 32000U;
                break;
            case SBC_SAMPLING_FREQ_44100:
                sbc_info->sbc.frequency = SBC_FREQ_44100;
                u->sample_spec.rate = 44100U;
                break;
            case SBC_SAMPLING_FREQ_48000:
                sbc_info->sbc.frequency = SBC_FREQ_48000;
                u->sample_spec.rate = 48000U;
                break;
            default:
                pa_assert_not_reached();
        }

        switch (config->channel_mode) {
            case SBC_CHANNEL_MODE_MONO:
                sbc_info->sbc.mode = SBC_MODE_MONO;
                u->sample_spec.channels = 1;
                break;
            case SBC_CHANNEL_MODE_DUAL_CHANNEL:
                sbc_info->sbc.mode = SBC_MODE_DUAL_CHANNEL;
                u->sample_spec.channels = 2;
                break;
            case SBC_CHANNEL_MODE_STEREO:
                sbc_info->sbc.mode = SBC_MODE_STEREO;
                u->sample_spec.channels = 2;
                break;
            case SBC_CHANNEL_MODE_JOINT_STEREO:
                sbc_info->sbc.mode = SBC_MODE_JOINT_STEREO;
                u->sample_spec.channels = 2;
                break;
            default:
                pa_assert_not_reached();
        }

        switch (config->allocation_method) {
            case SBC_ALLOCATION_SNR:
                sbc_info->sbc.allocation = SBC_AM_SNR;
                break;
            case SBC_ALLOCATION_LOUDNESS:
                sbc_info->sbc.allocation = SBC_AM_LOUDNESS;
                break;
            default:
                pa_assert_not_reached();
        }

        switch (config->subbands) {
            case SBC_SUBBANDS_4:
                sbc_info->sbc.subbands = SBC_SB_4;
                break;
            case SBC_SUBBANDS_8:
                sbc_info->sbc.subbands = SBC_SB_8;
                break;
            default:
                pa_assert_not_reached();
        }

        switch (config->block_length) {
            case SBC_BLOCK_LENGTH_4:
                sbc_info->sbc.blocks = SBC_BLK_4;
                break;
            case SBC_BLOCK_LENGTH_8:
                sbc_info->sbc.blocks = SBC_BLK_8;
                break;
            case SBC_BLOCK_LENGTH_12:
                sbc_info->sbc.blocks = SBC_BLK_12;
                break;
            case SBC_BLOCK_LENGTH_16:
                sbc_info->sbc.blocks = SBC_BLK_16;
                break;
            default:
                pa_assert_not_reached();
        }

        sbc_info->min_bitpool = config->min_bitpool;
        sbc_info->max_bitpool = config->max_bitpool;

        /* Set minimum bitpool for source to get the maximum possible block_size */
        sbc_info->sbc.bitpool = u->profile == PA_BLUETOOTH_PROFILE_A2DP_SINK ? sbc_info->max_bitpool : sbc_info->min_bitpool;
        sbc_info->codesize = sbc_get_codesize(&sbc_info->sbc);
        sbc_info->frame_length = sbc_get_frame_length(&sbc_info->sbc);

        pa_log_info("SBC parameters: allocation=%u, subbands=%u, blocks=%u, bitpool=%u",
                    sbc_info->sbc.allocation, sbc_info->sbc.subbands, sbc_info->sbc.blocks, sbc_info->sbc.bitpool);
    }
}

/* Run from main thread */
static int setup_transport(struct userdata *u) {
    pa_bluetooth_transport *t;

    pa_assert(u);
    pa_assert(!u->transport);
    pa_assert(u->profile != PA_BLUETOOTH_PROFILE_OFF);

    /* check if profile has a transport */
    t = u->device->transports[u->profile];
    if (!t || t->state <= PA_BLUETOOTH_TRANSPORT_STATE_DISCONNECTED) {
        pa_log_warn("Profile has no transport");
        return -1;
    }

    u->transport = t;

    if (u->profile == PA_BLUETOOTH_PROFILE_A2DP_SOURCE || u->profile == PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY)
        transport_acquire(u, true); /* In case of error, the sink/sources will be created suspended */
    else if (transport_acquire(u, false) < 0)
        return -1; /* We need to fail here until the interactions with module-suspend-on-idle and alike get improved */

    transport_config(u);

    return 0;
}

/* Run from main thread */
static pa_direction_t get_profile_direction(pa_bluetooth_profile_t p) {
    static const pa_direction_t profile_direction[] = {
        [PA_BLUETOOTH_PROFILE_A2DP_SINK] = PA_DIRECTION_OUTPUT,
        [PA_BLUETOOTH_PROFILE_A2DP_SOURCE] = PA_DIRECTION_INPUT,
        [PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT] = PA_DIRECTION_INPUT | PA_DIRECTION_OUTPUT,
        [PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY] = PA_DIRECTION_INPUT | PA_DIRECTION_OUTPUT,
        [PA_BLUETOOTH_PROFILE_OFF] = 0
    };

    return profile_direction[p];
}

/* Run from main thread */
static int init_profile(struct userdata *u) {
    int r = 0;
    pa_assert(u);
    pa_assert(u->profile != PA_BLUETOOTH_PROFILE_OFF);

    if (setup_transport(u) < 0)
        return -1;

    pa_assert(u->transport);

    if (get_profile_direction (u->profile) & PA_DIRECTION_OUTPUT)
        if (add_sink(u) < 0)
            r = -1;

    if (get_profile_direction (u->profile) & PA_DIRECTION_INPUT)
        if (add_source(u) < 0)
            r = -1;

    return r;
}

/* I/O thread function */
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

        if (pollfd && (pollfd->revents & ~(POLLOUT|POLLIN))) {
            pa_log_info("FD error: %s%s%s%s",
                        pollfd->revents & POLLERR ? "POLLERR " :"",
                        pollfd->revents & POLLHUP ? "POLLHUP " :"",
                        pollfd->revents & POLLPRI ? "POLLPRI " :"",
                        pollfd->revents & POLLNVAL ? "POLLNVAL " :"");

            if (pollfd->revents & POLLHUP) {
                pollfd = NULL;
                teardown_stream(u);
                do_write = 0;
                pending_read_bytes = 0;
                writable = false;
                pa_asyncmsgq_post(pa_thread_mq_get()->outq, PA_MSGOBJECT(u->msg), BLUETOOTH_MESSAGE_STREAM_FD_HUP, NULL, 0, NULL, NULL);
            } else
                goto fail;
        }

        if (u->source && PA_SOURCE_IS_LINKED(u->source->thread_info.state)) {

            /* We should send two blocks to the device before we expect
             * a response. */

            if (u->write_index == 0 && u->read_index <= 0)
                do_write = 2;

            if (pollfd && (pollfd->revents & POLLIN)) {
                int n_read;

                if (u->profile == PA_BLUETOOTH_PROFILE_A2DP_SOURCE)
                    n_read = a2dp_process_push(u);
                else
                    n_read = sco_process_push(u);

                if (n_read < 0)
                    goto fail;

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

                                if (u->profile == PA_BLUETOOTH_PROFILE_A2DP_SINK)
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

                    if (u->profile == PA_BLUETOOTH_PROFILE_A2DP_SINK) {
                        if ((n_written = a2dp_process_render(u)) < 0)
                            goto fail;
                    } else {
                        if ((n_written = sco_process_render(u)) < 0)
                            goto fail;
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
            transport_release(u);
            goto finish;
        }
    }

fail:
    /* If this was no regular exit from the loop we have to continue processing messages until we receive PA_MESSAGE_SHUTDOWN */
    pa_log_debug("IO thread failed");
    pa_asyncmsgq_post(pa_thread_mq_get()->outq, PA_MSGOBJECT(u->msg), BLUETOOTH_MESSAGE_IO_THREAD_FAILED, NULL, 0, NULL, NULL);
    pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);

finish:
    pa_log_debug("IO thread shutting down");
}

/* Run from main thread */
static int start_thread(struct userdata *u) {
    pa_assert(u);
    pa_assert(!u->thread);
    pa_assert(!u->rtpoll);
    pa_assert(!u->rtpoll_item);

    u->rtpoll = pa_rtpoll_new();
    pa_thread_mq_init(&u->thread_mq, u->core->mainloop, u->rtpoll);

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

/* Run from main thread */
static void stop_thread(struct userdata *u) {
    pa_assert(u);

    if (u->sink)
        pa_sink_unlink(u->sink);

    if (u->source)
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
        transport_release(u);
        u->transport = NULL;
    }

    if (u->sink) {
        pa_sink_unref(u->sink);
        u->sink = NULL;
    }

    if (u->source) {
        pa_source_unref(u->source);
        u->source = NULL;
    }

    if (u->read_smoother) {
        pa_smoother_free(u->read_smoother);
        u->read_smoother = NULL;
    }
}

/* Run from main thread */
static char *cleanup_name(const char *name) {
    char *t, *s, *d;
    bool space = false;

    pa_assert(name);

    while ((*name >= 1 && *name <= 32) || *name >= 127)
        name++;

    t = pa_xstrdup(name);

    for (s = d = t; *s; s++) {

        if (*s <= 32 || *s >= 127 || *s == '_') {
            space = true;
            continue;
        }

        if (space) {
            *(d++) = ' ';
            space = false;
        }

        *(d++) = *s;
    }

    *d = 0;

    return t;
}

/* Run from main thread */
static pa_available_t get_port_availability(struct userdata *u, pa_direction_t direction) {
    pa_available_t result = PA_AVAILABLE_NO;
    unsigned i;

    pa_assert(u);
    pa_assert(u->device);

    for (i = 0; i < PA_BLUETOOTH_PROFILE_COUNT; i++) {
        pa_bluetooth_transport *transport;

        if (!(get_profile_direction(i) & direction))
            continue;

        if (!(transport = u->device->transports[i]))
            continue;

        switch(transport->state) {
            case PA_BLUETOOTH_TRANSPORT_STATE_DISCONNECTED:
                continue;

            case PA_BLUETOOTH_TRANSPORT_STATE_IDLE:
                if (result == PA_AVAILABLE_NO)
                    result = PA_AVAILABLE_UNKNOWN;

                break;

            case PA_BLUETOOTH_TRANSPORT_STATE_PLAYING:
                return PA_AVAILABLE_YES;
        }
    }

    return result;
}

/* Run from main thread */
static pa_available_t transport_state_to_availability(pa_bluetooth_transport_state_t state) {
    switch (state) {
        case PA_BLUETOOTH_TRANSPORT_STATE_DISCONNECTED:
            return PA_AVAILABLE_NO;
        case PA_BLUETOOTH_TRANSPORT_STATE_PLAYING:
            return PA_AVAILABLE_YES;
        default:
            return PA_AVAILABLE_UNKNOWN;
    }
}

/* Run from main thread */
static void create_card_ports(struct userdata *u, pa_hashmap *ports) {
    pa_device_port *port;
    pa_device_port_new_data port_data;
    const char *name_prefix, *input_description, *output_description;

    pa_assert(u);
    pa_assert(ports);
    pa_assert(u->device);

    name_prefix = "unknown";
    input_description = _("Bluetooth Input");
    output_description = _("Bluetooth Output");

    switch (form_factor_from_class(u->device->class_of_device)) {
        case PA_BLUETOOTH_FORM_FACTOR_HEADSET:
            name_prefix = "headset";
            input_description = output_description = _("Headset");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_HANDSFREE:
            name_prefix = "handsfree";
            input_description = output_description = _("Handsfree");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_MICROPHONE:
            name_prefix = "microphone";
            input_description = _("Microphone");
            output_description = _("Bluetooth Output");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_SPEAKER:
            name_prefix = "speaker";
            input_description = _("Bluetooth Input");
            output_description = _("Speaker");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_HEADPHONE:
            name_prefix = "headphone";
            input_description = _("Bluetooth Input");
            output_description = _("Headphone");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_PORTABLE:
            name_prefix = "portable";
            input_description = output_description = _("Portable");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_CAR:
            name_prefix = "car";
            input_description = output_description = _("Car");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_HIFI:
            name_prefix = "hifi";
            input_description = output_description = _("HiFi");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_PHONE:
            name_prefix = "phone";
            input_description = output_description = _("Phone");
            break;

        case PA_BLUETOOTH_FORM_FACTOR_UNKNOWN:
            name_prefix = "unknown";
            input_description = _("Bluetooth Input");
            output_description = _("Bluetooth Output");
            break;
    }

    u->output_port_name = pa_sprintf_malloc("%s-output", name_prefix);
    pa_device_port_new_data_init(&port_data);
    pa_device_port_new_data_set_name(&port_data, u->output_port_name);
    pa_device_port_new_data_set_description(&port_data, output_description);
    pa_device_port_new_data_set_direction(&port_data, PA_DIRECTION_OUTPUT);
    pa_device_port_new_data_set_available(&port_data, get_port_availability(u, PA_DIRECTION_OUTPUT));
    pa_assert_se(port = pa_device_port_new(u->core, &port_data, 0));
    pa_assert_se(pa_hashmap_put(ports, port->name, port) >= 0);
    pa_device_port_new_data_done(&port_data);

    u->input_port_name = pa_sprintf_malloc("%s-input", name_prefix);
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
    pa_card_profile *cp = NULL;
    pa_bluetooth_profile_t *p;

    pa_assert(u->input_port_name);
    pa_assert(u->output_port_name);
    pa_assert_se(input_port = pa_hashmap_get(ports, u->input_port_name));
    pa_assert_se(output_port = pa_hashmap_get(ports, u->output_port_name));

    if (pa_streq(uuid, PA_BLUETOOTH_UUID_A2DP_SINK)) {
        cp = pa_card_profile_new("a2dp_sink", _("High Fidelity Playback (A2DP Sink)"), sizeof(pa_bluetooth_profile_t));
        cp->priority = 10;
        cp->n_sinks = 1;
        cp->n_sources = 0;
        cp->max_sink_channels = 2;
        cp->max_source_channels = 0;
        pa_hashmap_put(output_port->profiles, cp->name, cp);

        p = PA_CARD_PROFILE_DATA(cp);
        *p = PA_BLUETOOTH_PROFILE_A2DP_SINK;
    } else if (pa_streq(uuid, PA_BLUETOOTH_UUID_A2DP_SOURCE)) {
        cp = pa_card_profile_new("a2dp_source", _("High Fidelity Capture (A2DP Source)"), sizeof(pa_bluetooth_profile_t));
        cp->priority = 10;
        cp->n_sinks = 0;
        cp->n_sources = 1;
        cp->max_sink_channels = 0;
        cp->max_source_channels = 2;
        pa_hashmap_put(input_port->profiles, cp->name, cp);

        p = PA_CARD_PROFILE_DATA(cp);
        *p = PA_BLUETOOTH_PROFILE_A2DP_SOURCE;
    } else if (pa_streq(uuid, PA_BLUETOOTH_UUID_HSP_HS) || pa_streq(uuid, PA_BLUETOOTH_UUID_HFP_HF)) {
        cp = pa_card_profile_new("headset_head_unit", _("Headset Head Unit (HSP/HFP)"), sizeof(pa_bluetooth_profile_t));
        cp->priority = 20;
        cp->n_sinks = 1;
        cp->n_sources = 1;
        cp->max_sink_channels = 1;
        cp->max_source_channels = 1;
        pa_hashmap_put(input_port->profiles, cp->name, cp);
        pa_hashmap_put(output_port->profiles, cp->name, cp);

        p = PA_CARD_PROFILE_DATA(cp);
        *p = PA_BLUETOOTH_PROFILE_HEADSET_HEAD_UNIT;
    } else if (pa_streq(uuid, PA_BLUETOOTH_UUID_HSP_AG) || pa_streq(uuid, PA_BLUETOOTH_UUID_HFP_AG)) {
        cp = pa_card_profile_new("headset_audio_gateway", _("Headset Audio Gateway (HSP/HFP)"), sizeof(pa_bluetooth_profile_t));
        cp->priority = 20;
        cp->n_sinks = 1;
        cp->n_sources = 1;
        cp->max_sink_channels = 1;
        cp->max_source_channels = 1;
        pa_hashmap_put(input_port->profiles, cp->name, cp);
        pa_hashmap_put(output_port->profiles, cp->name, cp);

        p = PA_CARD_PROFILE_DATA(cp);
        *p = PA_BLUETOOTH_PROFILE_HEADSET_AUDIO_GATEWAY;
    }

    if (cp) {
        if (u->device->transports[*p])
            cp->available = transport_state_to_availability(u->device->transports[*p]->state);
        else
            cp->available = PA_AVAILABLE_NO;
    }

    return cp;
}

/* Run from main thread */
static int set_profile_cb(pa_card *c, pa_card_profile *new_profile) {
    struct userdata *u;
    pa_bluetooth_profile_t *p;

    pa_assert(c);
    pa_assert(new_profile);
    pa_assert_se(u = c->userdata);

    p = PA_CARD_PROFILE_DATA(new_profile);

    if (*p != PA_BLUETOOTH_PROFILE_OFF) {
        const pa_bluetooth_device *d = u->device;

        if (!d->transports[*p] || d->transports[*p]->state <= PA_BLUETOOTH_TRANSPORT_STATE_DISCONNECTED) {
            pa_log_warn("Refused to switch profile to %s: Not connected", new_profile->name);
            return -PA_ERR_IO;
        }
    }

    stop_thread(u);

    u->profile = *p;

    if (u->profile != PA_BLUETOOTH_PROFILE_OFF)
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
static int add_card(struct userdata *u) {
    const pa_bluetooth_device *d;
    pa_card_new_data data;
    char *alias;
    pa_bluetooth_form_factor_t ff;
    pa_card_profile *cp;
    pa_bluetooth_profile_t *p;
    const char *uuid;
    void *state;

    pa_assert(u);
    pa_assert(u->device);

    d = u->device;

    pa_card_new_data_init(&data);
    data.driver = __FILE__;
    data.module = u->module;

    alias = cleanup_name(d->alias);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_DESCRIPTION, alias);
    pa_xfree(alias);

    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_STRING, d->address);
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_API, "bluez");
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_CLASS, "sound");
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_BUS, "bluetooth");

    if ((ff = form_factor_from_class(d->class_of_device)) != PA_BLUETOOTH_FORM_FACTOR_UNKNOWN)
        pa_proplist_sets(data.proplist, PA_PROP_DEVICE_FORM_FACTOR, form_factor_to_string(ff));

    pa_proplist_sets(data.proplist, "bluez.path", d->path);
    pa_proplist_setf(data.proplist, "bluez.class", "0x%06x", d->class_of_device);
    pa_proplist_sets(data.proplist, "bluez.alias", d->alias);
    data.name = pa_sprintf_malloc("bluez_card.%s", d->address);
    data.namereg_fail = false;

    create_card_ports(u, data.ports);

    PA_HASHMAP_FOREACH(uuid, d->uuids, state) {
        cp = create_card_profile(u, uuid, data.ports);

        if (!cp)
            continue;

        if (pa_hashmap_get(data.profiles, cp->name)) {
            pa_card_profile_free(cp);
            continue;
        }

        pa_hashmap_put(data.profiles, cp->name, cp);
    }

    pa_assert(!pa_hashmap_isempty(data.profiles));

    cp = pa_card_profile_new("off", _("Off"), sizeof(pa_bluetooth_profile_t));
    cp->available = PA_AVAILABLE_YES;
    p = PA_CARD_PROFILE_DATA(cp);
    *p = PA_BLUETOOTH_PROFILE_OFF;
    pa_hashmap_put(data.profiles, cp->name, cp);

    u->card = pa_card_new(u->core, &data);
    pa_card_new_data_done(&data);
    if (!u->card) {
        pa_log("Failed to allocate card.");
        return -1;
    }

    u->card->userdata = u;
    u->card->set_profile = set_profile_cb;

    p = PA_CARD_PROFILE_DATA(u->card->active_profile);
    u->profile = *p;

    return 0;
}

/* Run from main thread */
static void handle_transport_state_change(struct userdata *u, struct pa_bluetooth_transport *t) {
    bool acquire = false;
    bool release = false;
    pa_card_profile *cp;
    pa_device_port *port;
    pa_available_t oldavail;

    pa_assert(u);
    pa_assert(t);
    pa_assert_se(cp = pa_hashmap_get(u->card->profiles, pa_bluetooth_profile_to_string(t->profile)));

    oldavail = cp->available;
    pa_card_profile_set_available(cp, transport_state_to_availability(t->state));

    /* Update port availability */
    pa_assert_se(port = pa_hashmap_get(u->card->ports, u->output_port_name));
    pa_device_port_set_available(port, get_port_availability(u, PA_DIRECTION_OUTPUT));
    pa_assert_se(port = pa_hashmap_get(u->card->ports, u->input_port_name));
    pa_device_port_set_available(port, get_port_availability(u, PA_DIRECTION_INPUT));

    /* Acquire or release transport as needed */
    acquire = (t->state == PA_BLUETOOTH_TRANSPORT_STATE_PLAYING && u->profile == t->profile);
    release = (oldavail != PA_AVAILABLE_NO && t->state != PA_BLUETOOTH_TRANSPORT_STATE_PLAYING && u->profile == t->profile);

    if (acquire && transport_acquire(u, true) >= 0) {
        if (u->source) {
            pa_log_debug("Resuming source %s because its transport state changed to playing", u->source->name);

            /* We remove the IDLE suspend cause, because otherwise
             * module-loopback doesn't uncork its streams. FIXME: Messing with
             * the IDLE suspend cause here is wrong, the correct way to handle
             * this would probably be to uncork the loopback streams not only
             * when the other end is unsuspended, but also when the other end's
             * suspend cause changes to IDLE only (currently there's no
             * notification mechanism for suspend cause changes, though). */
            pa_source_suspend(u->source, false, PA_SUSPEND_IDLE|PA_SUSPEND_USER);
        }

        if (u->sink) {
            pa_log_debug("Resuming sink %s because its transport state changed to playing", u->sink->name);

            /* FIXME: See the previous comment. */
            pa_sink_suspend(u->sink, false, PA_SUSPEND_IDLE|PA_SUSPEND_USER);
        }
    }

    if (release && u->transport_acquired) {
        /* FIXME: this release is racy, since the audio stream might have
         * been set up again in the meantime (but not processed yet by PA).
         * BlueZ should probably release the transport automatically, and in
         * that case we would just mark the transport as released */

        /* Remote side closed the stream so we consider it PA_SUSPEND_USER */
        if (u->source) {
            pa_log_debug("Suspending source %s because the remote end closed the stream", u->source->name);
            pa_source_suspend(u->source, true, PA_SUSPEND_USER);
        }

        if (u->sink) {
            pa_log_debug("Suspending sink %s because the remote end closed the stream", u->sink->name);
            pa_sink_suspend(u->sink, true, PA_SUSPEND_USER);
        }
    }
}

/* Run from main thread */
static pa_hook_result_t device_connection_changed_cb(pa_bluetooth_discovery *y, const pa_bluetooth_device *d, struct userdata *u) {
    pa_assert(d);
    pa_assert(u);

    if (d != u->device || pa_bluetooth_device_any_transport_connected(d))
        return PA_HOOK_OK;

    pa_log_debug("Unloading module for device %s", d->path);
    pa_module_unload(u->core, u->module, true);

    return PA_HOOK_OK;
}

/* Run from main thread */
static pa_hook_result_t transport_state_changed_cb(pa_bluetooth_discovery *y, pa_bluetooth_transport *t, struct userdata *u) {
    pa_assert(t);
    pa_assert(u);

    if (t == u->transport && t->state <= PA_BLUETOOTH_TRANSPORT_STATE_DISCONNECTED)
        pa_assert_se(pa_card_set_profile(u->card, pa_hashmap_get(u->card->profiles, "off"), false) >= 0);

    if (t->device == u->device)
        handle_transport_state_change(u, t);

    return PA_HOOK_OK;
}

static pa_hook_result_t transport_speaker_gain_changed_cb(pa_bluetooth_discovery *y, pa_bluetooth_transport *t, struct userdata *u) {
    pa_volume_t volume;
    pa_cvolume v;
    uint16_t gain;

    pa_assert(t);
    pa_assert(u);

    if (t != u->transport)
      return PA_HOOK_OK;

    gain = t->speaker_gain;
    volume = (pa_volume_t) (gain * PA_VOLUME_NORM / HSP_MAX_GAIN);

    /* increment volume by one to correct rounding errors */
    if (volume < PA_VOLUME_NORM)
        volume++;

    pa_cvolume_set(&v, u->sample_spec.channels, volume);
    pa_sink_volume_changed(u->sink, &v);

    return PA_HOOK_OK;
}

static pa_hook_result_t transport_microphone_gain_changed_cb(pa_bluetooth_discovery *y, pa_bluetooth_transport *t, struct userdata *u) {
    pa_volume_t volume;
    pa_cvolume v;
    uint16_t gain;

    pa_assert(t);
    pa_assert(u);

    if (t != u->transport)
      return PA_HOOK_OK;

    gain = t->microphone_gain;
    volume = (pa_volume_t) (gain * PA_VOLUME_NORM / HSP_MAX_GAIN);

    /* increment volume by one to correct rounding errors */
    if (volume < PA_VOLUME_NORM)
        volume++;

    pa_cvolume_set(&v, u->sample_spec.channels, volume);
    pa_source_volume_changed(u->source, &v);

    return PA_HOOK_OK;
}

/* Run from main thread context */
static int device_process_msg(pa_msgobject *obj, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct bluetooth_msg *m = BLUETOOTH_MSG(obj);
    struct userdata *u = m->card->userdata;

    switch (code) {
        case BLUETOOTH_MESSAGE_IO_THREAD_FAILED:
            if (m->card->module->unload_requested)
                break;

            pa_log_debug("Switching the profile to off due to IO thread failure.");
            pa_assert_se(pa_card_set_profile(m->card, pa_hashmap_get(m->card->profiles, "off"), false) >= 0);
            break;
        case BLUETOOTH_MESSAGE_STREAM_FD_HUP:
            pa_bluetooth_transport_set_state(u->transport, PA_BLUETOOTH_TRANSPORT_STATE_IDLE);
            break;
    }

    return 0;
}

int pa__init(pa_module* m) {
    struct userdata *u;
    const char *path;
    pa_modargs *ma;

    pa_assert(m);

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->module = m;
    u->core = m->core;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log_error("Failed to parse module arguments");
        goto fail;
    }

    if (!(path = pa_modargs_get_value(ma, "path", NULL))) {
        pa_log_error("Failed to get device path from module arguments");
        goto fail;
    }

    if ((u->discovery = pa_shared_get(u->core, "bluetooth-discovery")))
        pa_bluetooth_discovery_ref(u->discovery);
    else {
        pa_log_error("module-bluez5-discover doesn't seem to be loaded, refusing to load module-bluez5-device");
        goto fail;
    }

    if (!(u->device = pa_bluetooth_discovery_get_device_by_path(u->discovery, path))) {
        pa_log_error("%s is unknown", path);
        goto fail;
    }

    pa_modargs_free(ma);

    u->device_connection_changed_slot =
        pa_hook_connect(pa_bluetooth_discovery_hook(u->discovery, PA_BLUETOOTH_HOOK_DEVICE_CONNECTION_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) device_connection_changed_cb, u);

    u->transport_state_changed_slot =
        pa_hook_connect(pa_bluetooth_discovery_hook(u->discovery, PA_BLUETOOTH_HOOK_TRANSPORT_STATE_CHANGED),
                        PA_HOOK_NORMAL, (pa_hook_cb_t) transport_state_changed_cb, u);

    u->transport_speaker_gain_changed_slot =
        pa_hook_connect(pa_bluetooth_discovery_hook(u->discovery, PA_BLUETOOTH_HOOK_TRANSPORT_SPEAKER_GAIN_CHANGED), PA_HOOK_NORMAL, (pa_hook_cb_t) transport_speaker_gain_changed_cb, u);

    u->transport_microphone_gain_changed_slot =
        pa_hook_connect(pa_bluetooth_discovery_hook(u->discovery, PA_BLUETOOTH_HOOK_TRANSPORT_MICROPHONE_GAIN_CHANGED), PA_HOOK_NORMAL, (pa_hook_cb_t) transport_microphone_gain_changed_cb, u);


    if (add_card(u) < 0)
        goto fail;

    if (!(u->msg = pa_msgobject_new(bluetooth_msg)))
        goto fail;

    u->msg->parent.process_msg = device_process_msg;
    u->msg->card = u->card;

    if (u->profile != PA_BLUETOOTH_PROFILE_OFF)
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

    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    stop_thread(u);

    if (u->device_connection_changed_slot)
        pa_hook_slot_free(u->device_connection_changed_slot);

    if (u->transport_state_changed_slot)
        pa_hook_slot_free(u->transport_state_changed_slot);

    if (u->transport_speaker_gain_changed_slot)
        pa_hook_slot_free(u->transport_speaker_gain_changed_slot);

    if (u->transport_microphone_gain_changed_slot)
        pa_hook_slot_free(u->transport_microphone_gain_changed_slot);

    if (u->sbc_info.buffer)
        pa_xfree(u->sbc_info.buffer);

    if (u->sbc_info.sbc_initialized)
        sbc_finish(&u->sbc_info.sbc);

    if (u->msg)
        pa_xfree(u->msg);

    if (u->card)
        pa_card_free(u->card);

    if (u->discovery)
        pa_bluetooth_discovery_unref(u->discovery);

    pa_xfree(u->output_port_name);
    pa_xfree(u->input_port_name);

    pa_xfree(u);
}

int pa__get_n_used(pa_module *m) {
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return (u->sink ? pa_sink_linked_by(u->sink) : 0) + (u->source ? pa_source_linked_by(u->source) : 0);
}

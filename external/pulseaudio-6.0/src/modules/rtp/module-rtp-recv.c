
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

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/llist.h>
#include <pulsecore/sink.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/memblockq.h>
#include <pulsecore/log.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/namereg.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/macro.h>
#include <pulsecore/socket-util.h>
#include <pulsecore/atomic.h>
#include <pulsecore/once.h>
#include <pulsecore/poll.h>
#include <pulsecore/arpa-inet.h>

#include "module-rtp-recv-symdef.h"

#include "rtp.h"
#include "sdp.h"
#include "sap.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Receive data from a network via RTP/SAP/SDP");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink=<name of the sink> "
        "sap_address=<multicast address to listen on> "
        "latency_msec=<latency in ms> "
);

#define SAP_PORT 9875
#define DEFAULT_SAP_ADDRESS "224.0.0.56"
#define DEFAULT_LATENCY_MSEC 500
#define MEMBLOCKQ_MAXLENGTH (1024*1024*40)
#define MAX_SESSIONS 16
#define DEATH_TIMEOUT 20
#define RATE_UPDATE_INTERVAL (5*PA_USEC_PER_SEC)

static const char* const valid_modargs[] = {
    "sink",
    "sap_address",
    "latency_msec",
    NULL
};

struct session {
    struct userdata *userdata;
    PA_LLIST_FIELDS(struct session);

    pa_sink_input *sink_input;
    pa_memblockq *memblockq;

    bool first_packet;
    uint32_t ssrc;
    uint32_t offset;

    struct pa_sdp_info sdp_info;

    pa_rtp_context rtp_context;

    pa_rtpoll_item *rtpoll_item;

    pa_atomic_t timestamp;

    pa_usec_t intended_latency;
    pa_usec_t sink_latency;

    pa_usec_t last_rate_update;
    pa_usec_t last_latency;
    double estimated_rate;
    double avg_estimated_rate;
};

struct userdata {
    pa_module *module;
    pa_core *core;

    pa_sap_context sap_context;
    pa_io_event* sap_event;

    pa_time_event *check_death_event;

    char *sink_name;

    PA_LLIST_HEAD(struct session, sessions);
    pa_hashmap *by_origin;
    int n_sessions;

    pa_usec_t latency;
};

static void session_free(struct session *s);

/* Called from I/O thread context */
static int sink_input_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct session *s = PA_SINK_INPUT(o)->userdata;

    switch (code) {
        case PA_SINK_INPUT_MESSAGE_GET_LATENCY:
            *((pa_usec_t*) data) = pa_bytes_to_usec(pa_memblockq_get_length(s->memblockq), &s->sink_input->sample_spec);

            /* Fall through, the default handler will add in the extra
             * latency added by the resampler */
            break;
    }

    return pa_sink_input_process_msg(o, code, data, offset, chunk);
}

/* Called from I/O thread context */
static int sink_input_pop_cb(pa_sink_input *i, size_t length, pa_memchunk *chunk) {
    struct session *s;
    pa_sink_input_assert_ref(i);
    pa_assert_se(s = i->userdata);

    if (pa_memblockq_peek(s->memblockq, chunk) < 0)
        return -1;

    pa_memblockq_drop(s->memblockq, chunk->length);

    return 0;
}

/* Called from I/O thread context */
static void sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct session *s;

    pa_sink_input_assert_ref(i);
    pa_assert_se(s = i->userdata);

    pa_memblockq_rewind(s->memblockq, nbytes);
}

/* Called from I/O thread context */
static void sink_input_update_max_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct session *s;

    pa_sink_input_assert_ref(i);
    pa_assert_se(s = i->userdata);

    pa_memblockq_set_maxrewind(s->memblockq, nbytes);
}

/* Called from main context */
static void sink_input_kill(pa_sink_input* i) {
    struct session *s;
    pa_sink_input_assert_ref(i);
    pa_assert_se(s = i->userdata);

    pa_hashmap_remove_and_free(s->userdata->by_origin, s->sdp_info.origin);
}

/* Called from IO context */
static void sink_input_suspend_within_thread(pa_sink_input* i, bool b) {
    struct session *s;
    pa_sink_input_assert_ref(i);
    pa_assert_se(s = i->userdata);

    if (b)
        pa_memblockq_flush_read(s->memblockq);
    else
        s->first_packet = false;
}

/* Called from I/O thread context */
static int rtpoll_work_cb(pa_rtpoll_item *i) {
    pa_memchunk chunk;
    int64_t k, j, delta;
    struct timeval now = { 0, 0 };
    struct session *s;
    struct pollfd *p;

    pa_assert_se(s = pa_rtpoll_item_get_userdata(i));

    p = pa_rtpoll_item_get_pollfd(i, NULL);

    if (p->revents & (POLLERR|POLLNVAL|POLLHUP|POLLOUT)) {
        pa_log("poll() signalled bad revents.");
        return -1;
    }

    if ((p->revents & POLLIN) == 0)
        return 0;

    p->revents = 0;

    if (pa_rtp_recv(&s->rtp_context, &chunk, s->userdata->module->core->mempool, &now) < 0)
        return 0;

    if (s->sdp_info.payload != s->rtp_context.payload ||
        !PA_SINK_IS_OPENED(s->sink_input->sink->thread_info.state)) {
        pa_memblock_unref(chunk.memblock);
        return 0;
    }

    if (!s->first_packet) {
        s->first_packet = true;

        s->ssrc = s->rtp_context.ssrc;
        s->offset = s->rtp_context.timestamp;

        if (s->ssrc == s->userdata->module->core->cookie)
            pa_log_warn("Detected RTP packet loop!");
    } else {
        if (s->ssrc != s->rtp_context.ssrc) {
            pa_memblock_unref(chunk.memblock);
            return 0;
        }
    }

    /* Check whether there was a timestamp overflow */
    k = (int64_t) s->rtp_context.timestamp - (int64_t) s->offset;
    j = (int64_t) 0x100000000LL - (int64_t) s->offset + (int64_t) s->rtp_context.timestamp;

    if ((k < 0 ? -k : k) < (j < 0 ? -j : j))
        delta = k;
    else
        delta = j;

    pa_memblockq_seek(s->memblockq, delta * (int64_t) s->rtp_context.frame_size, PA_SEEK_RELATIVE, true);

    if (now.tv_sec == 0) {
        PA_ONCE_BEGIN {
            pa_log_warn("Using artificial time instead of timestamp");
        } PA_ONCE_END;
        pa_rtclock_get(&now);
    } else
        pa_rtclock_from_wallclock(&now);

    if (pa_memblockq_push(s->memblockq, &chunk) < 0) {
        pa_log_warn("Queue overrun");
        pa_memblockq_seek(s->memblockq, (int64_t) chunk.length, PA_SEEK_RELATIVE, true);
    }

/*     pa_log("blocks in q: %u", pa_memblockq_get_nblocks(s->memblockq)); */

    pa_memblock_unref(chunk.memblock);

    /* The next timestamp we expect */
    s->offset = s->rtp_context.timestamp + (uint32_t) (chunk.length / s->rtp_context.frame_size);

    pa_atomic_store(&s->timestamp, (int) now.tv_sec);

    if (s->last_rate_update + RATE_UPDATE_INTERVAL < pa_timeval_load(&now)) {
        pa_usec_t wi, ri, render_delay, sink_delay = 0, latency;
        uint32_t base_rate = s->sink_input->sink->sample_spec.rate;
        uint32_t current_rate = s->sink_input->sample_spec.rate;
        uint32_t new_rate;
        double estimated_rate, alpha = 0.02;

        pa_log_debug("Updating sample rate");

        wi = pa_bytes_to_usec((uint64_t) pa_memblockq_get_write_index(s->memblockq), &s->sink_input->sample_spec);
        ri = pa_bytes_to_usec((uint64_t) pa_memblockq_get_read_index(s->memblockq), &s->sink_input->sample_spec);

        pa_log_debug("wi=%lu ri=%lu", (unsigned long) wi, (unsigned long) ri);

        sink_delay = pa_sink_get_latency_within_thread(s->sink_input->sink);
        render_delay = pa_bytes_to_usec(pa_memblockq_get_length(s->sink_input->thread_info.render_memblockq), &s->sink_input->sink->sample_spec);

        if (ri > render_delay+sink_delay)
            ri -= render_delay+sink_delay;
        else
            ri = 0;

        if (wi < ri)
            latency = 0;
        else
            latency = wi - ri;

        pa_log_debug("Write index deviates by %0.2f ms, expected %0.2f ms", (double) latency/PA_USEC_PER_MSEC, (double) s->intended_latency/PA_USEC_PER_MSEC);

        /* The buffer is filling with some unknown rate R̂ samples/second. If the rate of reading in
         * the last T seconds was Rⁿ, then the increase in buffer latency ΔLⁿ = Lⁿ - Lⁿ⁻ⁱ in that
         * same period is ΔLⁿ = (TR̂ - TRⁿ) / R̂, giving the estimated target rate
         *                                           T
         *                                 R̂ = ─────────────── Rⁿ .                             (1)
         *                                     T - (Lⁿ - Lⁿ⁻ⁱ)
         *
         * Setting the sample rate to R̂ results in the latency being constant (if the estimate of R̂
         * is correct).  But there is also the requirement to keep the buffer at a predefined target
         * latency L̂.  So instead of setting Rⁿ⁺ⁱ to R̂ immediately, the strategy will be to reduce R
         * from Rⁿ⁺ⁱ to R̂ in a steps of T seconds, where Rⁿ⁺ⁱ is chosen such that in the total time
         * aT the latency is reduced from Lⁿ to L̂.  This strategy translates to the requirements
         *            ₐ      R̂ - Rⁿ⁺ʲ                            a-j+1         j-1
         *            Σ  T ────────── = L̂ - Lⁿ    with    Rⁿ⁺ʲ = ───── Rⁿ⁺ⁱ + ───── R̂ .
         *           ʲ⁼ⁱ        R̂                                  a            a
         * Solving for Rⁿ⁺ⁱ gives
         *                                     T - ²∕ₐ₊₁(L̂ - Lⁿ)
         *                              Rⁿ⁺ⁱ = ───────────────── R̂ .                            (2)
         *                                            T
         * In the code below a = 7 is used.
         *
         * Equation (1) is not directly used in (2), but instead an exponentially weighted average
         * of the estimated rate R̂ is used.  This average R̅ is defined as
         *                                R̅ⁿ = α R̂ⁿ + (1-α) R̅ⁿ⁻ⁱ .
         * Because it is difficult to find a fixed value for the coefficient α such that the
         * averaging is without significant lag but oscillations are filtered out, a heuristic is
         * used.  When the successive estimates R̂ⁿ do not change much then α→1, but when there is a
         * sudden spike in the estimated rate α→0, such that the deviation is given little weight.
         */
        estimated_rate = (double) current_rate * (double) RATE_UPDATE_INTERVAL / (double) (RATE_UPDATE_INTERVAL + s->last_latency - latency);
        if (fabs(s->estimated_rate - s->avg_estimated_rate) > 1) {
          double ratio = (estimated_rate + s->estimated_rate - 2*s->avg_estimated_rate) / (s->estimated_rate - s->avg_estimated_rate);
          alpha = PA_CLAMP(2 * (ratio + fabs(ratio)) / (4 + ratio*ratio), 0.02, 0.8);
        }
        s->avg_estimated_rate = alpha * estimated_rate + (1-alpha) * s->avg_estimated_rate;
        s->estimated_rate = estimated_rate;
        pa_log_debug("Estimated target rate: %.0f Hz, using average of %.0f Hz  (α=%.3f)", estimated_rate, s->avg_estimated_rate, alpha);
        new_rate = (uint32_t) ((double) (RATE_UPDATE_INTERVAL + latency/4 - s->intended_latency/4) / (double) RATE_UPDATE_INTERVAL * s->avg_estimated_rate);
        s->last_latency = latency;

        if (new_rate < (uint32_t) (base_rate*0.8) || new_rate > (uint32_t) (base_rate*1.25)) {
            pa_log_warn("Sample rates too different, not adjusting (%u vs. %u).", base_rate, new_rate);
            new_rate = base_rate;
        } else {
            if (base_rate < new_rate + 20 && new_rate < base_rate + 20)
              new_rate = base_rate;
            /* Do the adjustment in small steps; 2‰ can be considered inaudible */
            if (new_rate < (uint32_t) (current_rate*0.998) || new_rate > (uint32_t) (current_rate*1.002)) {
                pa_log_info("New rate of %u Hz not within 2‰ of %u Hz, forcing smaller adjustment", new_rate, current_rate);
                new_rate = PA_CLAMP(new_rate, (uint32_t) (current_rate*0.998), (uint32_t) (current_rate*1.002));
            }
        }
        s->sink_input->sample_spec.rate = new_rate;

        pa_assert(pa_sample_spec_valid(&s->sink_input->sample_spec));

        pa_resampler_set_input_rate(s->sink_input->thread_info.resampler, s->sink_input->sample_spec.rate);

        pa_log_debug("Updated sampling rate to %lu Hz.", (unsigned long) s->sink_input->sample_spec.rate);

        s->last_rate_update = pa_timeval_load(&now);
    }

    if (pa_memblockq_is_readable(s->memblockq) &&
        s->sink_input->thread_info.underrun_for > 0) {
        pa_log_debug("Requesting rewind due to end of underrun");
        pa_sink_input_request_rewind(s->sink_input,
                                     (size_t) (s->sink_input->thread_info.underrun_for == (uint64_t) -1 ? 0 : s->sink_input->thread_info.underrun_for),
                                     false, true, false);
    }

    return 1;
}

/* Called from I/O thread context */
static void sink_input_attach(pa_sink_input *i) {
    struct session *s;
    struct pollfd *p;

    pa_sink_input_assert_ref(i);
    pa_assert_se(s = i->userdata);

    pa_assert(!s->rtpoll_item);
    s->rtpoll_item = pa_rtpoll_item_new(i->sink->thread_info.rtpoll, PA_RTPOLL_LATE, 1);

    p = pa_rtpoll_item_get_pollfd(s->rtpoll_item, NULL);
    p->fd = s->rtp_context.fd;
    p->events = POLLIN;
    p->revents = 0;

    pa_rtpoll_item_set_work_callback(s->rtpoll_item, rtpoll_work_cb);
    pa_rtpoll_item_set_userdata(s->rtpoll_item, s);
}

/* Called from I/O thread context */
static void sink_input_detach(pa_sink_input *i) {
    struct session *s;
    pa_sink_input_assert_ref(i);
    pa_assert_se(s = i->userdata);

    pa_assert(s->rtpoll_item);
    pa_rtpoll_item_free(s->rtpoll_item);
    s->rtpoll_item = NULL;
}

static int mcast_socket(const struct sockaddr* sa, socklen_t salen) {
    int af, fd = -1, r, one;

    pa_assert(sa);
    pa_assert(salen > 0);

    af = sa->sa_family;
    if ((fd = pa_socket_cloexec(af, SOCK_DGRAM, 0)) < 0) {
        pa_log("Failed to create socket: %s", pa_cstrerror(errno));
        goto fail;
    }

    pa_make_udp_socket_low_delay(fd);

#ifdef SO_TIMESTAMP
    one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &one, sizeof(one)) < 0) {
        pa_log("SO_TIMESTAMP failed: %s", pa_cstrerror(errno));
        goto fail;
    }
#else
    pa_log("SO_TIMESTAMP unsupported on this platform");
    goto fail;
#endif

    one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        pa_log("SO_REUSEADDR failed: %s", pa_cstrerror(errno));
        goto fail;
    }

    r = 0;
    if (af == AF_INET) {
        /* IPv4 multicast addresses are in the 224.0.0.0-239.255.255.255 range */
        static const uint32_t ipv4_mcast_mask = 0xe0000000;

        if ((ntohl(((const struct sockaddr_in*) sa)->sin_addr.s_addr) & ipv4_mcast_mask) == ipv4_mcast_mask) {
            struct ip_mreq mr4;
            memset(&mr4, 0, sizeof(mr4));
            mr4.imr_multiaddr = ((const struct sockaddr_in*) sa)->sin_addr;
            r = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr4, sizeof(mr4));
        }
#ifdef HAVE_IPV6
    } else if (af == AF_INET6) {
        /* IPv6 multicast addresses have 255 as the most significant byte */
        if (((const struct sockaddr_in6*) sa)->sin6_addr.s6_addr[0] == 0xff) {
            struct ipv6_mreq mr6;
            memset(&mr6, 0, sizeof(mr6));
            mr6.ipv6mr_multiaddr = ((const struct sockaddr_in6*) sa)->sin6_addr;
            r = setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mr6, sizeof(mr6));
        }
#endif
    } else
        pa_assert_not_reached();

    if (r < 0) {
        pa_log_info("Joining mcast group failed: %s", pa_cstrerror(errno));
        goto fail;
    }

    if (bind(fd, sa, salen) < 0) {
        pa_log("bind() failed: %s", pa_cstrerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0)
        close(fd);

    return -1;
}

static struct session *session_new(struct userdata *u, const pa_sdp_info *sdp_info) {
    struct session *s = NULL;
    pa_sink *sink;
    int fd = -1;
    pa_memchunk silence;
    pa_sink_input_new_data data;
    struct timeval now;

    pa_assert(u);
    pa_assert(sdp_info);

    if (u->n_sessions >= MAX_SESSIONS) {
        pa_log("Session limit reached.");
        goto fail;
    }

    if (!(sink = pa_namereg_get(u->module->core, u->sink_name, PA_NAMEREG_SINK))) {
        pa_log("Sink does not exist.");
        goto fail;
    }

    pa_rtclock_get(&now);

    s = pa_xnew0(struct session, 1);
    s->userdata = u;
    s->first_packet = false;
    s->sdp_info = *sdp_info;
    s->rtpoll_item = NULL;
    s->intended_latency = u->latency;
    s->last_rate_update = pa_timeval_load(&now);
    s->last_latency = u->latency;
    s->estimated_rate = (double) sink->sample_spec.rate;
    s->avg_estimated_rate = (double) sink->sample_spec.rate;
    pa_atomic_store(&s->timestamp, (int) now.tv_sec);

    if ((fd = mcast_socket((const struct sockaddr*) &sdp_info->sa, sdp_info->salen)) < 0)
        goto fail;

    pa_sink_input_new_data_init(&data);
    pa_sink_input_new_data_set_sink(&data, sink, false);
    data.driver = __FILE__;
    pa_proplist_sets(data.proplist, PA_PROP_MEDIA_ROLE, "stream");
    pa_proplist_setf(data.proplist, PA_PROP_MEDIA_NAME,
                     "RTP Stream%s%s%s",
                     sdp_info->session_name ? " (" : "",
                     sdp_info->session_name ? sdp_info->session_name : "",
                     sdp_info->session_name ? ")" : "");

    if (sdp_info->session_name)
        pa_proplist_sets(data.proplist, "rtp.session", sdp_info->session_name);
    pa_proplist_sets(data.proplist, "rtp.origin", sdp_info->origin);
    pa_proplist_setf(data.proplist, "rtp.payload", "%u", (unsigned) sdp_info->payload);
    data.module = u->module;
    pa_sink_input_new_data_set_sample_spec(&data, &sdp_info->sample_spec);
    data.flags = PA_SINK_INPUT_VARIABLE_RATE;

    pa_sink_input_new(&s->sink_input, u->module->core, &data);
    pa_sink_input_new_data_done(&data);

    if (!s->sink_input) {
        pa_log("Failed to create sink input.");
        goto fail;
    }

    s->sink_input->userdata = s;

    s->sink_input->parent.process_msg = sink_input_process_msg;
    s->sink_input->pop = sink_input_pop_cb;
    s->sink_input->process_rewind = sink_input_process_rewind_cb;
    s->sink_input->update_max_rewind = sink_input_update_max_rewind_cb;
    s->sink_input->kill = sink_input_kill;
    s->sink_input->attach = sink_input_attach;
    s->sink_input->detach = sink_input_detach;
    s->sink_input->suspend_within_thread = sink_input_suspend_within_thread;

    pa_sink_input_get_silence(s->sink_input, &silence);

    s->sink_latency = pa_sink_input_set_requested_latency(s->sink_input, s->intended_latency/2);

    if (s->intended_latency < s->sink_latency*2)
        s->intended_latency = s->sink_latency*2;

    s->memblockq = pa_memblockq_new(
            "module-rtp-recv memblockq",
            0,
            MEMBLOCKQ_MAXLENGTH,
            MEMBLOCKQ_MAXLENGTH,
            &s->sink_input->sample_spec,
            pa_usec_to_bytes(s->intended_latency - s->sink_latency, &s->sink_input->sample_spec),
            0,
            0,
            &silence);

    pa_memblock_unref(silence.memblock);

    pa_rtp_context_init_recv(&s->rtp_context, fd, pa_frame_size(&s->sdp_info.sample_spec));

    pa_hashmap_put(s->userdata->by_origin, s->sdp_info.origin, s);
    u->n_sessions++;
    PA_LLIST_PREPEND(struct session, s->userdata->sessions, s);

    pa_sink_input_put(s->sink_input);

    pa_log_info("New session '%s'", s->sdp_info.session_name);

    return s;

fail:
    pa_xfree(s);

    if (fd >= 0)
        pa_close(fd);

    return NULL;
}

static void session_free(struct session *s) {
    pa_assert(s);

    pa_log_info("Freeing session '%s'", s->sdp_info.session_name);

    pa_sink_input_unlink(s->sink_input);
    pa_sink_input_unref(s->sink_input);

    PA_LLIST_REMOVE(struct session, s->userdata->sessions, s);
    pa_assert(s->userdata->n_sessions >= 1);
    s->userdata->n_sessions--;

    pa_memblockq_free(s->memblockq);
    pa_sdp_info_destroy(&s->sdp_info);
    pa_rtp_context_destroy(&s->rtp_context);

    pa_xfree(s);
}

static void sap_event_cb(pa_mainloop_api *m, pa_io_event *e, int fd, pa_io_event_flags_t flags, void *userdata) {
    struct userdata *u = userdata;
    bool goodbye = false;
    pa_sdp_info info;
    struct session *s;

    pa_assert(m);
    pa_assert(e);
    pa_assert(u);
    pa_assert(fd == u->sap_context.fd);
    pa_assert(flags == PA_IO_EVENT_INPUT);

    if (pa_sap_recv(&u->sap_context, &goodbye) < 0)
        return;

    if (!pa_sdp_parse(u->sap_context.sdp_data, &info, goodbye))
        return;

    if (goodbye) {
        pa_hashmap_remove_and_free(u->by_origin, info.origin);
        pa_sdp_info_destroy(&info);
    } else {

        if (!(s = pa_hashmap_get(u->by_origin, info.origin))) {
            if (!session_new(u, &info))
                pa_sdp_info_destroy(&info);

        } else {
            struct timeval now;
            pa_rtclock_get(&now);
            pa_atomic_store(&s->timestamp, (int) now.tv_sec);

            pa_sdp_info_destroy(&info);
        }
    }
}

static void check_death_event_cb(pa_mainloop_api *m, pa_time_event *t, const struct timeval *tv, void *userdata) {
    struct session *s, *n;
    struct userdata *u = userdata;
    struct timeval now;

    pa_assert(m);
    pa_assert(t);
    pa_assert(u);

    pa_rtclock_get(&now);

    pa_log_debug("Checking for dead streams ...");

    for (s = u->sessions; s; s = n) {
        int k;
        n = s->next;

        k = pa_atomic_load(&s->timestamp);

        if (k + DEATH_TIMEOUT < now.tv_sec)
            pa_hashmap_remove_and_free(u->by_origin, s->sdp_info.origin);
    }

    /* Restart timer */
    pa_core_rttime_restart(u->module->core, t, pa_rtclock_now() + DEATH_TIMEOUT * PA_USEC_PER_SEC);
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_modargs *ma = NULL;
    struct sockaddr_in sa4;
#ifdef HAVE_IPV6
    struct sockaddr_in6 sa6;
#endif
    struct sockaddr *sa;
    socklen_t salen;
    const char *sap_address;
    uint32_t latency_msec;
    int fd = -1;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("failed to parse module arguments");
        goto fail;
    }

    sap_address = pa_modargs_get_value(ma, "sap_address", DEFAULT_SAP_ADDRESS);

    if (inet_pton(AF_INET, sap_address, &sa4.sin_addr) > 0) {
        sa4.sin_family = AF_INET;
        sa4.sin_port = htons(SAP_PORT);
        sa = (struct sockaddr*) &sa4;
        salen = sizeof(sa4);
#ifdef HAVE_IPV6
    } else if (inet_pton(AF_INET6, sap_address, &sa6.sin6_addr) > 0) {
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(SAP_PORT);
        sa = (struct sockaddr*) &sa6;
        salen = sizeof(sa6);
#endif
    } else {
        pa_log("Invalid SAP address '%s'", sap_address);
        goto fail;
    }

    latency_msec = DEFAULT_LATENCY_MSEC;
    if (pa_modargs_get_value_u32(ma, "latency_msec", &latency_msec) < 0 || latency_msec < 1 || latency_msec > 300000) {
        pa_log("Invalid latency specification");
        goto fail;
    }

    if ((fd = mcast_socket(sa, salen)) < 0)
        goto fail;

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->module = m;
    u->core = m->core;
    u->sink_name = pa_xstrdup(pa_modargs_get_value(ma, "sink", NULL));
    u->latency = (pa_usec_t) latency_msec * PA_USEC_PER_MSEC;

    u->sap_event = m->core->mainloop->io_new(m->core->mainloop, fd, PA_IO_EVENT_INPUT, sap_event_cb, u);
    pa_sap_context_init_recv(&u->sap_context, fd);

    PA_LLIST_HEAD_INIT(struct session, u->sessions);
    u->n_sessions = 0;
    u->by_origin = pa_hashmap_new_full(pa_idxset_string_hash_func, pa_idxset_string_compare_func, NULL, (pa_free_cb_t) session_free);

    u->check_death_event = pa_core_rttime_new(m->core, pa_rtclock_now() + DEATH_TIMEOUT * PA_USEC_PER_SEC, check_death_event_cb, u);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    if (fd >= 0)
        pa_close(fd);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->sap_event)
        m->core->mainloop->io_free(u->sap_event);

    if (u->check_death_event)
        m->core->mainloop->time_free(u->check_death_event);

    pa_sap_context_destroy(&u->sap_context);

    if (u->by_origin)
        pa_hashmap_free(u->by_origin);

    pa_xfree(u->sink_name);
    pa_xfree(u);
}

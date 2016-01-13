/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

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

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <locale.h>

#include <sndfile.h>

#include <pulse/pulseaudio.h>
#include <pulse/rtclock.h>

#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/sndfile-util.h>
#include <pulsecore/sample-util.h>

#define TIME_EVENT_USEC 50000

#define CLEAR_LINE "\x1B[K"

static enum { RECORD, PLAYBACK } mode = PLAYBACK;

static pa_context *context = NULL;
static pa_stream *stream = NULL;
static pa_mainloop_api *mainloop_api = NULL;

static void *buffer = NULL;
static size_t buffer_length = 0, buffer_index = 0;

static void *silence_buffer = NULL;
static size_t silence_buffer_length = 0;

static pa_io_event* stdio_event = NULL;

static pa_proplist *proplist = NULL;
static char *device = NULL;

static SNDFILE* sndfile = NULL;

static bool verbose = false;
static pa_volume_t volume = PA_VOLUME_NORM;
static bool volume_is_set = false;

static pa_sample_spec sample_spec = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};
static bool sample_spec_set = false;

static pa_channel_map channel_map;
static bool channel_map_set = false;

static sf_count_t (*readf_function)(SNDFILE *_sndfile, void *ptr, sf_count_t frames) = NULL;
static sf_count_t (*writef_function)(SNDFILE *_sndfile, const void *ptr, sf_count_t frames) = NULL;

static pa_stream_flags_t flags = 0;

static size_t latency = 0, process_time = 0;
static int32_t latency_msec = 0, process_time_msec = 0;

static bool raw = true;
static int file_format = -1;

static uint32_t monitor_stream = PA_INVALID_INDEX;

static uint32_t cork_requests = 0;

/* A shortcut for terminating the application */
static void quit(int ret) {
    pa_assert(mainloop_api);
    mainloop_api->quit(mainloop_api, ret);
}

/* Connection draining complete */
static void context_drain_complete(pa_context*c, void *userdata) {
    pa_context_disconnect(c);
}

/* Stream draining complete */
static void stream_drain_complete(pa_stream*s, int success, void *userdata) {
    pa_operation *o = NULL;

    if (!success) {
        pa_log(_("Failed to drain stream: %s"), pa_strerror(pa_context_errno(context)));
        quit(1);
    }

    if (verbose)
        pa_log(_("Playback stream drained."));

    pa_stream_disconnect(stream);
    pa_stream_unref(stream);
    stream = NULL;

    if (!(o = pa_context_drain(context, context_drain_complete, NULL)))
        pa_context_disconnect(context);
    else {
        pa_operation_unref(o);
        if (verbose)
            pa_log(_("Draining connection to server."));
    }
}

/* Start draining */
static void start_drain(void) {

    if (stream) {
        pa_operation *o;

        pa_stream_set_write_callback(stream, NULL, NULL);

        if (!(o = pa_stream_drain(stream, stream_drain_complete, NULL))) {
            pa_log(_("pa_stream_drain(): %s"), pa_strerror(pa_context_errno(context)));
            quit(1);
            return;
        }

        pa_operation_unref(o);
    } else
        quit(0);
}

/* Write some data to the stream */
static void do_stream_write(size_t length) {
    size_t l;
    pa_assert(length);

    if (!buffer || !buffer_length)
        return;

    l = length;
    if (l > buffer_length)
        l = buffer_length;

    if (pa_stream_write(stream, (uint8_t*) buffer + buffer_index, l, NULL, 0, PA_SEEK_RELATIVE) < 0) {
        pa_log(_("pa_stream_write() failed: %s"), pa_strerror(pa_context_errno(context)));
        quit(1);
        return;
    }

    buffer_length -= l;
    buffer_index += l;

    if (!buffer_length) {
        pa_xfree(buffer);
        buffer = NULL;
        buffer_index = buffer_length = 0;
    }
}

/* This is called whenever new data may be written to the stream */
static void stream_write_callback(pa_stream *s, size_t length, void *userdata) {
    pa_assert(s);
    pa_assert(length > 0);

    if (raw) {
        pa_assert(!sndfile);

        if (stdio_event)
            mainloop_api->io_enable(stdio_event, PA_IO_EVENT_INPUT);

        if (!buffer)
            return;

        do_stream_write(length);

    } else {
        sf_count_t bytes;
        void *data;

        pa_assert(sndfile);

        for (;;) {
            size_t data_length = length;

            if (pa_stream_begin_write(s, &data, &data_length) < 0) {
                pa_log(_("pa_stream_begin_write() failed: %s"), pa_strerror(pa_context_errno(context)));
                quit(1);
                return;
            }

            if (readf_function) {
                size_t k = pa_frame_size(&sample_spec);

                if ((bytes = readf_function(sndfile, data, (sf_count_t) (data_length/k))) > 0)
                    bytes *= (sf_count_t) k;

            } else
                bytes = sf_read_raw(sndfile, data, (sf_count_t) data_length);

            if (bytes > 0)
                pa_stream_write(s, data, (size_t) bytes, NULL, 0, PA_SEEK_RELATIVE);
            else
                pa_stream_cancel_write(s);

            /* EOF? */
            if (bytes < (sf_count_t) data_length) {
                start_drain();
                break;
            }

            /* Request fulfilled */
            if ((size_t) bytes >= length)
                break;

            length -= bytes;
        }
    }
}

/* This is called whenever new data may is available */
static void stream_read_callback(pa_stream *s, size_t length, void *userdata) {

    pa_assert(s);
    pa_assert(length > 0);

    if (raw) {
        pa_assert(!sndfile);

        if (stdio_event)
            mainloop_api->io_enable(stdio_event, PA_IO_EVENT_OUTPUT);

        while (pa_stream_readable_size(s) > 0) {
            const void *data;

            if (pa_stream_peek(s, &data, &length) < 0) {
                pa_log(_("pa_stream_peek() failed: %s"), pa_strerror(pa_context_errno(context)));
                quit(1);
                return;
            }

            pa_assert(length > 0);

            /* If there is a hole in the stream, we generate silence, except
             * if it's a passthrough stream in which case we skip the hole. */
            if (data || !(flags & PA_STREAM_PASSTHROUGH)) {
                buffer = pa_xrealloc(buffer, buffer_length + length);
                if (data)
                    memcpy((uint8_t *) buffer + buffer_length, data, length);
                else
                    pa_silence_memory((uint8_t *) buffer + buffer_length, length, &sample_spec);

                buffer_length += length;
            }

            pa_stream_drop(s);
        }

    } else {
        pa_assert(sndfile);

        while (pa_stream_readable_size(s) > 0) {
            sf_count_t bytes;
            const void *data;

            if (pa_stream_peek(s, &data, &length) < 0) {
                pa_log(_("pa_stream_peek() failed: %s"), pa_strerror(pa_context_errno(context)));
                quit(1);
                return;
            }

            pa_assert(length > 0);

            if (!data && (flags & PA_STREAM_PASSTHROUGH)) {
                pa_stream_drop(s);
                continue;
            }

            if (!data && length > silence_buffer_length) {
                silence_buffer = pa_xrealloc(silence_buffer, length);
                pa_silence_memory((uint8_t *) silence_buffer + silence_buffer_length, length - silence_buffer_length, &sample_spec);
                silence_buffer_length = length;
            }

            if (writef_function) {
                size_t k = pa_frame_size(&sample_spec);

                if ((bytes = writef_function(sndfile, data ? data : silence_buffer, (sf_count_t) (length/k))) > 0)
                    bytes *= (sf_count_t) k;

            } else
                bytes = sf_write_raw(sndfile, data ? data : silence_buffer, (sf_count_t) length);

            if (bytes < (sf_count_t) length)
                quit(1);

            pa_stream_drop(s);
        }
    }
}

/* This routine is called whenever the stream state changes */
static void stream_state_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY:

            if (verbose) {
                const pa_buffer_attr *a;
                char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

                pa_log(_("Stream successfully created."));

                if (!(a = pa_stream_get_buffer_attr(s)))
                    pa_log(_("pa_stream_get_buffer_attr() failed: %s"), pa_strerror(pa_context_errno(pa_stream_get_context(s))));
                else {

                    if (mode == PLAYBACK)
                        pa_log(_("Buffer metrics: maxlength=%u, tlength=%u, prebuf=%u, minreq=%u"), a->maxlength, a->tlength, a->prebuf, a->minreq);
                    else {
                        pa_assert(mode == RECORD);
                        pa_log(_("Buffer metrics: maxlength=%u, fragsize=%u"), a->maxlength, a->fragsize);
                    }
                }

                pa_log(_("Using sample spec '%s', channel map '%s'."),
                        pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(s)),
                        pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(s)));

                pa_log(_("Connected to device %s (index: %u, suspended: %s)."),
                        pa_stream_get_device_name(s),
                        pa_stream_get_device_index(s),
                        pa_yes_no(pa_stream_is_suspended(s)));
            }

            break;

        case PA_STREAM_FAILED:
        default:
            pa_log(_("Stream error: %s"), pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            quit(1);
    }
}

static void stream_suspended_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    if (verbose) {
        if (pa_stream_is_suspended(s))
            pa_log(_("Stream device suspended.%s"), CLEAR_LINE);
        else
            pa_log(_("Stream device resumed.%s"), CLEAR_LINE);
    }
}

static void stream_underflow_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    if (verbose)
        pa_log(_("Stream underrun.%s"),  CLEAR_LINE);
}

static void stream_overflow_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    if (verbose)
        pa_log(_("Stream overrun.%s"), CLEAR_LINE);
}

static void stream_started_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    if (verbose)
        pa_log(_("Stream started.%s"), CLEAR_LINE);
}

static void stream_moved_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    if (verbose)
        pa_log(_("Stream moved to device %s (%u, %ssuspended).%s"), pa_stream_get_device_name(s), pa_stream_get_device_index(s), pa_stream_is_suspended(s) ? "" : _("not "),  CLEAR_LINE);
}

static void stream_buffer_attr_callback(pa_stream *s, void *userdata) {
    pa_assert(s);

    if (verbose)
        pa_log(_("Stream buffer attributes changed.%s"),  CLEAR_LINE);
}

static void stream_event_callback(pa_stream *s, const char *name, pa_proplist *pl, void *userdata) {
    char *t;

    pa_assert(s);
    pa_assert(name);
    pa_assert(pl);

    t = pa_proplist_to_string_sep(pl, ", ");
    pa_log("Got event '%s', properties '%s'", name, t);

    if (pa_streq(name, PA_STREAM_EVENT_REQUEST_CORK)) {
        if (cork_requests == 0) {
            pa_log(_("Cork request stack is empty: corking stream"));
            pa_operation_unref(pa_stream_cork(s, 1, NULL, NULL));
        }
        cork_requests++;
    } else if (pa_streq(name, PA_STREAM_EVENT_REQUEST_UNCORK)) {
        if (cork_requests == 1) {
            pa_log(_("Cork request stack is empty: uncorking stream"));
            pa_operation_unref(pa_stream_cork(s, 0, NULL, NULL));
        }
        if (cork_requests == 0)
            pa_log(_("Warning: Received more uncork requests than cork requests!"));
        else
            cork_requests--;
    }

    pa_xfree(t);
}

/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata) {
    pa_assert(c);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            pa_buffer_attr buffer_attr;

            pa_assert(c);
            pa_assert(!stream);

            if (verbose)
                pa_log(_("Connection established.%s"), CLEAR_LINE);

            if (!(stream = pa_stream_new_with_proplist(c, NULL, &sample_spec, &channel_map, proplist))) {
                pa_log(_("pa_stream_new() failed: %s"), pa_strerror(pa_context_errno(c)));
                goto fail;
            }

            pa_stream_set_state_callback(stream, stream_state_callback, NULL);
            pa_stream_set_write_callback(stream, stream_write_callback, NULL);
            pa_stream_set_read_callback(stream, stream_read_callback, NULL);
            pa_stream_set_suspended_callback(stream, stream_suspended_callback, NULL);
            pa_stream_set_moved_callback(stream, stream_moved_callback, NULL);
            pa_stream_set_underflow_callback(stream, stream_underflow_callback, NULL);
            pa_stream_set_overflow_callback(stream, stream_overflow_callback, NULL);
            pa_stream_set_started_callback(stream, stream_started_callback, NULL);
            pa_stream_set_event_callback(stream, stream_event_callback, NULL);
            pa_stream_set_buffer_attr_callback(stream, stream_buffer_attr_callback, NULL);

            pa_zero(buffer_attr);
            buffer_attr.maxlength = (uint32_t) -1;
            buffer_attr.prebuf = (uint32_t) -1;

            if (latency_msec > 0) {
                buffer_attr.fragsize = buffer_attr.tlength = pa_usec_to_bytes(latency_msec * PA_USEC_PER_MSEC, &sample_spec);
                flags |= PA_STREAM_ADJUST_LATENCY;
            } else if (latency > 0) {
                buffer_attr.fragsize = buffer_attr.tlength = (uint32_t) latency;
                flags |= PA_STREAM_ADJUST_LATENCY;
            } else
                buffer_attr.fragsize = buffer_attr.tlength = (uint32_t) -1;

            if (process_time_msec > 0) {
                buffer_attr.minreq = pa_usec_to_bytes(process_time_msec * PA_USEC_PER_MSEC, &sample_spec);
            } else if (process_time > 0)
                buffer_attr.minreq = (uint32_t) process_time;
            else
                buffer_attr.minreq = (uint32_t) -1;

            if (mode == PLAYBACK) {
                pa_cvolume cv;
                if (pa_stream_connect_playback(stream, device, &buffer_attr, flags, volume_is_set ? pa_cvolume_set(&cv, sample_spec.channels, volume) : NULL, NULL) < 0) {
                    pa_log(_("pa_stream_connect_playback() failed: %s"), pa_strerror(pa_context_errno(c)));
                    goto fail;
                }

            } else {
                if (monitor_stream != PA_INVALID_INDEX && (pa_stream_set_monitor_stream(stream, monitor_stream) < 0)) {
                    pa_log(_("Failed to set monitor stream: %s"), pa_strerror(pa_context_errno(c)));
                    goto fail;
                }
                if (pa_stream_connect_record(stream, device, &buffer_attr, flags) < 0) {
                    pa_log(_("pa_stream_connect_record() failed: %s"), pa_strerror(pa_context_errno(c)));
                    goto fail;
                }
            }
            break;
        }

        case PA_CONTEXT_TERMINATED:
            quit(0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            pa_log(_("Connection failure: %s"), pa_strerror(pa_context_errno(c)));
            goto fail;
    }

    return;

fail:
    quit(1);

}

/* New data on STDIN **/
static void stdin_callback(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t f, void *userdata) {
    size_t l, w = 0;
    ssize_t r;

    pa_assert(a == mainloop_api);
    pa_assert(e);
    pa_assert(stdio_event == e);

    if (buffer) {
        mainloop_api->io_enable(stdio_event, PA_IO_EVENT_NULL);
        return;
    }

    if (!stream || pa_stream_get_state(stream) != PA_STREAM_READY || !(l = w = pa_stream_writable_size(stream)))
        l = 4096;

    buffer = pa_xmalloc(l);

    if ((r = pa_read(fd, buffer, l, userdata)) <= 0) {
        if (r == 0) {
            if (verbose)
                pa_log(_("Got EOF."));

            start_drain();

        } else {
            pa_log(_("read() failed: %s"), strerror(errno));
            quit(1);
        }

        mainloop_api->io_free(stdio_event);
        stdio_event = NULL;
        return;
    }

    buffer_length = (uint32_t) r;
    buffer_index = 0;

    if (w)
        do_stream_write(w);
}

/* Some data may be written to STDOUT */
static void stdout_callback(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t f, void *userdata) {
    ssize_t r;

    pa_assert(a == mainloop_api);
    pa_assert(e);
    pa_assert(stdio_event == e);

    if (!buffer) {
        mainloop_api->io_enable(stdio_event, PA_IO_EVENT_NULL);
        return;
    }

    pa_assert(buffer_length);

    if ((r = pa_write(fd, (uint8_t*) buffer+buffer_index, buffer_length, userdata)) <= 0) {
        pa_log(_("write() failed: %s"), strerror(errno));
        quit(1);

        mainloop_api->io_free(stdio_event);
        stdio_event = NULL;
        return;
    }

    buffer_length -= (uint32_t) r;
    buffer_index += (uint32_t) r;

    if (!buffer_length) {
        pa_xfree(buffer);
        buffer = NULL;
        buffer_length = buffer_index = 0;
    }
}

/* UNIX signal to quit received */
static void exit_signal_callback(pa_mainloop_api*m, pa_signal_event *e, int sig, void *userdata) {
    if (verbose)
        pa_log(_("Got signal, exiting."));
    quit(0);
}

/* Show the current latency */
static void stream_update_timing_callback(pa_stream *s, int success, void *userdata) {
    pa_usec_t l, usec;
    int negative = 0;

    pa_assert(s);

    if (!success ||
        pa_stream_get_time(s, &usec) < 0 ||
        pa_stream_get_latency(s, &l, &negative) < 0) {
        pa_log(_("Failed to get latency: %s"), pa_strerror(pa_context_errno(context)));
        quit(1);
        return;
    }

    fprintf(stderr, _("Time: %0.3f sec; Latency: %0.0f usec."),
            (float) usec / 1000000,
            (float) l * (negative?-1.0f:1.0f));
    fprintf(stderr, "        \r");
}

#ifdef SIGUSR1
/* Someone requested that the latency is shown */
static void sigusr1_signal_callback(pa_mainloop_api*m, pa_signal_event *e, int sig, void *userdata) {

    if (!stream)
        return;

    pa_operation_unref(pa_stream_update_timing_info(stream, stream_update_timing_callback, NULL));
}
#endif

static void time_event_callback(pa_mainloop_api *m, pa_time_event *e, const struct timeval *t, void *userdata) {
    if (stream && pa_stream_get_state(stream) == PA_STREAM_READY) {
        pa_operation *o;
        if (!(o = pa_stream_update_timing_info(stream, stream_update_timing_callback, NULL)))
            pa_log(_("pa_stream_update_timing_info() failed: %s"), pa_strerror(pa_context_errno(context)));
        else
            pa_operation_unref(o);
    }

    pa_context_rttime_restart(context, e, pa_rtclock_now() + TIME_EVENT_USEC);
}

static void help(const char *argv0) {

    printf(_("%s [options]\n\n"
             "  -h, --help                            Show this help\n"
             "      --version                         Show version\n\n"
             "  -r, --record                          Create a connection for recording\n"
             "  -p, --playback                        Create a connection for playback\n\n"
             "  -v, --verbose                         Enable verbose operations\n\n"
             "  -s, --server=SERVER                   The name of the server to connect to\n"
             "  -d, --device=DEVICE                   The name of the sink/source to connect to\n"
             "  -n, --client-name=NAME                How to call this client on the server\n"
             "      --stream-name=NAME                How to call this stream on the server\n"
             "      --volume=VOLUME                   Specify the initial (linear) volume in range 0...65536\n"
             "      --rate=SAMPLERATE                 The sample rate in Hz (defaults to 44100)\n"
             "      --format=SAMPLEFORMAT             The sample type, one of s16le, s16be, u8, float32le,\n"
             "                                        float32be, ulaw, alaw, s32le, s32be, s24le, s24be,\n"
             "                                        s24-32le, s24-32be (defaults to s16ne)\n"
             "      --channels=CHANNELS               The number of channels, 1 for mono, 2 for stereo\n"
             "                                        (defaults to 2)\n"
             "      --channel-map=CHANNELMAP          Channel map to use instead of the default\n"
             "      --fix-format                      Take the sample format from the sink/source the stream is\n"
             "                                        being connected to.\n"
             "      --fix-rate                        Take the sampling rate from the sink/source the stream is\n"
             "                                        being connected to.\n"
             "      --fix-channels                    Take the number of channels and the channel map\n"
             "                                        from the sink/source the stream is being connected to.\n"
             "      --no-remix                        Don't upmix or downmix channels.\n"
             "      --no-remap                        Map channels by index instead of name.\n"
             "      --latency=BYTES                   Request the specified latency in bytes.\n"
             "      --process-time=BYTES              Request the specified process time per request in bytes.\n"
             "      --latency-msec=MSEC               Request the specified latency in msec.\n"
             "      --process-time-msec=MSEC          Request the specified process time per request in msec.\n"
             "      --property=PROPERTY=VALUE         Set the specified property to the specified value.\n"
             "      --raw                             Record/play raw PCM data.\n"
             "      --passthrough                     Passthrough data.\n"
             "      --file-format[=FFORMAT]           Record/play formatted PCM data.\n"
             "      --list-file-formats               List available file formats.\n"
             "      --monitor-stream=INDEX            Record from the sink input with index INDEX.\n")
           , argv0);
}

enum {
    ARG_VERSION = 256,
    ARG_STREAM_NAME,
    ARG_VOLUME,
    ARG_SAMPLERATE,
    ARG_SAMPLEFORMAT,
    ARG_CHANNELS,
    ARG_CHANNELMAP,
    ARG_FIX_FORMAT,
    ARG_FIX_RATE,
    ARG_FIX_CHANNELS,
    ARG_NO_REMAP,
    ARG_NO_REMIX,
    ARG_LATENCY,
    ARG_PROCESS_TIME,
    ARG_RAW,
    ARG_PASSTHROUGH,
    ARG_PROPERTY,
    ARG_FILE_FORMAT,
    ARG_LIST_FILE_FORMATS,
    ARG_LATENCY_MSEC,
    ARG_PROCESS_TIME_MSEC,
    ARG_MONITOR_STREAM,
};

int main(int argc, char *argv[]) {
    pa_mainloop* m = NULL;
    int ret = 1, c;
    char *bn, *server = NULL;
    pa_time_event *time_event = NULL;
    const char *filename = NULL;
    /* type for pa_read/_write. passed as userdata to the callbacks */
    unsigned long type = 0;

    static const struct option long_options[] = {
        {"record",       0, NULL, 'r'},
        {"playback",     0, NULL, 'p'},
        {"device",       1, NULL, 'd'},
        {"server",       1, NULL, 's'},
        {"client-name",  1, NULL, 'n'},
        {"stream-name",  1, NULL, ARG_STREAM_NAME},
        {"version",      0, NULL, ARG_VERSION},
        {"help",         0, NULL, 'h'},
        {"verbose",      0, NULL, 'v'},
        {"volume",       1, NULL, ARG_VOLUME},
        {"rate",         1, NULL, ARG_SAMPLERATE},
        {"format",       1, NULL, ARG_SAMPLEFORMAT},
        {"channels",     1, NULL, ARG_CHANNELS},
        {"channel-map",  1, NULL, ARG_CHANNELMAP},
        {"fix-format",   0, NULL, ARG_FIX_FORMAT},
        {"fix-rate",     0, NULL, ARG_FIX_RATE},
        {"fix-channels", 0, NULL, ARG_FIX_CHANNELS},
        {"no-remap",     0, NULL, ARG_NO_REMAP},
        {"no-remix",     0, NULL, ARG_NO_REMIX},
        {"latency",      1, NULL, ARG_LATENCY},
        {"process-time", 1, NULL, ARG_PROCESS_TIME},
        {"property",     1, NULL, ARG_PROPERTY},
        {"raw",          0, NULL, ARG_RAW},
        {"passthrough",  0, NULL, ARG_PASSTHROUGH},
        {"file-format",  2, NULL, ARG_FILE_FORMAT},
        {"list-file-formats", 0, NULL, ARG_LIST_FILE_FORMATS},
        {"latency-msec", 1, NULL, ARG_LATENCY_MSEC},
        {"process-time-msec", 1, NULL, ARG_PROCESS_TIME_MSEC},
        {"monitor-stream", 1, NULL, ARG_MONITOR_STREAM},
        {NULL,           0, NULL, 0}
    };

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PULSE_LOCALEDIR);
#endif

    bn = pa_path_get_filename(argv[0]);

    if (strstr(bn, "play")) {
        mode = PLAYBACK;
        raw = false;
    } else if (strstr(bn, "record")) {
        mode = RECORD;
        raw = false;
    } else if (strstr(bn, "cat")) {
        mode = PLAYBACK;
        raw = true;
    } else if (strstr(bn, "rec") || strstr(bn, "mon")) {
        mode = RECORD;
        raw = true;
    }

    proplist = pa_proplist_new();

    while ((c = getopt_long(argc, argv, "rpd:s:n:hv", long_options, NULL)) != -1) {

        switch (c) {
            case 'h' :
                help(bn);
                ret = 0;
                goto quit;

            case ARG_VERSION:
                printf(_("pacat %s\n"
                         "Compiled with libpulse %s\n"
                         "Linked with libpulse %s\n"),
                       PACKAGE_VERSION,
                       pa_get_headers_version(),
                       pa_get_library_version());
                ret = 0;
                goto quit;

            case 'r':
                mode = RECORD;
                break;

            case 'p':
                mode = PLAYBACK;
                break;

            case 'd':
                pa_xfree(device);
                device = pa_xstrdup(optarg);
                break;

            case 's':
                pa_xfree(server);
                server = pa_xstrdup(optarg);
                break;

            case 'n': {
                char *t;

                if (!(t = pa_locale_to_utf8(optarg)) ||
                    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, t) < 0) {

                    pa_log(_("Invalid client name '%s'"), t ? t : optarg);
                    pa_xfree(t);
                    goto quit;
                }

                pa_xfree(t);
                break;
            }

            case ARG_STREAM_NAME: {
                char *t;

                if (!(t = pa_locale_to_utf8(optarg)) ||
                    pa_proplist_sets(proplist, PA_PROP_MEDIA_NAME, t) < 0) {

                    pa_log(_("Invalid stream name '%s'"), t ? t : optarg);
                    pa_xfree(t);
                    goto quit;
                }

                pa_xfree(t);
                break;
            }

            case 'v':
                verbose = 1;
                break;

            case ARG_VOLUME: {
                int v = atoi(optarg);
                volume = v < 0 ? 0U : (pa_volume_t) v;
                volume_is_set = true;
                break;
            }

            case ARG_CHANNELS:
                sample_spec.channels = (uint8_t) atoi(optarg);
                sample_spec_set = true;
                break;

            case ARG_SAMPLEFORMAT:
                sample_spec.format = pa_parse_sample_format(optarg);
                sample_spec_set = true;
                break;

            case ARG_SAMPLERATE:
                sample_spec.rate = (uint32_t) atoi(optarg);
                sample_spec_set = true;
                break;

            case ARG_CHANNELMAP:
                if (!pa_channel_map_parse(&channel_map, optarg)) {
                    pa_log(_("Invalid channel map '%s'"), optarg);
                    goto quit;
                }

                channel_map_set = true;
                break;

            case ARG_FIX_CHANNELS:
                flags |= PA_STREAM_FIX_CHANNELS;
                break;

            case ARG_FIX_RATE:
                flags |= PA_STREAM_FIX_RATE;
                break;

            case ARG_FIX_FORMAT:
                flags |= PA_STREAM_FIX_FORMAT;
                break;

            case ARG_NO_REMIX:
                flags |= PA_STREAM_NO_REMIX_CHANNELS;
                break;

            case ARG_NO_REMAP:
                flags |= PA_STREAM_NO_REMAP_CHANNELS;
                break;

            case ARG_LATENCY:
                if (((latency = (size_t) atoi(optarg))) <= 0) {
                    pa_log(_("Invalid latency specification '%s'"), optarg);
                    goto quit;
                }
                break;

            case ARG_PROCESS_TIME:
                if (((process_time = (size_t) atoi(optarg))) <= 0) {
                    pa_log(_("Invalid process time specification '%s'"), optarg);
                    goto quit;
                }
                break;

            case ARG_LATENCY_MSEC:
                if (((latency_msec = (int32_t) atoi(optarg))) <= 0) {
                    pa_log(_("Invalid latency specification '%s'"), optarg);
                    goto quit;
                }
                break;

            case ARG_PROCESS_TIME_MSEC:
                if (((process_time_msec = (int32_t) atoi(optarg))) <= 0) {
                    pa_log(_("Invalid process time specification '%s'"), optarg);
                    goto quit;
                }
                break;

            case ARG_PROPERTY: {
                char *t;

                if (!(t = pa_locale_to_utf8(optarg)) ||
                    pa_proplist_setp(proplist, t) < 0) {

                    pa_xfree(t);
                    pa_log(_("Invalid property '%s'"), optarg);
                    goto quit;
                }

                pa_xfree(t);
                break;
            }

            case ARG_RAW:
                raw = true;
                break;

            case ARG_PASSTHROUGH:
                flags |= PA_STREAM_PASSTHROUGH;
                break;

            case ARG_FILE_FORMAT:
                if (optarg) {
                    if ((file_format = pa_sndfile_format_from_string(optarg)) < 0) {
                        pa_log(_("Unknown file format %s."), optarg);
                        goto quit;
                    }
                }

                raw = false;
                break;

            case ARG_LIST_FILE_FORMATS:
                pa_sndfile_dump_formats();
                ret = 0;
                goto quit;

            case ARG_MONITOR_STREAM:
                if (pa_atou(optarg, &monitor_stream) < 0) {
                    pa_log(_("Failed to parse the argument for --monitor-stream"));
                    goto quit;
                }
                break;

            default:
                goto quit;
        }
    }

    if (!pa_sample_spec_valid(&sample_spec)) {
        pa_log(_("Invalid sample specification"));
        goto quit;
    }

    if (optind+1 == argc) {
        int fd;

        filename = argv[optind];

        if ((fd = pa_open_cloexec(argv[optind], mode == PLAYBACK ? O_RDONLY : O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0) {
            pa_log(_("open(): %s"), strerror(errno));
            goto quit;
        }

        if (dup2(fd, mode == PLAYBACK ? STDIN_FILENO : STDOUT_FILENO) < 0) {
            pa_log(_("dup2(): %s"), strerror(errno));
            goto quit;
        }

        pa_close(fd);

    } else if (optind+1 <= argc) {
        pa_log(_("Too many arguments."));
        goto quit;
    }

    if (!raw) {
        SF_INFO sfi;
        pa_zero(sfi);

        if (mode == RECORD) {
            /* This might patch up the sample spec */
            if (pa_sndfile_write_sample_spec(&sfi, &sample_spec) < 0) {
                pa_log(_("Failed to generate sample specification for file."));
                goto quit;
            }

            if (file_format <= 0) {
                char *extension;
                if (filename && (extension = strrchr(filename, '.')))
                    file_format = pa_sndfile_format_from_string(extension+1);
                if (file_format <= 0)
                    file_format = SF_FORMAT_WAV;
                /* Transparently upgrade classic .wav to wavex for multichannel audio */
                if (file_format == SF_FORMAT_WAV &&
                    (sample_spec.channels > 2 ||
                    (channel_map_set &&
                    !(sample_spec.channels == 1 && channel_map.map[0] == PA_CHANNEL_POSITION_MONO) &&
                    !(sample_spec.channels == 2 && channel_map.map[0] == PA_CHANNEL_POSITION_LEFT
                                                && channel_map.map[1] == PA_CHANNEL_POSITION_RIGHT))))
                    file_format = SF_FORMAT_WAVEX;
            }

            sfi.format |= file_format;
        }

        if (!(sndfile = sf_open_fd(mode == RECORD ? STDOUT_FILENO : STDIN_FILENO,
                                   mode == RECORD ? SFM_WRITE : SFM_READ,
                                   &sfi, 0))) {
            pa_log(_("Failed to open audio file."));
            goto quit;
        }

        if (mode == PLAYBACK) {
            if (sample_spec_set)
                pa_log(_("Warning: specified sample specification will be overwritten with specification from file."));

            if (pa_sndfile_read_sample_spec(sndfile, &sample_spec) < 0) {
                pa_log(_("Failed to determine sample specification from file."));
                goto quit;
            }
            sample_spec_set = true;

            if (!channel_map_set) {
                /* Allow the user to overwrite the channel map on the command line */
                if (pa_sndfile_read_channel_map(sndfile, &channel_map) < 0) {
                    if (sample_spec.channels > 2)
                        pa_log(_("Warning: Failed to determine channel map from file."));
                } else
                    channel_map_set = true;
            }
        }
    }

    if (!channel_map_set)
        pa_channel_map_init_extend(&channel_map, sample_spec.channels, PA_CHANNEL_MAP_DEFAULT);

    if (!pa_channel_map_compatible(&channel_map, &sample_spec)) {
        pa_log(_("Channel map doesn't match sample specification"));
        goto quit;
    }

    if (!raw) {
        pa_proplist *sfp;

        if (mode == PLAYBACK)
            readf_function = pa_sndfile_readf_function(&sample_spec);
        else {
            if (pa_sndfile_write_channel_map(sndfile, &channel_map) < 0)
                pa_log(_("Warning: failed to write channel map to file."));

            writef_function = pa_sndfile_writef_function(&sample_spec);
        }

        /* Fill in libsndfile prop list data */
        sfp = pa_proplist_new();
        pa_sndfile_init_proplist(sndfile, sfp);
        pa_proplist_update(proplist, PA_UPDATE_MERGE, sfp);
        pa_proplist_free(sfp);
    }

    if (verbose) {
        char tss[PA_SAMPLE_SPEC_SNPRINT_MAX], tcm[PA_CHANNEL_MAP_SNPRINT_MAX];

        pa_log(_("Opening a %s stream with sample specification '%s' and channel map '%s'."),
                mode == RECORD ? _("recording") : _("playback"),
                pa_sample_spec_snprint(tss, sizeof(tss), &sample_spec),
                pa_channel_map_snprint(tcm, sizeof(tcm), &channel_map));
    }

    /* Fill in client name if none was set */
    if (!pa_proplist_contains(proplist, PA_PROP_APPLICATION_NAME)) {
        char *t;

        if ((t = pa_locale_to_utf8(bn))) {
            pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, t);
            pa_xfree(t);
        }
    }

    /* Fill in media name if none was set */
    if (!pa_proplist_contains(proplist, PA_PROP_MEDIA_NAME)) {
        const char *t;

        if ((t = filename) ||
            (t = pa_proplist_gets(proplist, PA_PROP_APPLICATION_NAME)))
            pa_proplist_sets(proplist, PA_PROP_MEDIA_NAME, t);

        if (!pa_proplist_contains(proplist, PA_PROP_MEDIA_NAME)) {
            pa_log(_("Failed to set media name."));
            goto quit;
        }
    }

    /* Set up a new main loop */
    if (!(m = pa_mainloop_new())) {
        pa_log(_("pa_mainloop_new() failed."));
        goto quit;
    }

    mainloop_api = pa_mainloop_get_api(m);

    pa_assert_se(pa_signal_init(mainloop_api) == 0);
    pa_signal_new(SIGINT, exit_signal_callback, NULL);
    pa_signal_new(SIGTERM, exit_signal_callback, NULL);
#ifdef SIGUSR1
    pa_signal_new(SIGUSR1, sigusr1_signal_callback, NULL);
#endif
    pa_disable_sigpipe();

    if (raw) {
#ifdef OS_IS_WIN32
        /* need to turn on binary mode for stdio io. Windows, meh */
        setmode(mode == PLAYBACK ? STDIN_FILENO : STDOUT_FILENO, O_BINARY);
#endif
        if (!(stdio_event = mainloop_api->io_new(mainloop_api,
                                                 mode == PLAYBACK ? STDIN_FILENO : STDOUT_FILENO,
                                                 mode == PLAYBACK ? PA_IO_EVENT_INPUT : PA_IO_EVENT_OUTPUT,
                                                 mode == PLAYBACK ? stdin_callback : stdout_callback, &type))) {
            pa_log(_("io_new() failed."));
            goto quit;
        }
    }

    /* Create a new connection context */
    if (!(context = pa_context_new_with_proplist(mainloop_api, NULL, proplist))) {
        pa_log(_("pa_context_new() failed."));
        goto quit;
    }

    pa_context_set_state_callback(context, context_state_callback, NULL);

    /* Connect the context */
    if (pa_context_connect(context, server, 0, NULL) < 0) {
        pa_log(_("pa_context_connect() failed: %s"), pa_strerror(pa_context_errno(context)));
        goto quit;
    }

    if (verbose) {
        if (!(time_event = pa_context_rttime_new(context, pa_rtclock_now() + TIME_EVENT_USEC, time_event_callback, NULL))) {
            pa_log(_("pa_context_rttime_new() failed."));
            goto quit;
        }
    }

    /* Run the main loop */
    if (pa_mainloop_run(m, &ret) < 0) {
        pa_log(_("pa_mainloop_run() failed."));
        goto quit;
    }

quit:
    if (stream)
        pa_stream_unref(stream);

    if (context)
        pa_context_unref(context);

    if (stdio_event) {
        pa_assert(mainloop_api);
        mainloop_api->io_free(stdio_event);
    }

    if (time_event) {
        pa_assert(mainloop_api);
        mainloop_api->time_free(time_event);
    }

    if (m) {
        pa_signal_done();
        pa_mainloop_free(m);
    }

    pa_xfree(silence_buffer);
    pa_xfree(buffer);

    pa_xfree(server);
    pa_xfree(device);

    if (sndfile)
        sf_close(sndfile);

    if (proplist)
        pa_proplist_free(proplist);

    return ret;
}

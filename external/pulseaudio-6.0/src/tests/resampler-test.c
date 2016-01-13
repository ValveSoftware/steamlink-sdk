/***
  This file is part of PulseAudio.

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
#include <getopt.h>
#include <locale.h>

#include <pulse/pulseaudio.h>

#include <pulse/rtclock.h>
#include <pulse/sample.h>
#include <pulse/volume.h>

#include <pulsecore/i18n.h>
#include <pulsecore/log.h>
#include <pulsecore/resampler.h>
#include <pulsecore/macro.h>
#include <pulsecore/endianmacros.h>
#include <pulsecore/memblock.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/core-util.h>

static void dump_block(const char *label, const pa_sample_spec *ss, const pa_memchunk *chunk) {
    void *d;
    unsigned i;

    if (getenv("MAKE_CHECK"))
        return;
    printf("%s:  \t", label);

    d = pa_memblock_acquire(chunk->memblock);

    switch (ss->format) {

        case PA_SAMPLE_U8:
        case PA_SAMPLE_ULAW:
        case PA_SAMPLE_ALAW: {
            uint8_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++)
                printf("      0x%02x ", *(u++));

            break;
        }

        case PA_SAMPLE_S16NE:
        case PA_SAMPLE_S16RE: {
            uint16_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++)
                printf("    0x%04x ", *(u++));

            break;
        }

        case PA_SAMPLE_S32NE:
        case PA_SAMPLE_S32RE: {
            uint32_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++)
                printf("0x%08x ", *(u++));

            break;
        }

        case PA_SAMPLE_S24_32NE:
        case PA_SAMPLE_S24_32RE: {
            uint32_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++)
                printf("0x%08x ", *(u++));

            break;
        }

        case PA_SAMPLE_FLOAT32NE:
        case PA_SAMPLE_FLOAT32RE: {
            float *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                printf("%4.3g ", ss->format == PA_SAMPLE_FLOAT32NE ? *u : PA_READ_FLOAT32RE(u));
                u++;
            }

            break;
        }

        case PA_SAMPLE_S24LE:
        case PA_SAMPLE_S24BE: {
            uint8_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                printf("  0x%06x ", PA_READ24NE(u));
                u += pa_frame_size(ss);
            }

            break;
        }

        default:
            pa_assert_not_reached();
    }

    printf("\n");

    pa_memblock_release(chunk->memblock);
}

static pa_memblock* generate_block(pa_mempool *pool, const pa_sample_spec *ss) {
    pa_memblock *r;
    void *d;
    unsigned i;

    pa_assert_se(r = pa_memblock_new(pool, pa_frame_size(ss) * 10));
    d = pa_memblock_acquire(r);

    switch (ss->format) {

        case PA_SAMPLE_U8:
        case PA_SAMPLE_ULAW:
        case PA_SAMPLE_ALAW: {
            uint8_t *u = d;

            u[0] = 0x00;
            u[1] = 0xFF;
            u[2] = 0x7F;
            u[3] = 0x80;
            u[4] = 0x9f;
            u[5] = 0x3f;
            u[6] = 0x1;
            u[7] = 0xF0;
            u[8] = 0x20;
            u[9] = 0x21;
            break;
        }

        case PA_SAMPLE_S16NE:
        case PA_SAMPLE_S16RE: {
            uint16_t *u = d;

            u[0] = 0x0000;
            u[1] = 0xFFFF;
            u[2] = 0x7FFF;
            u[3] = 0x8000;
            u[4] = 0x9fff;
            u[5] = 0x3fff;
            u[6] = 0x1;
            u[7] = 0xF000;
            u[8] = 0x20;
            u[9] = 0x21;
            break;
        }

        case PA_SAMPLE_S32NE:
        case PA_SAMPLE_S32RE: {
            uint32_t *u = d;

            u[0] = 0x00000001;
            u[1] = 0xFFFF0002;
            u[2] = 0x7FFF0003;
            u[3] = 0x80000004;
            u[4] = 0x9fff0005;
            u[5] = 0x3fff0006;
            u[6] =    0x10007;
            u[7] = 0xF0000008;
            u[8] =   0x200009;
            u[9] =   0x21000A;
            break;
        }

        case PA_SAMPLE_S24_32NE:
        case PA_SAMPLE_S24_32RE: {
            uint32_t *u = d;

            u[0] = 0x000001;
            u[1] = 0xFF0002;
            u[2] = 0x7F0003;
            u[3] = 0x800004;
            u[4] = 0x9f0005;
            u[5] = 0x3f0006;
            u[6] =    0x107;
            u[7] = 0xF00008;
            u[8] =   0x2009;
            u[9] =   0x210A;
            break;
        }

        case PA_SAMPLE_FLOAT32NE:
        case PA_SAMPLE_FLOAT32RE: {
            float *u = d;

            u[0] = 0.0f;
            u[1] = -1.0f;
            u[2] = 1.0f;
            u[3] = 4711.0f;
            u[4] = 0.222f;
            u[5] = 0.33f;
            u[6] = -.3f;
            u[7] = 99.0f;
            u[8] = -0.555f;
            u[9] = -.123f;

            if (ss->format == PA_SAMPLE_FLOAT32RE)
                for (i = 0; i < 10; i++)
                    PA_WRITE_FLOAT32RE(&u[i], u[i]);

            break;
        }

        case PA_SAMPLE_S24NE:
        case PA_SAMPLE_S24RE: {
            uint8_t *u = d;

            PA_WRITE24NE(u,    0x000001);
            PA_WRITE24NE(u+3,  0xFF0002);
            PA_WRITE24NE(u+6,  0x7F0003);
            PA_WRITE24NE(u+9,  0x800004);
            PA_WRITE24NE(u+12, 0x9f0005);
            PA_WRITE24NE(u+15, 0x3f0006);
            PA_WRITE24NE(u+18,    0x107);
            PA_WRITE24NE(u+21, 0xF00008);
            PA_WRITE24NE(u+24,   0x2009);
            PA_WRITE24NE(u+27,   0x210A);
            break;
        }

        default:
            pa_assert_not_reached();
    }

    pa_memblock_release(r);

    return r;
}

static void help(const char *argv0) {
    printf(_("%s [options]\n\n"
             "-h, --help                            Show this help\n"
             "-v, --verbose                         Print debug messages\n"
             "      --from-rate=SAMPLERATE          From sample rate in Hz (defaults to 44100)\n"
             "      --from-format=SAMPLEFORMAT      From sample type (defaults to s16le)\n"
             "      --from-channels=CHANNELS        From number of channels (defaults to 1)\n"
             "      --to-rate=SAMPLERATE            To sample rate in Hz (defaults to 44100)\n"
             "      --to-format=SAMPLEFORMAT        To sample type (defaults to s16le)\n"
             "      --to-channels=CHANNELS          To number of channels (defaults to 1)\n"
             "      --resample-method=METHOD        Resample method (defaults to auto)\n"
             "      --seconds=SECONDS               From stream duration (defaults to 60)\n"
             "\n"
             "If the formats are not specified, the test performs all formats combinations,\n"
             "back and forth.\n"
             "\n"
             "Sample type must be one of s16le, s16be, u8, float32le, float32be, ulaw, alaw,\n"
             "s24le, s24be, s24-32le, s24-32be, s32le, s32be (defaults to s16ne)\n"
             "\n"
             "See --dump-resample-methods for possible values of resample methods.\n"),
             argv0);
}

enum {
    ARG_VERSION = 256,
    ARG_FROM_SAMPLERATE,
    ARG_FROM_SAMPLEFORMAT,
    ARG_FROM_CHANNELS,
    ARG_TO_SAMPLERATE,
    ARG_TO_SAMPLEFORMAT,
    ARG_TO_CHANNELS,
    ARG_SECONDS,
    ARG_RESAMPLE_METHOD,
    ARG_DUMP_RESAMPLE_METHODS
};

static void dump_resample_methods(void) {
    int i;

    for (i = 0; i < PA_RESAMPLER_MAX; i++)
        if (pa_resample_method_supported(i))
            printf("%s\n", pa_resample_method_to_string(i));

}

int main(int argc, char *argv[]) {
    pa_mempool *pool = NULL;
    pa_sample_spec a, b;
    int ret = 1, c;
    bool all_formats = true;
    pa_resample_method_t method;
    int seconds;

    static const struct option long_options[] = {
        {"help",                  0, NULL, 'h'},
        {"verbose",               0, NULL, 'v'},
        {"version",               0, NULL, ARG_VERSION},
        {"from-rate",             1, NULL, ARG_FROM_SAMPLERATE},
        {"from-format",           1, NULL, ARG_FROM_SAMPLEFORMAT},
        {"from-channels",         1, NULL, ARG_FROM_CHANNELS},
        {"to-rate",               1, NULL, ARG_TO_SAMPLERATE},
        {"to-format",             1, NULL, ARG_TO_SAMPLEFORMAT},
        {"to-channels",           1, NULL, ARG_TO_CHANNELS},
        {"seconds",               1, NULL, ARG_SECONDS},
        {"resample-method",       1, NULL, ARG_RESAMPLE_METHOD},
        {"dump-resample-methods", 0, NULL, ARG_DUMP_RESAMPLE_METHODS},
        {NULL,                    0, NULL, 0}
    };

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PULSE_LOCALEDIR);
#endif

    pa_log_set_level(PA_LOG_WARN);
    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_INFO);

    pa_assert_se(pool = pa_mempool_new(false, 0));

    a.channels = b.channels = 1;
    a.rate = b.rate = 44100;
    a.format = b.format = PA_SAMPLE_S16LE;

    method = PA_RESAMPLER_AUTO;
    seconds = 60;

    while ((c = getopt_long(argc, argv, "hv", long_options, NULL)) != -1) {

        switch (c) {
            case 'h' :
                help(argv[0]);
                ret = 0;
                goto quit;

            case 'v':
                pa_log_set_level(PA_LOG_DEBUG);
                break;

            case ARG_VERSION:
                printf(_("%s %s\n"), argv[0], PACKAGE_VERSION);
                ret = 0;
                goto quit;

            case ARG_DUMP_RESAMPLE_METHODS:
                dump_resample_methods();
                ret = 0;
                goto quit;

            case ARG_FROM_CHANNELS:
                a.channels = (uint8_t) atoi(optarg);
                break;

            case ARG_FROM_SAMPLEFORMAT:
                a.format = pa_parse_sample_format(optarg);
                all_formats = false;
                break;

            case ARG_FROM_SAMPLERATE:
                a.rate = (uint32_t) atoi(optarg);
                break;

            case ARG_TO_CHANNELS:
                b.channels = (uint8_t) atoi(optarg);
                break;

            case ARG_TO_SAMPLEFORMAT:
                b.format = pa_parse_sample_format(optarg);
                all_formats = false;
                break;

            case ARG_TO_SAMPLERATE:
                b.rate = (uint32_t) atoi(optarg);
                break;

            case ARG_SECONDS:
                seconds = atoi(optarg);
                break;

            case ARG_RESAMPLE_METHOD:
                if (*optarg == '\0' || pa_streq(optarg, "help")) {
                    dump_resample_methods();
                    ret = 0;
                    goto quit;
                }
                method = pa_parse_resample_method(optarg);
                break;

            default:
                goto quit;
        }
    }

    ret = 0;
    pa_assert_se(pool = pa_mempool_new(false, 0));

    if (!all_formats) {

        pa_resampler *resampler;
        pa_memchunk i, j;
        pa_usec_t ts;

        pa_log_debug("Compilation CFLAGS: %s", PA_CFLAGS);
        pa_log_debug("=== %d seconds: %d Hz %d ch (%s) -> %d Hz %d ch (%s)", seconds,
                   a.rate, a.channels, pa_sample_format_to_string(a.format),
                   b.rate, b.channels, pa_sample_format_to_string(b.format));

        ts = pa_rtclock_now();
        pa_assert_se(resampler = pa_resampler_new(pool, &a, NULL, &b, NULL, method, 0));
        pa_log_info("init: %llu", (long long unsigned)(pa_rtclock_now() - ts));

        i.memblock = pa_memblock_new(pool, pa_usec_to_bytes(1*PA_USEC_PER_SEC, &a));

        ts = pa_rtclock_now();
        i.length = pa_memblock_get_length(i.memblock);
        i.index = 0;
        while (seconds--) {
            pa_resampler_run(resampler, &i, &j);
            if (j.memblock)
                pa_memblock_unref(j.memblock);
        }
        pa_log_info("resampling: %llu", (long long unsigned)(pa_rtclock_now() - ts));
        pa_memblock_unref(i.memblock);

        pa_resampler_free(resampler);

        goto quit;
    }

    for (a.format = 0; a.format < PA_SAMPLE_MAX; a.format ++) {
        for (b.format = 0; b.format < PA_SAMPLE_MAX; b.format ++) {
            pa_resampler *forth, *back;
            pa_memchunk i, j, k;

            pa_log_debug("=== %s -> %s -> %s -> /2",
                       pa_sample_format_to_string(a.format),
                       pa_sample_format_to_string(b.format),
                       pa_sample_format_to_string(a.format));

            pa_assert_se(forth = pa_resampler_new(pool, &a, NULL, &b, NULL, method, 0));
            pa_assert_se(back = pa_resampler_new(pool, &b, NULL, &a, NULL, method, 0));

            i.memblock = generate_block(pool, &a);
            i.length = pa_memblock_get_length(i.memblock);
            i.index = 0;
            pa_resampler_run(forth, &i, &j);
            pa_resampler_run(back, &j, &k);

            dump_block("before", &a, &i);
            dump_block("after", &b, &j);
            dump_block("reverse", &a, &k);

            pa_memblock_unref(i.memblock);
            pa_memblock_unref(j.memblock);
            pa_memblock_unref(k.memblock);

            pa_resampler_free(forth);
            pa_resampler_free(back);
        }
    }

 quit:
    if (pool)
        pa_mempool_free(pool);

    return ret;
}

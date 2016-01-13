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
#include <math.h>

#include <check.h>

#include <pulse/sample.h>
#include <pulse/volume.h>

#include <pulsecore/macro.h>
#include <pulsecore/endianmacros.h>
#include <pulsecore/memblock.h>
#include <pulsecore/sample-util.h>
#include <pulsecore/mix.h>

/* PA_SAMPLE_U8 */
static const uint8_t u8_result[3][10] = {
{ 0x00, 0xff, 0x7f, 0x80, 0x9f, 0x3f, 0x01, 0xf0, 0x20, 0x21 },
{ 0x0c, 0xf2, 0x7f, 0x80, 0x9b, 0x45, 0x0d, 0xe4, 0x29, 0x2a },
{ 0x00, 0xff, 0x7e, 0x80, 0xba, 0x04, 0x00, 0xff, 0x00, 0x00 },
};

/* PA_SAMPLE_ALAW */
static const uint8_t alaw_result[3][10] = {
{ 0x00, 0xff, 0x7f, 0x80, 0x9f, 0x3f, 0x01, 0xf0, 0x20, 0x21 },
{ 0x06, 0xf2, 0x72, 0x86, 0x92, 0x32, 0x07, 0xf6, 0x26, 0x27 },
{ 0x31, 0xec, 0x6d, 0xb1, 0x8c, 0x2d, 0x36, 0xe1, 0x2a, 0x2a },
};

/* PA_SAMPLE_ULAW */
static const uint8_t ulaw_result[3][10] = {
{ 0x00, 0xff, 0x7f, 0x80, 0x9f, 0x3f, 0x01, 0xf0, 0x20, 0x21 },
{ 0x03, 0xff, 0xff, 0x83, 0xa2, 0x42, 0x04, 0xf2, 0x23, 0x24 },
{ 0x00, 0xff, 0xff, 0x80, 0x91, 0x31, 0x00, 0xe9, 0x12, 0x13 },
};

static const uint16_t s16ne_result[3][10] = {
{ 0x0000, 0xffff, 0x7fff, 0x8000, 0x9fff, 0x3fff, 0x0001, 0xf000, 0x0020, 0x0021 },
{ 0x0000, 0xffff, 0x7332, 0x8ccd, 0xa998, 0x3998, 0x0000, 0xf199, 0x001c, 0x001d },
{ 0x0000, 0xfffe, 0x7fff, 0x8000, 0x8000, 0x7997, 0x0001, 0xe199, 0x003c, 0x003e },
};

static const float float32ne_result[3][10] = {
{ 0.000000, -1.000000, 1.000000, 4711.000000, 0.222000, 0.330000, -0.300000, 99.000000, -0.555000, -0.123000 },
{ 0.000000, -0.899987, 0.899987, 4239.837402, 0.199797, 0.296996, -0.269996, 89.098679, -0.499493, -0.110698 },
{ 0.000000, -1.899987, 1.899987, 8950.837891, 0.421797, 0.626996, -0.569996, 188.098679, -1.054493, -0.233698 },
};

static const uint32_t s32ne_result[3][10] = {
{ 0x00000001, 0xffff0002, 0x7fff0003, 0x80000004, 0x9fff0005, 0x3fff0006, 0x00010007, 0xf0000008, 0x00200009, 0x0021000a },
{ 0x00000000, 0xffff199b, 0x7332199c, 0x8ccd0003, 0xa998d99e, 0x3998999f, 0x0000e66c, 0xf199a007, 0x001cccc8, 0x001db32e },
{ 0x00000001, 0xfffe199d, 0x7fffffff, 0x80000000, 0x80000000, 0x799799a5, 0x0001e673, 0xe199a00f, 0x003cccd1, 0x003eb338 },
};

/* attention: result is in BE, not NE! */
static const uint8_t s24be_result[3][30] = {
{ 0x00, 0x00, 0x01, 0xff, 0xff, 0x02, 0x7f, 0xff, 0x03, 0x80, 0x00, 0x04, 0x9f, 0xff, 0x05, 0x3f, 0xff, 0x06, 0x01, 0x00, 0x07, 0xf0, 0x00, 0x08, 0x20, 0x00, 0x09, 0x21, 0x00, 0x0a },
{ 0x00, 0x00, 0x00, 0xff, 0xff, 0x1b, 0x73, 0x32, 0x1c, 0x8c, 0xcd, 0x03, 0xa9, 0x98, 0xde, 0x39, 0x98, 0x9f, 0x00, 0xe6, 0x6c, 0xf1, 0x99, 0xa7, 0x1c, 0xcc, 0xc8, 0x1d, 0xb3, 0x2e },
{ 0x00, 0x00, 0x01, 0xff, 0xfe, 0x1d, 0x7f, 0xff, 0xff, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x79, 0x97, 0xa5, 0x01, 0xe6, 0x73, 0xe1, 0x99, 0xaf, 0x3c, 0xcc, 0xd1, 0x3e, 0xb3, 0x38 },
};

static const uint32_t s24_32ne_result[3][10] = {
{ 0x00000001, 0xffff0002, 0x7fff0003, 0x80000004, 0x9fff0005, 0x3fff0006, 0x00010007, 0xf0000008, 0x00200009, 0x0021000a },
{ 0x00000000, 0x00ff199b, 0x00ff199c, 0x00000003, 0x00ff199e, 0x00ff199f, 0x0000e66c, 0x00000007, 0x001cccc8, 0x001db32e },
{ 0x00000001, 0x00fe199d, 0x00fe199f, 0x00000007, 0x00fe19a3, 0x00fe19a5, 0x0001e673, 0x0000000f, 0x003cccd1, 0x003eb338 },
};

static void compare_block(const pa_sample_spec *ss, const pa_memchunk *chunk, int iter) {
    void *d;
    unsigned i;

    d = pa_memblock_acquire(chunk->memblock);

    switch (ss->format) {
        case PA_SAMPLE_U8: {
            const uint8_t *v = u8_result[iter];
            uint8_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                fail_unless(*u == *v, NULL);
                ++u;
                ++v;
            }
            break;
        }

        case PA_SAMPLE_ALAW: {
            const uint8_t *v = alaw_result[iter];
            uint8_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                fail_unless(*u == *v, NULL);
                ++u;
                ++v;
            }
            break;
        }

        case PA_SAMPLE_ULAW: {
            const uint8_t *v = ulaw_result[iter];
            uint8_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                fail_unless(*u == *v, NULL);
                ++u;
                ++v;
            }
            break;
        }

        case PA_SAMPLE_S16NE:
        case PA_SAMPLE_S16RE: {
            const uint16_t *v = s16ne_result[iter];
            uint16_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                uint16_t uu = PA_MAYBE_UINT16_SWAP(ss->format != PA_SAMPLE_S16NE, *u);
                fail_unless(uu == *v, NULL);
                ++u;
                ++v;
            }
            break;
        }

        case PA_SAMPLE_FLOAT32NE:
        case PA_SAMPLE_FLOAT32RE: {
            const float *v = float32ne_result[iter];
            float *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                float uu = ss->format == PA_SAMPLE_FLOAT32NE ? *u : PA_READ_FLOAT32RE(u);
                fail_unless(fabsf(uu - *v) <= 1e-6f, NULL);
                ++u;
                ++v;
            }
            break;
        }

        case PA_SAMPLE_S32NE:
        case PA_SAMPLE_S32RE: {
            const uint32_t *v = s32ne_result[iter];
            uint32_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                uint32_t uu = PA_MAYBE_UINT32_SWAP(ss->format != PA_SAMPLE_S32NE, *u);
                fail_unless(uu == *v, NULL);
                ++u;
                ++v;
            }
            break;
        }

        case PA_SAMPLE_S24_32NE:
        case PA_SAMPLE_S24_32RE: {
            const uint32_t *v = s24_32ne_result[iter];
            uint32_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                uint32_t uu = PA_MAYBE_UINT32_SWAP(ss->format != PA_SAMPLE_S24_32NE, *u);
                fail_unless(uu == *v, NULL);
                ++u;
                ++v;
            }
            break;
        }

        case PA_SAMPLE_S24NE:
        case PA_SAMPLE_S24RE: {
            const uint8_t *v = s24be_result[iter];
            uint8_t *u = d;

            for (i = 0; i < chunk->length / pa_frame_size(ss); i++) {
                uint32_t uu = ss->format == PA_SAMPLE_S24LE ? PA_READ24LE(u) : PA_READ24BE(u);
                fail_unless(uu == PA_READ24BE(v), NULL);

                u += 3;
                v += 3;
            }
            break;
        }

        default:
            pa_assert_not_reached();
    }

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
            memcpy(d, u8_result[0], sizeof(u8_result[0]));
            break;
        }

        case PA_SAMPLE_S16NE:
        case PA_SAMPLE_S16RE: {
            if (ss->format == PA_SAMPLE_S16RE) {
                uint16_t *u = d;
                for (i = 0; i < 10; i++)
                    u[i] = PA_UINT16_SWAP(s16ne_result[0][i]);
            } else
                memcpy(d, s16ne_result[0], sizeof(s16ne_result[0]));
            break;
        }

        case PA_SAMPLE_S24_32NE:
        case PA_SAMPLE_S24_32RE:
        case PA_SAMPLE_S32NE:
        case PA_SAMPLE_S32RE: {
            if (ss->format == PA_SAMPLE_S24_32RE || ss->format == PA_SAMPLE_S32RE) {
                uint32_t *u = d;
                for (i = 0; i < 10; i++)
                    u[i] = PA_UINT32_SWAP(s32ne_result[0][i]);
            } else
                memcpy(d, s32ne_result[0], sizeof(s32ne_result[0]));
            break;
        }

        case PA_SAMPLE_S24NE:
        case PA_SAMPLE_S24RE:
            if (ss->format == PA_SAMPLE_S24LE) {
                uint8_t *u = d;
                for (i = 0; i < 30; i += 3)
                    PA_WRITE24LE(&u[i], PA_READ24BE(&s24be_result[0][i]));
            } else
                memcpy(d, s24be_result[0], sizeof(s24be_result[0]));
            break;

        case PA_SAMPLE_FLOAT32NE:
        case PA_SAMPLE_FLOAT32RE: {
            if (ss->format == PA_SAMPLE_FLOAT32RE) {
                float *u = d;
                for (i = 0; i < 10; i++)
                    PA_WRITE_FLOAT32RE(&u[i], float32ne_result[0][i]);
            } else
                memcpy(d, float32ne_result[0], sizeof(float32ne_result[0]));

            break;
        }

        default:
            pa_assert_not_reached();
    }

    pa_memblock_release(r);

    return r;
}

START_TEST (mix_test) {
    pa_mempool *pool;
    pa_sample_spec a;
    pa_cvolume v;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    fail_unless((pool = pa_mempool_new(false, 0)) != NULL, NULL);

    a.channels = 1;
    a.rate = 44100;

    v.channels = a.channels;
    v.values[0] = pa_sw_volume_from_linear(0.9);

    for (a.format = 0; a.format < PA_SAMPLE_MAX; a.format ++) {
        pa_memchunk i, j, k;
        pa_mix_info m[2];
        void *ptr;

        pa_log_debug("=== mixing: %s\n", pa_sample_format_to_string(a.format));

        /* Generate block */
        i.memblock = generate_block(pool, &a);
        i.length = pa_memblock_get_length(i.memblock);
        i.index = 0;

        /* Make a copy */
        j = i;
        pa_memblock_ref(j.memblock);
        pa_memchunk_make_writable(&j, 0);

        /* Adjust volume of the copy */
        pa_volume_memchunk(&j, &a, &v);

        compare_block(&a, &j, 1);

        m[0].chunk = i;
        m[0].volume.values[0] = PA_VOLUME_NORM;
        m[0].volume.channels = a.channels;
        m[1].chunk = j;
        m[1].volume.values[0] = PA_VOLUME_NORM;
        m[1].volume.channels = a.channels;

        k.memblock = pa_memblock_new(pool, i.length);
        k.length = i.length;
        k.index = 0;

        ptr = pa_memblock_acquire_chunk(&k);
        pa_mix(m, 2, ptr, k.length, &a, NULL, false);
        pa_memblock_release(k.memblock);

        compare_block(&a, &k, 2);

        pa_memblock_unref(i.memblock);
        pa_memblock_unref(j.memblock);
        pa_memblock_unref(k.memblock);
    }

    pa_mempool_free(pool);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    s = suite_create("Mix");
    tc = tcase_create("mix");
    tcase_add_test(tc, mix_test);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

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

#include <check.h>

#include <pulse/pulseaudio.h>
#include <pulse/sample.h>
#include <pulsecore/memblock.h>

#include <pulsecore/filter/lfe-filter.h>

struct lfe_filter_test {
    pa_lfe_filter_t *lf;
    pa_mempool *pool;
    pa_sample_spec *ss;
};

static uint8_t *ori_sample_ptr;

#define ONE_BLOCK_SAMPLES 4096
#define TOTAL_SAMPLES 8192
#define TOLERANT_VARIATION 1

static void save_data_block(struct lfe_filter_test *lft, void *d, pa_memblock *blk) {
    uint8_t *dst = d, *src;
    size_t blk_size = pa_frame_size(lft->ss) * ONE_BLOCK_SAMPLES;

    src = pa_memblock_acquire(blk);
    memcpy(dst, src, blk_size);
    pa_memblock_release(blk);
}

static pa_memblock* generate_data_block(struct lfe_filter_test *lft, int start) {
    pa_memblock *r;
    uint8_t *d, *s = ori_sample_ptr;
    size_t blk_size = pa_frame_size(lft->ss) * ONE_BLOCK_SAMPLES;

    pa_assert_se(r = pa_memblock_new(lft->pool, blk_size));
    d = pa_memblock_acquire(r);
    memcpy(d, s + start,  blk_size);
    pa_memblock_release(r);

    return r;
}

static int compare_data_block(struct lfe_filter_test *lft, void *a, void *b) {
    int ret = 0;
    uint32_t i;
    uint16_t *r = a, *u = b;

    pa_assert(lft->ss->format == PA_SAMPLE_S16NE);

    for (i = 0; i < ONE_BLOCK_SAMPLES; i++) {
        if (abs(*r++ - *u++) > TOLERANT_VARIATION) {
            pa_log_error("lfe-filter-test: test failed, the output data in the position 0x%x of a block does not equal!\n", i);
            ret = -1;
            break;
        }
    }
    return ret;
}

/* in this test case, we pass two blocks of sample data to lfe-filter, each
   block contains 4096 samples, and don't let rewind_samples exceed TOTAL_SAMPLES */
static int lfe_filter_rewind_test(struct lfe_filter_test *lft, int rewind_samples)
{
    int ret = -1, pos, i;
    pa_memchunk mc;
    uint8_t *outptr;
    uint32_t fz = pa_frame_size(lft->ss);

    if (rewind_samples > TOTAL_SAMPLES || rewind_samples < TOTAL_SAMPLES - ONE_BLOCK_SAMPLES) {
        pa_log_error("lfe-filter-test: Please keep %d samples < rewind_samples < %d samples\n", TOTAL_SAMPLES - ONE_BLOCK_SAMPLES, TOTAL_SAMPLES);
        return ret;
    }

    outptr = pa_xmalloc(fz * TOTAL_SAMPLES);

    /* let lfe-filter process all samples first, and save the processed data to the temp buffer,
       then rewind back to some position, reprocess some samples and compare the output data with
       the processed data saved before. */
    for (i = 0; i < TOTAL_SAMPLES / ONE_BLOCK_SAMPLES; i++) {
        mc.memblock = generate_data_block(lft, i * ONE_BLOCK_SAMPLES * fz);
        mc.length = pa_memblock_get_length(mc.memblock);
        mc.index = 0;
        pa_lfe_filter_process(lft->lf, &mc);
        save_data_block(lft, outptr + i * ONE_BLOCK_SAMPLES * fz, mc.memblock);
        pa_memblock_unref(mc.memblock);
    }

    pa_lfe_filter_rewind(lft->lf, rewind_samples * fz);
    pos = (TOTAL_SAMPLES - rewind_samples) * fz;
    mc.memblock = generate_data_block(lft, pos);
    mc.length = pa_memblock_get_length(mc.memblock);
    mc.index = 0;
    pa_lfe_filter_process(lft->lf, &mc);
    ret = compare_data_block(lft, outptr + pos, pa_memblock_acquire(mc.memblock));
    pa_memblock_release(mc.memblock);
    pa_memblock_unref(mc.memblock);

    pa_xfree(outptr);

    return ret;
}

START_TEST (lfe_filter_test) {
    pa_sample_spec a;
    int ret = -1;
    unsigned i, crossover_freq = 120;
    pa_channel_map chmapmono = {1, {PA_CHANNEL_POSITION_LFE}};
    struct lfe_filter_test lft;
    short *tmp_ptr;

    pa_log_set_level(PA_LOG_DEBUG);

    a.channels = 1;
    a.rate = 44100;
    a.format = PA_SAMPLE_S16NE;

    lft.ss = &a;
    pa_assert_se(lft.pool = pa_mempool_new(false, 0));

    /* We prepare pseudo-random input audio samples for lfe-filter rewind testing*/
    ori_sample_ptr = pa_xmalloc(pa_frame_size(lft.ss) * TOTAL_SAMPLES);
    tmp_ptr = (short *) ori_sample_ptr;
    for (i = 0; i < pa_frame_size(lft.ss) * TOTAL_SAMPLES / sizeof(short); i++)
        *tmp_ptr++ = random();

    /* we create a lfe-filter with cutoff frequency 120Hz and max rewind time 10 seconds */
    pa_assert_se(lft.lf = pa_lfe_filter_new(&a, &chmapmono, crossover_freq, a.rate * 10));
    /* rewind to a block boundary */
    ret = lfe_filter_rewind_test(&lft, ONE_BLOCK_SAMPLES);
    if (ret)
        pa_log_error("lfe-filer-test: rewind to block boundary test failed!!!");
    pa_lfe_filter_free(lft.lf);

    /* we create a lfe-filter with cutoff frequency 120Hz and max rewind time 10 seconds */
    pa_assert_se(lft.lf = pa_lfe_filter_new(&a, &chmapmono, crossover_freq, a.rate * 10));
    /* rewind to the middle position of a block */
    ret = lfe_filter_rewind_test(&lft, ONE_BLOCK_SAMPLES + ONE_BLOCK_SAMPLES / 2);
    if (ret)
        pa_log_error("lfe-filer-test: rewind to middle of block test failed!!!");

    pa_xfree(ori_sample_ptr);

    pa_lfe_filter_free(lft.lf);

    pa_mempool_free(lft.pool);

    if (!ret)
        pa_log_debug("lfe-filter-test: tests for both rewind to block boundary and rewind to middle position of a block passed!");

    fail_unless(ret == 0);
}
END_TEST

int main(int argc, char *argv[]) {
    int failed = 0;
    Suite *s;
    TCase *tc;
    SRunner *sr;

    if (!getenv("MAKE_CHECK"))
        pa_log_set_level(PA_LOG_DEBUG);

    s = suite_create("lfe-filter");
    tc = tcase_create("lfe-filter");
    tcase_add_test(tc, lfe_filter_test);
    tcase_set_timeout(tc, 10);
    suite_add_tcase(s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

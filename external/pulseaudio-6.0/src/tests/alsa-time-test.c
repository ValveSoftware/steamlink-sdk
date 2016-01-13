#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#define SAMPLE_RATE 44100
#define CHANNELS 2

static uint64_t timespec_us(const struct timespec *ts) {
    return
        ts->tv_sec * 1000000LLU +
        ts->tv_nsec / 1000LLU;
}

int main(int argc, char *argv[]) {
    const char *dev;
    int r, cap, count = 0;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_status_t *status;
    snd_pcm_t *pcm;
    unsigned rate = SAMPLE_RATE;
    unsigned periods = 2;
    snd_pcm_uframes_t boundary, buffer_size = SAMPLE_RATE/10; /* 100s */
    int dir = 1;
    int fillrate;
    struct timespec start, last_timestamp = { 0, 0 };
    uint64_t start_us, last_us = 0;
    snd_pcm_sframes_t last_avail = 0, last_delay = 0;
    struct pollfd *pollfds;
    int n_pollfd;
    int64_t sample_count = 0;
    uint16_t *samples;
    struct sched_param sp;

    r = -1;
#ifdef _POSIX_PRIORITY_SCHEDULING
    sp.sched_priority = 5;
    r = pthread_setschedparam(pthread_self(), SCHED_RR, &sp);
#endif
    if (r)
        printf("Could not get RT prio. :(\n");

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_status_alloca(&status);

    r = clock_gettime(CLOCK_MONOTONIC, &start);
    assert(r == 0);

    start_us = timespec_us(&start);

    dev = argc > 1 ? argv[1] : "front:0";
    cap = argc > 2 ? atoi(argv[2]) : 0;
    fillrate = argc > 3 ? atoi(argv[3]) : 1;

    samples = calloc(fillrate, CHANNELS*sizeof(uint16_t));
    assert(samples);

    if (cap == 0)
      r = snd_pcm_open(&pcm, dev, SND_PCM_STREAM_PLAYBACK, 0);
    else
      r = snd_pcm_open(&pcm, dev, SND_PCM_STREAM_CAPTURE, 0);
    assert(r == 0);

    r = snd_pcm_hw_params_any(pcm, hwparams);
    assert(r == 0);

    r = snd_pcm_hw_params_set_rate_resample(pcm, hwparams, 0);
    assert(r == 0);

    r = snd_pcm_hw_params_set_access(pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    assert(r == 0);

    r = snd_pcm_hw_params_set_format(pcm, hwparams, SND_PCM_FORMAT_S16_LE);
    assert(r == 0);

    r = snd_pcm_hw_params_set_rate_near(pcm, hwparams, &rate, NULL);
    assert(r == 0);

    r = snd_pcm_hw_params_set_channels(pcm, hwparams, CHANNELS);
    assert(r == 0);

    r = snd_pcm_hw_params_set_periods_integer(pcm, hwparams);
    assert(r == 0);

    r = snd_pcm_hw_params_set_periods_near(pcm, hwparams, &periods, &dir);
    assert(r == 0);

    r = snd_pcm_hw_params_set_buffer_size_near(pcm, hwparams, &buffer_size);
    assert(r == 0);

    r = snd_pcm_hw_params(pcm, hwparams);
    assert(r == 0);

    r = snd_pcm_hw_params_current(pcm, hwparams);
    assert(r == 0);

    r = snd_pcm_sw_params_current(pcm, swparams);
    assert(r == 0);

    if (cap == 0)
      r = snd_pcm_sw_params_set_avail_min(pcm, swparams, 1);
    else
      r = snd_pcm_sw_params_set_avail_min(pcm, swparams, 0);
    assert(r == 0);

    r = snd_pcm_sw_params_set_period_event(pcm, swparams, 0);
    assert(r == 0);

    r = snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
    assert(r == 0);
    r = snd_pcm_sw_params_set_start_threshold(pcm, swparams, buffer_size - (buffer_size % fillrate));
    assert(r == 0);

    r = snd_pcm_sw_params_get_boundary(swparams, &boundary);
    assert(r == 0);
    r = snd_pcm_sw_params_set_stop_threshold(pcm, swparams, boundary);
    assert(r == 0);

    r = snd_pcm_sw_params_set_tstamp_mode(pcm, swparams, SND_PCM_TSTAMP_ENABLE);
    assert(r == 0);

    r = snd_pcm_sw_params(pcm, swparams);
    assert(r == 0);

    r = snd_pcm_prepare(pcm);
    assert(r == 0);

    r = snd_pcm_sw_params_current(pcm, swparams);
    assert(r == 0);

/*     assert(snd_pcm_hw_params_is_monotonic(hwparams) > 0); */

    n_pollfd = snd_pcm_poll_descriptors_count(pcm);
    assert(n_pollfd > 0);

    pollfds = malloc(sizeof(struct pollfd) * n_pollfd);
    assert(pollfds);

    r = snd_pcm_poll_descriptors(pcm, pollfds, n_pollfd);
    assert(r == n_pollfd);

    printf("Starting. Buffer size is %u frames\n", (unsigned int) buffer_size);

    if (cap) {
      r = snd_pcm_start(pcm);
      assert(r == 0);
    }

    for (;;) {
        snd_pcm_sframes_t avail, delay;
        struct timespec now, timestamp;
        unsigned short revents;
        int handled = 0;
        uint64_t now_us, timestamp_us;
        snd_pcm_state_t state;
        unsigned long long pos;

        r = poll(pollfds, n_pollfd, 0);
        assert(r >= 0);

        r = snd_pcm_poll_descriptors_revents(pcm, pollfds, n_pollfd, &revents);
        assert(r == 0);

        if (cap == 0)
          assert((revents & ~POLLOUT) == 0);
        else
          assert((revents & ~POLLIN) == 0);

        avail = snd_pcm_avail(pcm);
        assert(avail >= 0);

        r = snd_pcm_status(pcm, status);
        assert(r == 0);

        /* This assertion fails from time to time. ALSA seems to be broken */
/*         assert(avail == (snd_pcm_sframes_t) snd_pcm_status_get_avail(status)); */
/*         printf("%lu %lu\n", (unsigned long) avail, (unsigned long) snd_pcm_status_get_avail(status)); */

        snd_pcm_status_get_htstamp(status, &timestamp);
        delay = snd_pcm_status_get_delay(status);
        state = snd_pcm_status_get_state(status);

        r = clock_gettime(CLOCK_MONOTONIC, &now);
        assert(r == 0);

        assert(!revents || avail > 0);

        if ((!cap && (avail >= fillrate)) || (cap && (unsigned)avail >= buffer_size)) {
            snd_pcm_sframes_t sframes;

            if (cap == 0)
              sframes = snd_pcm_writei(pcm, samples, fillrate);
            else
              sframes = snd_pcm_readi(pcm, samples, fillrate);
            assert(sframes == fillrate);

            handled = fillrate;
            sample_count += fillrate;
        }

        if (!handled &&
            memcmp(&timestamp, &last_timestamp, sizeof(timestamp)) == 0 &&
            avail == last_avail &&
            delay == last_delay) {
            /* This is boring */
            continue;
        }

        now_us = timespec_us(&now);
        timestamp_us = timespec_us(&timestamp);

        if (cap == 0)
            pos = (unsigned long long) ((sample_count - handled - delay) * 1000000LU / SAMPLE_RATE);
        else
            pos = (unsigned long long) ((sample_count - handled + delay) * 1000000LU / SAMPLE_RATE);

        if (count++ % 50 == 0)
            printf("Elapsed\tCPU\tALSA\tPos\tSamples\tavail\tdelay\trevents\thandled\tstate\n");

        printf("%llu\t%llu\t%llu\t%llu\t%llu\t%li\t%li\t%i\t%i\t%i\n",
               (unsigned long long) (now_us - last_us),
               (unsigned long long) (now_us - start_us),
               (unsigned long long) (timestamp_us ? timestamp_us - start_us : 0),
               pos,
               (unsigned long long) sample_count,
               (signed long) avail,
               (signed long) delay,
               revents,
               handled,
               state);

        if (cap == 0)
          /** When this assert is hit, most likely something bad
           * happened, i.e. the avail jumped suddenly. */
          assert((unsigned) avail <= buffer_size);

        last_avail = avail;
        last_delay = delay;
        last_timestamp = timestamp;
        last_us = now_us;
    }

    return 0;
}

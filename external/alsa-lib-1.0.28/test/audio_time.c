/*
 * This program only tracks the difference between system time
 * and audio time, as reported in snd_pcm_status(). It should be
 * helpful to verify the information reported by drivers.
 */

#include "../include/asoundlib.h"
#include <math.h>

static char *device = "hw:0,0";

snd_output_t *output = NULL;

long long timestamp2ns(snd_htimestamp_t t)
{
	long long nsec;

	nsec = t.tv_sec * 1000000000;
	nsec += t.tv_nsec;

	return nsec;
}

long long timediff(snd_htimestamp_t t1, snd_htimestamp_t t2)
{
	long long nsec1, nsec2;

	nsec1 = timestamp2ns(t1);
	nsec2 = timestamp2ns(t2);

	return nsec1 - nsec2;
}

void gettimestamp(snd_pcm_t *handle, snd_htimestamp_t *timestamp,
		  snd_htimestamp_t *trigger_timestamp,
		  snd_htimestamp_t *audio_timestamp,
		  snd_pcm_uframes_t *avail, snd_pcm_sframes_t *delay)
{
	int err;
	snd_pcm_status_t *status;

	snd_pcm_status_alloca(&status);
	if ((err = snd_pcm_status(handle, status)) < 0) {
		printf("Stream status error: %s\n", snd_strerror(err));
		exit(0);
	}
	snd_pcm_status_get_trigger_htstamp(status, trigger_timestamp);
	snd_pcm_status_get_htstamp(status, timestamp);
	snd_pcm_status_get_audio_htstamp(status, audio_timestamp);
	*avail = snd_pcm_status_get_avail(status);
	*delay = snd_pcm_status_get_delay(status);
}

#define PERIOD 6000
#define PCM_LINK        /* sync start for playback and capture */
#define TRACK_CAPTURE   /* dump capture timing info  */
#define TRACK_PLAYBACK  /* dump playback timing info */
#define TRACK_SAMPLE_COUNTS /* show difference between sample counters and audiotimestamps returned by driver */
#define PLAYBACK_BUFFERS 4


int main(void)
{
        int err;
        unsigned int i;
        snd_pcm_t *handle_p = NULL;
        snd_pcm_t *handle_c = NULL;
        snd_pcm_sframes_t frames;
	snd_htimestamp_t tstamp_c, tstamp_p;
	snd_htimestamp_t trigger_tstamp_c, trigger_tstamp_p;
	snd_htimestamp_t audio_tstamp_c, audio_tstamp_p;
	unsigned char buffer_p[PERIOD*4*4];
	unsigned char buffer_c[PERIOD*4*4];

	snd_pcm_hw_params_t *hwparams_p;
	snd_pcm_hw_params_t *hwparams_c;

	snd_pcm_sw_params_t *swparams_p;
	snd_pcm_sw_params_t *swparams_c;

	snd_pcm_uframes_t curr_count_c;
	snd_pcm_uframes_t frame_count_c = 0;
	snd_pcm_uframes_t curr_count_p;
	snd_pcm_uframes_t frame_count_p = 0;

	snd_pcm_sframes_t delay_p, delay_c;
	snd_pcm_uframes_t avail_p, avail_c;

	if ((err = snd_pcm_open(&handle_p, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		goto _exit;
	}
	if ((err = snd_pcm_set_params(handle_p,
	                              SND_PCM_FORMAT_S16,
	                              SND_PCM_ACCESS_RW_INTERLEAVED,
	                              2,
	                              48000,
	                              0,
	                              500000)) < 0) {	/* 0.5sec */
		printf("Playback open error: %s\n", snd_strerror(err));
		goto _exit;
	}

	snd_pcm_hw_params_alloca(&hwparams_p);
	/* get the current hwparams */
	err = snd_pcm_hw_params_current(handle_p, hwparams_p);
	if (err < 0) {
		printf("Unable to determine current hwparams_p: %s\n", snd_strerror(err));
		goto _exit;
	}
	if (snd_pcm_hw_params_supports_audio_wallclock_ts(hwparams_p))
		printf("Playback relies on audio wallclock timestamps\n");
	else
		printf("Playback relies on audio sample counter timestamps\n");

	snd_pcm_sw_params_alloca(&swparams_p);
	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle_p, swparams_p);
	if (err < 0) {
		printf("Unable to determine current swparams_p: %s\n", snd_strerror(err));
		goto _exit;
	}

	/* enable tstamp */
	err = snd_pcm_sw_params_set_tstamp_mode(handle_p, swparams_p, SND_PCM_TSTAMP_ENABLE);
	if (err < 0) {
		printf("Unable to set tstamp mode : %s\n", snd_strerror(err));
		goto _exit;
	}

	/* write the sw parameters */
	err = snd_pcm_sw_params(handle_p, swparams_p);
	if (err < 0) {
		printf("Unable to set swparams_p : %s\n", snd_strerror(err));
		goto _exit;
	}

	if ((err = snd_pcm_open(&handle_c, device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
		printf("Capture open error: %s\n", snd_strerror(err));
		goto _exit;
	}
	if ((err = snd_pcm_set_params(handle_c,
	                              SND_PCM_FORMAT_S16,
	                              SND_PCM_ACCESS_RW_INTERLEAVED,
	                              2,
	                              48000,
	                              0,
	                              500000)) < 0) {	/* 0.5sec */
		printf("Capture open error: %s\n", snd_strerror(err));
		goto _exit;
	}

	snd_pcm_hw_params_alloca(&hwparams_c);
	/* get the current hwparams */
	err = snd_pcm_hw_params_current(handle_c, hwparams_c);
	if (err < 0) {
		printf("Unable to determine current hwparams_c: %s\n", snd_strerror(err));
		goto _exit;
	}
	if (snd_pcm_hw_params_supports_audio_wallclock_ts(hwparams_c))
		printf("Capture relies on audio wallclock timestamps\n");
	else
		printf("Capture relies on audio sample counter timestamps\n");

	snd_pcm_sw_params_alloca(&swparams_c);
	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle_c, swparams_c);
	if (err < 0) {
		printf("Unable to determine current swparams_c: %s\n", snd_strerror(err));
		goto _exit;
	}

	/* enable tstamp */
	err = snd_pcm_sw_params_set_tstamp_mode(handle_c, swparams_c, SND_PCM_TSTAMP_ENABLE);
	if (err < 0) {
		printf("Unable to set tstamp mode : %s\n", snd_strerror(err));
		goto _exit;
	}

	/* write the sw parameters */
	err = snd_pcm_sw_params(handle_c, swparams_c);
	if (err < 0) {
		printf("Unable to set swparams_c : %s\n", snd_strerror(err));
		goto _exit;
	}

#ifdef PCM_LINK
	if ((err = snd_pcm_link(handle_c, handle_p)) < 0) {
		printf("Streams link error: %s\n", snd_strerror(err));
		exit(0);
	}
#endif

	i = PLAYBACK_BUFFERS;
	while (i--) {
                frames = snd_pcm_writei(handle_p, buffer_p, PERIOD);
                if (frames < 0) {
                        printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
                        goto _exit;
                }
		frame_count_p += frames;
	}

	if (PLAYBACK_BUFFERS != 4)
		snd_pcm_start(handle_p);

#ifndef PCM_LINK
	/* need to start capture explicitly */
	snd_pcm_start(handle_c);
#endif

        while (1) {

		frames = snd_pcm_wait(handle_c, -1);
		if (frames < 0) {
			printf("snd_pcm_wait failed: %s\n", snd_strerror(frames));
                        goto _exit;
		}

		frames = snd_pcm_readi(handle_c, buffer_c, PERIOD);
                if (frames < 0) {
                        printf("snd_pcm_readi failed: %s\n", snd_strerror(frames));
                        goto _exit;
                }
		frame_count_c += frames;

                frames = snd_pcm_writei(handle_p, buffer_p, PERIOD);
                if (frames < 0) {
                        printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
                        goto _exit;
                }

		frame_count_p += frames;

#if defined(TRACK_PLAYBACK)
		gettimestamp(handle_p, &tstamp_p, &trigger_tstamp_p, &audio_tstamp_p, &avail_p, &delay_p);

#if defined(TRACK_SAMPLE_COUNTS)
		curr_count_p = frame_count_p - delay_p; /* written minus queued */

		printf("playback: curr_count %lli driver count %lli, delta %lli\n",
		       (long long)curr_count_p * 1000000000LL / 48000 ,
		       timestamp2ns(audio_tstamp_p),
		       (long long)curr_count_p * 1000000000LL / 48000 - timestamp2ns(audio_tstamp_p)
		       );
#endif

		printf("playback: systime: %lli nsec, audio time %lli nsec, \tsystime delta %lli\n",
		       timediff(tstamp_p, trigger_tstamp_p),
		       timestamp2ns(audio_tstamp_p),
		       timediff(tstamp_p, trigger_tstamp_p) - timestamp2ns(audio_tstamp_p)
		       );
#endif

#if defined(TRACK_CAPTURE)
		gettimestamp(handle_c, &tstamp_c, &trigger_tstamp_c, &audio_tstamp_c, &avail_c, &delay_c);

#if defined(TRACK_SAMPLE_COUNTS)
		curr_count_c = frame_count_c + delay_c; /* read plus queued */


		printf("capture: curr_count %lli driver count %lli, delta %lli\n",
		       (long long)curr_count_c * 1000000000LL / 48000 ,
		       timestamp2ns(audio_tstamp_c),
		       (long long)curr_count_c * 1000000000LL / 48000 - timestamp2ns(audio_tstamp_c)
		       );
#endif

		printf("\t capture: systime: %lli nsec, audio time %lli nsec, \tsystime delta %lli\n",
		       timediff(tstamp_c, trigger_tstamp_c),
		       timestamp2ns(audio_tstamp_c),
		       timediff(tstamp_c, trigger_tstamp_c) - timestamp2ns(audio_tstamp_c)
		       );
#endif

        }

_exit:
	if (handle_p)
		snd_pcm_close(handle_p);
	if (handle_c)
		snd_pcm_close(handle_c);

	return 0;
}

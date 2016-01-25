
#include "driver.h"
#include "minimal.h"

//sq #include <SDL/SDL.h>
#include <alsa/asoundlib.h>

extern "C" {
#include "fifo_buffer.h"
#include "thread.h"
}

int soundcard,usestereo;
int master_volume = 100;

static INT16 *stream_cache_data;
static int stream_cache_channels;
static int samples_per_frame;

//============================================================
//      LOCAL VARIABLES
//============================================================

int attenuation = 0;

// buffer over/underflow counts
static int fifo_underrun;
static int fifo_overrun;
static int snd_underrun;

typedef struct alsa
{
	snd_pcm_t *pcm;
	bool has_float;
	volatile bool thread_dead;
	
	size_t buffer_size_bytes;
	size_t period_size_bytes;
	snd_pcm_uframes_t period_size_frames;
	
	fifo_buffer_t *buffer;
	sthread_t *worker_thread;
	slock_t *fifo_lock;
	scond_t *cond;
	slock_t *cond_lock;
} alsa_t;

static alsa_t *g_alsa;

static alsa_t *alsa_init(void);
static void alsa_worker_thread(void *data);
static void alsa_free(void *data);

//============================================================

int msdos_init_sound(void)
{
	/* Ask the user if no soundcard was chosen */
	if (soundcard == -1) soundcard=1;

	if (soundcard == 0)     /* silence */
	{
		/* update the Machine structure to show that sound is disabled */
		Machine->sample_rate = 0;
		return 0;
	}

	stream_cache_data = 0;
	stream_cache_channels = 0;

	logerror("set sample rate: %d\n",Machine->sample_rate);

	osd_set_mastervolume(attenuation);	/* set the startup volume */

	return 0;
}

void msdos_shutdown_sound(void)
{
	return;
}

int osd_start_audio_stream(int stereo)
{
	if (stereo) stereo = 1;	/* make sure it's either 0 or 1 */

	stream_cache_channels = (stereo?2:1);

	if (options.force_stereo) stream_cache_channels=2;

	/* determine the number of samples per frame */
	//Fix for games like galaxian that have fractional fps
	if(Machine->drv->frames_per_second > 60) 
	{
		samples_per_frame = Machine->sample_rate / (int)Machine->drv->frames_per_second;
	} 
	else 
	{
		samples_per_frame = Machine->sample_rate / Machine->drv->frames_per_second;
	}

	//Sound switched off?
	if (Machine->sample_rate == 0) return samples_per_frame;

	// attempt to initialize SDL
	// alsa_init will also ammend the samples_per_frame
	g_alsa = alsa_init();

	return samples_per_frame;
}

void osd_stop_audio_stream(void)
{
	// if nothing to do, don't do it
	if (Machine->sample_rate == 0)
		return;
	
	alsa_free(g_alsa);
	
	// print out over/underflow stats
	logerror("Sound buffer: fifo_overrun=%d fifo_underrun=%d snd_underrun=%d\n", fifo_overrun, fifo_underrun, snd_underrun);	

}

#define min(a, b) ((a) < (b) ? (a) : (b))

static void alsa_worker_thread(void *data)
{
	alsa_t *alsa = (alsa_t*)data;
	int wait_on_buffer=1;
	size_t fifo_size;

	UINT8 *buf = (UINT8 *)calloc(1, alsa->period_size_bytes);
	if (!buf)
	{
		logerror("failed to allocate audio buffer");
		goto end;
	}
	
	while (!alsa->thread_dead)
	{
		slock_lock(alsa->fifo_lock);
		size_t avail = fifo_read_avail(alsa->buffer);

		//First run wait until the buffer is filled with a few frames
		if(avail < alsa->period_size_bytes*3 && wait_on_buffer)
		{
			slock_unlock(alsa->fifo_lock);
			continue;
		}
		wait_on_buffer=0;

		if(avail < alsa->period_size_bytes)
		{
			slock_unlock(alsa->fifo_lock);
			fifo_size = 0;
		}
		else
		{
			fifo_size = min(alsa->period_size_bytes, avail);
			if(fifo_size > alsa->period_size_bytes)
				fifo_size = alsa->period_size_bytes;
			fifo_read(alsa->buffer, buf, fifo_size);
			scond_signal(alsa->cond);
			slock_unlock(alsa->fifo_lock);
		}
	    
		// If underrun, fill rest with silence.
 		if(alsa->period_size_bytes != fifo_size) {
			memset(buf + fifo_size, 0, alsa->period_size_bytes - fifo_size);
 			fifo_underrun++;
 		}

		snd_pcm_sframes_t frames = snd_pcm_writei(alsa->pcm, buf, alsa->period_size_frames);
		if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
		{
			snd_underrun++;
			if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
			{
				logerror("[ALSA]: (#2) Failed to recover from error (%s)\n",
					snd_strerror(frames));
				break;
			}

			continue;
		}
		else if (frames < 0)
		{
			logerror("[ALSA]: Unknown error occured (%s).\n", snd_strerror(frames));
			break;
		}
	}
	
end:
	slock_lock(alsa->cond_lock);
	alsa->thread_dead = true;
	scond_signal(alsa->cond);
	slock_unlock(alsa->cond_lock);
	free(buf);
}

static ssize_t alsa_write(void *data, const void *buf, size_t size)
{
	alsa_t *alsa = (alsa_t*)data;
	
	if (alsa->thread_dead)
		return -1;
	
	slock_lock(alsa->fifo_lock);
	size_t avail = fifo_write_avail(alsa->buffer);
	size_t write_amt = min(avail, size);
	if(write_amt) 
		fifo_write(alsa->buffer, buf, write_amt);
	slock_unlock(alsa->fifo_lock);
	if(write_amt<size)	
	{
		fifo_overrun++;
	}
	return write_amt;
}

int osd_update_audio_stream(INT16 *buffer)
{
	//Soundcard switch off?
	if (Machine->sample_rate == 0) return samples_per_frame;

	stream_cache_data = buffer;
	
	profiler_mark(PROFILER_USER1);
	alsa_write(g_alsa, buffer, (samples_per_frame * stream_cache_channels * sizeof(signed short)) );
	profiler_mark(PROFILER_END);

	return samples_per_frame;
}

int msdos_update_audio(void)
{
	if (Machine->sample_rate == 0 || stream_cache_data == 0) return 0;
//	profiler_mark(PROFILER_MIXER);
//	updateaudiostream();
//	profiler_mark(PROFILER_END);
	return 0;
}

/* attenuation in dB */
void osd_set_mastervolume(int _attenuation)
{
}

int osd_get_mastervolume(void)
{
	return attenuation;
}

void osd_sound_enable(int enable_it)
{
}

#define TRY_ALSA(x) if (x < 0) { \
goto error; \
}

static alsa_t *alsa_init(void)
{
	alsa_t *alsa = (alsa_t*)calloc(1, sizeof(alsa_t));
	if (!alsa)
		return NULL;

	fifo_underrun=0;
	fifo_overrun=0;
	snd_underrun=0;
	
	snd_pcm_hw_params_t *params = NULL;
	
	const char *alsa_dev = "default";
	
	snd_pcm_uframes_t buffer_size_frames;
	
	TRY_ALSA(snd_pcm_open(&alsa->pcm, alsa_dev, SND_PCM_STREAM_PLAYBACK, 0));

	TRY_ALSA(snd_pcm_hw_params_malloc(&params));

	//latency is one frame times by a multiplier (higher improves crackling?)
	TRY_ALSA(snd_pcm_set_params(alsa->pcm,
								SND_PCM_FORMAT_S16,
								SND_PCM_ACCESS_RW_INTERLEAVED,
								stream_cache_channels,
								Machine->sample_rate,
								0,
								((float)TICKS_PER_SEC / (float)Machine->drv->frames_per_second) * 4)) ;

	TRY_ALSA(snd_pcm_get_params ( alsa->pcm, &buffer_size_frames, &alsa->period_size_frames ));

	//SQ Adjust MAME sound engine to what ALSA says its frame size is, ALSA 
	//tends to be even whereas MAME uses odd - based on the frame & sound rates.
	samples_per_frame = (int)alsa->period_size_frames;

	logerror("ALSA: Period size: %d frames\n", (int)alsa->period_size_frames);
	logerror("ALSA: Buffer size: %d frames\n", (int)buffer_size_frames);

	alsa->buffer_size_bytes = snd_pcm_frames_to_bytes(alsa->pcm, buffer_size_frames);
	alsa->period_size_bytes = snd_pcm_frames_to_bytes(alsa->pcm, alsa->period_size_frames);

	logerror("ALSA: Period size: %d bytes\n", (int)alsa->period_size_bytes);
	logerror("ALSA: Buffer size: %d bytes\n", (int)alsa->buffer_size_bytes);

	TRY_ALSA(snd_pcm_prepare(alsa->pcm));

	snd_pcm_hw_params_free(params);

	//Write initial blank sound to stop underruns?
	{
		void *tempbuf;
		tempbuf=calloc(1, alsa->period_size_bytes*3);
		snd_pcm_writei (alsa->pcm, tempbuf, 2 * alsa->period_size_frames);
		free(tempbuf);
	}

	alsa->fifo_lock = slock_new();
	alsa->cond_lock = slock_new();
	alsa->cond = scond_new();
	alsa->buffer = fifo_new(alsa->buffer_size_bytes*6);
	if (!alsa->fifo_lock || !alsa->cond_lock || !alsa->cond || !alsa->buffer)
		goto error;
	
	alsa->worker_thread = sthread_create(alsa_worker_thread, alsa);
	if (!alsa->worker_thread)
	{
		logerror("error initializing worker thread\n");
		goto error;
	}
	
	return alsa;
	
error:
	logerror("ALSA: Failed to initialize...\n");
	if (params)
		snd_pcm_hw_params_free(params);
	
	alsa_free(alsa);
	
	return NULL;

}


static void alsa_free(void *data)
{
	alsa_t *alsa = (alsa_t*)data;
	
	if (alsa)
	{
		if (alsa->worker_thread)
		{
			alsa->thread_dead = true;
			sthread_join(alsa->worker_thread);
		}
		if (alsa->buffer)
			fifo_free(alsa->buffer);
		if (alsa->cond)
			scond_free(alsa->cond);
		if (alsa->fifo_lock)
			slock_free(alsa->fifo_lock);
		if (alsa->cond_lock)
			slock_free(alsa->cond_lock);
		if (alsa->pcm)
		{
			snd_pcm_drop(alsa->pcm);
			snd_pcm_close(alsa->pcm);
		}
		free(alsa);
	}
}


static bool alsa_stop(void *data)
{
	(void)data;
	return true;
}

static bool alsa_start(void *data)
{
	(void)data;
	return true;
}

static size_t alsa_write_avail(void *data)
{
	alsa_t *alsa = (alsa_t*)data;
	
	if (alsa->thread_dead)
		return 0;
	slock_lock(alsa->fifo_lock);
	size_t val = fifo_write_avail(alsa->buffer);
	slock_unlock(alsa->fifo_lock);
	return val;
}

static size_t alsa_buffer_size(void *data)
{
	alsa_t *alsa = (alsa_t*)data;
	return alsa->buffer_size_bytes;
}


void osd_opl_control(int chip,int reg)
{
}

void osd_opl_write(int chip,int data)
{
}

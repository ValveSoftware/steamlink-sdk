
#include "driver.h"
#include "minimal.h"

#ifdef STEAMLINK
#define USE_SLAUDIO
#endif

#ifdef USE_SLAUDIO
#include <SLAudio.h>
#else
#include <SDL.h>
#endif

int soundcard,usestereo;
int master_volume = 100;

#ifdef USE_SLAUDIO
static CSLAudioContext *s_pContext;
static CSLAudioStream *s_pStream;
#else
static SDL_AudioDeviceID audio_device = 1;
#endif
static int stream_cache_channels;
static int samples_per_frame;
static int bytes_per_frame;

//============================================================
//      LOCAL VARIABLES
//============================================================

int attenuation = 0;

// buffer over/underflow counts
static int fifo_underrun;
static int fifo_overrun;

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

	stream_cache_channels = 0;

	logerror("set sample rate: %d\n", Machine->sample_rate);

	osd_set_mastervolume(attenuation);	/* set the startup volume */

	return 0;
}

void msdos_shutdown_sound(void)
{
}

int osd_start_audio_stream(int stereo)
{
	stream_cache_channels = (stereo?2:1);

	if (options.force_stereo) stream_cache_channels=2;

	/* determine the number of samples per frame */
	//Fix for games like galaxian that have fractional fps
	if (Machine->drv->frames_per_second > 60) 
	{
		samples_per_frame = Machine->sample_rate / (int)Machine->drv->frames_per_second;
	} 
	else 
	{
		samples_per_frame = Machine->sample_rate / Machine->drv->frames_per_second;
	}
	bytes_per_frame = (samples_per_frame * stream_cache_channels * sizeof(int16_t));

	//Sound switched off?
	if (Machine->sample_rate == 0) return samples_per_frame;

#ifdef USE_SLAUDIO
	s_pContext = SLAudio_CreateContext();
	s_pStream = SLAudio_CreateStream(s_pContext, Machine->sample_rate, stream_cache_channels, bytes_per_frame);
	if (!s_pStream) {
		logerror("Couldn't create audio stream\n");
		Machine->sample_rate = 0;
		return samples_per_frame;
	}
#else
	// attempt to initialize SDL
	SDL_AudioSpec spec;
	SDL_zero(spec);
	spec.freq = Machine->sample_rate;
	spec.format = AUDIO_S16SYS;
	spec.channels = stream_cache_channels;
	spec.samples = 1024;
	if (SDL_OpenAudio(&spec, NULL) < 0) {
		logerror("Couldn't open audio: %s\n", SDL_GetError());
		Machine->sample_rate = 0;
		return samples_per_frame;
	}
	SDL_PauseAudioDevice(audio_device, 0);
#endif

	return samples_per_frame;
}

void osd_stop_audio_stream(void)
{
	// if nothing to do, don't do it
	if (Machine->sample_rate == 0)
		return;

#ifdef USE_SLAUDIO
	if (s_pStream) {
		SLAudio_FreeStream(s_pStream);
		s_pStream = NULL;
	}
	if (s_pContext) {
		SLAudio_FreeContext(s_pContext);
		s_pContext = NULL;
	}
#else
	SDL_CloseAudio();
#endif

	// print out over/underflow stats
	logerror("Sound buffer: fifo_overrun=%d fifo_underrun=%d\n", fifo_overrun, fifo_underrun);
}

int osd_update_audio_stream(INT16 *buffer)
{
	// Soundcard switch off?
	if (Machine->sample_rate == 0) return samples_per_frame;

#ifdef USE_SLAUDIO
	void *frame = SLAudio_BeginFrame(s_pStream);
	memcpy(frame, buffer, bytes_per_frame);
	SLAudio_SubmitFrame(s_pStream);
#else
	Uint32 unBytesQueued = SDL_GetQueuedAudioSize(audio_device);
	if (unBytesQueued == 0) {
		++fifo_underrun;

		// Queue some silence to start buffering
		uint8_t silence[bytes_per_frame];
		memset(silence, 0, sizeof(silence));
		SDL_QueueAudio(audio_device, silence, sizeof(silence));
	}
	if (unBytesQueued > 10*bytes_per_frame) {
		++fifo_overrun;

		SDL_ClearQueuedAudio(audio_device);
	}

	profiler_mark(PROFILER_USER1);
	if (attenuation < 0) {
		/* FIXME: Need to attenuate the audio stream? */
	}
	SDL_QueueAudio(audio_device, buffer, bytes_per_frame);
	profiler_mark(PROFILER_END);
#endif

	return samples_per_frame;
}

int msdos_update_audio(void)
{
	return 0;
}

/* attenuation in dB */
void osd_set_mastervolume(int _attenuation)
{
	// clamp the attenuation to 0-32 range
	if (_attenuation > 0) {
		attenuation = 0;
	} else if (_attenuation < -32) {
		attenuation = -32;
	} else {
		attenuation = _attenuation;
	}
}

int osd_get_mastervolume(void)
{
	return attenuation;
}

void osd_sound_enable(int enable_it)
{
}
void osd_opl_control(int chip,int reg)
{
}

void osd_opl_write(int chip,int data)
{
}

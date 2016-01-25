/***************************************************************************

  mixer.c

  Manage audio channels allocation, with volume and panning control

***************************************************************************/

#include "driver.h"
#include <math.h>


/* enable this to turn off clipping (helpful to find cases where we max out */
#define DISABLE_CLIPPING		0

/* accumulators have ACCUMULATOR_SAMPLES samples (must be a power of 2) */
#define ACCUMULATOR_SAMPLES		8192
#define ACCUMULATOR_MASK		(ACCUMULATOR_SAMPLES - 1)

/* fractional numbers have FRACTION_BITS bits of resolution */
#define FRACTION_BITS			16
#define FRACTION_MASK			((1 << FRACTION_BITS) - 1)


static int mixer_sound_enabled;


/* holds all the data for the a mixer channel */
struct mixer_channel_data
{
	char		name[40];

	/* current volume, gain and pan */
	INT32		volume;
	INT32		gain;
	INT32		pan;

	/* mixing levels */
	UINT8		mixing_level;
	UINT8		default_mixing_level;
	UINT8		config_mixing_level;
	UINT8		config_default_mixing_level;

	/* current playback positions */
	UINT32		input_frac;
	UINT32		samples_available;
	UINT32		frequency;
	UINT32		step_size;

	/* state of non-streamed playback */
	UINT8		is_stream;
	UINT8		is_playing;
	UINT8		is_looping;
	UINT8		is_16bit;
	void *		data_start;
	void *		data_end;
	void *		data_current;
};



/* channel data */
static struct mixer_channel_data mixer_channel[MIXER_MAX_CHANNELS];
static UINT8 config_mixing_level[MIXER_MAX_CHANNELS];
static UINT8 config_default_mixing_level[MIXER_MAX_CHANNELS];
static UINT8 first_free_channel = 0;
static UINT8 config_invalid;
static UINT8 is_stereo;

/* 32-bit accumulators */
static UINT32 accum_base;
static INT32 left_accum[ACCUMULATOR_SAMPLES];
static INT32 right_accum[ACCUMULATOR_SAMPLES];

/* 16-bit mix buffers */
static INT16 mix_buffer[ACCUMULATOR_SAMPLES*2];	/* *2 for stereo */

/* global sample tracking */
static UINT32 samples_this_frame;



/* function prototypes */
static void mix_sample_8(struct mixer_channel_data *channel, int samples_to_generate);
static void mix_sample_16(struct mixer_channel_data *channel, int samples_to_generate);



/***************************************************************************
	mixer_sh_start
***************************************************************************/

int mixer_sh_start(void)
{
	struct mixer_channel_data *channel;
	int i;

	/* reset all channels to their defaults */
	memset(&mixer_channel, 0, sizeof(mixer_channel));
	for (i = 0, channel = mixer_channel; i < MIXER_MAX_CHANNELS; i++, channel++)
	{
		channel->mixing_level 					= 0xff;
		channel->default_mixing_level 			= 0xff;
		channel->config_mixing_level 			= config_mixing_level[i];
		channel->config_default_mixing_level 	= config_default_mixing_level[i];
	}

	/* determine if we're playing in stereo or not */
	first_free_channel = 0;
	is_stereo = ((Machine->drv->sound_attributes & SOUND_SUPPORTS_STEREO) != 0);

	/* clear the accumulators */
	accum_base = 0;
	memset(left_accum, 0, ACCUMULATOR_SAMPLES * sizeof(INT32));
	memset(right_accum, 0, ACCUMULATOR_SAMPLES * sizeof(INT32));

	samples_this_frame = osd_start_audio_stream(is_stereo);

	mixer_sound_enabled = 1;

	return 0;
}


/***************************************************************************
	mixer_sh_stop
***************************************************************************/

void mixer_sh_stop(void)
{
	osd_stop_audio_stream();
}


/***************************************************************************
	mixer_update_channel
***************************************************************************/

void mixer_update_channel(struct mixer_channel_data *channel, int total_sample_count)
{
	int samples_to_generate = total_sample_count - channel->samples_available;

	/* don't do anything for streaming channels */
	if (channel->is_stream)
		return;

	/* if we're all caught up, just return */
	if (samples_to_generate <= 0)
		return;

	/* if we're playing, mix in the data */
	if (channel->is_playing)
	{
		if (channel->is_16bit)
			mix_sample_16(channel, samples_to_generate);
		else
			mix_sample_8(channel, samples_to_generate);
	}

	/* just eat the rest */
	channel->samples_available += samples_to_generate;
}


/***************************************************************************
	mixer_sh_update
***************************************************************************/

void mixer_sh_update(void)
{
	struct mixer_channel_data *	channel;
	UINT32 accum_pos = accum_base;
	INT16 *mix;
	INT32 sample;
	int	i;

	profiler_mark(PROFILER_MIXER);

	/* update all channels (for streams this is a no-op) */
	for (i = 0, channel = mixer_channel; i < first_free_channel; i++, channel++)
	{
		mixer_update_channel(channel, samples_this_frame);

		/* if we needed more than they could give, adjust their pointers */
		if (samples_this_frame > channel->samples_available)
			channel->samples_available = 0;
		else
			channel->samples_available -= samples_this_frame;
	}

	/* copy the mono 32-bit data to a 16-bit buffer, clipping along the way */
	if (!is_stereo)
	{
		mix = mix_buffer;
		for (i = 0; i < samples_this_frame; i++)
		{
			/* fetch and clip the sample */
			sample = left_accum[accum_pos];
#if !DISABLE_CLIPPING
			if (sample < -32768)
				sample = -32768;
			else if (sample > 32767)
				sample = 32767;
#endif

			/* store and zero out behind us */
			*mix++ = sample;
			//SQ Force stereo for mono sound so just copy left to both channels (interleaved)
			if (options.force_stereo)
				*mix++ = sample;
			left_accum[accum_pos] = 0;

			/* advance to the next sample */
			accum_pos = (accum_pos + 1) & ACCUMULATOR_MASK;
		}
	}

	/* copy the stereo 32-bit data to a 16-bit buffer, clipping along the way */
	else
	{
		mix = mix_buffer;
		for (i = 0; i < samples_this_frame; i++)
		{
			/* fetch and clip the left sample */
			sample = left_accum[accum_pos];
#if !DISABLE_CLIPPING
			if (sample < -32768)
				sample = -32768;
			else if (sample > 32767)
				sample = 32767;
#endif

			/* store and zero out behind us */
			*mix++ = sample;
			left_accum[accum_pos] = 0;

			/* fetch and clip the right sample */
			sample = right_accum[accum_pos];
#if !DISABLE_CLIPPING
			if (sample < -32768)
				sample = -32768;
			else if (sample > 32767)
				sample = 32767;
#endif

			/* store and zero out behind us */
			*mix++ = sample;
			right_accum[accum_pos] = 0;

			/* advance to the next sample */
			accum_pos = (accum_pos + 1) & ACCUMULATOR_MASK;
		}
	}

	/* play the result */
	samples_this_frame = osd_update_audio_stream(mix_buffer);

	accum_base = accum_pos;

	profiler_mark(PROFILER_END);
}


/***************************************************************************
	mixer_allocate_channel
***************************************************************************/

int mixer_allocate_channel(int default_mixing_level)
{
	/* this is just a degenerate case of the multi-channel mixer allocate */
	return mixer_allocate_channels(1, &default_mixing_level);
}


/***************************************************************************
	mixer_allocate_channels
***************************************************************************/

int mixer_allocate_channels(int channels, const int *default_mixing_levels)
{
	int i, j;

	/* make sure we didn't overrun the number of available channels */
	if (first_free_channel + channels > MIXER_MAX_CHANNELS)
	{
		logerror("Too many mixer channels (requested %d, available %d)\n", first_free_channel + channels, MIXER_MAX_CHANNELS);
		exit(1);
	}

	/* loop over channels requested */
	for (i = 0; i < channels; i++)
	{
		struct mixer_channel_data *channel = &mixer_channel[first_free_channel + i];

		/* extract the basic data */
		channel->default_mixing_level 	= MIXER_GET_LEVEL(default_mixing_levels[i]);
		channel->pan 					= MIXER_GET_PAN(default_mixing_levels[i]);
		channel->gain 					= MIXER_GET_GAIN(default_mixing_levels[i]);
		channel->volume 				= 100;

		/* backwards compatibility with old 0-255 volume range */
		if (channel->default_mixing_level > 100)
			channel->default_mixing_level = channel->default_mixing_level * 25 / 255;

		/* attempt to load in the configuration data for this channel */
		channel->mixing_level = channel->default_mixing_level;
		if (!config_invalid)
		{
			/* if the defaults match, set the mixing level from the config */
			if (channel->default_mixing_level == channel->config_default_mixing_level)
				channel->mixing_level = channel->config_mixing_level;

			/* otherwise, invalidate all channels that have been created so far */
			else
			{
				config_invalid = 1;
				for (j = 0; j < first_free_channel + i; j++)
					mixer_set_mixing_level(j, mixer_channel[j].default_mixing_level);
			}
		}

		/* set the default name */
		mixer_set_name(first_free_channel + i, 0);
	}

	/* increment the counter and return the first one */
	first_free_channel += channels;
	return first_free_channel - channels;
}


/***************************************************************************
	mixer_set_name
***************************************************************************/

void mixer_set_name(int ch, const char *name)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	/* either copy the name or create a default one */
	if (name != NULL)
		strcpy(channel->name, name);
	else
		sprintf(channel->name, "<channel #%d>", ch);

	/* append left/right onto the channel as appropriate */
	if (channel->pan == MIXER_PAN_LEFT)
		strcat(channel->name, " (Lt)");
	else if (channel->pan == MIXER_PAN_RIGHT)
		strcat(channel->name, " (Rt)");
}


/***************************************************************************
	mixer_get_name
***************************************************************************/

const char *mixer_get_name(int ch)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	/* return a pointer to the name or a NULL for an unused channel */
	if (channel->name[0] != 0)
		return channel->name;
	else
		return NULL;
}


/***************************************************************************
	mixer_set_volume
***************************************************************************/

void mixer_set_volume(int ch, int volume)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));
	channel->volume = volume;
}


/***************************************************************************
	mixer_set_mixing_level
***************************************************************************/

void mixer_set_mixing_level(int ch, int level)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));
	channel->mixing_level = level;
}


/***************************************************************************
	mixer_get_mixing_level
***************************************************************************/

int mixer_get_mixing_level(int ch)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];
	return channel->mixing_level;
}


/***************************************************************************
	mixer_get_default_mixing_level
***************************************************************************/

int mixer_get_default_mixing_level(int ch)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];
	return channel->default_mixing_level;
}


/***************************************************************************
	mixer_read_config
***************************************************************************/

void mixer_read_config(void *f)
{
	UINT8 default_levels[MIXER_MAX_CHANNELS];
	UINT8 mixing_levels[MIXER_MAX_CHANNELS];
	int i;

	memset(default_levels, 0xff, sizeof(default_levels));
	memset(mixing_levels, 0xff, sizeof(mixing_levels));
	osd_fread(f, default_levels, MIXER_MAX_CHANNELS);
	osd_fread(f, mixing_levels, MIXER_MAX_CHANNELS);
	for (i = 0; i < MIXER_MAX_CHANNELS; i++)
	{
		config_default_mixing_level[i] = default_levels[i];
		config_mixing_level[i] = mixing_levels[i];
	}
	config_invalid = 0;
}


/***************************************************************************
	mixer_write_config
***************************************************************************/

void mixer_write_config(void *f)
{
	UINT8 default_levels[MIXER_MAX_CHANNELS];
	UINT8 mixing_levels[MIXER_MAX_CHANNELS];
	int i;

	for (i = 0; i < MIXER_MAX_CHANNELS; i++)
	{
		default_levels[i] = mixer_channel[i].default_mixing_level;
		mixing_levels[i] = mixer_channel[i].mixing_level;
	}
	osd_fwrite(f, default_levels, MIXER_MAX_CHANNELS);
	osd_fwrite(f, mixing_levels, MIXER_MAX_CHANNELS);
}


/***************************************************************************
	mixer_play_streamed_sample_16
***************************************************************************/

void mixer_play_streamed_sample_16(int ch, INT16 *data, int len, int freq)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];
	UINT32 step_size, input_pos, output_pos, samples_mixed;
	INT32 mixing_volume;

	/* skip if sound is off */
	if (Machine->sample_rate == 0)
		return;
	channel->is_stream = 1;

	profiler_mark(PROFILER_MIXER);

	/* compute the overall mixing volume */
	if (mixer_sound_enabled)
		mixing_volume = ((channel->volume * channel->mixing_level * 256) << channel->gain) / (100*100);
	else
		mixing_volume = 0;

	/* compute the step size for sample rate conversion */
	if (freq != channel->frequency)
	{
		channel->frequency = freq;
		channel->step_size = (UINT32)((double)freq * (double)(1 << FRACTION_BITS) / (double)Machine->sample_rate);
	}
	step_size = channel->step_size;

	/* now determine where to mix it */
	input_pos = channel->input_frac;
	output_pos = (accum_base + channel->samples_available) & ACCUMULATOR_MASK;

	/* compute the length in fractional form */
	len = (len / 2) << FRACTION_BITS;
	samples_mixed = 0;

	/* if we're mono or left panning, just mix to the left channel */
	if (!is_stereo || channel->pan == MIXER_PAN_LEFT)
	{
		while (input_pos < len)
		{
			left_accum[output_pos] += (data[input_pos >> FRACTION_BITS] * mixing_volume) >> 8;
			input_pos += step_size;
			output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
			samples_mixed++;
		}
	}

	/* if we're right panning, just mix to the right channel */
	else if (channel->pan == MIXER_PAN_RIGHT)
	{
		while (input_pos < len)
		{
			right_accum[output_pos] += (data[input_pos >> FRACTION_BITS] * mixing_volume) >> 8;
			input_pos += step_size;
			output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
			samples_mixed++;
		}
	}

	/* if we're stereo center, mix to both channels */
	else
	{
		while (input_pos < len)
		{
			INT32 mixing_value = (data[input_pos >> FRACTION_BITS] * mixing_volume) >> 8;
			left_accum[output_pos] += mixing_value;
			right_accum[output_pos] += mixing_value;
			input_pos += step_size;
			output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
			samples_mixed++;
		}
	}

	/* update the final positions */
	channel->input_frac = input_pos & FRACTION_MASK;
	channel->samples_available += samples_mixed;

	profiler_mark(PROFILER_END);
}


/***************************************************************************
	mixer_samples_this_frame
***************************************************************************/

int mixer_samples_this_frame(void)
{
	return samples_this_frame;
}


/***************************************************************************
	mixer_need_samples_this_frame
***************************************************************************/
#define EXTRA_SAMPLES 1    // safety margin for sampling rate conversion
int mixer_need_samples_this_frame(int channel,int freq)
{
	return (samples_this_frame - mixer_channel[channel].samples_available)
			* freq / Machine->sample_rate + EXTRA_SAMPLES;
}


/***************************************************************************
	mixer_play_sample
***************************************************************************/

void mixer_play_sample(int ch, INT8 *data, int len, int freq, int loop)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	/* skip if sound is off, or if this channel is a stream */
	if (Machine->sample_rate == 0 || channel->is_stream)
		return;

	/* update the state of this channel */
	mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));

	/* compute the step size for sample rate conversion */
	if (freq != channel->frequency)
	{
		channel->frequency = freq;
		channel->step_size = (UINT32)((double)freq * (double)(1 << FRACTION_BITS) / (double)Machine->sample_rate);
	}

	/* now determine where to mix it */
	channel->input_frac = 0;
	channel->data_start = data;
	channel->data_current = data;
	channel->data_end = (UINT8 *)data + len;
	channel->is_playing = 1;
	channel->is_looping = loop;
	channel->is_16bit = 0;
}


/***************************************************************************
	mixer_play_sample_16
***************************************************************************/

void mixer_play_sample_16(int ch, INT16 *data, int len, int freq, int loop)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	/* skip if sound is off, or if this channel is a stream */
	if (Machine->sample_rate == 0 || channel->is_stream)
		return;

	/* update the state of this channel */
	mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));

	/* compute the step size for sample rate conversion */
	if (freq != channel->frequency)
	{
		channel->frequency = freq;
		channel->step_size = (UINT32)((double)freq * (double)(1 << FRACTION_BITS) / (double)Machine->sample_rate);
	}

	/* now determine where to mix it */
	channel->input_frac = 0;
	channel->data_start = data;
	channel->data_current = data;
	channel->data_end = (UINT8 *)data + len;
	channel->is_playing = 1;
	channel->is_looping = loop;
	channel->is_16bit = 1;
}


/***************************************************************************
	mixer_stop_sample
***************************************************************************/

void mixer_stop_sample(int ch)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));
	channel->is_playing = 0;
}


/***************************************************************************
	mixer_is_sample_playing
***************************************************************************/

int mixer_is_sample_playing(int ch)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));
	return channel->is_playing;
}


/***************************************************************************
	mixer_set_sample_frequency
***************************************************************************/

void mixer_set_sample_frequency(int ch, int freq)
{
	struct mixer_channel_data *channel = &mixer_channel[ch];

	mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));

	/* compute the step size for sample rate conversion */
	if (freq != channel->frequency)
	{
		channel->frequency = freq;
		channel->step_size = (UINT32)((double)freq * (double)(1 << FRACTION_BITS) / (double)Machine->sample_rate);
	}
}


/***************************************************************************
	mixer_sound_enable_global_w
***************************************************************************/

void mixer_sound_enable_global_w(int enable)
{
	int i;
	struct mixer_channel_data *channel;

	/* update all channels (for streams this is a no-op) */
	for (i = 0, channel = mixer_channel; i < first_free_channel; i++, channel++)
	{
		mixer_update_channel(channel, sound_scalebufferpos(samples_this_frame));
	}

	mixer_sound_enabled = enable;
}


/***************************************************************************
	mix_sample_8
***************************************************************************/

void mix_sample_8(struct mixer_channel_data *channel, int samples_to_generate)
{
	UINT32 step_size, input_frac, output_pos;
	INT8 *source, *source_end;
	INT32 mixing_volume;

	/* compute the overall mixing volume */
	if (mixer_sound_enabled)
		mixing_volume = ((channel->volume * channel->mixing_level * 256) << channel->gain) / (100*100);
	else
		mixing_volume = 0;

	/* get the initial state */
	step_size = channel->step_size;
	source = (INT8*)channel->data_current;
	source_end = (INT8*)channel->data_end;
	input_frac = channel->input_frac;
	output_pos = (accum_base + channel->samples_available) & ACCUMULATOR_MASK;

	/* an outer loop to handle looping samples */
	while (samples_to_generate > 0)
	{
		/* if we're mono or left panning, just mix to the left channel */
		if (!is_stereo || channel->pan == MIXER_PAN_LEFT)
		{
			while (source < source_end && samples_to_generate > 0)
			{
				left_accum[output_pos] += *source * mixing_volume;
				input_frac += step_size;
				source += input_frac >> FRACTION_BITS;
				input_frac &= FRACTION_MASK;
				output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
				samples_to_generate--;
			}
		}

		/* if we're right panning, just mix to the right channel */
		else if (channel->pan == MIXER_PAN_RIGHT)
		{
			while (source < source_end && samples_to_generate > 0)
			{
				right_accum[output_pos] += *source * mixing_volume;
				input_frac += step_size;
				source += input_frac >> FRACTION_BITS;
				input_frac &= FRACTION_MASK;
				output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
				samples_to_generate--;
			}
		}

		/* if we're stereo center, mix to both channels */
		else
		{
			while (source < source_end && samples_to_generate > 0)
			{
				INT32 mixing_value = *source * mixing_volume;
				left_accum[output_pos] += mixing_value;
				right_accum[output_pos] += mixing_value;
				input_frac += step_size;
				source += input_frac >> FRACTION_BITS;
				input_frac &= FRACTION_MASK;
				output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
				samples_to_generate--;
			}
		}

		/* handle the end case */
		if (source >= source_end)
		{
			/* if we're done, stop playing */
			if (!channel->is_looping)
			{
				channel->is_playing = 0;
				break;
			}

			/* if we're looping, wrap to the beginning */
			else
				source -= (INT8 *)source_end - (INT8 *)channel->data_start;
		}
	}

	/* update the final positions */
	channel->input_frac = input_frac;
	channel->data_current = source;
}


/***************************************************************************
	mix_sample_16
***************************************************************************/

void mix_sample_16(struct mixer_channel_data *channel, int samples_to_generate)
{
	UINT32 step_size, input_frac, output_pos;
	INT16 *source, *source_end;
	INT32 mixing_volume;

	/* compute the overall mixing volume */
	if (mixer_sound_enabled)
		mixing_volume = ((channel->volume * channel->mixing_level * 256) << channel->gain) / (100*100);
	else
		mixing_volume = 0;

	/* get the initial state */
	step_size = channel->step_size;
	source = (INT16*)channel->data_current;
	source_end = (INT16*)channel->data_end;
	input_frac = channel->input_frac;
	output_pos = (accum_base + channel->samples_available) & ACCUMULATOR_MASK;

	/* an outer loop to handle looping samples */
	while (samples_to_generate > 0)
	{
		/* if we're mono or left panning, just mix to the left channel */
		if (!is_stereo || channel->pan == MIXER_PAN_LEFT)
		{
			while (source < source_end && samples_to_generate > 0)
			{
				left_accum[output_pos] += (*source * mixing_volume) >> 8;

				input_frac += step_size;
				source += input_frac >> FRACTION_BITS;
				input_frac &= FRACTION_MASK;

				output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
				samples_to_generate--;
			}
		}

		/* if we're right panning, just mix to the right channel */
		else if (channel->pan == MIXER_PAN_RIGHT)
		{
			while (source < source_end && samples_to_generate > 0)
			{
				right_accum[output_pos] += (*source * mixing_volume) >> 8;

				input_frac += step_size;
				source += input_frac >> FRACTION_BITS;
				input_frac &= FRACTION_MASK;

				output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
				samples_to_generate--;
			}
		}

		/* if we're stereo center, mix to both channels */
		else
		{
			while (source < source_end && samples_to_generate > 0)
			{
				INT32 mixing_value = (*source * mixing_volume) >> 8;
				left_accum[output_pos] += mixing_value;
				right_accum[output_pos] += mixing_value;

				input_frac += step_size;
				source += input_frac >> FRACTION_BITS;
				input_frac &= FRACTION_MASK;

				output_pos = (output_pos + 1) & ACCUMULATOR_MASK;
				samples_to_generate--;
			}
		}

		/* handle the end case */
		if (source >= source_end)
		{
			/* if we're done, stop playing */
			if (!channel->is_looping)
			{
				channel->is_playing = 0;
				break;
			}

			/* if we're looping, wrap to the beginning */
			else
				source -= (INT16 *)source_end - (INT16 *)channel->data_start;
		}
	}

	/* update the final positions */
	channel->input_frac = input_frac;
	channel->data_current = source;
}



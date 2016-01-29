#include "driver.h"



static int firstchannel,numchannels;


/* Start one of the samples loaded from disk. Note: channel must be in the range */
/* 0 .. Samplesinterface->channels-1. It is NOT the discrete channel to pass to */
/* mixer_play_sample() */
void sample_start(int channel,int samplenum,int loop)
{
	if (Machine->sample_rate == 0) return;
	if (Machine->samples == 0) return;
	if (Machine->samples->sample[samplenum] == 0) return;
	if (channel >= numchannels)
	{
		logerror("error: sample_start() called with channel = %d, but only %d channels allocated\n",channel,numchannels);
		return;
	}
	if (samplenum >= Machine->samples->total)
	{
		logerror("error: sample_start() called with samplenum = %d, but only %d samples available\n",samplenum,Machine->samples->total);
		return;
	}

	if ( Machine->samples->sample[samplenum]->resolution == 8 )
	{
		//logerror("play 8 bit sample %d, channel %d\n",samplenum,channel);
		mixer_play_sample(firstchannel + channel,
				Machine->samples->sample[samplenum]->data,
				Machine->samples->sample[samplenum]->length,
				Machine->samples->sample[samplenum]->smpfreq,
				loop);
	}
	else
	{
		//logerror("play 16 bit sample %d, channel %d\n",samplenum,channel);
		mixer_play_sample_16(firstchannel + channel,
				(short *) Machine->samples->sample[samplenum]->data,
				Machine->samples->sample[samplenum]->length,
				Machine->samples->sample[samplenum]->smpfreq,
				loop);
	}
}

void sample_set_freq(int channel,int freq)
{
	if (Machine->sample_rate == 0) return;
	if (Machine->samples == 0) return;
	if (channel >= numchannels)
	{
		logerror("error: sample_adjust() called with channel = %d, but only %d channels allocated\n",channel,numchannels);
		return;
	}

	mixer_set_sample_frequency(channel + firstchannel,freq);
}

void sample_set_volume(int channel,int volume)
{
	if (Machine->sample_rate == 0) return;
	if (Machine->samples == 0) return;
	if (channel >= numchannels)
	{
		logerror("error: sample_adjust() called with channel = %d, but only %d channels allocated\n",channel,numchannels);
		return;
	}

	mixer_set_volume(channel + firstchannel,volume * 100 / 255);
}

void sample_stop(int channel)
{
	if (Machine->sample_rate == 0) return;
	if (channel >= numchannels)
	{
		logerror("error: sample_stop() called with channel = %d, but only %d channels allocated\n",channel,numchannels);
		return;
	}

	mixer_stop_sample(channel + firstchannel);
}

int sample_playing(int channel)
{
	if (Machine->sample_rate == 0) return 0;
	if (channel >= numchannels)
	{
		logerror("error: sample_playing() called with channel = %d, but only %d channels allocated\n",channel,numchannels);
		return 0;
	}

	return mixer_is_sample_playing(channel + firstchannel);
}



int samples_sh_start(const struct MachineSound *msound)
{
	int i;
	int vol[MIXER_MAX_CHANNELS];
	const struct Samplesinterface *intf = (const struct Samplesinterface *)msound->sound_interface;

	/* read audio samples if available */
	Machine->samples = readsamples(intf->samplenames,Machine->gamedrv->name);

	numchannels = intf->channels;
	for (i = 0;i < numchannels;i++)
		vol[i] = intf->volume;
	firstchannel = mixer_allocate_channels(numchannels,vol);
	for (i = 0;i < numchannels;i++)
	{
		char buf[40];

		sprintf(buf,"Sample #%d",i);
		mixer_set_name(firstchannel + i,buf);
	}
	return 0;
}

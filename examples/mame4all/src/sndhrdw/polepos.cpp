/***************************************************************************
	polepos.c
	Sound handler
****************************************************************************/
#include "driver.h"

static int sample_msb = 0;
static int sample_lsb = 0;
static int sample_enable = 0;

static int current_position;
static int sound_stream;

/* speech section */
static int channel;
static INT8 *speech;
/* macro to convert 4-bit unsigned samples to 8-bit signed samples */
#define SAMPLE_CONV4(a) (0x11*((a&0x0f))-0x80)
#define SAMPLE_SIZE 0x8000

#define AMP(r)	(r*128/10100)
static int volume_table[8] =
{
	AMP(2200), AMP(3200), AMP(4400), AMP(5400),
	AMP(6900), AMP(7900), AMP(9100), AMP(10100)
};
static int sample_offsets[5];

/************************************/
/* Stream updater                   */
/************************************/
static void engine_sound_update(int num, INT16 *buffer, int length)
{
	UINT32 current = current_position, step, clock, slot, volume;
	UINT8 *base;


	/* if we're not enabled, just fill with 0 */
	if (!sample_enable || Machine->sample_rate == 0)
	{
		memset(buffer, 0, length * sizeof(INT16));
		return;
	}

	/* determine the effective clock rate */
	clock = (Machine->drv->cpu[0].cpu_clock / 64) * ((sample_msb + 1) * 64 + sample_lsb + 1) / (16*64);
	step = (clock << 12) / Machine->sample_rate;

	/* determine the volume */
	slot = (sample_msb >> 3) & 7;
	volume = volume_table[slot];
	base = &memory_region(REGION_SOUND1)[0x1000 + slot * 0x800];

	/* fill in the sample */
	while (length--)
	{
		*buffer++ = (base[(current >> 12) & 0x7ff] * volume);
		current += step;
	}

	current_position = current;
}

/************************************/
/* Sound handler start              */
/************************************/
int polepos_sh_start(const struct MachineSound *msound)
{
	int i, bits, last=0;

	channel = mixer_allocate_channel(25);
	mixer_set_name(channel,"Speech");

	speech = (INT8*)malloc(16*SAMPLE_SIZE);
	if (!speech)
		return 1;

	/* decode the rom samples, interpolating to make it sound a little better */
	for (i = 0;i < SAMPLE_SIZE;i++)
	{
		bits = memory_region(REGION_SOUND1)[0x5000+i] & 0x0f;
		bits = SAMPLE_CONV4(bits);
		speech[16*i+0] = (7 * last + 1 * bits) / 8;
		speech[16*i+1] = (6 * last + 2 * bits) / 8;
		speech[16*i+2] = (5 * last + 3 * bits) / 8;
		speech[16*i+3] = (4 * last + 4 * bits) / 8;
		speech[16*i+4] = (3 * last + 5 * bits) / 8;
		speech[16*i+5] = (2 * last + 6 * bits) / 8;
		speech[16*i+6] = (1 * last + 7 * bits) / 8;
		speech[16*i+7] = bits;
		last = bits;

		bits = (memory_region(REGION_SOUND1)[0x5000+i] & 0xf0) >> 4;
		bits = SAMPLE_CONV4(bits);
		speech[16*i+8] = (7 * last + 1 * bits) / 8;
		speech[16*i+9] = (6 * last + 2 * bits) / 8;
		speech[16*i+10] = (5 * last + 3 * bits) / 8;
		speech[16*i+11] = (4 * last + 4 * bits) / 8;
		speech[16*i+12] = (3 * last + 5 * bits) / 8;
		speech[16*i+13] = (2 * last + 6 * bits) / 8;
		speech[16*i+14] = (1 * last + 7 * bits) / 8;
		speech[16*i+15] = bits;
		last = bits;
	}

	/* Japanese or US PROM? */
	if (memory_region(REGION_SOUND1)[0x5000] == 0)
	{
		/* US */
		sample_offsets[0] = 0x0020;
		sample_offsets[1] = 0x0c00;
		sample_offsets[2] = 0x1c00;
		sample_offsets[3] = 0x2000;
		sample_offsets[4] = 0x2000;
	}
	else
	{
		/* Japan */
		sample_offsets[0] = 0x0020;
		sample_offsets[1] = 0x0900;
		sample_offsets[2] = 0x1f00;
		sample_offsets[3] = 0x4000;
		sample_offsets[4] = 0x6000;		/* How is this triggered? */
	}

	sound_stream = stream_init("Engine Sound", 50, Machine->sample_rate, 0, engine_sound_update);
	current_position = 0;
	sample_msb = sample_lsb = 0;
	sample_enable = 0;
    return 0;
}

/************************************/
/* Sound handler stop               */
/************************************/
void polepos_sh_stop(void)
{
	if (speech)
		free(speech);
	speech = NULL;
}

/************************************/
/* Sound handler update 			*/
/************************************/
void polepos_sh_update(void)
{
}

/************************************/
/* Write LSB of engine sound		*/
/************************************/
WRITE_HANDLER( polepos_engine_sound_lsb_w )
{
	stream_update(sound_stream, 0);
	sample_lsb = data & 62;
    sample_enable = data & 1;
}

/************************************/
/* Write MSB of engine sound		*/
/************************************/
WRITE_HANDLER( polepos_engine_sound_msb_w )
{
	stream_update(sound_stream, 0);
	sample_msb = data & 63;
}

/************************************/
/* Play speech sample				*/
/************************************/
void polepos_sample_play(int sample)
{
	int start = sample_offsets[sample];
	int len = sample_offsets[sample + 1] - start;

	if (Machine->sample_rate == 0)
		return;

	mixer_play_sample(channel, speech + start * 16, len * 16, 4000*8, 0);
}

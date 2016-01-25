/***************************************************************************

								-= Seta Games =-

					driver by	Luca Elia (eliavit@unina.it)


Sound Chip:

	X1-010 					[Seta Custom]
	Unsigned 16 Bit PCM 	[Fixed Per Game Pitch?]
	16 Voices				[There's More Data Written!]

Format:

	8 registers per channel (mapped to the lower bytes of 16 words on the 68K)

	Reg:	Bits:		Meaning:

	0		7654 321-
			---- ---0	Key On / Off

	1		7654 ----	Volume 1(L?)
			---- 3210	Volume 2(R?)

	2					? (high byte?)
	3					? (low  byte?)

	4					Sample Start / 0x1000 			[Start/End in bytes]
	5					0x100 - (Sample End / 0x1000)	[PCM ROM is Max 1MB?]

	6					?
	7					?


Hardcoded Values (for now):

	PCM ROM region:		REGION_SOUND1
	Sample Frequency:	4, 6, 8 KHz

***************************************************************************/
#include "driver.h"

/* Variables and functions that driver has access to */
unsigned char *seta_sound_ram;

#define SETA_NUM_CHANNELS 16

/* Variables only used here */
static int firstchannel, frequency;
static int seta_reg[SETA_NUM_CHANNELS][8];




int seta_sh_start(const struct MachineSound *msound)
{
	int i;
	int vol[MIXER_MAX_CHANNELS];

	for (i = 0;i < MIXER_MAX_CHANNELS;i++)	vol[i] = 100;
	firstchannel = mixer_allocate_channels(SETA_NUM_CHANNELS,vol);

	for (i = 0; i < SETA_NUM_CHANNELS; i++)
	{
		char buf[40];
		sprintf(buf,"X1-010 Channel #%d",i);
		mixer_set_name(firstchannel + i,buf);
	}
	return 0;
}

int seta_sh_start_4KHz(const struct MachineSound *msound)
{
	frequency = 4000;
	return seta_sh_start(msound);
}

int seta_sh_start_6KHz(const struct MachineSound *msound)
{
	frequency = 6000;
	return seta_sh_start(msound);
}

int seta_sh_start_8KHz(const struct MachineSound *msound)
{
	frequency = 8000;
	return seta_sh_start(msound);
}



/* Use these for 8 bit CPUs */


READ_HANDLER( seta_sound_r )
{
	int channel	=	offset / 8;
	int reg		=	offset % 8;

	if (channel < SETA_NUM_CHANNELS)
	{
		switch (reg)
		{
			case 0:
				return ( mixer_is_sample_playing(firstchannel + channel) ? 1 : 0 );
			default:
				//logerror("PC: %06X - X1-010 channel %X, register %X read!\n",cpu_get_pc(),channel,reg);
				return seta_reg[channel][reg];
		}
	}

	return seta_sound_ram[offset];
}




#define DUMP_REGS \
	logerror("X1-010 REGS: ch %X] %02X %02X %02X %02X - %02X %02X %02X %02X\n", \
							channel, \
							seta_reg[channel][0],seta_reg[channel][1], \
							seta_reg[channel][2],seta_reg[channel][3], \
							seta_reg[channel][4],seta_reg[channel][5], \
							seta_reg[channel][6],seta_reg[channel][7] );


WRITE_HANDLER( seta_sound_w )
{
	int channel, reg;

	seta_sound_ram[offset] = data;

	if (Machine->sample_rate == 0)		return;

	channel	=	offset / 8;
	reg		=	offset % 8;

	if (channel >= SETA_NUM_CHANNELS)	return;

	seta_reg[channel][reg] = data & 0xff;

	switch (reg)
	{

		case 0:

			//DUMP_REGS

			if (data & 1)	// key on
			{
				int volume	=	seta_reg[channel][1];

				int start	=	seta_reg[channel][4]           * 0x1000;
				int end		=	(0x100 - seta_reg[channel][5]) * 0x1000; // from the end of the rom

				int len		=	end - start;
				int maxlen	=	memory_region_length(REGION_SOUND1);

				if (!( (start < end) && (end <= maxlen) ))
				{
					//logerror("PC: %06X - X1-010 OUT OF RANGE SAMPLE: %06X - %06X, channel %X\n",cpu_get_pc(),start,end,channel);
					//DUMP_REGS
					return;
				}

#if 1
/* Print some more debug info */
//logerror("PC: %06X - Play 16 bit sample %06X - %06X, channel %X\n",cpu_get_pc(),start, end, channel);
//DUMP_REGS
#endif

				/*
				   Twineagl continuosly writes 1 to reg 0 of the channel, so
				   the sample is restarted every time and never plays to the
				   end. It looks like the previous sample must be explicitly
				   stopped before a new one can be played
				*/
				if ( seta_sound_r(offset) & 1 )	return;	// play to the end

				/* These samples are probaly looped and use the 3rd & 4th register's value */
				if (data & 2)	return;

				/* left and right speaker's volume can be set indipendently.
				   We use a mean volume for now */
				mixer_set_volume(firstchannel + channel, ((volume & 0xf)+(volume >> 4))*100/(2*0xf)  );

				/* I assume the pitch is fixed for a given board. It ranges
				   from 4 to 8 KHz for the games I've seen */

				mixer_play_sample_16(
					firstchannel + channel,
					(short *) (memory_region(REGION_SOUND1) + start),	// start
					len,												// len
					frequency,											// frequency
					0);													// loop
			}
			else
				mixer_stop_sample(channel + firstchannel);

			break;

	}
}





/* Use these for 16 bit CPUs */

READ_HANDLER( seta_sound_word_r )
{
	return seta_sound_r(offset/2) & 0xff;
}

WRITE_HANDLER( seta_sound_word_w )
{
	if ( (data & 0x00ff0000) == 0 )
		seta_sound_w(offset/2, data & 0xff);
}

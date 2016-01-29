/***************************************************************************

Sega G-80 Raster game sound code

Across these games, there's a mixture of discrete sound circuitry,
speech boards, ADPCM samples, and a TMS3617 music chip.

08-JAN-1999 - MAB:
 - added NEC 7751 support to Monster Bash

05-DEC-1998 - MAB:
 - completely rewrote sound code to use Samples interface.
   (It's based on the Zaxxon sndhrdw code.)

TODO:
 - Astro Blaster needs "Attack Rate" modifiers implemented
 - Sample support for 005
 - Melody support for 005
 - Sound for Pig Newton
 - Speech for Astro Blaster

- Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"
#include "cpu/i8039/i8039.h"

#define TOTAL_SOUNDS 16

struct sa
{
	int channel;
	int num;
	int looped;
	int stoppable;
	int restartable;
};

/***************************************************************************
Astro Blaster

Sound is created through two boards.
The Astro Blaster Sound Board consists solely of sounds created through
discrete circuitry.
The G-80 Speech Synthesis Board consists of an 8035 and a speech synthesizer.
***************************************************************************/

/* Special temporary code for Astro Blaster Speech handling */
#define MAX_SPEECH_QUEUE 10
#define NOT_PLAYING     -1              /* The queue holds the sample # so -1 will indicate no sample */
#define SPEECH_CHANNEL 11           /* Note that Astro Blaster sounds only use tracks 0-10 */

static int speechQueue[MAX_SPEECH_QUEUE];
static int speechQueuePtr = 0;
/* End of speech code */

const char *astrob_sample_names[] =
{
	"*astrob",
	"invadr1.wav","winvadr1.wav","invadr2.wav","winvadr2.wav",
	"invadr3.wav","winvadr3.wav","invadr4.wav","winvadr4.wav",
	"asteroid.wav","refuel.wav",

	"pbullet.wav","ebullet.wav","eexplode.wav","pexplode.wav",
	"deedle.wav","sonar.wav",

	"01.wav","02.wav","03.wav","04.wav","05.wav","06.wav","07.wav","08.wav",
	"09.wav","0a.wav","0b.wav","0c.wav","0d.wav","0e.wav","0f.wav","10.wav",
	"11.wav","12.wav","13.wav","14.wav","15.wav","16.wav","17.wav","18.wav",
	"19.wav","1a.wav","1b.wav","1c.wav","1d.wav","1e.wav","1f.wav","20.wav",
	"22.wav","23.wav",
	0
};

enum
{
	invadr1 = 0,winvadr1,invadr2,winvadr2,
	invadr3,winvadr3,invadr4,winvadr4,
	asteroid,refuel,
	pbullet,ebullet,eexplode,pexplode,
	deedle,sonar,
	v01,v02,v03,v04,v05,v06,v07,v08,
	v09,v0a,v0b,v0c,v0d,v0e,v0f,v10,
	v11,v12,v13,v14,v15,v16,v17,v18,
	v19,v1a,v1b,v1c,v1d,v1e,v1f,v20,
	v22,v23
};

static struct sa astrob_sa[TOTAL_SOUNDS] =
{
	/* Port 0x3E: */
	{  0, invadr1,  1, 1, 1 },      /* Line  0 - Invader 1 */
	{  1, invadr2,  1, 1, 1 },      /* Line  1 - Invader 2 */
	{  2, invadr3,  1, 1, 1 },      /* Line  2 - Invader 3 */
	{  3, invadr4,  1, 1, 1 },      /* Line  3 - Invader 4 */
	{  4, asteroid, 1, 1, 1 },      /* Line  4 - Asteroids */
	{ -1 },                                         /* Line  5 - <Mute> */
	{  5, refuel,   0, 1, 0 },      /* Line  6 - Refill */
	{ -1 },                                         /* Line  7 - <Warp Modifier> */

	/* Port 0x3F: */
	{  6, pbullet,  0, 0, 1 },      /* Line  0 - Laser #1 */
	{  7, ebullet,  0, 0, 1 },      /* Line  1 - Laser #2 */
	{  8, eexplode, 0, 0, 1 },      /* Line  2 - Short Explosion */
	{  8, pexplode, 0, 0, 1 },      /* Line  3 - Long Explosion */
	{ -1 },                                         /* Line  4 - <Attack Rate> */
	{ -1 },                                         /* Line  5 - <Rate Reset> */
	{  9, deedle,   0, 0, 1 },      /* Line  6 - Bonus */
	{ 10, sonar,    0, 0, 1 },      /* Line  7 - Sonar */
};

/* Special speech handling code.  Someday this will hopefully be
   replaced with true speech synthesis. */
int astrob_speech_sh_start(const struct MachineSound *msound)
{
   int i;

   for (i = 0; i < MAX_SPEECH_QUEUE; i++)
   {
	  speechQueue[i] = NOT_PLAYING;
   }
   speechQueuePtr = 0;

   return 0;
}

void astrob_speech_sh_update (void)
{
	int sound;

	if( Machine->samples == 0 )
		return;

	if (speechQueue[speechQueuePtr] != NOT_PLAYING)
	{
		sound = speechQueue[speechQueuePtr];

		if (!sample_playing(SPEECH_CHANNEL))
		{
			if (Machine->samples->sample[sound])
				sample_start(SPEECH_CHANNEL, sound, 0);
			speechQueue[speechQueuePtr] = NOT_PLAYING;
			speechQueuePtr = ((speechQueuePtr + 1) % MAX_SPEECH_QUEUE);
		}
	}
}


static void astrob_queue_speech(int sound)
{
	int newPtr;

	/* Queue the new sound */
	newPtr = speechQueuePtr;
	while (speechQueue[newPtr] != NOT_PLAYING)
	{
		newPtr = ((newPtr + 1) % MAX_SPEECH_QUEUE);
		if (newPtr == speechQueuePtr)
		{
			 /* The queue has overflowed. Oops. */
			//logerror("*** Speech queue overflow!\n");
			return;
		}
	}
	speechQueue[newPtr] = sound;
}

WRITE_HANDLER( astrob_speech_port_w )
{
	if( Machine->samples == 0 )
		return;

	switch (data)
	{
		case 0x01:      astrob_queue_speech(v01); break;
		case 0x02:      astrob_queue_speech(v02); break;
		case 0x03:      astrob_queue_speech(v03); break;
		case 0x04:      astrob_queue_speech(v04); break;
		case 0x05:      astrob_queue_speech(v05); break;
		case 0x06:      astrob_queue_speech(v06); break;
		case 0x07:      astrob_queue_speech(v07); break;
		case 0x08:      astrob_queue_speech(v08); break;
		case 0x09:      astrob_queue_speech(v09); break;
		case 0x0a:      astrob_queue_speech(v0a); break;
		case 0x0b:      astrob_queue_speech(v0b); break;
		case 0x0c:      astrob_queue_speech(v0c); break;
		case 0x0d:      astrob_queue_speech(v0d); break;
		case 0x0e:      astrob_queue_speech(v0e); break;
		case 0x0f:      astrob_queue_speech(v0f); break;
		case 0x10:      astrob_queue_speech(v10); break;
		case 0x11:      astrob_queue_speech(v11); break;
		case 0x12:      astrob_queue_speech(v12); break;
		case 0x13:      astrob_queue_speech(v13); break;
		case 0x14:      astrob_queue_speech(v14); break;
		case 0x15:      astrob_queue_speech(v15); break;
		case 0x16:      astrob_queue_speech(v16); break;
		case 0x17:      astrob_queue_speech(v17); break;
		case 0x18:      astrob_queue_speech(v18); break;
		case 0x19:      astrob_queue_speech(v19); break;
		case 0x1a:      astrob_queue_speech(v1a); break;
		case 0x1b:      astrob_queue_speech(v1b); break;
		case 0x1c:      astrob_queue_speech(v1c); break;
		case 0x1d:      astrob_queue_speech(v1d); break;
		case 0x1e:      astrob_queue_speech(v1e); break;
		case 0x1f:      astrob_queue_speech(v1f); break;
		case 0x20:      astrob_queue_speech(v20); break;
		case 0x22:      astrob_queue_speech(v22); break;
		case 0x23:      astrob_queue_speech(v23); break;
	}
}

WRITE_HANDLER( astrob_audio_ports_w )
{
	int line;
	int noise;
	int warp = 0;

	/* First, handle special control lines: */

	/* MUTE */
	if ((offset == 0) && (data & 0x20))
	{
		/* Note that this also stops our speech from playing. */
		/* (If our speech ever gets synthesized, this will probably
		   need to call some type of speech_mute function) */
		for (noise = 0; noise < TOTAL_SOUNDS; noise++)
			sample_stop(astrob_sa[noise].channel);
		return;
	}

	/* WARP */
	if ((offset == 0) && (!(data & 0x80)))
	{
		warp = 1;
	}

	/* ATTACK RATE */
	if ((offset == 1) && (!(data & 0x10)))
	{
		/* TODO: this seems to modify the speed of the invader sounds */
	}

	/* RATE RESET */
	if ((offset == 1) && (!(data & 0x20)))
	{
		/* TODO: this seems to modify the speed of the invader sounds */
	}

	/* Now, play our discrete sounds */
	for (line = 0;line < 8;line++)
	{
		noise = 8 * offset + line;

		if (astrob_sa[noise].channel != -1)
		{
			/* trigger sound */
			if ((data & (1 << line)) == 0)
			{
				/* Special case: If we're on Invaders sounds, modify with warp */
				if ((astrob_sa[noise].num >= invadr1) &&
					(astrob_sa[noise].num <= invadr4))
				{
					if (astrob_sa[noise].restartable || !sample_playing(astrob_sa[noise].channel))
						sample_start(astrob_sa[noise].channel, astrob_sa[noise].num + warp, astrob_sa[noise].looped);
				}
				/* All other sounds are normal */
				else
				{
					if (astrob_sa[noise].restartable || !sample_playing(astrob_sa[noise].channel))
						sample_start(astrob_sa[noise].channel, astrob_sa[noise].num, astrob_sa[noise].looped);
				}
			}
			else
			{
				if (sample_playing(astrob_sa[noise].channel) && astrob_sa[noise].stoppable)
					sample_stop(astrob_sa[noise].channel);
			}
		}
	}
}

/***************************************************************************
005

The Sound Board seems to consist of the following:
An 8255:
  Port A controls the sounds that use discrete circuitry
    A0 - Large Expl. Sound Trig
    A1 - Small Expl. Sound Trig
    A2 - Drop Sound Bomb Trig
    A3 - Shoot Sound Pistol Trig
    A4 - Missile Sound Trig
    A5 - Helicopter Sound Trig
    A6 - Whistle Sound Trig
    A7 - <unused>
  Port B controls the melody generator (described below)
    B0-B3 - connected to addr lines A7-A10 of the 2716 sound ROM
    B4 - connected to 1CL and 2CL on the LS393, and to RD on an LS293 just before output
    B5-B6 - connected to 1CK on the LS393
    B7 - <unused>
  Port C is apparently unused

Melody Generator:
	There's an LS393 counter hooked to addr lines A0-A6 of a 2716 ROM.
	Port B from the 8255 hooks to addr lines A7-A10 of the 2716.
	D0-D4 output from the 2716 into an 6331.
	D5 outputs from the 2716 to a 555 timer.
	D0-D7 on the 6331 output to two LS161s.
	The LS161s output to the LS293 (also connected to 8255 B4).
	The output of this feeds into a 4391, which produces "MELODY" output.
***************************************************************************/

const char *s005_sample_names[] =
{
		0
};

/***************************************************************************
Space Odyssey

The Sound Board consists solely of sounds created through discrete circuitry.
***************************************************************************/

const char *spaceod_sample_names[] =
{
		"fire.wav", "bomb.wav", "eexplode.wav", "pexplode.wav",
		"warp.wav", "birth.wav", "scoreup.wav", "ssound.wav",
		"accel.wav", "damaged.wav", "erocket.wav",
		0
};

enum
{
		sofire = 0,sobomb,soeexplode,sopexplode,
		sowarp,sobirth,soscoreup,sossound,
		soaccel,sodamaged,soerocket
};

static struct sa spaceod_sa[TOTAL_SOUNDS] =
{
		/* Port 0x0E: */
//      {  0, sossound,   1, 1, 1 },    /* Line  0 - background noise */
		{ -1 },                                                 /* Line  0 - background noise */
		{ -1 },                                                 /* Line  1 - unused */
		{  1, soeexplode, 0, 0, 1 },    /* Line  2 - Short Explosion */
		{ -1 },                                                 /* Line  3 - unused */
		{  2, soaccel,    0, 0, 1 },    /* Line  4 - Accelerate */
		{  3, soerocket,  0, 0, 1 },    /* Line  5 - Battle Star */
		{  4, sobomb,     0, 0, 1 },    /* Line  6 - D. Bomb */
		{  5, sopexplode, 0, 0, 1 },    /* Line  7 - Long Explosion */
		/* Port 0x0F: */
		{  6, sofire,     0, 0, 1 },    /* Line  0 - Shot */
		{  7, soscoreup,  0, 0, 1 },    /* Line  1 - Bonus Up */
		{ -1 },                                                 /* Line  2 - unused */
		{  8, sowarp,     0, 0, 1 },    /* Line  3 - Warp */
		{ -1 },                                                 /* Line  4 - unused */
		{ -1 },                                                 /* Line  5 - unused */
		{  9, sobirth,    0, 0, 1 },    /* Line  6 - Appearance UFO */
		{ 10, sodamaged,  0, 0, 1 },    /* Line  7 - Black Hole */
};

WRITE_HANDLER( spaceod_audio_ports_w )
{
		int line;
		int noise;

		for (line = 0;line < 8;line++)
		{
				noise = 8 * offset + line;

				if (spaceod_sa[noise].channel != -1)
				{
						/* trigger sound */
						if ((data & (1 << line)) == 0)
						{
								if (spaceod_sa[noise].restartable || !sample_playing(spaceod_sa[noise].channel))
										sample_start(spaceod_sa[noise].channel, spaceod_sa[noise].num, spaceod_sa[noise].looped);
						}
						else
						{
								if (sample_playing(spaceod_sa[noise].channel) && spaceod_sa[noise].stoppable)
										sample_stop(spaceod_sa[noise].channel);
						}
				}
		}
}

/***************************************************************************
Monster Bash

The Sound Board is a fairly complex mixture of different components.
An 8255A-5 controls the interface to/from the sound board.
Port A connects to a TMS3617 (basic music synthesizer) circuit.
Port B connects to two sounds generated by discrete circuitry.
Port C connects to a NEC7751 (8048 CPU derivative) to control four "samples".
***************************************************************************/

static unsigned int port_8255_c03 = 0;
static unsigned int port_8255_c47 = 0;
static unsigned int port_7751_p27 = 0;
static unsigned int rom_offset = 0;

const char *monsterb_sample_names[] =
{
		"zap.wav","jumpdown.wav",
		0
};

enum
{
		mbzap = 0,mbjumpdown
};


/* Monster Bash uses an 8255 to control the sounds, much like Zaxxon */
WRITE_HANDLER( monsterb_audio_8255_w )
{
	/* Port A controls the special TMS3617 music chip */
	if (offset == 0)
	{
		int enable_val;

		/* Lower four data lines get decoded into 13 control lines */
		tms36xx_note_w(0, 0, data & 15);

		/* Top four data lines address an 82S123 ROM that enables/disables voices */
		enable_val = memory_region(REGION_SOUND2)[(data & 0xF0) >> 4];
		tms3617_enable_w(0, enable_val >> 2);
	}
	/* Port B controls the two discrete sound circuits */
	else if (offset == 1)
	{
		if (!(data & 0x01))
			sample_start(0, mbzap, 0);

		if (!(data & 0x02))
			sample_start(1, mbjumpdown, 0);

        /* TODO: D7 on Port B might affect TMS3617 output (mute?) */
	}
	/* Port C controls a NEC7751, which is an 8048 CPU with onboard ROM */
	else if (offset == 2)
	{
		// D0-D2 = P24-P26, D3 = INT
		port_8255_c03 = data & 0x0F;
		if ((data & 0x08) == 0)
			cpu_cause_interrupt(1,I8039_EXT_INT);

	}
	/* Write to 8255 control port, this should be 0x80 for "simple mode" */
	/*else
	{
		if (data != 0x80)
			logerror("8255 Control Port Write = %02X\n",data);
	}*/
}

READ_HANDLER( monsterb_audio_8255_r )
{
	// Only PC4 is hooked up
	/* 0x00 = BUSY, 0x10 = NOT BUSY */
	return (port_8255_c47 & 0x10);
}

/* read from BUS */
READ_HANDLER( monsterb_sh_rom_r )
{
	unsigned char *sound_rom = memory_region(REGION_SOUND1);

	return sound_rom[rom_offset];
}

/* read from T1 */
READ_HANDLER( monsterb_sh_t1_r )
{
	// Labelled as "TEST", connected to ground
	return 0;
}

/* read from P2 */
READ_HANDLER( monsterb_sh_command_r )
{
	// 8255's PC0-2 connects to 7751's S0-2 (P24-P26 on an 8048)
	return ((port_8255_c03 & 0x07) << 4) | port_7751_p27;
}

/* write to P1 */
WRITE_HANDLER( monsterb_sh_dac_w )
{
	DAC_data_w(0,data);
}

/* write to P2 */
WRITE_HANDLER( monsterb_sh_busy_w )
{
	// 8255's PC0-2 connects to 7751's S0-2 (P24-P26 on an 8048)
	// 8255's PC4 connects to 7751's BSY OUT (P27 on an 8048)
	port_8255_c03 = (data & 0x70) >> 4;
	port_8255_c47 = (data & 0x80) >> 3;
	port_7751_p27 = data & 0x80;
}

/* write to P4 */
WRITE_HANDLER( monsterb_sh_offset_a0_a3_w )
{
	rom_offset = (rom_offset & 0x1FF0) | (data & 0x0F);
}

/* write to P5 */
WRITE_HANDLER( monsterb_sh_offset_a4_a7_w )
{
	rom_offset = (rom_offset & 0x1F0F) | ((data & 0x0F) << 4);
}

/* write to P6 */
WRITE_HANDLER( monsterb_sh_offset_a8_a11_w )
{
	rom_offset = (rom_offset & 0x10FF) | ((data & 0x0F) << 8);
}

/* write to P7 */
WRITE_HANDLER( monsterb_sh_rom_select_w )
{
	rom_offset = (rom_offset & 0x0FFF);

	/* D0 = !ROM1 enable, D1 = !ROM2 enable, D2/3 hit empty sockets. */
	if ((data & 0x02) == 0)
		rom_offset |= 0x1000;
}


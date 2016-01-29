#include "../vidhrdw/cyberbal.cpp"

/***************************************************************************

Cyberball Memory Map
--------------------

CYBERBALL 68000 MEMORY MAP

Function                           Address        R/W  DATA
-------------------------------------------------------------
Program ROM                        000000-07FFFF  R    D0-D15

Switch 1 (Player 1)                FC0000         R    D0-D7
Action                                            R    D5
Step (Development Only)                           R    D4
Joystick Left                                     R    D3
Joystick Right                                    R    D2
Joystick Down                                     R    D1
Joystick Up                                       R    D0

Switch 2 (Player 2)                FC2000         R    D0-D7
Action                                            R    D5
Step (Development Only)                           R    D4
Joystick Left                                     R    D3
Joystick Right                                    R    D2
Joystick Down                                     R    D1
Joystick Up                                       R    D0

Self-Test (Active Low)             FC4000         R    D7
Vertical Blank                                    R    D6
Audio Busy Flag (Active Low)                      R    D5

Audio Receive Port                 FC6000         R    D8-D15

EEPROM                             FC8000-FC8FFE  R/W  D0-D7

Color RAM                          FCA000-FCAFFE  R/W  D0-D15

Unlock EEPROM                      FD0000         W    xx
Sound Processor Reset              FD2000         W    xx
Watchdog reset                     FD4000         W    xx
IRQ Acknowledge                    FD6000         W    xx
Audio Send Port                    FD8000         W    D8-D15

Playfield RAM                      FF0000-FF1FFF  R/W  D0-D15
Alpha RAM                          FF2000-FF2FFF  R/W  D0-D15
Motion Object RAM                  FF3000-FF3FFF  R/W  D0-D15
RAM                                FF4000-FFFFFF  R/W

****************************************************************************/


#include "driver.h"
#include "cpu/m6502/m6502.h"
#include "sound/adpcm.h"
#include "machine/atarigen.h"
#include "sndhrdw/atarijsa.h"
#include "vidhrdw/generic.h"

#include <math.h>


/* better to leave this on; otherwise, you end up playing entire games out of the left speaker */
#define USE_MONO_SOUND	1

/* don't use this; it's incredibly slow (10k interrupts/second!) and doesn't really work */
/* it's left in primarily for documentation purposes */
/*#define EMULATE_SOUND_68000 1*/


void cyberbal_set_screen(int which);

WRITE_HANDLER( cyberbal_playfieldram_1_w );
WRITE_HANDLER( cyberbal_playfieldram_2_w );

WRITE_HANDLER( cyberbal_paletteram_1_w );
READ_HANDLER( cyberbal_paletteram_1_r );
WRITE_HANDLER( cyberbal_paletteram_2_w );
READ_HANDLER( cyberbal_paletteram_2_r );

int cyberbal_vh_start(void);
void cyberbal_vh_stop(void);
void cyberbal_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);

void cyberbal_scanline_update(int param);

extern UINT8 *cyberbal_playfieldram_1;
extern UINT8 *cyberbal_playfieldram_2;



/* internal prototypes and variables */
//static void update_sound_68k_interrupts(void);
static void handle_68k_sound_command(int data);

static UINT8 *bank_base;
static UINT8 fast_68k_int, io_68k_int;
static UINT8 sound_data_from_68k, sound_data_from_6502;
static UINT8 sound_data_from_68k_ready, sound_data_from_6502_ready;



/*************************************
 *
 *	Initialization
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate1 = 0;
	int newstate2 = 0;
	int temp;

	if (atarigen_sound_int_state)
		newstate1 |= 1;
	if (atarigen_video_int_state)
		newstate2 |= 1;

	if (newstate1)
		cpu_set_irq_line(0, newstate1, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);

	if (newstate2)
		cpu_set_irq_line(2, newstate2, ASSERT_LINE);
	else
		cpu_set_irq_line(2, 7, CLEAR_LINE);

	/* check for screen swapping */
	temp = input_port_2_r(0);
	if (temp & 1) cyberbal_set_screen(0);
	else if (temp & 2) cyberbal_set_screen(1);
}


static void init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_slapstic_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(cyberbal_scanline_update, 8);
	atarigen_sound_io_reset(1);

	/* reset the sound system */
	bank_base = &memory_region(REGION_CPU2)[0x10000];
	cpu_setbank(8, &bank_base[0x0000]);
	fast_68k_int = io_68k_int = 0;
	sound_data_from_68k = sound_data_from_6502 = 0;
	sound_data_from_68k_ready = sound_data_from_6502_ready = 0;

	/* CPU 2 doesn't run until reset */
	cpu_set_reset_line(2,ASSERT_LINE);

	/* make sure we're pointing to the right screen by default */
	cyberbal_set_screen(0);
}


static void cyberb2p_update_interrupts(void)
{
	int newstate = 0;

	if (atarigen_video_int_state)
		newstate |= 1;
	if (atarigen_sound_int_state)
		newstate |= 3;

	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static void cyberb2p_init_machine(void)
{
	atarigen_eeprom_reset();
	atarigen_interrupt_reset(cyberb2p_update_interrupts);
	atarigen_scanline_timer_reset(cyberbal_scanline_update, 8);
	atarijsa_reset();

	/* make sure we're pointing to the only screen */
	cyberbal_set_screen(0);
}



/*************************************
 *
 *	I/O read dispatch.
 *
 *************************************/

static READ_HANDLER( special_port0_r )
{
	int temp = input_port_0_r(offset);
	if (atarigen_cpu_to_sound_ready) temp ^= 0x0080;
	return temp;
}


static READ_HANDLER( special_port2_r )
{
	int temp = input_port_2_r(offset);
	if (atarigen_cpu_to_sound_ready) temp ^= 0x2000;
	return temp;
}


static READ_HANDLER( sound_state_r )
{
	int temp = 0xffff;

	(void)offset;
	if (atarigen_cpu_to_sound_ready) temp ^= 0xffff;
	return temp;
}



/*************************************
 *
 *	Extra I/O handlers.
 *
 *************************************/

static WRITE_HANDLER( p2_reset_w )
{
	(void)offset;
	(void)data;
	cpu_set_reset_line(2,CLEAR_LINE);
}



/*************************************
 *
 *	6502 Sound Interface
 *
 *************************************/

static READ_HANDLER( special_port3_r )
{
	int temp = input_port_3_r(offset);
	if (!(readinputport(0) & 0x8000)) temp ^= 0x80;
	if (atarigen_cpu_to_sound_ready) temp ^= 0x40;
	if (atarigen_sound_to_cpu_ready) temp ^= 0x20;
	return temp;
}


static READ_HANDLER( sound_6502_stat_r )
{
	int temp = 0xff;

	(void)offset;
	if (sound_data_from_6502_ready) temp ^= 0x80;
	if (sound_data_from_68k_ready) temp ^= 0x40;
	return temp;
}


static WRITE_HANDLER( sound_bank_select_w )
{
	(void)offset;
	cpu_setbank(8, &bank_base[0x1000 * ((data >> 6) & 3)]);
}


static READ_HANDLER( sound_68k_6502_r )
{
	(void)offset;
	sound_data_from_68k_ready = 0;
	return sound_data_from_68k;
}


static WRITE_HANDLER( sound_68k_6502_w )
{
	(void)offset;
	sound_data_from_6502 = data;
	sound_data_from_6502_ready = 1;

#ifdef EMULATE_SOUND_68000
	if (!io_68k_int)
	{
		io_68k_int = 1;
		update_sound_68k_interrupts();
	}
#else
	handle_68k_sound_command(data);
#endif
}



/*************************************
 *
 *	68000 Sound Interface
 *
 *************************************/

/*static void update_sound_68k_interrupts(void)
{
#ifdef EMULATE_SOUND_68000
	int newstate = 0;

	if (fast_68k_int)
		newstate |= 6;
	if (io_68k_int)
		newstate |= 2;

	if (newstate)
		cpu_set_irq_line(3, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(3, 7, CLEAR_LINE);
#endif
}*/


/*static int sound_68k_irq_gen(void)
{
	if (!fast_68k_int)
	{
		fast_68k_int = 1;
		update_sound_68k_interrupts();
	}
	return 0;
}*/


/*static WRITE_HANDLER( io_68k_irq_ack_w )
{
	(void)offset;
	(void)data;
	if (io_68k_int)
	{
		io_68k_int = 0;
		update_sound_68k_interrupts();
	}
}*/


static READ_HANDLER( sound_68k_r )
{
	int temp = (sound_data_from_6502 << 8) | 0xff;

	(void)offset;
	sound_data_from_6502_ready = 0;

	if (sound_data_from_6502_ready) temp ^= 0x08;
	if (sound_data_from_68k_ready) temp ^= 0x04;
	return temp;
}


static WRITE_HANDLER( sound_68k_w )
{
	(void)offset;
	if (!(data & 0xff000000))
	{
		sound_data_from_68k = (data >> 8) & 0xff;
		sound_data_from_68k_ready = 1;
	}
}


/*static WRITE_HANDLER( sound_68k_dac_w )
{
	DAC_signed_data_w((offset >> 4) & 1, (INT16)data >> 8);

	if (fast_68k_int)
	{
		fast_68k_int = 0;
		update_sound_68k_interrupts();
	}
}*/


/*************************************
 *
 *	68000 Sound Simulator
 *
 *************************************/

#define SAMPLE_RATE 10000

struct sound_descriptor
{
	/*00*/UINT16 start_address_h;
	/*02*/UINT16 start_address_l;
	/*04*/UINT16 end_address_h;
	/*06*/UINT16 end_address_l;
	/*08*/UINT16 reps;
	/*0a*/INT16 volume;
	/*0c*/INT16 delta_volume;
	/*0e*/INT16 target_volume;
	/*10*/UINT16 voice_priority;	/* voice high, priority low */
	/*12*/UINT16 buffer_number;		/* buffer high, number low */
	/*14*/UINT16 continue_unused;	/* continue high, unused low */
};

struct voice_descriptor
{
	UINT8 playing;
	UINT8 *start;
	UINT8 *current;
	UINT8 *end;
	UINT16 reps;
	INT16 volume;
	INT16 delta_volume;
	INT16 target_volume;
	UINT8 priority;
	UINT8 number;
	UINT8 buffer;
	UINT8 cont;
	INT16 chunk[48];
	UINT8 chunk_remaining;
};


static INT16 *volume_table;
static struct voice_descriptor voices[6];
static UINT8 sound_enabled;
static int stream_channel;


static void decode_chunk(UINT8 *memory, INT16 *output, int overall)
{
	UINT16 volume_bits = READ_WORD(memory);
	int volume, i, j;

	memory += 2;
	for (i = 0; i < 3; i++)
	{
		/* get the volume */
		volume = ((overall & 0x03e0) + (volume_bits & 0x3e0)) >> 1;
		volume_bits = ((volume_bits >> 5) & 0x07ff) | ((volume_bits << 11) & 0xf800);

		for (j = 0; j < 4; j++)
		{
			UINT16 data = READ_WORD(memory);
			memory += 2;
			*output++ = volume_table[volume | ((data >>  0) & 0x000f)];
			*output++ = volume_table[volume | ((data >>  4) & 0x000f)];
			*output++ = volume_table[volume | ((data >>  8) & 0x000f)];
			*output++ = volume_table[volume | ((data >> 12) & 0x000f)];
		}
	}
}


static void sample_stream_update(int param, INT16 **buffer, int length)
{
	INT16 *buf_left = buffer[0];
	INT16 *buf_right = buffer[1];
	int i;

	(void)param;

	/* reset the buffers so we can add into them */
	memset(buf_left, 0, length * sizeof(INT16));
	memset(buf_right, 0, length * sizeof(INT16));

	/* loop over voices */
	for (i = 0; i < 6; i++)
	{
		struct voice_descriptor *voice = &voices[i];
		int left = length;
		INT16 *output;

		/* bail if not playing */
		if (!voice->playing || !voice->buffer)
			continue;

		/* pick a buffer */
		output = (voice->buffer == 0x10) ? buf_left : buf_right;

		/* loop until we're done */
		while (left)
		{
			INT16 *source;
			int this_batch;

			if (!voice->chunk_remaining)
			{
				/* loop if necessary */
				if (voice->current >= voice->end)
				{
					if (--voice->reps == 0)
					{
						voice->playing = 0;
						break;
					}
					voice->current = voice->start;
				}

				/* decode this chunk */
				decode_chunk(voice->current, voice->chunk, voice->volume);
				voice->current += 26;
				voice->chunk_remaining = 48;

				/* update volumes */
				voice->volume += voice->delta_volume;
				if ((voice->volume & 0xffe0) == (voice->target_volume & 0xffe0))
					voice->delta_volume = 0;
			}

			/* determine how much to copy */
			this_batch = (left > voice->chunk_remaining) ? voice->chunk_remaining : left;
			source = voice->chunk + 48 - voice->chunk_remaining;
			voice->chunk_remaining -= this_batch;
			left -= this_batch;

			while (this_batch--)
				*output++ += *source++;
		}
	}
}


static int samples_start(const struct MachineSound *msound)
{
	const char *names[] =
	{
		"68000 Simulator left",
		"68000 Simulator right"
	};
	int vol[2], i, j;

	(void)msound;

	/* allocate volume table */
	volume_table = (INT16*)malloc(sizeof(INT16) * 64 * 16);
	if (!volume_table)
		return 1;

	/* build the volume table */
	for (j = 0; j < 64; j++)
	{
		float factor = pow(0.5, (float)j * 0.25);
		for (i = 0; i < 16; i++)
			volume_table[j * 16 + i] = (INT16)(factor * (float)((INT16)(i << 12)));
	}

	/* get stream channels */
#if USE_MONO_SOUND
	vol[0] = MIXER(50, MIXER_PAN_CENTER);
	vol[1] = MIXER(50, MIXER_PAN_CENTER);
#else
	vol[0] = MIXER(100, MIXER_PAN_LEFT);
	vol[1] = MIXER(100, MIXER_PAN_RIGHT);
#endif
	stream_channel = stream_init_multi(2, names, vol, SAMPLE_RATE, 0, sample_stream_update);

	/* reset voices */
	memset(voices, 0, sizeof(voices));
	sound_enabled = 1;

	return 0;
}


static void samples_stop(void)
{
	if (volume_table)
		free(volume_table);
	volume_table = NULL;
}


static void handle_68k_sound_command(int command)
{
	struct sound_descriptor *sound;
	struct voice_descriptor *voice;
	UINT16 offset;
	int actual_delta, actual_volume;
	int temp;

	/* read the data to reset the latch */
	sound_68k_r(0);

	switch (command)
	{
		case 0:		/* reset */
			break;

		case 1:		/* self-test */
			sound_68k_w(0, 0x40 << 8);
			break;

		case 2:		/* status */
			sound_68k_w(0, 0x00 << 8);
			break;

		case 3:
			sound_enabled = 0;
			break;

		case 4:
			sound_enabled = 1;
			break;

		default:
			/* bail if we're not enabled or if we get a bogus voice */
			offset = READ_WORD(&memory_region(REGION_CPU4)[0x1e2a + 2 * command]);
			sound = (struct sound_descriptor *)&memory_region(REGION_CPU4)[offset];

			/* check the voice */
			temp = sound->voice_priority >> 8;
			if (!sound_enabled || temp > 5)
				break;
			voice = &voices[temp];

			/* see if we're allowed to take over */
			actual_volume = sound->volume;
			actual_delta = sound->delta_volume;
			if (voice->playing && voice->cont)
			{
				temp = sound->buffer_number & 0xff;
				if (voice->number != temp)
					break;

				/* if we're ramping, adjust for the current volume */
				if (actual_delta != 0)
				{
					actual_volume = voice->volume;
					if ((actual_delta < 0 && voice->volume <= sound->target_volume) ||
						(actual_delta > 0 && voice->volume >= sound->target_volume))
						actual_delta = 0;
				}
			}
			else if (voice->playing)
			{
				temp = sound->voice_priority & 0xff;
				if (voice->priority > temp ||
					(voice->priority == temp && (temp & 1) == 0))
					break;
			}

			/* fill in the voice; we're taking over */
			voice->playing = 1;
			voice->start = &memory_region(REGION_CPU4)[(sound->start_address_h << 16) | sound->start_address_l];
			voice->current = voice->start;
			voice->end = &memory_region(REGION_CPU4)[(sound->end_address_h << 16) | sound->end_address_l];
			voice->reps = sound->reps;
			voice->volume = actual_volume;
			voice->delta_volume = actual_delta;
			voice->target_volume = sound->target_volume;
			voice->priority = sound->voice_priority & 0xff;
			voice->number = sound->buffer_number & 0xff;
			voice->buffer = sound->buffer_number >> 8;
			voice->cont = sound->continue_unused >> 8;
			voice->chunk_remaining = 0;
			break;
	}
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress main_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xfc0000, 0xfc03ff, atarigen_eeprom_r },
	{ 0xfc8000, 0xfcffff, atarigen_sound_upper_r },
	{ 0xfe0000, 0xfe0fff, special_port0_r },
	{ 0xfe1000, 0xfe1fff, input_port_1_r },
	{ 0xfe8000, 0xfe8fff, cyberbal_paletteram_2_r },
	{ 0xfec000, 0xfecfff, cyberbal_paletteram_1_r },
	{ 0xff0000, 0xff1fff, MRA_BANK1 },
	{ 0xff2000, 0xff3fff, MRA_BANK2 },
	{ 0xff4000, 0xff5fff, MRA_BANK3 },
	{ 0xff6000, 0xff7fff, MRA_BANK4 },
	{ 0xff8000, 0xff9fff, MRA_BANK5 },
	{ 0xffa000, 0xffbfff, MRA_BANK6 },
	{ 0xffc000, 0xffffff, MRA_BANK7 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress main_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xfc0000, 0xfc03ff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0xfd0000, 0xfd1fff, atarigen_eeprom_enable_w },
	{ 0xfd2000, 0xfd3fff, atarigen_sound_reset_w },
	{ 0xfd4000, 0xfd5fff, watchdog_reset_w },
	{ 0xfd6000, 0xfd7fff, p2_reset_w },
	{ 0xfd8000, 0xfd9fff, atarigen_sound_upper_w },
	{ 0xfe8000, 0xfe8fff, cyberbal_paletteram_2_w, &paletteram_2 },
	{ 0xfec000, 0xfecfff, cyberbal_paletteram_1_w, &paletteram },
	{ 0xff0000, 0xff1fff, cyberbal_playfieldram_2_w },
	{ 0xff2000, 0xff3fff, MWA_BANK2 },
	{ 0xff4000, 0xff5fff, cyberbal_playfieldram_1_w },
	{ 0xff6000, 0xff7fff, MWA_BANK4 },
	{ 0xff8000, 0xff9fff, MWA_BANK5 },
	{ 0xffa000, 0xffbfff, MWA_NOP },
	{ 0xffc000, 0xffffff, MWA_BANK7 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Extra CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress extra_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xfe0000, 0xfe0fff, special_port0_r },
	{ 0xfe1000, 0xfe1fff, input_port_1_r },
	{ 0xfe8000, 0xfe8fff, cyberbal_paletteram_2_r },
	{ 0xfec000, 0xfecfff, cyberbal_paletteram_1_r },
	{ 0xff0000, 0xff1fff, MRA_BANK1 },
	{ 0xff2000, 0xff3fff, MRA_BANK2 },
	{ 0xff4000, 0xff5fff, MRA_BANK3 },
	{ 0xff6000, 0xff7fff, MRA_BANK4 },
	{ 0xff8000, 0xff9fff, MRA_BANK5 },
	{ 0xffa000, 0xffbfff, MRA_BANK6 },
	{ 0xffc000, 0xffffff, MRA_BANK7 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress extra_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xfc0000, 0xfdffff, atarigen_video_int_ack_w },
	{ 0xfe8000, 0xfe8fff, cyberbal_paletteram_2_w },
	{ 0xfec000, 0xfecfff, cyberbal_paletteram_1_w },			/* player 2 palette RAM */
	{ 0xff0000, 0xff1fff, cyberbal_playfieldram_2_w, &cyberbal_playfieldram_2 },
	{ 0xff2000, 0xff3fff, MWA_BANK2 },
	{ 0xff4000, 0xff5fff, cyberbal_playfieldram_1_w, &cyberbal_playfieldram_1, &atarigen_playfieldram_size },
	{ 0xff6000, 0xff7fff, MWA_BANK4 },
	{ 0xff8000, 0xff9fff, MWA_BANK5 },
	{ 0xffa000, 0xffbfff, MWA_BANK6 },
	{ 0xffc000, 0xffffff, MWA_NOP },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_RAM },
	{ 0x2000, 0x2001, YM2151_status_port_0_r },
	{ 0x2802, 0x2803, atarigen_6502_irq_ack_r },
	{ 0x2c00, 0x2c01, atarigen_6502_sound_r },
	{ 0x2c02, 0x2c03, special_port3_r },
	{ 0x2c04, 0x2c05, sound_68k_6502_r },
	{ 0x2c06, 0x2c07, sound_6502_stat_r },
	{ 0x3000, 0x3fff, MRA_BANK8 },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_RAM },
	{ 0x2000, 0x2000, YM2151_register_port_0_w },
	{ 0x2001, 0x2001, YM2151_data_port_0_w },
	{ 0x2800, 0x2801, sound_68k_6502_w },
	{ 0x2802, 0x2803, atarigen_6502_irq_ack_w },
	{ 0x2804, 0x2805, atarigen_6502_sound_w },
	{ 0x2806, 0x2807, sound_bank_select_w },
	{ 0x3000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	68000 Sound CPU memory handlers
 *
 *************************************/

#ifdef EMULATE_SOUND_68000

static UINT8 *ram;
static READ_HANDLER( ram_r ) { return READ_WORD(&ram[offset]); }
static WRITE_HANDLER( ram_w ) { COMBINE_WORD_MEM(&ram[offset], data); }

static struct MemoryReadAddress sound_68k_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },
	{ 0xff8000, 0xff87ff, sound_68k_r },
	{ 0xfff000, 0xffffff, ram_r, &ram },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_68k_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xff8800, 0xff8fff, sound_68k_w },
	{ 0xff9000, 0xff97ff, io_68k_irq_ack_w },
	{ 0xff9800, 0xff9fff, sound_68k_dac_w },
	{ 0xfff000, 0xffffff, ram_w, &ram },
	{ -1 }  /* end of table */
};

#endif



/*************************************
 *
 *	2-player main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress cyberb2p_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0xfc0000, 0xfc0003, input_port_0_r },
	{ 0xfc2000, 0xfc2003, input_port_1_r },
	{ 0xfc4000, 0xfc4003, special_port2_r },
	{ 0xfc6000, 0xfc6003, atarigen_sound_upper_r },
	{ 0xfc8000, 0xfc8fff, atarigen_eeprom_r },
	{ 0xfca000, 0xfcafff, MRA_BANK1 },
	{ 0xfe0000, 0xfe0003, sound_state_r },
	{ 0xff0000, 0xff1fff, MRA_BANK2 },
	{ 0xff2000, 0xff2fff, MRA_BANK3 },
	{ 0xff3000, 0xff3fff, MRA_BANK4 },
	{ 0xff4000, 0xffffff, MRA_BANK5 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cyberb2p_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0xfc8000, 0xfc8fff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0xfca000, 0xfcafff, atarigen_666_paletteram_w, &paletteram },
	{ 0xfd0000, 0xfd0003, atarigen_eeprom_enable_w },
	{ 0xfd2000, 0xfd2003, atarigen_sound_reset_w },
	{ 0xfd4000, 0xfd4003, watchdog_reset_w },
	{ 0xfd6000, 0xfd6003, atarigen_video_int_ack_w },
	{ 0xfd8000, 0xfd8003, atarigen_sound_upper_w },
	{ 0xff0000, 0xff1fff, cyberbal_playfieldram_1_w, &cyberbal_playfieldram_1, &atarigen_playfieldram_size },
	{ 0xff2000, 0xff2fff, MWA_BANK3, &atarigen_alpharam, &atarigen_alpharam_size },
	{ 0xff3000, 0xff3fff, MWA_BANK4, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0xff4000, 0xffffff, MWA_BANK5 },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( cyberbal )
	PORT_START      /* fe0000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER4 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER4 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x00c0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER3 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER3 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	PORT_START      /* fe1000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x00c0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_VBLANK )

	PORT_START		/* fake port for screen switching */
	PORT_BITX(  0x0001, IP_ACTIVE_HIGH, IPT_BUTTON2, "Select Left Screen", KEYCODE_9, IP_JOY_NONE )
	PORT_BITX(  0x0002, IP_ACTIVE_HIGH, IPT_BUTTON2, "Select Right Screen", KEYCODE_0, IP_JOY_NONE )
	PORT_BIT( 0xfffc, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START		/* audio board port */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_COIN4 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )	/* output buffer full */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )		/* input buffer full */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )	/* self test */
INPUT_PORTS_END


INPUT_PORTS_START( cyberb2p )
	PORT_START      /* fc0000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER1 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* fc2000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START		/* fc4000 */
	PORT_BIT( 0x1fff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x4000, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )

	JSA_II_PORT		/* audio board port */
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout pflayout =
{
	16,8,	/* 8*8 chars */
	8192,	/* 8192 chars */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0,0, 4,4, 8,8, 12,12, 16,16, 20,20, 24,24, 28,28 },
	{ 0*8, 4*8, 8*8, 12*8, 16*8, 20*8, 24*8, 28*8 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout anlayout =
{
	16,8,	/* 8*8 chars */
	4096,	/* 4096 chars */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0,0, 4,4, 8,8, 12,12, 16,16, 20,20, 24,24, 28,28 },
	{ 0*8, 4*8, 8*8, 12*8, 16*8, 20*8, 24*8, 28*8 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout pflayout_interleaved =
{
	16,8,	/* 8*8 chars */
	8192,	/* 8192 chars */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0x20000*8+0,0x20000*8+0, 0x20000*8+4,0x20000*8+4, 0,0, 4,4, 0x20000*8+8,0x20000*8+8, 0x20000*8+12,0x20000*8+12, 8,8, 12,12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout anlayout_interleaved =
{
	16,8,	/* 8*8 chars */
	4096,	/* 4096 chars */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0x10000*8+0,0x10000*8+0, 0x10000*8+4,0x10000*8+4, 0,0, 4,4, 0x10000*8+8,0x10000*8+8, 0x10000*8+12,0x10000*8+12, 8,8, 12,12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxLayout molayout =
{
	16,8,	/* 8*8 chars */
	20480,	/* 20480 chars */
	4,		/* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	{ 0xf0000*8+0, 0xf0000*8+4, 0xa0000*8+0, 0xa0000*8+4, 0x50000*8+0, 0x50000*8+4, 0, 4,
	  0xf0000*8+8, 0xf0000*8+12, 0xa0000*8+8, 0xa0000*8+12, 0x50000*8+8, 0x50000*8+12, 8, 12 },
	{ 0*8, 2*8, 4*8, 6*8, 8*8, 10*8, 12*8, 14*8 },
	16*8	/* every char takes 16 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX2, 0, &pflayout,     0, 128 },
	{ REGION_GFX1, 0, &molayout, 0x600, 16 },
	{ REGION_GFX3, 0, &anlayout, 0x780, 8 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo gfxdecodeinfo_interleaved[] =
{
	{ REGION_GFX2, 0, &pflayout_interleaved,     0, 128 },
	{ REGION_GFX1, 0, &molayout,             0x600, 16 },
	{ REGION_GFX3, 0, &anlayout_interleaved, 0x780, 8 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	ATARI_CLOCK_14MHz/4,
#if USE_MONO_SOUND
	{ YM3012_VOL(30,MIXER_PAN_CENTER,30,MIXER_PAN_CENTER) },
#else
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
#endif
	{ atarigen_ym2151_irq_gen }
};

#ifdef EMULATE_SOUND_68000
static struct DACinterface dac_interface =
{
	2,
	{ MIXER(100,MIXER_PAN_LEFT), MIXER(100,MIXER_PAN_RIGHT) }
};
#endif

static struct CustomSound_interface samples_interface =
{
	samples_start,
	samples_stop,
	NULL
};


/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_cyberbal =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz/2,
			main_readmem,main_writemem,0,0,
			ignore_interrupt,1
		},
		{
			CPU_M6502,
			ATARI_CLOCK_14MHz/8,
			sound_readmem,sound_writemem,0,0,
			0,0,
			atarigen_6502_irq_gen,(UINT32)(1000000000.0/((float)ATARI_CLOCK_14MHz/4/4/16/16/14))
		},
		{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz/2,
			extra_readmem,extra_writemem,0,0,
			atarigen_video_int_gen,1
		}
#ifdef EMULATE_SOUND_68000
		,{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz/2,
			sound_68k_readmem,sound_68k_writemem,0,0,
			0,0,
			sound_68k_irq_gen,10000
		}
#endif
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,
	init_machine,

	/* video hardware */
	42*16, 30*8, { 0*16, 42*16-1, 0*8, 30*8-1 },
	gfxdecodeinfo_interleaved,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK |
			VIDEO_PIXEL_ASPECT_RATIO_1_2,
	0,
	cyberbal_vh_start,
	cyberbal_vh_stop,
	cyberbal_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
#ifdef EMULATE_SOUND_68000
		{
			SOUND_DAC,
			&dac_interface
		}
#else
		{
			SOUND_CUSTOM,
			&samples_interface
		}
#endif
	},

	atarigen_nvram_handler
};


static struct MachineDriver machine_driver_cyberb2p =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,		/* verified */
			ATARI_CLOCK_14MHz/2,
			cyberb2p_readmem,cyberb2p_writemem,0,0,
			atarigen_video_int_gen,1
		},
		JSA_II_CPU
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,
	cyberb2p_init_machine,

	/* video hardware */
	42*16, 30*8, { 0*16, 42*16-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK |
			VIDEO_PIXEL_ASPECT_RATIO_1_2,
	0,
	cyberbal_vh_start,
	cyberbal_vh_stop,
	cyberbal_vh_screenrefresh,

	/* sound hardware */
	JSA_II_MONO(REGION_SOUND1),

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( cyberbal )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "4123.1m", 0x00000, 0x10000, 0xfb872740 )
	ROM_LOAD_ODD ( "4124.1k", 0x00000, 0x10000, 0x87babad9 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "2131-snd.2f",  0x10000, 0x4000, 0xbd7e3d84 )
	ROM_CONTINUE(             0x04000, 0xc000 )

	ROM_REGION( 0x40000, REGION_CPU3 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "2127.3c", 0x00000, 0x10000, 0x3e5feb1f )
	ROM_LOAD_ODD ( "2128.1b", 0x00000, 0x10000, 0x4e642cc3 )
	ROM_LOAD_EVEN( "2129.1c", 0x20000, 0x10000, 0xdb11d2f0 )
	ROM_LOAD_ODD ( "2130.3b", 0x20000, 0x10000, 0xfd86b8aa )

	ROM_REGION( 0x40000, REGION_CPU4 )	/* 256k for 68000 sound code */
	ROM_LOAD_EVEN( "1132-snd.5c",  0x00000, 0x10000, 0xca5ce8d8 )
	ROM_LOAD_ODD ( "1133-snd.7c",  0x00000, 0x10000, 0xffeb8746 )
	ROM_LOAD_EVEN( "1134-snd.5a",  0x20000, 0x10000, 0xbcbd4c00 )
	ROM_LOAD_ODD ( "1135-snd.7a",  0x20000, 0x10000, 0xd520f560 )

	ROM_REGION( 0x140000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1150.15a",  0x000000, 0x10000, 0xe770eb3e ) /* MO */
	ROM_LOAD( "1154.16a",  0x010000, 0x10000, 0x40db00da ) /* MO */
	ROM_LOAD( "2158.17a",  0x020000, 0x10000, 0x52bb08fb ) /* MO */
	ROM_LOAD( "1162.19a",  0x030000, 0x10000, 0x0a11d877 ) /* MO */

	ROM_LOAD( "1151.11a",  0x050000, 0x10000, 0x6f53c7c1 ) /* MO */
	ROM_LOAD( "1155.12a",  0x060000, 0x10000, 0x5de609e5 ) /* MO */
	ROM_LOAD( "2159.13a",  0x070000, 0x10000, 0xe6f95010 ) /* MO */
	ROM_LOAD( "1163.14a",  0x080000, 0x10000, 0x47f56ced ) /* MO */

	ROM_LOAD( "1152.15c",  0x0a0000, 0x10000, 0xc8f1f7ff ) /* MO */
	ROM_LOAD( "1156.16c",  0x0b0000, 0x10000, 0x6bf0bf98 ) /* MO */
	ROM_LOAD( "2160.17c",  0x0c0000, 0x10000, 0xc3168603 ) /* MO */
	ROM_LOAD( "1164.19c",  0x0d0000, 0x10000, 0x7ff29d09 ) /* MO */

	ROM_LOAD( "1153.11c",  0x0f0000, 0x10000, 0x99629412 ) /* MO */
	ROM_LOAD( "1157.12c",  0x100000, 0x10000, 0xaa198cb7 ) /* MO */
	ROM_LOAD( "2161.13c",  0x110000, 0x10000, 0x6cf79a67 ) /* MO */
	ROM_LOAD( "1165.14c",  0x120000, 0x10000, 0x40bdf767 ) /* MO */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1146.9l",   0x000000, 0x10000, 0xa64b4da8 ) /* playfield */
	ROM_LOAD( "1147.8l",   0x010000, 0x10000, 0xca91ec1b ) /* playfield */
	ROM_LOAD( "1148.11l",  0x020000, 0x10000, 0xee29d1d1 ) /* playfield */
	ROM_LOAD( "1149.10l",  0x030000, 0x10000, 0x882649f8 ) /* playfield */

	ROM_REGION( 0x020000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1166.14n",  0x000000, 0x10000, 0x0ca1e3b3 ) /* alphanumerics */
	ROM_LOAD( "1167.16n",  0x010000, 0x10000, 0x882f4e1c ) /* alphanumerics */
ROM_END


ROM_START( cyberba2 )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "2123.1m", 0x00000, 0x10000, 0x502676e8 )
	ROM_LOAD_ODD ( "2124.1k", 0x00000, 0x10000, 0x30f55915 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "2131-snd.2f",  0x10000, 0x4000, 0xbd7e3d84 )
	ROM_CONTINUE(             0x04000, 0xc000 )

	ROM_REGION( 0x40000, REGION_CPU3 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "2127.3c", 0x00000, 0x10000, 0x3e5feb1f )
	ROM_LOAD_ODD ( "2128.1b", 0x00000, 0x10000, 0x4e642cc3 )
	ROM_LOAD_EVEN( "2129.1c", 0x20000, 0x10000, 0xdb11d2f0 )
	ROM_LOAD_ODD ( "2130.3b", 0x20000, 0x10000, 0xfd86b8aa )

	ROM_REGION( 0x40000, REGION_CPU4 )	/* 256k for 68000 sound code */
	ROM_LOAD_EVEN( "1132-snd.5c",  0x00000, 0x10000, 0xca5ce8d8 )
	ROM_LOAD_ODD ( "1133-snd.7c",  0x00000, 0x10000, 0xffeb8746 )
	ROM_LOAD_EVEN( "1134-snd.5a",  0x20000, 0x10000, 0xbcbd4c00 )
	ROM_LOAD_ODD ( "1135-snd.7a",  0x20000, 0x10000, 0xd520f560 )

	ROM_REGION( 0x140000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1150.15a",  0x000000, 0x10000, 0xe770eb3e ) /* MO */
	ROM_LOAD( "1154.16a",  0x010000, 0x10000, 0x40db00da ) /* MO */
	ROM_LOAD( "2158.17a",  0x020000, 0x10000, 0x52bb08fb ) /* MO */
	ROM_LOAD( "1162.19a",  0x030000, 0x10000, 0x0a11d877 ) /* MO */

	ROM_LOAD( "1151.11a",  0x050000, 0x10000, 0x6f53c7c1 ) /* MO */
	ROM_LOAD( "1155.12a",  0x060000, 0x10000, 0x5de609e5 ) /* MO */
	ROM_LOAD( "2159.13a",  0x070000, 0x10000, 0xe6f95010 ) /* MO */
	ROM_LOAD( "1163.14a",  0x080000, 0x10000, 0x47f56ced ) /* MO */

	ROM_LOAD( "1152.15c",  0x0a0000, 0x10000, 0xc8f1f7ff ) /* MO */
	ROM_LOAD( "1156.16c",  0x0b0000, 0x10000, 0x6bf0bf98 ) /* MO */
	ROM_LOAD( "2160.17c",  0x0c0000, 0x10000, 0xc3168603 ) /* MO */
	ROM_LOAD( "1164.19c",  0x0d0000, 0x10000, 0x7ff29d09 ) /* MO */

	ROM_LOAD( "1153.11c",  0x0f0000, 0x10000, 0x99629412 ) /* MO */
	ROM_LOAD( "1157.12c",  0x100000, 0x10000, 0xaa198cb7 ) /* MO */
	ROM_LOAD( "2161.13c",  0x110000, 0x10000, 0x6cf79a67 ) /* MO */
	ROM_LOAD( "1165.14c",  0x120000, 0x10000, 0x40bdf767 ) /* MO */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1146.9l",   0x000000, 0x10000, 0xa64b4da8 ) /* playfield */
	ROM_LOAD( "1147.8l",   0x010000, 0x10000, 0xca91ec1b ) /* playfield */
	ROM_LOAD( "1148.11l",  0x020000, 0x10000, 0xee29d1d1 ) /* playfield */
	ROM_LOAD( "1149.10l",  0x030000, 0x10000, 0x882649f8 ) /* playfield */

	ROM_REGION( 0x020000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1166.14n",  0x000000, 0x10000, 0x0ca1e3b3 ) /* alphanumerics */
	ROM_LOAD( "1167.16n",  0x010000, 0x10000, 0x882f4e1c ) /* alphanumerics */
ROM_END


ROM_START( cyberbt )
	ROM_REGION( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "cyb1007.bin", 0x00000, 0x10000, 0xd434b2d7 )
	ROM_LOAD_ODD ( "cyb1008.bin", 0x00000, 0x10000, 0x7d6c4163 )
	ROM_LOAD_EVEN( "cyb1009.bin", 0x20000, 0x10000, 0x3933e089 )
	ROM_LOAD_ODD ( "cyb1010.bin", 0x20000, 0x10000, 0xe7a7cae8 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "cyb1029.bin",  0x10000, 0x4000, 0xafee87e1 )
	ROM_CONTINUE(             0x04000, 0xc000 )

	ROM_REGION( 0x40000, REGION_CPU3 )
	ROM_LOAD_EVEN( "cyb1011.bin", 0x00000, 0x10000, 0x22d3e09c )
	ROM_LOAD_ODD ( "cyb1012.bin", 0x00000, 0x10000, 0xa8eeed8c )
	ROM_LOAD_EVEN( "cyb1013.bin", 0x20000, 0x10000, 0x11d287c9 )
	ROM_LOAD_ODD ( "cyb1014.bin", 0x20000, 0x10000, 0xbe15db42 )

	ROM_REGION( 0x40000, REGION_CPU4 )	/* 256k for 68000 sound code */
	ROM_LOAD_EVEN( "1132-snd.5c",  0x00000, 0x10000, 0xca5ce8d8 )
	ROM_LOAD_ODD ( "1133-snd.7c",  0x00000, 0x10000, 0xffeb8746 )
	ROM_LOAD_EVEN( "1134-snd.5a",  0x20000, 0x10000, 0xbcbd4c00 )
	ROM_LOAD_ODD ( "1135-snd.7a",  0x20000, 0x10000, 0xd520f560 )

	ROM_REGION( 0x140000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1001.bin",  0x000000, 0x20000, 0x586ba107 ) /* MO */
	ROM_LOAD( "1005.bin",  0x020000, 0x20000, 0xa53e6248 ) /* MO */
	ROM_LOAD( "1032.bin",  0x040000, 0x10000, 0x131f52a0 ) /* MO */

	ROM_LOAD( "1002.bin",  0x050000, 0x20000, 0x0f71f86c ) /* MO */
	ROM_LOAD( "1006.bin",  0x070000, 0x20000, 0xdf0ab373 ) /* MO */
	ROM_LOAD( "1033.bin",  0x090000, 0x10000, 0xb6270943 ) /* MO */

	ROM_LOAD( "1003.bin",  0x0a0000, 0x20000, 0x1cf373a2 ) /* MO */
	ROM_LOAD( "1007.bin",  0x0c0000, 0x20000, 0xf2ffab24 ) /* MO */
	ROM_LOAD( "1034.bin",  0x0e0000, 0x10000, 0x6514f0bd ) /* MO */

	ROM_LOAD( "1004.bin",  0x0f0000, 0x20000, 0x537f6de3 ) /* MO */
	ROM_LOAD( "1008.bin",  0x110000, 0x20000, 0x78525bbb ) /* MO */
	ROM_LOAD( "1035.bin",  0x130000, 0x10000, 0x1be3e5c8 ) /* MO */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cyb1015.bin",  0x000000, 0x10000, 0xdbbad153 ) /* playfield */
	ROM_LOAD( "cyb1016.bin",  0x010000, 0x10000, 0x76e0d008 ) /* playfield */
	ROM_LOAD( "cyb1017.bin",  0x020000, 0x10000, 0xddca9ca2 ) /* playfield */
	ROM_LOAD( "cyb1018.bin",  0x030000, 0x10000, 0xaa495b6f ) /* playfield */

	ROM_REGION( 0x020000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cyb1019.bin",  0x000000, 0x10000, 0x833b4768 ) /* alphanumerics */
	ROM_LOAD( "cyb1020.bin",  0x010000, 0x10000, 0x4976cffd ) /* alphanumerics */
ROM_END


ROM_START( cyberb2p )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 8*64k for 68000 code */
	ROM_LOAD_EVEN( "3019.bin", 0x00000, 0x10000, 0x029f8cb6 )
	ROM_LOAD_ODD ( "3020.bin", 0x00000, 0x10000, 0x1871b344 )
	ROM_LOAD_EVEN( "3021.bin", 0x20000, 0x10000, 0xfd7ebead )
	ROM_LOAD_ODD ( "3022.bin", 0x20000, 0x10000, 0x173ccad4 )
	ROM_LOAD_EVEN( "2023.bin", 0x40000, 0x10000, 0xe541b08f )
	ROM_LOAD_ODD ( "2024.bin", 0x40000, 0x10000, 0x5a77ee95 )
	ROM_LOAD_EVEN( "1025.bin", 0x60000, 0x10000, 0x95ff68c6 )
	ROM_LOAD_ODD ( "1026.bin", 0x60000, 0x10000, 0xf61c4898 )

	ROM_REGION( 0x14000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "1042.bin",  0x10000, 0x4000, 0xe63cf125 )
	ROM_CONTINUE(          0x04000, 0xc000 )

	ROM_REGION( 0x140000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1001.bin",  0x000000, 0x20000, 0x586ba107 ) /* MO */
	ROM_LOAD( "1005.bin",  0x020000, 0x20000, 0xa53e6248 ) /* MO */
	ROM_LOAD( "1032.bin",  0x040000, 0x10000, 0x131f52a0 ) /* MO */

	ROM_LOAD( "1002.bin",  0x050000, 0x20000, 0x0f71f86c ) /* MO */
	ROM_LOAD( "1006.bin",  0x070000, 0x20000, 0xdf0ab373 ) /* MO */
	ROM_LOAD( "1033.bin",  0x090000, 0x10000, 0xb6270943 ) /* MO */

	ROM_LOAD( "1003.bin",  0x0a0000, 0x20000, 0x1cf373a2 ) /* MO */
	ROM_LOAD( "1007.bin",  0x0c0000, 0x20000, 0xf2ffab24 ) /* MO */
	ROM_LOAD( "1034.bin",  0x0e0000, 0x10000, 0x6514f0bd ) /* MO */

	ROM_LOAD( "1004.bin",  0x0f0000, 0x20000, 0x537f6de3 ) /* MO */
	ROM_LOAD( "1008.bin",  0x110000, 0x20000, 0x78525bbb ) /* MO */
	ROM_LOAD( "1035.bin",  0x130000, 0x10000, 0x1be3e5c8 ) /* MO */

	ROM_REGION( 0x040000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1036.bin",  0x000000, 0x10000, 0xcdf6e3d6 ) /* playfield */
	ROM_LOAD( "1037.bin",  0x010000, 0x10000, 0xec2fef3e ) /* playfield */
	ROM_LOAD( "1038.bin",  0x020000, 0x10000, 0xe866848f ) /* playfield */
	ROM_LOAD( "1039.bin",  0x030000, 0x10000, 0x9b9a393c ) /* playfield */

	ROM_REGION( 0x020000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1040.bin",  0x000000, 0x10000, 0xa4c116f9 ) /* alphanumerics */
	ROM_LOAD( "1041.bin",  0x010000, 0x10000, 0xe25d7847 ) /* alphanumerics */

	ROM_REGION( 0x40000, REGION_SOUND1 )	/* 256k for ADPCM samples */
	ROM_LOAD( "1049.bin",  0x00000, 0x10000, 0x94f24575 )
	ROM_LOAD( "1050.bin",  0x10000, 0x10000, 0x87208e1e )
	ROM_LOAD( "1051.bin",  0x20000, 0x10000, 0xf82558b9 )
	ROM_LOAD( "1052.bin",  0x30000, 0x10000, 0xd96437ad )
ROM_END



/*************************************
 *
 *	Machine initialization
 *
 *************************************/

static const UINT16 default_eeprom[] =
{
	0x0001,0x01FF,0x0F00,0x011A,0x014A,0x0100,0x01A1,0x0200,
	0x010E,0x01AF,0x0300,0x01FF,0x0114,0x0144,0x01FF,0x0F00,
	0x011A,0x014A,0x0100,0x01A1,0x0200,0x010E,0x01AF,0x0300,
	0x01FF,0x0114,0x0144,0x01FF,0x0E00,0x01FF,0x0E00,0x01FF,
	0x0E00,0x01FF,0x0E00,0x01FF,0x0E00,0x01FF,0x0E00,0x01FF,
	0x0E00,0x01A8,0x0131,0x010B,0x0100,0x014C,0x0A00,0x01FF,
	0x0E00,0x01FF,0x0E00,0x01FF,0x0E00,0xB5FF,0x0E00,0x01FF,
	0x0E00,0x01FF,0x0E00,0x01FF,0x0E00,0x01FF,0x0E00,0x01FF,
	0x0E00,0x01FF,0x0E00,0x0000
};

static void init_cyberbal(void)
{
	atarigen_eeprom_default = default_eeprom;
	atarigen_slapstic_init(0, 0x018000, 0);

	/* make sure the banks are pointing to the correct location */
	cpu_setbank(1, cyberbal_playfieldram_2);
	cpu_setbank(3, cyberbal_playfieldram_1);

	/* display messages */
/*	atarigen_show_slapstic_message(); -- no slapstic */
	atarigen_show_sound_message();

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4191, 0x41A9);

	atarigen_playfieldram = cyberbal_playfieldram_1;
}


static void init_cyberbt(void)
{
	atarigen_eeprom_default = default_eeprom;
	atarigen_slapstic_init(0, 0x018000, 116);

	/* make sure the banks are pointing to the correct location */
	cpu_setbank(1, cyberbal_playfieldram_2);
	cpu_setbank(3, cyberbal_playfieldram_1);

	/* display messages */
/*	atarigen_show_slapstic_message(); -- no known slapstic problems - yet! */
	atarigen_show_sound_message();

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4191, 0x41A9);

	atarigen_playfieldram = cyberbal_playfieldram_1;
}


static void init_cyberb2p(void)
{
	atarigen_eeprom_default = default_eeprom;

	/* initialize the JSA audio board */
	atarijsa_init(1, 3, 2, 0x8000);

	/* display messages */
	atarigen_show_sound_message();

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x4159, 0x4171);

	atarigen_playfieldram = cyberbal_playfieldram_1;
}



/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1988, cyberbal, 0,        cyberbal, cyberbal, cyberbal, ROT0_16BIT, "Atari Games", "Cyberball (Version 4)" )
GAME( 1988, cyberba2, cyberbal, cyberbal, cyberbal, cyberbal, ROT0_16BIT, "Atari Games", "Cyberball (Version 2)" )
GAME( 1989, cyberbt,  cyberbal, cyberbal, cyberbal, cyberbt,  ROT0_16BIT, "Atari Games", "Tournament Cyberball 2072" )
GAME( 1989, cyberb2p, cyberbal, cyberb2p, cyberb2p, cyberb2p, ROT0_16BIT, "Atari Games", "Cyberball 2072 (2 player)" )

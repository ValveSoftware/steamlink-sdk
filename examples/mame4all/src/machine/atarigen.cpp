/***************************************************************************

  atarigen.c

  General functions for mid-to-late 80's Atari raster games.

***************************************************************************/


#include "driver.h"
#include "atarigen.h"
#include "cpu/m6502/m6502.h"


/*--------------------------------------------------------------------------

	Atari generic interrupt model (required)

		atarigen_scanline_int_state - state of the scanline interrupt line
		atarigen_sound_int_state - state of the sound interrupt line
		atarigen_video_int_state - state of the video interrupt line

		atarigen_int_callback - called when the interrupt state changes

		atarigen_interrupt_reset - resets & initializes the interrupt state
		atarigen_update_interrupts - forces the interrupts to be reevaluted

		atarigen_scanline_int_set - scanline interrupt initialization
		atarigen_scanline_int_gen - scanline interrupt generator
		atarigen_scanline_int_ack_w - scanline interrupt acknowledgement

		atarigen_sound_int_gen - sound interrupt generator
		atarigen_sound_int_ack_w - sound interrupt acknowledgement

		atarigen_video_int_gen - video interrupt generator
		atarigen_video_int_ack_w - video interrupt acknowledgement

--------------------------------------------------------------------------*/

/* globals */
int atarigen_scanline_int_state;
int atarigen_sound_int_state;
int atarigen_video_int_state;

/* statics */
static atarigen_int_callback update_int_callback;
static void *scanline_interrupt_timer;

/* prototypes */
static void scanline_interrupt_callback(int param);


/*
 *	Interrupt initialization
 *
 *	Resets the various interrupt states.
 *
 */

void atarigen_interrupt_reset(atarigen_int_callback update_int)
{
	/* set the callback */
	update_int_callback = update_int;

	/* reset the interrupt states */
	atarigen_video_int_state = atarigen_sound_int_state = atarigen_scanline_int_state = 0;
	scanline_interrupt_timer = NULL;
}


/*
 *	Update interrupts
 *
 *	Forces the interrupt callback to be called with the current VBLANK and sound interrupt states.
 *
 */

void atarigen_update_interrupts(void)
{
	(*update_int_callback)();
}



/*
 *	Scanline interrupt initialization
 *
 *	Sets the scanline when the next scanline interrupt should be generated.
 *
 */

void atarigen_scanline_int_set(int scanline)
{
	if (scanline_interrupt_timer)
		timer_remove(scanline_interrupt_timer);
	scanline_interrupt_timer = timer_set(cpu_getscanlinetime(scanline), 0, scanline_interrupt_callback);
}


/*
 *	Scanline interrupt generator
 *
 *	Standard interrupt routine which sets the scanline interrupt state.
 *
 */

int atarigen_scanline_int_gen(void)
{
	atarigen_scanline_int_state = 1;
	(*update_int_callback)();
	return 0;
}


/*
 *	Scanline interrupt acknowledge write handler
 *
 *	Resets the state of the scanline interrupt.
 *
 */

WRITE_HANDLER( atarigen_scanline_int_ack_w )
{
	atarigen_scanline_int_state = 0;
	(*update_int_callback)();
}


/*
 *	Sound interrupt generator
 *
 *	Standard interrupt routine which sets the sound interrupt state.
 *
 */

int atarigen_sound_int_gen(void)
{
	atarigen_sound_int_state = 1;
	(*update_int_callback)();
	return 0;
}


/*
 *	Sound interrupt acknowledge write handler
 *
 *	Resets the state of the sound interrupt.
 *
 */

WRITE_HANDLER( atarigen_sound_int_ack_w )
{
	atarigen_sound_int_state = 0;
	(*update_int_callback)();
}


/*
 *	Video interrupt generator
 *
 *	Standard interrupt routine which sets the video interrupt state.
 *
 */

int atarigen_video_int_gen(void)
{
	atarigen_video_int_state = 1;
	(*update_int_callback)();
	return 0;
}


/*
 *	Video interrupt acknowledge write handler
 *
 *	Resets the state of the video interrupt.
 *
 */

WRITE_HANDLER( atarigen_video_int_ack_w )
{
	atarigen_video_int_state = 0;
	(*update_int_callback)();
}


/*
 *	Scanline interrupt generator
 *
 *	Signals an interrupt.
 *
 */

static void scanline_interrupt_callback(int param)
{
	/* generate the interrupt */
	atarigen_scanline_int_gen();

	/* set a new timer to go off at the same scan line next frame */
	scanline_interrupt_timer = timer_set(TIME_IN_HZ(Machine->drv->frames_per_second), 0, scanline_interrupt_callback);
}



/*--------------------------------------------------------------------------

	EEPROM I/O (optional)

		atarigen_eeprom_default - pointer to compressed default data
		atarigen_eeprom - pointer to base of EEPROM memory
		atarigen_eeprom_size - size of EEPROM memory

		atarigen_eeprom_reset - resets the EEPROM system

		atarigen_eeprom_enable_w - write handler to enable EEPROM access
		atarigen_eeprom_w - write handler for EEPROM data (low byte)
		atarigen_eeprom_r - read handler for EEPROM data (low byte)

		atarigen_nvram_handler - load/save EEPROM data

--------------------------------------------------------------------------*/

/* globals */
const UINT16 *atarigen_eeprom_default;
UINT8 *atarigen_eeprom;
size_t atarigen_eeprom_size;

/* statics */
static UINT8 unlocked;

/* prototypes */
static void decompress_eeprom_word(const UINT16 *data);
static void decompress_eeprom_byte(const UINT16 *data);


/*
 *	EEPROM reset
 *
 *	Makes sure that the unlocked state is cleared when we reset.
 *
 */

void atarigen_eeprom_reset(void)
{
	unlocked = 0;
}


/*
 *	EEPROM enable write handler
 *
 *	Any write to this handler will allow one byte to be written to the
 *	EEPROM data area the next time.
 *
 */

WRITE_HANDLER( atarigen_eeprom_enable_w )
{
	unlocked = 1;
}


/*
 *	EEPROM write handler (low byte of word)
 *
 *	Writes a "word" to the EEPROM, which is almost always accessed via
 *	the low byte of the word only. If the EEPROM hasn't been unlocked,
 *	the write attempt is ignored.
 *
 */

WRITE_HANDLER( atarigen_eeprom_w )
{
	if (!unlocked)
		return;

	COMBINE_WORD_MEM(&atarigen_eeprom[offset], data);
	unlocked = 0;
}


/*
 *	EEPROM read handler (low byte of word)
 *
 *	Reads a "word" from the EEPROM, which is almost always accessed via
 *	the low byte of the word only.
 *
 */

READ_HANDLER( atarigen_eeprom_r )
{
	return READ_WORD(&atarigen_eeprom[offset]) | 0xff00;
}

READ_HANDLER( atarigen_eeprom_upper_r )
{
	return READ_WORD(&atarigen_eeprom[offset]) | 0x00ff;
}


/*
 *	Standard high score load
 *
 *	Loads the EEPROM data as a "high score".
 *
 */

void atarigen_nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file, atarigen_eeprom, atarigen_eeprom_size);
	else
	{
		if (file)
			osd_fread(file, atarigen_eeprom, atarigen_eeprom_size);
		else
		{
			/* all 0xff's work for most games */
			memset(atarigen_eeprom, 0xff, atarigen_eeprom_size);

			/* anything else must be decompressed */
			if (atarigen_eeprom_default)
			{
				if (atarigen_eeprom_default[0] == 0)
					decompress_eeprom_byte(atarigen_eeprom_default + 1);
				else
					decompress_eeprom_word(atarigen_eeprom_default + 1);
			}
		}
	}
}



/*
 *	Decompress word-based EEPROM data
 *
 *	Used for decompressing EEPROM data that has every other byte invalid.
 *
 */

void decompress_eeprom_word(const UINT16 *data)
{
	UINT16 *dest = (UINT16 *)atarigen_eeprom;
	UINT16 value;

	while ((value = *data++) != 0)
	{
		int count = (value >> 8);
		value = (value << 8) | (value & 0xff);

		while (count--)
		{
			WRITE_WORD(dest, value);
			dest++;
		}
	}
}


/*
 *	Decompress byte-based EEPROM data
 *
 *	Used for decompressing EEPROM data that is byte-packed.
 *
 */

void decompress_eeprom_byte(const UINT16 *data)
{
	UINT8 *dest = (UINT8 *)atarigen_eeprom;
	UINT16 value;

	while ((value = *data++) != 0)
	{
		int count = (value >> 8);
		value = (value << 8) | (value & 0xff);

		while (count--)
			*dest++ = value;
	}
}



/*--------------------------------------------------------------------------

	Slapstic I/O (optional)

		atarigen_slapstic - pointer to base of slapstic memory

		atarigen_slapstic_init - select and initialize the slapstic handlers
		atarigen_slapstic_reset - resets the slapstic state

		atarigen_slapstic_w - write handler for slapstic data
		atarigen_slapstic_r - read handler for slapstic data

		slapstic_init - low-level init routine
		slapstic_reset - low-level reset routine
		slapstic_bank - low-level routine to return the current bank
		slapstic_tweak - low-level tweak routine

--------------------------------------------------------------------------*/

/* globals */
static UINT8 atarigen_slapstic_num;
static UINT8 *atarigen_slapstic;


/*
 *	Slapstic initialization
 *
 *	Installs memory handlers for the slapstic and sets the chip number.
 *
 */

void atarigen_slapstic_init(int cpunum, int base, int chipnum)
{
	atarigen_slapstic_num = chipnum;
	atarigen_slapstic = NULL;
	if (chipnum)
	{
		slapstic_init(chipnum);
		atarigen_slapstic = (UINT8*)install_mem_read_handler(cpunum, base, base + 0x7fff, atarigen_slapstic_r);
		atarigen_slapstic = (UINT8*)install_mem_write_handler(cpunum, base, base + 0x7fff, atarigen_slapstic_w);
	}
}


/*
 *	Slapstic initialization
 *
 *	Makes the selected slapstic number active and resets its state.
 *
 */

void atarigen_slapstic_reset(void)
{
	if (atarigen_slapstic_num)
		slapstic_reset();
}


/*
 *	Slapstic write handler
 *
 *	Assuming that the slapstic sits in ROM memory space, we just simply
 *	tweak the slapstic at this address and do nothing more.
 *
 */

WRITE_HANDLER( atarigen_slapstic_w )
{
	slapstic_tweak(offset / 2);
}


/*
 *	Slapstic read handler
 *
 *	Tweaks the slapstic at the appropriate address and then reads a
 *	word from the underlying memory.
 *
 */

READ_HANDLER( atarigen_slapstic_r )
{
	int bank = slapstic_tweak(offset / 2) * 0x2000;
	return READ_WORD(&atarigen_slapstic[bank + (offset & 0x1fff)]);
}




/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/



/*--------------------------------------------------------------------------

	Sound I/O

		atarigen_sound_io_reset - reset the sound I/O system

		atarigen_6502_irq_gen - standard 6502 IRQ interrupt generator
		atarigen_6502_irq_ack_r - standard 6502 IRQ interrupt acknowledgement
		atarigen_6502_irq_ack_w - standard 6502 IRQ interrupt acknowledgement

		atarigen_ym2151_irq_gen - YM2151 sound IRQ generator

		atarigen_sound_w - Main CPU -> sound CPU data write (low byte)
		atarigen_sound_r - Sound CPU -> main CPU data read (low byte)
		atarigen_sound_upper_w - Main CPU -> sound CPU data write (high byte)
		atarigen_sound_upper_r - Sound CPU -> main CPU data read (high byte)

		atarigen_sound_reset_w - 6502 CPU reset
		atarigen_6502_sound_w - Sound CPU -> main CPU data write
		atarigen_6502_sound_r - Main CPU -> sound CPU data read

--------------------------------------------------------------------------*/

/* constants */
#define SOUND_INTERLEAVE_RATE		TIME_IN_USEC(50)
#define SOUND_INTERLEAVE_REPEAT		20

/* globals */
int atarigen_cpu_to_sound_ready;
int atarigen_sound_to_cpu_ready;

/* statics */
static UINT8 sound_cpu_num;
static UINT8 atarigen_cpu_to_sound;
static UINT8 atarigen_sound_to_cpu;
static UINT8 timed_int;
static UINT8 ym2151_int;

/* prototypes */
static void update_6502_irq(void);
static void sound_comm_timer(int reps_left);
static void delayed_sound_reset(int param);
static void delayed_sound_w(int param);
static void delayed_6502_sound_w(int param);


/*
 *	Sound I/O reset
 *
 *	Resets the state of the sound I/O.
 *
 */

void atarigen_sound_io_reset(int cpu_num)
{
	/* remember which CPU is the sound CPU */
	sound_cpu_num = cpu_num;

	/* reset the internal interrupts states */
	timed_int = ym2151_int = 0;

	/* reset the sound I/O states */
	atarigen_cpu_to_sound = atarigen_sound_to_cpu = 0;
	atarigen_cpu_to_sound_ready = atarigen_sound_to_cpu_ready = 0;
}


/*
 *	6502 IRQ generator
 *
 *	Generates an IRQ signal to the 6502 sound processor.
 *
 */

int atarigen_6502_irq_gen(void)
{
	timed_int = 1;
	update_6502_irq();
	return 0;
}


/*
 *	6502 IRQ acknowledgement
 *
 *	Resets the IRQ signal to the 6502 sound processor. Both reads and writes can be used.
 *
 */

READ_HANDLER( atarigen_6502_irq_ack_r )
{
	timed_int = 0;
	update_6502_irq();
	return 0;
}

WRITE_HANDLER( atarigen_6502_irq_ack_w )
{
	timed_int = 0;
	update_6502_irq();
}


/*
 *	YM2151 IRQ generation
 *
 *	Sets the state of the YM2151's IRQ line.
 *
 */

void atarigen_ym2151_irq_gen(int irq)
{
	ym2151_int = irq;
	update_6502_irq();
}


/*
 *	Sound CPU write handler
 *
 *	Write handler which resets the sound CPU in response.
 *
 */

WRITE_HANDLER( atarigen_sound_reset_w )
{
	timer_set(TIME_NOW, 0, delayed_sound_reset);
}


/*
 *	Sound CPU reset handler
 *
 *	Resets the state of the sound CPU manually.
 *
 */

void atarigen_sound_reset(void)
{
	timer_set(TIME_NOW, 1, delayed_sound_reset);
}


/*
 *	Main -> sound CPU data write handlers
 *
 *	Handles communication from the main CPU to the sound CPU. Two versions are provided,
 *	one with the data byte in the low 8 bits, and one with the data byte in the upper 8
 *	bits.
 *
 */

WRITE_HANDLER( atarigen_sound_w )
{
	if (!(data & 0x00ff0000))
		timer_set(TIME_NOW, data & 0xff, delayed_sound_w);
}

WRITE_HANDLER( atarigen_sound_upper_w )
{
	if (!(data & 0xff000000))
		timer_set(TIME_NOW, (data >> 8) & 0xff, delayed_sound_w);
}


/*
 *	Sound -> main CPU data read handlers
 *
 *	Handles reading data communicated from the sound CPU to the main CPU. Two versions
 *	are provided, one with the data byte in the low 8 bits, and one with the data byte
 *	in the upper 8 bits.
 *
 */

READ_HANDLER( atarigen_sound_r )
{
	atarigen_sound_to_cpu_ready = 0;
	atarigen_sound_int_ack_w(0, 0);
	return atarigen_sound_to_cpu | 0xff00;
}

READ_HANDLER( atarigen_sound_upper_r )
{
	atarigen_sound_to_cpu_ready = 0;
	atarigen_sound_int_ack_w(0, 0);
	return (atarigen_sound_to_cpu << 8) | 0x00ff;
}


/*
 *	Sound -> main CPU data write handler
 *
 *	Handles communication from the sound CPU to the main CPU.
 *
 */

WRITE_HANDLER( atarigen_6502_sound_w )
{
	timer_set(TIME_NOW, data, delayed_6502_sound_w);
}


/*
 *	Main -> sound CPU data read handler
 *
 *	Handles reading data communicated from the main CPU to the sound CPU.
 *
 */

READ_HANDLER( atarigen_6502_sound_r )
{
	atarigen_cpu_to_sound_ready = 0;
	cpu_set_nmi_line(sound_cpu_num, CLEAR_LINE);
	return atarigen_cpu_to_sound;
}


/*
 *	6502 IRQ state updater
 *
 *	Called whenever the IRQ state changes. An interrupt is generated if
 *	either atarigen_6502_irq_gen() was called, or if the YM2151 generated
 *	an interrupt via the atarigen_ym2151_irq_gen() callback.
 *
 */

void update_6502_irq(void)
{
	if (timed_int || ym2151_int)
		cpu_set_irq_line(sound_cpu_num, M6502_INT_IRQ, ASSERT_LINE);
	else
		cpu_set_irq_line(sound_cpu_num, M6502_INT_IRQ, CLEAR_LINE);
}


/*
 *	Sound communications timer
 *
 *	Set whenever a command is written from the main CPU to the sound CPU, in order to
 *	temporarily bump up the interleave rate. This helps ensure that communications
 *	between the two CPUs works properly.
 *
 */

static void sound_comm_timer(int reps_left)
{
	if (--reps_left)
		timer_set(SOUND_INTERLEAVE_RATE, reps_left, sound_comm_timer);
}


/*
 *	Sound CPU reset timer
 *
 *	Synchronizes the sound reset command between the two CPUs.
 *
 */

static void delayed_sound_reset(int param)
{
	/* unhalt and reset the sound CPU */
	if (param == 0)
	{
		cpu_set_halt_line(sound_cpu_num, CLEAR_LINE);
		cpu_set_reset_line(sound_cpu_num, PULSE_LINE);
	}

	/* reset the sound write state */
	atarigen_sound_to_cpu_ready = 0;
	atarigen_sound_int_ack_w(0, 0);
}


/*
 *	Main -> sound data write timer
 *
 *	Synchronizes a data write from the main CPU to the sound CPU.
 *
 */

static void delayed_sound_w(int param)
{
	/* warn if we missed something */
	//if (atarigen_cpu_to_sound_ready)
	//	logerror("Missed command from 68010\n");

	/* set up the states and signal an NMI to the sound CPU */
	atarigen_cpu_to_sound = param;
	atarigen_cpu_to_sound_ready = 1;
	cpu_set_nmi_line(sound_cpu_num, ASSERT_LINE);

	/* allocate a high frequency timer until a response is generated */
	/* the main CPU is *very* sensistive to the timing of the response */
	timer_set(SOUND_INTERLEAVE_RATE, SOUND_INTERLEAVE_REPEAT, sound_comm_timer);
}


/*
 *	Sound -> main data write timer
 *
 *	Synchronizes a data write from the sound CPU to the main CPU.
 *
 */

static void delayed_6502_sound_w(int param)
{
	/* warn if we missed something */
	//if (atarigen_sound_to_cpu_ready)
	//	logerror("Missed result from 6502\n");

	/* set up the states and signal the sound interrupt to the main CPU */
	atarigen_sound_to_cpu = param;
	atarigen_sound_to_cpu_ready = 1;
	atarigen_sound_int_gen();
}



/*--------------------------------------------------------------------------

	Misc sound helpers

		atarigen_init_6502_speedup - installs 6502 speedup cheat handler
		atarigen_set_ym2151_vol - set the volume of the 2151 chip
		atarigen_set_ym2413_vol - set the volume of the 2151 chip
		atarigen_set_pokey_vol - set the volume of the POKEY chip(s)
		atarigen_set_tms5220_vol - set the volume of the 5220 chip
		atarigen_set_oki6295_vol - set the volume of the OKI6295

--------------------------------------------------------------------------*/

/* statics */
static UINT8 *speed_a, *speed_b;
static UINT32 speed_pc;

/* prototypes */
static READ_HANDLER( m6502_speedup_r );


/*
 *	6502 CPU speedup cheat installer
 *
 *	Installs a special read handler to catch the main spin loop in the
 *	6502 sound code. The addresses accessed seem to be the same across
 *	a large number of games, though the PC shifts.
 *
 */

void atarigen_init_6502_speedup(int cpunum, int compare_pc1, int compare_pc2)
{
	UINT8 *memory = memory_region(REGION_CPU1+cpunum);
	int address_low, address_high;

	/* determine the pointer to the first speed check location */
	address_low = memory[compare_pc1 + 1] | (memory[compare_pc1 + 2] << 8);
	address_high = memory[compare_pc1 + 4] | (memory[compare_pc1 + 5] << 8);
	//if (address_low != address_high - 1)
	//	logerror("Error: address %04X does not point to a speedup location!", compare_pc1);
	speed_a = &memory[address_low];

	/* determine the pointer to the second speed check location */
	address_low = memory[compare_pc2 + 1] | (memory[compare_pc2 + 2] << 8);
	address_high = memory[compare_pc2 + 4] | (memory[compare_pc2 + 5] << 8);
	//if (address_low != address_high - 1)
	//	logerror("Error: address %04X does not point to a speedup location!", compare_pc2);
	speed_b = &memory[address_low];

	/* install a handler on the second address */
	speed_pc = compare_pc2;
	install_mem_read_handler(cpunum, address_low, address_low, m6502_speedup_r);
}


/*
 *	Set the YM2151 volume
 *
 *	What it says.
 *
 */

void atarigen_set_ym2151_vol(int volume)
{
	int ch;

	for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
	{
		const char *name = mixer_get_name(ch);
		if (name && strstr(name, "2151"))
			mixer_set_volume(ch, volume);
	}
}


/*
 *	Set the YM2413 volume
 *
 *	What it says.
 *
 */

void atarigen_set_ym2413_vol(int volume)
{
	int ch;

	for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
	{
		const char *name = mixer_get_name(ch);
		if (name && strstr(name, "3812"))/*"2413")) -- need this change until 2413 stands alone */
			mixer_set_volume(ch, volume);
	}
}


/*
 *	Set the POKEY volume
 *
 *	What it says.
 *
 */

void atarigen_set_pokey_vol(int volume)
{
	int ch;

	for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
	{
		const char *name = mixer_get_name(ch);
		if (name && strstr(name, "POKEY"))
			mixer_set_volume(ch, volume);
	}
}


/*
 *	Set the TMS5220 volume
 *
 *	What it says.
 *
 */

void atarigen_set_tms5220_vol(int volume)
{
	int ch;

	for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
	{
		const char *name = mixer_get_name(ch);
		if (name && strstr(name, "5220"))
			mixer_set_volume(ch, volume);
	}
}


/*
 *	Set the OKI6295 volume
 *
 *	What it says.
 *
 */

void atarigen_set_oki6295_vol(int volume)
{
	int ch;

	for (ch = 0; ch < MIXER_MAX_CHANNELS; ch++)
	{
		const char *name = mixer_get_name(ch);
		if (name && strstr(name, "6295"))
			mixer_set_volume(ch, volume);
	}
}


/*
 *	Generic 6502 CPU speedup handler
 *
 *	Special shading renderer that runs any pixels under pen 1 through a lookup table.
 *
 */

static READ_HANDLER( m6502_speedup_r )
{
	int result = speed_b[0];

	if (cpu_getpreviouspc() == speed_pc && speed_a[0] == speed_a[1] && result == speed_b[1])
		cpu_spinuntil_int();

	return result;
}




/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/
/***********************************************************************************************/



/* general video globals */
UINT8 *atarigen_playfieldram;
UINT8 *atarigen_playfield2ram;
UINT8 *atarigen_playfieldram_color;
UINT8 *atarigen_playfield2ram_color;
UINT8 *atarigen_spriteram;
UINT8 *atarigen_alpharam;
UINT8 *atarigen_vscroll;
UINT8 *atarigen_hscroll;

size_t atarigen_playfieldram_size;
size_t atarigen_playfield2ram_size;
size_t atarigen_spriteram_size;
size_t atarigen_alpharam_size;



/*--------------------------------------------------------------------------

	Video scanline timing

		atarigen_scanline_timer_reset - call to reset the system

--------------------------------------------------------------------------*/

/* statics */
static atarigen_scanline_callback scanline_callback;
static int scanlines_per_callback;
static timer_tm scanline_callback_period;
static int last_scanline;

/* prototypes */
static void vblank_timer(int param);
static void scanline_timer(int scanline);

/*
 *	Scanline timer callback
 *
 *	Called once every n scanlines to generate the periodic callback to the main system.
 *
 */

void atarigen_scanline_timer_reset(atarigen_scanline_callback update_graphics, int frequency)
{
	/* set the scanline callback */
	scanline_callback = update_graphics;
	scanline_callback_period = (timer_tm)frequency * cpu_getscanlineperiod();
	scanlines_per_callback = frequency;

	/* compute the last scanline */
	last_scanline = (int)(TIME_IN_HZ(Machine->drv->frames_per_second) / cpu_getscanlineperiod());

	/* set a timer to go off on the next VBLANK */
	timer_set(cpu_getscanlinetime(Machine->drv->screen_height), 0, vblank_timer);
}


/*
 *	VBLANK timer callback
 *
 *	Called once every VBLANK to prime the scanline timers.
 *
 */

static void vblank_timer(int param)
{
	/* set a timer to go off at scanline 0 */
	timer_set(TIME_IN_USEC(Machine->drv->vblank_duration), 0, scanline_timer);

	/* set a timer to go off on the next VBLANK */
	timer_set(cpu_getscanlinetime(Machine->drv->screen_height), 1, vblank_timer);
}


/*
 *	Scanline timer callback
 *
 *	Called once every n scanlines to generate the periodic callback to the main system.
 *
 */

static void scanline_timer(int scanline)
{
	/* if this is scanline 0, we reset the MO and playfield system */
	if (scanline == 0)
	{
		atarigen_mo_reset();
		atarigen_pf_reset();
		atarigen_pf2_reset();
	}

	/* callback */
	if (scanline_callback)
	{
		(*scanline_callback)(scanline);

		/* generate another? */
		scanline += scanlines_per_callback;
		if (scanline < last_scanline && scanlines_per_callback)
			timer_set(scanline_callback_period, scanline, scanline_timer);
	}
}



/*--------------------------------------------------------------------------

	Video Controller I/O: used in Shuuz, Thunderjaws, Relief Pitcher, Off the Wall

		atarigen_video_control_data - pointer to base of control memory
		atarigen_video_control_latch1 - latch #1 value (-1 means disabled)
		atarigen_video_control_latch2 - latch #2 value (-1 means disabled)

		atarigen_video_control_reset - initializes the video controller

		atarigen_video_control_w - write handler for the video controller
		atarigen_video_control_r - read handler for the video controller

--------------------------------------------------------------------------*/

/* globals */
UINT8 *atarigen_video_control_data;
struct atarigen_video_control_state_desc atarigen_video_control_state;

/* statics */
static int actual_video_control_latch1;
static int actual_video_control_latch2;


/*
 *	Video controller initialization
 *
 *	Resets the state of the video controller.
 *
 */

void atarigen_video_control_reset(void)
{
	/* clear the RAM we use */
	memset(atarigen_video_control_data, 0, 0x40);
	memset(&atarigen_video_control_state, 0, sizeof(atarigen_video_control_state));

	/* reset the latches */
	atarigen_video_control_state.latch1 = atarigen_video_control_state.latch2 = -1;
	actual_video_control_latch1 = actual_video_control_latch2 = -1;
}


/*
 *	Video controller update
 *
 *	Copies the data from the specified location once/frame into the video controller registers
 *
 */

void atarigen_video_control_update(const UINT8 *data)
{
	int i;

	/* echo all the commands to the video controller */
	for (i = 0; i < 0x38; i += 2)
		if (READ_WORD(&data[i]))
			atarigen_video_control_w(i, READ_WORD(&data[i]));

	/* use this for debugging the video controller values */
#if 0
	if (keyboard_pressed(KEYCODE_8))
	{
		static FILE *out;
		if (!out) out = fopen("scroll.log", "w");
		if (out)
		{
			for (i = 0; i < 64; i++)
				fprintf(out, "%04X ", READ_WORD(&data[2 * i]));
			fprintf(out, "\n");
		}
	}
#endif
}


/*
 *	Video controller write
 *
 *	Handles an I/O write to the video controller.
 *
 */

WRITE_HANDLER( atarigen_video_control_w )
{
	int oldword = READ_WORD(&atarigen_video_control_data[offset]);
	int newword = COMBINE_WORD(oldword, data);
	WRITE_WORD(&atarigen_video_control_data[offset], newword);

	/* switch off the offset */
	switch (offset)
	{
		/* set the scanline interrupt here */
		case 0x06:
			if (oldword != newword)
				atarigen_scanline_int_set(newword & 0x1ff);
			break;

		/* latch enable */
		case 0x14:

			/* reset the latches when disabled */
			if (!(newword & 0x0080))
				atarigen_video_control_state.latch1 = atarigen_video_control_state.latch2 = -1;
			else
				atarigen_video_control_state.latch1 = actual_video_control_latch1,
				atarigen_video_control_state.latch2 = actual_video_control_latch2;

			/* check for rowscroll enable */
			atarigen_video_control_state.rowscroll_enable = (newword & 0x2000) >> 13;

			/* check for palette banking */
			atarigen_video_control_state.palette_bank = ((newword & 0x0400) >> 10) ^ 1;
			break;

		/* indexed parameters */
		case 0x20: case 0x22: case 0x24: case 0x26:
		case 0x28: case 0x2a: case 0x2c: case 0x2e:
		case 0x30: case 0x32: case 0x34: case 0x36:
			switch (newword & 15)
			{
				case 9:
					atarigen_video_control_state.sprite_xscroll = (newword >> 7) & 0x1ff;
					break;

				case 10:
					atarigen_video_control_state.pf2_xscroll = (newword >> 7) & 0x1ff;
					break;

				case 11:
					atarigen_video_control_state.pf1_xscroll = (newword >> 7) & 0x1ff;
					break;

				case 13:
					atarigen_video_control_state.sprite_yscroll = (newword >> 7) & 0x1ff;
					break;

				case 14:
					atarigen_video_control_state.pf2_yscroll = (newword >> 7) & 0x1ff;
					break;

				case 15:
					atarigen_video_control_state.pf1_yscroll = (newword >> 7) & 0x1ff;
					break;
			}
			break;

		/* latch 1 value */
		case 0x38:
			actual_video_control_latch1 = newword;
			actual_video_control_latch2 = -1;
			if (READ_WORD(&atarigen_video_control_data[0x14]) & 0x80)
				atarigen_video_control_state.latch1 = actual_video_control_latch1;
			break;

		/* latch 2 value */
		case 0x3a:
			actual_video_control_latch1 = -1;
			actual_video_control_latch2 = newword;
			if (READ_WORD(&atarigen_video_control_data[0x14]) & 0x80)
				atarigen_video_control_state.latch2 = actual_video_control_latch2;
			break;

		/* scanline IRQ ack here */
		case 0x3c:
			atarigen_scanline_int_ack_w(0, 0);
			break;

		/* log anything else */
		case 0x00:
		default:
			//if (oldword != newword)
			//	logerror("video_control_w(%02X, %04X) ** [prev=%04X]\n", offset, newword, oldword);
			break;
	}
}


/*
 *	Video controller read
 *
 *	Handles an I/O read from the video controller.
 *
 */

READ_HANDLER( atarigen_video_control_r )
{
	//logerror("video_control_r(%02X)\n", offset);

	/* a read from offset 0 returns the current scanline */
	/* also sets bit 0x4000 if we're in VBLANK */
	if (offset == 0)
	{
		int result = cpu_getscanline();

		if (result > 255)
			result = 255;
		if (result > Machine->visible_area.max_y)
			result |= 0x4000;

		return result;
	}
	else
		return READ_WORD(&atarigen_video_control_data[offset]);
}



/*--------------------------------------------------------------------------

	Motion object rendering

		atarigen_mo_desc - description of the M.O. layout

		atarigen_mo_callback - called back for each M.O. during processing

		atarigen_mo_init - initializes and configures the M.O. list walker
		atarigen_mo_free - frees all memory allocated by atarigen_mo_init
		atarigen_mo_reset - reset for a new frame (use only if not using interrupt system)
		atarigen_mo_update - updates the M.O. list for the given scanline
		atarigen_mo_process - processes the current list

--------------------------------------------------------------------------*/

/* statics */
static struct atarigen_mo_desc modesc;

static UINT16 *molist;
static UINT16 *molist_end;
static UINT16 *molist_last;
static UINT16 *molist_upper_bound;


/*
 *	Motion object render initialization
 *
 *	Allocates memory for the motion object display cache.
 *
 */

int atarigen_mo_init(const struct atarigen_mo_desc *source_desc)
{
	modesc = *source_desc;
	if (modesc.entrywords == 0) modesc.entrywords = 4;
	modesc.entrywords++;

	/* make sure everything is free */
	atarigen_mo_free();

	/* allocate memory for the cached list */
	molist = (UINT16*)malloc(modesc.maxcount * 2 * modesc.entrywords * (Machine->drv->screen_height / 8));
	if (!molist)
		return 1;
	molist_upper_bound = molist + (modesc.maxcount * modesc.entrywords * (Machine->drv->screen_height / 8));

	/* initialize the end/last pointers */
	atarigen_mo_reset();

	return 0;
}


/*
 *	Motion object render free
 *
 *	Frees all data allocated for the motion objects.
 *
 */

void atarigen_mo_free(void)
{
	if (molist)
		free(molist);
	molist = NULL;
}


/*
 *	Motion object render reset
 *
 *	Resets the motion object system for a new frame. Note that this is automatically called
 *	if you're using the scanline timing system.
 *
 */

void atarigen_mo_reset(void)
{
	molist_end = molist;
	molist_last = NULL;
}


/*
 *	Motion object updater
 *
 *	Parses the current motion object list, caching all entries.
 *
 */

void atarigen_mo_update(const UINT8 *base, int link, int scanline)
{
	int entryskip = modesc.entryskip, wordskip = modesc.wordskip, wordcount = modesc.entrywords - 1;
	UINT8 spritevisit[ATARIGEN_MAX_MAXCOUNT];
	UINT16 *data, *data_start, *prev_data;
	int match = 0;

	/* set up local pointers */
	data_start = data = molist_end;
	prev_data = molist_last;

	/* if the last list entries were on the same scanline, overwrite them */
	if (prev_data)
	{
		if (*prev_data == scanline)
			data_start = data = prev_data;
		else
			match = 1;
	}

	/* visit all the sprites and copy their data into the display list */
	memset(spritevisit, 0, modesc.linkmask + 1);
	while (!spritevisit[link])
	{
		const UINT8 *modata = &base[link * entryskip];
		UINT16 tempdata[16];
		int temp, i;

		/* bounds checking */
		if (data >= molist_upper_bound)
		{
			//logerror("Motion object list exceeded maximum\n");
			break;
		}

		/* start with the scanline */
		*data++ = scanline;

		/* add the data words */
		for (i = temp = 0; i < wordcount; i++, temp += wordskip)
			tempdata[i] = *data++ = READ_WORD(&modata[temp]);

		/* is this one to ignore? (note that ignore is predecremented by 4) */
		if (tempdata[modesc.ignoreword] == 0xffff)
			data -= wordcount + 1;

		/* update our match status */
		else if (match)
		{
			prev_data++;
			for (i = 0; i < wordcount; i++)
				if (*prev_data++ != tempdata[i])
				{
					match = 0;
					break;
				}
		}

		/* link to the next object */
		spritevisit[link] = 1;
		if (modesc.linkword >= 0)
			link = (tempdata[modesc.linkword] >> modesc.linkshift) & modesc.linkmask;
		else
			link = (link + 1) & modesc.linkmask;
	}

	/* if we didn't match the last set of entries, update the counters */
	if (!match)
	{
		molist_end = data;
		molist_last = data_start;
	}
}


/*
 *	Motion object updater using SLIPs
 *
 *	Updates motion objects using a SLIP read from a table, assuming a 512-pixel high playfield.
 *
 */

void atarigen_mo_update_slip_512(const UINT8 *base, int scroll, int scanline, const UINT8 *slips)
{
	/* catch a fractional character off the top of the screen */
	if (scanline == 0 && (scroll & 7) != 0)
	{
		int pfscanline = scroll & 0x1f8;
		int link = (READ_WORD(&slips[2 * (pfscanline / 8)]) >> modesc.linkshift) & modesc.linkmask;
		atarigen_mo_update(base, link, 0);
	}

	/* if we're within screen bounds, grab the next batch of MO's and process */
	if (scanline < Machine->drv->screen_height)
	{
		int pfscanline = (scanline + scroll + 7) & 0x1f8;
		int link = (READ_WORD(&slips[2 * (pfscanline / 8)]) >> modesc.linkshift) & modesc.linkmask;
		atarigen_mo_update(base, link, (pfscanline - scroll) & 0x1ff);
	}
}


/*
 *	Motion object processor
 *
 *	Processes the cached motion object entries.
 *
 */

void atarigen_mo_process(atarigen_mo_callback callback, void *param)
{
	UINT16 *base = molist;
	int last_start_scan = -1;
	struct rectangle clip;

	/* create a clipping rectangle so that only partial sections are updated at a time */
	clip.min_x = 0;
	clip.max_x = Machine->drv->screen_width - 1;

	/* loop over the list until the end */
	while (base < molist_end)
	{
		UINT16 *data, *first, *last;
		int start_scan = base[0], step;

		last_start_scan = start_scan;
		clip.min_y = start_scan;

		/* look for an entry whose scanline start is different from ours; that's our bottom */
		for (data = base; data < molist_end; data += modesc.entrywords)
			if (*data != start_scan)
			{
				clip.max_y = *data;
				break;
			}

		/* if we didn't find any additional regions, go until the bottom of the screen */
		if (data == molist_end)
			clip.max_y = Machine->drv->screen_height - 1;

		/* set the start and end points */
		if (modesc.reverse)
		{
			first = data - modesc.entrywords;
			last = base - modesc.entrywords;
			step = -modesc.entrywords;
		}
		else
		{
			first = base;
			last = data;
			step = modesc.entrywords;
		}

		/* update the base */
		base = data;

		/* render the mos */
		for (data = first; data != last; data += step)
			(*callback)(&data[1], &clip, param);
	}
}



/*--------------------------------------------------------------------------

	RLE Motion object rendering/decoding

		atarigen_rle_init - prescans the RLE objects
		atarigen_rle_free - frees all memory allocated by atarigen_rle_init
		atarigen_rle_render - render an RLE-compressed motion object

--------------------------------------------------------------------------*/

/* globals */
int atarigen_rle_count;
struct atarigen_rle_descriptor *atarigen_rle_info;

/* statics */
static UINT8 rle_region;
static UINT8 rle_bpp[8];
static UINT16 *rle_table[8];
static UINT16 *rle_colortable;

/* prototypes */
static int build_rle_tables(void);
static void prescan_rle(int which);
static void draw_rle_zoom(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip);
static void draw_rle_zoom_16(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip);
static void draw_rle_zoom_hflip(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip);
static void draw_rle_zoom_hflip_16(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip);

/*
 *	RLE motion object initialization
 *
 *	Pre-parses the motion object list and potentially pre-decompresses the data.
 *
 */

int atarigen_rle_init(int region, int colorbase)
{
	const UINT16 *base = (const UINT16 *)memory_region(region);
	int lowest_address = memory_region_length(region);
	int i;

	rle_region = region;
	rle_colortable = &Machine->remapped_colortable[colorbase];

	/* build and allocate the tables */
	if (build_rle_tables())
		return 1;

	/* first determine the lowest address of all objects */
	for (i = 0; i < lowest_address; i += 4)
	{
		int offset = ((base[i + 2] & 0xff) << 16) | base[i + 3];
		if (offset > i && offset < lowest_address)
			lowest_address = offset;
	}

	/* that determines how many objects */
	atarigen_rle_count = lowest_address / 4;
	atarigen_rle_info = (struct atarigen_rle_descriptor*)malloc(sizeof(struct atarigen_rle_descriptor) * atarigen_rle_count);
	if (!atarigen_rle_info)
	{
		atarigen_rle_free();
		return 1;
	}
	memset(atarigen_rle_info, 0, sizeof(struct atarigen_rle_descriptor) * atarigen_rle_count);

	/* now loop through and prescan the objects */
	for (i = 0; i < atarigen_rle_count; i++)
		prescan_rle(i);

	return 0;
}


/*
 *	RLE motion object free
 *
 *	Frees all memory allocated to track the motion objects.
 *
 */

void atarigen_rle_free(void)
{
	/* free the info data */
	if (atarigen_rle_info)
		free(atarigen_rle_info);
	atarigen_rle_info = NULL;

	/* free the tables */
	if (rle_table[0])
		free(rle_table[0]);
	memset(rle_table, 0, sizeof(rle_table));
}


/*
 *	RLE motion object render
 *
 *	Renders a compressed motion object.
 *
 */

void atarigen_rle_render(struct osd_bitmap *bitmap, struct atarigen_rle_descriptor *info, int color, int hflip, int vflip,
	int x, int y, int xscale, int yscale, const struct rectangle *clip)
{
	int scaled_xoffs = (xscale * info->xoffs) >> 12;
	int scaled_yoffs = (yscale * info->yoffs) >> 12;

	/* we're hflipped, account for it */
	if (hflip) scaled_xoffs = ((xscale * info->width) >> 12) - scaled_xoffs;

	/* adjust for the x and y offsets */
	x -= scaled_xoffs;
	y -= scaled_yoffs;

	/* bail on a NULL object */
	if (!info->data)
		return;

	/* 16-bit case */
	if (bitmap->depth == 16)
	{
		if (!hflip)
			draw_rle_zoom_16(bitmap, info, color, vflip, x, y, xscale << 4, yscale << 4, clip);
		else
			draw_rle_zoom_hflip_16(bitmap, info, color, vflip, x, y, xscale << 4, yscale << 4, clip);
	}

	/* 8-bit case */
	else
	{
		if (!hflip)
			draw_rle_zoom(bitmap, info, color, vflip, x, y, xscale << 4, yscale << 4, clip);
		else
			draw_rle_zoom_hflip(bitmap, info, color, vflip, x, y, xscale << 4, yscale << 4, clip);
	}
}


/*
 *	Builds internally-used tables
 *
 *	Special two-byte tables with the upper byte giving the count and the lower
 *	byte giving the pixel value.
 *
 */

static int build_rle_tables(void)
{
	UINT16 *base;
	int i;

	/* allocate all 5 tables */
	base = (UINT16*)malloc(0x500 * sizeof(UINT16));
	if (!base)
		return 1;

	/* assign the tables */
	rle_table[0] = &base[0x000];
	rle_table[1] = &base[0x100];
	rle_table[2] = rle_table[3] = &base[0x200];
	rle_table[4] = rle_table[6] = &base[0x300];
	rle_table[5] = rle_table[7] = &base[0x400];

	/* set the bpps */
	rle_bpp[0] = 4;
	rle_bpp[1] = rle_bpp[2] = rle_bpp[3] = 5;
	rle_bpp[4] = rle_bpp[5] = rle_bpp[6] = rle_bpp[7] = 6;

	/* build the 4bpp table */
	for (i = 0; i < 256; i++)
		rle_table[0][i] = (((i & 0xf0) + 0x10) << 4) | (i & 0x0f);

	/* build the 5bpp table */
	for (i = 0; i < 256; i++)
		rle_table[2][i] = (((i & 0xe0) + 0x20) << 3) | (i & 0x1f);

	/* build the special 5bpp table */
	for (i = 0; i < 256; i++)
	{
		if ((i & 0x0f) == 0)
			rle_table[1][i] = (((i & 0xf0) + 0x10) << 4) | (i & 0x0f);
		else
			rle_table[1][i] = (((i & 0xe0) + 0x20) << 3) | (i & 0x1f);
	}

	/* build the 6bpp table */
	for (i = 0; i < 256; i++)
		rle_table[5][i] = (((i & 0xc0) + 0x40) << 2) | (i & 0x3f);

	/* build the special 6bpp table */
	for (i = 0; i < 256; i++)
	{
		if ((i & 0x0f) == 0)
			rle_table[4][i] = (((i & 0xf0) + 0x10) << 4) | (i & 0x0f);
		else
			rle_table[4][i] = (((i & 0xc0) + 0x40) << 2) | (i & 0x3f);
	}

	return 0;
}


/*
 *	Prescans an RLE-compressed object
 *
 *	Determines the pen usage, width, height, and other data for an RLE object.
 *
 */

static void prescan_rle(int which)
{
	UINT16 *base = (UINT16 *)&memory_region(rle_region)[which * 8];
	struct atarigen_rle_descriptor *rle_data = &atarigen_rle_info[which];
	UINT32 usage = 0, usage_hi = 0;
	int width = 0, height, flags, offset;
	const UINT16 *table;

	/* look up the offset */
	rle_data->xoffs = (INT16)base[0];
	rle_data->yoffs = (INT16)base[1];

	/* determine the depth and table */
	flags = base[2];
	rle_data->bpp = rle_bpp[(flags >> 8) & 7];
	table = rle_data->table = rle_table[(flags >> 8) & 7];

	/* determine the starting offset */
	offset = ((base[2] & 0xff) << 16) | base[3];
	rle_data->data = base = (UINT16 *)&memory_region(rle_region)[offset * 2];

	/* make sure it's valid */
	if (offset < which * 4 || offset > memory_region_length(rle_region))
	{
		memset(rle_data, 0, sizeof(*rle_data));
		return;
	}

	/* first pre-scan to determine the width and height */
	for (height = 0; height < 1024; height++)
	{
		int tempwidth = 0;
		int entry_count = *base++;

		/* if the high bit is set, assume we're inverted */
		if (entry_count & 0x8000)
		{
			entry_count ^= 0xffff;

			/* also change the ROM data so we don't have to do this again at runtime */
			base[-1] ^= 0xffff;
		}

		/* we're done when we hit 0 */
		if (entry_count == 0)
			break;

		/* track the width */
		while (entry_count--)
		{
			int word = *base++;
			int count, value;

			/* decode the low byte first */
			count = table[word & 0xff];
			value = count & 0xff;
			tempwidth += count >> 8;
			if (value < 32)
				usage |= 1 << value;
			else
				usage_hi |= 1 << (value - 32);

			/* decode the upper byte second */
			count = table[word >> 8];
			value = count & 0xff;
			tempwidth += count >> 8;
			if (value < 32)
				usage |= 1 << value;
			else
				usage_hi |= 1 << (value - 32);
		}

		/* only remember the max */
		if (tempwidth > width) width = tempwidth;
	}

	/* fill in the data */
	rle_data->width = width;
	rle_data->height = height;
	rle_data->pen_usage = usage;
	rle_data->pen_usage_hi = usage_hi;
}


/*
 *	Draw a compressed RLE object
 *
 *	What it says. RLE decoding is performed on the fly to an 8-bit bitmap.
 *
 */

void draw_rle_zoom(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip)
{
	const UINT16 *palette = &rle_colortable[color];
	const UINT16 *row_start = gfx->data;
	const UINT16 *table = gfx->table;
	volatile int current_row = 0;

	int scaled_width = (scalex * gfx->width + 0x7fff) >> 16;
	int scaled_height = (scaley * gfx->height + 0x7fff) >> 16;

	int pixels_to_skip = 0, xclipped = 0;
	int dx, dy, ex, ey;
	int y, sourcey;

	/* make sure we didn't end up with 0 */
	if (scaled_width == 0) scaled_width = 1;
	if (scaled_height == 0) scaled_height = 1;

	/* compute the remaining parameters */
	dx = (gfx->width << 16) / scaled_width;
	dy = (gfx->height << 16) / scaled_height;
	ex = sx + scaled_width - 1;
	ey = sy + scaled_height - 1;
	sourcey = dy / 2;

	/* left edge clip */
	if (sx < clip->min_x)
		pixels_to_skip = clip->min_x - sx, xclipped = 1;
	if (sx > clip->max_x)
		return;

	/* right edge clip */
	if (ex > clip->max_x)
		ex = clip->max_x, xclipped = 1;
	else if (ex < clip->min_x)
		return;

	/* top edge clip */
	if (sy < clip->min_y)
	{
		sourcey += (clip->min_y - sy) * dy;
		sy = clip->min_y;
	}
	else if (sy > clip->max_y)
		return;

	/* bottom edge clip */
	if (ey > clip->max_y)
		ey = clip->max_y;
	else if (ey < clip->min_y)
		return;

	/* loop top to bottom */
	for (y = sy; y <= ey; y++, sourcey += dy)
	{
		UINT8 *dest = &bitmap->line[y][sx];
		int j, sourcex = dx / 2, rle_end = 0;
		const UINT16 *base;
		int entry_count;

		/* loop until we hit the row we're on */
		for ( ; current_row != (sourcey >> 16); current_row++)
			row_start += 1 + *row_start;

		/* grab our starting parameters from this row */
		base = row_start;
		entry_count = *base++;

		/* non-clipped case */
		if (!xclipped)
		{
			/* decode the pixels */
			for (j = 0; j < entry_count; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}

				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}
			}
		}

		/* clipped case */
		else
		{
			const UINT8 *end = &bitmap->line[y][ex];
			int to_be_skipped = pixels_to_skip;

			/* decode the pixels */
			for (j = 0; j < entry_count && dest <= end; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest++, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next1;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest <= end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}

			next1:
				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest++, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next2;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest <= end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}
			next2:
				;
			}
		}
	}
}


/*
 *	Draw a compressed RLE object
 *
 *	What it says. RLE decoding is performed on the fly to a 16-bit bitmap.
 *
 */

void draw_rle_zoom_16(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip)
{
	const UINT16 *palette = &rle_colortable[color];
	const UINT16 *row_start = gfx->data;
	const UINT16 *table = gfx->table;
	volatile int current_row = 0;

	int scaled_width = (scalex * gfx->width + 0x7fff) >> 16;
	int scaled_height = (scaley * gfx->height + 0x7fff) >> 16;

	int pixels_to_skip = 0, xclipped = 0;
	int dx, dy, ex, ey;
	int y, sourcey;

	/* make sure we didn't end up with 0 */
	if (scaled_width == 0) scaled_width = 1;
	if (scaled_height == 0) scaled_height = 1;

	/* compute the remaining parameters */
	dx = (gfx->width << 16) / scaled_width;
	dy = (gfx->height << 16) / scaled_height;
	ex = sx + scaled_width - 1;
	ey = sy + scaled_height - 1;
	sourcey = dy / 2;

	/* left edge clip */
	if (sx < clip->min_x)
		pixels_to_skip = clip->min_x - sx, xclipped = 1;
	if (sx > clip->max_x)
		return;

	/* right edge clip */
	if (ex > clip->max_x)
		ex = clip->max_x, xclipped = 1;
	else if (ex < clip->min_x)
		return;

	/* top edge clip */
	if (sy < clip->min_y)
	{
		sourcey += (clip->min_y - sy) * dy;
		sy = clip->min_y;
	}
	else if (sy > clip->max_y)
		return;

	/* bottom edge clip */
	if (ey > clip->max_y)
		ey = clip->max_y;
	else if (ey < clip->min_y)
		return;

	/* loop top to bottom */
	for (y = sy; y <= ey; y++, sourcey += dy)
	{
		UINT16 *dest = (UINT16 *)&bitmap->line[y][sx * 2];
		int j, sourcex = dx / 2, rle_end = 0;
		const UINT16 *base;
		int entry_count;

		/* loop until we hit the row we're on */
		for ( ; current_row != (sourcey >> 16); current_row++)
			row_start += 1 + *row_start;

		/* grab our starting parameters from this row */
		base = row_start;
		entry_count = *base++;

		/* non-clipped case */
		if (!xclipped)
		{
			/* decode the pixels */
			for (j = 0; j < entry_count; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}

				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}
			}
		}

		/* clipped case */
		else
		{
			const UINT16 *end = (const UINT16 *)&bitmap->line[y][ex * 2];
			int to_be_skipped = pixels_to_skip;

			/* decode the pixels */
			for (j = 0; j < entry_count && dest <= end; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest++, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next3;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest <= end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}

			next3:
				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest++, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next4;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest <= end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}
			next4:
				;
			}
		}
	}
}


/*
 *	Draw a horizontally-flipped RLE-compressed object
 *
 *	What it says. RLE decoding is performed on the fly to an 8-bit bitmap.
 *
 */

void draw_rle_zoom_hflip(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip)
{
	const UINT16 *palette = &rle_colortable[color];
	const UINT16 *row_start = gfx->data;
	const UINT16 *table = gfx->table;
	volatile int current_row = 0;

	int scaled_width = (scalex * gfx->width + 0x7fff) >> 16;
	int scaled_height = (scaley * gfx->height + 0x7fff) >> 16;
	int pixels_to_skip = 0, xclipped = 0;
	int dx, dy, ex, ey;
	int y, sourcey;

	/* make sure we didn't end up with 0 */
	if (scaled_width == 0) scaled_width = 1;
	if (scaled_height == 0) scaled_height = 1;

	/* compute the remaining parameters */
	dx = (gfx->width << 16) / scaled_width;
	dy = (gfx->height << 16) / scaled_height;
	ex = sx + scaled_width - 1;
	ey = sy + scaled_height - 1;
	sourcey = dy / 2;

	/* left edge clip */
	if (sx < clip->min_x)
		sx = clip->min_x, xclipped = 1;
	if (sx > clip->max_x)
		return;

	/* right edge clip */
	if (ex > clip->max_x)
		pixels_to_skip = ex - clip->max_x, xclipped = 1;
	else if (ex < clip->min_x)
		return;

	/* top edge clip */
	if (sy < clip->min_y)
	{
		sourcey += (clip->min_y - sy) * dy;
		sy = clip->min_y;
	}
	else if (sy > clip->max_y)
		return;

	/* bottom edge clip */
	if (ey > clip->max_y)
		ey = clip->max_y;
	else if (ey < clip->min_y)
		return;

	/* loop top to bottom */
	for (y = sy; y <= ey; y++, sourcey += dy)
	{
		UINT8 *dest = &bitmap->line[y][ex];
		int j, sourcex = dx / 2, rle_end = 0;
		const UINT16 *base;
		int entry_count;

		/* loop until we hit the row we're on */
		for ( ; current_row != (sourcey >> 16); current_row++)
			row_start += 1 + *row_start;

		/* grab our starting parameters from this row */
		base = row_start;
		entry_count = *base++;

		/* non-clipped case */
		if (!xclipped)
		{
			/* decode the pixels */
			for (j = 0; j < entry_count; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}

				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}
			}
		}

		/* clipped case */
		else
		{
			const UINT8 *start = &bitmap->line[y][sx];
			int to_be_skipped = pixels_to_skip;

			/* decode the pixels */
			for (j = 0; j < entry_count && dest >= start; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest--, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next1;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest >= start)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}

			next1:
				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest--, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next2;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest >= start)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}
			next2:
				;
			}
		}
	}
}


/*
 *	Draw a horizontally-flipped RLE-compressed object
 *
 *	What it says. RLE decoding is performed on the fly to a 16-bit bitmap.
 *
 */

void draw_rle_zoom_hflip_16(struct osd_bitmap *bitmap, const struct atarigen_rle_descriptor *gfx,
		UINT32 color, int flipy, int sx, int sy, int scalex, int scaley,
		const struct rectangle *clip)
{
	const UINT16 *palette = &rle_colortable[color];
	const UINT16 *row_start = gfx->data;
	const UINT16 *table = gfx->table;
	volatile int current_row = 0;

	int scaled_width = (scalex * gfx->width + 0x7fff) >> 16;
	int scaled_height = (scaley * gfx->height + 0x7fff) >> 16;
	int pixels_to_skip = 0, xclipped = 0;
	int dx, dy, ex, ey;
	int y, sourcey;

	/* make sure we didn't end up with 0 */
	if (scaled_width == 0) scaled_width = 1;
	if (scaled_height == 0) scaled_height = 1;

	/* compute the remaining parameters */
	dx = (gfx->width << 16) / scaled_width;
	dy = (gfx->height << 16) / scaled_height;
	ex = sx + scaled_width - 1;
	ey = sy + scaled_height - 1;
	sourcey = dy / 2;

	/* left edge clip */
	if (sx < clip->min_x)
		sx = clip->min_x, xclipped = 1;
	if (sx > clip->max_x)
		return;

	/* right edge clip */
	if (ex > clip->max_x)
		pixels_to_skip = ex - clip->max_x, xclipped = 1;
	else if (ex < clip->min_x)
		return;

	/* top edge clip */
	if (sy < clip->min_y)
	{
		sourcey += (clip->min_y - sy) * dy;
		sy = clip->min_y;
	}
	else if (sy > clip->max_y)
		return;

	/* bottom edge clip */
	if (ey > clip->max_y)
		ey = clip->max_y;
	else if (ey < clip->min_y)
		return;

	/* loop top to bottom */
	for (y = sy; y <= ey; y++, sourcey += dy)
	{
		UINT16 *dest = (UINT16 *)&bitmap->line[y][ex * 2];
		int j, sourcex = dx / 2, rle_end = 0;
		const UINT16 *base;
		int entry_count;

		/* loop until we hit the row we're on */
		for ( ; current_row != (sourcey >> 16); current_row++)
			row_start += 1 + *row_start;

		/* grab our starting parameters from this row */
		base = row_start;
		entry_count = *base++;

		/* non-clipped case */
		if (!xclipped)
		{
			/* decode the pixels */
			for (j = 0; j < entry_count; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}

				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}
			}
		}

		/* clipped case */
		else
		{
			const UINT16 *start = (const UINT16 *)&bitmap->line[y][sx * 2];
			int to_be_skipped = pixels_to_skip;

			/* decode the pixels */
			for (j = 0; j < entry_count && dest >= start; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest--, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next3;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest >= start)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}

			next3:
				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest--, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next4;
				}
				if (value)
				{
					value = palette[value];
					while (sourcex < rle_end && dest >= start)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}
			next4:
				;
			}
		}
	}
}


/*--------------------------------------------------------------------------

	Playfield rendering

		atarigen_pf_state - data block describing the playfield

		atarigen_pf_callback - called back for each chunk during processing

		atarigen_pf_init - initializes and configures the playfield state
		atarigen_pf_free - frees all memory allocated by atarigen_pf_init
		atarigen_pf_reset - reset for a new frame (use only if not using interrupt system)
		atarigen_pf_update - updates the playfield state for the given scanline
		atarigen_pf_process - processes the current list of parameters

		atarigen_pf2_init - same as above but for a second playfield
		atarigen_pf2_free - same as above but for a second playfield
		atarigen_pf2_reset - same as above but for a second playfield
		atarigen_pf2_update - same as above but for a second playfield
		atarigen_pf2_process - same as above but for a second playfield

--------------------------------------------------------------------------*/

/* types */
struct playfield_data
{
	struct osd_bitmap *bitmap;
	UINT8 *dirty;
	UINT8 *visit;

	int tilewidth;
	int tileheight;
	int tilewidth_shift;
	int tileheight_shift;
	int xtiles_mask;
	int ytiles_mask;

	int entries;
	int *scanline;
	struct atarigen_pf_state *state;
	struct atarigen_pf_state *last_state;
};

/* globals */
struct osd_bitmap *atarigen_pf_bitmap;
UINT8 *atarigen_pf_dirty;
UINT8 *atarigen_pf_visit;

struct osd_bitmap *atarigen_pf2_bitmap;
UINT8 *atarigen_pf2_dirty;
UINT8 *atarigen_pf2_visit;

struct osd_bitmap *atarigen_pf_overrender_bitmap;
UINT16 atarigen_overrender_colortable[32];

/* statics */
static struct playfield_data playfield;
static struct playfield_data playfield2;

/* prototypes */
static int internal_pf_init(struct playfield_data *pf, const struct atarigen_pf_desc *source_desc);
static void internal_pf_free(struct playfield_data *pf);
static void internal_pf_reset(struct playfield_data *pf);
static void internal_pf_update(struct playfield_data *pf, const struct atarigen_pf_state *state, int scanline);
static void internal_pf_process(struct playfield_data *pf, atarigen_pf_callback callback, void *param, const struct rectangle *clip);
static int compute_shift(int size);
static int compute_mask(int count);


/*
 *	Playfield render initialization
 *
 *	Allocates memory for the playfield and initializes all structures.
 *
 */

static int internal_pf_init(struct playfield_data *pf, const struct atarigen_pf_desc *source_desc)
{
	/* allocate the bitmap */
	if (!source_desc->noscroll)
		pf->bitmap = bitmap_alloc(source_desc->tilewidth * source_desc->xtiles,
									source_desc->tileheight * source_desc->ytiles);
	else
		pf->bitmap = bitmap_alloc(Machine->drv->screen_width,
									Machine->drv->screen_height);
	if (!pf->bitmap)
		return 1;

	/* allocate the dirty tile map */
	pf->dirty = (UINT8*)malloc(source_desc->xtiles * source_desc->ytiles);
	if (!pf->dirty)
	{
		internal_pf_free(pf);
		return 1;
	}
	memset(pf->dirty, 0xff, source_desc->xtiles * source_desc->ytiles);

	/* allocate the visitation map */
	pf->visit = (UINT8*)malloc(source_desc->xtiles * source_desc->ytiles);
	if (!pf->visit)
	{
		internal_pf_free(pf);
		return 1;
	}

	/* allocate the list of scanlines */
	pf->scanline = (int*)malloc(source_desc->ytiles * source_desc->tileheight * sizeof(int));
	if (!pf->scanline)
	{
		internal_pf_free(pf);
		return 1;
	}

	/* allocate the list of parameters */
	pf->state = (struct atarigen_pf_state*)malloc(source_desc->ytiles * source_desc->tileheight * sizeof(struct atarigen_pf_state));
	if (!pf->state)
	{
		internal_pf_free(pf);
		return 1;
	}

	/* copy the basic data */
	pf->tilewidth = source_desc->tilewidth;
	pf->tileheight = source_desc->tileheight;
	pf->tilewidth_shift = compute_shift(source_desc->tilewidth);
	pf->tileheight_shift = compute_shift(source_desc->tileheight);
	pf->xtiles_mask = compute_mask(source_desc->xtiles);
	pf->ytiles_mask = compute_mask(source_desc->ytiles);

	/* initialize the last state to all zero */
	pf->last_state = pf->state;
	memset(pf->last_state, 0, sizeof(*pf->last_state));

	/* reset */
	internal_pf_reset(pf);

	return 0;
}

int atarigen_pf_init(const struct atarigen_pf_desc *source_desc)
{
	int result = internal_pf_init(&playfield, source_desc);
	if (!result)
	{
		/* allocate the overrender bitmap */
		atarigen_pf_overrender_bitmap = bitmap_alloc(Machine->drv->screen_width, Machine->drv->screen_height);
		if (!atarigen_pf_overrender_bitmap)
		{
			internal_pf_free(&playfield);
			return 1;
		}

		atarigen_pf_bitmap = playfield.bitmap;
		atarigen_pf_dirty = playfield.dirty;
		atarigen_pf_visit = playfield.visit;
	}
	return result;
}

int atarigen_pf2_init(const struct atarigen_pf_desc *source_desc)
{
	int result = internal_pf_init(&playfield2, source_desc);
	if (!result)
	{
		atarigen_pf2_bitmap = playfield2.bitmap;
		atarigen_pf2_dirty = playfield2.dirty;
		atarigen_pf2_visit = playfield2.visit;
	}
	return result;
}


/*
 *	Playfield render free
 *
 *	Frees all memory allocated by the playfield system.
 *
 */

static void internal_pf_free(struct playfield_data *pf)
{
	if (pf->bitmap)
		bitmap_free(pf->bitmap);
	pf->bitmap = NULL;

	if (pf->dirty)
		free(pf->dirty);
	pf->dirty = NULL;

	if (pf->visit)
		free(pf->visit);
	pf->visit = NULL;

	if (pf->scanline)
		free(pf->scanline);
	pf->scanline = NULL;

	if (pf->state)
		free(pf->state);
	pf->state = NULL;
}

void atarigen_pf_free(void)
{
	internal_pf_free(&playfield);

	/* free the overrender bitmap */
	if (atarigen_pf_overrender_bitmap)
		bitmap_free(atarigen_pf_overrender_bitmap);
	atarigen_pf_overrender_bitmap = NULL;
}

void atarigen_pf2_free(void)
{
	internal_pf_free(&playfield2);
}


/*
 *	Playfield render reset
 *
 *	Resets the playfield system for a new frame. Note that this is automatically called
 *	if you're using the interrupt system.
 *
 */

void internal_pf_reset(struct playfield_data *pf)
{
	/* verify memory has been allocated -- we're called even if we're not used */
	if (pf->scanline && pf->state)
	{
		pf->entries = 0;
		internal_pf_update(pf, pf->last_state, 0);
	}
}

void atarigen_pf_reset(void)
{
	internal_pf_reset(&playfield);
}

void atarigen_pf2_reset(void)
{
	internal_pf_reset(&playfield2);
}


/*
 *	Playfield render update
 *
 *	Sets the parameters for a given scanline.
 *
 */

void internal_pf_update(struct playfield_data *pf, const struct atarigen_pf_state *state, int scanline)
{
	if (pf->entries > 0)
	{
		/* if the current scanline matches the previous one, just overwrite */
		if (pf->scanline[pf->entries - 1] == scanline)
			pf->entries--;

		/* if the current data matches the previous data, ignore it */
		else if (pf->last_state->hscroll == state->hscroll &&
				 pf->last_state->vscroll == state->vscroll &&
				 pf->last_state->param[0] == state->param[0] &&
				 pf->last_state->param[1] == state->param[1])
			return;
	}

	/* remember this entry as the last set of parameters */
	pf->last_state = &pf->state[pf->entries];

	/* copy in the data */
	pf->scanline[pf->entries] = scanline;
	pf->state[pf->entries++] = *state;

	/* set the final scanline to be huge -- it will be clipped during processing */
	pf->scanline[pf->entries] = 100000;
}

void atarigen_pf_update(const struct atarigen_pf_state *state, int scanline)
{
	internal_pf_update(&playfield, state, scanline);
}

void atarigen_pf2_update(const struct atarigen_pf_state *state, int scanline)
{
	internal_pf_update(&playfield2, state, scanline);
}


/*
 *	Playfield render process
 *
 *	Processes the playfield in chunks.
 *
 */

void internal_pf_process(struct playfield_data *pf, atarigen_pf_callback callback, void *param, const struct rectangle *clip)
{
	struct rectangle curclip;
	struct rectangle tiles;
	int y;

	/* preinitialization */
	curclip.min_x = clip->min_x;
	curclip.max_x = clip->max_x;

	/* loop over all entries */
	for (y = 0; y < pf->entries; y++)
	{
		struct atarigen_pf_state *current = &pf->state[y];

		/* determine the clip rect */
		curclip.min_y = pf->scanline[y];
		curclip.max_y = pf->scanline[y + 1] - 1;

		/* skip if we're clipped out */
		if (curclip.min_y > clip->max_y || curclip.max_y < clip->min_y)
			continue;

		/* clip the clipper */
		if (curclip.min_y < clip->min_y)
			curclip.min_y = clip->min_y;
		if (curclip.max_y > clip->max_y)
			curclip.max_y = clip->max_y;

		/* determine the tile rect */
		tiles.min_x = ((current->hscroll + curclip.min_x) >> pf->tilewidth_shift) & pf->xtiles_mask;
		tiles.max_x = ((current->hscroll + curclip.max_x + pf->tilewidth) >> pf->tilewidth_shift) & pf->xtiles_mask;
		tiles.min_y = ((current->vscroll + curclip.min_y) >> pf->tileheight_shift) & pf->ytiles_mask;
		tiles.max_y = ((current->vscroll + curclip.max_y + pf->tileheight) >> pf->tileheight_shift) & pf->ytiles_mask;

		/* call the callback */
		(*callback)(&curclip, &tiles, current, param);
	}
}

void atarigen_pf_process(atarigen_pf_callback callback, void *param, const struct rectangle *clip)
{
	internal_pf_process(&playfield, callback, param, clip);
}

void atarigen_pf2_process(atarigen_pf_callback callback, void *param, const struct rectangle *clip)
{
	internal_pf_process(&playfield2, callback, param, clip);
}


/*
 *	Shift value computer
 *
 *	Determines the log2(value).
 *
 */

static int compute_shift(int size)
{
	int i;

	/* loop until we shift to zero */
	for (i = 0; i < 32; i++)
		if (!(size >>= 1))
			break;
	return i;
}


/*
 *	Mask computer
 *
 *	Determines the best mask to use for the given value.
 *
 */

static int compute_mask(int count)
{
	int shift = compute_shift(count);

	/* simple case - count is an even power of 2 */
	if (count == (1 << shift))
		return count - 1;

	/* slightly less simple case - round up to the next power of 2 */
	else
		return (1 << (shift + 1)) - 1;
}





/*--------------------------------------------------------------------------

	Misc Video stuff

		atarigen_get_hblank - returns the current HBLANK state
		atarigen_halt_until_hblank_0_w - write handler for a HBLANK halt
		atarigen_666_paletteram_w - 6-6-6 special RGB paletteram handler
		atarigen_expanded_666_paletteram_w - byte version of above

--------------------------------------------------------------------------*/

/* prototypes */
static void unhalt_cpu(int param);


/*
 *	Compute HBLANK state
 *
 *	Returns a guesstimate about the current HBLANK state, based on the assumption that
 *	HBLANK represents 10% of the scanline period.
 *
 */

int atarigen_get_hblank(void)
{
	return (cpu_gethorzbeampos() > (Machine->drv->screen_width * 9 / 10));
}


/*
 *	Halt CPU 0 until HBLANK
 *
 *	What it says.
 *
 */

WRITE_HANDLER( atarigen_halt_until_hblank_0_w )
{
	/* halt the CPU until the next HBLANK */
	int hpos = cpu_gethorzbeampos();
	int hblank = Machine->drv->screen_width * 9 / 10;
	float fraction;

	/* if we're in hblank, set up for the next one */
	if (hpos >= hblank)
		hblank += Machine->drv->screen_width;

	/* halt and set a timer to wake up */
	fraction = (float)(hblank - hpos) / (float)Machine->drv->screen_width;
	timer_set(cpu_getscanlineperiod() * fraction, 0, unhalt_cpu);
	cpu_set_halt_line(0, ASSERT_LINE);
}


/*
 *	6-6-6 RGB palette RAM handler
 *
 *	What it says.
 *
 */

WRITE_HANDLER( atarigen_666_paletteram_w )
{
	int oldword = READ_WORD(&paletteram[offset]);
	int newword = COMBINE_WORD(oldword,data);
	WRITE_WORD(&paletteram[offset],newword);

	{
		int r, g, b;

		r = ((newword >> 9) & 0x3e) | ((newword >> 15) & 1);
		g = ((newword >> 4) & 0x3e) | ((newword >> 15) & 1);
		b = ((newword << 1) & 0x3e) | ((newword >> 15) & 1);

		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);

		palette_change_color(offset / 2, r, g, b);
	}
}


/*
 *	6-6-6 RGB expanded palette RAM handler
 *
 *	What it says.
 *
 */

WRITE_HANDLER( atarigen_expanded_666_paletteram_w )
{
	COMBINE_WORD_MEM(&paletteram[offset], data);

	if (!(data & 0xff000000))
	{
		int palentry = offset / 4;
		int newword = (READ_WORD(&paletteram[palentry * 4]) & 0xff00) | (READ_WORD(&paletteram[palentry * 4 + 2]) >> 8);

		int r, g, b;

		r = ((newword >> 9) & 0x3e) | ((newword >> 15) & 1);
		g = ((newword >> 4) & 0x3e) | ((newword >> 15) & 1);
		b = ((newword << 1) & 0x3e) | ((newword >> 15) & 1);

		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);

		palette_change_color(palentry & 0x1ff, r, g, b);
	}
}


/*
 *	CPU unhalter
 *
 *	Timer callback to release the CPU from a halted state.
 *
 */

static void unhalt_cpu(int param)
{
	cpu_set_halt_line(param, CLEAR_LINE);
}



/*--------------------------------------------------------------------------

	General stuff

		atarigen_show_slapstic_message - display warning about slapstic
		atarigen_show_sound_message - display warning about coins
		atarigen_update_messages - update messages

--------------------------------------------------------------------------*/

/* statics */
static char *message_text[10];
static int message_countdown;

/*
 *	Display a warning message about slapstic protection
 *
 *	What it says.
 *
 */

void atarigen_show_slapstic_message(void)
{
	message_text[0] = "There are known problems with";
	message_text[1] = "later levels of this game due";
	message_text[2] = "to incomplete slapstic emulation.";
	message_text[3] = "You have been warned.";
	message_text[4] = NULL;
	message_countdown = 15 * Machine->drv->frames_per_second;
}


/*
 *	Display a warning message about sound being disabled
 *
 *	What it says.
 *
 */

void atarigen_show_sound_message(void)
{
	if (Machine->sample_rate == 0)
	{
		message_text[0] = "This game may have trouble accepting";
		message_text[1] = "coins, or may even behave strangely,";
		message_text[2] = "because you have disabled sound.";
		message_text[3] = NULL;
		message_countdown = 15 * Machine->drv->frames_per_second;
	}
}


/*
 *	Update on-screen messages
 *
 *	What it says.
 *
 */

void atarigen_update_messages(void)
{
	if (message_countdown && message_text[0])
	{
		int maxwidth = 0;
		int lines, x, y, i, j;

		/* first count lines and determine the maximum width */
		for (lines = 0; lines < 10; lines++)
		{
			if (!message_text[lines]) break;
			x = strlen(message_text[lines]);
			if (x > maxwidth) maxwidth = x;
		}
		maxwidth += 2;

		/* determine y offset */
		x = (Machine->uiwidth - Machine->uifontwidth * maxwidth) / 2;
		y = (Machine->uiheight - Machine->uifontheight * (lines + 2)) / 2;

		/* draw a row of spaces at the top and bottom */
		for (i = 0; i < maxwidth; i++)
		{
			ui_text(Machine->scrbitmap, " ", x + i * Machine->uifontwidth, y);
			ui_text(Machine->scrbitmap, " ", x + i * Machine->uifontwidth, y + (lines + 1) * Machine->uifontheight);
		}
		y += Machine->uifontheight;

		/* draw the message */
		for (i = 0; i < lines; i++)
		{
			int width = strlen(message_text[i]) * Machine->uifontwidth;
			int dx = (Machine->uifontwidth * maxwidth - width) / 2;

			for (j = 0; j < dx; j += Machine->uifontwidth)
			{
				ui_text(Machine->scrbitmap, " ", x + j, y);
				ui_text(Machine->scrbitmap, " ", x + (maxwidth - 1) * Machine->uifontwidth - j, y);
			}

			ui_text(Machine->scrbitmap, message_text[i], x + dx, y);
			y += Machine->uifontheight;
		}

		/* decrement the counter */
		message_countdown--;

		/* if a coin is inserted, make the message go away */
		if (keyboard_pressed(KEYCODE_5) || keyboard_pressed(KEYCODE_6) ||
		    keyboard_pressed(KEYCODE_7) || keyboard_pressed(KEYCODE_8))
			message_countdown = 0;
	}
	else
		message_text[0] = NULL;
}



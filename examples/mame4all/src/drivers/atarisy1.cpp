#include "../vidhrdw/atarisy1.cpp"

/***************************************************************************

	Atari System 1 hardware

	driver by Aaron Giles

	Games supported:
		* Marble Madness (1984) [3 sets]
		* Peter Packrat (1984)
		* Indiana Jones & the Temple of Doom (1985) [4 sets]
		* Road Runner (1985)
		* Road Blasters (1987)

	Known bugs:
		* none at this time

****************************************************************************

	Memory map

****************************************************************************

	========================================================================
	MAIN CPU
	========================================================================
	000000-07FFFF   R     xxxxxxxx xxxxxxxx   Program ROM
	080000-087FFF   R     xxxxxxxx xxxxxxxx   Slapstic-protected ROM
	2E0000          R     -------- x-------   Sprite interrupt state
	400000-401FFF   R/W   xxxxxxxx xxxxxxxx   Program RAM
	800000            W   -------x xxxxxxxx   Playfield X scroll
	820000            W   -------x xxxxxxxx   Playfield Y scroll
	840000            W   -------- xxxxxxxx   Playfield priority color mask
	860000            W   -------- xxxxxxxx   Audio/video control
	                  W   -------- x-------      (Sound CPU reset)
	                  W   -------- -x------      (Trackball test)
	                  W   -------- --xxx---      (Motion object RAM bank select)
	                  W   -------- -----x--      (Playfield tile bank select)
	                  W   -------- ------x-      (Trackball resolution & test LED)
	                  W   -------- -------x      (Alphanumerics tile bank select)
	880000            W   -------- --------   Watchdog reset
	8A0000            W   -------- --------   VBLANK IRQ acknowledge
	8C0000            W   -------- --------   EEPROM enable
	900000-9FFFFF   R/W   xxxxxxxx xxxxxxxx   Catridge external RAM/ROM
	A00000-A01FFF   R/W   xxxxxxxx xxxxxxxx   Playfield RAM (64x64 tiles)
	                R/W   x------- --------      (Horizontal flip)
	                R/W   -xxxxxxx --------      (Tile ROM & palette select)
	                R/W   -------- xxxxxxxx      (Tile index, 8 LSB)
	A02000-A02FFF   R/W   xxxxxxxx xxxxxxxx   Motion object RAM (8 banks x 64 entries x 4 words)
	                R/W   x------- --------      (0: X flip)
	                R/W   --xxxxxx xxx-----      (0: Y position)
	                R/W   -------- ----xxxx      (0: Number of Y tiles - 1)
	                R/W   xxxxxxxx --------      (64: Tile ROM & palette select)
	                R/W   -------- xxxxxxxx      (64: Tile index, 8 LSB)
	                R/W   --xxxxxx xxx-----      (128: X position)
	                R/W   -------- ----xxxx      (128: Number of X tiles - 1)
	                R/W   -------- --xxxxxx      (192: Link to the next object)
	A03000-A03FFF   R/W   --xxxxxx xxxxxxxx   Alphanumerics RAM
	                R/W   --x----- --------      (Opaque/transparent)
	                R/W   ---xxx-- --------      (Palette index)
	                R/W   ------xx xxxxxxxx      (Tile index)
	B00000-B001FF   R/W   xxxxxxxx xxxxxxxx   Alphanumerics palette RAM (256 entries)
	                R/W   xxxx---- --------      (Intensity)
	                R/W   ----xxxx --------      (Red)
	                R/W   -------- xxxx----      (Green)
	                R/W   -------- ----xxxx      (Blue)
	B00200-B003FF   R/W   xxxxxxxx xxxxxxxx   Motion object palette RAM (256 entries)
	B00400-B005FF   R/W   xxxxxxxx xxxxxxxx   Playfield palette RAM (256 entries)
	B00600-B007FF   R/W   xxxxxxxx xxxxxxxx   Translucency palette RAM (256 entries)
	F00000-F00FFF   R/W   -------- xxxxxxxx   EEPROM
	F20000-F20007   R     -------- xxxxxxxx   Analog inputs
	F40000-F4001F   R     -------- xxxxxxxx   Joystick inputs
	F40000-F4001F     W   -------- --------   Joystick IRQ enable
	F60000          R     -------- xxxxxxxx   Switch inputs
	                R     -------- x-------      (Command buffer full)
	                R     -------- -x------      (Self test)
	                R     -------- --x-xxxx      (Game-specific switches)
	                R     -------- ---x----      (VBLANK)
	FC0000          R     -------- xxxxxxxx   Sound response read
	FE0000            W   -------- xxxxxxxx   Sound command write
	========================================================================
	Interrupts:
		IRQ2 = joystick interrupt
		IRQ3 = sprite-based interrupt
		IRQ4 = VBLANK
		IRQ6 = sound CPU communications
	========================================================================


	========================================================================
	SOUND CPU
	========================================================================
	0000-0FFF   R/W   xxxxxxxx   Program RAM
	1000-100F   R/W   xxxxxxxx   M6522
	1000-1FFF   R/W   xxxxxxxx   Catridge external RAM/ROM
	1800-1801   R/W   xxxxxxxx   YM2151 communications
	1810        R     xxxxxxxx   Sound command read
	1810          W   xxxxxxxx   Sound response write
	1820        R     x--xxxxx   Sound status/input read
	            R     x-------      (Self-test)
	            R     ---x----      (Response buffer full)
	            R     ----x---      (Command buffer full)
	            R     -----x--      (Service coin)
	            R     ------x-      (Left coin)
	            R     -------x      (Right coin)
	1824-1825     W   -------x   LED control
	1826          W   -------x   Right coin counter
	1827          W   -------x   Left coin counter
	1870-187F   R/W   xxxxxxxx   POKEY communications
	4000-FFFF   R     xxxxxxxx   Program ROM
	========================================================================
	Interrupts:
		IRQ = YM2151 interrupt
		NMI = latch on sound command
	========================================================================

****************************************************************************/


#include "driver.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"


extern UINT8 *atarisys1_bankselect;
extern UINT8 *atarisys1_prioritycolor;

READ_HANDLER( atarisys1_int3state_r );

WRITE_HANDLER( atarisys1_playfieldram_w );
WRITE_HANDLER( atarisys1_spriteram_w );
WRITE_HANDLER( atarisys1_bankselect_w );
WRITE_HANDLER( atarisys1_hscroll_w );
WRITE_HANDLER( atarisys1_vscroll_w );

void atarisys1_scanline_update(int scanline);

int atarisys1_vh_start(void);
void atarisys1_vh_stop(void);
void atarisys1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static UINT8 joystick_type;
static UINT8 trackball_type;

static void *joystick_timer;
static UINT8 joystick_int;
static UINT8 joystick_int_enable;
static UINT8 joystick_value;

static UINT8 m6522_ddra, m6522_ddrb;
static UINT8 m6522_dra, m6522_drb;
static UINT8 m6522_regs[16];

static UINT8 *marble_speedcheck;
static UINT32 speedcheck_time1, speedcheck_time2;



/*************************************
 *
 *	Initialization of globals.
 *
 *************************************/

static void update_interrupts(void)
{
	int newstate = 0;

	/* all interrupts go through an LS148, which gives priority to the highest */
	if (joystick_int && joystick_int_enable)
		newstate = 2;
	if (atarigen_scanline_int_state)
		newstate = 3;
	if (atarigen_video_int_state)
		newstate = 4;
	if (atarigen_sound_int_state)
		newstate = 6;

	/* set the new state of the IRQ lines */
	if (newstate)
		cpu_set_irq_line(0, newstate, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 7, CLEAR_LINE);
}


static void init_machine(void)
{
	/* initialize the system */
	atarigen_eeprom_reset();
	atarigen_slapstic_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(atarisys1_scanline_update, 8);
	atarigen_sound_io_reset(1);

	/* reset the joystick parameters */
	joystick_value = 0;
	joystick_timer = NULL;
	joystick_int = 0;
	joystick_int_enable = 0;

	/* reset the 6522 controller */
	m6522_ddra = m6522_ddrb = 0xff;
	m6522_dra = m6522_drb = 0xff;
	memset(m6522_regs, 0xff, sizeof(m6522_regs));

	/* reset the Marble Madness speedup checks */
	speedcheck_time1 = speedcheck_time2 = 0;
}



/*************************************
 *
 *	LED handlers.
 *
 *************************************/

static WRITE_HANDLER( led_w )
{
	osd_led_w(offset, ~data & 1);
}



/*************************************
 *
 *	Joystick read.
 *
 *************************************/

static void delayed_joystick_int(int param)
{
	joystick_timer = NULL;
	joystick_value = param;
	joystick_int = 1;
	atarigen_update_interrupts();
}


static READ_HANDLER( joystick_r )
{
	int newval = 0xff;

	/* digital joystick type */
	if (joystick_type == 1)
		newval = (input_port_0_r(offset) & (0x80 >> (offset / 2))) ? 0xf0 : 0x00;

	/* Hall-effect analog joystick */
	else if (joystick_type == 2)
		newval = (offset & 2) ? input_port_0_r(offset) : input_port_1_r(offset);

	/* Road Blasters gas pedal */
	else if (joystick_type == 3)
		newval = input_port_1_r(offset);

	/* set a timer on the joystick interrupt */
	if (joystick_timer)
		timer_remove(joystick_timer);
	joystick_timer = NULL;

	/* the A4 bit enables/disables joystick IRQs */
	joystick_int_enable = ((offset >> 4) & 1) ^ 1;

	/* clear any existing interrupt and set a timer for a new one */
	joystick_int = 0;
	joystick_timer = timer_set(TIME_IN_USEC(50), newval, delayed_joystick_int);
	atarigen_update_interrupts();

	return joystick_value;
}


static WRITE_HANDLER( joystick_w )
{
	/* the A4 bit enables/disables joystick IRQs */
	joystick_int_enable = ((offset >> 4) & 1) ^ 1;
}



/*************************************
 *
 *	Trackball read.
 *
 *************************************/

static READ_HANDLER( trakball_r )
{
	int result = 0xff;

	/* Marble Madness trackball type -- rotated 45 degrees! */
	if (trackball_type == 1)
	{
		static UINT8 cur[2][2];
		int player = (offset >> 2) & 1;
		int which = (offset >> 1) & 1;

		/* when reading the even ports, do a real analog port update */
		if (which == 0)
		{
			UINT8 posx,posy;

			if (player == 0)
			{
				posx = (INT8)input_port_0_r(offset);
				posy = (INT8)input_port_1_r(offset);
			}
			else
			{
				posx = (INT8)input_port_2_r(offset);
				posy = (INT8)input_port_3_r(offset);
			}

			cur[player][0] = posx + posy;
			cur[player][1] = posx - posy;
		}

		result = cur[player][which];
	}

	/* Road Blasters steering wheel */
	else if (trackball_type == 2)
		result = input_port_0_r(offset);

	return result;
}



/*************************************
 *
 *	I/O read dispatch.
 *
 *************************************/

static READ_HANDLER( input_r )
{
	int temp = input_port_4_r(offset);
	if (atarigen_cpu_to_sound_ready) temp ^= 0x0080;
	return temp;
}


static READ_HANDLER( switch_6502_r )
{
	int temp = input_port_5_r(offset);

	if (atarigen_cpu_to_sound_ready) temp ^= 0x08;
	if (atarigen_sound_to_cpu_ready) temp ^= 0x10;
	if (!(input_port_4_r(offset) & 0x0040)) temp ^= 0x80;

	return temp;
}



/*************************************
 *
 *	TMS5220 communications
 *
 *************************************/

/*
 *	All communication to the 5220 goes through an SY6522A, which is an overpowered chip
 *	for the job.  Here is a listing of the I/O addresses:
 *
 *		$00	DRB		Data register B
 *		$01	DRA		Data register A
 *		$02	DDRB	Data direction register B (0=input, 1=output)
 *		$03	DDRA	Data direction register A (0=input, 1=output)
 *		$04	T1CL	T1 low counter
 *		$05	T1CH	T1 high counter
 *		$06	T1LL	T1 low latches
 *		$07	T1LH	T1 high latches
 *		$08	T2CL	T2 low counter
 *		$09	T2CH	T2 high counter
 *		$0A	SR		Shift register
 *		$0B	ACR		Auxiliary control register
 *		$0C	PCR		Peripheral control register
 *		$0D	IFR		Interrupt flag register
 *		$0E	IER		Interrupt enable register
 *		$0F	NHDRA	No handshake DRA
 *
 *	Fortunately, only addresses $00,$01,$0B,$0C, and $0F are accessed in the code, and
 *	$0B and $0C are merely set up once.
 *
 *	The ports are hooked in like follows:
 *
 *	Port A, D0-D7 = TMS5220 data lines (i/o)
 *
 *	Port B, D0 = 	Write strobe (out)
 *	        D1 = 	Read strobe (out)
 *	        D2 = 	Ready signal (in)
 *	        D3 = 	Interrupt signal (in)
 *	        D4 = 	LED (out)
 *	        D5 = 	??? (out)
 */

static READ_HANDLER( m6522_r )
{
	switch (offset)
	{
		case 0x00:	/* DRB */
			return (m6522_drb & m6522_ddrb) | (!tms5220_ready_r() << 2) | (!tms5220_int_r() << 3);

		case 0x01:	/* DRA */
		case 0x0f:	/* NHDRA */
			return (m6522_dra & m6522_ddra);

		case 0x02:	/* DDRB */
			return m6522_ddrb;

		case 0x03:	/* DDRA */
			return m6522_ddra;

		default:
			return m6522_regs[offset & 15];
	}
}


WRITE_HANDLER( m6522_w )
{
	int old;

	switch (offset)
	{
		case 0x00:	/* DRB */
			old = m6522_drb;
			m6522_drb = (m6522_drb & ~m6522_ddrb) | (data & m6522_ddrb);
			if (!(old & 1) && (m6522_drb & 1))
				tms5220_data_w(0, m6522_dra);
			if (!(old & 2) && (m6522_drb & 2))
				m6522_dra = (m6522_dra & m6522_ddra) | (tms5220_status_r(0) & ~m6522_ddra);

			/* bit 4 is connected to an up-counter, clocked by SYCLKB */
			data = 5 | ((data >> 3) & 2);
			tms5220_set_frequency(ATARI_CLOCK_14MHz/2 / (16 - data));
			break;

		case 0x01:	/* DRA */
		case 0x0f:	/* NHDRA */
			m6522_dra = (m6522_dra & ~m6522_ddra) | (data & m6522_ddra);
			break;

		case 0x02:	/* DDRB */
			m6522_ddrb = data;
			break;

		case 0x03:	/* DDRA */
			m6522_ddra = data;
			break;

		default:
			m6522_regs[offset & 15] = data;
			break;
	}
}



/*************************************
 *
 *	Speed cheats
 *
 *************************************/

READ_HANDLER( marble_speedcheck_r )
{
	int result = READ_WORD(&marble_speedcheck[offset]);

	if (offset == 2 && result == 0)
	{
		int time = cpu_gettotalcycles();
		if (time - speedcheck_time1 < 100 && speedcheck_time1 - speedcheck_time2 < 100)
			cpu_spinuntil_int();

		speedcheck_time2 = speedcheck_time1;
		speedcheck_time1 = time;
	}

	return result;
}


WRITE_HANDLER( marble_speedcheck_w )
{
	COMBINE_WORD_MEM(&marble_speedcheck[offset], data);
	speedcheck_time1 = cpu_gettotalcycles() - 1000;
	speedcheck_time2 = speedcheck_time1 - 1000;
}



/*************************************
 *
 *	Opcode memory catcher.
 *
 *************************************/

static OPBASE_HANDLER( indytemp_setopbase )
{
	int prevpc = cpu_getpreviouspc();

	/*
	 *	This is a slightly ugly kludge for Indiana Jones & the Temple of Doom because it jumps
	 *	directly to code in the slapstic.  The general order of things is this:
	 *
	 *		jump to $3A, which turns off interrupts and jumps to $00 (the reset address)
	 *		look up the request in a table and jump there
	 *		(under some circumstances, tweak the special addresses)
	 *		return via an RTS at the real bankswitch address
	 *
	 *	To simulate this, we tweak the slapstic reset address on entry into slapstic code; then
	 *	we let the system tweak whatever other addresses it wishes.  On exit, we tweak the
	 *	address of the previous PC, which is the RTS instruction, thereby completing the
	 *	bankswitch sequence.
	 *
	 *	Fortunately for us, all 4 banks have exactly the same code at this point in their
	 *	ROM, so it doesn't matter which version we're actually executing.
	 */

	if (address & 0x80000)
		atarigen_slapstic_r(0);
	else if (prevpc & 0x80000)
		atarigen_slapstic_r(prevpc & 0x7fff);

	return address;
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress main_readmem[] =
{
	{ 0x000000, 0x087fff, MRA_ROM },
	{ 0x2e0000, 0x2e0003, atarisys1_int3state_r },
	{ 0x400000, 0x401fff, MRA_BANK1 },
	{ 0x840000, 0x840003, MRA_BANK2 },
	{ 0x900000, 0x9fffff, MRA_BANK3 },
	{ 0xa00000, 0xa01fff, MRA_BANK4 },
	{ 0xa02000, 0xa02fff, MRA_BANK5 },
	{ 0xa03000, 0xa03fff, MRA_BANK6 },
	{ 0xb00000, 0xb007ff, paletteram_word_r },
	{ 0xf00000, 0xf00fff, atarigen_eeprom_r },
	{ 0xf20000, 0xf20007, trakball_r },
	{ 0xf40000, 0xf4001f, joystick_r },
	{ 0xf60000, 0xf60003, input_r },
	{ 0xfc0000, 0xfc0003, atarigen_sound_r },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress main_writemem[] =
{
	{ 0x000000, 0x087fff, MWA_ROM },
	{ 0x400000, 0x401fff, MWA_BANK1 },
	{ 0x800000, 0x800003, atarisys1_hscroll_w, &atarigen_hscroll },
	{ 0x820000, 0x820003, atarisys1_vscroll_w, &atarigen_vscroll },
	{ 0x840000, 0x840003, MWA_BANK2, &atarisys1_prioritycolor },
	{ 0x860000, 0x860003, atarisys1_bankselect_w, &atarisys1_bankselect },
	{ 0x880000, 0x880003, watchdog_reset_w },
	{ 0x8a0000, 0x8a0003, atarigen_video_int_ack_w },
	{ 0x8c0000, 0x8c0003, atarigen_eeprom_enable_w },
	{ 0x900000, 0x9fffff, MWA_BANK3 },
	{ 0xa00000, 0xa01fff, atarisys1_playfieldram_w, &atarigen_playfieldram, &atarigen_playfieldram_size },
	{ 0xa02000, 0xa02fff, atarisys1_spriteram_w, &atarigen_spriteram, &atarigen_spriteram_size },
	{ 0xa03000, 0xa03fff, MWA_BANK6, &atarigen_alpharam, &atarigen_alpharam_size },
	{ 0xb00000, 0xb007ff, paletteram_IIIIRRRRGGGGBBBB_word_w, &paletteram },
	{ 0xf00000, 0xf00fff, atarigen_eeprom_w, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0xf40000, 0xf4001f, joystick_w },
	{ 0xfe0000, 0xfe0003, atarigen_sound_w },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Sound CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x1000, 0x100f, m6522_r },
	{ 0x1800, 0x1801, YM2151_status_port_0_r },
	{ 0x1810, 0x1810, atarigen_6502_sound_r },
	{ 0x1820, 0x1820, switch_6502_r },
	{ 0x1870, 0x187f, pokey1_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x100f, m6522_w },
	{ 0x1800, 0x1800, YM2151_register_port_0_w },
	{ 0x1801, 0x1801, YM2151_data_port_0_w },
	{ 0x1810, 0x1810, atarigen_6502_sound_w },
	{ 0x1824, 0x1825, led_w },
	{ 0x1820, 0x1827, MWA_NOP },
	{ 0x1870, 0x187f, pokey1_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( marble )
	PORT_START  /* F20000 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_REVERSE | IPF_PLAYER1, 30, 30, 0, 0 )

	PORT_START  /* F20002 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER1, 30, 30, 0, 0 )

	PORT_START  /* F20004 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_REVERSE | IPF_PLAYER2, 30, 30, 0, 0 )

	PORT_START  /* F20006 */
    PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_Y | IPF_PLAYER2, 30, 30, 0, 0 )

	PORT_START	/* F60000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 1820 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SPECIAL )
INPUT_PORTS_END


INPUT_PORTS_START( peterpak )
	PORT_START	/* F40000 */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* F60000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 1820 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SPECIAL )
INPUT_PORTS_END


INPUT_PORTS_START( indytemp )
	PORT_START	/* F40000 */
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* F60000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* freeze? */
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 1820 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SPECIAL )
INPUT_PORTS_END


INPUT_PORTS_START( roadrunn )
	PORT_START	/* F40000 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_REVERSE | IPF_PLAYER1, 100, 10, 0x10, 0xf0 )

	PORT_START	/* F40002 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0x10, 0xf0 )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* F60000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 1820 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SPECIAL )
INPUT_PORTS_END


INPUT_PORTS_START( roadblst )
	PORT_START	/* F20000 */
	PORT_ANALOG( 0xff, 0x40, IPT_DIAL | IPF_REVERSE, 25, 10, 0x00, 0x7f )

	PORT_START	/* F40000 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL, 100, 64, 0x00, 0xff )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* n/a */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* F60000 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x0040, IP_ACTIVE_LOW )
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 1820 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SPECIAL )
INPUT_PORTS_END



/*************************************
 *
 *	Graphics definitions
 *
 *************************************/

static struct GfxLayout anlayout =
{
	8,8,
	RGN_FRAC(1,1),
	2,
	{ 0, 4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &anlayout,       0, 64 },
	{ -1 } /* end of array */
};



/*************************************
 *
 *	Sound definitions
 *
 *************************************/

static struct YM2151interface ym2151_interface =
{
	1,
	ATARI_CLOCK_14MHz/4,
	{ YM3012_VOL(80,MIXER_PAN_LEFT,80,MIXER_PAN_RIGHT) },
	{ atarigen_ym2151_irq_gen }
};


static struct POKEYinterface pokey_interface =
{
	1,
	ATARI_CLOCK_14MHz/8,
	{ 40 },
};


static struct TMS5220interface tms5220_interface =
{
	ATARI_CLOCK_14MHz/2/11,
	100,
	0
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_atarisy1 =
{
	/* basic machine hardware */
	{
		{
			CPU_M68010,		/* verified */
			ATARI_CLOCK_14MHz/2,
			main_readmem,main_writemem,0,0,
			atarigen_video_int_gen,1
		},
		{
			CPU_M6502,
			ATARI_CLOCK_14MHz/8,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	init_machine,

	/* video hardware */
	42*8, 30*8, { 0*8, 42*8-1, 0*8, 30*8-1 },
	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	atarisys1_vh_start,
	atarisys1_vh_stop,
	atarisys1_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_POKEY,
			&pokey_interface
		},
		{
			SOUND_TMS5220,
			&tms5220_interface
		}
	},

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM decoders
 *
 *************************************/

static void rom_decode(void)
{
	UINT32 *data = (UINT32 *)&memory_region(REGION_GFX2)[0];
	int chips = memory_region_length(REGION_GFX2) / 0x8000;
	int i, j;

	/* invert the graphics bits on the playfield and motion objects */
	for (i = 0; i < chips; i++, data += 0x2000)
	{
		/* but first... if this is all zeros, don't do it */
		for (j = 0; j < 0x2000; j++)
			if (data[j] != 0)
				break;

		if (j != 0x2000)
			for (j = 0; j < 0x2000; j++)
				data[j] ^= 0xffffffff;
	}
}


static void roadblst_rom_decode(void)
{
	int i;

	/* ROMs 39+40 load the lower half at 10000 and the upper half at 50000 */
	/* ROMs 55+56 load the lower half at 20000 and the upper half at 60000 */
	/* However, we load 39+40 into 10000 and 20000, and 55+56 into 50000+60000 */
	/* We need to swap the memory at 20000 and 50000 */
	for (i = 0; i < 0x10000; i++)
	{
		int temp = memory_region(REGION_CPU1)[0x20000 + i];
		memory_region(REGION_CPU1)[0x20000 + i] = memory_region(REGION_CPU1)[0x50000 + i];
		memory_region(REGION_CPU1)[0x50000 + i] = temp;
	}

	/* invert the graphics bits on the playfield and motion objects */
	rom_decode();
}



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void init_marble(void)
{
	atarigen_eeprom_default = NULL;
	atarigen_slapstic_init(0, 0x080000, 103);

	joystick_type = 0;	/* none */
	trackball_type = 1;	/* rotated */

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x8108, 0x8120);

	/* speed up the 68010 */
	marble_speedcheck = (UINT8*)install_mem_read_handler(0, 0x400014, 0x400015, marble_speedcheck_r);
	install_mem_write_handler(0, 0x400014, 0x400015, marble_speedcheck_w);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_peterpak(void)
{
	atarigen_eeprom_default = NULL;
	atarigen_slapstic_init(0, 0x080000, 107);

	joystick_type = 1;	/* digital */
	trackball_type = 0;	/* none */

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x8101, 0x8119);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_indytemp(void)
{
	atarigen_eeprom_default = NULL;
	atarigen_slapstic_init(0, 0x080000, 105);

	/* special case for the Indiana Jones slapstic */
	cpu_setOPbaseoverride(0,indytemp_setopbase);

	joystick_type = 1;	/* digital */
	trackball_type = 0;	/* none */

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x410b, 0x4123);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_roadrunn(void)
{
	atarigen_eeprom_default = NULL;
	atarigen_slapstic_init(0, 0x080000, 108);

	joystick_type = 2;	/* analog */
	trackball_type = 0;	/* none */

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x8106, 0x811e);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_roadblst(void)
{
	atarigen_eeprom_default = NULL;
	atarigen_slapstic_init(0, 0x080000, 110);

	joystick_type = 3;	/* pedal */
	trackball_type = 2;	/* steering wheel */

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x410b, 0x4123);

	/* display messages */
	atarigen_show_sound_message();

	roadblst_rom_decode();
}



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( marble )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136033.401",   0x10000, 0x08000, 0xecfc25a2 )
	ROM_LOAD_ODD ( "136033.402",   0x10000, 0x08000, 0x7ce9bf53 )
	ROM_LOAD_EVEN( "136033.403",   0x20000, 0x08000, 0xdafee7a2 )
	ROM_LOAD_ODD ( "136033.404",   0x20000, 0x08000, 0xb59ffcf6 )
	ROM_LOAD_EVEN( "136033.107",   0x80000, 0x04000, 0xf3b8745b )
	ROM_LOAD_ODD ( "136033.108",   0x80000, 0x04000, 0xe51eecaa )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136033.421",   0x8000, 0x4000, 0x78153dc3 )
	ROM_LOAD( "136033.422",   0xc000, 0x4000, 0x2e66300e )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136033.137",   0x00000, 0x04000, 0x7a45f5c1 )  /* bank 1, plane 0 */
	ROM_LOAD( "136033.138",   0x04000, 0x04000, 0x7e954a88 )
	ROM_LOAD( "136033.139",   0x08000, 0x04000, 0x1eb1bb5f )  /* bank 1, plane 1 */
	ROM_LOAD( "136033.140",   0x0c000, 0x04000, 0x8a82467b )
	ROM_LOAD( "136033.141",   0x10000, 0x04000, 0x52448965 )  /* bank 1, plane 2 */
	ROM_LOAD( "136033.142",   0x14000, 0x04000, 0xb4a70e4f )
	ROM_LOAD( "136033.143",   0x18000, 0x04000, 0x7156e449 )  /* bank 1, plane 3 */
	ROM_LOAD( "136033.144",   0x1c000, 0x04000, 0x4c3e4c79 )
	ROM_LOAD( "136033.145",   0x20000, 0x04000, 0x9062be7f )  /* bank 1, plane 4 */
	ROM_LOAD( "136033.146",   0x24000, 0x04000, 0x14566dca )

	ROM_LOAD( "136033.149",   0x34000, 0x04000, 0xb6658f06 )  /* bank 2, plane 0 */
	ROM_LOAD( "136033.151",   0x3c000, 0x04000, 0x84ee1c80 )  /* bank 2, plane 1 */
	ROM_LOAD( "136033.153",   0x44000, 0x04000, 0xdaa02926 )  /* bank 2, plane 2 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136033.118",   0x000, 0x200, 0x2101b0ed )  /* remap */
	ROM_LOAD( "136033.119",   0x200, 0x200, 0x19f6e767 )  /* color */
ROM_END

ROM_START( marble2 )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136033.201",   0x10000, 0x08000, 0x9395804d )
	ROM_LOAD_ODD ( "136033.202",   0x10000, 0x08000, 0xedd313f5 )
	ROM_LOAD_EVEN( "136033.403",   0x20000, 0x08000, 0xdafee7a2 )
	ROM_LOAD_ODD ( "136033.204",   0x20000, 0x08000, 0x4d621731 )
	ROM_LOAD_EVEN( "136033.107",   0x80000, 0x04000, 0xf3b8745b )
	ROM_LOAD_ODD ( "136033.108",   0x80000, 0x04000, 0xe51eecaa )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136033.121",   0x8000, 0x4000, 0x73fe2b46 )
	ROM_LOAD( "136033.122",   0xc000, 0x4000, 0x03bf65c3 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136033.137",   0x00000, 0x04000, 0x7a45f5c1 )  /* bank 1, plane 0 */
	ROM_LOAD( "136033.138",   0x04000, 0x04000, 0x7e954a88 )
	ROM_LOAD( "136033.139",   0x08000, 0x04000, 0x1eb1bb5f )  /* bank 1, plane 1 */
	ROM_LOAD( "136033.140",   0x0c000, 0x04000, 0x8a82467b )
	ROM_LOAD( "136033.141",   0x10000, 0x04000, 0x52448965 )  /* bank 1, plane 2 */
	ROM_LOAD( "136033.142",   0x14000, 0x04000, 0xb4a70e4f )
	ROM_LOAD( "136033.143",   0x18000, 0x04000, 0x7156e449 )  /* bank 1, plane 3 */
	ROM_LOAD( "136033.144",   0x1c000, 0x04000, 0x4c3e4c79 )
	ROM_LOAD( "136033.145",   0x20000, 0x04000, 0x9062be7f )  /* bank 1, plane 4 */
	ROM_LOAD( "136033.146",   0x24000, 0x04000, 0x14566dca )

	ROM_LOAD( "136033.149",   0x34000, 0x04000, 0xb6658f06 )  /* bank 2, plane 0 */
	ROM_LOAD( "136033.151",   0x3c000, 0x04000, 0x84ee1c80 )  /* bank 2, plane 1 */
	ROM_LOAD( "136033.153",   0x44000, 0x04000, 0xdaa02926 )  /* bank 2, plane 2 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136033.118",   0x000, 0x200, 0x2101b0ed )  /* remap */
	ROM_LOAD( "136033.119",   0x200, 0x200, 0x19f6e767 )  /* color */
ROM_END

ROM_START( marblea )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136033.323",   0x10000, 0x04000, 0x4dc2987a )
	ROM_LOAD_ODD ( "136033.324",   0x10000, 0x04000, 0xe22e6e11 )
	ROM_LOAD_EVEN( "136033.225",   0x18000, 0x04000, 0x743f6c5c )
	ROM_LOAD_ODD ( "136033.226",   0x18000, 0x04000, 0xaeb711e3 )
	ROM_LOAD_EVEN( "136033.227",   0x20000, 0x04000, 0xd06d2c22 )
	ROM_LOAD_ODD ( "136033.228",   0x20000, 0x04000, 0xe69cec16 )
	ROM_LOAD_EVEN( "136033.229",   0x28000, 0x04000, 0xc81d5c14 )
	ROM_LOAD_ODD ( "136033.230",   0x28000, 0x04000, 0x526ce8ad )
	ROM_LOAD_EVEN( "136033.107",   0x80000, 0x04000, 0xf3b8745b )
	ROM_LOAD_ODD ( "136033.108",   0x80000, 0x04000, 0xe51eecaa )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136033.257",   0x8000, 0x4000, 0x2e2e0df8 )
	ROM_LOAD( "136033.258",   0xc000, 0x4000, 0x1b9655cd )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0x60000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136033.137",   0x00000, 0x04000, 0x7a45f5c1 )  /* bank 1, plane 0 */
	ROM_LOAD( "136033.138",   0x04000, 0x04000, 0x7e954a88 )
	ROM_LOAD( "136033.139",   0x08000, 0x04000, 0x1eb1bb5f )  /* bank 1, plane 1 */
	ROM_LOAD( "136033.140",   0x0c000, 0x04000, 0x8a82467b )
	ROM_LOAD( "136033.141",   0x10000, 0x04000, 0x52448965 )  /* bank 1, plane 2 */
	ROM_LOAD( "136033.142",   0x14000, 0x04000, 0xb4a70e4f )
	ROM_LOAD( "136033.143",   0x18000, 0x04000, 0x7156e449 )  /* bank 1, plane 3 */
	ROM_LOAD( "136033.144",   0x1c000, 0x04000, 0x4c3e4c79 )
	ROM_LOAD( "136033.145",   0x20000, 0x04000, 0x9062be7f )  /* bank 1, plane 4 */
	ROM_LOAD( "136033.146",   0x24000, 0x04000, 0x14566dca )

	ROM_LOAD( "136033.149",   0x34000, 0x04000, 0xb6658f06 )  /* bank 2, plane 0 */
	ROM_LOAD( "136033.151",   0x3c000, 0x04000, 0x84ee1c80 )  /* bank 2, plane 1 */
	ROM_LOAD( "136033.153",   0x44000, 0x04000, 0xdaa02926 )  /* bank 2, plane 2 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136033.118",   0x000, 0x200, 0x2101b0ed )  /* remap */
	ROM_LOAD( "136033.119",   0x200, 0x200, 0x19f6e767 )  /* color */
ROM_END

ROM_START( peterpak )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136028.142",   0x10000, 0x04000, 0x4f9fc020 )
	ROM_LOAD_ODD ( "136028.143",   0x10000, 0x04000, 0x9fb257cc )
	ROM_LOAD_EVEN( "136028.144",   0x18000, 0x04000, 0x50267619 )
	ROM_LOAD_ODD ( "136028.145",   0x18000, 0x04000, 0x7b6a5004 )
	ROM_LOAD_EVEN( "136028.146",   0x20000, 0x04000, 0x4183a67a )
	ROM_LOAD_ODD ( "136028.147",   0x20000, 0x04000, 0x14e2d97b )
	ROM_LOAD_EVEN( "136028.148",   0x80000, 0x04000, 0x230e8ba9 )
	ROM_LOAD_ODD ( "136028.149",   0x80000, 0x04000, 0x0ff0c13a )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136028.101",   0x8000, 0x4000, 0xff712aa2 )
	ROM_LOAD( "136028.102",   0xc000, 0x4000, 0x89ea21a1 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0x90000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136028.138",   0x00000, 0x08000, 0x53eaa018 )  /* bank 1, plane 0 */
	ROM_LOAD( "136028.139",   0x08000, 0x08000, 0x354a19cb )  /* bank 1, plane 1 */
	ROM_LOAD( "136028.140",   0x10000, 0x08000, 0x8d2c4717 )  /* bank 1, plane 2 */
	ROM_LOAD( "136028.141",   0x18000, 0x08000, 0xbf59ea19 )  /* bank 1, plane 3 */

	ROM_LOAD( "136028.150",   0x30000, 0x08000, 0x83362483 )  /* bank 2, plane 0 */
	ROM_LOAD( "136028.151",   0x38000, 0x08000, 0x6e95094e )  /* bank 2, plane 1 */
	ROM_LOAD( "136028.152",   0x40000, 0x08000, 0x9553f084 )  /* bank 2, plane 2 */
	ROM_LOAD( "136028.153",   0x48000, 0x08000, 0xc2a9b028 )  /* bank 2, plane 3 */

	ROM_LOAD( "136028.105",   0x64000, 0x04000, 0xac9a5a44 )  /* bank 3, plane 0 */
	ROM_LOAD( "136028.108",   0x6c000, 0x04000, 0x51941e64 )  /* bank 3, plane 1 */
	ROM_LOAD( "136028.111",   0x74000, 0x04000, 0x246599f3 )  /* bank 3, plane 2 */
	ROM_LOAD( "136028.114",   0x7c000, 0x04000, 0x918a5082 )  /* bank 3, plane 3 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136028.136",   0x000, 0x200, 0x861cfa36 )  /* remap */
	ROM_LOAD( "136028.137",   0x200, 0x200, 0x8507e5ea )  /* color */
ROM_END

ROM_START( indytemp )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136036.432",   0x10000, 0x08000, 0xd888cdf1 )
	ROM_LOAD_ODD ( "136036.431",   0x10000, 0x08000, 0xb7ac7431 )
	ROM_LOAD_EVEN( "136036.434",   0x20000, 0x08000, 0x802495fd )
	ROM_LOAD_ODD ( "136036.433",   0x20000, 0x08000, 0x3a914e5c )
	ROM_LOAD_EVEN( "136036.456",   0x30000, 0x04000, 0xec146b09 )
	ROM_LOAD_ODD ( "136036.457",   0x30000, 0x04000, 0x6628de01 )
	ROM_LOAD_EVEN( "136036.358",   0x80000, 0x04000, 0xd9351106 )
	ROM_LOAD_ODD ( "136036.359",   0x80000, 0x04000, 0xe731caea )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136036.153",   0x4000, 0x4000, 0x95294641 )
	ROM_LOAD( "136036.154",   0x8000, 0x4000, 0xcbfc6adb )
	ROM_LOAD( "136036.155",   0xc000, 0x4000, 0x4c8233ac )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0xc0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136036.135",   0x00000, 0x08000, 0xffa8749c )  /* bank 1, plane 0 */
	ROM_LOAD( "136036.139",   0x08000, 0x08000, 0xb682bfca )  /* bank 1, plane 1 */
	ROM_LOAD( "136036.143",   0x10000, 0x08000, 0x7697da26 )  /* bank 1, plane 2 */
	ROM_LOAD( "136036.147",   0x18000, 0x08000, 0x4e9d664c )  /* bank 1, plane 3 */

	ROM_LOAD( "136036.136",   0x30000, 0x08000, 0xb2b403aa )  /* bank 2, plane 0 */
	ROM_LOAD( "136036.140",   0x38000, 0x08000, 0xec0c19ca )  /* bank 2, plane 1 */
	ROM_LOAD( "136036.144",   0x40000, 0x08000, 0x4407df98 )  /* bank 2, plane 2 */
	ROM_LOAD( "136036.148",   0x48000, 0x08000, 0x70dce06d )  /* bank 2, plane 3 */

	ROM_LOAD( "136036.137",   0x60000, 0x08000, 0x3f352547 )  /* bank 3, plane 0 */
	ROM_LOAD( "136036.141",   0x68000, 0x08000, 0x9cbdffd0 )  /* bank 3, plane 1 */
	ROM_LOAD( "136036.145",   0x70000, 0x08000, 0xe828e64b )  /* bank 3, plane 2 */
	ROM_LOAD( "136036.149",   0x78000, 0x08000, 0x81503a23 )  /* bank 3, plane 3 */

	ROM_LOAD( "136036.138",   0x90000, 0x08000, 0x48c4d79d )  /* bank 4, plane 0 */
	ROM_LOAD( "136036.142",   0x98000, 0x08000, 0x7faae75f )  /* bank 4, plane 1 */
	ROM_LOAD( "136036.146",   0xa0000, 0x08000, 0x8ae5a7b5 )  /* bank 4, plane 2 */
	ROM_LOAD( "136036.150",   0xa8000, 0x08000, 0xa10c4bd9 )  /* bank 4, plane 3 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136036.152",   0x000, 0x200, 0x4f96e57c )  /* remap */
	ROM_LOAD( "136036.151",   0x200, 0x200, 0x7daf351f )  /* color */
ROM_END

ROM_START( indytem2 )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136036.470",   0x10000, 0x08000, 0x7fac1dd8 )
	ROM_LOAD_ODD ( "136036.471",   0x10000, 0x08000, 0xe93272fb )
	ROM_LOAD_EVEN( "136036.434",   0x20000, 0x08000, 0x802495fd )
	ROM_LOAD_ODD ( "136036.433",   0x20000, 0x08000, 0x3a914e5c )
	ROM_LOAD_EVEN( "136036.456",   0x30000, 0x04000, 0xec146b09 )
	ROM_LOAD_ODD ( "136036.457",   0x30000, 0x04000, 0x6628de01 )
	ROM_LOAD_EVEN( "136036.358",   0x80000, 0x04000, 0xd9351106 )
	ROM_LOAD_ODD ( "136036.359",   0x80000, 0x04000, 0xe731caea )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136036.153",   0x4000, 0x4000, 0x95294641 )
	ROM_LOAD( "136036.154",   0x8000, 0x4000, 0xcbfc6adb )
	ROM_LOAD( "136036.155",   0xc000, 0x4000, 0x4c8233ac )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0xc0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136036.135",   0x00000, 0x08000, 0xffa8749c )  /* bank 1, plane 0 */
	ROM_LOAD( "136036.139",   0x08000, 0x08000, 0xb682bfca )  /* bank 1, plane 1 */
	ROM_LOAD( "136036.143",   0x10000, 0x08000, 0x7697da26 )  /* bank 1, plane 2 */
	ROM_LOAD( "136036.147",   0x18000, 0x08000, 0x4e9d664c )  /* bank 1, plane 3 */

	ROM_LOAD( "136036.136",   0x30000, 0x08000, 0xb2b403aa )  /* bank 2, plane 0 */
	ROM_LOAD( "136036.140",   0x38000, 0x08000, 0xec0c19ca )  /* bank 2, plane 1 */
	ROM_LOAD( "136036.144",   0x40000, 0x08000, 0x4407df98 )  /* bank 2, plane 2 */
	ROM_LOAD( "136036.148",   0x48000, 0x08000, 0x70dce06d )  /* bank 2, plane 3 */

	ROM_LOAD( "136036.137",   0x60000, 0x08000, 0x3f352547 )  /* bank 3, plane 0 */
	ROM_LOAD( "136036.141",   0x68000, 0x08000, 0x9cbdffd0 )  /* bank 3, plane 1 */
	ROM_LOAD( "136036.145",   0x70000, 0x08000, 0xe828e64b )  /* bank 3, plane 2 */
	ROM_LOAD( "136036.149",   0x78000, 0x08000, 0x81503a23 )  /* bank 3, plane 3 */

	ROM_LOAD( "136036.138",   0x90000, 0x08000, 0x48c4d79d )  /* bank 4, plane 0 */
	ROM_LOAD( "136036.142",   0x98000, 0x08000, 0x7faae75f )  /* bank 4, plane 1 */
	ROM_LOAD( "136036.146",   0xa0000, 0x08000, 0x8ae5a7b5 )  /* bank 4, plane 2 */
	ROM_LOAD( "136036.150",   0xa8000, 0x08000, 0xa10c4bd9 )  /* bank 4, plane 3 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136036.152",   0x000, 0x200, 0x4f96e57c )  /* remap */
	ROM_LOAD( "136036.151",   0x200, 0x200, 0x7daf351f )  /* color */
ROM_END

ROM_START( indytem3 )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "232.10b",      0x10000, 0x08000, 0x1e80108f )
	ROM_LOAD_ODD ( "231.10a",      0x10000, 0x08000, 0x8ae54c0c )
	ROM_LOAD_EVEN( "234.12b",      0x20000, 0x08000, 0x86be7e07 )
	ROM_LOAD_ODD ( "233.12a",      0x20000, 0x08000, 0xbfcea7ae )
	ROM_LOAD_EVEN( "256.15b",      0x30000, 0x04000, 0x3a076fd2 )
	ROM_LOAD_ODD ( "257.15a",      0x30000, 0x04000, 0x15293606 )
	ROM_LOAD_EVEN( "158.16b",      0x80000, 0x04000, 0x10372888 )
	ROM_LOAD_ODD ( "159.16a",      0x80000, 0x04000, 0x50f890a8 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136036.153",   0x4000, 0x4000, 0x95294641 )
	ROM_LOAD( "136036.154",   0x8000, 0x4000, 0xcbfc6adb )
	ROM_LOAD( "136036.155",   0xc000, 0x4000, 0x4c8233ac )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0xc0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136036.135",   0x00000, 0x08000, 0xffa8749c )  /* bank 1, plane 0 */
	ROM_LOAD( "136036.139",   0x08000, 0x08000, 0xb682bfca )  /* bank 1, plane 1 */
	ROM_LOAD( "136036.143",   0x10000, 0x08000, 0x7697da26 )  /* bank 1, plane 2 */
	ROM_LOAD( "136036.147",   0x18000, 0x08000, 0x4e9d664c )  /* bank 1, plane 3 */

	ROM_LOAD( "136036.136",   0x30000, 0x08000, 0xb2b403aa )  /* bank 2, plane 0 */
	ROM_LOAD( "136036.140",   0x38000, 0x08000, 0xec0c19ca )  /* bank 2, plane 1 */
	ROM_LOAD( "136036.144",   0x40000, 0x08000, 0x4407df98 )  /* bank 2, plane 2 */
	ROM_LOAD( "136036.148",   0x48000, 0x08000, 0x70dce06d )  /* bank 2, plane 3 */

	ROM_LOAD( "136036.137",   0x60000, 0x08000, 0x3f352547 )  /* bank 3, plane 0 */
	ROM_LOAD( "136036.141",   0x68000, 0x08000, 0x9cbdffd0 )  /* bank 3, plane 1 */
	ROM_LOAD( "136036.145",   0x70000, 0x08000, 0xe828e64b )  /* bank 3, plane 2 */
	ROM_LOAD( "136036.149",   0x78000, 0x08000, 0x81503a23 )  /* bank 3, plane 3 */

	ROM_LOAD( "136036.138",   0x90000, 0x08000, 0x48c4d79d )  /* bank 4, plane 0 */
	ROM_LOAD( "136036.142",   0x98000, 0x08000, 0x7faae75f )  /* bank 4, plane 1 */
	ROM_LOAD( "136036.146",   0xa0000, 0x08000, 0x8ae5a7b5 )  /* bank 4, plane 2 */
	ROM_LOAD( "136036.150",   0xa8000, 0x08000, 0xa10c4bd9 )  /* bank 4, plane 3 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136036.152",   0x000, 0x200, 0x4f96e57c )  /* remap */
	ROM_LOAD( "136036.151",   0x200, 0x200, 0x7daf351f )  /* color */
ROM_END

ROM_START( indytem4 )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136036.332",   0x10000, 0x08000, 0xa5563773 )
	ROM_LOAD_ODD ( "136036.331",   0x10000, 0x08000, 0x7d562141 )
	ROM_LOAD_EVEN( "136036.334",   0x20000, 0x08000, 0xe40828e5 )
	ROM_LOAD_ODD ( "136036.333",   0x20000, 0x08000, 0x96e1f1aa )
	ROM_LOAD_EVEN( "136036.356",   0x30000, 0x04000, 0x5eba2ac7 )
	ROM_LOAD_ODD ( "136036.357",   0x30000, 0x04000, 0x26e84b5c )
	ROM_LOAD_EVEN( "136036.358",   0x80000, 0x04000, 0xd9351106 )
	ROM_LOAD_ODD ( "136036.359",   0x80000, 0x04000, 0xe731caea )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136036.153",   0x4000, 0x4000, 0x95294641 )
	ROM_LOAD( "136036.154",   0x8000, 0x4000, 0xcbfc6adb )
	ROM_LOAD( "136036.155",   0xc000, 0x4000, 0x4c8233ac )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0xc0000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136036.135",   0x00000, 0x08000, 0xffa8749c )  /* bank 1, plane 0 */
	ROM_LOAD( "136036.139",   0x08000, 0x08000, 0xb682bfca )  /* bank 1, plane 1 */
	ROM_LOAD( "136036.143",   0x10000, 0x08000, 0x7697da26 )  /* bank 1, plane 2 */
	ROM_LOAD( "136036.147",   0x18000, 0x08000, 0x4e9d664c )  /* bank 1, plane 3 */

	ROM_LOAD( "136036.136",   0x30000, 0x08000, 0xb2b403aa )  /* bank 2, plane 0 */
	ROM_LOAD( "136036.140",   0x38000, 0x08000, 0xec0c19ca )  /* bank 2, plane 1 */
	ROM_LOAD( "136036.144",   0x40000, 0x08000, 0x4407df98 )  /* bank 2, plane 2 */
	ROM_LOAD( "136036.148",   0x48000, 0x08000, 0x70dce06d )  /* bank 2, plane 3 */

	ROM_LOAD( "136036.137",   0x60000, 0x08000, 0x3f352547 )  /* bank 3, plane 0 */
	ROM_LOAD( "136036.141",   0x68000, 0x08000, 0x9cbdffd0 )  /* bank 3, plane 1 */
	ROM_LOAD( "136036.145",   0x70000, 0x08000, 0xe828e64b )  /* bank 3, plane 2 */
	ROM_LOAD( "136036.149",   0x78000, 0x08000, 0x81503a23 )  /* bank 3, plane 3 */

	ROM_LOAD( "136036.138",   0x90000, 0x08000, 0x48c4d79d )  /* bank 4, plane 0 */
	ROM_LOAD( "136036.142",   0x98000, 0x08000, 0x7faae75f )  /* bank 4, plane 1 */
	ROM_LOAD( "136036.146",   0xa0000, 0x08000, 0x8ae5a7b5 )  /* bank 4, plane 2 */
	ROM_LOAD( "136036.150",   0xa8000, 0x08000, 0xa10c4bd9 )  /* bank 4, plane 3 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136036.152",   0x000, 0x200, 0x4f96e57c )  /* remap */
	ROM_LOAD( "136036.151",   0x200, 0x200, 0x7daf351f )  /* color */
ROM_END

ROM_START( roadrunn )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "136040.228",   0x10000, 0x08000, 0xb66c629a )
	ROM_LOAD_ODD ( "136040.229",   0x10000, 0x08000, 0x5638959f )
	ROM_LOAD_EVEN( "136040.230",   0x20000, 0x08000, 0xcd7956a3 )
	ROM_LOAD_ODD ( "136040.231",   0x20000, 0x08000, 0x722f2d3b )
	ROM_LOAD_EVEN( "136040.134",   0x50000, 0x08000, 0x18f431fe )
	ROM_LOAD_ODD ( "136040.135",   0x50000, 0x08000, 0xcb06f9ab )
	ROM_LOAD_EVEN( "136040.136",   0x60000, 0x08000, 0x8050bce4 )
	ROM_LOAD_ODD ( "136040.137",   0x60000, 0x08000, 0x3372a5cf )
	ROM_LOAD_EVEN( "136040.138",   0x70000, 0x08000, 0xa83155ee )
	ROM_LOAD_ODD ( "136040.139",   0x70000, 0x08000, 0x23aead1c )
	ROM_LOAD_EVEN( "136040.140",   0x80000, 0x04000, 0xd1464c88 )
	ROM_LOAD_ODD ( "136040.141",   0x80000, 0x04000, 0xf8f2acdf )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136040.143",   0x8000, 0x4000, 0x62b9878e )
	ROM_LOAD( "136040.144",   0xc000, 0x4000, 0x6ef1b804 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136040.101",   0x00000, 0x08000, 0x26d9f29c )  /* bank 1, plane 0 */
	ROM_LOAD( "136040.107",   0x08000, 0x08000, 0x8aac0ba4 )  /* bank 1, plane 1 */
	ROM_LOAD( "136040.113",   0x10000, 0x08000, 0x48b74c52 )  /* bank 1, plane 2 */
	ROM_LOAD( "136040.119",   0x18000, 0x08000, 0x17a6510c )  /* bank 1, plane 3 */

	ROM_LOAD( "136040.102",   0x30000, 0x08000, 0xae88f54b )  /* bank 2, plane 0 */
	ROM_LOAD( "136040.108",   0x38000, 0x08000, 0xa2ac13d4 )  /* bank 2, plane 1 */
	ROM_LOAD( "136040.114",   0x40000, 0x08000, 0xc91c3fcb )  /* bank 2, plane 2 */
	ROM_LOAD( "136040.120",   0x48000, 0x08000, 0x42d25859 )  /* bank 2, plane 3 */

	ROM_LOAD( "136040.103",   0x60000, 0x08000, 0xf2d7ef55 )  /* bank 3, plane 0 */
	ROM_LOAD( "136040.109",   0x68000, 0x08000, 0x11a843dc )  /* bank 3, plane 1 */
	ROM_LOAD( "136040.115",   0x70000, 0x08000, 0x8b1fa5bc )  /* bank 3, plane 2 */
	ROM_LOAD( "136040.121",   0x78000, 0x08000, 0xecf278f2 )  /* bank 3, plane 3 */

	ROM_LOAD( "136040.104",   0x90000, 0x08000, 0x0203d89c )  /* bank 4, plane 0 */
	ROM_LOAD( "136040.110",   0x98000, 0x08000, 0x64801601 )  /* bank 4, plane 1 */
	ROM_LOAD( "136040.116",   0xa0000, 0x08000, 0x52b23a36 )  /* bank 4, plane 2 */
	ROM_LOAD( "136040.122",   0xa8000, 0x08000, 0xb1137a9d )  /* bank 4, plane 3 */

	ROM_LOAD( "136040.105",   0xc0000, 0x08000, 0x398a36f8 )  /* bank 5, plane 0 */
	ROM_LOAD( "136040.111",   0xc8000, 0x08000, 0xf08b418b )  /* bank 5, plane 1 */
	ROM_LOAD( "136040.117",   0xd0000, 0x08000, 0xc4394834 )  /* bank 5, plane 2 */
	ROM_LOAD( "136040.123",   0xd8000, 0x08000, 0xdafd3dbe )  /* bank 5, plane 3 */

	ROM_LOAD( "136040.106",   0xe0000, 0x08000, 0x36a77bc5 )  /* bank 6, plane 0 */
	ROM_LOAD( "136040.112",   0xe8000, 0x08000, 0xb6624f3c )  /* bank 6, plane 1 */
	ROM_LOAD( "136040.118",   0xf0000, 0x08000, 0xf489a968 )  /* bank 6, plane 2 */
	ROM_LOAD( "136040.124",   0xf8000, 0x08000, 0x524d65f7 )  /* bank 6, plane 3 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "136040.126",   0x000, 0x200, 0x1713c0cd )  /* remap */
	ROM_LOAD( "136040.125",   0x200, 0x200, 0xa9ca8795 )  /* color */
ROM_END

ROM_START( roadblst )
	ROM_REGION( 0x88000, REGION_CPU1 )	/* 8.5*64k for 68000 code & slapstic ROM */
	ROM_LOAD_EVEN( "136032.205",   0x00000, 0x04000, 0x88d0be26 )
	ROM_LOAD_ODD ( "136032.206",   0x00000, 0x04000, 0x3c79ef05 )
	ROM_LOAD_EVEN( "048-1139.rom", 0x10000, 0x10000, 0xb73c1bd5 )
	ROM_LOAD_ODD ( "048-1140.rom", 0x10000, 0x10000, 0x6305429b )
	ROM_LOAD_EVEN( "048-1155.rom", 0x50000, 0x10000, 0xe95fc7d2 )
	ROM_LOAD_ODD ( "048-1156.rom", 0x50000, 0x10000, 0x727510f9 )
	ROM_LOAD_EVEN( "048-1167.rom", 0x70000, 0x08000, 0xc6d30d6f )
	ROM_LOAD_ODD ( "048-1168.rom", 0x70000, 0x08000, 0x16951020 )
	ROM_LOAD_EVEN( "048-2147.rom", 0x80000, 0x04000, 0x5c1adf67 )
	ROM_LOAD_ODD ( "048-2148.rom", 0x80000, 0x04000, 0xd9ac8966 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "048-1149.rom", 0x4000, 0x4000, 0x2e54f95e )
	ROM_LOAD( "048-1169.rom", 0x8000, 0x4000, 0xee318052 )
	ROM_LOAD( "048-1170.rom", 0xc000, 0x4000, 0x75dfec33 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136032.107",   0x00000, 0x02000, 0x7a29dc07 )  /* alpha font */

	ROM_REGION( 0x120000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "048-1101.rom", 0x00000, 0x08000, 0xfe342d27 )  /* bank 1, plane 0 */
	ROM_LOAD( "048-1102.rom", 0x08000, 0x08000, 0x17c7e780 )  /* bank 1, plane 1 */
	ROM_LOAD( "048-1103.rom", 0x10000, 0x08000, 0x39688e01 )  /* bank 1, plane 2 */
	ROM_LOAD( "048-1104.rom", 0x18000, 0x08000, 0xc8f9bd8e )  /* bank 1, plane 3 */
	ROM_LOAD( "048-1105.rom", 0x20000, 0x08000, 0xc69e439e )  /* bank 1, plane 4 */
	ROM_LOAD( "048-1106.rom", 0x28000, 0x08000, 0x4ee55796 )  /* bank 1, plane 5 */

	ROM_LOAD( "048-1107.rom", 0x30000, 0x08000, 0x02117c58 )  /* bank 2, plane 0 */
	ROM_CONTINUE(             0x60000, 0x08000 )			  /* bank 3, plane 0 */
	ROM_LOAD( "048-1108.rom", 0x38000, 0x08000, 0x1e148525 )  /* bank 2, plane 1 */
	ROM_CONTINUE(             0x68000, 0x08000 )			  /* bank 3, plane 1 */
	ROM_LOAD( "048-1109.rom", 0x40000, 0x08000, 0x110ce07e )  /* bank 2, plane 2 */
	ROM_CONTINUE(             0x70000, 0x08000 )			  /* bank 3, plane 2 */
	ROM_LOAD( "048-1110.rom", 0x48000, 0x08000, 0xc00aa0f4 )  /* bank 2, plane 3 */
	ROM_CONTINUE(             0x78000, 0x08000 )			  /* bank 3, plane 3 */

	ROM_LOAD( "048-1111.rom", 0x90000, 0x08000, 0xc951d014 )  /* bank 4, plane 0 */
	ROM_CONTINUE(             0xc0000, 0x08000 )			  /* bank 5, plane 0 */
	ROM_LOAD( "048-1112.rom", 0x98000, 0x08000, 0x95c5a006 )  /* bank 4, plane 1 */
	ROM_CONTINUE(             0xc8000, 0x08000 )			  /* bank 5, plane 1 */
	ROM_LOAD( "048-1113.rom", 0xa0000, 0x08000, 0xf61f2370 )  /* bank 4, plane 2 */
	ROM_CONTINUE(             0xd0000, 0x08000 )			  /* bank 5, plane 2 */
	ROM_LOAD( "048-1114.rom", 0xa8000, 0x08000, 0x774a36a8 )  /* bank 4, plane 3 */
	ROM_CONTINUE(             0xd8000, 0x08000 )			  /* bank 5, plane 3 */

	ROM_LOAD( "048-1115.rom", 0x100000, 0x08000, 0xa47bc79d ) /* bank 7, plane 0 */
	ROM_CONTINUE(             0xe0000, 0x08000 )			  /* bank 6, plane 0 */
	ROM_LOAD( "048-1116.rom", 0x108000, 0x08000, 0xb8a5c215 ) /* bank 7, plane 1 */
	ROM_CONTINUE(             0xe8000, 0x08000 )			  /* bank 6, plane 1 */
	ROM_LOAD( "048-1117.rom", 0x110000, 0x08000, 0x2d1c1f64 ) /* bank 7, plane 2 */
	ROM_CONTINUE(             0xf0000, 0x08000 )			  /* bank 6, plane 2 */
	ROM_LOAD( "048-1118.rom", 0x118000, 0x08000, 0xbe879b8e ) /* bank 7, plane 3 */
	ROM_CONTINUE(             0xf8000, 0x08000 )			  /* bank 6, plane 3 */

	ROM_REGION( 0x400, REGION_PROMS )	/* graphics mapping PROMs */
	ROM_LOAD( "048-1174.bpr",   0x000, 0x200, 0xdb4a4d53 )  /* remap */
	ROM_LOAD( "048-1173.bpr",   0x200, 0x200, 0xc80574af )  /* color */
ROM_END



/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1984, marble,   0,        atarisy1, marble,   marble,   ROT0, "Atari Games", "Marble Madness (set 1)" )
GAME( 1984, marble2,  marble,   atarisy1, marble,   marble,   ROT0, "Atari Games", "Marble Madness (set 2)" )
GAME( 1984, marblea,  marble,   atarisy1, marble,   marble,   ROT0, "Atari Games", "Marble Madness (set 3)" )
GAME( 1984, peterpak, 0,        atarisy1, peterpak, peterpak, ROT0, "Atari Games", "Peter Pack-Rat" )
GAME( 1985, indytemp, 0,        atarisy1, indytemp, indytemp, ROT0, "Atari Games", "Indiana Jones and the Temple of Doom (set 1)" )
GAME( 1985, indytem2, indytemp, atarisy1, indytemp, indytemp, ROT0, "Atari Games", "Indiana Jones and the Temple of Doom (set 2)" )
GAME( 1985, indytem3, indytemp, atarisy1, indytemp, indytemp, ROT0, "Atari Games", "Indiana Jones and the Temple of Doom (set 3)" )
GAME( 1985, indytem4, indytemp, atarisy1, indytemp, indytemp, ROT0, "Atari Games", "Indiana Jones and the Temple of Doom (set 4)" )
GAME( 1985, roadrunn, 0,        atarisy1, roadrunn, roadrunn, ROT0, "Atari Games", "Road Runner" )
GAME( 1987, roadblst, 0,        atarisy1, roadblst, roadblst, ROT0, "Atari Games", "Road Blasters" )

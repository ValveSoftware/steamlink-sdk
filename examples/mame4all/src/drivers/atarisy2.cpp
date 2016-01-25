#include "../vidhrdw/atarisy2.cpp"

/***************************************************************************

	Atari System 2

	driver by Aaron Giles

	Games supported:
		* Paperboy (1984)
		* 720 Degrees (1986) [2 sets]
		* Super Sprint (1986)
		* Championship Sprint (1986)
		* APB - All Points Bulletin (1987) [2 sets]

	Known bugs:
		* none at this time

****************************************************************************

	Memory map

****************************************************************************

	========================================================================
	MAIN CPU
	========================================================================
	0000-0FFF   R/W   xxxxxxxx xxxxxxxx   Program RAM
	1000-107F   R/W   xxxxxxxx xxxxxxxx   Motion object palette RAM (64 entries)
	            R/W   xxxx---- --------      (Intensity)
	            R/W   ----xxxx --------      (Red)
	            R/W   -------- xxxx----      (Green)
	            R/W   -------- ----xxxx      (Blue)
	1080-10BF   R/W   xxxxxxxx xxxxxxxx   Alphanumerics palette RAM (32 entries)
	1100-11FF   R/W   xxxxxxxx xxxxxxxx   Playfield palette RAM (128 entries)
	1400        R     -------- xxxxxxxx   ADC data read
	1400          W   xxxxxx-- --------   Bank 1 ROM select
	1402          W   xxxxxx-- --------   Bank 2 ROM select
	1480-148F     W   -------- --------   ADC strobe/select
	1580          W   -------- --------   Sound command read IRQ reset
	15A0          W   -------- --------   Sound CPU reset
	15C0          W   -------- --------   32V IRQ reset
	15E0          W   -------- --------   VBLANK IRQ reset
	1600          W   -------- ----xxxx   IRQ enable
	              W   -------- ----x---      (VBLANK IRQ enable)
	              W   -------- -----x--      (32V IRQ enable)
	              W   -------- ------x-      (Sound response IRQ enable)
	              W   -------- -------x      (Sound command read IRQ enable)
	1680          W   -------- xxxxxxxx   Sound command
	1700          W   xxxxxxxx xx--xxxx   Playfield X scroll/bank 1 select
	              W   xxxxxxxx xx------      (Playfield X scroll)
	              W   -------- ----xxxx      (Playfield bank 1 select)
	1780          W   -xxxxxxx xx--xxxx   Playfield Y scroll/bank 2 select
	              W   -xxxxxxx xx------      (Playfield Y scroll)
	              W   -------- ----xxxx      (Playfield bank 2 select)
	1800        R     x------- xxxxxxxx   Switch inputs
	            R     x------- --------      (Test switch)
	            R     -------- xx--xxxx      (Game-specific switches)
	            R     -------- --x-----      (Sound command buffer full)
	            R     -------- ---x----      (Sound response buffer full)
	1800          W   -------- --------   Watchdog reset
	1C00        R     -------- xxxxxxxx   Sound response read
	2000-37FF   R/W   xxx---xx xxxxxxxx   Alphanumerics RAM (bank 0, 64x32 tiles)
	            R/W   xxx----- --------      (Palette select)
	            R/W   ------xx xxxxxxxx      (Tile index)
	3800-3FFF   R/W   xxxxxxxx xxxxxxxx   Motion object RAM (bank 0, 256 entries x 4 words)
	            R/W   xxxxxxxx xx------      (0: Y position)
	            R/W   -------- -----xxx      (0: Tile index, 3 MSB)
	            R/W   x------- --------      (1: Hold position from last object)
	            R/W   -x------ --------      (1: Horizontal flip)
	            R/W   --xxx--- --------      (1: Number of Y tiles - 1)
	            R/W   -----xxx xxxxxxxx      (1: Tile index, 11 LSB)
	            R/W   xxxxxxxx xx------      (2: X position)
	            R/W   xx------ --------      (3: Priority)
	            R/W   -xxx---- --------      (3: Palette select)
	            R/W   -------- xxxxxxxx      (3: Link to the next object)
	2000-3FFF   R/W   --xxxxxx xxxxxxxx   Playfield RAM (banks 2 & 3, 128x64 tiles)
	            R/W   --xxx--- --------      (Palette select)
	            R/W   -----x-- --------      (Tile bank select)
	            R/W   ------xx xxxxxxxx      (Tile index, 10 LSB)
	4000-5FFF   R     xxxxxxxx xxxxxxxx   Bank 1 ROM
	6000-7FFF   R     xxxxxxxx xxxxxxxx   Bank 2 ROM
	8000-FFFF   R     xxxxxxxx xxxxxxxx   Program ROM (slapstic mapped here as well)
	========================================================================
	Interrupts:
		IRQ0 = sound command read
		IRQ1 = sound command write
		IRQ2 = 32V
		IRQ3 = VBLANK
	========================================================================


	========================================================================
	SOUND CPU
	========================================================================
	0000-0FFF   R/W   xxxxxxxx   Program RAM
	1000-17FF   R/W   xxxxxxxx   EEPROM
	1800-180F   R/W   xxxxxxxx   POKEY 1 (left) communications
	1810-1813   R     xxxxxxxx   LETA analog inputs
	1830-183F   R/W   xxxxxxxx   POKEY 2 (right) communications
	1850-1851   R/W   xxxxxxxx   YM2151 communications
	1860        R     xxxxxxxx   Sound command read
	1870          W   xxxxxxxx   TMS5220 data latch
	1872          W   --------   TMS5220 data strobe low
	1873          W   --------   TMS5220 data strobe high
	1874          W   xxxxxxxx   Sound response write
	1876          W   ------xx   Coin counters
	1878          W   --------   Interrupt acknowledge
	187A          W   xxxxxxxx   Mixer control
	              W   xxx-----      (TMS5220 volume)
	              W   ---xx---      (POKEY volume)
	              W   -----xxx      (YM2151 volume)
	187C          W   --xxxx--   Misc. control bits
	              W   --x-----      (TMS5220 frequency control)
	              W   ---x----      (LETA resolution control)
	              W   ----xx--      (LEDs)
	187E          W   -------x   Sound enable
	4000-FFFF   R     xxxxxxxx   Program ROM
	========================================================================
	Interrupts:
		IRQ = YM2151 interrupt
		NMI = latch on sound command
	========================================================================

****************************************************************************/

#include "driver.h"
#include "cpu/t11/t11.h"
#include "machine/atarigen.h"
#include "vidhrdw/generic.h"


extern UINT8 *atarisys2_slapstic;


READ_HANDLER( atarisys2_slapstic_r );
READ_HANDLER( atarisys2_videoram_r );

WRITE_HANDLER( atarisys2_slapstic_w );
WRITE_HANDLER( atarisys2_vscroll_w );
WRITE_HANDLER( atarisys2_hscroll_w );
WRITE_HANDLER( atarisys2_videoram_w );
WRITE_HANDLER( atarisys2_paletteram_w );

void atarisys2_scanline_update(int scanline);

int atarisys2_vh_start(void);
void atarisys2_vh_stop(void);
void atarisys2_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);



static UINT8 *interrupt_enable;
static UINT8 *bankselect;

static INT8 pedal_count;

static UINT8 has_tms5220;
static UINT8 tms5220_data;
static UINT8 tms5220_data_strobe;

static UINT8 which_adc;

static UINT8 p2portwr_state;
static UINT8 p2portrd_state;



/*************************************
 *
 *	Interrupt updating
 *
 *************************************/

static void update_interrupts(void)
{
	if (atarigen_video_int_state)
		cpu_set_irq_line(0, 3, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 3, CLEAR_LINE);

	if (atarigen_scanline_int_state)
		cpu_set_irq_line(0, 2, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 2, CLEAR_LINE);

	if (p2portwr_state)
		cpu_set_irq_line(0, 1, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 1, CLEAR_LINE);

	if (p2portrd_state)
		cpu_set_irq_line(0, 0, ASSERT_LINE);
	else
		cpu_set_irq_line(0, 0, CLEAR_LINE);
}



/*************************************
 *
 *	Every 8-scanline update
 *
 *************************************/

static void scanline_update(int scanline)
{
	/* update the display list */
	if (scanline < Machine->drv->screen_height)
		atarisys2_scanline_update(scanline);

	if (scanline <= Machine->drv->screen_height)
	{
		/* generate the 32V interrupt (IRQ 2) */
		if ((scanline % 64) == 0)
			if (READ_WORD(&interrupt_enable[0]) & 4)
				atarigen_scanline_int_gen();
	}
}



/*************************************
 *
 *	Initialization
 *
 *************************************/

static void init_machine(void)
{
	atarigen_eeprom_reset();
	slapstic_reset();
	atarigen_interrupt_reset(update_interrupts);
	atarigen_scanline_timer_reset(scanline_update, 8);
	atarigen_sound_io_reset(1);

	tms5220_data_strobe = 1;

	p2portwr_state = 0;
	p2portrd_state = 0;

	which_adc = 0;
}



/*************************************
 *
 *	Interrupt handlers
 *
 *************************************/

static int vblank_interrupt(void)
{
	/* clock the VBLANK through */
	if (READ_WORD(&interrupt_enable[0]) & 8)
		atarigen_video_int_gen();

	return 0;
}


static WRITE_HANDLER( interrupt_ack_w )
{
	switch (offset & 0x60)
	{
		/* reset sound IRQ */
		case 0x00:
			if (offset == 0x00)
			{
				p2portrd_state = 0;
				atarigen_update_interrupts();
			}
			break;

		/* reset sound CPU */
		case 0x20:
			if (!(data & 0xff000000))
				cpu_set_reset_line(1,ASSERT_LINE);
			if (!(data & 0x00ff0000))
				cpu_set_reset_line(1,CLEAR_LINE);
			break;

		/* reset 32V IRQ */
		case 0x40:
			atarigen_scanline_int_ack_w(0, 0);
			break;

		/* reset VBLANK IRQ */
		case 0x60:
			atarigen_video_int_ack_w(0, 0);
			break;
	}
}



/*************************************
 *
 *	Bank selection.
 *
 *************************************/

static WRITE_HANDLER( bankselect_w )
{
	static int bankoffset[64] =
	{
		0x28000, 0x20000, 0x18000, 0x10000,
		0x2a000, 0x22000, 0x1a000, 0x12000,
		0x2c000, 0x24000, 0x1c000, 0x14000,
		0x2e000, 0x26000, 0x1e000, 0x16000,
		0x48000, 0x40000, 0x38000, 0x30000,
		0x4a000, 0x42000, 0x3a000, 0x32000,
		0x4c000, 0x44000, 0x3c000, 0x34000,
		0x4e000, 0x46000, 0x3e000, 0x36000,
		0x68000, 0x60000, 0x58000, 0x50000,
		0x6a000, 0x62000, 0x5a000, 0x52000,
		0x6c000, 0x64000, 0x5c000, 0x54000,
		0x6e000, 0x66000, 0x5e000, 0x56000,
		0x88000, 0x80000, 0x78000, 0x70000,
		0x8a000, 0x82000, 0x7a000, 0x72000,
		0x8c000, 0x84000, 0x7c000, 0x74000,
		0x8e000, 0x86000, 0x7e000, 0x76000
	};

	int oldword = READ_WORD(&bankselect[offset]);
	int newword = COMBINE_WORD(oldword, data);
	UINT8 *RAM = memory_region(REGION_CPU1);
	UINT8 *base = &RAM[bankoffset[(newword >> 10) & 0x3f]];

	WRITE_WORD(&bankselect[offset], newword);
	if (offset == 0)
	{
		cpu_setbank(1, base);
		t11_SetBank(0x4000, base);
	}
	else if (offset == 2)
	{
		cpu_setbank(2, base);
		t11_SetBank(0x6000, base);
	}
}



/*************************************
 *
 *	I/O read dispatch.
 *
 *************************************/

static READ_HANDLER( switch_r )
{
	int result = input_port_1_r(offset) | (input_port_2_r(offset) << 8);

	if (atarigen_cpu_to_sound_ready) result ^= 0x20;
	if (atarigen_sound_to_cpu_ready) result ^= 0x10;

	return result;
}


static READ_HANDLER( switch_6502_r )
{
	int result = input_port_0_r(offset);

	if (atarigen_cpu_to_sound_ready) result ^= 0x01;
	if (atarigen_sound_to_cpu_ready) result ^= 0x02;
	if (!has_tms5220 || tms5220_ready_r()) result ^= 0x04;
	if (!(input_port_2_r(offset) & 0x80)) result ^= 0x10;

	return result;
}


static WRITE_HANDLER( switch_6502_w )
{
	(void)offset;

	if (has_tms5220)
	{
		data = 12 | ((data >> 5) & 1);
		tms5220_set_frequency(ATARI_CLOCK_20MHz/4 / (16 - data) / 2);
	}
}



/*************************************
 *
 *	Controls read
 *
 *************************************/

static WRITE_HANDLER( adc_strobe_w )
{
	(void)data;
	which_adc = (offset / 2) & 3;
}


static READ_HANDLER( adc_r )
{
	(void)offset;

	if (which_adc < pedal_count)
		return ~readinputport(3 + which_adc);

	return readinputport(3 + which_adc) | 0xff00;
}


static READ_HANDLER( leta_r )
{
    if (pedal_count == -1)   /* 720 */
	{
		switch (offset & 3)
		{
			case 0: return readinputport(7) >> 8;
			case 1: return readinputport(7) & 0xff;
			case 2: return 0xff;
			case 3: return 0xff;
		}
	}

	return readinputport(7 + (offset & 3));
}



/*************************************
 *
 *	Global sound control
 *
 *************************************/

static WRITE_HANDLER( mixer_w )
{
	(void)offset;

	atarigen_set_ym2151_vol((data & 7) * 100 / 7);
	atarigen_set_pokey_vol(((data >> 3) & 3) * 100 / 3);
	atarigen_set_tms5220_vol(((data >> 5) & 7) * 100 / 7);
}


static WRITE_HANDLER( sound_enable_w )
{
	(void)offset;
	(void)data;
}


static READ_HANDLER( sound_r )
{
	/* clear the p2portwr state on a p1portrd */
	p2portwr_state = 0;
	atarigen_update_interrupts();

	/* handle it normally otherwise */
	return atarigen_sound_r(offset);
}


static WRITE_HANDLER( sound_6502_w )
{
	/* clock the state through */
	p2portwr_state = (READ_WORD(&interrupt_enable[0]) & 2) != 0;
	atarigen_update_interrupts();

	/* handle it normally otherwise */
	atarigen_6502_sound_w(offset, data);
}


static READ_HANDLER( sound_6502_r )
{
	/* clock the state through */
	p2portrd_state = (READ_WORD(&interrupt_enable[0]) & 1) != 0;
	atarigen_update_interrupts();

	/* handle it normally otherwise */
	return atarigen_6502_sound_r(offset);
}


/*************************************
 *
 *	Speech chip
 *
 *************************************/

static WRITE_HANDLER( tms5220_w )
{
	(void)offset;
	tms5220_data = data;
}


static WRITE_HANDLER( tms5220_strobe_w )
{
	(void)data;

	if (!(offset & 1) && tms5220_data_strobe)
		if (has_tms5220)
			tms5220_data_w(0, tms5220_data);
	tms5220_data_strobe = offset & 1;
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress main_readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x1000, 0x11ff, paletteram_word_r },
	{ 0x1400, 0x1403, adc_r },
	{ 0x1800, 0x1801, switch_r },
	{ 0x1c00, 0x1c01, sound_r },
	{ 0x2000, 0x3fff, atarisys2_videoram_r },
	{ 0x4000, 0x5fff, MRA_BANK1 },
	{ 0x6000, 0x7fff, MRA_BANK2 },
	{ 0x8000, 0x81ff, atarisys2_slapstic_r },
	{ 0x8200, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress main_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x11ff, atarisys2_paletteram_w, &paletteram },
	{ 0x1400, 0x1403, bankselect_w, &bankselect },
	{ 0x1480, 0x148f, adc_strobe_w },
	{ 0x1580, 0x15ff, interrupt_ack_w },
	{ 0x1600, 0x1601, MWA_RAM, &interrupt_enable },
	{ 0x1680, 0x1681, atarigen_sound_w },
	{ 0x1700, 0x1701, atarisys2_hscroll_w, &atarigen_hscroll },
	{ 0x1780, 0x1781, atarisys2_vscroll_w, &atarigen_vscroll },
	{ 0x1800, 0x1801, watchdog_reset_w },
	{ 0x2000, 0x3fff, atarisys2_videoram_w },
	{ 0x4000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x81ff, atarisys2_slapstic_w, &atarisys2_slapstic },
	{ 0x8200, 0xffff, MWA_ROM },
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
	{ 0x1000, 0x17ff, MRA_RAM },
	{ 0x1800, 0x180f, pokey1_r },
	{ 0x1810, 0x1813, leta_r },
	{ 0x1830, 0x183f, pokey2_r },
	{ 0x1840, 0x1840, switch_6502_r },
	{ 0x1850, 0x1851, YM2151_status_port_0_r },
	{ 0x1860, 0x1860, sound_6502_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x17ff, MWA_RAM, &atarigen_eeprom, &atarigen_eeprom_size },
	{ 0x1800, 0x180f, pokey1_w },
	{ 0x1830, 0x183f, pokey2_w },
	{ 0x1850, 0x1850, YM2151_register_port_0_w },
	{ 0x1851, 0x1851, YM2151_data_port_0_w },
	{ 0x1870, 0x1870, tms5220_w },
	{ 0x1872, 0x1873, tms5220_strobe_w },
	{ 0x1874, 0x1874, sound_6502_w },
	{ 0x1876, 0x1876, MWA_NOP },
	{ 0x1878, 0x1878, atarigen_6502_irq_ack_w },
	{ 0x187a, 0x187a, mixer_w },
	{ 0x187c, 0x187c, switch_6502_w },
	{ 0x187e, 0x187e, sound_enable_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( paperboy )
	PORT_START	/* 1840 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* 1800 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* 1801 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* ADC0 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_PLAYER1, 100, 10, 0x10, 0xf0 )

	PORT_START	/* ADC1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_PLAYER1, 100, 10, 0x10, 0xf0 )

	PORT_START	/* ADC2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* ADC3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA0 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA1 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x0c, 0x00, "Right Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x04, "*4" )
	PORT_DIPSETTING(    0x08, "*5" )
	PORT_DIPSETTING(    0x0c, "*6" )
	PORT_DIPNAME( 0x10, 0x00, "Left Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x10, "*2" )
	PORT_DIPNAME( 0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPSETTING(    0x80, "1 each 5" )
	PORT_DIPSETTING(    0x40, "1 each 4" )
	PORT_DIPSETTING(    0xa0, "1 each 3" )
	PORT_DIPSETTING(    0x60, "2 each 4" )
	PORT_DIPSETTING(    0x20, "1 each 2" )
	PORT_DIPSETTING(    0xc0, "1 each ?" )
	PORT_DIPSETTING(    0xe0, DEF_STR( Free_Play ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Easy" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x00, "Med. Hard" )
	PORT_DIPSETTING(    0x03, "Hard" )
	PORT_DIPNAME( 0x0c, 0x08, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "10000" )
	PORT_DIPSETTING(    0x00, "15000" )
	PORT_DIPSETTING(    0x0c, "20000" )
	PORT_DIPSETTING(    0x04, "None" )
	PORT_DIPNAME( 0x30, 0x20, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x30, "5" )
	PORT_BITX( 0,       0x10, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( 720 )
	PORT_START	/* 1840 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* 1800 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* 1801 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* ADC0 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* ADC1 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* ADC2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* ADC3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA0/1 */
	PORT_ANALOG( 0xffff, 0x0000, IPT_DIAL | IPF_PLAYER1, 30, 10, 0, 0 )

	PORT_START	/* filler */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x0c, 0x00, "Right Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x04, "*4" )
	PORT_DIPSETTING(    0x08, "*5" )
	PORT_DIPSETTING(    0x0c, "*6" )
	PORT_DIPNAME( 0x10, 0x00, "Left Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x10, "*2" )
	PORT_DIPNAME( 0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING(    0x00, "None" )
/*	PORT_DIPSETTING(    0xc0, "None" )*/
	PORT_DIPSETTING(    0x80, "1 each 5" )
	PORT_DIPSETTING(    0x40, "1 each 4" )
	PORT_DIPSETTING(    0xa0, "1 each 3" )
	PORT_DIPSETTING(    0x60, "2 each 4" )
	PORT_DIPSETTING(    0x20, "1 each 2" )
	PORT_DIPSETTING(    0xe0, DEF_STR( Free_Play ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x01, "3000" )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x02, "8000" )
	PORT_DIPSETTING(    0x03, "12000" )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x00, "Medium" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x0c, "Hardest" )
	PORT_DIPNAME( 0x30, 0x10, "Maximum Add. A. Coins" )
	PORT_DIPSETTING(    0x10, "0" )
	PORT_DIPSETTING(    0x20, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPNAME( 0xc0, 0x40, "Coins Required" )
	PORT_DIPSETTING(    0x80, "3 to Start, 2 to Continue" )
	PORT_DIPSETTING(    0xc0, "3 to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x00, "2 to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, "1 to Start, 1 to Continue" )
INPUT_PORTS_END


INPUT_PORTS_START( ssprint )
	PORT_START	/* 1840 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3 )

	PORT_START	/* 1800 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* 1801 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* ADC0 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER1, 100, 4, 0x00, 0x3f )

	PORT_START	/* ADC1 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER2, 100, 4, 0x00, 0x3f )

	PORT_START	/* ADC2 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER3, 100, 4, 0x00, 0x3f )

	PORT_START	/* ADC3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA0 */
	PORT_ANALOG( 0xff, 0x40, IPT_DIAL | IPF_PLAYER1, 25, 10, 0x00, 0x7f )

	PORT_START	/* LETA1 */
	PORT_ANALOG( 0xff, 0x40, IPT_DIAL | IPF_PLAYER2, 25, 10, 0x00, 0x7f )

	PORT_START	/* LETA2 */
	PORT_ANALOG( 0xff, 0x40, IPT_DIAL | IPF_PLAYER3, 25, 10, 0x00, 0x7f )

	PORT_START	/* LETA3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x1c, 0x00, "Coin Multiplier" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x04, "*2" )
	PORT_DIPSETTING(    0x08, "*3" )
	PORT_DIPSETTING(    0x0c, "*4" )
	PORT_DIPSETTING(    0x10, "*5" )
	PORT_DIPSETTING(    0x14, "*6" )
	PORT_DIPSETTING(    0x18, "*7" )
	PORT_DIPSETTING(    0x1c, "*8" )
	PORT_DIPNAME( 0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPSETTING(    0x80, "1 each 5" )
	PORT_DIPSETTING(    0x40, "1 each 4" )
	PORT_DIPSETTING(    0xa0, "1 each 3" )
	PORT_DIPSETTING(    0x60, "2 each 4" )
	PORT_DIPSETTING(    0x20, "1 each 2" )
	PORT_DIPSETTING(    0xc0, "1 each ?" )
	PORT_DIPSETTING(    0xe0, DEF_STR( Free_Play ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Easy" )
	PORT_DIPSETTING(    0x00, "Medium" )
	PORT_DIPSETTING(    0x02, "Med. Hard" )
	PORT_DIPSETTING(    0x03, "Hard" )
	PORT_DIPNAME( 0x0c, 0x04, "Obstacles" )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x00, "Medium" )
	PORT_DIPSETTING(    0x08, "Med. Hard" )
	PORT_DIPSETTING(    0x0c, "Hard" )
	PORT_DIPNAME( 0x30, 0x00, "Wrenches" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x30, "5" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( csprint )
	PORT_START	/* 1840 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START	/* 1800 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* 1801 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* ADC0 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER1, 100, 4, 0x00, 0x3f )

	PORT_START	/* ADC1 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER2, 100, 4, 0x00, 0x3f )

	PORT_START	/* ADC2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* ADC3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA0 */
	PORT_ANALOG( 0xff, 0x40, IPT_DIAL | IPF_PLAYER1, 25, 10, 0x00, 0x7f )

	PORT_START	/* LETA1 */
	PORT_ANALOG( 0xff, 0x40, IPT_DIAL | IPF_PLAYER2, 25, 10, 0x00, 0x7f )

	PORT_START	/* LETA2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x1c, 0x00, "Coin Multiplier" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x04, "*2" )
	PORT_DIPSETTING(    0x08, "*3" )
	PORT_DIPSETTING(    0x0c, "*4" )
	PORT_DIPSETTING(    0x10, "*5" )
	PORT_DIPSETTING(    0x14, "*6" )
	PORT_DIPSETTING(    0x18, "*7" )
	PORT_DIPSETTING(    0x1c, "*8" )
	PORT_DIPNAME( 0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPSETTING(    0x80, "1 each 5" )
	PORT_DIPSETTING(    0x40, "1 each 4" )
	PORT_DIPSETTING(    0xa0, "1 each 3" )
	PORT_DIPSETTING(    0x60, "2 each 4" )
	PORT_DIPSETTING(    0x20, "1 each 2" )
	PORT_DIPSETTING(    0xc0, "1 each ?" )
	PORT_DIPSETTING(    0xe0, DEF_STR( Free_Play ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x01, "Easy" )
	PORT_DIPSETTING(    0x00, "Medium" )
	PORT_DIPSETTING(    0x02, "Med. Hard" )
	PORT_DIPSETTING(    0x03, "Hard" )
	PORT_DIPNAME( 0x0c, 0x04, "Obstacles" )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x00, "Medium" )
	PORT_DIPSETTING(    0x08, "Med. Hard" )
	PORT_DIPSETTING(    0x0c, "Hard" )
	PORT_DIPNAME( 0x30, 0x00, "Wrenches" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPSETTING(    0x30, "5" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Auto High Score Reset" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( apb )
	PORT_START	/* 1840 (sound) */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_SPECIAL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_COIN3  )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_COIN2  )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_COIN1  )

	PORT_START	/* 1800 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,  IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_SPECIAL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNUSED )

	PORT_START	/* 1801 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )

	PORT_START	/* ADC0 */
	PORT_BIT( 0xff, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START	/* ADC1 */
	PORT_ANALOG( 0xff, 0x00, IPT_PEDAL | IPF_PLAYER1, 100, 4, 0x00, 0x3f )

	PORT_START	/* ADC2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* ADC3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA0 */
	PORT_ANALOG( 0xff, 0x40, IPT_DIAL | IPF_PLAYER1, 25, 10, 0x00, 0x7f )

	PORT_START	/* LETA1 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA2 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* LETA3 */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x0c, 0x00, "Right Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x04, "*4" )
	PORT_DIPSETTING(    0x08, "*5" )
	PORT_DIPSETTING(    0x0c, "*6" )
	PORT_DIPNAME( 0x10, 0x00, "Left Coin" )
	PORT_DIPSETTING(    0x00, "*1" )
	PORT_DIPSETTING(    0x10, "*2" )
	PORT_DIPNAME( 0xe0, 0x00, "Bonus Coins" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_DIPSETTING(    0xc0, "1 each 6" )
	PORT_DIPSETTING(    0xa0, "1 each 5" )
	PORT_DIPSETTING(    0x80, "1 each 4" )
	PORT_DIPSETTING(    0x60, "1 each 3" )
	PORT_DIPSETTING(    0x40, "1 each 2" )
	PORT_DIPSETTING(    0x20, "1 each 1" )
	PORT_DIPSETTING(    0xe0, DEF_STR( Free_Play ) )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x01, 0x01, "Attract Lights" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x06, 0x06, "Max Continues" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x04, "10" )
	PORT_DIPSETTING(    0x00, "25" )
	PORT_DIPSETTING(    0x06, "199" )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x38, "1 (Easy)" )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x28, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPSETTING(    0x10, "6" )
	PORT_DIPSETTING(    0x08, "7" )
	PORT_DIPSETTING(    0x18, "8 (Hard)" )
	PORT_DIPNAME( 0xc0, 0x40, "Coins Required" )
	PORT_DIPSETTING(    0x80, "3 to Start, 2 to Continue" )
	PORT_DIPSETTING(    0xc0, "3 to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x00, "2 to Start, 1 to Continue" )
	PORT_DIPSETTING(    0x40, "1 to Start, 1 to Continue" )
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


static struct GfxLayout pflayout =
{
	8,8,
	RGN_FRAC(1,2),
	4,
	{ 0, 4, RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16
};


static struct GfxLayout molayout =
{
	16,16,
	RGN_FRAC(1,2),
	4,
	{ 0, 4, RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32, 8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32 },
	8*64
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &pflayout, 128, 8 },
	{ REGION_GFX2, 0, &molayout,   0, 4 },
	{ REGION_GFX3, 0, &anlayout,  64, 8 },
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
	{ 0 }
};


static struct POKEYinterface pokey_interface =
{
	2,
	ATARI_CLOCK_14MHz/8,
	{ MIXER(60,MIXER_PAN_LEFT), MIXER(60,MIXER_PAN_RIGHT) },
	/* The 8 pot handlers */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	/* The allpot handler */
	{ input_port_11_r, input_port_12_r },	/* dip switches */
};


static struct TMS5220interface tms5220_interface =
{
	ATARI_CLOCK_20MHz/4/4/2,
	100,
	0
};



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_paperboy =
{
	/* basic machine hardware */
	{
		{
			CPU_T11,
			ATARI_CLOCK_20MHz/2,
			main_readmem,main_writemem,0,0,
			vblank_interrupt,1
		},
		{
			CPU_M6502,
			ATARI_CLOCK_14MHz/8,
			sound_readmem,sound_writemem,0,0,
			0,0,
			atarigen_6502_irq_gen,(UINT32)(1000000000.0/((float)ATARI_CLOCK_20MHz/2/16/16/16/10))
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	init_machine,

	/* video hardware */
	64*8, 48*8, { 0*8, 64*8-1, 0*8, 48*8-1 },
	gfxdecodeinfo,
	256,256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	atarisys2_vh_start,
	atarisys2_vh_stop,
	atarisys2_vh_screenrefresh,

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


static struct MachineDriver machine_driver_a720 =
{
	/* basic machine hardware */
	{
		{
			CPU_T11,
			ATARI_CLOCK_20MHz/2,
			main_readmem,main_writemem,0,0,
			vblank_interrupt,1
		},
		{
			CPU_M6502,
			2000000,	/* artifically high to prevent deadlock at startup ATARI_CLOCK_14MHz/8,*/
			sound_readmem,sound_writemem,0,0,
			0,0,
			atarigen_6502_irq_gen,(UINT32)(1000000000.0/((float)ATARI_CLOCK_20MHz/2/16/16/16/10))
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	init_machine,

	/* video hardware */
	64*8, 48*8, { 0*8, 64*8-1, 0*8, 48*8-1 },
	gfxdecodeinfo,
	256,256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	atarisys2_vh_start,
	atarisys2_vh_stop,
	atarisys2_vh_screenrefresh,

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


static struct MachineDriver machine_driver_sprint =
{
	/* basic machine hardware */
	{
		{
			CPU_T11,
			ATARI_CLOCK_20MHz/2,
			main_readmem,main_writemem,0,0,
			vblank_interrupt,1
		},
		{
			CPU_M6502,
			ATARI_CLOCK_14MHz/8,
			sound_readmem,sound_writemem,0,0,
			0,0,
			atarigen_6502_irq_gen,(UINT32)(1000000000.0/((float)ATARI_CLOCK_20MHz/2/16/16/16/10))
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	init_machine,

	/* video hardware */
	64*8, 48*8, { 0*8, 64*8-1, 0*8, 48*8-1 },
	gfxdecodeinfo,
	256,256,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_SUPPORTS_DIRTY,
	0,
	atarisys2_vh_start,
	atarisys2_vh_stop,
	atarisys2_vh_screenrefresh,

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
		}
	},

	atarigen_nvram_handler
};



/*************************************
 *
 *	ROM decoding
 *
 *************************************/

static void rom_decode(void)
{
	int i;

	/* invert the bits of the sprites */
	for (i = 0; i < memory_region_length(REGION_GFX2); i++)
		memory_region(REGION_GFX2)[i] ^= 0xff;
}



/*************************************
 *
 *	Driver initialization
 *
 *************************************/

static void init_paperboy(void)
{
	static const UINT16 compressed_default_eeprom[] =
	{
		0x0000,0x4300,0x0113,0x0124,0x0150,0x0153,0x0154,0x0100,
		0x0112,0x01C0,0x0155,0x0143,0x0148,0x0100,0x0112,0x015C,
		0x0154,0x014F,0x0149,0x0100,0x0111,0x01F8,0x0120,0x0152,
		0x0153,0x0100,0x0111,0x0149,0x0159,0x0145,0x0120,0x0100,
		0x0111,0x0130,0x014F,0x0120,0x0154,0x0100,0x0110,0x01CC,
		0x0155,0x014F,0x0141,0x0100,0x0110,0x0168,0x0152,0x014E,
		0x0142,0x0100,0x0110,0x0104,0x0120,0x0154,0x014C,0x0100,
		0x010F,0x01A0,0x0120,0x014F,0x0145,0x0100,0x0126,0x01AC,
		0x0150,0x0149,0x0147,0x0100,0x0126,0x0148,0x0141,0x0153,
		0x0152,0x0100,0x0125,0x01E4,0x0150,0x0120,0x0145,0x0100,
		0x0125,0x0180,0x0145,0x0154,0x0141,0x0100,0x0125,0x011C,
		0x0152,0x0148,0x0154,0x0100,0x0124,0x01B8,0x0120,0x0245,
		0x0100,0x0124,0x0154,0x0142,0x0120,0x0153,0x0100,0x0123,
		0x01F0,0x014F,0x0120,0x0154,0x0100,0x0123,0x018C,0x0159,
		0x0220,0x0100,0x0123,0x0128,0x0320,0x0100,0x013A,0x0134,
		0x0144,0x0141,0x0154,0x0100,0x0139,0x01D0,0x0154,0x0148,
		0x0145,0x0100,0x0139,0x016C,0x0142,0x014F,0x0159,0x0100,
		0x0139,0x0108,0x0142,0x0146,0x0120,0x0100,0x0138,0x01A4,
		0x014D,0x0145,0x0143,0x0100,0x0138,0x0140,0x0143,0x014A,
		0x0120,0x0100,0x0137,0x01DC,0x014A,0x0145,0x0153,0x0100,
		0x0137,0x0178,0x0150,0x0143,0x0154,0x0100,0x0137,0x0114,
		0x014D,0x0241,0x0100,0x0136,0x01B0,0x0142,0x0141,0x0146,
		0x0101,0x0400,0x010F,0x017F,0x012A,0x017F,0x013D,0x010F,
		0x017F,0x012A,0x017F,0x013C,0x010F,0x017F,0x012A,0x017F,
		0x013B,0xFF00,0xFF00,0xFF00,0xFF00,0xFF00,0xFF00,0xFC00,
		0x0000
	};
	int i;

	/* expand the 16k program ROMs into full 64k chunks */
	for (i = 0x10000; i < 0x90000; i += 0x20000)
	{
		memcpy(&memory_region(REGION_CPU1)[i + 0x08000], &memory_region(REGION_CPU1)[i], 0x8000);
		memcpy(&memory_region(REGION_CPU1)[i + 0x10000], &memory_region(REGION_CPU1)[i], 0x8000);
		memcpy(&memory_region(REGION_CPU1)[i + 0x18000], &memory_region(REGION_CPU1)[i], 0x8000);
	}

	atarigen_eeprom_default = compressed_default_eeprom;
	slapstic_init(105);

	pedal_count = 0;
	has_tms5220 = 1;

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x410f, 0x4127);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_a720(void)
{
	atarigen_eeprom_default = NULL;
	slapstic_init(107);

	pedal_count = -1;
	has_tms5220 = 1;

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x410f, 0x4127);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_ssprint(void)
{
	static const UINT16 compressed_default_eeprom[] =
	{
		0x0000,0x01FF,0x0E00,0x01FF,0x0100,0x0120,0x0100,0x0120,
		0x0300,0x0120,0x0500,0x0120,0x01FF,0x0100,0x0140,0x0100,
		0x0140,0x0110,0x0100,0x0110,0x0150,0x0100,0x0110,0x0300,
		0x0140,0x01FF,0x0100,0x0160,0x0100,0x0160,0x0300,0x0160,
		0x0500,0x0160,0x01FF,0x0100,0x0180,0x0100,0x0180,0x0300,
		0x0180,0x0500,0x0180,0x01FF,0x0100,0x01A0,0x0100,0x01A0,
		0x0300,0x01A0,0x0500,0x01A0,0x01FF,0x0100,0x01C0,0x0100,
		0x01C0,0x0300,0x01C0,0x0500,0x01C0,0xFFFF,0x1EFF,0x0103,
		0x01E8,0x0146,0x01D6,0x0103,0x01DE,0x0128,0x01B3,0x0103,
		0x01D4,0x0144,0x0123,0x0103,0x01CA,0x011C,0x010B,0x0103,
		0x01C0,0x0159,0x01BF,0x0103,0x01B6,0x0129,0x019F,0x0103,
		0x01AC,0x014A,0x01C2,0x0103,0x01A2,0x010E,0x01DF,0x0103,
		0x0198,0x0131,0x01BF,0x0103,0x018E,0x010D,0x0106,0x0103,
		0x0184,0x010E,0x0186,0x0103,0x017A,0x0124,0x010C,0x0103,
		0x0170,0x014A,0x0148,0x0103,0x0166,0x0151,0x01F2,0x0103,
		0x015C,0x013E,0x013F,0x0103,0x0152,0x0111,0x0106,0x0103,
		0x0148,0x0145,0x01B1,0x0103,0x013E,0x017E,0x0164,0x0103,
		0x0134,0x017F,0x01E0,0x0103,0x012A,0x017F,0x01F3,0x0103,
		0x0120,0x017F,0x01FF,0x0103,0x0116,0x012A,0x01D6,0x0103,
		0x010C,0x0125,0x0176,0x0103,0x0102,0x014C,0x0161,0x0102,
		0x01F8,0x0128,0x0101,0x0102,0x01EE,0x0101,0x0153,0x0102,
		0x01E4,0x0109,0x0132,0x0102,0x01DA,0x012C,0x0132,0x0102,
		0x01D0,0x0125,0x0186,0x0102,0x01C6,0x011D,0x011F,0xFF00,
		0xFF00,0xFF00,0xFF00,0xFF00,0xFF00,0x0800,0x0000
	};
	int i;

	/* expand the 32k program ROMs into full 64k chunks */
	for (i = 0x10000; i < 0x90000; i += 0x20000)
		memcpy(&memory_region(REGION_CPU1)[i + 0x10000], &memory_region(REGION_CPU1)[i], 0x10000);

	atarigen_eeprom_default = compressed_default_eeprom;
	slapstic_init(108);

	pedal_count = 3;
	has_tms5220 = 0;

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x8107, 0x811f);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_csprint(void)
{
	static const UINT16 compressed_default_eeprom[] =
	{
		0x0000,0x01FF,0x0E00,0x0128,0x01D0,0x0127,0x0100,0x0120,
		0x0300,0x01F7,0x01D0,0x0107,0x0300,0x0120,0x010F,0x01F0,
		0x0140,0x0100,0x0140,0x0110,0x0100,0x0110,0x01A0,0x01F0,
		0x0110,0x0300,0x0140,0x01FF,0x0100,0x0160,0x0100,0x0160,
		0x0300,0x0160,0x0500,0x0160,0x01FF,0x0100,0x0180,0x0100,
		0x0180,0x0300,0x0180,0x0500,0x0180,0x01FF,0x0100,0x01A0,
		0x0100,0x01A0,0x0300,0x01A0,0x0500,0x01A0,0x01FF,0x0100,
		0x01C0,0x0100,0x01C0,0x0300,0x01C0,0x0500,0x01C0,0xFFFF,
		0x0100,0x0127,0x0110,0x0146,0x01D6,0x0100,0x0126,0x01AC,
		0x0128,0x01B3,0x0100,0x0126,0x0148,0x0144,0x0123,0x0100,
		0x0125,0x01E4,0x011C,0x010B,0x0100,0x0125,0x0180,0x0159,
		0x01BF,0x0100,0x0125,0x011C,0x0129,0x019F,0x0100,0x0124,
		0x0168,0x014A,0x01C2,0x0100,0x0124,0x0154,0x010E,0x01DF,
		0x0100,0x0123,0x01F0,0x0131,0x01BF,0x0100,0x0123,0x018C,
		0x010D,0x0106,0x0100,0x0123,0x0128,0x010E,0x0186,0x0100,
		0x0122,0x01C4,0x0124,0x010C,0x0100,0x0122,0x0160,0x014A,
		0x0148,0x0100,0x0121,0x01FC,0x0151,0x01F2,0x0100,0x0121,
		0x0198,0x013E,0x013F,0x0100,0x0121,0x0134,0x0111,0x0106,
		0x0100,0x0120,0x01D0,0x0145,0x01B1,0x0100,0x0120,0x016C,
		0x017E,0x0164,0x0100,0x0120,0x0108,0x017F,0x01E0,0x0100,
		0x011F,0x01A4,0x017F,0x01F3,0x0100,0x011F,0x0140,0x017F,
		0x01FF,0x0100,0x011E,0x01DC,0x012A,0x01D6,0x0100,0x011E,
		0x0178,0x0125,0x0176,0x0100,0x011E,0x0114,0x014C,0x0161,
		0x0100,0x011D,0x01B0,0x0128,0x0101,0x0100,0x011D,0x014C,
		0x0101,0x0153,0x0100,0x011C,0x01E8,0x0109,0x0132,0x0100,
		0x011C,0x0184,0x012C,0x0132,0x0100,0x011C,0x0120,0x0125,
		0x0186,0x0100,0x011B,0x01BC,0x011D,0x011F,0x0000
	};
	int i;

	/* expand the 32k program ROMs into full 64k chunks */
	for (i = 0x10000; i < 0x90000; i += 0x20000)
		memcpy(&memory_region(REGION_CPU1)[i + 0x10000], &memory_region(REGION_CPU1)[i], 0x10000);

	atarigen_eeprom_default = compressed_default_eeprom;
	slapstic_init(109);

	pedal_count = 2;
	has_tms5220 = 0;

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x8107, 0x811f);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}


static void init_apb(void)
{
	atarigen_eeprom_default = NULL;
	slapstic_init(110);

	pedal_count = 2;
	has_tms5220 = 1;

	/* speed up the 6502 */
	atarigen_init_6502_speedup(1, 0x410f, 0x4127);

	/* display messages */
	atarigen_show_sound_message();

	rom_decode();
}



/*************************************
 *
 *	ROM definition(s)
 *
 *************************************/

ROM_START( paperboy )
	ROM_REGION( 0x90000, REGION_CPU1 )	/* 9*64k for T11 code */
	ROM_LOAD_ODD ( "cpu_l07.bin",  0x08000, 0x04000, 0x4024bb9b )
	ROM_LOAD_EVEN( "cpu_n07.bin",  0x08000, 0x04000, 0x0260901a )
	ROM_LOAD_ODD ( "cpu_f06.bin",  0x10000, 0x04000, 0x3fea86ac )
	ROM_LOAD_EVEN( "cpu_n06.bin",  0x10000, 0x04000, 0x711b17ba )
	ROM_LOAD_ODD ( "cpu_j06.bin",  0x30000, 0x04000, 0xa754b12d )
	ROM_LOAD_EVEN( "cpu_p06.bin",  0x30000, 0x04000, 0x89a1ff9c )
	ROM_LOAD_ODD ( "cpu_k06.bin",  0x50000, 0x04000, 0x290bb034 )
	ROM_LOAD_EVEN( "cpu_r06.bin",  0x50000, 0x04000, 0x826993de )
	ROM_LOAD_ODD ( "cpu_l06.bin",  0x70000, 0x04000, 0x8a754466 )
	ROM_LOAD_EVEN( "cpu_s06.bin",  0x70000, 0x04000, 0x224209f9 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "cpu_a02.bin",  0x04000, 0x04000, 0x4a759092 )
	ROM_LOAD( "cpu_b02.bin",  0x08000, 0x04000, 0xe4e7a8b9 )
	ROM_LOAD( "cpu_c02.bin",  0x0c000, 0x04000, 0xd44c2aa2 )

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_a06.bin",  0x000000, 0x08000, 0xb32ffddf ) 	/* playfield, planes 0/1 */
	ROM_LOAD( "vid_b06.bin",  0x00c000, 0x04000, 0x301b849d )
	ROM_LOAD( "vid_c06.bin",  0x010000, 0x08000, 0x7bb59d68 ) 	/* playfield, planes 2/3 */
	ROM_LOAD( "vid_d06.bin",  0x01c000, 0x04000, 0x1a1d4ba8 )

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_l06.bin",  0x000000, 0x08000, 0x067ef202 )	/* motion objects, planes 0/1 */
	ROM_LOAD( "vid_k06.bin",  0x008000, 0x08000, 0x76b977c4 )
	ROM_LOAD( "vid_j06.bin",  0x010000, 0x08000, 0x2a3cc8d0 )
	ROM_LOAD( "vid_h06.bin",  0x018000, 0x08000, 0x6763a321 )
	ROM_LOAD( "vid_s06.bin",  0x020000, 0x08000, 0x0a321b7b )	/* motion objects, planes 2/3 */
	ROM_LOAD( "vid_p06.bin",  0x028000, 0x08000, 0x5bd089ee )
	ROM_LOAD( "vid_n06.bin",  0x030000, 0x08000, 0xc34a517d )
	ROM_LOAD( "vid_m06.bin",  0x038000, 0x08000, 0xdf723956 )

	ROM_REGION( 0x2000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vid_t06.bin",  0x000000, 0x02000, 0x60d7aebb )	/* alphanumerics */
ROM_END


ROM_START( 720 )
	ROM_REGION( 0x90000, REGION_CPU1 )     /* 9 * 64k T11 code */
	ROM_LOAD_ODD ( "3126.rom",     0x08000, 0x04000, 0x43abd367 )
	ROM_LOAD_EVEN( "3127.rom",     0x08000, 0x04000, 0x772e1e5b )
	ROM_LOAD_ODD ( "3128.rom",     0x10000, 0x10000, 0xbf6f425b )
	ROM_LOAD_EVEN( "4131.rom",     0x10000, 0x10000, 0x2ea8a20f )
	ROM_LOAD_ODD ( "1129.rom",     0x30000, 0x10000, 0xeabf0b01 )
	ROM_LOAD_EVEN( "1132.rom",     0x30000, 0x10000, 0xa24f333e )
	ROM_LOAD_ODD ( "1130.rom",     0x50000, 0x10000, 0x93fba845 )
	ROM_LOAD_EVEN( "1133.rom",     0x50000, 0x10000, 0x53c177be )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 6502 code */
	ROM_LOAD( "1134.rom",     0x04000, 0x04000, 0x09a418c2 )
	ROM_LOAD( "1135.rom",     0x08000, 0x04000, 0xb1f157d0 )
	ROM_LOAD( "1136.rom",     0x0c000, 0x04000, 0xdad40e6d )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1121.rom",     0x000000, 0x08000, 0x7adb5f9a ) 	/* playfield, planes 0/1 */
	ROM_LOAD( "1122.rom",     0x008000, 0x08000, 0x41b60141 )
	ROM_LOAD( "1123.rom",     0x010000, 0x08000, 0x501881d5 )
	ROM_LOAD( "1124.rom",     0x018000, 0x08000, 0x096f2574 )
	ROM_LOAD( "1117.rom",     0x020000, 0x08000, 0x5a55f149 ) 	/* playfield, planes 2/3 */
	ROM_LOAD( "1118.rom",     0x028000, 0x08000, 0x9bb2429e )
	ROM_LOAD( "1119.rom",     0x030000, 0x08000, 0x8f7b20e5 )
	ROM_LOAD( "1120.rom",     0x038000, 0x08000, 0x46af6d35 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1109.rom",     0x020000, 0x08000, 0x0a46b693 )	/* motion objects, planes 0/1 */
	ROM_CONTINUE(             0x000000, 0x08000 )
	ROM_LOAD( "1110.rom",     0x028000, 0x08000, 0x457d7e38 )
	ROM_CONTINUE(             0x008000, 0x08000 )
	ROM_LOAD( "1111.rom",     0x030000, 0x08000, 0xffad0a5b )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "1112.rom",     0x038000, 0x08000, 0x06664580 )
	ROM_CONTINUE(             0x018000, 0x08000 )
	ROM_LOAD( "1113.rom",     0x060000, 0x08000, 0x7445dc0f )
	ROM_CONTINUE(             0x040000, 0x08000 )
	ROM_LOAD( "1114.rom",     0x068000, 0x08000, 0x23eaceb0 )
	ROM_CONTINUE(             0x048000, 0x08000 )
	ROM_LOAD( "1115.rom",     0x070000, 0x08000, 0x0cc8de53 )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "1116.rom",     0x078000, 0x08000, 0x2d8f1369 )
	ROM_CONTINUE(             0x058000, 0x08000 )
	ROM_LOAD( "1101.rom",     0x0a0000, 0x08000, 0x2ac77b80 )	/* motion objects, planes 2/3 */
	ROM_CONTINUE(             0x080000, 0x08000 )
	ROM_LOAD( "1102.rom",     0x0a8000, 0x08000, 0xf19c3b06 )
	ROM_CONTINUE(             0x088000, 0x08000 )
	ROM_LOAD( "1103.rom",     0x0b0000, 0x08000, 0x78f9ab90 )
	ROM_CONTINUE(             0x090000, 0x08000 )
	ROM_LOAD( "1104.rom",     0x0b8000, 0x08000, 0x77ce4a7f )
	ROM_CONTINUE(             0x098000, 0x08000 )
	ROM_LOAD( "1105.rom",     0x0e0000, 0x08000, 0xbef5a025 )
	ROM_CONTINUE(             0x0c0000, 0x08000 )
	ROM_LOAD( "1106.rom",     0x0e8000, 0x08000, 0x92a159c8 )
	ROM_CONTINUE(             0x0c8000, 0x08000 )
	ROM_LOAD( "1107.rom",     0x0f0000, 0x08000, 0x0a94a3ef )
	ROM_CONTINUE(             0x0d0000, 0x08000 )
	ROM_LOAD( "1108.rom",     0x0f8000, 0x08000, 0x9815eda6 )
	ROM_CONTINUE(             0x0d8000, 0x08000 )

	ROM_REGION( 0x4000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1125.rom",     0x000000, 0x04000, 0x6b7e2328 )	/* alphanumerics */
ROM_END


ROM_START( 720b )
	ROM_REGION( 0x90000, REGION_CPU1 )     /* 9 * 64k T11 code */
	ROM_LOAD_ODD ( "2126.7l",      0x08000, 0x04000, 0xd07e731c )
	ROM_LOAD_EVEN( "2127.7n",      0x08000, 0x04000, 0x2d19116c )
	ROM_LOAD_ODD ( "2128.6f",      0x10000, 0x10000, 0xedad0bc0 )
	ROM_LOAD_EVEN( "3131.6n",      0x10000, 0x10000, 0x704dc925 )
	ROM_LOAD_ODD ( "1129.rom",     0x30000, 0x10000, 0xeabf0b01 )
	ROM_LOAD_EVEN( "1132.rom",     0x30000, 0x10000, 0xa24f333e )
	ROM_LOAD_ODD ( "1130.rom",     0x50000, 0x10000, 0x93fba845 )
	ROM_LOAD_EVEN( "1133.rom",     0x50000, 0x10000, 0x53c177be )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 6502 code */
	ROM_LOAD( "1134.rom",     0x04000, 0x04000, 0x09a418c2 )
	ROM_LOAD( "1135.rom",     0x08000, 0x04000, 0xb1f157d0 )
	ROM_LOAD( "1136.rom",     0x0c000, 0x04000, 0xdad40e6d )

	ROM_REGION( 0x40000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1121.rom",     0x000000, 0x08000, 0x7adb5f9a ) 	/* playfield, planes 0/1 */
	ROM_LOAD( "1122.rom",     0x008000, 0x08000, 0x41b60141 )
	ROM_LOAD( "1123.rom",     0x010000, 0x08000, 0x501881d5 )
	ROM_LOAD( "1124.rom",     0x018000, 0x08000, 0x096f2574 )
	ROM_LOAD( "1117.rom",     0x020000, 0x08000, 0x5a55f149 ) 	/* playfield, planes 2/3 */
	ROM_LOAD( "1118.rom",     0x028000, 0x08000, 0x9bb2429e )
	ROM_LOAD( "1119.rom",     0x030000, 0x08000, 0x8f7b20e5 )
	ROM_LOAD( "1120.rom",     0x038000, 0x08000, 0x46af6d35 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1109.rom",     0x020000, 0x08000, 0x0a46b693 )	/* motion objects, planes 0/1 */
	ROM_CONTINUE(             0x000000, 0x08000 )
	ROM_LOAD( "1110.rom",     0x028000, 0x08000, 0x457d7e38 )
	ROM_CONTINUE(             0x008000, 0x08000 )
	ROM_LOAD( "1111.rom",     0x030000, 0x08000, 0xffad0a5b )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "1112.rom",     0x038000, 0x08000, 0x06664580 )
	ROM_CONTINUE(             0x018000, 0x08000 )
	ROM_LOAD( "1113.rom",     0x060000, 0x08000, 0x7445dc0f )
	ROM_CONTINUE(             0x040000, 0x08000 )
	ROM_LOAD( "1114.rom",     0x068000, 0x08000, 0x23eaceb0 )
	ROM_CONTINUE(             0x048000, 0x08000 )
	ROM_LOAD( "1115.rom",     0x070000, 0x08000, 0x0cc8de53 )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "1116.rom",     0x078000, 0x08000, 0x2d8f1369 )
	ROM_CONTINUE(             0x058000, 0x08000 )
	ROM_LOAD( "1101.rom",     0x0a0000, 0x08000, 0x2ac77b80 )	/* motion objects, planes 2/3 */
	ROM_CONTINUE(             0x080000, 0x08000 )
	ROM_LOAD( "1102.rom",     0x0a8000, 0x08000, 0xf19c3b06 )
	ROM_CONTINUE(             0x088000, 0x08000 )
	ROM_LOAD( "1103.rom",     0x0b0000, 0x08000, 0x78f9ab90 )
	ROM_CONTINUE(             0x090000, 0x08000 )
	ROM_LOAD( "1104.rom",     0x0b8000, 0x08000, 0x77ce4a7f )
	ROM_CONTINUE(             0x098000, 0x08000 )
	ROM_LOAD( "1105.rom",     0x0e0000, 0x08000, 0xbef5a025 )
	ROM_CONTINUE(             0x0c0000, 0x08000 )
	ROM_LOAD( "1106.rom",     0x0e8000, 0x08000, 0x92a159c8 )
	ROM_CONTINUE(             0x0c8000, 0x08000 )
	ROM_LOAD( "1107.rom",     0x0f0000, 0x08000, 0x0a94a3ef )
	ROM_CONTINUE(             0x0d0000, 0x08000 )
	ROM_LOAD( "1108.rom",     0x0f8000, 0x08000, 0x9815eda6 )
	ROM_CONTINUE(             0x0d8000, 0x08000 )

	ROM_REGION( 0x4000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1125.rom",     0x000000, 0x04000, 0x6b7e2328 )	/* alphanumerics */
ROM_END


ROM_START( ssprint )
	ROM_REGION( 0x90000, REGION_CPU1 )	/* 9*64k for T11 code */
	ROM_LOAD_ODD ( "136042.330",   0x08000, 0x04000, 0xee312027 )
	ROM_LOAD_EVEN( "136042.331",   0x08000, 0x04000, 0x2ef15354 )
	ROM_LOAD_ODD ( "136042.329",   0x10000, 0x08000, 0xed1d6205 )
	ROM_LOAD_EVEN( "136042.325",   0x10000, 0x08000, 0xaecaa2bf )
	ROM_LOAD_ODD ( "136042.127",   0x50000, 0x08000, 0xde6c4db9 )
	ROM_LOAD_EVEN( "136042.123",   0x50000, 0x08000, 0xaff23b5a )
	ROM_LOAD_ODD ( "136042.126",   0x70000, 0x08000, 0x92f5392c )
	ROM_LOAD_EVEN( "136042.122",   0x70000, 0x08000, 0x0381f362 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "136042.419",   0x08000, 0x4000, 0xb277915a )
	ROM_LOAD( "136042.420",   0x0c000, 0x4000, 0x170b2c53 )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136042.105",   0x020000, 0x08000, 0x911499fe ) 	/* playfield, planes 0/1 */
	ROM_CONTINUE(             0x000000, 0x08000 )
	ROM_LOAD( "136042.106",   0x008000, 0x08000, 0xa39b25ed )
	ROM_LOAD( "136042.101",   0x030000, 0x08000, 0x6d015c72 )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "136042.102",   0x018000, 0x08000, 0x54e21f0a )
	ROM_LOAD( "136042.107",   0x060000, 0x08000, 0xb7ded658 ) 	/* playfield, planes 2/3 */
	ROM_CONTINUE(             0x040000, 0x08000 )
	ROM_LOAD( "136042.108",   0x048000, 0x08000, 0x4a804a4c )
	ROM_LOAD( "136042.104",   0x070000, 0x08000, 0x339644ed )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "136042.103",   0x058000, 0x08000, 0x64d473a8 )

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136042.113",   0x000000, 0x08000, 0xf869b0fc )	/* motion objects, planes 0/1 */
	ROM_LOAD( "136042.112",   0x008000, 0x08000, 0xabcbc114 )
	ROM_LOAD( "136042.110",   0x010000, 0x08000, 0x9e91e734 )
	ROM_LOAD( "136042.109",   0x018000, 0x08000, 0x3a051f36 )
	ROM_LOAD( "136042.117",   0x020000, 0x08000, 0xb15c1b90 )	/* motion objects, planes 2/3 */
	ROM_LOAD( "136042.116",   0x028000, 0x08000, 0x1dcdd5aa )
	ROM_LOAD( "136042.115",   0x030000, 0x08000, 0xfb5677d9 )
	ROM_LOAD( "136042.114",   0x038000, 0x08000, 0x35e70a8d )

	ROM_REGION( 0x4000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136042.218",   0x000000, 0x04000, 0x8e500be1 )  /* alphanumerics */
ROM_END


ROM_START( csprint )
	ROM_REGION( 0x90000, REGION_CPU1 )	/* 9*64k for T11 code */
	ROM_LOAD_ODD ( "045-2126.7l",  0x08000, 0x04000, 0x0ff83de8 )
	ROM_LOAD_EVEN( "045-1127.7mn", 0x08000, 0x04000, 0xe3e37258 )
	ROM_LOAD_ODD ( "045-1125.6f",  0x10000, 0x08000, 0x650623d2 )
	ROM_LOAD_EVEN( "045-1122.6mn", 0x10000, 0x08000, 0xca1b1cbf )
	ROM_LOAD_ODD ( "045-1124.6k",  0x50000, 0x08000, 0x47efca1f )
	ROM_LOAD_EVEN( "045-1121.6r",  0x50000, 0x08000, 0x6ca404bb )
	ROM_LOAD_ODD ( "045-1123.6l",  0x70000, 0x08000, 0x0a4d216a )
	ROM_LOAD_EVEN( "045-1120.6s",  0x70000, 0x08000, 0x103f3fde )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for 6502 code */
	ROM_LOAD( "045-1118.2bc", 0x08000, 0x4000, 0xeba41b2f )
	ROM_LOAD( "045-1119.2d",  0x0c000, 0x4000, 0x9e49043a )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "045-1105.6a",  0x000000, 0x08000, 0x3773bfbb ) 	/* playfield, planes 0/1 */
	ROM_LOAD( "045-1106.6b",  0x008000, 0x08000, 0x13a24886 )
	ROM_LOAD( "045-1101.7a",  0x030000, 0x08000, 0x5a55f931 )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "045-1102.7b",  0x018000, 0x08000, 0x37548a60 )
	ROM_LOAD( "045-1107.6c",  0x040000, 0x08000, 0xe35e354e ) 	/* playfield, planes 2/3 */
	ROM_LOAD( "045-1108.6d",  0x048000, 0x08000, 0x361db8b7 )
	ROM_LOAD( "045-1104.7d",  0x070000, 0x08000, 0xd1f8fe7b )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "045-1103.7c",  0x058000, 0x08000, 0x8f8c9692 )

	ROM_REGION( 0x40000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "045-1112.6t",  0x000000, 0x08000, 0xf869b0fc )	/* motion objects, planes 0/1 */
	ROM_LOAD( "045-1111.6s",  0x008000, 0x08000, 0xabcbc114 )
	ROM_LOAD( "045-1110.6p",  0x010000, 0x08000, 0x9e91e734 )
	ROM_LOAD( "045-1109.6n",  0x018000, 0x08000, 0x3a051f36 )
	ROM_LOAD( "045-1116.5t",  0x020000, 0x08000, 0xb15c1b90 )	/* motion objects, planes 2/3 */
	ROM_LOAD( "045-1115.5s",  0x028000, 0x08000, 0x1dcdd5aa )
	ROM_LOAD( "045-1114.5p",  0x030000, 0x08000, 0xfb5677d9 )
	ROM_LOAD( "045-1113.5n",  0x038000, 0x08000, 0x35e70a8d )

	ROM_REGION( 0x4000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "045-1117.4t",  0x000000, 0x04000, 0x82da786d )  /* alphanumerics */
ROM_END


ROM_START( apb )
	ROM_REGION( 0x90000, REGION_CPU1 )     /* 9 * 64k T11 code */
	ROM_LOAD_ODD ( "2126",    0x08000, 0x04000, 0x8edf4726 )
	ROM_LOAD_EVEN( "2127",    0x08000, 0x04000, 0xe2b2aff2 )
	ROM_LOAD_ODD ( "5128",    0x10000, 0x10000, 0x4b4ff365 )
	ROM_LOAD_EVEN( "5129",    0x10000, 0x10000, 0x059ab792 )
	ROM_LOAD_ODD ( "1130",    0x30000, 0x10000, 0xf64c752e )
	ROM_LOAD_EVEN( "1131",    0x30000, 0x10000, 0x0a506e04 )
	ROM_LOAD_ODD ( "1132",    0x70000, 0x10000, 0x6d0e7a4e )
	ROM_LOAD_EVEN( "1133",    0x70000, 0x10000, 0xaf88d429 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 6502 code */
	ROM_LOAD( "4134",         0x04000, 0x04000, 0x45e03b0e )
	ROM_LOAD( "4135",         0x08000, 0x04000, 0xb4ca24b2 )
	ROM_LOAD( "4136",         0x0c000, 0x04000, 0x11efaabf )

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1118",         0x000000, 0x08000, 0x93752c49 ) 	/* playfield, planes 0/1 */
	ROM_LOAD( "1120",         0x028000, 0x08000, 0x043086f8 )
	ROM_CONTINUE(             0x008000, 0x08000 )
	ROM_LOAD( "1122",         0x030000, 0x08000, 0x5ee79481 )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "1124",         0x038000, 0x08000, 0x27760395 )
	ROM_CONTINUE(             0x018000, 0x08000 )
	ROM_LOAD( "1117",         0x040000, 0x08000, 0xcfc3f8a3 ) 	/* playfield, planes 2/3 */
	ROM_LOAD( "1119",         0x068000, 0x08000, 0x68850612 )
	ROM_CONTINUE(             0x048000, 0x08000 )
	ROM_LOAD( "1121",         0x070000, 0x08000, 0xc7977062 )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "1123",         0x078000, 0x08000, 0x3c96c848 )
	ROM_CONTINUE(             0x058000, 0x08000 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1105",         0x020000, 0x08000, 0x9b78a88e )	/* motion objects, planes 0/1 */
	ROM_CONTINUE(             0x000000, 0x08000 )
	ROM_LOAD( "1106",         0x028000, 0x08000, 0x4787ff58 )
	ROM_CONTINUE(             0x008000, 0x08000 )
	ROM_LOAD( "1107",         0x030000, 0x08000, 0x0e85f2ac )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "1108",         0x038000, 0x08000, 0x70ff9308 )
	ROM_CONTINUE(             0x018000, 0x08000 )
	ROM_LOAD( "1113",         0x060000, 0x08000, 0x4a445356 )
	ROM_CONTINUE(             0x040000, 0x08000 )
	ROM_LOAD( "1114",         0x068000, 0x08000, 0xb9b27f3c )
	ROM_CONTINUE(             0x048000, 0x08000 )
	ROM_LOAD( "1115",         0x070000, 0x08000, 0xa7671dd8 )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "1116",         0x078000, 0x08000, 0x879fc7de )
	ROM_CONTINUE(             0x058000, 0x08000 )
	ROM_LOAD( "1101",         0x0a0000, 0x08000, 0x0ef13513 )	/* motion objects, planes 2/3 */
	ROM_CONTINUE(             0x080000, 0x08000 )
	ROM_LOAD( "1102",         0x0a8000, 0x08000, 0x401e06fd )
	ROM_CONTINUE(             0x088000, 0x08000 )
	ROM_LOAD( "1103",         0x0b0000, 0x08000, 0x50d820e8 )
	ROM_CONTINUE(             0x090000, 0x08000 )
	ROM_LOAD( "1104",         0x0b8000, 0x08000, 0x912d878f )
	ROM_CONTINUE(             0x098000, 0x08000 )
	ROM_LOAD( "1109",         0x0e0000, 0x08000, 0x6716a408 )
	ROM_CONTINUE(             0x0c0000, 0x08000 )
	ROM_LOAD( "1110",         0x0e8000, 0x08000, 0x7e184981 )
	ROM_CONTINUE(             0x0c8000, 0x08000 )
	ROM_LOAD( "1111",         0x0f0000, 0x08000, 0x353a14fd )
	ROM_CONTINUE(             0x0d0000, 0x08000 )
	ROM_LOAD( "1112",         0x0f8000, 0x08000, 0x3af7c50f )
	ROM_CONTINUE(             0x0d8000, 0x08000 )

	ROM_REGION( 0x4000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1125",         0x000000, 0x04000, 0x05a0341c )	/* alphanumerics */
ROM_END


ROM_START( apb2 )
	ROM_REGION( 0x90000, REGION_CPU1 )     /* 9 * 64k T11 code */
	ROM_LOAD_ODD ( "2126",         0x08000, 0x04000, 0x8edf4726 )
	ROM_LOAD_EVEN( "2127",         0x08000, 0x04000, 0xe2b2aff2 )
	ROM_LOAD_ODD ( "4128",         0x10000, 0x10000, 0x46009f6b )
	ROM_LOAD_EVEN( "4129",         0x10000, 0x10000, 0xe8ca47e2 )
	ROM_LOAD_ODD ( "1130",         0x30000, 0x10000, 0xf64c752e )
	ROM_LOAD_EVEN( "1131",         0x30000, 0x10000, 0x0a506e04 )
	ROM_LOAD_ODD ( "1132",         0x70000, 0x10000, 0x6d0e7a4e )
	ROM_LOAD_EVEN( "1133",         0x70000, 0x10000, 0xaf88d429 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for 6502 code */
	ROM_LOAD( "5134",         0x04000, 0x04000, 0x1c8bdeed )
	ROM_LOAD( "5135",         0x08000, 0x04000, 0xed6adb91 )
	ROM_LOAD( "5136",         0x0c000, 0x04000, 0x341f8486 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1118",         0x000000, 0x08000, 0x93752c49 ) 	/* playfield, planes 0/1 */
	ROM_LOAD( "1120",         0x028000, 0x08000, 0x043086f8 )
	ROM_CONTINUE(             0x008000, 0x08000 )
	ROM_LOAD( "1122",         0x030000, 0x08000, 0x5ee79481 )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "1124",         0x038000, 0x08000, 0x27760395 )
	ROM_CONTINUE(             0x018000, 0x08000 )
	ROM_LOAD( "1117",         0x040000, 0x08000, 0xcfc3f8a3 ) 	/* playfield, planes 2/3 */
	ROM_LOAD( "1119",         0x068000, 0x08000, 0x68850612 )
	ROM_CONTINUE(             0x048000, 0x08000 )
	ROM_LOAD( "1121",         0x070000, 0x08000, 0xc7977062 )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "1123",         0x078000, 0x08000, 0x3c96c848 )
	ROM_CONTINUE(             0x058000, 0x08000 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1105",         0x020000, 0x08000, 0x9b78a88e )	/* motion objects, planes 0/1 */
	ROM_CONTINUE(             0x000000, 0x08000 )
	ROM_LOAD( "1106",         0x028000, 0x08000, 0x4787ff58 )
	ROM_CONTINUE(             0x008000, 0x08000 )
	ROM_LOAD( "1107",         0x030000, 0x08000, 0x0e85f2ac )
	ROM_CONTINUE(             0x010000, 0x08000 )
	ROM_LOAD( "1108",         0x038000, 0x08000, 0x70ff9308 )
	ROM_CONTINUE(             0x018000, 0x08000 )
	ROM_LOAD( "1113",         0x060000, 0x08000, 0x4a445356 )
	ROM_CONTINUE(             0x040000, 0x08000 )
	ROM_LOAD( "1114",         0x068000, 0x08000, 0xb9b27f3c )
	ROM_CONTINUE(             0x048000, 0x08000 )
	ROM_LOAD( "1115",         0x070000, 0x08000, 0xa7671dd8 )
	ROM_CONTINUE(             0x050000, 0x08000 )
	ROM_LOAD( "1116",         0x078000, 0x08000, 0x879fc7de )
	ROM_CONTINUE(             0x058000, 0x08000 )
	ROM_LOAD( "1101",         0x0a0000, 0x08000, 0x0ef13513 )	/* motion objects, planes 2/3 */
	ROM_CONTINUE(             0x080000, 0x08000 )
	ROM_LOAD( "1102",         0x0a8000, 0x08000, 0x401e06fd )
	ROM_CONTINUE(             0x088000, 0x08000 )
	ROM_LOAD( "1103",         0x0b0000, 0x08000, 0x50d820e8 )
	ROM_CONTINUE(             0x090000, 0x08000 )
	ROM_LOAD( "1104",         0x0b8000, 0x08000, 0x912d878f )
	ROM_CONTINUE(             0x098000, 0x08000 )
	ROM_LOAD( "1109",         0x0e0000, 0x08000, 0x6716a408 )
	ROM_CONTINUE(             0x0c0000, 0x08000 )
	ROM_LOAD( "1110",         0x0e8000, 0x08000, 0x7e184981 )
	ROM_CONTINUE(             0x0c8000, 0x08000 )
	ROM_LOAD( "1111",         0x0f0000, 0x08000, 0x353a14fd )
	ROM_CONTINUE(             0x0d0000, 0x08000 )
	ROM_LOAD( "1112",         0x0f8000, 0x08000, 0x3af7c50f )
	ROM_CONTINUE(             0x0d8000, 0x08000 )

	ROM_REGION( 0x4000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "1125",         0x000000, 0x04000, 0x05a0341c )	/* alphanumerics */
ROM_END



/*************************************
 *
 *	Game driver(s)
 *
 *************************************/

GAME( 1984, paperboy, 0,   paperboy, paperboy, paperboy, ROT0,   "Atari Games", "Paperboy" )
GAME( 1986, 720,      0,   a720,     720,      a720,     ROT0,   "Atari Games", "720 Degrees (set 1)" )
GAME( 1986, 720b,     720, a720,     720,      a720,     ROT0,   "Atari Games", "720 Degrees (set 2)" )
GAME( 1986, ssprint,  0,   sprint,   ssprint,  ssprint,  ROT0,   "Atari Games", "Super Sprint" )
GAME( 1986, csprint,  0,   sprint,   csprint,  csprint,  ROT0,   "Atari Games", "Championship Sprint" )
GAME( 1987, apb,      0,   paperboy, apb,      apb,      ROT270, "Atari Games", "APB - All Points Bulletin (set 1)" )
GAME( 1987, apb2,     apb, paperboy, apb,      apb,      ROT270, "Atari Games", "APB - All Points Bulletin (set 2)" )

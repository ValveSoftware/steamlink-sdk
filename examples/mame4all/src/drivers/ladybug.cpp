#include "../vidhrdw/ladybug.cpp"

/***************************************************************************

Lady Bug memory map (preliminary)

driver by Nicola Salmoria

0000-5fff ROM
6000-6fff RAM
d000-d3ff video RAM
          d000-d007/d020-d027/d040-d047/d060-d067 contain the column scroll
          registers (not used by Lady Bug)
d400-d7ff color RAM (4 bits wide)

memory mapped ports:

read:
9000      IN0
9001      IN1
9002      DSW0
9003      DSW1
e000      IN2 (not used by Lady Bug)
8000      interrupt enable? (toggle)?
see the input_ports definition below for details on the input bits

write:
7000-73ff sprites
a000      flip screen
b000      sound port 1
c000      sound port 2

interrupts:
There is no vblank interrupt. The vblank status is read from IN1.
Coin insertion in left slot generates a NMI, in right slot an IRQ.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



READ_HANDLER( ladybug_IN0_r );
READ_HANDLER( ladybug_IN1_r );
int ladybug_interrupt(void);

void ladybug_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
WRITE_HANDLER( ladybug_flipscreen_w );
void ladybug_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x5fff, MRA_ROM },
	{ 0x6000, 0x6fff, MRA_RAM },
	{ 0x8000, 0x8fff, MRA_NOP },
	{ 0x9000, 0x9000, input_port_0_r },	/* IN0 */
	{ 0x9001, 0x9001, input_port_1_r },	/* IN1 */
	{ 0x9002, 0x9002, input_port_3_r },	/* DSW0 */
	{ 0x9003, 0x9003, input_port_4_r },	/* DSW1 */
	{ 0xd000, 0xd7ff, MRA_RAM },	/* video and color RAM */
	{ 0xe000, 0xe000, input_port_2_r },	/* IN2 */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x5fff, MWA_ROM },
	{ 0x6000, 0x6fff, MWA_RAM },
	{ 0x7000, 0x73ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0xa000, 0xa000, ladybug_flipscreen_w },
	{ 0xb000, 0xbfff, SN76496_0_w },
	{ 0xc000, 0xcfff, SN76496_1_w },
	{ 0xd000, 0xd3ff, videoram_w, &videoram, &videoram_size },
	{ 0xd400, 0xd7ff, colorram_w, &colorram },
	{ -1 }	/* end of table */
};



/***************************************************************************

  Lady Bug doesn't have VBlank interrupts.
  Interrupts are still used by the game: but they are related to coin
  slots. Left slot generates a NMI, Right slot an IRQ.

***************************************************************************/
int ladybug_interrupt(void)
{
	if (readinputport(5) & 1)	/* Left Coin */
		return nmi_interrupt();
	else if (readinputport(5) & 2)	/* Right Coin */
		return interrupt();
	else return ignore_interrupt();
}

INPUT_PORTS_START( ladybug )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_TILT )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	/* This should be connected to the 4V clock. I don't think the game uses it. */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	/* Note that there are TWO VBlank inputs, one is active low, the other active */
	/* high. There are probably other differencies in the hardware, but emulating */
	/* them this way is enough to get the game running. */
	PORT_BIT( 0xc0, 0x40, IPT_VBLANK )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_UNUSED )
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, "High Score Names" )
	PORT_DIPSETTING(    0x00, "3 Letters" )
	PORT_DIPSETTING(    0x04, "10 Letters" )
	PORT_BITX(    0x08, 0x08, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Rack Test", KEYCODE_F1, IP_JOY_NONE )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Freeze" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x00, "5" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0f, "Right Coin" )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	/* settings 0x00 thru 0x05 all give 1 Coin/1 Credit */
	PORT_DIPNAME( 0xf0, 0xf0, "Left Coin" )
	PORT_DIPSETTING(    0x60, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	/* settings 0x00 thru 0x50 all give 1 Coin/1 Credit */

	PORT_START	/* FAKE */
	/* The coin slots are not memory mapped. Coin Left causes a NMI, */
	/* Coin Right an IRQ. This fake input port is used by the interrupt */
	/* handler to be notified of coin insertions. We use IMPULSE to */
	/* trigger exactly one interrupt, without having to check when the */
	/* user releases the key. */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
INPUT_PORTS_END

INPUT_PORTS_START( snapjack )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_TILT )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	/* This should be connected to the 4V clock. I don't think the game uses it. */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	/* Note that there are TWO VBlank inputs, one is active low, the other active */
	/* high. There are probably other differencies in the hardware, but emulating */
	/* them this way is enough to get the game running. */
	PORT_BIT( 0xc0, 0x40, IPT_VBLANK )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_UNUSED  )
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, "High Score Names" )
	PORT_DIPSETTING(    0x00, "3 Letters" )
	PORT_DIPSETTING(    0x04, "10 Letters" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x10, 0x00, "unused1?" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, "unused2?" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x40, "5" )

	PORT_START	/* DSW1 */
	/* coinage is slightly different from Lady Bug and Cosmic Avenger */
	PORT_DIPNAME( 0x0f, 0x0f, "Right Coin" )
	PORT_DIPSETTING(    0x05, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	/* settings 0x00 thru 0x04 all give 1 Coin/1 Credit */
	PORT_DIPNAME( 0xf0, 0xf0, "Left Coin" )
	PORT_DIPSETTING(    0x50, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	/* settings 0x00 thru 0x04 all give 1 Coin/1 Credit */

	PORT_START	/* FAKE */
	/* The coin slots are not memory mapped. Coin Left causes a NMI, */
	/* Coin Right an IRQ. This fake input port is used by the interrupt */
	/* handler to be notified of coin insertions. We use IMPULSE to */
	/* trigger exactly one interrupt, without having to check when the */
	/* user releases the key. */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
INPUT_PORTS_END

INPUT_PORTS_START( cavenger )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_TILT )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	/* This should be connected to the 4V clock. I don't think the game uses it. */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	/* Note that there are TWO VBlank inputs, one is active low, the other active */
	/* high. There are probably other differencies in the hardware, but emulating */
	/* them this way is enough to get the game running. */
	PORT_BIT( 0xc0, 0x40, IPT_VBLANK )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0e, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "Easy" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, "High Score Names" )
	PORT_DIPSETTING(    0x00, "3 Letters" )
	PORT_DIPSETTING(    0x04, "10 Letters" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x30, 0x00, "Initial High Score" )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x30, "5000" )
	PORT_DIPSETTING(    0x20, "8000" )
	PORT_DIPSETTING(    0x10, "10000" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x40, "5" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0f, "Right Coin" )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	/* settings 0x00 thru 0x05 all give 1 Coin/1 Credit */
	PORT_DIPNAME( 0xf0, 0xf0, "Left Coin" )
	PORT_DIPSETTING(    0x60, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	/* settings 0x00 thru 0x50 all give 1 Coin/1 Credit */

	PORT_START	/* FAKE */
	/* The coin slots are not memory mapped. Coin Left causes a NMI, */
	/* Coin Right an IRQ. This fake input port is used by the interrupt */
	/* handler to be notified of coin insertions. We use IMPULSE to */
	/* trigger exactly one interrupt, without having to check when the */
	/* user releases the key. */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	/* 2 bits per pixel */
	{ 0, 512*8*8 },	/* the two bitplanes are separated */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	128,	/* 128 sprites */
	2,	/* 2 bits per pixel */
	{ 1, 0 },	/* the two bitplanes are packed in two consecutive bits */
	{ 0, 2, 4, 6, 8, 10, 12, 14,
			8*16+0, 8*16+2, 8*16+4, 8*16+6, 8*16+8, 8*16+10, 8*16+12, 8*16+14 },
	{ 23*16, 22*16, 21*16, 20*16, 19*16, 18*16, 17*16, 16*16,
			7*16, 6*16, 5*16, 4*16, 3*16, 2*16, 1*16, 0*16 },
	64*8	/* every sprite takes 64 consecutive bytes */
};
static struct GfxLayout spritelayout2 =
{
	8,8,	/* 8*8 sprites */
	512,	/* 512 sprites */
	2,	/* 2 bits per pixel */
	{ 1, 0 },	/* the two bitplanes are packed in two consecutive bits */
	{ 0, 2, 4, 6, 8, 10, 12, 14 },
	{ 7*16, 6*16, 5*16, 4*16, 3*16, 2*16, 1*16, 0*16 },
	16*8	/* every sprite takes 16 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,      0,  8 },
	{ REGION_GFX2, 0, &spritelayout,  4*8, 16 },
	{ REGION_GFX2, 0, &spritelayout2, 4*8, 16 },
	{ -1 } /* end of array */
};


static struct SN76496interface sn76496_interface =
{
	2,	/* 2 chips */
	{ 4000000, 4000000 },	/* 4 MHz */
	{ 100, 100 }
};



static struct MachineDriver machine_driver_ladybug =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			4000000,	/* 4 Mhz */
			readmem,writemem,0,0,
			ladybug_interrupt,1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 4*8, 28*8-1 },
	gfxdecodeinfo,
	32,4*24,
	ladybug_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	ladybug_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76496,
			&sn76496_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( ladybug )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "lb1.cpu",      0x0000, 0x1000, 0xd09e0adb )
	ROM_LOAD( "lb2.cpu",      0x1000, 0x1000, 0x88bc4a0a )
	ROM_LOAD( "lb3.cpu",      0x2000, 0x1000, 0x53e9efce )
	ROM_LOAD( "lb4.cpu",      0x3000, 0x1000, 0xffc424d7 )
	ROM_LOAD( "lb5.cpu",      0x4000, 0x1000, 0xad6af809 )
	ROM_LOAD( "lb6.cpu",      0x5000, 0x1000, 0xcf1acca4 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lb9.vid",      0x0000, 0x1000, 0x77b1da1e )
	ROM_LOAD( "lb10.vid",     0x1000, 0x1000, 0xaa82e00b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lb8.cpu",      0x0000, 0x1000, 0x8b99910b )
	ROM_LOAD( "lb7.cpu",      0x1000, 0x1000, 0x86a5b448 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "10-2.vid",     0x0000, 0x0020, 0xdf091e52 ) /* palette */
	ROM_LOAD( "10-1.vid",     0x0020, 0x0020, 0x40640d8f ) /* sprite color lookup table */
	ROM_LOAD( "10-3.vid",     0x0040, 0x0020, 0x27fa3a50 ) /* ?? */
ROM_END

ROM_START( ladybugb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "lb1a.cpu",     0x0000, 0x1000, 0xec135e54 )
	ROM_LOAD( "lb2a.cpu",     0x1000, 0x1000, 0x3049c5c6 )
	ROM_LOAD( "lb3a.cpu",     0x2000, 0x1000, 0xb0fef837 )
	ROM_LOAD( "lb4.cpu",      0x3000, 0x1000, 0xffc424d7 )
	ROM_LOAD( "lb5.cpu",      0x4000, 0x1000, 0xad6af809 )
	ROM_LOAD( "lb6a.cpu",     0x5000, 0x1000, 0x88c8002a )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lb9.vid",      0x0000, 0x1000, 0x77b1da1e )
	ROM_LOAD( "lb10.vid",     0x1000, 0x1000, 0xaa82e00b )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "lb8.cpu",      0x0000, 0x1000, 0x8b99910b )
	ROM_LOAD( "lb7.cpu",      0x1000, 0x1000, 0x86a5b448 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "10-2.vid",     0x0000, 0x0020, 0xdf091e52 ) /* palette */
	ROM_LOAD( "10-1.vid",     0x0020, 0x0020, 0x40640d8f ) /* sprite color lookup table */
	ROM_LOAD( "10-3.vid",     0x0040, 0x0020, 0x27fa3a50 ) /* ?? */
ROM_END

ROM_START( snapjack )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "sj2a.bin",     0x0000, 0x1000, 0x6b30fcda )
	ROM_LOAD( "sj2b.bin",     0x1000, 0x1000, 0x1f1088d1 )
	ROM_LOAD( "sj2c.bin",     0x2000, 0x1000, 0xedd65f3a )
	ROM_LOAD( "sj2d.bin",     0x3000, 0x1000, 0xf4481192 )
	ROM_LOAD( "sj2e.bin",     0x4000, 0x1000, 0x1bff7d05 )
	ROM_LOAD( "sj2f.bin",     0x5000, 0x1000, 0x21793edf )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sj2i.bin",     0x0000, 0x1000, 0xff2011c7 )
	ROM_LOAD( "sj2j.bin",     0x1000, 0x1000, 0xf097babb )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "sj2h.bin",     0x0000, 0x1000, 0xb7f105b6 )
	ROM_LOAD( "sj2g.bin",     0x1000, 0x1000, 0x1cdb03a8 )

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "sj8t.bin",     0x0000, 0x0020, 0xcbbd9dd1 ) /* palette */
	ROM_LOAD( "sj9k.bin",     0x0020, 0x0020, 0x5b16fbd2 ) /* sprite color lookup table */
	ROM_LOAD( "sj9h.bin",     0x0040, 0x0020, 0x27fa3a50 ) /* ?? */
ROM_END

ROM_START( cavenger )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "1",            0x0000, 0x1000, 0x9e0cc781 )
	ROM_LOAD( "2",            0x1000, 0x1000, 0x5ce5b950 )
	ROM_LOAD( "3",            0x2000, 0x1000, 0xbc28218d )
	ROM_LOAD( "4",            0x3000, 0x1000, 0x2b32e9f5 )
	ROM_LOAD( "5",            0x4000, 0x1000, 0xd117153e )
	ROM_LOAD( "6",            0x5000, 0x1000, 0xc7d366cb )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9",            0x0000, 0x1000, 0x63357785 )
	ROM_LOAD( "0",            0x1000, 0x1000, 0x52ad1133 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "8",            0x0000, 0x1000, 0xb022bf2d )
	/* 1000-1fff empty */

	ROM_REGION( 0x0060, REGION_PROMS )
	ROM_LOAD( "t8.bpr",       0x0000, 0x0020, 0x42a24dd5 ) /* palette */
	ROM_LOAD( "k9.bpr",       0x0020, 0x0020, 0xd736b8de ) /* sprite color lookup table */
	ROM_LOAD( "h9.bpr",       0x0040, 0x0020, 0x27fa3a50 ) /* ?? */
ROM_END



GAME( 1981, ladybug,  0,       ladybug, ladybug,  0, ROT270, "Universal", "Lady Bug" )
GAME( 1982, ladybugb, ladybug, ladybug, ladybug,  0, ROT270, "bootleg", "Lady Bug (bootleg)" )
GAME( 1981?,snapjack, 0,       ladybug, snapjack, 0, ROT0,   "Universal", "Snap Jack" )
GAME( 1981, cavenger, 0,       ladybug, cavenger, 0, ROT0,   "Universal", "Cosmic Avenger" )

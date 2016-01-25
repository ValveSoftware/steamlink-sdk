#include "../vidhrdw/ddragon3.cpp"

/******************************************************************

	Double Dragon 3						Technos Japan Corp 1990
	The Combatribes						Technos Japan Corp 1990


	Notes:

	Both games have original and bootleg versions supported.
	Double Dragon 3 bootleg has some misplaced graphics, but I
	think this is how the real thing would look.
	Double Dragon 3 original cut scenes seem to fade a bit fast?
	Combatribes has sprite lag but it seems to be caused by poor
	programming and I think the original does the same.

******************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "cpu/m68000/m68000.h"
#include "vidhrdw/generic.h"

void ddragon3_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void ctribe_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( ddragon3_scroll_w );

extern int ddragon3_vh_start(void);

extern unsigned char *ddragon3_bg_videoram;
WRITE_HANDLER( ddragon3_bg_videoram_w );
READ_HANDLER( ddragon3_bg_videoram_r );

extern unsigned char *ddragon3_fg_videoram;
WRITE_HANDLER( ddragon3_fg_videoram_w );
READ_HANDLER( ddragon3_fg_videoram_r );

/***************************************************************************/

static WRITE_HANDLER( oki_bankswitch_w )
{
	OKIM6295_set_bank_base(0, ALL_VOICES, (data & 1) * 0x40000);
}

static READ_HANDLER( ddrago3b_io_r )
{
	switch (offset) {
		case 0x0: return readinputport(0) + 256*((readinputport(3)&0x0f)|((readinputport(4)&0xc0)<<2));
		case 0x2: return readinputport(1) + 256*(readinputport(4)&0x3f);
		case 0x4: return readinputport(2) + 256*(readinputport(5)&0x3f);
		case 0x6: return (readinputport(5)&0xc0)<<2;
	}
	return 0xffff;
}

static READ_HANDLER( ctribe_io_r ){
	switch (offset) {
		case 0x0: return readinputport(0) + 256*((readinputport(3)&0x0f)|((readinputport(4)&0xc0)<<2));
		case 0x2: return readinputport(1) + 256*(readinputport(4)&0x3f);
		case 0x4: return readinputport(2) + 256*(readinputport(5)&0x3f);
		case 0x6: return 256*(readinputport(5)&0xc0);
	}
	return 0xffff;
}

static READ_HANDLER( ddragon3_io_r )
{
	switch (offset) {
		case 0x0: return readinputport(0);
		case 0x2: return readinputport(1);
		case 0x4: return readinputport(2);
		case 0x6: return readinputport(3);

		default:
		//logerror("INPUT 1800[%02x] \n", offset);
		return 0xffff;
	}
}

extern UINT16 ddragon3_vreg;

static int ddragon3_cpu_interrupt(void) { /* 6:0x177e - 5:0x176a */
	if( cpu_getiloops() == 0 ){
		return MC68000_IRQ_6;  /* VBlank */
	}
	else {
		return MC68000_IRQ_5; /* Input Ports */
	}
	return MC68000_INT_NONE;
}

static UINT16 reg[8];

static WRITE_HANDLER( ddragon3_io_w ){
	reg[offset/2] = COMBINE_WORD(reg[offset],data);

	switch (offset) {
		case 0x0:
		ddragon3_vreg = reg[0];
		break;

		case 0x2: /* soundlatch_w */
		soundlatch_w(1,reg[1]&0xff);
		cpu_cause_interrupt( 1, Z80_NMI_INT );
		break;

		case 0x4:
		/*	this gets written to on startup and at the end of IRQ6
		**	possibly trigger IRQ on sound CPU
		*/
		break;

		case 0x6:
		/*	this gets written to on startup,
		**	and at the end of IRQ5 (input port read) */
		break;

		case 0x8:
		/* this gets written to at the end of IRQ6 only */
		break;

		default:
		//logerror("OUTPUT 1400[%02x] %08x, pc=%06x \n", offset,(unsigned)data, cpu_get_pc() );
		break;
	}
}

/**************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM   },
	{ 0x080000, 0x080fff, ddragon3_fg_videoram_r }, /* Foreground (32x32 Tiles - 4 by per tile) */
	{ 0x082000, 0x0827ff, ddragon3_bg_videoram_r }, /* Background (32x32 Tiles - 2 by per tile) */
	{ 0x100000, 0x100007, ddragon3_io_r },
	{ 0x140000, 0x1405ff, paletteram_word_r },
	{ 0x180000, 0x180fff, MRA_BANK1 },
	{ 0x1c0000, 0x1c3fff, MRA_BANK2 }, /* working RAM */
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x080000, 0x080fff, ddragon3_fg_videoram_w, &ddragon3_fg_videoram },
	{ 0x082000, 0x0827ff, ddragon3_bg_videoram_w, &ddragon3_bg_videoram },
	{ 0x0c0000, 0x0c000f, ddragon3_scroll_w },
	{ 0x100000, 0x10000f, ddragon3_io_w },
	{ 0x140000, 0x1405ff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram},
	{ 0x180000, 0x180fff, MWA_BANK1, &spriteram }, /* Sprites (16 bytes per sprite) */
	{ 0x1c0000, 0x1c3fff, MWA_BANK2 },
	{ -1 }
};

static struct MemoryReadAddress dd3b_readmem[] = {
	{ 0x000000, 0x07ffff, MRA_ROM   },
	{ 0x080000, 0x080fff, ddragon3_fg_videoram_r }, /* Foreground (32x32 Tiles - 4 by per tile) */
	{ 0x081000, 0x081fff, MRA_BANK1 },
	{ 0x082000, 0x0827ff, ddragon3_bg_videoram_r }, /* Background (32x32 Tiles - 2 by per tile) */
	{ 0x100000, 0x1005ff, paletteram_word_r },
	{ 0x180000, 0x180007, ddrago3b_io_r },
	{ 0x1c0000, 0x1c3fff, MRA_BANK2 }, /* working RAM */
	{ -1 }
};

static struct MemoryWriteAddress dd3b_writemem[] = {
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x080000, 0x080fff, ddragon3_fg_videoram_w, &ddragon3_fg_videoram },
	{ 0x081000, 0x081fff, MWA_BANK1, &spriteram }, /* Sprites (16 bytes per sprite) */
	{ 0x082000, 0x0827ff, ddragon3_bg_videoram_w, &ddragon3_bg_videoram },
	{ 0x0c0000, 0x0c000f, ddragon3_scroll_w },
	{ 0x100000, 0x1005ff, paletteram_xBBBBBGGGGGRRRRR_word_w, &paletteram},
	{ 0x140000, 0x14000f, ddragon3_io_w },
	{ 0x1c0000, 0x1c3fff, MWA_BANK2 },
	{ -1 }
};

static struct MemoryReadAddress ctribe_readmem[] = {
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x080000, 0x080fff, ddragon3_fg_videoram_r }, /* Foreground (32x32 Tiles - 4 by per tile) */
	{ 0x081000, 0x081fff, MRA_BANK1 },
	{ 0x082000, 0x0827ff, ddragon3_bg_videoram_r }, /* Background (32x32 Tiles - 2 by per tile) */
	{ 0x100000, 0x1005ff, paletteram_word_r },
	{ 0x180000, 0x180007, ctribe_io_r },
	{ 0x1c0000, 0x1c3fff, MRA_BANK2 }, /* working RAM */
	{ -1 }
};

static struct MemoryWriteAddress ctribe_writemem[] = {
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x080000, 0x080fff, ddragon3_fg_videoram_w, &ddragon3_fg_videoram },
	{ 0x081000, 0x081fff, MWA_BANK1, &spriteram }, /* Sprites (16 bytes per sprite) */
	{ 0x082000, 0x0827ff, ddragon3_bg_videoram_w, &ddragon3_bg_videoram },
	{ 0x0c0000, 0x0c000f, ddragon3_scroll_w },
	{ 0x100000, 0x1005ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram},
	{ 0x140000, 0x14000f, ddragon3_io_w },
	{ 0x1c0000, 0x1c3fff, MWA_BANK2 },
	{ -1 }
};

/**************************************************************************/

static struct MemoryReadAddress readmem_sound[] = {
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xc801, 0xc801, YM2151_status_port_0_r },
	{ 0xd800, 0xd800, OKIM6295_status_0_r },
	{ 0xe000, 0xe000, soundlatch_r },
	{ -1 }
};

static struct MemoryWriteAddress writemem_sound[] = {
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xc800, 0xc800, YM2151_register_port_0_w },
	{ 0xc801, 0xc801, YM2151_data_port_0_w },
	{ 0xd800, 0xd800, OKIM6295_data_0_w },
	{ 0xe800, 0xe800, oki_bankswitch_w },
	{ -1 }
};

static struct MemoryReadAddress ctribe_readmem_sound[] = {
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x87ff, MRA_RAM },
	{ 0x8801, 0x8801, YM2151_status_port_0_r },
	{ 0x9800, 0x9800, OKIM6295_status_0_r },
	{ 0xa000, 0xa000, soundlatch_r },
	{ -1 }
};

static struct MemoryWriteAddress ctribe_writemem_sound[] = {
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8800, YM2151_register_port_0_w },
	{ 0x8801, 0x8801, YM2151_data_port_0_w },
	{ 0x9800, 0x9800, OKIM6295_data_0_w },
	{ -1 }
};

/***************************************************************************/

INPUT_PORTS_START( ddrago3b )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_COIN4 )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Continue Discount" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START /* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR(Difficulty) )
	PORT_DIPSETTING(	0x02, "Easy" )
	PORT_DIPSETTING(	0x03, "Normal" )
	PORT_DIPSETTING(	0x01, "Hard" )
	PORT_DIPSETTING(	0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, "P1 hurt P2" )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) ) /* timer speed? */
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Test Mode" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Stage Clear Power" )
	PORT_DIPSETTING(	0x20, "0" )
	PORT_DIPSETTING(	0x00, "50" )
	PORT_DIPNAME( 0x40, 0x40, "Starting Power" )
	PORT_DIPSETTING(	0x00, "200" )
	PORT_DIPSETTING(	0x40, "230" )
	PORT_DIPNAME( 0x80, 0x80, "Simultaneous Players" )
	PORT_DIPSETTING(	0x80, "2" )
	PORT_DIPSETTING(	0x00, "3" )
INPUT_PORTS_END

INPUT_PORTS_START( ddragon3 )
	PORT_START /* 180000 (P1 Controls) */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START1 )

	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON3 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* 180002 */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START /* 180004 (P3 Controls) */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_START3 )

	/* DSWA */
	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Coinage ) )
	PORT_DIPSETTING(      0x0000, "A" )
	PORT_DIPSETTING(      0x0100, "B" )
	PORT_DIPSETTING(      0x0200, "C" )
	PORT_DIPSETTING(      0x0300, "D" )
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0800, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, "Coin Statistics" )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, "Invert Screen" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_START /* 180006 DSW */
	PORT_DIPNAME( 0x0100, 0x0100, "Starting Power" )
	PORT_DIPSETTING(      0x0000, "200" )
	PORT_DIPSETTING(      0x0100, "230" )
	PORT_DIPNAME( 0x0200, 0x0200, "Simultaneous Players" )
	PORT_DIPSETTING(      0x0200, "2" )
	PORT_DIPSETTING(      0x0000, "3" )
	PORT_DIPNAME( 0x0400, 0x0400, "Stage Clear Power" )
	PORT_DIPSETTING(      0x0400, "50" )
	PORT_DIPSETTING(      0x0000, "0" )
	PORT_DIPNAME( 0x0800, 0x0800, "Test Mode?" )
	PORT_DIPSETTING(      0x0800, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, "P1 hurt P2" )
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0xc000, 0xc000, DEF_STR(Difficulty) )
	PORT_DIPSETTING(      0x4000, "Easy" )
	PORT_DIPSETTING(      0xc000, "Normal" )
	PORT_DIPSETTING(      0x8000, "Hard" )
	PORT_DIPSETTING(      0x0000, "Hardest" )
INPUT_PORTS_END

INPUT_PORTS_START( ctribe )
	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPF_PLAYER2 | IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPF_PLAYER3 | IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START3 )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START /* DSW1 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(	0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Continue Discount" )
	PORT_DIPSETTING(	0x10, DEF_STR( No ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40,	0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80,	0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(	0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )

	PORT_START /* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR(Difficulty) )
	PORT_DIPSETTING(	0x02, "Easy" )
	PORT_DIPSETTING(	0x03, "Normal" )
	PORT_DIPSETTING(	0x01, "Hard" )
	PORT_DIPSETTING(	0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, "Timer Speed" )
	PORT_DIPSETTING(	0x04, "Normal" )
	PORT_DIPSETTING(	0x00, "Fast" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(	0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Test Mode" )
	PORT_DIPSETTING(	0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x60, 0x60, "Stage Clear Power" )
	PORT_DIPSETTING(	0x60, "0" )
	PORT_DIPSETTING(	0x40, "50" )
	PORT_DIPSETTING(	0x20, "100" )
	PORT_DIPSETTING(	0x00, "150" )
	PORT_DIPNAME( 0x80, 0x80, "Simultaneous Players" )
	PORT_DIPSETTING(	0x80, "2" )
	PORT_DIPSETTING(	0x00, "3" )
INPUT_PORTS_END

/***************************************************************************/

static struct GfxLayout tile_layout =
{
	16,16,	/* 16*16 tiles */
	8192,	/* 8192 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 0x40000*8, 2*0x40000*8 , 3*0x40000*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every tile takes 32 consecutive bytes */
};

static struct GfxLayout sprite_layout = {
	16,16,	/* 16*16 tiles */
	0x90000/32,	/* 4096 tiles */
	4,	/* 4 bits per pixel */
	{ 0, 0x100000*8, 2*0x100000*8 , 3*0x100000*8 },	/* the bitplanes are separated */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
		16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every tile takes 32 consecutive bytes */
};

static struct GfxDecodeInfo ddragon3_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tile_layout,	  256, 32 },
	{ REGION_GFX2, 0, &sprite_layout,		0, 16 },
	{ -1 }
};

/***************************************************************************/

static void dd3_ymirq_handler(int irq)
{
	cpu_set_irq_line( 1, 0 , irq ? ASSERT_LINE : CLEAR_LINE );
}

static struct YM2151interface ym2151_interface =
{
	1,			/* 1 chip */
	3579545,	/* Guess */
	{ YM3012_VOL(45,MIXER_PAN_LEFT,45,MIXER_PAN_RIGHT) },
	{ dd3_ymirq_handler }
};

static struct OKIM6295interface okim6295_interface =
{
	1,              /* 1 chip */
	{ 8500 },       /* frequency (Hz) */
	{ REGION_SOUND1 },	/* memory region */
	{ 47 }
};

/**************************************************************************/

static struct MachineDriver machine_driver_ddragon3 =
{
	{
		{
			CPU_M68000,
			12000000, /* Guess */
			readmem,writemem,0,0,
			ddragon3_cpu_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* Guess */
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,0
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,	/* CPU slices per frame */
	0, /* init machine */

	/* video hardware */
	320, 240, { 0, 319, 8, 239 },

	ddragon3_gfxdecodeinfo,
	768,768,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ddragon3_vh_start,
	0,
	ddragon3_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_ddrago3b =
{
	{
		{
			CPU_M68000,
			12000000, /* Guess */
			dd3b_readmem,dd3b_writemem,0,0,
			ddragon3_cpu_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* Guess */
			readmem_sound,writemem_sound,0,0,
			ignore_interrupt,0
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,	/* CPU slices per frame */
	0, /* init machine */

	/* video hardware */
	320, 240, { 0, 319, 8, 239 },

	ddragon3_gfxdecodeinfo,
	768,768,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ddragon3_vh_start,
	0,
	ddragon3_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver machine_driver_ctribe =
{
	{
		{
			CPU_M68000,
			12000000, /* Guess */
			ctribe_readmem,ctribe_writemem,0,0,
			ddragon3_cpu_interrupt,2
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,	/* Guess */
			ctribe_readmem_sound,ctribe_writemem_sound,0,0,
			ignore_interrupt,0
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* CPU slices per frame */
	0, /* init machine */

	/* video hardware */
	320, 240, { 0, 319, 8, 239 },

	ddragon3_gfxdecodeinfo,
	768,768,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ddragon3_vh_start,
	0,
	ctribe_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

/**************************************************************************/

ROM_START( ddragon3 )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 64k for cpu code */
	ROM_LOAD_ODD ( "30a14" ,  0x00000, 0x40000, 0xf42fe016 )
	ROM_LOAD_EVEN( "30a15" ,  0x00000, 0x20000, 0xad50e92c )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu code */
	ROM_LOAD( "dd3.06" ,   0x00000, 0x10000, 0x1e974d9b )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dd3.f" ,  0x000000, 0x40000, 0x89d58d32 ) /* Background */
	ROM_LOAD( "dd3.e" ,  0x040000, 0x40000, 0x9bf1538e )
	ROM_LOAD( "dd3.b" ,  0x080000, 0x40000, 0x8f671a62 )
	ROM_LOAD( "dd3.a" ,  0x0c0000, 0x40000, 0x0f74ea1c )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	/* sprites  */
	ROM_LOAD( "dd3.3e" ,  0x000000, 0x20000, 0x726c49b7 ) //4a
	ROM_LOAD( "dd3.3d" ,  0x020000, 0x20000, 0x37a1c335 ) //3a
	ROM_LOAD( "dd3.3c" ,  0x040000, 0x20000, 0x2bcfe63c ) //2a
	ROM_LOAD( "dd3.3b" ,  0x060000, 0x20000, 0xb864cf17 ) //1a
	ROM_LOAD( "dd3.3a" ,  0x080000, 0x10000, 0x20d64bea ) //5a

	ROM_LOAD( "dd3.2e" ,  0x100000, 0x20000, 0x8c71eb06 ) //4b
	ROM_LOAD( "dd3.2d" ,  0x120000, 0x20000, 0x3e134be9 ) //3b
	ROM_LOAD( "dd3.2c" ,  0x140000, 0x20000, 0xb4115ef0 ) //2b
	ROM_LOAD( "dd3.2b" ,  0x160000, 0x20000, 0x4639333d ) //1b
	ROM_LOAD( "dd3.2a" ,  0x180000, 0x10000, 0x785d71b0 ) //5b

	ROM_LOAD( "dd3.1e" ,  0x200000, 0x20000, 0x04420cc8 ) //4c
	ROM_LOAD( "dd3.1d" ,  0x220000, 0x20000, 0x33f97b2f ) //3c
	ROM_LOAD( "dd3.1c" ,  0x240000, 0x20000, 0x0f9a8f2a ) //2c
	ROM_LOAD( "dd3.1b" ,  0x260000, 0x20000, 0x15c91772 ) //1c
	ROM_LOAD( "dd3.1a" ,  0x280000, 0x10000, 0x15e43d12 ) //5c

	ROM_LOAD( "dd3.0e" ,  0x300000, 0x20000, 0x894734b3 ) //4d
	ROM_LOAD( "dd3.0d" ,  0x320000, 0x20000, 0xcd504584 ) //3d
	ROM_LOAD( "dd3.0c" ,  0x340000, 0x20000, 0x38e8a9ad ) //2d
	ROM_LOAD( "dd3.0b" ,  0x360000, 0x20000, 0x80c1cb74 ) //1d
	ROM_LOAD( "dd3.0a" ,  0x380000, 0x10000, 0x5a47e7a4 ) //5d

	ROM_REGION( 0x080000, REGION_SOUND1 )	/* ADPCM Samples */
	ROM_LOAD( "dd3.j7" ,  0x000000, 0x40000, 0x3af21dbe )
	ROM_LOAD( "dd3.j8" ,  0x040000, 0x40000, 0xc28b53cd )
ROM_END

ROM_START( ddrago3b )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 64k for cpu code */
	ROM_LOAD_ODD ( "dd3.01" ,  0x00000, 0x20000, 0x68321d8b )
	ROM_LOAD_EVEN( "dd3.03" ,  0x00000, 0x20000, 0xbc05763b )
	ROM_LOAD_ODD ( "dd3.02" ,  0x40000, 0x20000, 0x38d9ae75 )
	/* No EVEN rom! */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu code */
	ROM_LOAD( "dd3.06" ,   0x00000, 0x10000, 0x1e974d9b )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	/* Background */
	ROM_LOAD( "dd3.f" ,  0x000000, 0x40000, 0x89d58d32 )
	ROM_LOAD( "dd3.e" ,  0x040000, 0x40000, 0x9bf1538e )
	ROM_LOAD( "dd3.b" ,  0x080000, 0x40000, 0x8f671a62 )
	ROM_LOAD( "dd3.a" ,  0x0c0000, 0x40000, 0x0f74ea1c )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	/* sprites  */
	ROM_LOAD( "dd3.3e" ,  0x000000, 0x20000, 0x726c49b7 ) //4a
	ROM_LOAD( "dd3.3d" ,  0x020000, 0x20000, 0x37a1c335 ) //3a
	ROM_LOAD( "dd3.3c" ,  0x040000, 0x20000, 0x2bcfe63c ) //2a
	ROM_LOAD( "dd3.3b" ,  0x060000, 0x20000, 0xb864cf17 ) //1a
	ROM_LOAD( "dd3.3a" ,  0x080000, 0x10000, 0x20d64bea ) //5a

	ROM_LOAD( "dd3.2e" ,  0x100000, 0x20000, 0x8c71eb06 ) //4b
	ROM_LOAD( "dd3.2d" ,  0x120000, 0x20000, 0x3e134be9 ) //3b
	ROM_LOAD( "dd3.2c" ,  0x140000, 0x20000, 0xb4115ef0 ) //2b
	ROM_LOAD( "dd3.2b" ,  0x160000, 0x20000, 0x4639333d ) //1b
	ROM_LOAD( "dd3.2a" ,  0x180000, 0x10000, 0x785d71b0 ) //5b

	ROM_LOAD( "dd3.1e" ,  0x200000, 0x20000, 0x04420cc8 ) //4c
	ROM_LOAD( "dd3.1d" ,  0x220000, 0x20000, 0x33f97b2f ) //3c
	ROM_LOAD( "dd3.1c" ,  0x240000, 0x20000, 0x0f9a8f2a ) //2c
	ROM_LOAD( "dd3.1b" ,  0x260000, 0x20000, 0x15c91772 ) //1c
	ROM_LOAD( "dd3.1a" ,  0x280000, 0x10000, 0x15e43d12 ) //5c

	ROM_LOAD( "dd3.0e" ,  0x300000, 0x20000, 0x894734b3 ) //4d
	ROM_LOAD( "dd3.0d" ,  0x320000, 0x20000, 0xcd504584 ) //3d
	ROM_LOAD( "dd3.0c" ,  0x340000, 0x20000, 0x38e8a9ad ) //2d
	ROM_LOAD( "dd3.0b" ,  0x360000, 0x20000, 0x80c1cb74 ) //1d
	ROM_LOAD( "dd3.0a" ,  0x380000, 0x10000, 0x5a47e7a4 ) //5d

	ROM_REGION( 0x080000, REGION_SOUND1 )	/* ADPCM Samples */
	ROM_LOAD( "dd3.j7" ,  0x000000, 0x40000, 0x3af21dbe )
	ROM_LOAD( "dd3.j8" ,  0x040000, 0x40000, 0xc28b53cd )
ROM_END

ROM_START( ctribe )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 64k for cpu code */
	ROM_LOAD_ODD ( "ic-26",      0x00000, 0x20000, 0xc46b2e63 )
	ROM_LOAD_EVEN( "ic-25",      0x00000, 0x20000, 0x3221c755 )
	ROM_LOAD_ODD ( "ct_ep2.rom", 0x40000, 0x10000, 0x8c2c6dbd )
	/* No EVEN rom! */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu code */
	ROM_LOAD( "ct_ep4.rom",   0x00000, 0x8000, 0x4346de13 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ct_mr7.rom",  0x000000, 0x40000, 0xa8b773f1 ) 	/* Background */
	ROM_LOAD( "ct_mr6.rom",  0x040000, 0x40000, 0x617530fc )
	ROM_LOAD( "ct_mr5.rom",  0x080000, 0x40000, 0xcef0a821 )
	ROM_LOAD( "ct_mr4.rom",  0x0c0000, 0x40000, 0xb84fda09 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ct_mr3.rom",  0x000000, 0x80000, 0x1ac2a461 ) 	/* Sprites */
	ROM_LOAD( "ct_ep5.rom",  0x080000, 0x10000, 0x972faddb )
	ROM_LOAD( "ct_mr2.rom",  0x100000, 0x80000, 0x8c796707 )
	ROM_LOAD( "ct_ep6.rom",  0x180000, 0x10000, 0xeb3ab374 )
	ROM_LOAD( "ct_mr1.rom",  0x200000, 0x80000, 0x1c9badbd )
	ROM_LOAD( "ct_ep7.rom",  0x280000, 0x10000, 0xc602ac97 )
	ROM_LOAD( "ct_mr0.rom",  0x300000, 0x80000, 0xba73c49e )
	ROM_LOAD( "ct_ep8.rom",  0x380000, 0x10000, 0x4da1d8e5 )

	ROM_REGION( 0x040000, REGION_SOUND1 )	/* ADPCM Samples */
	ROM_LOAD( "ct_mr8.rom" ,  0x020000, 0x20000, 0x9963a6be )
	ROM_CONTINUE(             0x000000, 0x20000 )
ROM_END

ROM_START( ctribeb )
	ROM_REGION( 0x80000, REGION_CPU1 )	/* 64k for cpu code */
	ROM_LOAD_ODD ( "ct_ep1.rom", 0x00000, 0x20000, 0x9cfa997f )
	ROM_LOAD_EVEN( "ct_ep3.rom", 0x00000, 0x20000, 0x2ece8681 )
	ROM_LOAD_ODD ( "ct_ep2.rom", 0x40000, 0x10000, 0x8c2c6dbd )
	/* No EVEN rom! */

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for sound cpu code */
	ROM_LOAD( "ct_ep4.rom",   0x00000, 0x8000, 0x4346de13 )

	ROM_REGION( 0x200000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ct_mr7.rom",  0x000000, 0x40000, 0xa8b773f1 ) 	/* Background */
	ROM_LOAD( "ct_mr6.rom",  0x040000, 0x40000, 0x617530fc )
	ROM_LOAD( "ct_mr5.rom",  0x080000, 0x40000, 0xcef0a821 )
	ROM_LOAD( "ct_mr4.rom",  0x0c0000, 0x40000, 0xb84fda09 )

	ROM_REGION( 0x400000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ct_mr3.rom",  0x000000, 0x80000, 0x1ac2a461 ) 	/* Sprites */
	ROM_LOAD( "ct_ep5.rom",  0x080000, 0x10000, 0x972faddb )
	ROM_LOAD( "ct_mr2.rom",  0x100000, 0x80000, 0x8c796707 )
	ROM_LOAD( "ct_ep6.rom",  0x180000, 0x10000, 0xeb3ab374 )
	ROM_LOAD( "ct_mr1.rom",  0x200000, 0x80000, 0x1c9badbd )
	ROM_LOAD( "ct_ep7.rom",  0x280000, 0x10000, 0xc602ac97 )
	ROM_LOAD( "ct_mr0.rom",  0x300000, 0x80000, 0xba73c49e )
	ROM_LOAD( "ct_ep8.rom",  0x380000, 0x10000, 0x4da1d8e5 )

	ROM_REGION( 0x040000, REGION_SOUND1 )	/* ADPCM Samples */
	ROM_LOAD( "ct_mr8.rom" ,  0x020000, 0x20000, 0x9963a6be )
	ROM_CONTINUE(             0x000000, 0x20000 )
ROM_END

/**************************************************************************/

GAMEX( 1990, ddragon3, 0,        ddragon3, ddragon3, 0, ROT0, "Technos", "Double Dragon 3 - The Rosetta Stone", GAME_NO_COCKTAIL )
GAMEX( 1990, ddrago3b, ddragon3, ddrago3b, ddrago3b, 0, ROT0, "bootleg", "Double Dragon 3 - The Rosetta Stone (bootleg)", GAME_NO_COCKTAIL )
GAMEX( 1990, ctribe,   0,        ctribe,   ctribe,   0, ROT0, "Technos", "The Combatribes (US)", GAME_NO_COCKTAIL )
GAMEX( 1990, ctribeb,  ctribe,   ctribe,   ctribe,   0, ROT0, "bootleg", "The Combatribes (bootleg)", GAME_NO_COCKTAIL )

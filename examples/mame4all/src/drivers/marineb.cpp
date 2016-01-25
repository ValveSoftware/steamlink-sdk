#include "../vidhrdw/marineb.cpp"

/***************************************************************************

Marine Boy hardware memory map (preliminary)

driver by Zsolt Vasvari

MAIN CPU:

0000-7fff ROM (not all games use the entire region)
8000-87ff RAM
8c18-8c3f sprite RAM (Hoccer only)
8800-8bff video RAM	 \ (contains sprite RAM in invisible regions)
9000-93ff color RAM	 /

read:
a000	  IN0
a800	  IN1
b000	  DSW
b800      IN2/watchdog reset

write:
9800      column scroll
9a00      char palette bank bit 0 (not used by Hoccer)
9c00      char palette bank bit 1 (not used by Hoccer)
a000      NMI interrupt acknowledge/enable
a001      flipy
a002      flipx
b800	  ???


I/0 ports:
write
08        8910  control
09        8910  write

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



extern unsigned char *marineb_column_scroll;
extern int marineb_active_low_flipscreen;

void espial_init_machine(void);
void espial_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

WRITE_HANDLER( marineb_palbank0_w );
WRITE_HANDLER( marineb_palbank1_w );

WRITE_HANDLER( marineb_flipscreen_x_w );
WRITE_HANDLER( marineb_flipscreen_y_w );

void marineb_vh_screenrefresh (struct osd_bitmap *bitmap,int full_refresh);
void changes_vh_screenrefresh (struct osd_bitmap *bitmap,int full_refresh);
void springer_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void hoccer_vh_screenrefresh  (struct osd_bitmap *bitmap,int full_refresh);
void hopprobo_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static void marineb_init_machine(void)
{
	marineb_active_low_flipscreen = 0;
	espial_init_machine();
}

static void springer_init_machine(void)
{
	marineb_active_low_flipscreen = 1;
	espial_init_machine();
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x8bff, MRA_RAM },
	{ 0x9000, 0x93ff, MRA_RAM },
	{ 0xa000, 0xa000, input_port_0_r },
	{ 0xa800, 0xa800, input_port_1_r },
	{ 0xb000, 0xb000, input_port_2_r },
	{ 0xb800, 0xb800, input_port_3_r },  /* also watchdog */
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x87ff, MWA_RAM },
	{ 0x8800, 0x8bff, videoram_w, &videoram, &videoram_size },
	{ 0x8c00, 0x8c3f, MWA_RAM, &spriteram },  /* Hoccer only */
	{ 0x9000, 0x93ff, colorram_w, &colorram },
	{ 0x9800, 0x9800, MWA_RAM, &marineb_column_scroll },
	{ 0x9a00, 0x9a00, marineb_palbank0_w },
	{ 0x9c00, 0x9c00, marineb_palbank1_w },
	{ 0xa000, 0xa000, interrupt_enable_w },
	{ 0xa001, 0xa001, marineb_flipscreen_y_w },
	{ 0xa002, 0xa002, marineb_flipscreen_x_w },
	{ 0xb800, 0xb800, MWA_NOP },
	{ -1 }	/* end of table */
};


static struct IOWritePort marineb_writeport[] =
{
	{ 0x08, 0x08, AY8910_control_port_0_w },
	{ 0x09, 0x09, AY8910_write_port_0_w },
	{ -1 }	/* end of table */
};

static struct IOWritePort wanted_writeport[] =
{
	{ 0x00, 0x00, AY8910_control_port_0_w },
	{ 0x01, 0x01, AY8910_write_port_0_w },
	{ 0x02, 0x02, AY8910_control_port_1_w },
	{ 0x03, 0x03, AY8910_write_port_1_w },
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( marineb )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x3c, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x3c, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x1c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
  //PORT_DIPSETTING(    0x04, "???" )
  //PORT_DIPSETTING(    0x08, "???" )
  //PORT_DIPSETTING(    0x0c, "???" )
  //PORT_DIPSETTING(    0x10, "???" )
  //PORT_DIPSETTING(    0x14, "???" )
  //PORT_DIPSETTING(    0x18, "???" )
	PORT_DIPSETTING(    0x1c, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "20000 50000" )
	PORT_DIPSETTING(    0x20, "40000 70000" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( changes )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x3c, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x3c, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
  //PORT_DIPSETTING(    0x04, "???" )
  //PORT_DIPSETTING(    0x08, "???" )
	PORT_DIPSETTING(    0x0c, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x00, "1st Bonus Life" )
	PORT_DIPSETTING(    0x00, "20000" )
	PORT_DIPSETTING(    0x10, "40000" )
	PORT_DIPNAME( 0x20, 0x00, "2nd Bonus Life" )
	PORT_DIPSETTING(    0x00, "50000" )
	PORT_DIPSETTING(    0x20, "100000" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( hoccer )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x3c, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x3c, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Unknown ) )  /* difficulty maybe? */
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x20, "5" )
	PORT_DIPSETTING(    0x30, "6" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Coinage ) )
    PORT_DIPSETTING(    0xc0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )

	PORT_START	/* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2)
	PORT_BIT( 0xc0, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( wanted )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_COIN1 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "6" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0xf0, 0x10, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x40, " A 3C/1C  B 3C/1C" )
	PORT_DIPSETTING(    0xe0, " A 3C/1C  B 1C/2C" )
	PORT_DIPSETTING(    0xf0, " A 3C/1C  B 1C/4C" )
	PORT_DIPSETTING(    0x20, " A 2C/1C  B 2C/1C" )
	PORT_DIPSETTING(    0xd0, " A 2C/1C  B 1C/1C" )
	PORT_DIPSETTING(    0x70, " A 2C/1C  B 1C/3C" )
	PORT_DIPSETTING(    0xb0, " A 2C/1C  B 1C/5C" )
	PORT_DIPSETTING(    0xc0, " A 2C/1C  B 1C/6C" )
	PORT_DIPSETTING(    0x60, " A 1C/1C  B 4C/1C" )
	PORT_DIPSETTING(    0x50, " A 1C/1C  B 2C/1C" )
	PORT_DIPSETTING(    0x10, " A 1C/1C  B 1C/1C" )
	PORT_DIPSETTING(    0x30, " A 1C/2C  B 1C/2C" )
	PORT_DIPSETTING(    0xa0, " A 1C/1C  B 1C/3C" )
	PORT_DIPSETTING(    0x80, " A 1C/1C  B 1C/5C" )
	PORT_DIPSETTING(    0x90, " A 1C/1C  B 1C/6C" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
INPUT_PORTS_END


static struct GfxLayout marineb_charlayout =
{
	8,8,	/* 8*8 characters */
	512,	/* 512 characters */
	2,	    /* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },	/* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 bytes */
};

static struct GfxLayout wanted_charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	2,	    /* 2 bits per pixel */
	{ 4, 0 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },	/* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 bytes */
};

static struct GfxLayout hopprobo_charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	2,	    /* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{ 0, 1, 2, 3, 8*8+0, 8*8+1, 8*8+2, 8*8+3 },	/* bits are packed in groups of four */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	16*8	/* every char takes 16 bytes */
};

static struct GfxLayout marineb_small_spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	    /* 64 sprites */
	2,	    /* 2 bits per pixel */
	{ 0, 256*32*8 },	/* the two bitplanes are separated */
	{     0,     1,     2,     3,     4,     5,     6,     7,
	  8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{  0*8,  1*8,  2*8,  3*8,  4*8,  5*8,  6*8,  7*8,
	  16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout marineb_big_spritelayout =
{
	32,32,	/* 32*32 sprites */
	64,	    /* 64 sprites */
	2,	    /* 2 bits per pixel */
	{ 0, 256*32*8 },	/* the two bitplanes are separated */
	{      0,      1,      2,      3,      4,      5,      6,      7,
	   8*8+0,  8*8+1,  8*8+2,  8*8+3,  8*8+4,  8*8+5,  8*8+6,  8*8+7,
	  32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
	  40*8+0, 40*8+1, 40*8+2, 40*8+3, 40*8+4, 40*8+5, 40*8+6, 40*8+7 },
	{  0*8,  1*8,  2*8,  3*8,  4*8,  5*8,  6*8,  7*8,
	  16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8,
	  64*8, 65*8, 66*8, 67*8, 68*8, 69*8, 70*8, 71*8,
	  80*8, 81*8, 82*8, 83*8, 84*8, 85*8, 86*8, 87*8 },
	4*32*8	/* every sprite takes 128 consecutive bytes */
};

static struct GfxLayout changes_small_spritelayout =
{
	16,16,	/* 16*16 sprites */
	64,	    /* 64 sprites */
	2,	    /* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{      0,      1,      2,      3,  8*8+0,  8*8+1,  8*8+2,  8*8+3,
	  16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3 },
	{  0*8,  1*8,  2*8,  3*8,  4*8,  5*8,  6*8,  7*8,
	  32*8, 33*8, 34*8, 35*8, 36*8, 37*8, 38*8, 39*8 },
	64*8	/* every sprite takes 64 consecutive bytes */
};

static struct GfxLayout changes_big_spritelayout =
{
	32,32,	/* 32*3 sprites */
	16,	    /* 32 sprites */
	2,	    /* 2 bits per pixel */
	{ 0, 4 },	/* the two bitplanes for 4 pixels are packed into one byte */
	{      0,      1,      2,      3,  8*8+0,  8*8+1,  8*8+2,  8*8+3,
	  16*8+0, 16*8+1, 16*8+2, 16*8+3, 24*8+0, 24*8+1, 24*8+2, 24*8+3,
	  64*8+0, 64*8+1, 64*8+2, 64*8+3, 72*8+0, 72*8+1, 72*8+2, 72*8+3,
	  80*8+0, 80*8+1, 80*8+2, 80*8+3, 88*8+0, 88*8+1, 88*8+2, 88*8+3 },
	{   0*8,   1*8,   2*8,   3*8,   4*8,   5*8,   6*8,   7*8,
	   32*8,  33*8,  34*8,  35*8,  36*8,  37*8,  38*8,  39*8,
	  128*8, 129*8, 130*8, 131*8, 132*8, 133*8, 134*8, 135*8,
	  160*8, 161*8, 162*8, 163*8, 164*8, 165*8, 166*8, 167*8 },
	4*64*8	/* every sprite takes 256 consecutive bytes */
};


static struct GfxDecodeInfo marineb_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &marineb_charlayout,          0, 64 },
	{ REGION_GFX2, 0x0000, &marineb_small_spritelayout,  0, 64 },
	{ REGION_GFX2, 0x0000, &marineb_big_spritelayout,    0, 64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo wanted_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &wanted_charlayout,           0, 64 },
	{ REGION_GFX2, 0x0000, &marineb_small_spritelayout,  0, 64 },
	{ REGION_GFX2, 0x0000, &marineb_big_spritelayout,    0, 64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo changes_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &marineb_charlayout,          0, 64 },
	{ REGION_GFX2, 0x0000, &changes_small_spritelayout,  0, 64 },
	{ REGION_GFX2, 0x1000, &changes_big_spritelayout,    0, 64 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo hoccer_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &marineb_charlayout,          0, 16 },	/* no palette banks */
	{ REGION_GFX2, 0x0000, &changes_small_spritelayout,  0, 16 },	/* no palette banks */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo hopprobo_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &hopprobo_charlayout,         0, 64 },
	{ REGION_GFX2, 0x0000, &marineb_small_spritelayout,  0, 64 },
	{ REGION_GFX2, 0x0000, &marineb_big_spritelayout,    0, 64 },
	{ -1 } /* end of array */
};


static struct AY8910interface marineb_ay8910_interface =
{
	1,	/* 1 chip */
	1500000,	/* 1.5 MHz ? */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct AY8910interface wanted_ay8910_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz ? */
	{ 25, 25 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};


#define springer_gfxdecodeinfo   marineb_gfxdecodeinfo

#define wanted_vh_screenrefresh  springer_vh_screenrefresh

#define DRIVER(NAME, INITMACHINE, SNDHRDW, INTERRUPT)				\
static struct MachineDriver machine_driver_##NAME =					\
{																	\
	/* basic machine hardware */									\
	{																\
		{															\
			CPU_Z80,												\
			3072000,	/* 3.072 Mhz */								\
			readmem,writemem,0,SNDHRDW##_writeport,					\
			INTERRUPT,1	 	                                        \
		}															\
	},																\
	60, 5000,	/* frames per second, vblank duration */			\
	1,	/* single CPU game */										\
	INITMACHINE##_init_machine,										\
																	\
	/* video hardware */											\
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },						\
	NAME##_gfxdecodeinfo,											\
	256,256,														\
	espial_vh_convert_color_prom,									\
	VIDEO_TYPE_RASTER,												\
	0,																\
	generic_vh_start,												\
	generic_vh_stop,												\
	NAME##_vh_screenrefresh,										\
																	\
	/* sound hardware */											\
	0,0,0,0,														\
	{																\
		{															\
			SOUND_AY8910,											\
			&SNDHRDW##_ay8910_interface								\
		}															\
	}																\
}


/*     NAME      INITMACH  SNDHRDW	INTERRUPT */
DRIVER(marineb,  marineb,  marineb, nmi_interrupt);
DRIVER(changes,  marineb,  marineb, nmi_interrupt);
DRIVER(springer, springer, marineb, nmi_interrupt);
DRIVER(hoccer,   marineb,  marineb, nmi_interrupt);
DRIVER(wanted,   marineb,  wanted,  interrupt    );
DRIVER(hopprobo, marineb,  marineb, nmi_interrupt);


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( marineb )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "marineb.1",     0x0000, 0x1000, 0x661d6540 )
	ROM_LOAD( "marineb.2",     0x1000, 0x1000, 0x922da17f )
	ROM_LOAD( "marineb.3",     0x2000, 0x1000, 0x820a235b )
	ROM_LOAD( "marineb.4",     0x3000, 0x1000, 0xa157a283 )
	ROM_LOAD( "marineb.5",     0x4000, 0x1000, 0x9ffff9c0 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "marineb.6",     0x0000, 0x2000, 0xee53ec2e )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "marineb.8",     0x0000, 0x2000, 0xdc8bc46c )
	ROM_LOAD( "marineb.7",     0x2000, 0x2000, 0x9d2e19ab )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "marineb.1b",    0x0000, 0x0100, 0xf32d9472 ) /* palette low 4 bits */
	ROM_LOAD( "marineb.1c",    0x0100, 0x0100, 0x93c69d3e ) /* palette high 4 bits */
ROM_END

ROM_START( changes )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "changes.1",     0x0000, 0x1000, 0x56f83813 )
	ROM_LOAD( "changes.2",     0x1000, 0x1000, 0x0e627f0b )
	ROM_LOAD( "changes.3",     0x2000, 0x1000, 0xff8291e9 )
	ROM_LOAD( "changes.4",     0x3000, 0x1000, 0xa8e9aa22 )
	ROM_LOAD( "changes.5",     0x4000, 0x1000, 0xf4198e9e )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "changes.7",     0x0000, 0x2000, 0x2204194e )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "changes.6",     0x0000, 0x2000, 0x985c9db4 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "changes.1b",    0x0000, 0x0100, 0xf693c153 ) /* palette low 4 bits */
	ROM_LOAD( "changes.1c",    0x0100, 0x0100, 0xf8331705 ) /* palette high 4 bits */
ROM_END

ROM_START( looper )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "changes.1",     0x0000, 0x1000, 0x56f83813 )
	ROM_LOAD( "changes.2",     0x1000, 0x1000, 0x0e627f0b )
	ROM_LOAD( "changes.3",     0x2000, 0x1000, 0xff8291e9 )
	ROM_LOAD( "changes.4",     0x3000, 0x1000, 0xa8e9aa22 )
	ROM_LOAD( "changes.5",     0x4000, 0x1000, 0xf4198e9e )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "looper_7.bin",  0x0000, 0x2000, 0x71a89975 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "looper_6.bin",  0x0000, 0x2000, 0x1f3f70c2 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "changes.1b",    0x0000, 0x0100, 0xf693c153 ) /* palette low 4 bits */
	ROM_LOAD( "changes.1c",    0x0100, 0x0100, 0xf8331705 ) /* palette high 4 bits */
ROM_END

ROM_START( springer )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "springer.1",    0x0000, 0x1000, 0x0794103a )
	ROM_LOAD( "springer.2",    0x1000, 0x1000, 0xf4aecd9a )
	ROM_LOAD( "springer.3",    0x2000, 0x1000, 0x2f452371 )
	ROM_LOAD( "springer.4",    0x3000, 0x1000, 0x859d1bf5 )
	ROM_LOAD( "springer.5",    0x4000, 0x1000, 0x72adbbe3 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "springer.6",    0x0000, 0x1000, 0x6a961833 )
	ROM_LOAD( "springer.7",    0x1000, 0x1000, 0x95ab8fc0 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "springer.8",    0x0000, 0x1000, 0xa54bafdc )
							/* 0x1000-0x1fff empty for my convinience */
	ROM_LOAD( "springer.9",    0x2000, 0x1000, 0xfa302775 )
							/* 0x3000-0x3fff empty for my convinience */

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "1b.vid",        0x0000, 0x0100, 0xa2f935aa ) /* palette low 4 bits */
	ROM_LOAD( "1c.vid",        0x0100, 0x0100, 0xb95421f4 ) /* palette high 4 bits */
ROM_END

ROM_START( hoccer )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "hr1.cpu",       0x0000, 0x2000, 0x12e96635 )
	ROM_LOAD( "hr2.cpu",       0x2000, 0x2000, 0xcf1fc328 )
	ROM_LOAD( "hr3.cpu",       0x4000, 0x2000, 0x048a0659 )
	ROM_LOAD( "hr4.cpu",       0x6000, 0x2000, 0x9a788a2c )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hr.d",          0x0000, 0x2000, 0xd33aa980 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hr.c",          0x0000, 0x2000, 0x02808294 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "hr.1b",         0x0000, 0x0100, 0x896521d7 ) /* palette low 4 bits */
	ROM_LOAD( "hr.1c",         0x0100, 0x0100, 0x2efdd70b ) /* palette high 4 bits */
ROM_END

ROM_START( hoccer2 )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "hr.1",          0x0000, 0x2000, 0x122d159f )
	ROM_LOAD( "hr.2",          0x2000, 0x2000, 0x48e1efc0 )
	ROM_LOAD( "hr.3",          0x4000, 0x2000, 0x4e67b0be )
	ROM_LOAD( "hr.4",          0x6000, 0x2000, 0xd2b44f58 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hr.d",          0x0000, 0x2000, 0xd33aa980 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hr.c",          0x0000, 0x2000, 0x02808294 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "hr.1b",         0x0000, 0x0100, 0x896521d7 ) /* palette low 4 bits */
	ROM_LOAD( "hr.1c",         0x0100, 0x0100, 0x2efdd70b ) /* palette high 4 bits */
ROM_END

ROM_START( wanted )
	ROM_REGION( 0x10000, REGION_CPU1 )       /* 64k for code */
	ROM_LOAD( "prg-1",		   0x0000, 0x2000, 0x2dd90aed )
	ROM_LOAD( "prg-2",		   0x2000, 0x2000, 0x67ac0210 )
	ROM_LOAD( "prg-3",		   0x4000, 0x2000, 0x373c7d82 )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "vram-1",		   0x0000, 0x2000, 0xc4226e54 )
	ROM_LOAD( "vram-2",		   0x2000, 0x2000, 0x2a9b1e36 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "obj-a",		   0x0000, 0x2000, 0x90b60771 )
	ROM_LOAD( "obj-b",		   0x2000, 0x2000, 0xe14ee689 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "wanted.k7",	   0x0000, 0x0100, 0x2ba90a00 )	/* palette low 4 bits */
	ROM_LOAD( "wanted.k6",	   0x0100, 0x0100, 0xa93d87cc )	/* palette high 4 bits */
ROM_END

ROM_START( hopprobo )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "hopper01.3k",   0x0000, 0x1000, 0xfd7935c0 )
	ROM_LOAD( "hopper02.3l",   0x1000, 0x1000, 0xdf1a479a )
	ROM_LOAD( "hopper03.3n",   0x2000, 0x1000, 0x097ac2a7 )
	ROM_LOAD( "hopper04.3p",   0x3000, 0x1000, 0x0f4f3ca8 )
	ROM_LOAD( "hopper05.3r",   0x4000, 0x1000, 0x9d77a37b )

	ROM_REGION( 0x4000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hopper06.5c",   0x0000, 0x2000, 0x68f79bc8 )
	ROM_LOAD( "hopper07.5d",   0x2000, 0x1000, 0x33d82411 )
	ROM_RELOAD(				   0x3000, 0x1000 )

	ROM_REGION( 0x4000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "hopper08.6f",   0x0000, 0x2000, 0x06d37e64 )
	ROM_LOAD( "hopper09.6k",   0x2000, 0x2000, 0x047921c7 )

	ROM_REGION( 0x0200, REGION_PROMS )
	ROM_LOAD( "7052hop.1b",    0x0000, 0x0100, 0x94450775 ) /* palette low 4 bits */
	ROM_LOAD( "7052hop.1c",    0x0100, 0x0100, 0xa76bbd51 ) /* palette high 4 bits */
ROM_END



GAME( 1982, marineb,  0,       marineb,  marineb, 0, ROT0,   "Orca", "Marine Boy" )
GAME( 1982, changes,  0,       changes,  changes, 0, ROT0,   "Orca", "Changes" )
GAME( 1982, looper,   changes, changes,  changes, 0, ROT0,   "Orca", "Looper" )
GAME( 1982, springer, 0,       springer, marineb, 0, ROT270, "Orca", "Springer" )
GAME( 1983, hoccer,   0,       hoccer,   hoccer,  0, ROT90,  "Eastern Micro Electronics, Inc.", "Hoccer (set 1)" )
GAME( 1983, hoccer2,  hoccer,  hoccer,   hoccer,  0, ROT90,  "Eastern Micro Electronics, Inc.", "Hoccer (set 2)" )	/* earlier */
GAME( 1984, wanted,   0,       wanted,   wanted,  0, ROT90,  "Sigma Ent. Inc.", "Wanted" )
GAME( 1983, hopprobo, 0,       hopprobo, marineb, 0, ROT90,  "Sega", "Hopper Robo" )

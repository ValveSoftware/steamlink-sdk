#include "../vidhrdw/cloak.cpp"

/****************************************************************************

Master processor

IRQ: 4 IRQ's per frame at even intervals, 4th IRQ is at start of VBLANK

000-3FF	   Working RAM
400-7FF    Playfield RAM
800-FFF    Communication RAM (shared with slave processor)

1000-100F  Pokey 1
1008 (R)   bit 7 = Start 2 players
   	   bit 6 = Start 1 player

1800-180F  Pokey 2
1808(R)	   Dipswitches

2000 (R):  Joysticks
2200 (R):  Player 2 joysticks (for cocktail version)

2400 (R)   bit 0: Vertical Blank
	   bit 1: Self test switch
	   bit 2: Left Coin
	   bit 3: Right Coin
	   bit 4: Cocktail mode
	   bit 5: Aux Coin
	   bit 6: Player 2 Igniter button
	   bit 7: Player 1 Igniter button

2600 (W) Custom Write (this has something to do with positioning of the display out, I ignore it)

2800-29FF: (R/W) non-volatile RAM
3000-30FF: (R/W) Motion RAM
3200-327F: (W) Color RAM, Address bit 6 becomes the 9th bit of color RAM

3800: (W) Right Coin Counter
3801: (W) Left Coint Counter
3803: (W) Cocktail Output
3806: (W) Start 2 LED
3807: (W) Start 1 LED

3A00: (W) Watchdog reset
3C00: (W) Reset IRQ
3E00: (W) bit 0: Enable NVRAM

4000 - FFFF ROM
	4000-5FFF  136023.501
	6000-7FFF  136023.502
	8000-BFFF  136023.503
	C000-FFFF  136023.504


Slave processor

IRQ: 1 IRQ per frame at start of VBLANK

0000-0007: Working RAM
0008-000A, 000C-000E: (R/W) bit 0,1,2: Store to/Read From Bit Map RAM

0008: Decrement X/Increment Y
0009: Decrement Y
000A: Decrement X
000B: Set bitmap X coordinate
000C: Increment X/Increment Y  <-- Yes this is correct
000D: Increment Y
000E: Increment X
000F: Set bitmap Y coordinate

0010-07FF: Working RAM
0800-0FFF: Communication RAM (shared with master processor)

1000 (W): Reset IRQ
1200 (W):  bit 0: Swap bit maps
	   bit 1: Clear bit map
1400 (W): Custom Write (this has something to do with positioning of the display out, I ignore it)

2000-FFFF: Program ROM
	2000-3FFF: 136023.509
	4000-5FFF: 136023.510
	6000-7FFF: 136023.511
	8000-9FFF: 136023.512
	A000-BFFF: 136023.513
	C000-DFFF: 136023.514
	E000-EFFF: 136023.515


Motion object ROM: 136023.307,136023.308
Playfield ROM: 136023.306,136023.305

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

static unsigned char *enable_nvRAM;
static unsigned char *cloak_sharedram;
WRITE_HANDLER( cloak_paletteram_w );
READ_HANDLER( graph_processor_r );
WRITE_HANDLER( graph_processor_w );
WRITE_HANDLER( cloak_clearbmp_w );
extern int  cloak_vh_start(void);
extern void cloak_vh_stop(void);
extern void cloak_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


static unsigned char *nvram;
static size_t nvram_size;


static READ_HANDLER( cloak_sharedram_r )
{
	return cloak_sharedram[offset];
}

static WRITE_HANDLER( cloak_sharedram_w )
{
	cloak_sharedram[offset] = data;
}


static void nvram_handler(void *file, int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
		else
			memset(nvram,0,nvram_size);
	}
}


WRITE_HANDLER( cloak_led_w )
{
	osd_led_w(1 - offset,~data >> 7);
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0fff, cloak_sharedram_r },
	{ 0x2800, 0x29ff, MRA_RAM },
	{ 0x1000, 0x100f, pokey1_r },		/* DSW0 also */
	{ 0x1800, 0x180f, pokey2_r },		/* DSW1 also */
	{ 0x2000, 0x2000, input_port_0_r },	/* IN0 */
	{ 0x2200, 0x2200, input_port_1_r },	/* IN1 */
	{ 0x2400, 0x2400, input_port_2_r },	/* IN2 */
	{ 0x3000, 0x30ff, MRA_RAM },
	{ 0x3800, 0x3807, MRA_RAM },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x0400, 0x07ff, videoram_w, &videoram, &videoram_size },
	{ 0x0800, 0x0fff, cloak_sharedram_w, &cloak_sharedram },
	{ 0x1000, 0x100f, pokey1_w },
	{ 0x1800, 0x180f, pokey2_w },
	{ 0x2800, 0x29ff, MWA_RAM, &nvram, &nvram_size },
	{ 0x3000, 0x30ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3200, 0x327f, cloak_paletteram_w },
	{ 0x3800, 0x3801, coin_counter_w },
	{ 0x3802, 0x3805, MWA_RAM },
	{ 0x3806, 0x3807, cloak_led_w },
	{ 0x3a00, 0x3a00, watchdog_reset_w },
	{ 0x3e00, 0x3e00, MWA_RAM, &enable_nvRAM },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem2[] =
{
	{ 0x0000, 0x0007, MRA_RAM },
	{ 0x0008, 0x000f, graph_processor_r },
	{ 0x0010, 0x07ff, MRA_RAM },
	{ 0x0800, 0x0fff, cloak_sharedram_r },
	{ 0x2000, 0xffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem2[] =
{
	{ 0x0000, 0x0007, MWA_RAM },
	{ 0x0008, 0x000f, graph_processor_w },
	{ 0x0010, 0x07ff, MWA_RAM },
	{ 0x0800, 0x0fff, cloak_sharedram_w },
	{ 0x1200, 0x1200, cloak_clearbmp_w },
	{ 0x2000, 0xffff, MWA_ROM },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( cloak )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICKRIGHT_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_DOWN   | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_UP     | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_RIGHT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICKLEFT_LEFT   | IPF_8WAY | IPF_COCKTAIL )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_SERVICE( 0x02, IP_ACTIVE_LOW )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON6 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON5 )

	PORT_START	/* DSW0 */
	PORT_BIT( 0x3f, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x03, 0x02, "Credits" )
	PORT_DIPSETTING(    0x02, "*1" )
	PORT_DIPSETTING(    0x01, "*2" )
	PORT_DIPSETTING(    0x03, "/2" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0c, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Freeze" )	/* when active, press button 1 to freeze */
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

INPUT_PORTS_END


static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	256,    /* 512 characters */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the bitplanes are packed in one nibble */
	{ 0x1000*8+0, 0x1000*8+4, 0, 4, 0x1000*8+8, 0x1000*8+12, 8, 12 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	16*8   /* every char takes 16 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	8,16,   /* 8*16 sprites */
	128,    /* 128 sprites  */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the bitplanes are packed in one nibble */
	{ 0x1000*8+0, 0x1000*8+4, 0, 4, 0x1000*8+8, 0x1000*8+12, 8, 12 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	16*16   /* every char takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,     0,  1 },
	{ REGION_GFX2, 0, &spritelayout,  32,  1 },
	{ -1 }
};



static struct POKEYinterface pokey_interface =
{
	2,	/* 2 chips */
	1500000,	/* 1.5 MHz??? */
	{ 50, 50 },
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
	{ input_port_3_r, input_port_4_r }
};



static struct MachineDriver machine_driver_cloak =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1000000,	/* 1 Mhz ???? */
			readmem,writemem,0,0,
			interrupt,4
		},
		{
			CPU_M6502,
            1250000,        /* 1 Mhz ???? */
			readmem2,writemem2,0,0,
			interrupt,2
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	5,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 3*8, 32*8-1 },
	gfxdecodeinfo,
	64, 64,
	0,


	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_SUPPORTS_DIRTY,
	0,
	cloak_vh_start,
	cloak_vh_stop,
	cloak_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_POKEY,
			&pokey_interface
		}
	},

	nvram_handler
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( cloak )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "136023.501",   0x4000, 0x2000, 0xc2dbef1b )
	ROM_LOAD( "136023.502",   0x6000, 0x2000, 0x316d0c7b )
	ROM_LOAD( "136023.503",   0x8000, 0x4000, 0xb9c291a6 )
	ROM_LOAD( "136023.504",   0xc000, 0x4000, 0xd014a1c0 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for code */
	ROM_LOAD( "136023.509",   0x2000, 0x2000, 0x46c021a4 )
	ROM_LOAD( "136023.510",   0x4000, 0x2000, 0x8c9cf017 )
	ROM_LOAD( "136023.511",   0x6000, 0x2000, 0x66fd8a34 )
	ROM_LOAD( "136023.512",   0x8000, 0x2000, 0x48c8079e )
	ROM_LOAD( "136023.513",   0xa000, 0x2000, 0x13f1cbab )
	ROM_LOAD( "136023.514",   0xc000, 0x2000, 0x6f8c7991 )
	ROM_LOAD( "136023.515",   0xe000, 0x2000, 0x835438a0 )

	ROM_REGION( 0x2000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136023.305",   0x0000, 0x1000, 0xee443909 )
	ROM_LOAD( "136023.306",   0x1000, 0x1000, 0xd708b132 )

	ROM_REGION( 0x2000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "136023.307",   0x0000, 0x1000, 0xc42c84a4 )
	ROM_LOAD( "136023.308",   0x1000, 0x1000, 0x4fe13d58 )
ROM_END



GAMEX( 1983, cloak, 0, cloak, cloak, 0, ROT0, "Atari", "Cloak & Dagger", GAME_NO_COCKTAIL )

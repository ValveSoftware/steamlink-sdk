#include "../vidhrdw/exprraid.cpp"

/***************************************************************************

Express Raider - (c) 1986 Data East USA

Ernesto Corvi
ernesto@imagina.com

Memory Map:
Main CPU: ( 6502 )
0000-05ff RAM
0600-07ff Sprites
0800-0bff Videoram
0c00-0fff Colorram
1800-1800 DSW 0
1801-1801 Controls
1802-1802 Coins
1803-1803 DSW 1
2100-2100 Sound latch write
2800-2801 Protection
3800-3800 VBblank ( bootleg only )
4000-ffff MRA_ROM

Sound Cpu: ( 6809 )
0000-1fff RAM
2000-2001 YM2203
4000-4001 YM3526
6000-6000 Sound latch read
8000-ffff ROM

NOTES:
The main 6502 cpu is a custom one. The differences with a regular 6502 is as follows:
- Extra opcode ( $4b00 ), wich i think reads an external port. VBlank irq is on bit 1 ( 0x02 ).
- Reset, IRQ and NMI vectors are moved.

Also, there was some protection circuitry wich is now emulated.

The way i dealt with the custom opcode was to change it to return memory
position $ff (wich i verified is not used by the game). And i hacked in
a read handler wich returns the vblank on bit 1. It's an ugly hack, but
works fine.

The bootleg version patched the rom to get rid of the extra opcode ( bootlegs
used a regular 6502 ), the vectors hardcoded in place, and also had the
protection cracked.

The background tiles had a very ugly encoding. It was so ugly that our
decode gfx routine will not be able to decode it without some little help.
So thats why exprraid_gfx_expand() is there. Many thanks to Phil
Stroffolino, who figured out the encoding.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"

/* from vidhrdw */
extern unsigned char *exprraid_bgcontrol;
void exprraid_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void exprraid_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);


/*****************************************************************************************/
/* Emulate Protection ( only for original express raider, code is cracked on the bootleg */
/*****************************************************************************************/

static READ_HANDLER( exprraid_prot_0_r )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

	return RAM[0x02a9];
}

static READ_HANDLER( exprraid_prot_1_r )
{
	return 0x02;
}

static WRITE_HANDLER( sound_cpu_command_w )
{
    soundlatch_w(0,data);
    cpu_cause_interrupt(1,M6809_INT_NMI);
}

static READ_HANDLER( vblank_r ) {
	int val = readinputport( 0 );

	if ( ( val & 0x02 ) )
		cpu_spin();

	return val;
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x00ff, 0x00ff, vblank_r }, /* HACK!!!! see init_exprraid below */
    { 0x0000, 0x05ff, MRA_RAM },
    { 0x0600, 0x07ff, MRA_RAM }, /* sprites */
    { 0x0800, 0x0bff, videoram_r },
    { 0x0c00, 0x0fff, colorram_r },
    { 0x1800, 0x1800, input_port_1_r }, /* DSW 0 */
    { 0x1801, 0x1801, input_port_2_r }, /* Controls */
    { 0x1802, 0x1802, input_port_3_r }, /* Coins */
    { 0x1803, 0x1803, input_port_4_r }, /* DSW 1 */
	{ 0x2800, 0x2800, exprraid_prot_0_r }, /* protection */
	{ 0x2801, 0x2801, exprraid_prot_1_r }, /* protection */
    { 0x3800, 0x3800, vblank_r }, /* vblank on bootleg */
    { 0x4000, 0xffff, MRA_ROM },
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
    { 0x0000, 0x05ff, MWA_RAM },
    { 0x0600, 0x07ff, MWA_RAM, &spriteram, &spriteram_size }, /* sprites */
    { 0x0800, 0x0bff, videoram_w, &videoram, &videoram_size },
    { 0x0c00, 0x0fff, colorram_w, &colorram },
    { 0x2001, 0x2001, sound_cpu_command_w },
    { 0x2800, 0x2807, MWA_RAM, &exprraid_bgcontrol },
    { 0x4000, 0xffff, MWA_ROM },
    { -1 }  /* end of table */
};

static struct MemoryReadAddress sub_readmem[] =
{
    { 0x0000, 0x1fff, MRA_RAM },
    { 0x2000, 0x2000, YM2203_status_port_0_r },
	{ 0x2001, 0x2001, YM2203_read_port_0_r },
    { 0x4000, 0x4000, YM3526_status_port_0_r },
	{ 0x6000, 0x6000, soundlatch_r },
    { 0x8000, 0xffff, MRA_ROM },
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress sub_writemem[] =
{
    { 0x0000, 0x1fff, MWA_RAM },
    { 0x2000, 0x2000, YM2203_control_port_0_w },
	{ 0x2001, 0x2001, YM2203_write_port_0_w },
    { 0x4000, 0x4000, YM3526_control_port_0_w },
    { 0x4001, 0x4001, YM3526_write_port_0_w },
    { 0x8000, 0xffff, MWA_ROM },
    { -1 }  /* end of table */
};

INPUT_PORTS_START( exprraid )
	PORT_START /* IN 0 - 0x3800 */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VBLANK )

	PORT_START /* DSW 0 - 0x1800 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* IN 1 - 0x1801 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START /* IN 2 - 0x1802 */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START /* IN 3 - 0x1803 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x04, "Every 50000" )
	PORT_DIPSETTING(    0x00, "50000 80000" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )	/* This one has to be set for coin up */
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	1024,	/* 1024 characters */
	2,	/* 2 bits per pixel */
	{ 0, 4 },	/* the bitplanes are packed in the same byte */
	{ (0x2000*8)+0, (0x2000*8)+1, (0x2000*8)+2, (0x2000*8)+3, 0, 1, 2, 3 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,	/* 16*16 sprites */
	2048,	/* 2048 sprites */
	3,	/* 3 bits per pixel */
	{ 2*2048*32*8, 2048*32*8, 0 },	/* the bitplanes are separated */
	{ 128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7, 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every char takes 32 consecutive bytes */
};

static struct GfxLayout tile1 =
{
	16,16,	/* 16*16 tiles */
	128,	/* 128 tiles */
	3,	/* 3 bits per pixel */
	{ 4, 0x10000*8+0, 0x10000*8+4 },
	{ 0, 1, 2, 3, 1024*32*2,1024*32*2+1,1024*32*2+2,1024*32*2+3,
		128+0,128+1,128+2,128+3,128+1024*32*2,128+1024*32*2+1,128+1024*32*2+2,128+1024*32*2+3 }, /* BOGUS */
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
		64+0*8,64+1*8,64+2*8,64+3*8,64+4*8,64+5*8,64+6*8,64+7*8 },
	32*8
};

static struct GfxLayout tile2 =
{
	16,16,	/* 16*16 tiles */
	128,	/* 128 tiles */
	3,	/* 3 bits per pixel */
	{ 0, 0x11000*8+0, 0x11000*8+4  },
	{ 0, 1, 2, 3, 1024*32*2,1024*32*2+1,1024*32*2+2,1024*32*2+3,
		128+0,128+1,128+2,128+3,128+1024*32*2,128+1024*32*2+1,128+1024*32*2+2,128+1024*32*2+3 }, /* BOGUS */
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8,
		64+0*8,64+1*8,64+2*8,64+3*8,64+4*8,64+5*8,64+6*8,64+7*8 },
	32*8
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x00000, &charlayout,   128, 2 }, /* characters */
	{ REGION_GFX2, 0x00000, &spritelayout,  64, 8 }, /* sprites */
	{ REGION_GFX3, 0x00000, &tile1,          0, 4 }, /* background tiles */
	{ REGION_GFX3, 0x00000, &tile2,          0, 4 },
	{ REGION_GFX3, 0x04000, &tile1,          0, 4 },
	{ REGION_GFX3, 0x04000, &tile2,          0, 4 },
	{ REGION_GFX3, 0x08000, &tile1,          0, 4 },
	{ REGION_GFX3, 0x08000, &tile2,          0, 4 },
	{ REGION_GFX3, 0x0c000, &tile1,          0, 4 },
	{ REGION_GFX3, 0x0c000, &tile2,          0, 4 },
	{ -1 } /* end of array */
};



/* handler called by the 3812 emulator when the internal timers cause an IRQ */
static void irqhandler(int linestate)
{
	cpu_set_irq_line(1,0,linestate);
	//cpu_cause_interrupt(1,0xff);
}

static struct YM2203interface ym2203_interface =
{
    1,      /* 1 chip */
    1500000,        /* 1.5 MHz ??? */
    { YM2203_VOL(30,30) },
    { 0 },
    { 0 },
    { 0 },
    { 0 }
};

static struct YM3526interface ym3526_interface =
{
    1,                      /* 1 chip (no more supported) */
	3600000,	/* 3.600000 MHz ? (partially supported) */
    { 30 },		/* volume */
	{ irqhandler }
};

static int exprraid_interrupt(void)
{
	static int coin = 0;

	if ( ( ~readinputport( 3 ) ) & 0xc0 ) {
		if ( coin == 0 ) {
			coin = 1;
			return nmi_interrupt();
		}
	} else
		coin = 0;

	return ignore_interrupt();
}

static struct MachineDriver machine_driver_exprraid =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			4000000,        /* 4 Mhz ??? */
			readmem,writemem,0,0,
			exprraid_interrupt, 1
		},
		{
			CPU_M6809,
			2000000,        /* 2 Mhz ??? */
			sub_readmem,sub_writemem,0,0,
			ignore_interrupt,0	/* NMIs are caused by the main CPU */
								/* IRQs are caused by the YM3526 */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1, /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 1*8, 31*8-1, 1*8, 31*8-1 },
	gfxdecodeinfo,
	256, 256,
	exprraid_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	exprraid_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
	    {
	        SOUND_YM2203,
	        &ym2203_interface
	    },
	    {
	        SOUND_YM3526,
	        &ym3526_interface
	    }
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( exprraid )
    ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "cz01",    0x4000, 0x4000, 0xdc8f9fba )
    ROM_LOAD( "cz00",    0x8000, 0x8000, 0xa81290bc )

    ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the sub cpu */
    ROM_LOAD( "cz02",    0x8000, 0x8000, 0x552e6112 )

    ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "cz07",    0x00000, 0x4000, 0x686bac23 )	/* characters */

    ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cz09",    0x00000, 0x8000, 0x1ed250d1 )	/* sprites */
	ROM_LOAD( "cz08",    0x08000, 0x8000, 0x2293fc61 )
	ROM_LOAD( "cz13",    0x10000, 0x8000, 0x7c3bfd00 )
	ROM_LOAD( "cz12",    0x18000, 0x8000, 0xea2294c8 )
	ROM_LOAD( "cz11",    0x20000, 0x8000, 0xb7418335 )
	ROM_LOAD( "cz10",    0x28000, 0x8000, 0x2f611978 )

    ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cz04",    0x00000, 0x8000, 0x643a1bd3 )	/* tiles */
	/* Save 0x08000-0x0ffff to expand the previous so we can decode the thing */
	ROM_LOAD( "cz05",    0x10000, 0x8000, 0xc44570bf )	/* tiles */
	ROM_LOAD( "cz06",    0x18000, 0x8000, 0xb9bb448b )	/* tiles */

    ROM_REGION( 0x8000, REGION_GFX4 )     /* background tilemaps */
	ROM_LOAD( "cz03",    0x0000, 0x8000, 0x6ce11971 )

	ROM_REGION( 0x0400, REGION_PROMS )
    ROM_LOAD( "cz17.prm", 0x0000, 0x0100, 0xda31dfbc ) /* red */
    ROM_LOAD( "cz16.prm", 0x0100, 0x0100, 0x51f25b4c ) /* green */
    ROM_LOAD( "cz15.prm", 0x0200, 0x0100, 0xa6168d7f ) /* blue */
    ROM_LOAD( "cz14.prm", 0x0300, 0x0100, 0x52aad300 ) /* ??? */
ROM_END

ROM_START( wexpress )
    ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "2",       0x4000, 0x4000, 0xea5e5a8f )
    ROM_LOAD( "1",       0x8000, 0x8000, 0xa7daae12 )

    ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the sub cpu */
    ROM_LOAD( "cz02",    0x8000, 0x8000, 0x552e6112 )

    ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "cz07",    0x00000, 0x4000, 0x686bac23 )	/* characters */

    ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cz09",    0x00000, 0x8000, 0x1ed250d1 )	/* sprites */
	ROM_LOAD( "cz08",    0x08000, 0x8000, 0x2293fc61 )
	ROM_LOAD( "cz13",    0x10000, 0x8000, 0x7c3bfd00 )
	ROM_LOAD( "cz12",    0x18000, 0x8000, 0xea2294c8 )
	ROM_LOAD( "cz11",    0x20000, 0x8000, 0xb7418335 )
	ROM_LOAD( "cz10",    0x28000, 0x8000, 0x2f611978 )

    ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4",       0x00000, 0x8000, 0xf2e93ff0 )	/* tiles */
	/* Save 0x08000-0x0ffff to expand the previous so we can decode the thing */
	ROM_LOAD( "cz05",    0x10000, 0x8000, 0xc44570bf )	/* tiles */
	ROM_LOAD( "6",       0x18000, 0x8000, 0xc3a56de5 )	/* tiles */

    ROM_REGION( 0x8000, REGION_GFX4 )     /* background tilemaps */
	ROM_LOAD( "3",        0x0000, 0x8000, 0x242e3e64 )

	ROM_REGION( 0x0400, REGION_PROMS )
    ROM_LOAD( "cz17.prm", 0x0000, 0x0100, 0xda31dfbc ) /* red */
    ROM_LOAD( "cz16.prm", 0x0100, 0x0100, 0x51f25b4c ) /* green */
    ROM_LOAD( "cz15.prm", 0x0200, 0x0100, 0xa6168d7f ) /* blue */
    ROM_LOAD( "cz14.prm", 0x0300, 0x0100, 0x52aad300 ) /* ??? */
ROM_END

ROM_START( wexpresb )
    ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "wexpress.3", 0x4000, 0x4000, 0xb4dd0fa4 )
    ROM_LOAD( "wexpress.1", 0x8000, 0x8000, 0xe8466596 )

    ROM_REGION( 0x10000, REGION_CPU2 )	/* 64k for the sub cpu */
    ROM_LOAD( "cz02",    0x8000, 0x8000, 0x552e6112 )

    ROM_REGION( 0x04000, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "cz07",    0x00000, 0x4000, 0x686bac23 )	/* characters */

    ROM_REGION( 0x30000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cz09",    0x00000, 0x8000, 0x1ed250d1 )	/* sprites */
	ROM_LOAD( "cz08",    0x08000, 0x8000, 0x2293fc61 )
	ROM_LOAD( "cz13",    0x10000, 0x8000, 0x7c3bfd00 )
	ROM_LOAD( "cz12",    0x18000, 0x8000, 0xea2294c8 )
	ROM_LOAD( "cz11",    0x20000, 0x8000, 0xb7418335 )
	ROM_LOAD( "cz10",    0x28000, 0x8000, 0x2f611978 )

    ROM_REGION( 0x20000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "4",       0x00000, 0x8000, 0xf2e93ff0 )	/* tiles */
	/* Save 0x08000-0x0ffff to expand the previous so we can decode the thing */
	ROM_LOAD( "cz05",    0x10000, 0x8000, 0xc44570bf )	/* tiles */
	ROM_LOAD( "6",       0x18000, 0x8000, 0xc3a56de5 )	/* tiles */

    ROM_REGION( 0x8000, REGION_GFX4 )     /* background tilemaps */
	ROM_LOAD( "3",        0x0000, 0x8000, 0x242e3e64 )

	ROM_REGION( 0x0400, REGION_PROMS )
    ROM_LOAD( "cz17.prm", 0x0000, 0x0100, 0xda31dfbc ) /* red */
    ROM_LOAD( "cz16.prm", 0x0100, 0x0100, 0x51f25b4c ) /* green */
    ROM_LOAD( "cz15.prm", 0x0200, 0x0100, 0xa6168d7f ) /* blue */
    ROM_LOAD( "cz14.prm", 0x0300, 0x0100, 0x52aad300 ) /* ??? */
ROM_END



static void exprraid_gfx_expand(void)
{
	/* Expand the background rom so we can use regular decode routines */

	unsigned char	*gfx = memory_region(REGION_GFX3);
	int				offs = 0x10000-0x1000;
	int				i;


	for ( i = 0x8000-0x1000; i >= 0; i-= 0x1000 )
	{
		memcpy( &(gfx[offs]), &(gfx[i]), 0x1000 );

		offs -= 0x1000;

		memcpy( &(gfx[offs]), &(gfx[i]), 0x1000 );

		offs -= 0x1000;
	}
}


static void init_wexpress(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int i;


	exprraid_gfx_expand();

	/* HACK!: Implement custom opcode as regular with a mapped io read */
	for ( i = 0; i < 0x10000; i++ )
	{
		/* make sure is what we want to patch */
		if ( rom[i] == 0x4b && rom[i+1] == 0x00 && rom[i+2] == 0x29 && rom[i+3] == 0x02 )
		{
			/* replace custom opcode with: LDA	$FF */
			rom[i] = 0xa5;
			i++;
			rom[i] = 0xff;
		}
	}
}

static void init_exprraid(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);


	/* decode vectors */
	rom[0xfffa] = rom[0xfff7];
	rom[0xfffb] = rom[0xfff6];

	rom[0xfffc] = rom[0xfff1];
	rom[0xfffd] = rom[0xfff0];

	rom[0xfffe] = rom[0xfff3];
	rom[0xffff] = rom[0xfff2];

	/* HACK!: Implement custom opcode as regular with a mapped io read */
	init_wexpress();
}

static void init_wexpresb(void)
{
	exprraid_gfx_expand();
}


GAMEX( 1986, exprraid, 0,        exprraid, exprraid, exprraid, ROT0, "Data East USA", "Express Raider (US)", GAME_NO_COCKTAIL )
GAMEX( 1986, wexpress, exprraid, exprraid, exprraid, wexpress, ROT0, "Data East Corporation", "Western Express (World?)", GAME_NO_COCKTAIL )
GAMEX( 1986, wexpresb, exprraid, exprraid, exprraid, wexpresb, ROT0, "bootleg", "Western Express (bootleg)", GAME_NO_COCKTAIL )

#include "../vidhrdw/sidepckt.cpp"

/***************************************************************************

Side Pocket - (c) 1986 Data East

The original board has an 8751 protection mcu

Ernesto Corvi
ernesto@imagina.com

Thanks must go to Mirko Buffoni for testing the music.

i8751 protection simluation and other fixes by Bryan McPhail, 15/10/00.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6809/m6809.h"
#include "cpu/m6502/m6502.h"

/* from vidhrdw */
void sidepckt_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int sidepckt_vh_start(void);
void sidepckt_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( sidepckt_flipscreen_w );
WRITE_HANDLER( sidepckt_videoram_w );
WRITE_HANDLER( sidepckt_colorram_w );
static int i8751_return;


static WRITE_HANDLER( sound_cpu_command_w )
{
    soundlatch_w(offset,data);
    cpu_cause_interrupt(1,M6502_INT_NMI);
}

static READ_HANDLER( sidepckt_i8751_r )
{
	return i8751_return;
}

static WRITE_HANDLER( sidepckt_i8751_w )
{
	int table_1[]={5,3,2};
	int table_2[]={0x8e,0x42,0xad,0x58,0xec,0x85,0xdd,0x4c,0xad,0x9f,0x00,0x4c,0x7e,0x42,0xa2,0xff};
	int table_3[]={0xbd,0x73,0x80,0xbd,0x73,0xa7,0xbd,0x73,0xe0,0x7e,0x72,0x56,0xff,0xff,0xff,0xff};
	static int current_ptr=0,current_table=0,in_math=0,math_param;

	cpu_cause_interrupt(0,M6809_INT_FIRQ); /* i8751 triggers FIRQ on main cpu */

	/* This function takes multiple parameters */
	if (in_math==1) {
		in_math=2;
		i8751_return=math_param=data;
	}
	else if (in_math==2) {
		in_math=0;
		i8751_return=math_param/data;
	}
	else switch (data) {
		case 1: /* ID Check */
			current_table=1; current_ptr=0; i8751_return=table_1[current_ptr++]; break;

		case 2: /* Protection data (executable code) */
			current_table=2; current_ptr=0; i8751_return=table_2[current_ptr++]; break;

		case 3: /* Protection data (executable code) */
			current_table=3; current_ptr=0; i8751_return=table_3[current_ptr++]; break;

		case 4: /* Divide function - multiple parameters */
			in_math=1;
			i8751_return=4;
			break;

		case 6: /* Read table data */
			if (current_table==1) i8751_return=table_1[current_ptr++];
			if (current_table==2) i8751_return=table_2[current_ptr++];
			if (current_table==3) i8751_return=table_3[current_ptr++];
			break;
	}
}

static WRITE_HANDLER( sidepctj_i8751_w )
{
	int table_1[]={5,3,0};
	int table_2[]={0x8e,0x42,0xb2,0x58,0xec,0x85,0xdd,0x4c,0xad,0x9f,0x00,0x4c,0x7e,0x42,0xa7,0xff};
	int table_3[]={0xbd,0x71,0xc8,0xbd,0x71,0xef,0xbd,0x72,0x28,0x7e,0x70,0x9e,0xff,0xff,0xff,0xff};
	static int current_ptr=0,current_table=0,in_math,math_param;

	cpu_cause_interrupt(0,M6809_INT_FIRQ); /* i8751 triggers FIRQ on main cpu */

	/* This function takes multiple parameters */
	if (in_math==1) {
		in_math=2;
		i8751_return=math_param=data;
	}
	else if (in_math==2) {
		in_math=0;
		i8751_return=math_param/data;
	}
	else switch (data) {
		case 1: /* ID Check */
			current_table=1; current_ptr=0; i8751_return=table_1[current_ptr++]; break;

		case 2: /* Protection data */
			current_table=2; current_ptr=0; i8751_return=table_2[current_ptr++]; break;

		case 3: /* Protection data (executable code) */
			current_table=3; current_ptr=0; i8751_return=table_3[current_ptr++]; break;

		case 4: /* Divide function - multiple parameters */
			in_math=1;
			i8751_return=4;
			break;

		case 6: /* Read table data */
			if (current_table==1) i8751_return=table_1[current_ptr++];
			if (current_table==2) i8751_return=table_2[current_ptr++];
			if (current_table==3) i8751_return=table_3[current_ptr++];
			break;
	}
}

/******************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0fff, MRA_RAM },
	{ 0x1000, 0x17ff, videoram_r },
	{ 0x1800, 0x1fff, colorram_r },
	{ 0x2000, 0x20ff, MRA_RAM },
	{ 0x3000, 0x3000, input_port_0_r },
	{ 0x3001, 0x3001, input_port_1_r },
	{ 0x3002, 0x3002, input_port_2_r },
	{ 0x3003, 0x3003, input_port_3_r },
	{ 0x3014, 0x3014, sidepckt_i8751_r },
	{ 0x4000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x13ff, sidepckt_videoram_w, &videoram, &videoram_size },
	{ 0x1800, 0x1bff, sidepckt_colorram_w, &colorram },
	{ 0x2000, 0x20ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3004, 0x3004, sound_cpu_command_w },
	{ 0x300c, 0x300c, sidepckt_flipscreen_w },
	{ 0x3018, 0x3018, sidepckt_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress j_writemem[] =
{
	{ 0x0000, 0x0fff, MWA_RAM },
	{ 0x1000, 0x13ff, sidepckt_videoram_w, &videoram, &videoram_size },
	{ 0x1800, 0x1bff, sidepckt_colorram_w, &colorram },
	{ 0x2000, 0x20ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3004, 0x3004, sound_cpu_command_w },
	{ 0x300c, 0x300c, sidepckt_flipscreen_w },
	{ 0x3018, 0x3018, sidepctj_i8751_w },
	{ 0x4000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
    { 0x0000, 0x0fff, MRA_RAM },
    { 0x3000, 0x3000, soundlatch_r },
    { 0x8000, 0xffff, MRA_ROM },
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
    { 0x0000, 0x0fff, MWA_RAM },
    { 0x1000, 0x1000, YM2203_control_port_0_w },
    { 0x1001, 0x1001, YM2203_write_port_0_w },
    { 0x2000, 0x2000, YM3526_control_port_0_w },
    { 0x2001, 0x2001, YM3526_write_port_0_w },
    { 0x8000, 0xffff, MWA_ROM },
    { -1 }  /* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( sidepckt )
    PORT_START /* 0x3000 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

    PORT_START /* 0x3001 */
	/* I haven't found a way to make the game use the 2p controls */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

    PORT_START /* 0x3002 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x10, 0x10, "Unused?" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Unused?" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unused?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

    PORT_START /* 0x3003 */
	PORT_DIPNAME( 0x03, 0x03, "Timer Speed" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Stopped", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x03, "Slow" )
	PORT_DIPSETTING(    0x02, "Medium" )
	PORT_DIPSETTING(    0x01, "Fast" )
	PORT_DIPNAME( 0x0c, 0x08, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x0c, "6" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x30, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x10, "1" )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_DIPNAME( 0x80, 0x80, "Unused?" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
    8,8,    /* 8*8 characters */
    2048,   /* 2048 characters */
    3,      /* 3 bits per pixel */
    { 0, 0x8000*8, 0x10000*8 },     /* the bitplanes are separated */
    { 0, 1, 2, 3, 4, 5, 6, 7 },
    { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
    8*8     /* every char takes 8 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
    16,16,  /* 16*16 sprites */
    1024,   /* 1024 sprites */
    3,      /* 3 bits per pixel */
    { 0, 0x8000*8, 0x10000*8 },     /* the bitplanes are separated */
    { 128+0, 128+1, 128+2, 128+3, 128+4, 128+5, 128+6, 128+7, 0, 1, 2, 3, 4, 5, 6, 7 },
    { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
    32*8    /* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   128,  4 },	/* colors 128-159 */
	{ REGION_GFX2, 0, &spritelayout,   0, 16 },	/* colors   0-127 */
	{ -1 } /* end of array */
};



/* handler called by the 3526 emulator when the internal timers cause an IRQ */
static void irqhandler(int linestate)
{
	cpu_set_irq_line(1,0,linestate);
}

static struct YM2203interface ym2203_interface =
{
    1,      /* 1 chip */
    1500000,        /* 1.5 MHz */
    { YM2203_VOL(25,25) },
    { 0 },
    { 0 },
    { 0 },
    { 0 }
};

static struct YM3526interface ym3526_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz */
	{ 25 },		/* volume */
	{ irqhandler},
};



static struct MachineDriver machine_driver_sidepckt =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			2000000,        /* 2 MHz */
			readmem,writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,        /* 1.5 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM3526 */
								/* NMIs are triggered by the main cpu */
		}
	},
	58, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* VERIFY:  May be 55 or 56 */
	1, /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 256,
	sidepckt_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,// | VIDEO_SUPPORTS_DIRTY, tilemaps don't support dirty
	0,
	sidepckt_vh_start,
	generic_vh_stop,
	sidepckt_vh_screenrefresh,

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

static struct MachineDriver machine_driver_sidepctj =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			2000000,        /* 2 MHz */
			readmem,j_writemem,0,0,
			nmi_interrupt,1
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,        /* 1.5 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0	/* IRQs are triggered by the YM3526 */
								/* NMIs are triggered by the main cpu */
		}
	},
	58, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* VERIFY:  May be 55 or 56 */
	1, /* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	256, 256,
	sidepckt_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,// | VIDEO_SUPPORTS_DIRTY, tilemaps don't support dirty
	0,
	sidepckt_vh_start,
	generic_vh_stop,
	sidepckt_vh_screenrefresh,

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

ROM_START( sidepckt )
    ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
    ROM_LOAD( "dh00",         0x00000, 0x10000, 0x251b316e )

    ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio cpu */
    ROM_LOAD( "dh04.bin",     0x08000, 0x8000, 0xd076e62e )

    ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "sp_07.bin",    0x00000, 0x8000, 0x9d6f7969 ) /* characters */
    ROM_LOAD( "sp_06.bin",    0x08000, 0x8000, 0x580e4e43 )
    ROM_LOAD( "sp_05.bin",    0x10000, 0x8000, 0x05ab71d2 )

    ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "dh01.bin",     0x00000, 0x8000, 0xa2cdfbea ) /* sprites */
    ROM_LOAD( "dh02.bin",     0x08000, 0x8000, 0xeeb5c3e7 )
    ROM_LOAD( "dh03.bin",     0x10000, 0x8000, 0x8e18d21d )

    ROM_REGION( 0x0200, REGION_PROMS )	/* color PROMs */
    ROM_LOAD( "dh-09.bpr",    0x0000, 0x0100, 0xce049b4f )
    ROM_LOAD( "dh-08.bpr",    0x0100, 0x0100, 0xcdf2180f )
ROM_END

ROM_START( sidepctj )
    ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
    ROM_LOAD( "dh00.bin",     0x00000, 0x10000, 0xa66bc28d )

    ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio cpu */
    ROM_LOAD( "dh04.bin",     0x08000, 0x8000, 0xd076e62e )

    ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "dh07.bin",     0x00000, 0x8000, 0x7d0ce858 ) /* characters */
    ROM_LOAD( "dh06.bin",     0x08000, 0x8000, 0xb86ddf72 )
    ROM_LOAD( "dh05.bin",     0x10000, 0x8000, 0xdf6f94f2 )

    ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "dh01.bin",     0x00000, 0x8000, 0xa2cdfbea ) /* sprites */
    ROM_LOAD( "dh02.bin",     0x08000, 0x8000, 0xeeb5c3e7 )
    ROM_LOAD( "dh03.bin",     0x10000, 0x8000, 0x8e18d21d )

    ROM_REGION( 0x0200, REGION_PROMS )	/* color PROMs */
    ROM_LOAD( "dh-09.bpr",    0x0000, 0x0100, 0xce049b4f )
    ROM_LOAD( "dh-08.bpr",    0x0100, 0x0100, 0xcdf2180f )
ROM_END

ROM_START( sidepctb )
    ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
    ROM_LOAD( "sp_09.bin",    0x04000, 0x4000, 0x3c6fe54b )
    ROM_LOAD( "sp_08.bin",    0x08000, 0x8000, 0x347f81cd )

    ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio cpu */
    ROM_LOAD( "dh04.bin",     0x08000, 0x8000, 0xd076e62e )

    ROM_REGION( 0x18000, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "sp_07.bin",    0x00000, 0x8000, 0x9d6f7969 ) /* characters */
    ROM_LOAD( "sp_06.bin",    0x08000, 0x8000, 0x580e4e43 )
    ROM_LOAD( "sp_05.bin",    0x10000, 0x8000, 0x05ab71d2 )

    ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "dh01.bin",     0x00000, 0x8000, 0xa2cdfbea ) /* sprites */
    ROM_LOAD( "dh02.bin",     0x08000, 0x8000, 0xeeb5c3e7 )
    ROM_LOAD( "dh03.bin",     0x10000, 0x8000, 0x8e18d21d )

    ROM_REGION( 0x0200, REGION_PROMS )	/* color PROMs */
    ROM_LOAD( "dh-09.bpr",    0x0000, 0x0100, 0xce049b4f )
    ROM_LOAD( "dh-08.bpr",    0x0100, 0x0100, 0xcdf2180f )
ROM_END



GAME( 1986, sidepckt, 0,        sidepckt, sidepckt, 0, ROT0, "Data East Corporation", "Side Pocket (World)" )
GAME( 1986, sidepctj, sidepckt, sidepctj, sidepckt, 0, ROT0, "Data East Corporation", "Side Pocket (Japan)" )
GAME( 1986, sidepctb, sidepckt, sidepckt, sidepckt, 0, ROT0, "bootleg", "Side Pocket (bootleg)" )

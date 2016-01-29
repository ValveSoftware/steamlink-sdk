#include "../vidhrdw/dynduke.cpp"

/***************************************************************************

	Dynamite Duke						(c) 1989 Seibu Kaihatsu/Fabtek
	The Double Dynamites				(c) 1989 Seibu Kaihatsu/Fabtek


	To access test mode, reset with both start buttons held.

	Coins are controlled by the sound cpu, and the sound cpu is encrypted!
	You need to play with sound off at the moment to coin up.

	The background layer is 5bpp and I'm not 100% sure the colours are
	correct on it, although the layer is 5bpp the palette data is 4bpp.
	My current implementation looks pretty good though I've never seen
	the real game.

	There is a country code byte in the program to select between
	Seibu Kaihatsu/Fabtek/Taito licenses.

	Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "sndhrdw/seibu.h"

READ_HANDLER( dynduke_background_r );
READ_HANDLER( dynduke_foreground_r );
WRITE_HANDLER( dynduke_background_w );
WRITE_HANDLER( dynduke_foreground_w );
WRITE_HANDLER( dynduke_text_w );
WRITE_HANDLER( dynduke_gfxbank_w );
int dynduke_vh_start(void);
WRITE_HANDLER( dynduke_control_w );
void dynduke_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( dynduke_paletteram_w );

static unsigned char *dynduke_shared_ram;
extern unsigned char *dynduke_back_data,*dynduke_fore_data,*dynduke_scroll_ram,*dynduke_control_ram;

/***************************************************************************/

static READ_HANDLER( dynduke_shared_r ) { return dynduke_shared_ram[offset]; }
static WRITE_HANDLER( dynduke_shared_w ) { dynduke_shared_ram[offset]=data; }

static READ_HANDLER( dynduke_soundcpu_r )
{
	int erg,orig;
	orig=seibu_shared_sound_ram[offset];

	/* Small kludge to allows coins with sound off */
	if (offset==4 && (!Machine->sample_rate) && (readinputport(4)&1)==1) return 1;

	switch (offset)
	{
		case 0x04:{erg=seibu_shared_sound_ram[6];seibu_shared_sound_ram[6]=0;break;} /* just 1 time */
		case 0x06:{erg=0xa0;break;}
		case 0x0a:{erg=0;break;}
		default: erg=seibu_shared_sound_ram[offset];
	}
	return erg;
}

/******************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x00000, 0x07fff, MRA_RAM },
	{ 0x0a000, 0x0afff, dynduke_shared_r },
	{ 0x0b000, 0x0b000, input_port_0_r },
	{ 0x0b001, 0x0b001, input_port_1_r },
	{ 0x0b002, 0x0b002, input_port_2_r },
	{ 0x0b003, 0x0b003, input_port_3_r },
	{ 0x0d000, 0x0d00f, dynduke_soundcpu_r },
	{ 0xa0000, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x00000, 0x06fff, MWA_RAM },
	{ 0x07000, 0x07fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x08000, 0x080ff, MWA_RAM, &dynduke_scroll_ram },
	{ 0x0a000, 0x0afff, dynduke_shared_w, &dynduke_shared_ram },
	{ 0x0b000, 0x0b007, dynduke_control_w, &dynduke_control_ram },
	{ 0x0c000, 0x0c7ff, dynduke_text_w, &videoram },
	{ 0x0d000, 0x0d00f, seibu_soundlatch_w, &seibu_shared_sound_ram },
	{ 0xa0000, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sub_readmem[] =
{
	{ 0x00000, 0x05fff, MRA_RAM },
	{ 0x06000, 0x067ff, dynduke_background_r },
	{ 0x06800, 0x06fff, dynduke_foreground_r },
	{ 0x07000, 0x07fff, paletteram_r },
	{ 0x08000, 0x08fff, dynduke_shared_r },
	{ 0xc0000, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sub_writemem[] =
{
	{ 0x00000, 0x05fff, MWA_RAM },
	{ 0x06000, 0x067ff, dynduke_background_w, &dynduke_back_data },
	{ 0x06800, 0x06fff, dynduke_foreground_w, &dynduke_fore_data },
	{ 0x07000, 0x07fff, dynduke_paletteram_w, &paletteram },
	{ 0x08000, 0x08fff, dynduke_shared_w },
	{ 0x0a000, 0x0a001, dynduke_gfxbank_w },
	{ 0x0c000, 0x0c001, MWA_NOP },
	{ 0xc0000, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

/******************************************************************************/

#if 0
SEIBU_SOUND_SYSTEM_YM3812_MEMORY_MAP(input_port_4_r); /* Coin port */
#endif

/******************************************************************************/

INPUT_PORTS_START( dynduke )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* Dip switch A */
	PORT_DIPNAME( 0x07, 0x06, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x18, 0x08, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x20, 0x20, "Starting Coin" )
	PORT_DIPSETTING(    0x20, "normal" )
	PORT_DIPSETTING(    0x00, "X 2" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch B */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, "Difficulty?" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x40, 0x40, "Continue?" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Demo Sound?" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* Coins */
	PORT_BIT_IMPULSE( 0x01, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT_IMPULSE( 0x02, IP_ACTIVE_HIGH, IPT_COIN2, 1 )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,		/* 8*8 characters */
	1024,
	4,			/* 4 bits per pixel */
	{ 4,0,(0x10000*8)+4,0x10000*8 },
	{ 0,1,2,3,8,9,10,11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	128
};

static struct GfxLayout spritelayout =
{
  16,16,	/* 16*16 tiles */
  0x4000,
  4,		/* 4 bits per pixel */
  { 12, 8, 4, 0 },
  {
    0,1,2,3, 16,17,18,19,
	512+0,512+1,512+2,512+3,
	512+8+8,512+9+8,512+10+8,512+11+8,
  },
  {
	0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
	8*32, 9*32, 10*32, 11*32, 12*32, 13*32, 14*32, 15*32,
  },
  1024
};

static struct GfxLayout bg_layout =
{
	16,16,
	0x2000,
	5,
	{ 0x100000*8+4, 0x80000*8+4,0x80000*8,4,0 },
	{
		0,1,2,3,8,9,10,11,
		256+0,256+1,256+2,256+3,256+8,256+9,256+10,256+11
	},
	{
		0*16,1*16,2*16,3*16,4*16,5*16,6*16,7*16,
		8*16,9*16,10*16,11*16,12*16,13*16,14*16,15*16
	},
	512
};

static struct GfxLayout fg_layout =
{
	16,16,
	0x2000,
	4,
	{ 0x80000*8+4, 0x80000*8, 4, 0 },
	{
		0,1,2,3,8,9,10,11,
		256+0,256+1,256+2,256+3,256+8,256+9,256+10,256+11
	},
	{
		0*16,1*16,2*16,3*16,4*16,5*16,6*16,7*16,
		8*16,9*16,10*16,11*16,12*16,13*16,14*16,15*16
	},
	512
};

static struct GfxDecodeInfo dynduke_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   1280, 16 },
	{ REGION_GFX2, 0, &bg_layout,    2048, 32 }, /* Really 0 */
	{ REGION_GFX3, 0, &fg_layout,     512, 16 },
	{ REGION_GFX4, 0, &spritelayout,  768, 32 },
	{ -1 } /* end of array */
};

/******************************************************************************/

/* Parameters: YM3812 frequency, Oki frequency, Oki memory region */
SEIBU_SOUND_SYSTEM_YM3812_HARDWARE(14318180/4,8000,REGION_SOUND1);

static int dynduke_interrupt(void)
{
	return 0xc8/4;	/* VBL */
}

static void dynduke_eof_callback(void)
{
	buffer_spriteram_w(0,0); /* Could be a memory location instead */
}

static struct MachineDriver machine_driver_dynduke =
{
	/* basic machine hardware */
	{
		{
			CPU_V30, /* NEC V30-8 CPU */
			16000000, /* Guess */
			readmem,writemem,0,0,
			dynduke_interrupt,1
		},
		{
			CPU_V30, /* NEC V30-8 CPU */
			16000000, /* Guess */
			sub_readmem,sub_writemem,0,0,
			dynduke_interrupt,1
		},
#if 0
		{
			SEIBU_SOUND_SYSTEM_CPU(14318180/4)
		}
#endif
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	60,	/* CPU interleave  */
	0,//seibu_sound_init_2,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },

	dynduke_gfxdecodeinfo,
	2048+1024, 2048+1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	dynduke_eof_callback,
	dynduke_vh_start,
	0,
	dynduke_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		SEIBU_SOUND_SYSTEM_YM3812_INTERFACE
	}
};

/***************************************************************************/

ROM_START( dynduke )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* v30 main cpu */
	ROM_LOAD_V20_ODD ("dd1.cd8",   0x0a0000, 0x10000, 0xa5e2a95a )
	ROM_LOAD_V20_EVEN("dd2.cd7",   0x0a0000, 0x10000, 0x7e51af22 )
	ROM_LOAD_V20_ODD ("dd3.ef8",   0x0c0000, 0x20000, 0xa56f8692 )
	ROM_LOAD_V20_EVEN("dd4.ef7",   0x0c0000, 0x20000, 0xee4b87b3 )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* v30 sub cpu */
	ROM_LOAD_V20_ODD ("dd5.p8",  0x0e0000, 0x10000, 0x883d319c )
	ROM_LOAD_V20_EVEN("dd6.p7",  0x0e0000, 0x10000, 0xd94cb4ff )

	ROM_REGION( 0x18000, REGION_CPU3 ) /* sound Z80 */
	ROM_LOAD( "dd8.w8", 0x000000, 0x08000, 0x3c29480b )
	ROM_CONTINUE(       0x010000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dd9.jk5",	0x000000, 0x04000, 0xf2bc9af4 ) /* chars */
	ROM_LOAD( "dd10.jk3",	0x010000, 0x04000, 0xc2a9f19b )

	ROM_REGION( 0x180000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dd.a2",		0x000000, 0x40000, 0x598f343f ) /* background */
	ROM_LOAD( "dd.b2",		0x040000, 0x40000, 0x41a9088d )
	ROM_LOAD( "dd.c2",		0x080000, 0x40000, 0xcc341b42 )
	ROM_LOAD( "dd.d2",		0x0c0000, 0x40000, 0x4752b4d7 )
	ROM_LOAD( "dd.de3",		0x100000, 0x40000, 0x44a4cb62 )
	ROM_LOAD( "dd.ef3",		0x140000, 0x40000, 0xaa8aee1a )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dd.mn3",		0x000000, 0x40000, 0x2ee0ca98 ) /* foreground */
	ROM_LOAD( "dd.mn4",		0x040000, 0x40000, 0x6c71e2df )
	ROM_LOAD( "dd.n45",		0x080000, 0x40000, 0x85d918e1 )
	ROM_LOAD( "dd.mn5",		0x0c0000, 0x40000, 0xe71e34df )

	ROM_REGION( 0x200000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN(  "dd.n1", 0x000000, 0x40000, 0xcf1db927 ) /* sprites */
	ROM_LOAD_GFX_ODD (  "dd.n2", 0x000000, 0x40000, 0x5328150f )
	ROM_LOAD_GFX_EVEN(  "dd.m1", 0x080000, 0x40000, 0x80776452 )
	ROM_LOAD_GFX_ODD (  "dd.m2", 0x080000, 0x40000, 0xff61a573 )
	ROM_LOAD_GFX_EVEN(  "dd.e1", 0x100000, 0x40000, 0x84a0b87c )
	ROM_LOAD_GFX_ODD (  "dd.e2", 0x100000, 0x40000, 0xa9585df2 )
	ROM_LOAD_GFX_EVEN(  "dd.f1", 0x180000, 0x40000, 0x9aed24ba )
	ROM_LOAD_GFX_ODD (  "dd.f2", 0x180000, 0x40000, 0x3eb5783f )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "dd7.x10", 0x000000, 0x10000, 0x9cbc7b41 )
ROM_END

ROM_START( dbldyn )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* v30 main cpu */
	ROM_LOAD_V20_ODD ("dd1.cd8", 0x0a0000, 0x10000, 0xa5e2a95a )
	ROM_LOAD_V20_EVEN("dd2.cd7", 0x0a0000, 0x10000, 0x7e51af22 )
	ROM_LOAD_V20_ODD ("3.8e",    0x0c0000, 0x20000, 0x9b785028 )
	ROM_LOAD_V20_EVEN("4.7e",    0x0c0000, 0x20000, 0x0d0f6350 )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* v30 sub cpu */
	ROM_LOAD_V20_ODD ("5.8p",  0x0e0000, 0x10000, 0xea56d719 )
	ROM_LOAD_V20_EVEN("6.7p",  0x0e0000, 0x10000, 0x9ffa0ecd )

	ROM_REGION( 0x18000, REGION_CPU3 ) /* sound Z80 */
	ROM_LOAD( "8.8w", 0x000000, 0x08000, 0xf4066081 )
	ROM_CONTINUE(     0x010000, 0x08000 )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "9.5k",	    0x004000, 0x4000, 0x16bec703 ) /* chars */
	ROM_CONTINUE(           0x000000, 0x4000 )
	ROM_CONTINUE(           0x008000, 0x8000 )
	ROM_LOAD( "10.4k",	    0x014000, 0x4000, 0x719f909d )
	ROM_CONTINUE(           0x010000, 0x4000 )
	ROM_CONTINUE(           0x008000, 0x8000 )

	ROM_REGION( 0x180000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dd.a2",		0x000000, 0x40000, 0x598f343f ) /* background */
	ROM_LOAD( "dd.b2",		0x040000, 0x40000, 0x41a9088d )
	ROM_LOAD( "dd.c2",		0x080000, 0x40000, 0xcc341b42 )
	ROM_LOAD( "dd.d2",		0x0c0000, 0x40000, 0x4752b4d7 )
	ROM_LOAD( "dd.de3",		0x100000, 0x40000, 0x44a4cb62 )
	ROM_LOAD( "dd.ef3",		0x140000, 0x40000, 0xaa8aee1a )

	ROM_REGION( 0x100000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "dd.mn3",		0x000000, 0x40000, 0x2ee0ca98 ) /* foreground */
	ROM_LOAD( "dd.mn4",		0x040000, 0x40000, 0x6c71e2df )
	ROM_LOAD( "dd.n45",		0x080000, 0x40000, 0x85d918e1 )
	ROM_LOAD( "dd.mn5",		0x0c0000, 0x40000, 0xe71e34df )

	ROM_REGION( 0x200000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD_GFX_EVEN(  "dd.n1", 0x000000, 0x40000, 0xcf1db927 ) /* sprites */
	ROM_LOAD_GFX_ODD (  "dd.n2", 0x000000, 0x40000, 0x5328150f )
	ROM_LOAD_GFX_EVEN(  "dd.m1", 0x080000, 0x40000, 0x80776452 )
	ROM_LOAD_GFX_ODD (  "dd.m2", 0x080000, 0x40000, 0xff61a573 )
	ROM_LOAD_GFX_EVEN(  "dd.e1", 0x100000, 0x40000, 0x84a0b87c )
	ROM_LOAD_GFX_ODD (  "dd.e2", 0x100000, 0x40000, 0xa9585df2 )
	ROM_LOAD_GFX_EVEN(  "dd.f1", 0x180000, 0x40000, 0x9aed24ba )
	ROM_LOAD_GFX_ODD (  "dd.f2", 0x180000, 0x40000, 0x3eb5783f )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "dd7.x10", 0x000000, 0x10000, 0x9cbc7b41 )
ROM_END

/***************************************************************************/


static void init_dynduke(void)
{
	seibu_sound_decrypt();
}


GAMEX( 1989, dynduke, 0,       dynduke, dynduke, dynduke, ROT0, "Seibu Kaihatsu (Fabtek license)", "Dynamite Duke", GAME_NO_SOUND )
GAMEX( 1989, dbldyn,  dynduke, dynduke, dynduke, dynduke, ROT0, "Seibu Kaihatsu (Fabtek license)", "The Double Dynamites", GAME_NO_SOUND )

#include "../vidhrdw/raiden.cpp"

/***************************************************************************

	Raiden							(c) 1990 Seibu Kaihatsu
	Raiden (Alternate Hardware)		(c) 1990 Seibu Kaihatsu
	Raiden (Korean license)			(c) 1990 Seibu Kaihatsu

    driver by Oliver Bergmann, Bryan McPhail, Randy Mongenel


	The alternate hardware version is probably earlier than the main set.
	It looks closer to Dynamite Duke (1989 game), while the main set looks
	closer to the newer 68000 games in terms of graphics registers used, etc.

	As well as different graphics registers the alternate set has a
	different memory map, and different fix char layer memory layout!

	To access test mode, reset with both start buttons held.

	Coins are controlled by the sound cpu, there is a small kludge to allow
	the game to coin up if sound is off.

	The country byte is stored at 0xffffd in the main cpu region,
	(that's 0x1fffe in program rom 4).

		0x80  = World/Japan version? (Seibu Kaihatsu)
		0x81  = USA version (Fabtek license)
		0x82  = Taiwan version (Liang HWA Electronics license)
		0x83  = Hong Kong version (Wah Yan Electronics license)
		0x84  = Korean version (IBL Corporation license)

		There are also strings for Spanish, Greece, Mexico, Middle &
		South America though it's not clear if they are used.

	Todo: add support for Coin Mode B

	One of the boards is SEI8904 with SEI9008 subboard.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"
#include "sndhrdw/seibu.h"

READ_HANDLER( raiden_background_r );
READ_HANDLER( raiden_foreground_r );
WRITE_HANDLER( raiden_background_w );
WRITE_HANDLER( raiden_foreground_w );
WRITE_HANDLER( raiden_text_w );
WRITE_HANDLER( raidena_text_w );
int raiden_vh_start(void);
WRITE_HANDLER( raiden_control_w );
void raiden_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

static unsigned char *raiden_shared_ram;
extern unsigned char *raiden_back_data,*raiden_fore_data,*raiden_scroll_ram;

/***************************************************************************/

static READ_HANDLER( raiden_shared_r ) { return raiden_shared_ram[offset]; }
static WRITE_HANDLER( raiden_shared_w ) { raiden_shared_ram[offset]=data; }

static READ_HANDLER( raiden_sound_r )
{
	static int latch=0;
	int erg,orig,coin=readinputport(4);
	orig=seibu_shared_sound_ram[offset];

	/* Small kludge to allows coins with sound off */
	if (coin==0) latch=0;
	if (offset==4 && (!Machine->sample_rate) && coin && latch==0) {
		latch=1;
		return coin;
 	}

	switch (offset)
	{/* misusing $d006 as a latch...but it works !*/
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
	{ 0x0a000, 0x0afff, raiden_shared_r },
	{ 0x0b000, 0x0b000, input_port_0_r },
	{ 0x0b001, 0x0b001, input_port_1_r },
	{ 0x0b002, 0x0b002, input_port_2_r },
	{ 0x0b003, 0x0b003, input_port_3_r },
	{ 0x0d000, 0x0d00f, raiden_sound_r },
	{ 0xa0000, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x00000, 0x06fff, MWA_RAM },
	{ 0x07000, 0x07fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x0a000, 0x0afff, raiden_shared_w, &raiden_shared_ram },
	{ 0x0b000, 0x0b007, raiden_control_w },
	{ 0x0c000, 0x0c7ff, raiden_text_w, &videoram },
	{ 0x0d000, 0x0d00f, seibu_soundlatch_w, &seibu_shared_sound_ram },
	{ 0x0d060, 0x0d067, MWA_RAM, &raiden_scroll_ram },
	{ 0xa0000, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sub_readmem[] =
{
	{ 0x00000, 0x01fff, MRA_RAM },
	{ 0x02000, 0x027ff, raiden_background_r },
	{ 0x02800, 0x02fff, raiden_foreground_r },
	{ 0x03000, 0x03fff, paletteram_r },
	{ 0x04000, 0x04fff, raiden_shared_r },
	{ 0xc0000, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sub_writemem[] =
{
	{ 0x00000, 0x01fff, MWA_RAM },
	{ 0x02000, 0x027ff, raiden_background_w, &raiden_back_data },
	{ 0x02800, 0x02fff, raiden_foreground_w, &raiden_fore_data },
	{ 0x03000, 0x03fff, paletteram_xxxxBBBBGGGGRRRR_w, &paletteram },
	{ 0x04000, 0x04fff, raiden_shared_w },
	{ 0x07ffe, 0x0afff, MWA_NOP },
	{ 0xc0000, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

/************************* Alternate board set ************************/

static struct MemoryReadAddress alt_readmem[] =
{
	{ 0x00000, 0x07fff, MRA_RAM },
	{ 0x08000, 0x08fff, raiden_shared_r },
	{ 0x0a000, 0x0a00f, raiden_sound_r },
	{ 0x0e000, 0x0e000, input_port_0_r },
	{ 0x0e001, 0x0e001, input_port_1_r },
	{ 0x0e002, 0x0e002, input_port_2_r },
	{ 0x0e003, 0x0e003, input_port_3_r },
	{ 0xa0000, 0xfffff, MRA_ROM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress alt_writemem[] =
{
	{ 0x00000, 0x06fff, MWA_RAM },
	{ 0x07000, 0x07fff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x08000, 0x08fff, raiden_shared_w, &raiden_shared_ram },
	{ 0x0a000, 0x0a00f, seibu_soundlatch_w, &seibu_shared_sound_ram },
	{ 0x0b000, 0x0b007, raiden_control_w },
	{ 0x0c000, 0x0c7ff, raidena_text_w, &videoram },
	{ 0x0f000, 0x0f035, MWA_RAM, &raiden_scroll_ram },
	{ 0xa0000, 0xfffff, MWA_ROM },
	{ -1 }	/* end of table */
};

/******************************************************************************/

SEIBU_SOUND_SYSTEM_YM3812_MEMORY_MAP(input_port_4_r); /* Coin port */

/******************************************************************************/

INPUT_PORTS_START( raiden )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* Dip switch A */
	PORT_DIPNAME( 0x01, 0x01, "Coin Mode" )
	PORT_DIPSETTING(    0x01, "A" )
	PORT_DIPSETTING(    0x00, "B" )
	/* Coin Mode A */
	PORT_DIPNAME( 0x1e, 0x1e, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x16, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x1a, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 8C_3C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x1e, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x12, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )

	/* Coin Mode B */
/*	PORT_DIPNAME( 0x06, 0x06, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, "5C/1C or Free if Coin B too" )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, "1C/6C or Free if Coin A too" ) */

	PORT_DIPNAME( 0x20, 0x20, "Credits to Start" )
	PORT_DIPSETTING(    0x20, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x08, "80000 300000" )
	PORT_DIPSETTING(    0x0c, "150000 400000" )
	PORT_DIPSETTING(    0x04, "300000 1000000" )
	PORT_DIPSETTING(    0x00, "1000000 5000000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x20, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x10, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* Coins */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_COIN2 )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout raiden_charlayout =
{
	8,8,		/* 8*8 characters */
	2048,		/* 512 characters */
	4,			/* 4 bits per pixel */
	{ 4,0,(0x08000*8)+4,0x08000*8  },
	{ 0,1,2,3,8,9,10,11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	128
};

static struct GfxLayout raiden_spritelayout =
{
  16,16,	/* 16*16 tiles */
  4096,		/* 2048*4 tiles */
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

static struct GfxDecodeInfo raiden_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &raiden_charlayout,   768, 16 },
	{ REGION_GFX2, 0, &raiden_spritelayout,   0, 16 },
	{ REGION_GFX3, 0, &raiden_spritelayout, 256, 16 },
	{ REGION_GFX4, 0, &raiden_spritelayout, 512, 16 },
	{ -1 } /* end of array */
};

/******************************************************************************/

/* Parameters: YM3812 frequency, Oki frequency, Oki memory region */
SEIBU_SOUND_SYSTEM_YM3812_HARDWARE(14318180/4,8000,REGION_SOUND1);

static int raiden_interrupt(void)
{
	return 0xc8/4;	/* VBL */
}

static void raiden_eof_callback(void)
{
	buffer_spriteram_w(0,0); /* Could be a memory location instead */
}

static struct MachineDriver machine_driver_raiden =
{
	/* basic machine hardware */
	{
		{
			CPU_V30, /* NEC V30 CPU */
			20000000/2, /* 20MHz is correct, but glitched!? */
			readmem,writemem,0,0,
			raiden_interrupt,1
		},
		{
			CPU_V30, /* NEC V30 CPU */
			20000000/2, /* 20MHz is correct, but glitched!? */
			sub_readmem,sub_writemem,0,0,
			raiden_interrupt,1
		},
		{
			SEIBU_SOUND_SYSTEM_CPU(14318180/4)
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION*2,	/* frames per second, vblank duration */
	70,	/* CPU interleave  */
	seibu_sound_init_2,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	raiden_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	raiden_eof_callback,
	raiden_vh_start,
	0,
	raiden_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		SEIBU_SOUND_SYSTEM_YM3812_INTERFACE
	}
};

static struct MachineDriver machine_driver_raidena =
{
	/* basic machine hardware */
	{
		{
			CPU_V30, /* NEC V30 CPU */
			19000000, /* 20MHz is correct, but glitched!? */
			alt_readmem,alt_writemem,0,0,
			raiden_interrupt,1
		},
		{
			CPU_V30, /* NEC V30 CPU */
			19000000, /* 20MHz is correct, but glitched!? */
			sub_readmem,sub_writemem,0,0,
			raiden_interrupt,1
		},
		{
			SEIBU_SOUND_SYSTEM_CPU(14318180/4)
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION*2,	/* frames per second, vblank duration */
	60,	/* CPU interleave  */
	seibu_sound_init_2,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	raiden_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	raiden_eof_callback,
	raiden_vh_start,
	0,
	raiden_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		SEIBU_SOUND_SYSTEM_YM3812_INTERFACE
	}
};

/***************************************************************************/

ROM_START( raiden )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* v30 main cpu */
	ROM_LOAD_V20_ODD ( "rai1.bin",   0x0a0000, 0x10000, 0xa4b12785 )
	ROM_LOAD_V20_EVEN( "rai2.bin",   0x0a0000, 0x10000, 0x17640bd5 )
	ROM_LOAD_V20_ODD ( "rai3.bin",   0x0c0000, 0x20000, 0x9d735bf5 )
	ROM_LOAD_V20_EVEN( "rai4.bin",   0x0c0000, 0x20000, 0x8d184b99 )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* v30 sub cpu */
	ROM_LOAD_V20_ODD ( "rai5.bin",   0x0c0000, 0x20000, 0x7aca6d61 )
	ROM_LOAD_V20_EVEN( "rai6a.bin",  0x0c0000, 0x20000, 0xe3d35cc2 )

	ROM_REGION( 0x18000, REGION_CPU3 ) /* 64k code for sound Z80 */
	ROM_LOAD( "rai6.bin", 0x000000, 0x08000, 0x723a483b )
	ROM_CONTINUE(         0x010000, 0x08000 )

	ROM_REGION( 0x010000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rai9.bin",     0x00000, 0x08000, 0x1922b25e ) /* chars */
	ROM_LOAD( "rai10.bin",    0x08000, 0x08000, 0x5f90786a )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu0919.bin", 0x00000, 0x80000, 0xda151f0b ) /* tiles */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu0920.bin", 0x00000, 0x80000, 0xac1f57ac ) /* tiles */

	ROM_REGION( 0x090000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu165.bin",  0x00000, 0x80000, 0x946d7bde ) /* sprites */

	ROM_REGION( 0x10000, REGION_SOUND1 )	 /* ADPCM samples */
	ROM_LOAD( "rai7.bin", 0x00000, 0x10000, 0x8f927822 )
ROM_END

ROM_START( raidena )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* v30 main cpu */
	ROM_LOAD_V20_ODD ( "rai1.bin",     0x0a0000, 0x10000, 0xa4b12785 )
	ROM_LOAD_V20_EVEN( "rai2.bin",     0x0a0000, 0x10000, 0x17640bd5 )
	ROM_LOAD_V20_ODD ( "raiden03.rom", 0x0c0000, 0x20000, 0xf6af09d0 )
	ROM_LOAD_V20_EVEN( "raiden04.rom", 0x0c0000, 0x20000, 0x6bdfd416 )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* v30 sub cpu */
	ROM_LOAD_V20_ODD ( "raiden05.rom",   0x0c0000, 0x20000, 0xed03562e )
	ROM_LOAD_V20_EVEN( "raiden06.rom",   0x0c0000, 0x20000, 0xa19d5b5d )

	ROM_REGION( 0x18000, REGION_CPU3 ) /* 64k code for sound Z80 */
	ROM_LOAD( "raiden08.rom", 0x000000, 0x08000, 0x731adb43 )
	ROM_CONTINUE(             0x010000, 0x08000 )

	ROM_REGION( 0x010000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rai9.bin",     0x00000, 0x08000, 0x1922b25e ) /* chars */
	ROM_LOAD( "rai10.bin",    0x08000, 0x08000, 0x5f90786a )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu0919.bin", 0x00000, 0x80000, 0xda151f0b ) /* tiles */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu0920.bin", 0x00000, 0x80000, 0xac1f57ac ) /* tiles */

	ROM_REGION( 0x090000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu165.bin",  0x00000, 0x80000, 0x946d7bde ) /* sprites */

	ROM_REGION( 0x10000, REGION_SOUND1 )	 /* ADPCM samples */
	ROM_LOAD( "rai7.bin", 0x00000, 0x10000, 0x8f927822 )
ROM_END

ROM_START( raidenk )
	ROM_REGION( 0x100000, REGION_CPU1 ) /* v30 main cpu */
	ROM_LOAD_V20_ODD ( "rai1.bin",     0x0a0000, 0x10000, 0xa4b12785 )
	ROM_LOAD_V20_EVEN( "rai2.bin",     0x0a0000, 0x10000, 0x17640bd5 )
	ROM_LOAD_V20_ODD ( "raiden03.rom", 0x0c0000, 0x20000, 0xf6af09d0 )
	ROM_LOAD_V20_EVEN( "1i",           0x0c0000, 0x20000, 0xfddf24da )

	ROM_REGION( 0x100000, REGION_CPU2 ) /* v30 sub cpu */
	ROM_LOAD_V20_ODD ( "raiden05.rom",   0x0c0000, 0x20000, 0xed03562e )
	ROM_LOAD_V20_EVEN( "raiden06.rom",   0x0c0000, 0x20000, 0xa19d5b5d )

	ROM_REGION( 0x18000, REGION_CPU3 ) /* 64k code for sound Z80 */
	ROM_LOAD( "8b",           0x000000, 0x08000, 0x99ee7505 )
	ROM_CONTINUE(             0x010000, 0x08000 )

	ROM_REGION( 0x010000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "rai9.bin",     0x00000, 0x08000, 0x1922b25e ) /* chars */
	ROM_LOAD( "rai10.bin",    0x08000, 0x08000, 0x5f90786a )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu0919.bin", 0x00000, 0x80000, 0xda151f0b ) /* tiles */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu0920.bin", 0x00000, 0x80000, 0xac1f57ac ) /* tiles */

	ROM_REGION( 0x090000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "raiu165.bin",  0x00000, 0x80000, 0x946d7bde ) /* sprites */

	ROM_REGION( 0x10000, REGION_SOUND1 )	 /* ADPCM samples */
	ROM_LOAD( "rai7.bin", 0x00000, 0x10000, 0x8f927822 )
ROM_END

/***************************************************************************/

/* Spin the sub-cpu if it is waiting on the master cpu */
static READ_HANDLER( sub_cpu_spin_r )
{
	int pc=cpu_get_pc();
	int ret=raiden_shared_ram[0x8];

	if (offset==1) return raiden_shared_ram[0x9];

	if (pc==0xfcde6 && ret!=0x40)
		cpu_spin();

	return ret;
}

static READ_HANDLER( sub_cpu_spina_r )
{
	int pc=cpu_get_pc();
	int ret=raiden_shared_ram[0x8];

	if (offset==1) return raiden_shared_ram[0x9];

	if (pc==0xfcde8 && ret!=0x40)
		cpu_spin();

	return ret;
}

static void init_raiden(void)
{
	install_mem_read_handler(1, 0x4008, 0x4009, sub_cpu_spin_r);
	install_seibu_sound_speedup(2);
}

static void memory_patcha(void)
{
	install_mem_read_handler(1, 0x4008, 0x4009, sub_cpu_spina_r);
	install_seibu_sound_speedup(2);
}

/* This is based on code by Niclas Karlsson Mate, who figured out the
encryption method! The technique is a combination of a XOR table plus
bit-swapping */
static void common_decrypt(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int i,a;

	int xor_table[4][16]={
	  {0xF1,0xF9,0xF5,0xFD,0xF1,0xF1,0x3D,0x3D,   /* rom 3 */
	   0x73,0xFB,0x77,0xFF,0x73,0xF3,0x3F,0x3F},
	  {0xDF,0xFF,0xFF,0xFF,0xDB,0xFF,0xFB,0xFF,   /* rom 4 */
	   0xFF,0xFF,0xFF,0xFF,0xFB,0xFF,0xFB,0xFF},
	  {0x7F,0x7F,0xBB,0x77,0x77,0x77,0xBE,0xF6,   /* rom 5 */
	   0x7F,0x7F,0xBB,0x77,0x77,0x77,0xBE,0xF6},
	  {0xFF,0xFF,0xFD,0xFD,0xFD,0xFD,0xEF,0xEF,   /* rom 6 */
	   0xFF,0xFF,0xFD,0xFD,0xFD,0xFD,0xEF,0xEF}
	};

	/* Rom 3 - main cpu even bytes */
	for (i=0xc0000; i<0x100000; i+=2) {
		a=RAM[i];
		a^=xor_table[0][(i/2) & 0x0f];
    	a^=0xff;
   		a=(a & 0x31) | ((a<<1) & 0x04) | ((a>>5) & 0x02)
		| ((a<<4) & 0x40) | ((a<<4) & 0x80) | ((a>>4) & 0x08);
		RAM[i]=a;
	}

	/* Rom 4 - main cpu odd bytes */
	for (i=0xc0001; i<0x100000; i+=2) {
		a=RAM[i];
		a^=xor_table[1][(i/2) & 0x0f];
    	a^=0xff;
   		a=(a & 0xdb) | ((a>>3) & 0x04) | ((a<<3) & 0x20);
		RAM[i]=a;
	}

	RAM = memory_region(REGION_CPU2);

	/* Rom 5 - sub cpu even bytes */
	for (i=0xc0000; i<0x100000; i+=2) {
		a=RAM[i];
		a^=xor_table[2][(i/2) & 0x0f];
    	a^=0xff;
   		a=(a & 0x32) | ((a>>1) & 0x04) | ((a>>4) & 0x08)
		| ((a<<5) & 0x80) | ((a>>6) & 0x01) | ((a<<6) & 0x40);
		RAM[i]=a;
	}

	/* Rom 6 - sub cpu odd bytes */
	for (i=0xc0001; i<0x100000; i+=2) {
		a=RAM[i];
		a^=xor_table[3][(i/2) & 0x0f];
    	a^=0xff;
   		a=(a & 0xed) | ((a>>3) & 0x02) | ((a<<3) & 0x10);
		RAM[i]=a;
	}
}

static void init_raidenk(void)
{
	memory_patcha();
	common_decrypt();
}

static void init_raidena(void)
{
	memory_patcha();
	common_decrypt();
	seibu_sound_decrypt();
}


/***************************************************************************/

GAME( 1990, raiden,  0,      raiden,  raiden, raiden,  ROT270, "Seibu Kaihatsu", "Raiden" )
GAMEX(1990, raidena, raiden, raidena, raiden, raidena, ROT270, "Seibu Kaihatsu", "Raiden (Alternate Hardware)", GAME_NO_SOUND )
GAME( 1990, raidenk, raiden, raidena, raiden, raidenk, ROT270, "Seibu Kaihatsu (IBL Corporation license)", "Raiden (Korea)" )

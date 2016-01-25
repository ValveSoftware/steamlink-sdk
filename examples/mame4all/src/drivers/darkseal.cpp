#include "../vidhrdw/darkseal.cpp"

/***************************************************************************

	Dark Seal (Rev 3)    (c) 1990 Data East Corporation (World version)
	Dark Seal (Rev 1)    (c) 1990 Data East Corporation (World version)
	Dark Seal            (c) 1990 Data East Corporation (Japanese version)
	Gate Of Doom (Rev 4) (c) 1990 Data East Corporation (USA version)
	Gate of Doom (Rev 1) (c) 1990 Data East Corporation (USA version)

	Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/h6280/h6280.h"

int  darkseal_vh_start(void);
void darkseal_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( darkseal_pf1_data_w );
WRITE_HANDLER( darkseal_pf2_data_w );
WRITE_HANDLER( darkseal_pf3_data_w );
WRITE_HANDLER( darkseal_pf3b_data_w );
WRITE_HANDLER( darkseal_control_0_w );
WRITE_HANDLER( darkseal_control_1_w );
WRITE_HANDLER( darkseal_palette_24bit_rg_w );
WRITE_HANDLER( darkseal_palette_24bit_b_w );
READ_HANDLER( darkseal_palette_24bit_rg_r );
READ_HANDLER( darkseal_palette_24bit_b_r );
extern unsigned char *darkseal_pf12_row, *darkseal_pf34_row;
extern unsigned char *darkseal_pf1_data,*darkseal_pf2_data,*darkseal_pf3_data;
static unsigned char *darkseal_ram;

/******************************************************************************/

static WRITE_HANDLER( darkseal_control_w )
{
	switch (offset) {
    case 6: /* DMA flag */
		buffer_spriteram_w(0,0);
		return;
    case 8: /* Sound CPU write */
		soundlatch_w(0,data & 0xff);
		cpu_cause_interrupt(1,H6280_INT_IRQ1);
    	return;
  	case 0xa: /* IRQ Ack (VBL) */
		return;
	}
}

static READ_HANDLER( darkseal_control_r )
{
	switch (offset)
	{
		case 0: /* Dip Switches */
			return (readinputport(3) + (readinputport(4) << 8));

		case 2: /* Player 1 & Player 2 joysticks & fire buttons */
			return (readinputport(0) + (readinputport(1) << 8));

		case 4: /* Credits */
			return readinputport(2);
	}

	return 0xffff;
}

/******************************************************************************/

static struct MemoryReadAddress darkseal_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x103fff, MRA_BANK1 },
	{ 0x120000, 0x1207ff, MRA_BANK2 },
	{ 0x140000, 0x140fff, darkseal_palette_24bit_rg_r },
	{ 0x141000, 0x141fff, darkseal_palette_24bit_b_r },
	{ 0x180000, 0x18000f, darkseal_control_r },
	{ 0x220000, 0x220fff, MRA_BANK3 },
	{ 0x222000, 0x222fff, MRA_BANK4 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress darkseal_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x103fff, MWA_BANK1, &darkseal_ram },
	{ 0x120000, 0x1207ff, MWA_BANK2, &spriteram, &spriteram_size },
	{ 0x140000, 0x140fff, darkseal_palette_24bit_rg_w, &paletteram },
	{ 0x141000, 0x141fff, darkseal_palette_24bit_b_w, &paletteram_2 },
	{ 0x180000, 0x18000f, darkseal_control_w },
 	{ 0x200000, 0x200fff, darkseal_pf3b_data_w }, /* 2nd half of pf3, only used on last level */
	{ 0x202000, 0x203fff, darkseal_pf3_data_w, &darkseal_pf3_data },
	{ 0x220000, 0x220fff, MWA_BANK3, &darkseal_pf12_row },
	{ 0x222000, 0x222fff, MWA_BANK4, &darkseal_pf34_row },
	{ 0x240000, 0x24000f, darkseal_control_0_w },
	{ 0x260000, 0x261fff, darkseal_pf2_data_w, &darkseal_pf2_data },
	{ 0x262000, 0x263fff, darkseal_pf1_data_w, &darkseal_pf1_data },
	{ 0x2a0000, 0x2a000f, darkseal_control_1_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

static WRITE_HANDLER( YM2151_w )
{
	switch (offset) {
	case 0:
		YM2151_register_port_0_w(0,data);
		break;
	case 1:
		YM2151_data_port_0_w(0,data);
		break;
	}
}

static WRITE_HANDLER( YM2203_w )
{
	switch (offset) {
	case 0:
		YM2203_control_port_0_w(0,data);
		break;
	case 1:
		YM2203_write_port_0_w(0,data);
		break;
	}
}

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x100000, 0x100001, YM2203_status_port_0_r },
	{ 0x110000, 0x110001, YM2151_status_port_0_r },
	{ 0x120000, 0x120001, OKIM6295_status_0_r },
	{ 0x130000, 0x130001, OKIM6295_status_1_r },
	{ 0x140000, 0x140001, soundlatch_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x100000, 0x100001, YM2203_w },
	{ 0x110000, 0x110001, YM2151_w },
	{ 0x120000, 0x120001, OKIM6295_data_0_w },
	{ 0x130000, 0x130001, OKIM6295_data_1_w },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 },
	{ 0x1fec00, 0x1fec01, H6280_timer_w },
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( darkseal )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START	/* Credits */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x30, "Energy" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x10, "2.5" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	4096,
	4,		/* 4 bits per pixel  */
	{ 0x00000*8, 0x10000*8, 0x8000*8, 0x18000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout seallayout =
{
	16,16,
	4096,
	4,
	{ 8, 0,  0x40000*8+8, 0x40000*8,},
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxLayout seallayout2 =
{
	16,16,
	4096*2,
	4,
	{ 8, 0, 0x80000*8+8, 0x80000*8 },
	{ 32*8+0, 32*8+1, 32*8+2, 32*8+3, 32*8+4, 32*8+5, 32*8+6, 32*8+7,
		0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	64*8
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,    0, 16 },	/* Characters 8x8 */
	{ REGION_GFX2, 0, &seallayout,  768, 16 },	/* Tiles 16x16 */
	{ REGION_GFX3, 0, &seallayout, 1024, 16 },	/* Tiles 16x16 */
	{ REGION_GFX4, 0, &seallayout2, 256, 32 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

/******************************************************************************/

static struct OKIM6295interface okim6295_interface =
{
	2,              /* 2 chips */
	{ 7757, 15514 },/* Frequency */
	{ REGION_SOUND1, REGION_SOUND2 },       /* memory regions */
	{ 50, 25 }		/* Note!  Keep chip 1 (voices) louder than chip 2 */
};

static struct YM2203interface ym2203_interface =
{
	1,
	32220000/8,	/* Accurate, audio section crystal is 32.220 MHz */
	{ YM2203_VOL(40,40) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static void sound_irq(int state)
{
	cpu_set_irq_line(1,1,state); /* IRQ 2 */
}

static struct YM2151interface ym2151_interface =
{
	1,
	32220000/9, /* Accurate, audio section crystal is 32.220 MHz */
	{ YM3012_VOL(45,MIXER_PAN_LEFT,45,MIXER_PAN_RIGHT) },
	{ sound_irq }
};

static struct MachineDriver machine_driver_darkseal =
{
	/* basic machine hardware */
	{
	 	{
			CPU_M68000, /* Custom chip 59 */
			12000000,
			darkseal_readmem,darkseal_writemem,0,0,
			m68_level6_irq,1 /* VBL */
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU, /* Custom chip 45 */
			32220000/8, /* Audio section crystal is 32.220 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	58, 529, /* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
 	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK | VIDEO_BUFFERS_SPRITERAM,
	0,
	darkseal_vh_start,
	0,
	darkseal_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
  	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
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

/******************************************************************************/

ROM_START( darkseal )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ga04-3.rom",   0x00000, 0x20000, 0xbafad556 )
	ROM_LOAD_ODD ( "ga01-3.rom",   0x00000, 0x20000, 0xf409050e )
	ROM_LOAD_EVEN( "ga-00.rom",    0x40000, 0x20000, 0xfbf3ac63 )
	ROM_LOAD_ODD ( "ga-05.rom",    0x40000, 0x20000, 0xd5e3ae3f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fz-06.rom",    0x00000, 0x10000, 0xc4828a6d )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fz-02.rom",    0x000000, 0x10000, 0x3c9c3012 )	/* chars */
	ROM_LOAD( "fz-03.rom",    0x010000, 0x10000, 0x264b90ed )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-03.rom",   0x000000, 0x80000, 0x9996f3dc ) /* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-02.rom",   0x000000, 0x80000, 0x49504e89 ) /* tiles 2 */

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-00.rom",   0x000000, 0x80000, 0x52acf1d6 ) /* sprites */
	ROM_LOAD( "mac-01.rom",   0x080000, 0x80000, 0xb28f7584 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fz-08.rom",    0x00000, 0x20000, 0xc9bf68e1 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fz-07.rom",    0x00000, 0x20000, 0x588dd3cb )
ROM_END

ROM_START( darksea1 )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ga-04.rom",    0x00000, 0x20000, 0xa1a985a9 )
	ROM_LOAD_ODD ( "ga-01.rom",    0x00000, 0x20000, 0x98bd2940 )
	ROM_LOAD_EVEN( "ga-00.rom",    0x40000, 0x20000, 0xfbf3ac63 )
	ROM_LOAD_ODD ( "ga-05.rom",    0x40000, 0x20000, 0xd5e3ae3f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fz-06.rom",    0x00000, 0x10000, 0xc4828a6d )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fz-02.rom",    0x000000, 0x10000, 0x3c9c3012 )	/* chars */
	ROM_LOAD( "fz-03.rom",    0x010000, 0x10000, 0x264b90ed )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-03.rom",   0x000000, 0x80000, 0x9996f3dc ) /* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-02.rom",   0x000000, 0x80000, 0x49504e89 ) /* tiles 2 */

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-00.rom",   0x000000, 0x80000, 0x52acf1d6 ) /* sprites */
	ROM_LOAD( "mac-01.rom",   0x080000, 0x80000, 0xb28f7584 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fz-08.rom",    0x00000, 0x20000, 0xc9bf68e1 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fz-07.rom",    0x00000, 0x20000, 0x588dd3cb )
ROM_END

ROM_START( darkseaj )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fz-04.bin",    0x00000, 0x20000, 0x817faa2c )
	ROM_LOAD_ODD ( "fz-01.bin",    0x00000, 0x20000, 0x373caeee )
	ROM_LOAD_EVEN( "fz-00.bin",    0x40000, 0x20000, 0x1ab99aa7 )
	ROM_LOAD_ODD ( "fz-05.bin",    0x40000, 0x20000, 0x3374ef8c )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fz-06.rom",    0x00000, 0x10000, 0xc4828a6d )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fz-02.rom",    0x000000, 0x10000, 0x3c9c3012 )	/* chars */
	ROM_LOAD( "fz-03.rom",    0x010000, 0x10000, 0x264b90ed )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-03.rom",   0x000000, 0x80000, 0x9996f3dc ) /* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-02.rom",   0x000000, 0x80000, 0x49504e89 ) /* tiles 2 */

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-00.rom",   0x000000, 0x80000, 0x52acf1d6 ) /* sprites */
	ROM_LOAD( "mac-01.rom",   0x080000, 0x80000, 0xb28f7584 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fz-08.rom",    0x00000, 0x20000, 0xc9bf68e1 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fz-07.rom",    0x00000, 0x20000, 0x588dd3cb )
ROM_END

ROM_START( gatedoom )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "gb04-4",       0x00000, 0x20000, 0x8e3a0bfd )
	ROM_LOAD_ODD ( "gb01-4",       0x00000, 0x20000, 0x8d0fd383 )
	ROM_LOAD_EVEN( "ga-00.rom",    0x40000, 0x20000, 0xfbf3ac63 )
	ROM_LOAD_ODD ( "ga-05.rom",    0x40000, 0x20000, 0xd5e3ae3f )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fz-06.rom",    0x00000, 0x10000, 0xc4828a6d )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fz-02.rom",    0x000000, 0x10000, 0x3c9c3012 )	/* chars */
	ROM_LOAD( "fz-03.rom",    0x010000, 0x10000, 0x264b90ed )

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-03.rom",   0x000000, 0x80000, 0x9996f3dc ) /* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-02.rom",   0x000000, 0x80000, 0x49504e89 ) /* tiles 2 */

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-00.rom",   0x000000, 0x80000, 0x52acf1d6 ) /* sprites */
	ROM_LOAD( "mac-01.rom",   0x080000, 0x80000, 0xb28f7584 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fz-08.rom",    0x00000, 0x20000, 0xc9bf68e1 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fz-07.rom",    0x00000, 0x20000, 0x588dd3cb )
ROM_END

ROM_START( gatedom1 )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "gb04.bin",     0x00000, 0x20000, 0x4c3bbd2b )
	ROM_LOAD_ODD ( "gb01.bin",     0x00000, 0x20000, 0x59e367f4 )
	ROM_LOAD_EVEN( "gb00.bin",     0x40000, 0x20000, 0xa88c16a1 )
	ROM_LOAD_ODD ( "gb05.bin",     0x40000, 0x20000, 0x252d7e14 )

	ROM_REGION( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fz-06.rom",    0x00000, 0x10000, 0xc4828a6d )

	ROM_REGION( 0x020000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "fz-02.rom",    0x000000, 0x10000, 0x3c9c3012 )	/* chars */
	ROM_LOAD( "fz-03.rom",    0x010000, 0x10000, 0x264b90ed )

  	/* the following four have not been verified on a real Gate of Doom */
	/* board - might be different from Dark Seal! */

	ROM_REGION( 0x080000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-03.rom",   0x000000, 0x80000, 0x9996f3dc ) /* tiles 1 */

	ROM_REGION( 0x080000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-02.rom",   0x000000, 0x80000, 0x49504e89 ) /* tiles 2 */

	ROM_REGION( 0x100000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "mac-00.rom",   0x000000, 0x80000, 0x52acf1d6 ) /* sprites */
	ROM_LOAD( "mac-01.rom",   0x080000, 0x80000, 0xb28f7584 )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* ADPCM samples */
	ROM_LOAD( "fz-08.rom",    0x00000, 0x20000, 0xc9bf68e1 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* ADPCM samples */
	ROM_LOAD( "fz-07.rom",    0x00000, 0x20000, 0x588dd3cb )
ROM_END

/******************************************************************************/

static void darkseal_decrypt(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int i;

	for (i=0x00000; i<0x80000; i++)
		RAM[i]=(RAM[i] & 0xbd) | ((RAM[i] & 0x02) << 5) | ((RAM[i] & 0x40) >> 5);
}

static READ_HANDLER( darkseal_cycle_r )
{
	int b=READ_WORD(&darkseal_ram[0x6]);
	int a=READ_WORD(&darkseal_ram[0x2e7e]);
	int d=cpu_geticount();

	/* If possible skip this cpu segment - idle loop */
	if (d>99 && d<0xf0000000) {
		if (cpu_getpreviouspc()==0x160a && b==0xffff) {
			cpu_spinuntil_int();
			/* Update internal counter based on cycles left to run */
			a=((a+d/54)-1)&0xffff; /* 54 cycles per loop increment */
			WRITE_WORD(&darkseal_ram[0x2e7e],a);
		}
	}

	return b;
}

static void init_darkseal(void)
{
	install_mem_read_handler(0, 0x100006, 0x100007, darkseal_cycle_r);
	darkseal_decrypt();
}

/******************************************************************************/

GAME( 1990, darkseal, 0,        darkseal, darkseal, darkseal, ROT0, "Data East Corporation", "Dark Seal (World revision 3)" )
GAME( 1990, darksea1, darkseal, darkseal, darkseal, darkseal, ROT0, "Data East Corporation", "Dark Seal (World revision 1)" )
GAME( 1990, darkseaj, darkseal, darkseal, darkseal, darkseal, ROT0, "Data East Corporation", "Dark Seal (Japan)" )
GAME( 1990, gatedoom, darkseal, darkseal, darkseal, darkseal, ROT0, "Data East Corporation", "Gate of Doom (US revision 4)" )
GAME( 1990, gatedom1, darkseal, darkseal, darkseal, darkseal, ROT0, "Data East Corporation", "Gate of Doom (US revision 1)" )

#include "../vidhrdw/tumblep.cpp"

/***************************************************************************

  Tumblepop (World)     (c) 1991 Data East Corporation
  Tumblepop (Japan)     (c) 1991 Data East Corporation
  Tumblepop             (c) 1991 Data East Corporation (Bootleg 1)
  Tumblepop             (c) 1991 Data East Corporation (Bootleg 2)


  Bootleg sound is not quite correct yet (Nothing on bootleg 2).

  Emulation by Bryan McPhail, mish@tendril.co.uk

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/h6280/h6280.h"

int  tumblep_vh_start(void);
void tumblep_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void tumblepb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( tumblep_pf1_data_w );
WRITE_HANDLER( tumblep_pf2_data_w );
WRITE_HANDLER( tumblep_control_0_w );

extern unsigned char *tumblep_pf1_data,*tumblep_pf2_data;

/******************************************************************************/

static WRITE_HANDLER( tumblep_oki_w )
{
	OKIM6295_data_0_w(0,data&0xff);
    /* STUFF IN OTHER BYTE TOO..*/
}

static READ_HANDLER( tumblep_prot_r )
{
	return 0xffff;
}

static WRITE_HANDLER( tumblep_sound_w )
{
	soundlatch_w(0,data & 0xff);
	cpu_cause_interrupt(1,H6280_INT_IRQ1);
}

/******************************************************************************/

static READ_HANDLER( tumblepop_controls_r )
{
 	switch (offset)
	{
		case 0: /* Player 1 & Player 2 joysticks & fire buttons */
			return (readinputport(0) + (readinputport(1) << 8));
		case 2: /* Dips */
			return (readinputport(3) + (readinputport(4) << 8));
		case 8: /* Credits */
			return readinputport(2);
		case 10: /* ? */
		case 12:
        	return 0;
	}

	return 0xffff;
}

static READ_HANDLER( tumblep_pf1_data_r ) { return READ_WORD(&tumblep_pf1_data[offset]); }
static READ_HANDLER( tumblep_pf2_data_r ) { return READ_WORD(&tumblep_pf2_data[offset]); }

/******************************************************************************/

static struct MemoryReadAddress tumblepop_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x120000, 0x123fff, MRA_BANK1 },
	{ 0x140000, 0x1407ff, paletteram_word_r },
	{ 0x180000, 0x18000f, tumblepop_controls_r },
	{ 0x1a0000, 0x1a07ff, MRA_BANK2 },
	{ 0x320000, 0x320fff, tumblep_pf1_data_r },
	{ 0x322000, 0x322fff, tumblep_pf2_data_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress tumblepop_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x100001, tumblep_sound_w },
	{ 0x120000, 0x123fff, MWA_BANK1 },
	{ 0x140000, 0x1407ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0x18000c, 0x18000d, MWA_NOP },
	{ 0x1a0000, 0x1a07ff, MWA_BANK2, &spriteram },
	{ 0x300000, 0x30000f, tumblep_control_0_w },
	{ 0x320000, 0x320fff, tumblep_pf1_data_w, &tumblep_pf1_data },
	{ 0x322000, 0x322fff, tumblep_pf2_data_w, &tumblep_pf2_data },
	{ 0x340000, 0x3401ff, MWA_NOP }, /* Unused row scroll */
	{ 0x340400, 0x34047f, MWA_NOP }, /* Unused col scroll */
	{ 0x342000, 0x3421ff, MWA_NOP },
	{ 0x342400, 0x34247f, MWA_NOP },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress tumblepopb_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x100001, tumblep_prot_r },
	{ 0x120000, 0x123fff, MRA_BANK1 },
	{ 0x140000, 0x1407ff, paletteram_word_r },
	{ 0x160000, 0x1607ff, MRA_BANK5 },
	{ 0x180000, 0x18000f, tumblepop_controls_r },
	{ 0x1a0000, 0x1a07ff, MRA_BANK2 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress tumblepopb_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x100001, tumblep_oki_w },
	{ 0x120000, 0x123fff, MWA_BANK1 },
	{ 0x140000, 0x1407ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0x160000, 0x160807, MWA_BANK5, &spriteram }, /* Bootleg sprite buffer */
	{ 0x18000c, 0x18000d, MWA_NOP },
	{ 0x1a0000, 0x1a07ff, MWA_BANK2 },
	{ 0x300000, 0x30000f, tumblep_control_0_w },
	{ 0x320000, 0x320fff, tumblep_pf1_data_w, &tumblep_pf1_data },
	{ 0x322000, 0x322fff, tumblep_pf2_data_w, &tumblep_pf2_data },
	{ 0x340000, 0x3401ff, MWA_NOP }, /* Unused row scroll */
	{ 0x340400, 0x34047f, MWA_NOP }, /* Unused col scroll */
	{ 0x342000, 0x3421ff, MWA_NOP },
	{ 0x342400, 0x34247f, MWA_NOP },
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

/* Physical memory map (21 bits) */
static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x100000, 0x100001, MRA_NOP },
	{ 0x110000, 0x110001, YM2151_status_port_0_r },
	{ 0x120000, 0x120001, OKIM6295_status_0_r },
	{ 0x130000, 0x130001, MRA_NOP }, /* This board only has 1 oki chip */
	{ 0x140000, 0x140001, soundlatch_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x100000, 0x100001, MWA_NOP }, /* YM2203 - this board doesn't have one */
	{ 0x110000, 0x110001, YM2151_w },
	{ 0x120000, 0x120001, OKIM6295_data_0_w },
	{ 0x130000, 0x130001, MWA_NOP },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 },
	{ 0x1fec00, 0x1fec01, H6280_timer_w },
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

INPUT_PORTS_START( tumblep )
	PORT_START	/* Player 1 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START	/* Player 2 controls */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* button 3 - unused */
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
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x1c, 0x1c, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x1c, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x14, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "1" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x10, "Easy" )
	PORT_DIPSETTING(    0x30, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout tcharlayout =
{
	8,8,	/* 8*8 chars */
	4096,
	4,		/* 4 bits per pixel  */
	{ 0x40000*8 , 0x00000*8, 0x60000*8 , 0x20000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout tlayout =
{
	16,16,
	0x2000,
	4,
	{ 0x00000*8 , 0x40000*8, 0x80000*8, 0xc0000*8 },
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxLayout tlayout3 =
{
	16,16,
	4096,
	4,
	{  0x40000*8 , 0x00000*8, 0x60000*8 , 0x20000*8 },
	{
			0, 1, 2, 3, 4, 5, 6, 7,
           16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	32*8	/* every sprite takes 32 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tcharlayout,  256, 16 },	/* Characters 8x8 */
	{ REGION_GFX1, 0, &tlayout3,     512, 16 },	/* Tiles 16x16 */
	{ REGION_GFX1, 0, &tlayout3,     256, 16 },	/* Tiles 16x16 */
	{ REGION_GFX2, 0, &tlayout,        0, 16 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

/******************************************************************************/

static struct OKIM6295interface okim6295_interface2 =
{
	1,          /* 1 chip */
	{ 7757 },   /* 8000Hz frequency */
	{ 2 },      /* memory region 3 */
	{ 70 }
};

static struct OKIM6295interface okim6295_interface =
{
	1,          /* 1 chip */
	{ 7757 },	/* Frequency */
	{ REGION_SOUND1 },	/* memory region */
	{ 50 }
};

static void sound_irq(int state)
{
	cpu_set_irq_line(1,1,state); /* IRQ 2 */
}

static struct YM2151interface ym2151_interface =
{
	1,
	32220000/9, /* May not be correct, there is another crystal near the ym2151 */
	{ YM3012_VOL(45,MIXER_PAN_LEFT,45,MIXER_PAN_RIGHT) },
	{ sound_irq }
};

static struct MachineDriver machine_driver_tumblepop =
{
	/* basic machine hardware */
	{
	 	{
			CPU_M68000,
			14000000,
			tumblepop_readmem,tumblepop_writemem,0,0,
			m68_level6_irq,1
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU, /* Custom chip 45 */
			32220000/8, /* Audio section crystal is 32.220 MHz */
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,0
		}
	},
	58, 529,
	1,
	0,

	/* video hardware */
	40*8, 32*8, { 0*8, 40*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	tumblep_vh_start,
	0,
	tumblep_vh_screenrefresh,

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

static struct MachineDriver machine_driver_tumblepb =
{
	/* basic machine hardware */
	{
	 	{
			CPU_M68000,
			14000000,
			tumblepopb_readmem,tumblepopb_writemem,0,0,
			m68_level6_irq,1
		},
	},
	58, 529,
	1,
	0,

	/* video hardware */
	40*8, 32*8, { 0*8, 40*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	tumblep_vh_start,
	0,
	tumblepb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
  	{
		{
			SOUND_OKIM6295,
			&okim6295_interface2
		}
	}
};

/******************************************************************************/

ROM_START( tumblep )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_ODD ("hl01-1.f13", 0x00000, 0x40000, 0xd5a62a3f )
	ROM_LOAD_EVEN("hl00-1.f12", 0x00000, 0x40000, 0xfd697c1b )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound cpu */
	ROM_LOAD( "hl02-.f16",  0x00000,  0x10000, 0xa5cab888 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.19",  0x000000, 0x40000, 0x0795aab4 )
	ROM_LOAD( "thumbpop.18",  0x040000, 0x40000, 0xad58df43 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.15",  0x00000,  0x40000, 0xac3d8349 )
	ROM_LOAD( "thumbpop.14",  0x40000,  0x40000, 0x79a29725 )
	ROM_LOAD( "thumbpop.17",  0x80000,  0x40000, 0x87cffb06 )
	ROM_LOAD( "thumbpop.16",  0xc0000,  0x40000, 0xee91db18 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "hl03-.j15",  0x00000,  0x20000, 0x01b81da0 )
ROM_END

ROM_START( tumblepj )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_ODD ("hk01-1.f13", 0x00000, 0x40000, 0x56912a00 )
	ROM_LOAD_EVEN("hk00-1.f12", 0x00000, 0x40000, 0x2d3e4d3d )

	ROM_REGION( 0x10000, REGION_CPU2 ) /* Sound cpu */
	ROM_LOAD( "hl02-.f16",  0x00000,  0x10000, 0xa5cab888 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.19",  0x000000, 0x40000, 0x0795aab4 )
	ROM_LOAD( "thumbpop.18",  0x040000, 0x40000, 0xad58df43 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.15",  0x00000,  0x40000, 0xac3d8349 )
	ROM_LOAD( "thumbpop.14",  0x40000,  0x40000, 0x79a29725 )
	ROM_LOAD( "thumbpop.17",  0x80000,  0x40000, 0x87cffb06 )
	ROM_LOAD( "thumbpop.16",  0xc0000,  0x40000, 0xee91db18 )

	ROM_REGION( 0x20000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "hl03-.j15",  0x00000,  0x20000, 0x01b81da0 )
ROM_END

ROM_START( tumblepb )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN ("thumbpop.12", 0x00000, 0x40000, 0x0c984703 )
	ROM_LOAD_ODD ( "thumbpop.13", 0x00000, 0x40000, 0x864c4053 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.19",  0x000000, 0x40000, 0x0795aab4 )
	ROM_LOAD( "thumbpop.18",  0x040000, 0x40000, 0xad58df43 )

	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.15",  0x00000,  0x40000, 0xac3d8349 )
	ROM_LOAD( "thumbpop.14",  0x40000,  0x40000, 0x79a29725 )
	ROM_LOAD( "thumbpop.17",  0x80000,  0x40000, 0x87cffb06 )
	ROM_LOAD( "thumbpop.16",  0xc0000,  0x40000, 0xee91db18 )

	ROM_REGION( 0x80000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "thumbpop.snd",  0x00000,  0x80000, 0xfabbf15d )
ROM_END

ROM_START( tumblep2 )
	ROM_REGION( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN ("thumbpop.2", 0x00000, 0x40000, 0x34b016e1 )
	ROM_LOAD_ODD ( "thumbpop.3", 0x00000, 0x40000, 0x89501c71 )

	ROM_REGION( 0x080000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.19",  0x000000, 0x40000, 0x0795aab4 )
	ROM_LOAD( "thumbpop.18",  0x040000, 0x40000, 0xad58df43 )

 	ROM_REGION( 0x100000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "thumbpop.5",   0x00000,  0x40000, 0xdda8932e )
	ROM_LOAD( "thumbpop.14",  0x40000,  0x40000, 0x79a29725 )
	ROM_LOAD( "thumbpop.17",  0x80000,  0x40000, 0x87cffb06 )
	ROM_LOAD( "thumbpop.16",  0xc0000,  0x40000, 0xee91db18 )

	ROM_REGION( 0x80000, REGION_SOUND1 ) /* Oki samples */
	ROM_LOAD( "thumbpop.snd",  0x00000,  0x80000, 0xfabbf15d )
ROM_END

/******************************************************************************/

static void init_tumblep(void)
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int i,x,a;
    char z[64];

	/* Hmm, characters are stored in wrong word endian-ness for sequential graphics
		decode!  Very bad...  */
	RAM = memory_region(REGION_GFX1);

	for (a=0; a<4; a++) {
		for (i=32; i<0x2000; i+=32) {
            for (x=0; x<16; x++)
            	z[x]=RAM[i + x + (a*0x20000)];
            for (x=0; x<16; x++)
                RAM[i + x + (a*0x20000)]=RAM[i + x + 16 + (a*0x20000)];
            for (x=0; x<16; x++)
				RAM[i + x + 16 + (a*0x20000)]=z[x];
    	}
    }
}

/******************************************************************************/

GAME( 1991, tumblep,  0,       tumblepop, tumblep, tumblep, ROT0, "Data East Corporation", "Tumble Pop (World)" )
GAME( 1991, tumblepj, tumblep, tumblepop, tumblep, tumblep, ROT0, "Data East Corporation", "Tumble Pop (Japan)" )
GAMEX(1991, tumblepb, tumblep, tumblepb,  tumblep, tumblep, ROT0, "bootleg", "Tumble Pop (bootleg set 1)", GAME_IMPERFECT_SOUND )
GAMEX(1991, tumblep2, tumblep, tumblepb,  tumblep, tumblep, ROT0, "bootleg", "Tumble Pop (bootleg set 2)", GAME_IMPERFECT_SOUND )

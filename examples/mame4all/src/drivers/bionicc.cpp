#include "../vidhrdw/bionicc.cpp"

/********************************************************************

			  Bionic Commando



ToDo:
- finish video driver
	Some attributes are unknown. I don't remember the original game but
	seems there are some problems:
	- misplaced sprites ? ( see beginning of level 1 or 2 for example )
	- sprite / sprite priority ? ( see level 2 the reflectors )
	- sprite / background priority ? ( see level 1: birds walk through
		branches of different trees )
	- see the beginning of level 3: is the background screwed ?

- get rid of input port hack

	Controls appear to be mapped at 0xFE4000, alongside dip switches, but there
	is something strange going on that I can't (yet) figure out.
	Player controls and coin inputs are supposed to magically appear at
	0xFFFFFB (coin/start)
	0xFFFFFD (player 2)
	0xFFFFFF (player 1)
	This is probably done by an MPU on the board (whose ROM is not
	available).

	The MPU also takes care of the commands for the sound CPU, which are stored
	at FFFFF9.

	IRQ4 seems to be control related.
	On each interrupt, it reads 0xFE4000 (coin/start), shift the bits around
	and move the resulting byte into a dword RAM location. The dword RAM location
	is rotated by 8 bits each time this happens.
	This is probably done to be pedantic about coin insertions (might be protection
	related). In fact, currently coin insertions are not consistently recognized.

********************************************************************/


#include "driver.h"

static unsigned char *ram_bc; /* used by high scores */

WRITE_HANDLER( bionicc_fgvideoram_w );
WRITE_HANDLER( bionicc_bgvideoram_w );
WRITE_HANDLER( bionicc_txvideoram_w );
READ_HANDLER( bionicc_fgvideoram_r );
READ_HANDLER( bionicc_bgvideoram_r );
READ_HANDLER( bionicc_txvideoram_r );
WRITE_HANDLER( bionicc_paletteram_w );
WRITE_HANDLER( bionicc_scroll_w );
WRITE_HANDLER( bionicc_gfxctrl_w );

extern unsigned char *bionicc_bgvideoram;
extern unsigned char *bionicc_fgvideoram;
extern unsigned char *bionicc_txvideoram;
extern unsigned char *spriteram;
extern size_t spriteram_size;

int bionicc_vh_start(void);
void bionicc_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void bionicc_eof_callback(void);

void bionicc_readinputs(void);
void bionicc_sound_cmd(int data);



READ_HANDLER( bionicc_inputs_r )
{
//logerror("%06x: inputs_r %04x\n",cpu_get_pc(),offset);
	return (readinputport(offset)<<8) + readinputport(offset+1);
}

static unsigned char bionicc_inp[6];

WRITE_HANDLER( hacked_controls_w )
{
//logerror("%06x: hacked_controls_w %04x %02x\n",cpu_get_pc(),offset,data);
	COMBINE_WORD_MEM( &bionicc_inp[offset], data);
}

static READ_HANDLER( hacked_controls_r )
{
//logerror("%06x: hacked_controls_r %04x %04x\n",cpu_get_pc(),offset,READ_WORD( &bionicc_inp[offset] ));
	return READ_WORD( &bionicc_inp[offset] );
}

static WRITE_HANDLER( bionicc_mpu_trigger_w )
{
	data = readinputport(0) >> 4;
	WRITE_WORD(&bionicc_inp[0x00],data ^ 0x0f);

	data = readinputport(5); /* player 2 controls */
	WRITE_WORD(&bionicc_inp[0x02],data ^ 0xff);

	data = readinputport(4); /* player 1 controls */
	WRITE_WORD(&bionicc_inp[0x04],data ^ 0xff);
}



static unsigned char soundcommand[2];

WRITE_HANDLER( hacked_soundcommand_w )
{
	COMBINE_WORD_MEM( &soundcommand[offset], data);
	soundlatch_w(0,data & 0xff);
}

static READ_HANDLER( hacked_soundcommand_r )
{
	return READ_WORD( &soundcommand[offset] );
}


/********************************************************************

  INTERRUPT

  The game runs on 2 interrupts.

  IRQ 2 drives the game
  IRQ 4 processes the input ports

  The game is very picky about timing. The following is the only
  way I have found it to work.

********************************************************************/

int bionicc_interrupt(void)
{
	if (cpu_getiloops() == 0) return 2;
	else return 4;
}
static struct MemoryReadAddress readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM },                /* 68000 ROM */
	{ 0xfe0000, 0xfe07ff, MRA_BANK1 },              /* RAM? */
	{ 0xfe0800, 0xfe0cff, MRA_BANK2 },              /* sprites */
	{ 0xfe0d00, 0xfe3fff, MRA_BANK3 },              /* RAM? */
	{ 0xfe4000, 0xfe4003, bionicc_inputs_r },       /* dipswitches */
	{ 0xfec000, 0xfecfff, bionicc_txvideoram_r },
	{ 0xff0000, 0xff3fff, bionicc_fgvideoram_r },
	{ 0xff4000, 0xff7fff, bionicc_bgvideoram_r },
	{ 0xff8000, 0xff87ff, paletteram_word_r },
	{ 0xffc000, 0xfffff7, MRA_BANK8 },               /* working RAM */
	{ 0xfffff8, 0xfffff9, hacked_soundcommand_r },      /* hack */
	{ 0xfffffa, 0xffffff, hacked_controls_r },      /* hack */
	{ -1 }
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM },
	{ 0xfe0000, 0xfe07ff, MWA_BANK1 },	/* RAM? */
	{ 0xfe0800, 0xfe0cff, MWA_BANK2, &spriteram, &spriteram_size },
	{ 0xfe0d00, 0xfe3fff, MWA_BANK3 },              /* RAM? */
	{ 0xfe4000, 0xfe4001, bionicc_gfxctrl_w },	/* + coin counters */
	{ 0xfe8010, 0xfe8017, bionicc_scroll_w },
	{ 0xfe801a, 0xfe801b, bionicc_mpu_trigger_w },	/* ??? not sure, but looks like it */
	{ 0xfec000, 0xfecfff, bionicc_txvideoram_w, &bionicc_txvideoram },
	{ 0xff0000, 0xff3fff, bionicc_fgvideoram_w, &bionicc_fgvideoram },
	{ 0xff4000, 0xff7fff, bionicc_bgvideoram_w, &bionicc_bgvideoram },
	{ 0xff8000, 0xff87ff, bionicc_paletteram_w, &paletteram },
	{ 0xffc000, 0xfffff7, MWA_BANK8, &ram_bc },	/* working RAM */
	{ 0xfffff8, 0xfffff9, hacked_soundcommand_w },      /* hack */
	{ 0xfffffa, 0xffffff, hacked_controls_w },	/* hack */
	{ -1 }
};


static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8001, 0x8001, YM2151_status_port_0_r },
	{ 0xa000, 0xa000, soundlatch_r },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x8000, YM2151_register_port_0_w },
	{ 0x8001, 0x8001, YM2151_data_port_0_w },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ -1 }  /* end of table */
};



INPUT_PORTS_START( bionicc )
	PORT_START
	PORT_BIT( 0x0f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START      /* DSW2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x00, "7" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x18, "20K, 40K, every 60K")
	PORT_DIPSETTING(    0x10, "30K, 50K, every 70K" )
	PORT_DIPSETTING(    0x08, "20K and 60K only")
	PORT_DIPSETTING(    0x00, "30K and 70K only" )
	PORT_DIPNAME( 0x60, 0x40, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x40, "Easy" )
	PORT_DIPSETTING(    0x60, "Medium")
	PORT_DIPSETTING(    0x20, "Hard")
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x80, "Freeze" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START      /* DSW1 */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_6C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_6C ) )
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ))
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END



/********************************************************************

  GRAPHICS

********************************************************************/


static struct GfxLayout spritelayout_bionicc=
{
	16,16,  /* 16*16 sprites */
	2048,   /* 2048 sprites */
	4,      /* 4 bits per pixel */
	{ 0x30000*8,0x20000*8,0x10000*8,0 },
	{
		0,1,2,3,4,5,6,7,
		(16*8)+0,(16*8)+1,(16*8)+2,(16*8)+3,
		(16*8)+4,(16*8)+5,(16*8)+6,(16*8)+7
	},
	{
		0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
		8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8,
	},
	256   /* every sprite takes 256 consecutive bytes */
};

static struct GfxLayout vramlayout_bionicc=
{
	8,8,    /* 8*8 characters */
	1024,   /* 1024 character */
	2,      /* 2 bitplanes */
	{ 4,0 },
	{ 0,1,2,3,8,9,10,11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	128   /* every character takes 128 consecutive bytes */
};

static struct GfxLayout scroll2layout_bionicc=
{
	8,8,    /* 8*8 tiles */
	2048,   /* 2048 tiles */
	4,      /* 4 bits per pixel */
	{ (0x08000*8)+4,0x08000*8,4,0 },
	{ 0,1,2,3, 8,9,10,11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	128   /* every tile takes 128 consecutive bytes */
};

static struct GfxLayout scroll1layout_bionicc=
{
	16,16,  /* 16*16 tiles */
	2048,   /* 2048 tiles */
	4,      /* 4 bits per pixel */
	{ (0x020000*8)+4,0x020000*8,4,0 },
	{
		0,1,2,3, 8,9,10,11,
		(8*4*8)+0,(8*4*8)+1,(8*4*8)+2,(8*4*8)+3,
		(8*4*8)+8,(8*4*8)+9,(8*4*8)+10,(8*4*8)+11
	},
	{
		0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16
	},
	512   /* each tile takes 512 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo_bionicc[] =
{
	{ REGION_GFX1, 0, &vramlayout_bionicc,    768, 64 },	/* colors 768-1023 */
	{ REGION_GFX2, 0, &scroll2layout_bionicc,   0,  4 },	/* colors   0-  63 */
	{ REGION_GFX3, 0, &scroll1layout_bionicc, 256,  4 },	/* colors 256- 319 */
	{ REGION_GFX4, 0, &spritelayout_bionicc,  512, 16 },	/* colors 512- 767 */
	{ -1 }
};


static struct YM2151interface ym2151_interface =
{
	1,                      /* 1 chip */
	3579545,                /* 3.579545 MHz ? */
	{ YM3012_VOL(60,MIXER_PAN_LEFT,60,MIXER_PAN_RIGHT) },
	{ 0 }
};


static struct MachineDriver machine_driver_bionicc =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000, /* ?? MHz ? */
			readmem,writemem,0,0,
			bionicc_interrupt,8
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,  /* 4 Mhz ??? TODO: find real FRQ */
			sound_readmem,sound_writemem,0,0,
			nmi_interrupt,4	/* ??? */
		}
	},
	60, 5000, //DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo_bionicc,
	1024, 1024,	/* but a lot are not used */
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_BUFFERS_SPRITERAM,
	bionicc_eof_callback,
	bionicc_vh_start,
	0,
	bionicc_vh_screenrefresh,

	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
	}
};



ROM_START( bionicc )
	ROM_REGION( 0x40000, REGION_CPU1 )      /* 68000 code */
	ROM_LOAD_EVEN( "tsu_02b.rom",  0x00000, 0x10000, 0xcf965a0a ) /* 68000 code */
	ROM_LOAD_ODD ( "tsu_04b.rom",  0x00000, 0x10000, 0xc9884bfb ) /* 68000 code */
	ROM_LOAD_EVEN( "tsu_03b.rom",  0x20000, 0x10000, 0x4e157ae2 ) /* 68000 code */
	ROM_LOAD_ODD ( "tsu_05b.rom",  0x20000, 0x10000, 0xe66ca0f9 ) /* 68000 code */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "tsu_01b.rom",  0x00000, 0x8000, 0xa9a6cafa )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tsu_08.rom",   0x00000, 0x8000, 0x9bf0b7a2 )	/* VIDEORAM (text layer) tiles */

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tsu_07.rom",   0x00000, 0x8000, 0x9469efa4 )	/* SCROLL2 Layer Tiles */
	ROM_LOAD( "tsu_06.rom",   0x08000, 0x8000, 0x40bf0eb4 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ts_12.rom",    0x00000, 0x8000, 0xe4b4619e )	/* SCROLL1 Layer Tiles */
	ROM_LOAD( "ts_11.rom",    0x08000, 0x8000, 0xab30237a )
	ROM_LOAD( "ts_17.rom",    0x10000, 0x8000, 0xdeb657e4 )
	ROM_LOAD( "ts_16.rom",    0x18000, 0x8000, 0xd363b5f9 )
	ROM_LOAD( "ts_13.rom",    0x20000, 0x8000, 0xa8f5a004 )
	ROM_LOAD( "ts_18.rom",    0x28000, 0x8000, 0x3b36948c )
	ROM_LOAD( "ts_23.rom",    0x30000, 0x8000, 0xbbfbe58a )
	ROM_LOAD( "ts_24.rom",    0x38000, 0x8000, 0xf156e564 )

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tsu_10.rom",   0x00000, 0x8000, 0xf1180d02 )	/* Sprites */
	ROM_LOAD( "tsu_09.rom",   0x08000, 0x8000, 0x6a049292 )
	ROM_LOAD( "tsu_15.rom",   0x10000, 0x8000, 0xea912701 )
	ROM_LOAD( "tsu_14.rom",   0x18000, 0x8000, 0x46b2ad83 )
	ROM_LOAD( "tsu_20.rom",   0x20000, 0x8000, 0x17857ad2 )
	ROM_LOAD( "tsu_19.rom",   0x28000, 0x8000, 0xb5c82722 )
	ROM_LOAD( "tsu_22.rom",   0x30000, 0x8000, 0x5ee1ae6a )
	ROM_LOAD( "tsu_21.rom",   0x38000, 0x8000, 0x98777006 )
ROM_END

ROM_START( bionicc2 )
	ROM_REGION( 0x40000, REGION_CPU1 )      /* 68000 code */
	ROM_LOAD_EVEN( "02",      0x00000, 0x10000, 0xf2528f08 ) /* 68000 code */
	ROM_LOAD_ODD ( "04",      0x00000, 0x10000, 0x38b1c7e4 ) /* 68000 code */
	ROM_LOAD_EVEN( "03",      0x20000, 0x10000, 0x72c3b76f ) /* 68000 code */
	ROM_LOAD_ODD ( "05",      0x20000, 0x10000, 0x70621f83 ) /* 68000 code */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "tsu_01b.rom",  0x00000, 0x8000, 0xa9a6cafa )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tsu_08.rom",   0x00000, 0x8000, 0x9bf0b7a2 )	/* VIDEORAM (text layer) tiles */

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tsu_07.rom",   0x00000, 0x8000, 0x9469efa4 )	/* SCROLL2 Layer Tiles */
	ROM_LOAD( "tsu_06.rom",   0x08000, 0x8000, 0x40bf0eb4 )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ts_12.rom",    0x00000, 0x8000, 0xe4b4619e )	/* SCROLL1 Layer Tiles */
	ROM_LOAD( "ts_11.rom",    0x08000, 0x8000, 0xab30237a )
	ROM_LOAD( "ts_17.rom",    0x10000, 0x8000, 0xdeb657e4 )
	ROM_LOAD( "ts_16.rom",    0x18000, 0x8000, 0xd363b5f9 )
	ROM_LOAD( "ts_13.rom",    0x20000, 0x8000, 0xa8f5a004 )
	ROM_LOAD( "ts_18.rom",    0x28000, 0x8000, 0x3b36948c )
	ROM_LOAD( "ts_23.rom",    0x30000, 0x8000, 0xbbfbe58a )
	ROM_LOAD( "ts_24.rom",    0x38000, 0x8000, 0xf156e564 )

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "tsu_10.rom",   0x00000, 0x8000, 0xf1180d02 )	/* Sprites */
	ROM_LOAD( "tsu_09.rom",   0x08000, 0x8000, 0x6a049292 )
	ROM_LOAD( "tsu_15.rom",   0x10000, 0x8000, 0xea912701 )
	ROM_LOAD( "tsu_14.rom",   0x18000, 0x8000, 0x46b2ad83 )
	ROM_LOAD( "tsu_20.rom",   0x20000, 0x8000, 0x17857ad2 )
	ROM_LOAD( "tsu_19.rom",   0x28000, 0x8000, 0xb5c82722 )
	ROM_LOAD( "tsu_22.rom",   0x30000, 0x8000, 0x5ee1ae6a )
	ROM_LOAD( "tsu_21.rom",   0x38000, 0x8000, 0x98777006 )
ROM_END

ROM_START( topsecrt )
	ROM_REGION( 0x40000, REGION_CPU1 )      /* 68000 code */
	ROM_LOAD_EVEN( "ts_02.rom",  0x00000, 0x10000, 0xb2fe1ddb ) /* 68000 code */
	ROM_LOAD_ODD ( "ts_04.rom",  0x00000, 0x10000, 0x427a003d ) /* 68000 code */
	ROM_LOAD_EVEN( "ts_03.rom",  0x20000, 0x10000, 0x27f04bb6 ) /* 68000 code */
	ROM_LOAD_ODD ( "ts_05.rom",  0x20000, 0x10000, 0xc01547b1 ) /* 68000 code */

	ROM_REGION( 0x10000, REGION_CPU2 ) /* 64k for the audio CPU */
	ROM_LOAD( "ts_01.rom",    0x00000, 0x8000, 0x8ea07917 )

	ROM_REGION( 0x08000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ts_08.rom",    0x00000, 0x8000, 0x96ad379e )	/* VIDEORAM (text layer) tiles */

	ROM_REGION( 0x10000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ts_07.rom",    0x00000, 0x8000, 0x25cdf8b2 )	/* SCROLL2 Layer Tiles */
	ROM_LOAD( "ts_06.rom",    0x08000, 0x8000, 0x314fb12d )

	ROM_REGION( 0x40000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ts_12.rom",    0x00000, 0x8000, 0xe4b4619e )	/* SCROLL1 Layer Tiles */
	ROM_LOAD( "ts_11.rom",    0x08000, 0x8000, 0xab30237a )
	ROM_LOAD( "ts_17.rom",    0x10000, 0x8000, 0xdeb657e4 )
	ROM_LOAD( "ts_16.rom",    0x18000, 0x8000, 0xd363b5f9 )
	ROM_LOAD( "ts_13.rom",    0x20000, 0x8000, 0xa8f5a004 )
	ROM_LOAD( "ts_18.rom",    0x28000, 0x8000, 0x3b36948c )
	ROM_LOAD( "ts_23.rom",    0x30000, 0x8000, 0xbbfbe58a )
	ROM_LOAD( "ts_24.rom",    0x38000, 0x8000, 0xf156e564 )

	ROM_REGION( 0x40000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ts_10.rom",    0x00000, 0x8000, 0xc3587d05 )	/* Sprites */
	ROM_LOAD( "ts_09.rom",    0x08000, 0x8000, 0x6b63eef2 )
	ROM_LOAD( "ts_15.rom",    0x10000, 0x8000, 0xdb8cebb0 )
	ROM_LOAD( "ts_14.rom",    0x18000, 0x8000, 0xe2e41abf )
	ROM_LOAD( "ts_20.rom",    0x20000, 0x8000, 0xbfd1a695 )
	ROM_LOAD( "ts_19.rom",    0x28000, 0x8000, 0x928b669e )
	ROM_LOAD( "ts_22.rom",    0x30000, 0x8000, 0x3fe05d9a )
	ROM_LOAD( "ts_21.rom",    0x38000, 0x8000, 0x27a9bb7c )
ROM_END



GAME( 1987, bionicc,  0,       bionicc, bionicc, 0, ROT0, "Capcom", "Bionic Commando (US set 1)" )
GAME( 1987, bionicc2, bionicc, bionicc, bionicc, 0, ROT0, "Capcom", "Bionic Commando (US set 2)" )
GAME( 1987, topsecrt, bionicc, bionicc, bionicc, 0, ROT0, "Capcom", "Top Secret (Japan)" )

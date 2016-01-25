#include "../vidhrdw/route16.cpp"

/***************************************************************************

 Route 16/Stratovox memory map (preliminary)

 driver by Zsolt Vasvari

 Notes: Route 16 and Stratovox use identical hardware with the following
        exceptions: Stratovox has a DAC for voice.
        Route 16 has the added ability to turn off each bitplane indiviaually.
        This looks like an afterthought, as one of the same bits that control
        the palette selection is doubly utilized as the bitmap enable bit.

 CPU1

 0000-2fff ROM
 4000-43ff Shared RAM
 8000-bfff Video RAM

 I/O Read

 48xx IN0 - DIP Switches
 50xx IN1 - Input Port 1
 58xx IN2 - Input Port 2

 I/O Write

 48xx OUT0 - D0-D4 color select for VRAM 0
             D5    coin counter
 50xx OUT1 - D0-D4 color select for VRAM 1
             D5    VIDEO I/II (Flip Screen)

 I/O Port Write

 6800 AY-8910 Write Port
 6900 AY-8910 Control Port


 CPU2

 0000-1fff ROM
 4000-43ff Shared RAM
 8000-bfff Video RAM

 I/O Write

 2800      DAC output (Stratovox only)

 ***************************************************************************/

#include "driver.h"

extern unsigned char *route16_sharedram;
extern unsigned char *route16_videoram1;
extern unsigned char *route16_videoram2;
extern size_t route16_videoram_size;

void init_route16(void);
void init_route16b(void);
void init_stratvox(void);
void route16_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
int  route16_vh_start(void);
void route16_vh_stop(void);
WRITE_HANDLER( route16_out0_w );
WRITE_HANDLER( route16_out1_w );
WRITE_HANDLER( route16_videoram1_w );
WRITE_HANDLER( route16_videoram2_w );
READ_HANDLER( route16_videoram1_r );
READ_HANDLER( route16_videoram2_r );
WRITE_HANDLER( route16_sharedram_w );
READ_HANDLER( route16_sharedram_r );
void route16_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( stratvox_sn76477_w );

static struct MemoryReadAddress cpu1_readmem[] =
{
	{ 0x0000, 0x2fff, MRA_ROM },
	{ 0x4000, 0x43ff, route16_sharedram_r },
	{ 0x4800, 0x4800, input_port_0_r },
	{ 0x5000, 0x5000, input_port_1_r },
	{ 0x5800, 0x5800, input_port_2_r },
	{ 0x8000, 0xbfff, route16_videoram1_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cpu1_writemem[] =
{
	{ 0x0000, 0x2fff, MWA_ROM },
	{ 0x4000, 0x43ff, route16_sharedram_w, &route16_sharedram },
	{ 0x4800, 0x4800, route16_out0_w },
	{ 0x5000, 0x5000, route16_out1_w },
	{ 0x8000, 0xbfff, route16_videoram1_w, &route16_videoram1, &route16_videoram_size },
	{ 0xc000, 0xc000, MWA_RAM }, // Stratvox has an off by one error
                                 // when clearing the screen
	{ -1 }  /* end of table */
};


static struct IOWritePort cpu1_writeport[] =
{
	{ 0x6800, 0x6800, AY8910_write_port_0_w },
	{ 0x6900, 0x6900, AY8910_control_port_0_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress cpu2_readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x43ff, route16_sharedram_r },
	{ 0x8000, 0xbfff, route16_videoram2_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress cpu2_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x2800, 0x2800, DAC_0_data_w }, // Not used by Route 16
	{ 0x4000, 0x43ff, route16_sharedram_w },
	{ 0x8000, 0xbfff, route16_videoram2_w, &route16_videoram2 },
	{ 0xc000, 0xc1ff, MWA_NOP }, // Route 16 sometimes writes outside of
	{ -1 }  /* end of table */   // the video ram. (Probably a bug)
};


INPUT_PORTS_START( route16 )
	PORT_START      /* DSW 1 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) ) // Doesn't seem to
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )                    // be referenced
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) ) // Doesn't seem to
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )                    // be referenced
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
//	PORT_DIPSETTING(    0x18, DEF_STR( 2C_1C ) ) // Same as 0x08
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* Input Port 1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x40, IP_ACTIVE_HIGH )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* Input Port 2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_4WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )
INPUT_PORTS_END



INPUT_PORTS_START( stratvox )
	PORT_START      /* IN0 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x02, 0x00, "Replenish Astronouts" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x0c, 0x00, "2 Attackers At Wave" )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_DIPNAME( 0x10, 0x00, "Astronauts Kidnapped" )
	PORT_DIPSETTING(    0x10, "Less Often" )
	PORT_DIPSETTING(    0x00, "More Often" )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Demo Voices" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_COIN1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY | IPF_PLAYER2 )
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_START1 )
INPUT_PORTS_END


static struct AY8910interface ay8910_interface =
{
	1,	/* 1 chip */
	10000000/8,     /* 10Mhz / 8 = 1.25Mhz */
	{ 50 },
	{ 0 },
	{ 0 },
	{ stratvox_sn76477_w },  /* SN76477 commands (not used in Route 16?) */
	{ 0 }
};


static struct SN76477interface sn76477_interface =
{
	1,	/* 1 chip */
	{ 50 },  /* mixing level   pin description		 */
	{ RES_K( 47)   },		/*	4  noise_res		 */
	{ RES_K(150)   },		/*	5  filter_res		 */
	{ CAP_U(0.001) },		/*	6  filter_cap		 */
	{ RES_M(3.3)   },		/*	7  decay_res		 */
	{ CAP_U(1.0)   },		/*	8  attack_decay_cap  */
	{ RES_K(4.7)   },		/* 10  attack_res		 */
	{ RES_K(200)   },		/* 11  amplitude_res	 */
	{ RES_K( 55)   },		/* 12  feedback_res 	 */
	{ 5.0*2/(2+10) },		/* 16  vco_voltage		 */
	{ CAP_U(0.022) },		/* 17  vco_cap			 */
	{ RES_K(100)   },		/* 18  vco_res			 */
	{ 5.0		   },		/* 19  pitch_voltage	 */
	{ RES_K( 75)   },		/* 20  slf_res			 */
	{ CAP_U(1.0)   },		/* 21  slf_cap			 */
	{ CAP_U(2.2)   },		/* 23  oneshot_cap		 */
	{ RES_K(4.7)   }		/* 24  oneshot_res		 */
};


static struct DACinterface dac_interface =
{
	1,
	{ 50 }
};


#define MACHINE_DRIVER(GAMENAME, AUDIO_INTERFACES)   		\
															\
static struct MachineDriver machine_driver_##GAMENAME =		\
{															\
	/* basic machine hardware */							\
	{														\
		{													\
			CPU_Z80 | CPU_16BIT_PORT,						\
			2500000,	/* 10Mhz / 4 = 2.5Mhz */			\
			cpu1_readmem,cpu1_writemem,0,cpu1_writeport,	\
			interrupt,1										\
		},													\
		{													\
			CPU_Z80,										\
			2500000,	/* 10Mhz / 4 = 2.5Mhz */			\
			cpu2_readmem,cpu2_writemem,0,0,					\
			ignore_interrupt,0								\
		}													\
	},														\
	57, DEFAULT_REAL_60HZ_VBLANK_DURATION,       /* frames per second, vblank duration */ \
	1,														\
	0,														\
															\
	/* video hardware */									\
	256, 256, { 0, 256-1, 0, 256-1 },						\
	0,														\
	8, 0,													\
	route16_vh_convert_color_prom,							\
															\
	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE, \
	0,														\
	route16_vh_start,										\
	route16_vh_stop,										\
	route16_vh_screenrefresh,								\
															\
	/* sound hardware */									\
	0,0,0,0,												\
	{														\
		AUDIO_INTERFACES									\
	}														\
};

#define ROUTE16_AUDIO_INTERFACE  \
		{						 \
			SOUND_AY8910,		 \
			&ay8910_interface	 \
		}

#define STRATVOX_AUDIO_INTERFACE \
		{						 \
			SOUND_AY8910,		 \
			&ay8910_interface	 \
		},						 \
		{						 \
			SOUND_SN76477,		 \
			&sn76477_interface	 \
        },                       \
        {                        \
			SOUND_DAC,			 \
			&dac_interface		 \
		}

MACHINE_DRIVER(route16,  ROUTE16_AUDIO_INTERFACE )
MACHINE_DRIVER(stratvox, STRATVOX_AUDIO_INTERFACE)

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( route16 )
	ROM_REGION( 0x10000, REGION_CPU1 )  // 64k for the first CPU
	ROM_LOAD( "route16.a0",   0x0000, 0x0800, 0x8f9101bd )
	ROM_LOAD( "route16.a1",   0x0800, 0x0800, 0x389bc077 )
	ROM_LOAD( "route16.a2",   0x1000, 0x0800, 0x1065a468 )
	ROM_LOAD( "route16.a3",   0x1800, 0x0800, 0x0b1987f3 )
	ROM_LOAD( "route16.a4",   0x2000, 0x0800, 0xf67d853a )
	ROM_LOAD( "route16.a5",   0x2800, 0x0800, 0xd85cf758 )

	ROM_REGION( 0x10000, REGION_CPU2 )  // 64k for the second CPU
	ROM_LOAD( "route16.b0",   0x0000, 0x0800, 0x0f9588a7 )
	ROM_LOAD( "route16.b1",   0x0800, 0x0800, 0x2b326cf9 )
	ROM_LOAD( "route16.b2",   0x1000, 0x0800, 0x529cad13 )
	ROM_LOAD( "route16.b3",   0x1800, 0x0800, 0x3bd8b899 )

	ROM_REGION( 0x0200, REGION_PROMS )
	/* The upper 128 bytes are 0's, used by the hardware to blank the display */
	ROM_LOAD( "pr09",         0x0000, 0x0100, 0x08793ef7 ) /* top bitmap */
	ROM_LOAD( "pr10",         0x0100, 0x0100, 0x08793ef7 ) /* bottom bitmap */
ROM_END

ROM_START( route16b )
	ROM_REGION( 0x10000, REGION_CPU1 )  // 64k for the first CPU
	ROM_LOAD( "rt16.0",       0x0000, 0x0800, 0xb1f0f636 )
	ROM_LOAD( "rt16.1",       0x0800, 0x0800, 0x3ec52fe5 )
	ROM_LOAD( "rt16.2",       0x1000, 0x0800, 0xa8e92871 )
	ROM_LOAD( "rt16.3",       0x1800, 0x0800, 0xa0fc9fc5 )
	ROM_LOAD( "rt16.4",       0x2000, 0x0800, 0x6dcaf8c4 )
	ROM_LOAD( "rt16.5",       0x2800, 0x0800, 0x63d7b05b )

	ROM_REGION( 0x10000, REGION_CPU2 )  // 64k for the second CPU
	ROM_LOAD( "rt16.6",       0x0000, 0x0800, 0xfef605f3 )
	ROM_LOAD( "rt16.7",       0x0800, 0x0800, 0xd0d6c189 )
	ROM_LOAD( "rt16.8",       0x1000, 0x0800, 0xdefc5797 )
	ROM_LOAD( "rt16.9",       0x1800, 0x0800, 0x88d94a66 )

	ROM_REGION( 0x0200, REGION_PROMS )
	/* The upper 128 bytes are 0's, used by the hardware to blank the display */
	ROM_LOAD( "pr09",         0x0000, 0x0100, 0x08793ef7 ) /* top bitmap */
	ROM_LOAD( "pr10",         0x0100, 0x0100, 0x08793ef7 ) /* bottom bitmap */
ROM_END

ROM_START( stratvox )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "ls01.bin",     0x0000, 0x0800, 0xbf4d582e )
	ROM_LOAD( "ls02.bin",     0x0800, 0x0800, 0x16739dd4 )
	ROM_LOAD( "ls03.bin",     0x1000, 0x0800, 0x083c28de )
	ROM_LOAD( "ls04.bin",     0x1800, 0x0800, 0xb0927e3b )
	ROM_LOAD( "ls05.bin",     0x2000, 0x0800, 0xccd25c4e )
	ROM_LOAD( "ls06.bin",     0x2800, 0x0800, 0x07a907a7 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the second CPU */
	ROM_LOAD( "ls07.bin",     0x0000, 0x0800, 0x4d333985 )
	ROM_LOAD( "ls08.bin",     0x0800, 0x0800, 0x35b753fc )

	ROM_REGION( 0x0200, REGION_PROMS )
	/* The upper 128 bytes are 0's, used by the hardware to blank the display */
	ROM_LOAD( "pr09",         0x0000, 0x0100, 0x08793ef7 ) /* top bitmap */
	ROM_LOAD( "pr10",         0x0100, 0x0100, 0x08793ef7 ) /* bottom bitmap */
ROM_END

ROM_START( stratvxb )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "ls01.bin",     0x0000, 0x0800, 0xbf4d582e )
	ROM_LOAD( "ls02.bin",     0x0800, 0x0800, 0x16739dd4 )
	ROM_LOAD( "ls03.bin",     0x1000, 0x0800, 0x083c28de )
	ROM_LOAD( "ls04.bin",     0x1800, 0x0800, 0xb0927e3b )
	ROM_LOAD( "ls05.bin",     0x2000, 0x0800, 0xccd25c4e )
	ROM_LOAD( "a5-1",         0x2800, 0x0800, 0x70c4ef8e )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the second CPU */
	ROM_LOAD( "ls07.bin",     0x0000, 0x0800, 0x4d333985 )
	ROM_LOAD( "ls08.bin",     0x0800, 0x0800, 0x35b753fc )

	ROM_REGION( 0x0200, REGION_PROMS )
	/* The upper 128 bytes are 0's, used by the hardware to blank the display */
	ROM_LOAD( "pr09",         0x0000, 0x0100, 0x08793ef7 ) /* top bitmap */
	ROM_LOAD( "pr10",         0x0100, 0x0100, 0x08793ef7 ) /* bottom bitmap */
ROM_END

ROM_START( speakres )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "speakres.1",   0x0000, 0x0800, 0x6026e4ea )
	ROM_LOAD( "speakres.2",   0x0800, 0x0800, 0x93f0d4da )
	ROM_LOAD( "speakres.3",   0x1000, 0x0800, 0xa3874304 )
	ROM_LOAD( "speakres.4",   0x1800, 0x0800, 0xf484be3a )
	ROM_LOAD( "speakres.5",   0x2000, 0x0800, 0x61b12a67 )
	ROM_LOAD( "speakres.6",   0x2800, 0x0800, 0x220e0ab2 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the second CPU */
	ROM_LOAD( "speakres.7",   0x0000, 0x0800, 0xd417be13 )
	ROM_LOAD( "speakres.8",   0x0800, 0x0800, 0x52485d60 )

	ROM_REGION( 0x0200, REGION_PROMS )
	/* The upper 128 bytes are 0's, used by the hardware to blank the display */
	ROM_LOAD( "pr09",         0x0000, 0x0100, 0x08793ef7 ) /* top bitmap */
	ROM_LOAD( "pr10",         0x0100, 0x0100, 0x08793ef7 ) /* bottom bitmap */
ROM_END



GAME( 1981, route16,  0,        route16,  route16,  route16,  ROT270, "Tehkan/Sun (Centuri license)", "Route 16" )
GAME( 1981, route16b, route16,  route16,  route16,  route16b, ROT270, "bootleg", "Route 16 (bootleg)" )
GAME( 1980, stratvox, 0,        stratvox, stratvox, stratvox, ROT270, "Taito", "Stratovox" )
GAME( 1980, stratvxb, stratvox, stratvox, stratvox, stratvox, ROT270, "bootleg", "Stratovox (bootleg)" )
GAME( ????, speakres, stratvox, stratvox, stratvox, stratvox, ROT270, "<unknown>", "Speak & Rescue" )

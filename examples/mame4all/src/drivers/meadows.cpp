#include "../sndhrdw/meadows.cpp"
#include "../vidhrdw/meadows.cpp"

/***************************************************************************
	meadows.c
	Machine driver
	Dead Eye, Gypsy Juggler

	J. Buchmueller, June '98

    ***********************************************
    memory map CPU #0 (preliminary)
	***********************************************

	0000..0bff	ROM part one

	0c00..0c03	H/W input ports
	-----------------------------------------------
			0 R control buttons
				D0		button 1
				D1		start 2 player game

			1 R analog control
				D0-D7	center is 0x7f
			2 R horizontal sync divider chain
				D7 9.765kHz ... D0 2.5MHz
			3 R dip switch settings
				D0-D2	select 2 to 9 coins
				D3-D4	Coins per play D3 D4
						1 coin	1 play	0  0
						2 coins 1 play	1  0
						1 coin	2 plays 0  1
						free play		1  1
				D5		Attact music 0:off, 1:on
				D6-D7	Extended play  D6 D7
						none			0  0
						5000 pts		1  0
						15000 pts		0  1
						35000 pts		1  1

	0d00-0d0f	H/W sprites
	-----------------------------------------------
            0 W D0-D7    sprite 0 horz
			1 W D0-D7	 sprite 1 horz
			2 W D0-D7	 sprite 2 horz
			3 W D0-D7	 sprite 3 horz
			4 W D0-D7	 sprite 0 vert
			5 W D0-D7	 sprite 2 vert
			6 W D0-D7	 sprite 3 vert
			7 W D0-D7	 sprite 4 vert
			8 W D0-D7	 sprite 0 select
				D0-D3	 sprite #
				D4		 prom (not sure)
				D5		 flip x
			9 W 		 sprite 1 select
				D0-D3	 sprite #
				D4		 prom (not sure)
				D5		 flip x
			a W 		 sprite 2 select
				D0-D3	 sprite #
				D4		 prom (not sure)
				D5		 flip x
			b W 		 sprite 3 select
				D0-D3	 sprite #
				D4		 prom (not sure)
				D5		 flip x

	0e00-0eff	RAM

	1000-1bff	ROM 	part two

	1c00-1fff	RAM 	video buffer

	***********************************************
    memory map CPU #1 (preliminary)
	***********************************************

	0000..0bff	ROM part one

	0c00..0c03	H/W input ports
	-----------------------------------------------
			0 R sound command from CPU #0
				D0-D7	8 different sounds ???

			1 R ???
			2 R ???
			3 R ???

            0 W D0-D7   DAC
            1 W D0-D3   preset for counter, clk is 5 MHz / 256
				D4-D7	volume bits 0 .. 3 (bit 4 is CPU #1 flag output)
            2 W D0-D7   preset for counter, clk is 5 MHz / 32
            3 W D0      divide c02 counter by 0: 2, 1: 4
				D1		sound enable for c02 tone generator
                D2      sound enable for DAC
				D3		sound enable for c01 tone generator

	0e00-0eff	RAM




***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/*************************************************************/
/*                                                           */
/* Externals                                                 */
/*                                                           */
/*************************************************************/
int deadeye_vh_start(void);
int gypsyjug_vh_start(void);
void meadows_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
WRITE_HANDLER( meadows_videoram_w );
WRITE_HANDLER( meadows_sprite_w );

int meadows_sh_start(const struct MachineSound *msound);
void meadows_sh_stop(void);
void meadows_sh_dac_w(int data);
void meadows_sh_update(void);
extern unsigned char meadows_0c00;
extern unsigned char meadows_0c01;
extern unsigned char meadows_0c02;
extern unsigned char meadows_0c03;


/*************************************************************/
/*                                                           */
/* Statics                                                   */
/*                                                           */
/*************************************************************/
static  unsigned char flip_bits[0x100] = {
	0x00,0x80,0x40,0xc0,0x20,0xa0,0x60,0xe0,0x10,0x90,0x50,0xd0,0x30,0xb0,0x70,0xf0,
	0x08,0x88,0x48,0xc8,0x28,0xa8,0x68,0xe8,0x18,0x98,0x58,0xd8,0x38,0xb8,0x78,0xf8,
	0x04,0x84,0x44,0xc4,0x24,0xa4,0x64,0xe4,0x14,0x94,0x54,0xd4,0x34,0xb4,0x74,0xf4,
	0x0c,0x8c,0x4c,0xcc,0x2c,0xac,0x6c,0xec,0x1c,0x9c,0x5c,0xdc,0x3c,0xbc,0x7c,0xfc,
	0x02,0x82,0x42,0xc2,0x22,0xa2,0x62,0xe2,0x12,0x92,0x52,0xd2,0x32,0xb2,0x72,0xf2,
	0x0a,0x8a,0x4a,0xca,0x2a,0xaa,0x6a,0xea,0x1a,0x9a,0x5a,0xda,0x3a,0xba,0x7a,0xfa,
	0x06,0x86,0x46,0xc6,0x26,0xa6,0x66,0xe6,0x16,0x96,0x56,0xd6,0x36,0xb6,0x76,0xf6,
	0x0e,0x8e,0x4e,0xce,0x2e,0xae,0x6e,0xee,0x1e,0x9e,0x5e,0xde,0x3e,0xbe,0x7e,0xfe,
	0x01,0x81,0x41,0xc1,0x21,0xa1,0x61,0xe1,0x11,0x91,0x51,0xd1,0x31,0xb1,0x71,0xf1,
	0x09,0x89,0x49,0xc9,0x29,0xa9,0x69,0xe9,0x19,0x99,0x59,0xd9,0x39,0xb9,0x79,0xf9,
	0x05,0x85,0x45,0xc5,0x25,0xa5,0x65,0xe5,0x15,0x95,0x55,0xd5,0x35,0xb5,0x75,0xf5,
	0x0d,0x8d,0x4d,0xcd,0x2d,0xad,0x6d,0xed,0x1d,0x9d,0x5d,0xdd,0x3d,0xbd,0x7d,0xfd,
	0x03,0x83,0x43,0xc3,0x23,0xa3,0x63,0xe3,0x13,0x93,0x53,0xd3,0x33,0xb3,0x73,0xf3,
	0x0b,0x8b,0x4b,0xcb,0x2b,0xab,0x6b,0xeb,0x1b,0x9b,0x5b,0xdb,0x3b,0xbb,0x7b,0xfb,
	0x07,0x87,0x47,0xc7,0x27,0xa7,0x67,0xe7,0x17,0x97,0x57,0xd7,0x37,0xb7,0x77,0xf7,
	0x0f,0x8f,0x4f,0xcf,0x2f,0xaf,0x6f,0xef,0x1f,0x9f,0x5f,0xdf,0x3f,0xbf,0x7f,0xff,
};
static int cycles_at_vsync = 0;

/*************************************************************/
/*                                                           */
/* Hardware read/write from the main CPU                     */
/*                                                           */
/*************************************************************/
READ_HANDLER( meadows_hardware_r )
{
	switch( offset ) {
        case 0: /* buttons */
            return input_port_0_r(0);
        case 1: /* AD stick */
            return input_port_1_r(0);
        case 2: /* horizontal sync divider chain */
			return flip_bits[(cycles_currently_ran() - cycles_at_vsync) & 0xff];
        case 3: /* dip switches */
            return input_port_2_r(0);
    }
    return 0;
}

WRITE_HANDLER( meadows_hardware_w )
{
	switch( offset ) {
		case 0:
			if (meadows_0c00 == data)
				break;
			//logerror("meadows_hardware_w %d $%02x\n", offset, data);
			meadows_0c00 = data;
            break;
		case 1:
			//logerror("meadows_hardware_w %d $%02x\n", offset, data);
            break;
        case 2:
			//logerror("meadows_hardware_w %d $%02x\n", offset, data);
            break;
		case 3:
//			S2650_Clear_Pending_Interrupts();
			break;
	}
}

/*************************************************************/
/*                                                           */
/* Interrupt for the main cpu                                */
/*                                                           */
/*************************************************************/
int     meadows_interrupt(void)
{
static  int sense_state = 0;
static	int coin1_state = 0;
	/* preserve the actual cycle count */
    cycles_at_vsync = cycles_currently_ran();
    /* fake something toggling the sense input line of the S2650 */
	sense_state ^= 1;
	cpu_set_irq_line( 0, 1, (sense_state) ? ASSERT_LINE : CLEAR_LINE);
	if( input_port_3_r(0) & 0x01 ) {
		if( !coin1_state ) {
			coin1_state = 1;
			/* S2650 interrupt vector */
			cpu_irq_line_vector_w(0,0,0x82);
			cpu_set_irq_line(0,0,PULSE_LINE);
		}
	}
	coin1_state = 0;
	return ignore_interrupt();
}

/*************************************************************/
/*                                                           */
/* Hardware read/write for the sound CPU                     */
/*                                                           */
/*************************************************************/
static WRITE_HANDLER( sound_hardware_w )
{
	switch( offset & 3 ) {
		case 0: /* DAC */
			meadows_sh_dac_w(data ^ 0xff);
            break;
		case 1: /* counter clk 5 MHz / 256 */
			if (data == meadows_0c01)
				break;
			//logerror("sound_w ctr1 preset $%x amp %d\n", data & 15, data >> 4);
			meadows_0c01 = data;
			meadows_sh_update();
			break;
		case 2: /* counter clk 5 MHz / 32 (/ 2 or / 4) */
			if (data == meadows_0c02)
                break;
			//logerror("sound_w ctr2 preset $%02x\n", data);
			meadows_0c02 = data;
			meadows_sh_update();
            break;
		case 3: /* sound enable */
			if (data == meadows_0c03)
                break;
			//logerror("sound_w enable ctr2/2:%d ctr2:%d dac:%d ctr1:%d\n", data&1, (data>>1)&1, (data>>2)&1, (data>>3)&1);
			meadows_0c03 = data;
			meadows_sh_update();
            break;
	}
}

static READ_HANDLER( sound_hardware_r )
{
	int data = 0;

	switch( offset ) {
		case 0:
			data = meadows_0c00;
#if VERBOSE
            {
				static int last_data = 0;
				if (data != last_data) {
					last_data = data;
					logerror("sound_r %d $%02x\n", offset, data);
				}
			}
#endif
            break;
		case 1: break;
		case 2: break;
		case 3: break;
	}
    return data;
}

/*************************************************************/
/*                                                           */
/* Interrupt for the sound cpu                               */
/*                                                           */
/*************************************************************/
static int sound_interrupt(void)
{
	static	int sense_state = 0;

    /* fake something toggling the sense input line of the S2650 */
	sense_state ^= 1;
	cpu_set_irq_line( 1, 1, (sense_state) ? ASSERT_LINE : CLEAR_LINE );
	return ignore_interrupt();
}

/*************************************************************/
/*                                                           */
/* Memory layout                                             */
/*                                                           */
/*************************************************************/
static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x0bff, MWA_ROM },
	{ 0x0c00, 0x0c03, meadows_hardware_w },
	{ 0x0d00, 0x0d0f, meadows_sprite_w },
	{ 0x0e00, 0x0eff, MWA_RAM },
	{ 0x1000, 0x1bff, MWA_ROM },
	{ 0x1c00, 0x1fff, meadows_videoram_w, &videoram, &videoram_size },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x0bff, MRA_ROM },
	{ 0x0c00, 0x0c03, meadows_hardware_r },
	{ 0x0e00, 0x0eff, MRA_RAM },
	{ 0x1000, 0x1bff, MRA_ROM },
	{ 0x1c00, 0x1fff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x0bff, MWA_ROM },
	{ 0x0c00, 0x0c03, sound_hardware_w },
	{ 0x0e00, 0x0eff, MWA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x0bff, MRA_ROM },
	{ 0x0c00, 0x0c03, sound_hardware_r },
	{ 0x0e00, 0x0eff, MRA_RAM },
	{ -1 }	/* end of table */
};

INPUT_PORTS_START( meadows )
	PORT_START		/* IN0 buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START1  )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START		/* IN1 control 1 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X, 100, 10, 0x10, 0xf0 )

	PORT_START		/* IN2 dip switch */
	PORT_DIPNAME( 0x07, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x04, "6" )
	PORT_DIPSETTING(    0x05, "7" )
	PORT_DIPSETTING(    0x06, "8" )
	PORT_DIPSETTING(    0x07, "9" )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x20, 0x20, "Demo Sounds?" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x40, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x40, "5000")
	PORT_DIPSETTING(    0x80, "15000")
	PORT_DIPSETTING(    0xc0, "35000")
	PORT_DIPSETTING(    0x00, "None")

	PORT_START		/* FAKE coinage */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x8e, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

static struct GfxLayout charlayout =
{
	8,8,							/* 8*8 characters */
	128,							/* 128 characters ? */
	1,								/* 1 bit per pixel */
	{ 0 },							/* no bitplanes */
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, 	/* pretty straight layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 							/* every char takes 8 bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,							/* 16*16 sprites ?	*/
	32, 							/* 32 sprites  */
	1,								/* 1 bits per pixel */
	{ 0 },							/* 1 bitplane */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	8, 9,10,11,12,13,14,15 },	  /* pretty straight layout */
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
	8*16, 9*16,10*16,11*16,12*16,13*16,14*16,15*16 },
	16*2*8							/* every sprite takes 32 bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,	 0, 4 },		/* character generator */
	{ REGION_GFX2, 0, &spritelayout, 0, 4 },		/* sprite prom 1 */
	{ REGION_GFX3, 0, &spritelayout, 0, 4 },		/* sprite prom 2 */
	{ REGION_GFX4, 0, &spritelayout, 0, 4 },		/* sprite prom 3 (unused) */
	{ REGION_GFX5, 0, &spritelayout, 0, 4 },		/* sprite prom 4 (unused) */
	{ -1 } /* end of array */
};

static unsigned char palette[] =
{
	0x00,0x00,0x00, /* BLACK */
	0xff,0xff,0xff, /* WHITE */
};

#define ARTWORK_COLORS (2 + 32768)

static unsigned short colortable[ARTWORK_COLORS] =
{
	0,0,
	0,1,
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}


static struct DACinterface dac_interface =
{
	1,
	{ 100 }
};

static struct CustomSound_interface custom_interface =
{
	meadows_sh_start,
	meadows_sh_stop,
	0
};



static struct MachineDriver machine_driver_deadeye =
{
	/* basic machine hardware */
	{
		{
			CPU_S2650,
			625000, 	/* 5MHz / 8 = 625 kHz */
			readmem,writemem,0,0,
			meadows_interrupt,1 	/* one interrupt per frame!? */
		},
		{
			CPU_S2650 | CPU_AUDIO_CPU,
			625000, 	/* 5MHz / 8 = 625 kHz */
			sound_readmem,sound_writemem,
			0,0,
			0,0,
			sound_interrupt,38	/* 5000000/131072 interrupts per frame */
        }
    },
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10, 									/* dual CPU; interleave them */
	0,

	/* video hardware */
	32*8, 30*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	ARTWORK_COLORS,ARTWORK_COLORS,		/* Leave extra colors for the overlay */
	init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
    0,
	deadeye_vh_start,
	generic_vh_stop,
	meadows_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		},
		{
			SOUND_CUSTOM,
			&custom_interface
		}
    }
};

static struct MachineDriver machine_driver_gypsyjug =
{
	/* basic machine hardware */
	{
		{
			CPU_S2650,
			625000, 	/* 5MHz / 8 = 625 kHz */
			readmem,writemem,0,0,
			meadows_interrupt,1 	/* one interrupt per frame!? */
		},
		{
			CPU_S2650 | CPU_AUDIO_CPU,
			625000, 	/* 5MHz / 8 = 625 kHz */
			sound_readmem,sound_writemem,
			0,0,
			0,0,
			sound_interrupt,38	/* 5000000/131072 interrupts per frame */
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10, 									/* dual CPU; interleave them */
	0,

	/* video hardware */
	32*8, 30*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	gfxdecodeinfo,
	ARTWORK_COLORS,ARTWORK_COLORS,		/* Leave extra colors for the overlay */
	init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY | VIDEO_MODIFIES_PALETTE,
    0,
	gypsyjug_vh_start,
	generic_vh_stop,
	meadows_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&dac_interface
		},
		{
			SOUND_CUSTOM,
			&custom_interface
		}
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( deadeye )
	ROM_REGION( 0x08000, REGION_CPU1 ) 	/* 32K for code */
	ROM_LOAD( "de1.8h",       0x0000, 0x0400, 0xbd09e4dc )
	ROM_LOAD( "de2.9h",       0x0400, 0x0400, 0xb89edec3 )
	ROM_LOAD( "de3.10h",      0x0800, 0x0400, 0xacf24438 )
	ROM_LOAD( "de4.11h",      0x1000, 0x0400, 0x8b68f792 )
	ROM_LOAD( "de5.12h",      0x1400, 0x0400, 0x7bdb535c )
	ROM_LOAD( "de6.13h",      0x1800, 0x0400, 0x847f9467 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "de_char.15e",  0x0000, 0x0400, 0xb032bd8d )

	ROM_REGION( 0x0400, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "de_mov1.5a",   0x0000, 0x0400, 0xc046b4c6 )

	ROM_REGION( 0x0400, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "de_mov2.13a",  0x0000, 0x0400, 0xb89c5df9 )

	ROM_REGION( 0x0400, REGION_GFX4 | REGIONFLAG_DISPOSE )
	/* empty */
	ROM_REGION( 0x0400, REGION_GFX5 | REGIONFLAG_DISPOSE )
	/* empty */

	ROM_REGION( 0x08000, REGION_CPU2 ) 	/* 32K for code for the sound cpu */
	ROM_LOAD( "de_snd",       0x0000, 0x0400, 0xc10a1b1a )
ROM_END

ROM_START( gypsyjug )
	ROM_REGION( 0x08000, REGION_CPU1 ) 	/* 32K for code */
	ROM_LOAD( "gj.1b",        0x0000, 0x0400, 0xf6a71d9f )
	ROM_LOAD( "gj.2b",        0x0400, 0x0400, 0x94c14455 )
	ROM_LOAD( "gj.3b",        0x0800, 0x0400, 0x87ee0490 )
	ROM_LOAD( "gj.4b",        0x1000, 0x0400, 0xdca519c8 )
	ROM_LOAD( "gj.5b",        0x1400, 0x0400, 0x7d83f9d0 )

	ROM_REGION( 0x0400, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gj.e15",       0x0000, 0x0400, 0xadb25e13 )

	ROM_REGION( 0x0400, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "gj.a",         0x0000, 0x0400, 0xd3725193 )

	ROM_REGION( 0x0400, REGION_GFX3 | REGIONFLAG_DISPOSE )
	/* empty (copied from 2) */

	ROM_REGION( 0x0400, REGION_GFX4 | REGIONFLAG_DISPOSE )
	/* empty (filled with fake data) */

	ROM_REGION( 0x0400, REGION_GFX5 | REGIONFLAG_DISPOSE )
	/* empty (filled with fake data) */

	ROM_REGION( 0x08000, REGION_CPU2 ) 	/* 32K for code for the sound cpu */
	ROM_LOAD( "gj.a4s",       0x0000, 0x0400, 0x17a116bc )
	ROM_LOAD( "gj.a5s",       0x0400, 0x0400, 0xfc23ae09 )
	ROM_LOAD( "gj.a6s",       0x0800, 0x0400, 0x9e7bd71e )
ROM_END



/* A fake for the missing ball sprites #3 and #4 */
static void init_gypsyjug(void)
{
int i;
static unsigned char ball[16*2] = {
	0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	0x01,0x80, 0x03,0xc0, 0x03,0xc0, 0x01,0x80};

	memcpy(memory_region(REGION_GFX3),memory_region(REGION_GFX2),memory_region_length(REGION_GFX3));

	for (i = 0; i < memory_region_length(REGION_GFX4); i += 16*2)
	{
		memcpy(memory_region(REGION_GFX4) + i, ball, sizeof(ball));
		memcpy(memory_region(REGION_GFX5) + i, ball, sizeof(ball));
	}
}



GAME( 1978, deadeye,  0, deadeye,  meadows, 0,        ROT0, "Meadows", "Dead Eye" )
GAME( 1978, gypsyjug, 0, gypsyjug, meadows, gypsyjug, ROT0, "Meadows", "Gypsy Juggler" )

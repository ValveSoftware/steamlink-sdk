#include "../vidhrdw/crbaloon.cpp"

/***************************************************************************

Crazy Balloon memory map (preliminary)

0000-2fff ROM
4000-43ff RAM
4800-4bff Video RAM
5000-53ff Color RAM

I/O:

read:
00        dsw
01        joystick
02        bit 0-3 from chip PC3259 (bit 3 is the sprite/char collision detection)
          bit 4-7 dsw
03        bit 0 dsw
          bit 1 high score name reset
		  bit 2 service
		  bit 3 tilt
		  bit 4-7 from chip PC3092; coin inputs & start buttons
06-0a-0e  mirror addresses for 02; address lines 2 and 3 go to the PC3256 chip
          so they probably alter its output, while the dsw bits (4-7) stay the same.

write:
01        ?
02        bit 0-3 sprite code bit 4-7 sprite color
03        sprite X pos
04        sprite Y pos
05        music?? to a counter?
06        sound
          bit 0 IRQ enable/acknowledge
          bit 1 sound enable
          bit 2 sound related (to amplifier)
          bit 3 explosion (to 76477)
          bit 4 breath (to 76477)
          bit 5 appear (to 76477)
          bit 6 sound related (to 555)
          bit 7 to chip PC3259
07        to chip PC3092 (bits 0-3)
08        to chip PC3092 (bits 0-3)
          bit 0 seems to be flip screen
          bit 1 might enable coin input
09        to chip PC3092 (bits 0-3)
0a        to chip PC3092 (bits 0-3)
0b        to chip PC3092 (bits 0-3)
0c        MSK (to chip PC3259)
0d        CC (not used)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"


WRITE_HANDLER( crbaloon_spritectrl_w );
WRITE_HANDLER( crbaloon_flipscreen_w );
void crbaloon_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void crbaloon_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


int val06,val08,val0a;

static void crbaloon_machine_init(void)
{
	/* MIXER A = 0, MIXER C = 1 */
	SN76477_mixer_a_w(0, 0);
	SN76477_mixer_c_w(0, 1);
	/* ENVELOPE is constant: pin1 = hi, pin 28 = lo */
	SN76477_envelope_w(0, 1);
    /* fake: pulse the enable line to get rid of the constant noise */
    SN76477_enable_w(0, 1);
    SN76477_enable_w(0, 0);
}

WRITE_HANDLER( crbaloon_06_w )
{
	val06 = data;

	interrupt_enable_w(offset,data & 1);

	/* SOUND STOP high? */
    if( data & 0x02 )
	{

		if( data & 0x08 )
		{
			/* enable is connected to EXPLOSION */
			SN76477_enable_w(0, 1);
		}
		else
		{
			SN76477_enable_w(0, 0);
		}
		if( data & 0x10 )
		{
			/* BREATH changes slf_res to 10k (middle of two 10k resistors) */
			SN76477_set_slf_res(0, RES_K(10));
			/* it also puts a tantal capacitor agains GND on the output,
			   but this section of the schematics is not readable. */
		}
		else
		{
			SN76477_set_slf_res(0, RES_K(20));
		}

		if( data & 0x20 )
		{
			/* APPEAR is connected to MIXER B */
			SN76477_mixer_b_w(0, 1);
		}
		else
		{
			SN76477_mixer_b_w(0, 4);
		}

		/* constant: pin1 = hi, pin 28 = lo */
		SN76477_envelope_w(0, 1);
	}
}

WRITE_HANDLER( crbaloon_08_w )
{
	val08 = data;

	crbaloon_flipscreen_w(offset,data & 1);
}

WRITE_HANDLER( crbaloon_0a_w )
{
	val0a = data;
}

READ_HANDLER( crbaloon_IN2_r )
{
	extern int crbaloon_collision;

	if (crbaloon_collision != 0)
	{
		return (input_port_2_r(0) & 0xf0) | 0x08;
    }

	/* the following is needed for the game to boot up */
	if (val06 & 0x80)
	{
//logerror("PC %04x: %02x high\n",cpu_get_pc(),offset);
		return (input_port_2_r(0) & 0xf0) | 0x07;
	}
	else
	{
//logerror("PC %04x: %02x low\n",cpu_get_pc(),offset);
		return (input_port_2_r(0) & 0xf0) | 0x07;
	}
}

READ_HANDLER( crbaloon_IN3_r )
{
	if (val08 & 0x02)
		/* enable coin & start input? Wild guess!!! */
		return input_port_3_r(0);

	/* the following is needed for the game to boot up */
	if (val0a & 0x01)
	{
//logerror("PC %04x: 03 high\n",cpu_get_pc());
		return (input_port_3_r(0) & 0x0f) | 0x00;
	}
	else
	{
//logerror("PC %04x: 03 low\n",cpu_get_pc());
		return (input_port_3_r(0) & 0x0f) | 0x00;
	}
}


READ_HANDLER( crbaloon_IN_r )
{
	switch (offset & 0x03)
	{
		case 0:
			return input_port_0_r(offset);

		case 1:
			return input_port_1_r(offset);

		case 2:
			return crbaloon_IN2_r(offset);

		case 3:
			return crbaloon_IN3_r(offset);
	}

	return 0;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x2fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ 0x4800, 0x4bff, MRA_RAM },
	{ 0x5000, 0x53ff, MRA_RAM },
	{ -1 }	/* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x2fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ 0x4800, 0x4bff, videoram_w, &videoram, &videoram_size },
	{ 0x5000, 0x53ff, colorram_w, &colorram },
	{ -1 }	/* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x00, 0x0f, crbaloon_IN_r },
	{ -1 }	/* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x02, 0x04, crbaloon_spritectrl_w },
	{ 0x06, 0x06, crbaloon_06_w },
	{ 0x08, 0x08, crbaloon_08_w },
	{ 0x0a, 0x0a, crbaloon_0a_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( crbaloon )
	PORT_START
	PORT_DIPNAME( 0x01, 0x01, "Test?" )
	PORT_DIPSETTING(    0x01, "I/O Check?" )
	PORT_DIPSETTING(    0x00, "RAM Check?" )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x0c, 0x04, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x04, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x0c, "5" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x10, "10000" )
	PORT_DIPNAME( 0xe0, 0x80, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, "Disable" )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )

	PORT_START
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNKNOWN )	/* from chip PC3259 */
	PORT_BITX(    0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_BUTTON1, "High Score Name Reset", IP_KEY_DEFAULT, IP_JOY_DEFAULT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )	/* should be COIN2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_TILT )
	/* the following four bits come from chip PC3092 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 characters */
	256,	/* 256 characters */
	1,	/* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	8*8	/* every char takes 8 consecutive bytes */
};
static struct GfxLayout spritelayout =
{
	32,32,	/* 32*32 sprites */
	16,	/* 16 sprites */
	1,	/* 1 bit per pixel */
	{ 0 },
	{ 3*32*8+0, 3*32*8+1, 3*32*8+2, 3*32*8+3, 3*32*8+4, 3*32*8+5, 3*32*8+6, 3*32*8+7,
			2*32*8+0, 2*32*8+1, 2*32*8+2, 2*32*8+3, 2*32*8+4, 2*32*8+5, 2*32*8+6, 2*32*8+7,
			1*32*8+0, 1*32*8+1, 1*32*8+2, 1*32*8+3, 1*32*8+4, 1*32*8+5, 1*32*8+6, 1*32*8+7,
			0*32*8+0, 0*32*8+1, 0*32*8+2, 0*32*8+3, 0*32*8+4, 0*32*8+5, 0*32*8+6, 0*32*8+7 },
	{ 31*8, 30*8, 29*8, 28*8, 27*8, 26*8, 25*8, 24*8,
			23*8, 22*8, 21*8, 20*8, 19*8, 18*8, 17*8, 16*8,
			15*8, 14*8, 13*8, 12*8, 11*8, 10*8, 9*8, 8*8,
			7*8, 6*8, 5*8, 4*8, 3*8, 2*8, 1*8, 0*8 },
	32*4*8  /* every sprite takes 128 consecutive bytes */
};


static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &charlayout,   0, 16 },
	{ REGION_GFX2, 0, &spritelayout, 0, 16 },
	{ -1 } /* end of array */
};


static struct SN76477interface sn76477_interface =
{
	1,	/* 1 chip */
	{ 100 }, /* mixing level   pin description		 */
	{ RES_K( 47)   },		/*	4  noise_res		 */
	{ RES_K(330)   },		/*	5  filter_res		 */
	{ CAP_P(470)   },		/*	6  filter_cap		 */
	{ RES_K(220)   },		/*	7  decay_res		 */
	{ CAP_U(1.0)   },		/*	8  attack_decay_cap  */
	{ RES_K(4.7)   },		/* 10  attack_res		 */
	{ RES_M(  1)   },		/* 11  amplitude_res	 */
	{ RES_K(200)   },		/* 12  feedback_res 	 */
	{ 5.0		   },		/* 16  vco_voltage		 */
	{ CAP_P(470)   },		/* 17  vco_cap			 */
	{ RES_K(330)   },		/* 18  vco_res			 */
	{ 5.0		   },		/* 19  pitch_voltage	 */
	{ RES_K( 20)   },		/* 20  slf_res			 */
	{ CAP_P(420)   },		/* 21  slf_cap			 */
	{ CAP_U(1.0)   },		/* 23  oneshot_cap		 */
	{ RES_K( 47)   }		/* 24  oneshot_res		 */
};


static struct MachineDriver machine_driver_crbaloon =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			3072000,	/* 3.072 Mhz ????? */
			readmem,writemem,readport,writeport,
			interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* single CPU, no need for interleaving */
	crbaloon_machine_init,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 4*8, 32*8-1 },
	gfxdecodeinfo,
	16, 16*2,
	crbaloon_vh_convert_color_prom,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	crbaloon_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SN76477,
			&sn76477_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( crbaloon )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "cl01.bin",     0x0000, 0x0800, 0x9d4eef0b )
	ROM_LOAD( "cl02.bin",     0x0800, 0x0800, 0x10f7a6f7 )
	ROM_LOAD( "cl03.bin",     0x1000, 0x0800, 0x44ed6030 )
	ROM_LOAD( "cl04.bin",     0x1800, 0x0800, 0x62f66f6c )
	ROM_LOAD( "cl05.bin",     0x2000, 0x0800, 0xc8f1e2be )
	ROM_LOAD( "cl06.bin",     0x2800, 0x0800, 0x7d465691 )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cl07.bin",     0x0000, 0x0800, 0x2c1fbea8 )

	ROM_REGION( 0x0800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cl08.bin",     0x0000, 0x0800, 0xba898659 )
ROM_END

ROM_START( crbalon2 )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "cl01.bin",     0x0000, 0x0800, 0x9d4eef0b )
	ROM_LOAD( "crazybal.ep2", 0x0800, 0x0800, 0x87572086 )
	ROM_LOAD( "crazybal.ep3", 0x1000, 0x0800, 0x575fe995 )
	ROM_LOAD( "cl04.bin",     0x1800, 0x0800, 0x62f66f6c )
	ROM_LOAD( "cl05.bin",     0x2000, 0x0800, 0xc8f1e2be )
	ROM_LOAD( "crazybal.ep6", 0x2800, 0x0800, 0xfed6ff5c )

	ROM_REGION( 0x0800, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cl07.bin",     0x0000, 0x0800, 0x2c1fbea8 )

	ROM_REGION( 0x0800, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "cl08.bin",     0x0000, 0x0800, 0xba898659 )
ROM_END



GAMEX( 1980, crbaloon, 0,		crbaloon, crbaloon, 0, ROT90, "Taito Corporation", "Crazy Balloon (set 1)", GAME_IMPERFECT_SOUND )
GAMEX( 1980, crbalon2, crbaloon, crbaloon, crbaloon, 0, ROT90, "Taito Corporation", "Crazy Balloon (set 2)", GAME_IMPERFECT_SOUND )

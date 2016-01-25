#include "../vidhrdw/blockade.cpp"

/****************************************************************************

Blockade/Comotion/Blasto/Hustle Memory MAP
Frank Palazzolo (palazzol@home.com)

CPU - Intel 8080A

Memory Address              (Upper/Lower)

0xxx 00aa aaaa aaaa     ROM     U2/U3    R       1K for Blockade/Comotion/Blasto
0xxx 01aa aaaa aaaa     ROM     U4/U5    R       1K for Comotion/Blasto/Hustle Only
xxx1 xxxx aaaa aaaa     RAM              R/W     256 bytes
1xx0 xxaa aaaa aaaa    VRAM                      1K playfield

                    CHAR ROM  U29/U43            256 bytes for Blockade/Comotion
                                                 512 for Blasto/Hustle

Ports    In            Out
1        Controls      bit 7 = Coin Latch Reset
                       bit 5 = Pin 19?
2        Controls      Square Wave Pitch Register
4        Controls      Noise On
8        N/A           Noise Off


Notes:  Support is complete with the exception of the square wave generator
        and noise generator.  I have not created a sample for the noise
		generator, but any BOOM sound as a sample will do for now for
		Blockade & Comotion, at least.

****************************************************************************/


#include "driver.h"
#include "vidhrdw/generic.h"

/* #define BLOCKADE_LOG 1 */

/* in vidhrdw */
void blockade_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

WRITE_HANDLER( blockade_coin_latch_w );
WRITE_HANDLER( blockade_sound_freq_w );
WRITE_HANDLER( blockade_env_on_w );
WRITE_HANDLER( blockade_env_off_w );

/* These are used to simulate coin latch circuitry */

static int coin_latch;  /* Active Low */
static int just_been_reset;

void init_blockade(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int i;

	/* Merge nibble-wide roms together,
	   and load them into 0x0000-0x0400 */

	for (i = 0;i < 0x0400;i++)
	{
		rom[0x0000+i] = (rom[0x1000+i]<<4)+rom[0x1400+i];
	}

	/* Initialize the coin latch state here */
	coin_latch = 1;
	just_been_reset = 0;
}

void init_comotion(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int i;

	/* Merge nibble-wide roms together,
	   and load them into 0x0000-0x0800 */

	for(i = 0;i < 0x0400;i++)
	{
		rom[0x0000+i] = (rom[0x1000+i]<<4)+rom[0x1400+i];
		rom[0x0400+i] = (rom[0x1800+i]<<4)+rom[0x1c00+i];
	}

	/* They also need to show up here for comotion/blasto */

	for(i = 0;i < 2048;i++)
	{
		rom[0x4000+i] = rom[0x0000+i];
	}

	/* Initialize the coin latch state here */
	coin_latch = 1;
	just_been_reset = 0;
}

/*************************************************************/
/*                                                           */
/* Inserting a coin should work like this:                   */
/*  1) Reset the CPU                                         */
/*  2) CPU Sets the coin latch                               */
/*  3) Finally the CPU coin latch is Cleared by the hardware */
/*     (by the falling coin..?)                              */
/*                                                           */
/*  I am faking this by keeping the CPU from Setting         */
/*  the coin latch if we have just been reset.               */
/*                                                           */
/*************************************************************/


/* Need to check for a coin on the interrupt, */
/* This will reset the cpu                    */

int blockade_interrupt(void)
{
	timer_suspendcpu(0, 0, SUSPEND_ANY_REASON);

	if ((input_port_0_r(0) & 0x80) == 0)
	{
		just_been_reset = 1;
		cpu_set_reset_line(0,PULSE_LINE);
	}

	return ignore_interrupt();
}

READ_HANDLER( blockade_input_port_0_r )
{
    /* coin latch is bit 7 */

    int temp = (input_port_0_r(0)&0x7f);
    return (coin_latch<<7) | (temp);
}

WRITE_HANDLER( blockade_coin_latch_w )
{
    if (data & 0x80)
    {
    #ifdef BLOCKADE_LOG
        printf("Reset Coin Latch\n");
    #endif
        if (just_been_reset)
        {
            just_been_reset = 0;
            coin_latch = 0;
        }
        else
            coin_latch = 1;
    }

    if (data & 0x20)
    {
    #ifdef BLOCKADE_LOG
        printf("Pin 19 High\n");
    #endif
    }
    else
    {
    #ifdef BLOCKADE_LOG
        printf("Pin 19 Low\n");
    #endif
    }

    return;
}

WRITE_HANDLER( blockade_sound_freq_w )
{
#ifdef BLOCKADE_LOG
    printf("Sound Freq Write: %d\n",data);
#endif
    return;
}

WRITE_HANDLER( blockade_env_on_w )
{
#ifdef BLOCKADE_LOG
    printf("Boom Start\n");
#endif
    sample_start(0,0,0);
    return;
}

WRITE_HANDLER( blockade_env_off_w )
{
#ifdef BLOCKADE_LOG
    printf("Boom End\n");
#endif
    return;
}

static WRITE_HANDLER( blockade_videoram_w )
{
	videoram_w(offset, data);
	if (input_port_3_r(0) & 0x80)
	{
		//logerror("blockade_videoram_w: scanline %d\n", cpu_getscanline());
		cpu_spinuntil_int();
	}
}

static struct MemoryReadAddress readmem[] =
{
    { 0x0000, 0x07ff, MRA_ROM },
    { 0x4000, 0x47ff, MRA_ROM },  /* same image */
    { 0xe000, 0xe3ff, videoram_r },
    { 0xff00, 0xffff, MRA_RAM },
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
    { 0x0000, 0x07ff, MWA_ROM },
    { 0x4000, 0x47ff, MWA_ROM },  /* same image */
    { 0xe000, 0xe3ff, blockade_videoram_w, &videoram, &videoram_size },
    { 0xff00, 0xffff, MWA_RAM },
    { -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
    { 0x01, 0x01, blockade_input_port_0_r },
    { 0x02, 0x02, input_port_1_r },
    { 0x04, 0x04, input_port_2_r },
    { -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
    { 0x01, 0x01, blockade_coin_latch_w },
    { 0x02, 0x02, blockade_sound_freq_w },
    { 0x04, 0x04, blockade_env_on_w },
    { 0x08, 0x08, blockade_env_off_w },
    { -1 }  /* end of table */
};

/* These are not dip switches, they are mapped to */
/* connectors on the board.  Different games had  */
/* different harnesses which plugged in here, and */
/* some pins were unused.                         */

INPUT_PORTS_START( blockade )
    PORT_START  /* IN0 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_DIPNAME(    0x04, 0x04, "Boom Switch" )
    PORT_DIPSETTING( 0x00, DEF_STR( Off ) )
    PORT_DIPSETTING( 0x04, DEF_STR( On ) )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_DIPNAME(    0x70, 0x70, DEF_STR( Lives ) )
    PORT_DIPSETTING( 0x60, "3" )
    PORT_DIPSETTING( 0x50, "4" )
    PORT_DIPSETTING( 0x30, "5" )
    PORT_DIPSETTING( 0x70, "6" )
    PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 1 )
                                /* this is really used for the coin latch,  */
                                /* see blockade_interrupt()                 */

    PORT_START  /* IN1 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )

    PORT_START  /* IN2 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 */
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( comotion )
    PORT_START  /* IN0 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_DIPNAME(    0x04, 0x04, "Boom Switch" )
    PORT_DIPSETTING( 0x00, DEF_STR( Off ) )
    PORT_DIPSETTING( 0x04, DEF_STR( On ) )
    PORT_DIPNAME(    0x00, 0x08, DEF_STR( Lives ) )
    PORT_DIPSETTING( 0x00, "3" )
    PORT_DIPSETTING( 0x08, "4" )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 1 )
                                /* this is really used for the coin latch,  */
                                /* see blockade_interrupt()                 */

    PORT_START  /* IN1 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER3 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER3 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER3 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER3 )

    PORT_START  /* IN2 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER4 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER4 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER4 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER4 )

	PORT_START	/* IN3 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )
INPUT_PORTS_END

INPUT_PORTS_START( blasto )
    PORT_START  /* IN0 */
    PORT_DIPNAME(    0x03, 0x03, DEF_STR( Coinage ) )
    PORT_DIPSETTING( 0x00, DEF_STR( 4C_1C ) )
    PORT_DIPSETTING( 0x01, DEF_STR( 3C_1C ) )
    PORT_DIPSETTING( 0x02, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING( 0x03, DEF_STR( 1C_1C ) )
    PORT_DIPNAME(    0x04, 0x04, "Boom Switch" )
    PORT_DIPSETTING( 0x00, DEF_STR( Off ) )
    PORT_DIPSETTING( 0x04, DEF_STR( On ) )
    PORT_DIPNAME(    0x08, 0x08, "Game Time" )
    PORT_DIPSETTING( 0x00, "70 Secs" )
    PORT_DIPSETTING( 0x08, "90 Secs" )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 1 )
                                /* this is really used for the coin latch,  */
                                /* see blockade_interrupt()                 */

    PORT_START  /* IN1 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )

    PORT_START  /* IN2 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )

	PORT_START	/* IN3 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )
INPUT_PORTS_END

INPUT_PORTS_START( hustle )
    PORT_START  /* IN0 */
    PORT_DIPNAME(    0x03, 0x03, DEF_STR( Coinage ) )
    PORT_DIPSETTING( 0x00, DEF_STR( 4C_1C ) )
    PORT_DIPSETTING( 0x01, DEF_STR( 3C_1C ) )
    PORT_DIPSETTING( 0x02, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING( 0x03, DEF_STR( 1C_1C ) )
    PORT_DIPNAME(    0x04, 0x04, "Game Time" )
    PORT_DIPSETTING( 0x00, "1.5 mins" )
    PORT_DIPSETTING( 0x04, "2 mins" )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 1 )
                                /* this is really used for the coin latch,  */
                                /* see blockade_interrupt()                 */

    PORT_START  /* IN1 */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER2 )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_4WAY | IPF_PLAYER1 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_4WAY | IPF_PLAYER1 )

    PORT_START  /* IN2 */
	PORT_DIPNAME( 0xf1, 0xf0, "Free Game" )
	PORT_DIPSETTING ( 0x71, "11000" )
    PORT_DIPSETTING ( 0xb1, "13000" )
	PORT_DIPSETTING ( 0xd1, "15000" )
	PORT_DIPSETTING ( 0xe1, "17000" )
    PORT_DIPSETTING ( 0xf0, "Disabled" )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN3 */
	PORT_BIT( 0x7f, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_VBLANK )
INPUT_PORTS_END



static struct GfxLayout blockade_layout =
{
	8,8,    /* 8*8 characters */
	32, /* 32 characters */
	1,  /* 1 bit per pixel */
	{ 0 },  /* no separation in 1 bpp */
	{ 4,5,6,7,256*8+4,256*8+5,256*8+6,256*8+7 },    /* Merge nibble-wide roms */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxLayout blasto_layout =
{
	8,8,    /* 8*8 characters */
	64, /* 64 characters */
	1,  /* 1 bit per pixel */
	{ 0 },  /* no separation in 1 bpp */
	{ 4,5,6,7,512*8+4,512*8+5,512*8+6,512*8+7 },    /* Merge nibble-wide roms */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8 /* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &blockade_layout, 0, 2 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo blasto_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0x0000, &blasto_layout,   0, 2 },
	{ -1 } /* end of array */
};


static unsigned char gr_palette[] =
{
	0x00,0x00,0x00, /* BLACK */
	0x00,0xff,0x00, /* GREEN */     /* overlay (Blockade/Hustle) */
};
static unsigned char bw_palette[] =
{
	0x00,0x00,0x00, /* BLACK */
	0xff,0xff,0xff, /* WHITE */     /* Comotion/Blasto */
};
static void init_palette_gr(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,gr_palette,sizeof(gr_palette));
}
static void init_palette_bw(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,bw_palette,sizeof(bw_palette));
}



static const char *blockade_sample_names[] =
{
    "*blockade",
    "BOOM.wav",
    0   /* end of array */
};

static struct Samplesinterface samples_interface =
{
    1,	/* 1 channel */
	25,	/* volume */
	blockade_sample_names
};



static struct MachineDriver machine_driver_blockade =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			2079000,
			readmem,writemem,readport,writeport,
			blockade_interrupt,1
		},
	},
		60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
		1,
	0,

	/* video hardware */
	32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	2, 2,
	init_palette_gr,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	blockade_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	}
};

static struct MachineDriver machine_driver_comotion =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			2079000,
			readmem,writemem,readport,writeport,
			blockade_interrupt,1
		},
	},
		60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
		1,
	0,

	/* video hardware */
	32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	gfxdecodeinfo,
	2, 2,
	init_palette_bw,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	blockade_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	}
};

static struct MachineDriver machine_driver_blasto =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			2079000,
			readmem,writemem,readport,writeport,
			blockade_interrupt,1
		},
	},
		60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
		1,
	0,

	/* video hardware */
	32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	blasto_gfxdecodeinfo,
	2, 2,
	init_palette_bw,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_vh_start,
	generic_vh_stop,
	blockade_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_SAMPLES,
			&samples_interface
		}
	}
};

static struct MachineDriver machine_driver_hustle =
{
    /* basic machine hardware */
    {
        {
            CPU_8080,
            2079000,
            readmem,writemem,readport,writeport,
            blockade_interrupt,1
        },
    },
        60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
        1,
    0,

    /* video hardware */
    32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
    blasto_gfxdecodeinfo,
	2, 2,
	init_palette_gr,

    VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
    0,
    generic_vh_start,
    generic_vh_stop,
    blockade_vh_screenrefresh,

    /* sound hardware */
    0,0,0,0,
    {
        {
            SOUND_SAMPLES,
            &samples_interface
        }
    }
};

/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( blockade )
    ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
    /* Note: These are being loaded into a bogus location, */
    /*       They are nibble wide rom images which will be */
    /*       merged and loaded into the proper place by    */
    /*       blockade_rom_init()                           */
    ROM_LOAD( "316-04.u2", 0x1000, 0x0400, 0xa93833e9 )
    ROM_LOAD( "316-03.u3", 0x1400, 0x0400, 0x85960d3b )

    ROM_REGION( 0x200, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "316-02.u29", 0x0000, 0x0100, 0x409f610f )
    ROM_LOAD( "316-01.u43", 0x0100, 0x0100, 0x41a00b28 )
ROM_END

ROM_START( comotion )
    ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
    /* Note: These are being loaded into a bogus location, */
    /*       They are nibble wide rom images which will be */
    /*       merged and loaded into the proper place by    */
    /*       comotion_rom_init()                           */
    ROM_LOAD( "316-07.u2", 0x1000, 0x0400, 0x5b9bd054 )
    ROM_LOAD( "316-08.u3", 0x1400, 0x0400, 0x1a856042 )
    ROM_LOAD( "316-09.u4", 0x1800, 0x0400, 0x2590f87c )
    ROM_LOAD( "316-10.u5", 0x1c00, 0x0400, 0xfb49a69b )

    ROM_REGION( 0x200, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "316-06.u43", 0x0000, 0x0100, 0x8f071297 )  /* Note: these are reversed */
    ROM_LOAD( "316-05.u29", 0x0100, 0x0100, 0x53fb8821 )
ROM_END

ROM_START( blasto )
    ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
    /* Note: These are being loaded into a bogus location, */
    /*       They are nibble wide rom images which will be */
    /*       merged and loaded into the proper place by    */
    /*       comotion_rom_init()                           */
    ROM_LOAD( "blasto.u2", 0x1000, 0x0400, 0xec99d043 )
    ROM_LOAD( "blasto.u3", 0x1400, 0x0400, 0xbe333415 )
    ROM_LOAD( "blasto.u4", 0x1800, 0x0400, 0x1c889993 )
    ROM_LOAD( "blasto.u5", 0x1c00, 0x0400, 0xefb640cb )

    ROM_REGION( 0x400, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "blasto.u29", 0x0000, 0x0200, 0x4dd69499 )
    ROM_LOAD( "blasto.u43", 0x0200, 0x0200, 0x104051a4 )
ROM_END

ROM_START( hustle )
    ROM_REGION( 0x10000, REGION_CPU1 ) /* 64k for code */
    /* Note: These are being loaded into a bogus location, */
    /*       They are nibble wide rom images which will be */
    /*       merged and loaded into the proper place by    */
    /*       comotion_rom_init()                           */
    ROM_LOAD( "3160016.u2", 0x1000, 0x0400, 0xd983de7c )
    ROM_LOAD( "3160017.u3", 0x1400, 0x0400, 0xedec9cb9 )
    ROM_LOAD( "3160018.u4", 0x1800, 0x0400, 0xf599b9c0 )
    ROM_LOAD( "3160019.u5", 0x1c00, 0x0400, 0x7794bc7e )

    ROM_REGION( 0x400, REGION_GFX1 | REGIONFLAG_DISPOSE )
    ROM_LOAD( "3160020.u29", 0x0000, 0x0200, 0x541d2c67 )
    ROM_LOAD( "3160021.u43", 0x0200, 0x0200, 0xb5083128 )
ROM_END



GAMEX( 1976, blockade, 0, blockade, blockade, blockade, ROT0, "Gremlin", "Blockade", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1976, comotion, 0, comotion, comotion, comotion, ROT0, "Gremlin", "Comotion", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1978, blasto,   0, blasto,   blasto,   comotion, ROT0, "Gremlin", "Blasto", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1977, hustle,   0, hustle,   hustle,   comotion, ROT0, "Gremlin", "Hustle", GAME_IMPERFECT_SOUND | GAME_NO_COCKTAIL )

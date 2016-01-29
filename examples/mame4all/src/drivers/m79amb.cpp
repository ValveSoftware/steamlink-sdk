#include "../vidhrdw/m79amb.cpp"

/* Ramtek M79 Ambush */

#include "driver.h"
#include "vidhrdw/generic.h"
/*
 * in
 * 8000 DIP SW
 * 8002 D0=VBlank
 * 8004
 * 8005
 *
 * out
 * 8000
 * 8001 Mask Sel
 * 8002
 * 8003 D0=SelfTest LED
 *
 */

WRITE_HANDLER( ramtek_videoram_w );

int  invaders_interrupt(void);
void ramtek_sh_update(void);
WRITE_HANDLER( ramtek_mask_w );

/*
 * since these functions aren't used anywhere else, i've made them
 * static, and included them here
 */
static const int ControllerTable[32] = {
    0  , 1  , 3  , 2  , 6  , 7  , 5  , 4  ,
    12 , 13 , 15 , 14 , 10 , 11 , 9  , 8  ,
    24 , 25 , 27 , 26 , 30 , 31 , 29 , 28 ,
    20 , 21 , 23 , 22 , 18 , 19 , 17 , 16
};

static READ_HANDLER( gray5bit_controller0_r )
{
    return (input_port_2_r(0) & 0xe0) | (~ControllerTable[input_port_2_r(0) & 0x1f] & 0x1f);
}

static READ_HANDLER( gray5bit_controller1_r )
{
    return (input_port_3_r(0) & 0xe0) | (~ControllerTable[input_port_3_r(0) & 0x1f] & 0x1f);
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x1fff, MRA_ROM },
	{ 0x4000, 0x63ff, MRA_RAM },
	{ 0x8000, 0x8000, input_port_0_r},
	{ 0x8002, 0x8002, input_port_1_r},
	{ 0x8004, 0x8004, gray5bit_controller0_r},
	{ 0x8005, 0x8005, gray5bit_controller1_r},
	{ 0xC000, 0xC07f, MRA_RAM},			/* ?? */
	{ 0xC200, 0xC27f, MRA_RAM},			/* ?? */
	{ -1 }  /* end of table */
};

WRITE_HANDLER( sound_w )
{
}

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x1fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
    { 0x4400, 0x5fff, ramtek_videoram_w, &videoram },
    { 0x6000, 0x63ff, MWA_RAM },		/* ?? */
	{ 0x8001, 0x8001, ramtek_mask_w},
	{ 0x8000, 0x8000, sound_w },
	{ 0x8002, 0x8003, sound_w },
	{ 0xC000, 0xC07f, MWA_RAM},			/* ?? */
	{ 0xC200, 0xC27f, MWA_RAM},			/* ?? */
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( m79amb )
	PORT_START      /* 8000 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* dip switch */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START      /* 8002 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_VBLANK )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_COIN1  )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,  IPT_TILT   )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNUSED )

	PORT_START		/* 8004 */
	PORT_ANALOG( 0x1f, 0x10, IPT_PADDLE, 25, 10, 0, 0x1f)
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_BUTTON1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START      /* 8005 */
	PORT_ANALOG( 0x1f, 0x10, IPT_PADDLE | IPF_PLAYER2, 25, 10, 0, 0x1f)
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNKNOWN )

INPUT_PORTS_END


static unsigned char palette[] = /* V.V */ /* Smoothed pure colors, overlays are not so contrasted */
{
	0x00,0x00,0x00, /* BLACK */
	0xff,0xff,0xff, /* WHITE */
	0xff,0x20,0x20, /* RED */
	0x20,0xff,0x20, /* GREEN */
	0xff,0xff,0x20, /* YELLOW */
	0x20,0xff,0xff, /* CYAN */
	0xff,0x20,0xff  /* PURPLE */
};

static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
}

static int M79_interrupt(void)
{
	return 0x00cf;  /* RST 08h */
}

static void init_m79amb(void)
{
	unsigned char *rom = memory_region(REGION_CPU1);
	int i;

	/* PROM data is active low */
 	for (i = 0;i < 0x2000;i++)
		rom[i] = ~rom[i];
}

static struct MachineDriver machine_driver_m79amb =
{
	/* basic machine hardware */
	{
		{
			CPU_8080,
			1996800,
			readmem,writemem,0,0,
			M79_interrupt, 1
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,  /* frames per second, vblank duration */
	1,      /* single CPU, no need for interleaving */
	0,

	/* video hardware */
	32*8, 28*8, { 0*8, 32*8-1, 0*8, 28*8-1 },
	0,      /* no gfxdecodeinfo - bitmapped display */
	sizeof(palette) / sizeof(palette[0]) / 3, 0,
	init_palette,

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	generic_bitmapped_vh_start,
	generic_bitmapped_vh_stop,
	generic_bitmapped_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0
};



ROM_START( m79amb )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "m79.10t",      0x0000, 0x0200, 0xccf30b1e )
	ROM_LOAD( "m79.9t",       0x0200, 0x0200, 0xdaf807dd )
	ROM_LOAD( "m79.8t",       0x0400, 0x0200, 0x79fafa02 )
	ROM_LOAD( "m79.7t",       0x0600, 0x0200, 0x06f511f8 )
	ROM_LOAD( "m79.6t",       0x0800, 0x0200, 0x24634390 )
	ROM_LOAD( "m79.5t",       0x0a00, 0x0200, 0x95252aa6 )
	ROM_LOAD( "m79.4t",       0x0c00, 0x0200, 0x54cffb0f )
	ROM_LOAD( "m79.3ta",      0x0e00, 0x0200, 0x27db5ede )
	ROM_LOAD( "m79.10u",      0x1000, 0x0200, 0xe41d13d2 )
	ROM_LOAD( "m79.9u",       0x1200, 0x0200, 0xe35f5616 )
	ROM_LOAD( "m79.8u",       0x1400, 0x0200, 0x14eafd7c )
	ROM_LOAD( "m79.7u",       0x1600, 0x0200, 0xb9864f25 )
	ROM_LOAD( "m79.6u",       0x1800, 0x0200, 0xdd25197f )
	ROM_LOAD( "m79.5u",       0x1a00, 0x0200, 0x251545e2 )
	ROM_LOAD( "m79.4u",       0x1c00, 0x0200, 0xb5f55c75 )
	ROM_LOAD( "m79.3u",       0x1e00, 0x0200, 0xe968691a )
ROM_END



GAME( 1977, m79amb, 0, m79amb, m79amb, m79amb, ROT0, "Ramtek", "M79 Ambush" )

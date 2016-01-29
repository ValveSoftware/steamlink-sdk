#include "../machine/berzerk.cpp"
#include "../vidhrdw/berzerk.cpp"
#include "../sndhrdw/berzerk.cpp"

/***************************************************************************

 Berzerk Driver by Zsolt Vasvari
 Sound Driver by Alex Judd

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

extern unsigned char* berzerk_magicram;

void berzerk_init_machine(void);

void berzerk_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

int  berzerk_interrupt(void);
WRITE_HANDLER( berzerk_irq_enable_w );
WRITE_HANDLER( berzerk_nmi_enable_w );
WRITE_HANDLER( berzerk_nmi_disable_w );
READ_HANDLER( berzerk_nmi_enable_r );
READ_HANDLER( berzerk_nmi_disable_r );
READ_HANDLER( berzerk_led_on_r );
READ_HANDLER( berzerk_led_off_r );
READ_HANDLER( berzerk_voiceboard_r );
WRITE_HANDLER( berzerk_videoram_w );
WRITE_HANDLER( berzerk_colorram_w );

WRITE_HANDLER( berzerk_magicram_w );
WRITE_HANDLER( berzerk_magicram_control_w );
READ_HANDLER( berzerk_collision_r );

WRITE_HANDLER( berzerk_sound_control_a_w );
int  berzerk_sh_start(const struct MachineSound *msound);
void berzerk_sh_update(void);


static unsigned char *nvram;
static size_t nvram_size;

static void berzerk_nvram_handler(void *file,int read_or_write)
{
	if (read_or_write)
		osd_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			osd_fread(file,nvram,nvram_size);
	}
}



static struct MemoryReadAddress berzerk_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_ROM },
	{ 0x0800, 0x09ff, MRA_RAM },
	{ 0x1000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x87ff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress berzerk_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_ROM },
	{ 0x0800, 0x09ff, MWA_RAM, &nvram, &nvram_size },
	{ 0x1000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x5fff, berzerk_videoram_w, &videoram, &videoram_size},
	{ 0x6000, 0x7fff, berzerk_magicram_w, &berzerk_magicram},
	{ 0x8000, 0x87ff, berzerk_colorram_w, &colorram},
	{ -1 }  /* end of table */
};


static struct MemoryReadAddress frenzy_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x87ff, MRA_RAM },
	{ 0xc000, 0xcfff, MRA_ROM },
	{ 0xf800, 0xf9ff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress frenzy_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x5fff, berzerk_videoram_w, &videoram, &videoram_size},
	{ 0x6000, 0x7fff, berzerk_magicram_w, &berzerk_magicram},
	{ 0x8000, 0x87ff, berzerk_colorram_w, &colorram},
	{ 0xc000, 0xcfff, MWA_ROM },
	{ 0xf800, 0xf9ff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort readport[] =
{
	{ 0x44, 0x44, berzerk_voiceboard_r}, /* Sound stuff */
	{ 0x48, 0x48, input_port_0_r},
	{ 0x49, 0x49, input_port_1_r},
	{ 0x4a, 0x4a, input_port_2_r},
	{ 0x4c, 0x4c, berzerk_nmi_enable_r},
	{ 0x4d, 0x4d, berzerk_nmi_disable_r},
	{ 0x4e, 0x4e, berzerk_collision_r},
	{ 0x60, 0x60, input_port_3_r},
	{ 0x61, 0x61, input_port_4_r},
	{ 0x62, 0x62, input_port_5_r},
	{ 0x63, 0x63, input_port_6_r},
	{ 0x64, 0x64, input_port_7_r},
	{ 0x65, 0x65, input_port_8_r},
	{ 0x66, 0x66, berzerk_led_off_r},
	{ 0x67, 0x67, berzerk_led_on_r},
	{ -1 }  /* end of table */
};


static struct IOWritePort writeport[] =
{
	{ 0x40, 0x46, berzerk_sound_control_a_w}, /* First sound board */
	{ 0x47, 0x47, IOWP_NOP}, /* not used sound stuff */
	{ 0x4b, 0x4b, berzerk_magicram_control_w},
	{ 0x4c, 0x4c, berzerk_nmi_enable_w},
	{ 0x4d, 0x4d, berzerk_nmi_disable_w},
	{ 0x4f, 0x4f, berzerk_irq_enable_w},
	{ 0x50, 0x57, IOWP_NOP}, /* Second sound board but not used */
	{ -1 }  /* end of table */
};


#define COINAGE(CHUTE) \
	PORT_DIPNAME( 0x0f, 0x00, "Coin "#CHUTE ) \
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x0d, DEF_STR( 4C_3C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x0e, DEF_STR( 4C_5C ) ) \
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x0f, DEF_STR( 4C_7C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x0b, DEF_STR( 2C_5C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_7C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_7C ) ) \
	PORT_DIPSETTING(    0x07, "1 Coin/10 Credits" ) \
	PORT_DIPSETTING(    0x08, "1 Coin/14 Credits" ) \
	PORT_BIT( 0xf0, IP_ACTIVE_LOW,  IPT_UNUSED )


INPUT_PORTS_START( berzerk )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x60, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* IN3 */
	PORT_BITX(    0x01, 0x00, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Input Test Mode", KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_BITX(    0x02, 0x00, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Crosshair Pattern", KEYCODE_F4, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_BIT( 0x3c, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_DIPNAME( 0xc0, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "German" )
	PORT_DIPSETTING(    0x80, "French" )
	PORT_DIPSETTING(    0xc0, "Spanish" )

	PORT_START      /* IN4 */
	PORT_BITX(    0x03, 0x00, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Color Test", KEYCODE_F5, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x03, DEF_STR( On ) )
	PORT_BIT( 0x3c, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0xc0, "5000 and 10000" )
	PORT_DIPSETTING(    0x40, "5000" )
	PORT_DIPSETTING(    0x80, "10000" )
	PORT_DIPSETTING(    0x00, "None" )

	PORT_START      /* IN5 */
	COINAGE(3)

	PORT_START      /* IN6 */
	COINAGE(2)

	PORT_START      /* IN7 */
	COINAGE(1)

	PORT_START      /* IN8 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_BIT( 0x7e, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, 0, "Stats", KEYCODE_F1, IP_JOY_NONE )
INPUT_PORTS_END

INPUT_PORTS_START( frenzy )
	PORT_START      /* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x3c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START      /* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x60, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )

	PORT_START      /* IN3 */
	PORT_DIPNAME( 0x0f, 0x03, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x01, "1000" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPSETTING(    0x03, "3000" )
	PORT_DIPSETTING(    0x04, "4000" )
	PORT_DIPSETTING(    0x05, "5000" )
	PORT_DIPSETTING(    0x06, "6000" )
	PORT_DIPSETTING(    0x07, "7000" )
	PORT_DIPSETTING(    0x08, "8000" )
	PORT_DIPSETTING(    0x09, "9000" )
	PORT_DIPSETTING(    0x0a, "10000" )
	PORT_DIPSETTING(    0x0b, "11000" )
	PORT_DIPSETTING(    0x0c, "12000" )
	PORT_DIPSETTING(    0x0d, "13000" )
	PORT_DIPSETTING(    0x0e, "14000" )
	PORT_DIPSETTING(    0x0f, "15000" )
	PORT_DIPSETTING(    0x00, "None" )
	PORT_BIT( 0x30, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_DIPNAME( 0xc0, 0x00, "Language" )
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "German" )
	PORT_DIPSETTING(    0x80, "French" )
	PORT_DIPSETTING(    0xc0, "Spanish" )

	PORT_START      /* IN4 */
	PORT_BIT( 0x03, IP_ACTIVE_HIGH, IPT_UNUSED )  /* Bit 0 does some more hardware tests */
	PORT_BITX(    0x04, 0x00, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Input Test Mode", KEYCODE_F2, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_BITX(    0x08, 0x00, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "Crosshair Pattern", KEYCODE_F4, IP_JOY_NONE )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_BIT( 0xf0, IP_ACTIVE_LOW, IPT_UNUSED )

	/* The following 3 ports use all 8 bits, but I didn't feel like adding all 256 values :-) */
	PORT_START      /* IN5 */
	PORT_DIPNAME( 0x0f, 0x01, "Coins/Credit B" )
	/*PORT_DIPSETTING(    0x00, "0" )    Can't insert coins  */
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x05, "5" )
	PORT_DIPSETTING(    0x06, "6" )
	PORT_DIPSETTING(    0x07, "7" )
	PORT_DIPSETTING(    0x08, "8" )
	PORT_DIPSETTING(    0x09, "9" )
	PORT_DIPSETTING(    0x0a, "10" )
	PORT_DIPSETTING(    0x0b, "11" )
	PORT_DIPSETTING(    0x0c, "12" )
	PORT_DIPSETTING(    0x0d, "13" )
	PORT_DIPSETTING(    0x0e, "14" )
	PORT_DIPSETTING(    0x0f, "15" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH,  IPT_UNUSED )

	PORT_START      /* IN6 */
	PORT_DIPNAME( 0x0f, 0x01, "Coins/Credit A" )
	/*PORT_DIPSETTING(    0x00, "0" )    Can't insert coins  */
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x05, "5" )
	PORT_DIPSETTING(    0x06, "6" )
	PORT_DIPSETTING(    0x07, "7" )
	PORT_DIPSETTING(    0x08, "8" )
	PORT_DIPSETTING(    0x09, "9" )
	PORT_DIPSETTING(    0x0a, "10" )
	PORT_DIPSETTING(    0x0b, "11" )
	PORT_DIPSETTING(    0x0c, "12" )
	PORT_DIPSETTING(    0x0d, "13" )
	PORT_DIPSETTING(    0x0e, "14" )
	PORT_DIPSETTING(    0x0f, "15" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH,  IPT_UNUSED )

	PORT_START      /* IN7 */
	PORT_DIPNAME( 0x0f, 0x01, "Coin Multiplier" )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x05, "5" )
	PORT_DIPSETTING(    0x06, "6" )
	PORT_DIPSETTING(    0x07, "7" )
	PORT_DIPSETTING(    0x08, "8" )
	PORT_DIPSETTING(    0x09, "9" )
	PORT_DIPSETTING(    0x0a, "10" )
	PORT_DIPSETTING(    0x0b, "11" )
	PORT_DIPSETTING(    0x0c, "12" )
	PORT_DIPSETTING(    0x0d, "13" )
	PORT_DIPSETTING(    0x0e, "14" )
	PORT_DIPSETTING(    0x0f, "15" )
	PORT_BIT( 0xf0, IP_ACTIVE_HIGH,  IPT_UNUSED )

	PORT_START      /* IN8 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT( 0x7e, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BITX(0x80, IP_ACTIVE_HIGH, 0, "Stats", KEYCODE_F1, IP_JOY_NONE )
INPUT_PORTS_END




/* Simple 1-bit RGBI palette */
static unsigned char palette[16 * 3] =
{
	0x00, 0x00, 0x00,
	0xff, 0x00, 0x00,
	0x00, 0xff, 0x00,
	0xff, 0xff, 0x00,
	0x00, 0x00, 0xff,
	0xff, 0x00, 0xff,
	0x00, 0xff, 0xff,
	0xff, 0xff, 0xff,
	0x40, 0x40, 0x40,
	0xff, 0x40, 0x40,
	0x40, 0xff, 0x40,
	0xff, 0xff, 0x40,
	0x40, 0x40, 0xff,
	0xff, 0x40, 0xff,
	0x40, 0xff, 0xff,
	0xff, 0xff, 0xff
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
}



static const char *berzerk_sample_names[] =
{
	"*berzerk", /* universal samples directory */
	"",
	"01.wav", // "kill"
	"02.wav", // "attack"
	"03.wav", // "charge"
	"04.wav", // "got"
	"05.wav", // "to"
	"06.wav", // "get"
	"",
	"08.wav", // "alert"
	"09.wav", // "detected"
	"10.wav", // "the"
	"11.wav", // "in"
	"12.wav", // "it"
	"",
	"",
	"15.wav", // "humanoid"
	"16.wav", // "coins"
	"17.wav", // "pocket"
	"18.wav", // "intruder"
	"",
	"20.wav", // "escape"
	"21.wav", // "destroy"
	"22.wav", // "must"
	"23.wav", // "not"
	"24.wav", // "chicken"
	"25.wav", // "fight"
	"26.wav", // "like"
	"27.wav", // "a"
	"28.wav", // "robot"
	"",
	"30.wav", // player fire
	"31.wav", // baddie fire
	"32.wav", // kill baddie
	"33.wav", // kill human (real)
	"34.wav", // kill human (cheat)
	0	/* end of array */
};

static struct Samplesinterface berzerk_samples_interface =
{
	8,	/* 8 channels */
	25,	/* volume */
	berzerk_sample_names
};

static struct CustomSound_interface custom_interface =
{
	berzerk_sh_start,
	0,
	berzerk_sh_update
};



#define  frenzy_init_machine  0

#define DRIVER(GAMENAME,NVRAM)											\
																		\
static struct MachineDriver machine_driver_##GAMENAME =					\
{																		\
	/* basic machine hardware */										\
	{																	\
		{																\
			CPU_Z80,													\
			2500000,        /* 2.5 MHz */								\
			GAMENAME##_readmem,GAMENAME##_writemem,readport,writeport,	\
			berzerk_interrupt,8											\
		},																\
	},																	\
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */	\
	1,	/* single CPU, no need for interleaving */						\
	GAMENAME##_init_machine,											\
																		\
	/* video hardware */												\
	256, 256, { 0, 256-1, 32, 256-1 },									\
	0,																	\
	sizeof(palette) / sizeof(palette[0]) / 3, 0,						\
	init_palette,														\
																		\
	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,							\
	0,																	\
	0,																	\
	0,																	\
	berzerk_vh_screenrefresh,											\
																		\
	/* sound hardware */												\
	0,0,0,0,															\
	{																	\
		{																\
			SOUND_SAMPLES,												\
			&berzerk_samples_interface									\
		},																\
		{																\
			SOUND_CUSTOM,	/* actually plays the samples */			\
			&custom_interface											\
		}																\
	},																	\
																		\
	NVRAM																\
};


DRIVER(berzerk,berzerk_nvram_handler)
DRIVER(frenzy,0)



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( berzerk )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "1c-0",         0x0000, 0x0800, 0xca566dbc )
	ROM_LOAD( "1d-1",         0x1000, 0x0800, 0x7ba69fde )
	ROM_LOAD( "3d-2",         0x1800, 0x0800, 0xa1d5248b )
	ROM_LOAD( "5d-3",         0x2000, 0x0800, 0xfcaefa95 )
	ROM_LOAD( "6d-4",         0x2800, 0x0800, 0x1e35b9a0 )
	ROM_LOAD( "5c-5",         0x3000, 0x0800, 0xc8c665e5 )
ROM_END

ROM_START( berzerk1 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "rom0.1c",      0x0000, 0x0800, 0x5b7eb77d )
	ROM_LOAD( "rom1.1d",      0x1000, 0x0800, 0xe58c8678 )
	ROM_LOAD( "rom2.3d",      0x1800, 0x0800, 0x705bb339 )
	ROM_LOAD( "rom3.5d",      0x2000, 0x0800, 0x6a1936b4 )
	ROM_LOAD( "rom4.6d",      0x2800, 0x0800, 0xfa5dce40 )
	ROM_LOAD( "rom5.5c",      0x3000, 0x0800, 0x2579b9f4 )
ROM_END

ROM_START( frenzy )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "1c-0",         0x0000, 0x1000, 0xabdd25b8 )
	ROM_LOAD( "1d-1",         0x1000, 0x1000, 0x536e4ae8 )
	ROM_LOAD( "3d-2",         0x2000, 0x1000, 0x3eb9bc9b )
	ROM_LOAD( "5d-3",         0x3000, 0x1000, 0xe1d3133c )
	ROM_LOAD( "6d-4",         0xc000, 0x1000, 0x5581a7b1 )
	/* 1c & 2c are the voice ROMs */
ROM_END



GAME( 1980, berzerk,  0,       berzerk, berzerk, 0, ROT0, "Stern", "Berzerk (set 1)" )
GAME( 1980, berzerk1, berzerk, berzerk, berzerk, 0, ROT0, "Stern", "Berzerk (set 2)" )
GAME( 1982, frenzy,   0,       frenzy,  frenzy,  0, ROT0, "Stern", "Frenzy" )

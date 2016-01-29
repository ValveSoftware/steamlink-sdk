#include "../machine/spiders.cpp"
#include "../vidhrdw/crtc6845.cpp"
#include "../vidhrdw/spiders.cpp"

/***************************************************************************

Spiders driver by K.Wilkins May 1998


Memory map information
----------------------

Main CPU - Read Range

$0000-$1bff	Video Memory (bit0)
$4000-$5bff	Video Memory (bit1)
$8000-$9bff	Video Memory (bit2)
$c000-$c001	6845 CRT Controller (crtc6845)
$c044-$c047	MC6821 PIA 1 (Control input port - all input)
$c048-$c04b	MC6821 PIA 2 (Sprite data port - see machine/spiders.c)
$c050-$c053	MC6821 PIA 3 (Sound control - all output)
$c060		Dip Switch 1
$c080		Dip Switch 2
$c0a0		Dip Switch 3
$c100-ffff	ROM SPACE

Main CPU - Write Range

$0000-$1bff	Video Memory (bit0)
$4000-$5bff	Video Memory (bit1)
$8000-$9bff	Video Memory (bit2)
$c000-$c001	6845 CRT Controller (crtc6845)
$c044-$c047	MC6821 PIA 1
$c048-$c04b	MC6821 PIA 2 (Video port)
$c050-$c053	MC6821 PIA 3


DIP SWITCH 1
------------

   1   2   3   4   5   6   7   8    COIN/CREDIT
   ON  ON  ON                       FREE PLAY
   ON  ON  OFF                      1/2
   ON  OFF ON                       1/3
   OFF ON  ON                       2/1
   ON  OFF OFF                      4/5
   OFF OFF OFF                      1/1

DIP SWITCH 2
------------

   1   2   3   4   5   6   7   8
   ON  ON                           MODE A    A'
   ON  OFF                               A    B'
   OFF ON                                B    A'
   OFF OFF                               B    B'
           ON  ON                   14 # OF SPIDERS WHICH LAND TO
           ON  OFF                  20    COMPLETE SPIDER BELT
           OFF ON                   26
           OFF OFF                  16
                   ON               4  # 0F SPARE GUNS
                   OFF              3
                       ON   ON      NONE  SCORE FOR BONUS GUN
                       ON   OFF     20K
                       OFF  ON      25K
                       OFF  OFF     15K
                               ON   GIANT SPIDER AFTER FIRST SCREEN
                               OFF  GIANT SPIDER AFTER EVERY SCREEN

   PATTERN   1   2   3   4   5   6   7   8   9   10  11  12  13  14
   MODE A    27  36  45  54  63  72  81  98  45  54  63  72  81  98    PCS
   MODE B    20  27  34  41  48  55  62  69  34  41  48  55  62  69    PCS
   MODE A'   1   1   1   3.5 3.5 4   4.5 5   1   3.5 3.5 4   4.5 5     SECONDS
   MODE B'   .7  .7  .7  2   3   3.2 3.4 4   .7  2   3   2.3 3.4 4     SECONDS

   MODE A & B FOR THE NUMBER OF GROWABLE COCOONS
   MODE A' & B' FOR THE FREQUENCY OF SPIDERS APPEARANCE


DIP SWITCH 3
------------

   1   2   3   4   5   6   7   8
   X                                VIDEO FLIP
       ON                           UPRIGHT
       OFF                          TABLE

   SWITCHES 3,4,5 FOR ADJUSTING PICTURE VERTICALLY
   SWITCHES 6,7,8 FOR ADJUSTING PICTURE HORIZONTALLY


Unpopulated Switches
--------------------

  PS1 (Display Crosshatch)         - Connected to PIA1 CB1 via pull-up
  PS2 (Coin input, bypass counter) - Connected to PIA1 PA1 via pull-up and invertor
  PS3 (Display coin counter)       - Connected to PIA1 PA2 via pull-up and invertor


Graphic notes
-------------
Following roms appear to have graphic data

* Mapped in main CPU space

* SP1.BIN	- Appears to have some sprites in it.
* SP2.BIN	- Appears to have some 16x16 sprites. (Includes the word SIGMA)
* SP3.BIN	- Appears to have 2-4 sprites 16x16 - spiders
* SP4.BIN	- CPU Code 6809 - Main
  SP5.BIN	- Some 8x8 and 16x16 tiles/sprites
  SP6.BIN	- Some 8x8 tiles
  SP7.BIN	- Tiles/Sprites 8x8
  SP8.BIN	- Tiles/Sprites 8x8
  SP9A.BIN 	- Tiles/Sprites 8x8
  SP9B.BIN	- Tiles/Sprites 8x8
  SP10A.BIN	- Tiles/Sprites 8x8
  SP10B.BIN	- CPU Code 6802 - Sound

Spiders has a fully bitmapped display and all sprite drawing is handled by the CPU
hence no gfxdecode is done on the sprite ROMS, the CPU accesses them via PIA2.

Screen is arranged in three memory areas with three bits being combined from each
area to produce a 3bit colour send directly to the screen.

$0000-$1bff, $4000-$5bff, $8000-$9bff 	Bank 0
$2000-$3bff, %6000-$7bff, $a000-$bbff	Bank 1

The game normally runs from bank 0 only, but when lots of screen changes are required
e.g spider or explosion then it implements a double buffered scheme with bank 1.

The ram bank for screens is continuous from $0000-$bfff but is physically arranged
as 3 banks of 16k (8x16k DRAM!). The CPU stack/variables etc are stored in the unused
spaces between screens.


CODE NOTES
----------

6809 Data page = $1c00 (DP=1c)

Known data page contents
$05 - Dip switch 1 copy
$06 - Dip switch 2 copy (inverted)
$07 - Dip switch 3 copy
$18 - Bonus Gun Score
$1d - Spiders to complete belt after dipsw decode


$c496 - Wait for vblank ($c04a bit 7 goes high)
$f9cf - Clear screen (Bank0&1)
$c8c6 - RAM test of sorts, called from IRQ handler?
$de2f - Delay loop.
$f9bb - Memory clearance routine
$c761 - Partial DipSW decode
$F987 - Addresses table at $f98d containing four structs:
            3c 0C 04 0D 80 (Inverted screen bank 0)
            34 0C 00 0D 00 (Normal screen   bank 0)
            3C 0C 40 0D 80 (Inverted screen bank 1)
            34 0C 44 0D 00 (Inverted screen bank 1)
            XX             Written to PIA2 Reg 3 - B control
               XX XX       Written to CRTC addr/data
                     XX XX Written to CRTC addr/data
            These tables are used for frame flipping


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/crtc6845.h"
#include "machine/6821pia.h"


/* VIDHRDW */

int  spiders_vh_start(void);
void spiders_vh_stop(void);
void spiders_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

/* MACHINE */

void spiders_init_machine(void);
int spiders_timed_irq(void);


/* Driver structure definition */

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0xbfff, MRA_RAM },
//	{ 0x1c00, 0x1cff, MRA_RAM },	// Data page
//	{ 0x4000, 0x5bff, MRA_RAM },	// Video ram 1
//	{ 0x8000, 0x9bff, MRA_RAM },	// Video ram 2
//	{ 0x7800, 0x7fff, MRA_RAM },	// Stack space
	{ 0xc001, 0xc001, crtc6845_register_r },
	{ 0xc044, 0xc047, pia_0_r },
	{ 0xc048, 0xc04b, pia_1_r },
	{ 0xc050, 0xc053, pia_2_r },
	{ 0xc060, 0xc060, input_port_2_r },
	{ 0xc080, 0xc080, input_port_3_r },
	{ 0xc0a0, 0xc0a0, input_port_4_r },
	{ 0xc100, 0xffff, MRA_ROM },
	{ -1 } /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_RAM },
//	{ 0x1c00, 0x1cff, MWA_RAM },
//	{ 0x4000, 0x5bff, MWA_RAM },
//	{ 0x8000, 0x9bff, MWA_RAM },
//	{ 0x7800, 0x7fff, MWA_RAM },
	{ 0xc000, 0xc000, crtc6845_address_w },
	{ 0xc001, 0xc001, crtc6845_register_w },
	{ 0xc044, 0xc047, pia_0_w },
	{ 0xc048, 0xc04b, pia_1_w },
	{ 0xc050, 0xc053, pia_2_w },
	{ 0xc100, 0xffff, MWA_ROM },
	{ -1 } /* end of table */
};


#if 0
static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x007f, MRA_RAM },
	{ 0xf800, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x007f, MWA_RAM },
	{ 0xf800, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};
#endif


INPUT_PORTS_START( spiders )
    PORT_START      /* IN0 */
    PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
    PORT_BITX(0x02, 0x00, IP_ACTIVE_HIGH , "PS2 (Operator coin)", KEYCODE_4, IP_JOY_NONE )
    PORT_BITX(0x04, 0x00, IP_ACTIVE_HIGH , "PS3 (Coin Counter)", KEYCODE_F1, IP_JOY_NONE )
    PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 )
    PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_START1 )
    PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_START2 )
    PORT_BIT( 0x90, IP_ACTIVE_HIGH, IPT_UNUSED )

    PORT_START      /* IN1 */
    PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_2WAY )
    PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_2WAY )
    PORT_BIT( 0xF3, IP_ACTIVE_HIGH, IPT_UNUSED )

    PORT_START  /* DSW1 */
    PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
    PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x06, DEF_STR( 4C_5C ) )
    PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
    PORT_BIT(0xf8, IP_ACTIVE_LOW,IPT_UNUSED)

    PORT_START  /* DSW2 */
    PORT_DIPNAME( 0x03, 0x03, "Play mode" )
    PORT_DIPSETTING(    0x00, "A A'" )
    PORT_DIPSETTING(    0x01, "A B'" )
    PORT_DIPSETTING(    0x02, "B A'" )
    PORT_DIPSETTING(    0x03, "B B'" )
    PORT_DIPNAME( 0x0c, 0x0c, "Spiders to complete belt" )
    PORT_DIPSETTING(    0x00, "14" )
    PORT_DIPSETTING(    0x04, "20" )
    PORT_DIPSETTING(    0x08, "26" )
    PORT_DIPSETTING(    0x0c, "16" )
    PORT_DIPNAME( 0x10, 0x10, "Spare Guns" )
    PORT_DIPSETTING(    0x00, "4" )
    PORT_DIPSETTING(    0x10, "3" )
    PORT_DIPNAME( 0x60, 0x60, "Score for bonus gun" )
    PORT_DIPSETTING(    0x00, "NONE" )
    PORT_DIPSETTING(    0x20, "20K" )
    PORT_DIPSETTING(    0x40, "25K" )
    PORT_DIPSETTING(    0x60, "15K" )
    PORT_DIPNAME( 0x80, 0x00, "Giant Spiders" )
    PORT_DIPSETTING(    0x00, "First screen" )
    PORT_DIPSETTING(    0x80, "Every screen" )

    PORT_START  /* DSW3 */
    PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x01, DEF_STR( On ) )
    PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
    PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
    PORT_DIPNAME( 0x1c, 0x00, "Vertical Adjust" )
    PORT_DIPSETTING(    0x00, "0" )
    PORT_DIPSETTING(    0x04, "1" )
    PORT_DIPSETTING(    0x08, "2" )
    PORT_DIPSETTING(    0x0c, "3" )
    PORT_DIPSETTING(    0x10, "4" )
    PORT_DIPSETTING(    0x14, "5" )
    PORT_DIPSETTING(    0x18, "6" )
    PORT_DIPSETTING(    0x1c, "7" )
    PORT_DIPNAME( 0xe0, 0x00, "Horizontal Adjust" )
    PORT_DIPSETTING(    0x00, "0" )
    PORT_DIPSETTING(    0x20, "1" )
    PORT_DIPSETTING(    0x40, "2" )
    PORT_DIPSETTING(    0x60, "3" )
    PORT_DIPSETTING(    0x80, "4" )
    PORT_DIPSETTING(    0xa0, "5" )
    PORT_DIPSETTING(    0xc0, "6" )
    PORT_DIPSETTING(    0xe0, "7" )

    PORT_START      /* Connected to PIA1 CA1 input */
    PORT_BIT( 0xFF, IP_ACTIVE_HIGH, IPT_VBLANK )

    PORT_START      /* Connected to PIA0 CB1 input */
    PORT_BITX(0xff, 0xff, IP_ACTIVE_LOW, "PS1 (Crosshatch)", KEYCODE_F2, IP_JOY_NONE )

INPUT_PORTS_END



/* The bitmap RAM is directly mapped to colors, no PROM. */
static unsigned char palette[] =
{
	0x00,0x00,0x00,
	0xff,0x00,0x00,
	0x00,0xff,0x00,
	0xff,0xff,0x00,
	0x00,0x00,0xff,
	0xff,0x00,0xff,
	0x00,0xff,0xff,
	0xff,0xff,0xff,
};
static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
}



static struct MachineDriver machine_driver_spiders =
{
        /* basic machine hardware */
    {
        {
            CPU_M6809,
            2800000,
            readmem,writemem,0,0,
            0,0,                     /* Vblank Int */
            spiders_timed_irq , 25   /* Timed Int  */
        },
//        {
//            CPU_M6802 | CPU_AUDIO_CPU,
//            3000000/4,
//            1,
//            sound_readmem,sound_writemem,0,0,
//            0,0,
//            0,0
//        }
	},
	60,
	DEFAULT_REAL_60HZ_VBLANK_DURATION,
	10,     /* 10 CPU slices per frame - enough for the sound CPU to read all commands */
	spiders_init_machine,

	/* video hardware */
	32*8, 28*8,                         /* Width/Height         */
	{ 0*8, 32*8-1, 0*8, 28*8-1 },       /* Visible area         */
	0,
	sizeof(palette) / sizeof(palette[0]) / 3, 0,
	init_palette,

	VIDEO_TYPE_RASTER | VIDEO_SUPPORTS_DIRTY,                  /* Video attributes     */
	0,                                  /* Video initialisation */
	spiders_vh_start,                    /* Video start          */
	spiders_vh_stop,                     /* Video stop           */
	spiders_vh_screenrefresh,                   /* Video update         */

	/* sound hardware */
	0,0,0,0
	/* Sound struct here */
};



ROM_START( spiders )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "sp-ic74",      0xc000, 0x1000, 0x6a2578f6 )
	ROM_LOAD( "sp-ic73",      0xd000, 0x1000, 0xd69b2f21 )
	ROM_LOAD( "sp-ic72",      0xe000, 0x1000, 0x464125da )
	ROM_LOAD( "sp-ic71",      0xf000, 0x1000, 0xa9539b18 )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "sp-ic3",       0xf800, 0x0800, 0x944d761e )

	ROM_REGION( 0x10000, REGION_GFX1 )     /* 64k graphics block used at runtime */
	ROM_LOAD( "sp-ic33",      0x0000, 0x1000, 0xb6731baa )
	ROM_LOAD( "sp-ic25",      0x1000, 0x1000, 0xbaec64e7 )
	ROM_LOAD( "sp-ic24",      0x2000, 0x1000, 0xa40a5517 )
	ROM_LOAD( "sp-ic23",      0x3000, 0x1000, 0x3ca08053 )
	ROM_LOAD( "sp-ic22",      0x4000, 0x1000, 0x07ea073c )
	ROM_LOAD( "sp-ic21",      0x5000, 0x1000, 0x41b344b4 )
	ROM_LOAD( "sp-ic20",      0x6000, 0x1000, 0x4d37da5a )
ROM_END

ROM_START( spiders2 )
	ROM_REGION( 0x10000, REGION_CPU1 )
	ROM_LOAD( "sp-ic74",      0xc000, 0x1000, 0x6a2578f6 )
	ROM_LOAD( "sp2.bin",      0xd000, 0x1000, 0xcf71d12b )
	ROM_LOAD( "sp-ic72",      0xe000, 0x1000, 0x464125da )
	ROM_LOAD( "sp4.bin",      0xf000, 0x1000, 0xf3d126bb )

	ROM_REGION( 0x10000, REGION_CPU2 )     /* 64k for the audio CPU */
	ROM_LOAD( "sp-ic3",       0xf800, 0x0800, 0x944d761e )

	ROM_REGION( 0x10000, REGION_GFX1 )     /* 64k graphics block used at runtime */
	ROM_LOAD( "sp-ic33",      0x0000, 0x1000, 0xb6731baa )
	ROM_LOAD( "sp-ic25",      0x1000, 0x1000, 0xbaec64e7 )
	ROM_LOAD( "sp-ic24",      0x2000, 0x1000, 0xa40a5517 )
	ROM_LOAD( "sp-ic23",      0x3000, 0x1000, 0x3ca08053 )
	ROM_LOAD( "sp-ic22",      0x4000, 0x1000, 0x07ea073c )
	ROM_LOAD( "sp-ic21",      0x5000, 0x1000, 0x41b344b4 )
	ROM_LOAD( "sp-ic20",      0x6000, 0x1000, 0x4d37da5a )
ROM_END



/* this is a newer version with just one bug fix */
GAMEX( 1981, spiders,  0,       spiders, spiders, 0, ROT270, "Sigma Ent. Inc.", "Spiders (set 1)", GAME_NO_SOUND | GAME_NO_COCKTAIL )
GAMEX( 1981, spiders2, spiders, spiders, spiders, 0, ROT270, "Sigma Ent. Inc.", "Spiders (set 2)", GAME_NO_SOUND | GAME_NO_COCKTAIL )

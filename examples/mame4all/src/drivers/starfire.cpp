#include "../vidhrdw/starfire.cpp"

/***************************************************************************

	Star Fire/Fire One system

    driver by Daniel Boris, Olivier Galibert, Aaron Giles

****************************************************************************

	Memory map

****************************************************************************

	========================================================================
	MAIN CPU
	========================================================================
	0000-7FFF   R     xxxxxxxx   Program ROM
	8000-9FFF   R/W   xxxxxxxx   Scratch RAM, actually mapped into low VRAM
	9000          W   xxxxxxxx   VRAM write control register
	              W   xxx-----      (VRAM shift amount 1)
	              W   ---x----      (VRAM write mirror 1)
	              W   ----xxx-      (VRAM shift amount 2)
	              W   -------x      (VRAM write mirror 2)
	9001          W   xxxxxxxx   Video control register
	              W   x-------      (Color RAM source select)
	              W   -x------      (Palette RAM write enable)
	              W   --x-----      (Video RAM write enable)
	              W   ---x----      (Right side mask select)
	              W   ----xxxx      (Video RAM ALU operation)
	9800-9807   R     xxxxxxxx   Input ports
	A000-BFFF   R/W   xxxxxxxx   Color RAM
	C000-DFFF   R/W   xxxxxxxx   Video RAM, using shift/mirror 1 and color
	E000-FFFF   R/W   xxxxxxxx   Video RAM, using shift/mirror 2
	========================================================================
	Interrupts:
	   NMI generated once/frame
	========================================================================

***************************************************************************/


#include "driver.h"
#include "vidhrdw/generic.h"


/* In vidhrdw/starfire.c */
void starfire_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
extern int starfire_vh_start(void);
extern void starfire_vh_stop(void);
extern void starfire_video_update(int scanline, int count);

WRITE_HANDLER( starfire_videoram_w );
READ_HANDLER( starfire_videoram_r );
WRITE_HANDLER( starfire_colorram_w );
READ_HANDLER( starfire_colorram_r );
WRITE_HANDLER( starfire_vidctrl_w );
WRITE_HANDLER( starfire_vidctrl1_w );



UINT8 *starfire_videoram;
UINT8 *starfire_colorram;

static UINT8 fireone_select;
static mem_read_handler input_read;



/*************************************
 *
 *	Video updates
 *
 *************************************/

#define SCANLINE_UPDATE_CHUNK	8

static void update_callback(int scanline)
{
	/* update the previous chunk of scanlines */
	starfire_video_update(scanline, SCANLINE_UPDATE_CHUNK);
	scanline += SCANLINE_UPDATE_CHUNK;
	if (scanline >= Machine->drv->screen_height)
		scanline = 32;
	timer_set(cpu_getscanlinetime(scanline + SCANLINE_UPDATE_CHUNK - 1), scanline, update_callback);
}


static void init_machine(void)
{
	timer_set(cpu_getscanlinetime(32 + SCANLINE_UPDATE_CHUNK - 1), 32, update_callback);
}



/*************************************
 *
 *	Scratch RAM, mapped into video RAM
 *
 *************************************/

static WRITE_HANDLER( starfire_scratch_w )
{
	/* A12 and A3 select video control registers */
	if ((offset & 0x1008) == 0x1000)
	{
		switch (offset & 7)
		{
			case 0:	starfire_vidctrl_w(0, data); break;
			case 1: starfire_vidctrl1_w(0, data); break;
			case 2:
				/* Sounds */
				fireone_select = (data & 0x8) ? 0 : 1;
				break;
		}
	}

	/* convert to a videoram offset */
	offset = (offset & 0x31f) | ((offset & 0xe0) << 5);
    starfire_videoram[offset] = data;
}


static READ_HANDLER( starfire_scratch_r )
{
	/* A11 selects input ports */
	if (offset & 0x800)
		return (*input_read)(offset);

	/* convert to a videoram offset */
	offset = (offset & 0x31f) | ((offset & 0xe0) << 5);
    return starfire_videoram[offset];
}



/*************************************
 *
 *	Game-specific input handlers
 *
 *************************************/

static READ_HANDLER( starfire_input_r )
{
	switch (offset & 15)
	{
		case 0:	return input_port_0_r(0);
		case 1:	return input_port_1_r(0);	/* Note: need to loopback sounds lengths on that one */
		case 5: return input_port_4_r(0);
		case 6:	return input_port_2_r(0);
		case 7:	return input_port_3_r(0);
		default: return 0xff;
	}
}


static READ_HANDLER( fireone_input_r )
{
	static const UINT8 fireone_paddle_map[64] =
	{
		0x00,0x01,0x03,0x02,0x06,0x07,0x05,0x04,
		0x0c,0x0d,0x0f,0x0e,0x0a,0x0b,0x09,0x08,
		0x18,0x19,0x1b,0x1a,0x1e,0x1f,0x1d,0x1c,
		0x14,0x15,0x17,0x16,0x12,0x13,0x11,0x10,
		0x30,0x31,0x33,0x32,0x36,0x37,0x35,0x34,
		0x3c,0x3d,0x3f,0x3e,0x3a,0x3b,0x39,0x38,
		0x28,0x29,0x2b,0x2a,0x2e,0x2f,0x2d,0x2c,
		0x24,0x25,0x27,0x26,0x22,0x23,0x21,0x20
	};
	int temp;

	switch (offset & 15)
	{
		case 0:	return input_port_0_r(0);
		case 1: return input_port_1_r(0);
		case 2:
			temp = fireone_select ? input_port_2_r(0) : input_port_3_r(0);
			temp = (temp & 0xc0) | fireone_paddle_map[temp & 0x3f];
			return temp;
		default: return 0xff;
	}
}



/*************************************
 *
 *	Main CPU memory handlers
 *
 *************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, starfire_scratch_r },
	{ 0xa000, 0xbfff, starfire_colorram_r },
	{ 0xc000, 0xffff, starfire_videoram_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0x9fff, starfire_scratch_w },
	{ 0xa000, 0xbfff, starfire_colorram_w, &starfire_colorram },
	{ 0xc000, 0xffff, starfire_videoram_w, &starfire_videoram },
	{ -1 }	/* end of table */
};



/*************************************
 *
 *	Port definitions
 *
 *************************************/

INPUT_PORTS_START( starfire )
	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, "Time" )
	PORT_DIPSETTING(    0x00, "90 Sec" )
	PORT_DIPSETTING(    0x01, "80 Sec" )
	PORT_DIPSETTING(    0x02, "70 Sec" )
	PORT_DIPSETTING(    0x03, "60 Sec" )
	PORT_DIPNAME( 0x04, 0x00, "Coin(s) to Start" )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPNAME( 0x08, 0x00, "Fuel per Coin" )
	PORT_DIPSETTING(    0x00, "300" )
	PORT_DIPSETTING(    0x08, "600" )
	PORT_DIPNAME( 0x30, 0x00, "Bonus" )
	PORT_DIPSETTING(    0x00, "300 points" )
	PORT_DIPSETTING(    0x10, "500 points" )
	PORT_DIPSETTING(    0x20, "700 points" )
	PORT_DIPSETTING(    0x30, "None" )
	PORT_DIPNAME( 0x40, 0x00, "Score Table Hold" )
	PORT_DIPSETTING(    0x00, "fixed length" )
	PORT_DIPSETTING(    0x40, "fixed length+fire" )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START  /* IN2 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X, 100, 10, 0, 255 )

	PORT_START  /* IN3 */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y | IPF_REVERSE, 100, 10, 0, 255 )

	PORT_START /* Throttle (IN4) [not really player 2, but we need to separate it out] */
	PORT_ANALOG( 0xff, 0x80, IPT_PADDLE_V | IPF_REVERSE | IPF_PLAYER2, 100, 10, 0, 255 )
INPUT_PORTS_END


INPUT_PORTS_START( fireone )
	PORT_START      /* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, "2 Coins/1 Player" )
	PORT_DIPSETTING(    0x02, "2 Coins/1 or 2 Players" )
	PORT_DIPSETTING(    0x00, "1 Coin/1 Player" )
	PORT_DIPSETTING(    0x01, "1 Coin/1 or 2 Players" )
	PORT_DIPNAME( 0x0c, 0x0c, "Time" )
	PORT_DIPSETTING(    0x00, "75 Sec" )
	PORT_DIPSETTING(    0x04, "90 Sec" )
	PORT_DIPSETTING(    0x08, "105 Sec" )
	PORT_DIPSETTING(    0x0c, "120 Sec" )
	PORT_DIPNAME( 0x30, 0x00, "Bonus difficulty" )
	PORT_DIPSETTING(    0x00, "Easy" )
	PORT_DIPSETTING(    0x10, "Normal" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x30, "Very hard" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_HIGH )

	PORT_START      /* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 )
	PORT_BIT_IMPULSE( 0x04, IP_ACTIVE_HIGH, IPT_COIN1, 1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START  /* IN2 */
	PORT_ANALOG( 0x3f, 0x20, IPT_PADDLE | IPF_PLAYER1, 50, 1, 0, 63 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER1 )

	PORT_START  /* IN3 */
	PORT_ANALOG( 0x3f, 0x20, IPT_PADDLE | IPF_PLAYER2, 50, 1, 0, 63 )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
INPUT_PORTS_END



/*************************************
 *
 *	Machine driver
 *
 *************************************/

static struct MachineDriver machine_driver_starfire =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
            2500000,
			readmem,writemem,0,0,
            nmi_interrupt,1
		}
	},
	57, DEFAULT_60HZ_VBLANK_DURATION,
	1,
    init_machine,

	/* video hardware */
    256,256,
    { 0, 256-1, 32, 256-1 },
    0,
    64,64,0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
    starfire_vh_start,
    starfire_vh_stop,
    starfire_vh_screenrefresh,

	/* sound hardware */
    0,0,0,0
};



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( starfire )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "sfire.1a",     0x0000, 0x0800, 0x9990af64 )
	ROM_LOAD( "sfire.2a",     0x0800, 0x0800, 0x6e17ba33 )
	ROM_LOAD( "sfire.1b",     0x1000, 0x0800, 0x946175d0 )
	ROM_LOAD( "sfire.2b",     0x1800, 0x0800, 0x67be4275 )
	ROM_LOAD( "sfire.1c",     0x2000, 0x0800, 0xc56b4e07 )
	ROM_LOAD( "sfire.2c",     0x2800, 0x0800, 0xb4b9d3a7 )
	ROM_LOAD( "sfire.1d",     0x3000, 0x0800, 0xfd52ffb5 )
	ROM_LOAD( "sfire.2d",     0x3800, 0x0800, 0x51c69fe3 )
	ROM_LOAD( "sfire.1e",     0x4000, 0x0800, 0x01994ec8 )
	ROM_LOAD( "sfire.2e",     0x4800, 0x0800, 0xef3d1b71 )
	ROM_LOAD( "sfire.1f",     0x5000, 0x0800, 0xaf31dc39 )
ROM_END

ROM_START( fireone )
	ROM_REGION( 0x10000, REGION_CPU1 )     /* 64k for code */
	ROM_LOAD( "fo-ic13.7b",     0x0000, 0x0800, 0xf927f086 )
	ROM_LOAD( "fo-ic24.7c",     0x0800, 0x0800, 0x0d2d8723 )
	ROM_LOAD( "fo-ic12.6b",     0x1000, 0x0800, 0xac7783d9 )
	ROM_LOAD( "fo-ic23.6c",     0x1800, 0x0800, 0x15c74ee7 )
	ROM_LOAD( "fo-ic11.5b",     0x2000, 0x0800, 0x721930a1 )
	ROM_LOAD( "fo-ic22.5c",     0x2800, 0x0800, 0xf0c965b4 )
	ROM_LOAD( "fo-ic10.4b",     0x3000, 0x0800, 0x27a7b2c0 )
	ROM_LOAD( "fo-ic21.4c",     0x3800, 0x0800, 0xb142c857 )
	ROM_LOAD( "fo-ic09.3b",     0x4000, 0x0800, 0x1c076b1b )
	ROM_LOAD( "fo-ic20.3c",     0x4800, 0x0800, 0xb4ac6e71 )
	ROM_LOAD( "fo-ic08.2b",     0x5000, 0x0800, 0x5839e2ff )
	ROM_LOAD( "fo-ic19.2c",     0x5800, 0x0800, 0x9fd85e11 )
	ROM_LOAD( "fo-ic07.1b",     0x6000, 0x0800, 0xb90baae1 )
	ROM_LOAD( "fo-ic18.1c",     0x6800, 0x0800, 0x771ee5ba )
ROM_END



/*************************************
 *
 *	Driver init
 *
 *************************************/

static void init_starfire(void)
{
	input_read = starfire_input_r;
}

static void init_fireone(void)
{
	input_read = fireone_input_r;
}



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAMEX( 1979, starfire, 0, starfire, starfire, starfire, ROT0, "Exidy", "Star Fire", GAME_NO_SOUND )
GAMEX( 1979, fireone,  0, starfire, fireone,  fireone,  ROT0, "Exidy", "Fire One", GAME_NO_SOUND )

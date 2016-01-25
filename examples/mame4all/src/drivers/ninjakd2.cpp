#include "../vidhrdw/ninjakd2.cpp"

/***************************************************************************

 *****************************
 *** NINJA KID II hardware ***		(by Roberto Ventura)
 *****************************

Game authors:
Game design:    Tsutomu Fuzisawa
Program design: Satoru Kinjo
Char. design:   Tsutomu Fizisawa
Char. design:   Akemi Tsunoda
Sound compose:  Tsutomu Fuzisawa
Bgm create:     Mecano Associate
Data make:      Takashi Hayashi

General aspect.

The game is driven by a fast Z80 CPU.
Screen resolution is 256x192.
768 colors on screen.
96 sprites.

Rom Contents:

NK2_01.ROM              Main CPU program ROM
NK2_02.ROM              CPU banked data ROM 0 (banks 0 and 1)
NK2_03.ROM              CPU banked data ROM 1 (banks 2 and 3)
NK2_04.ROM              CPU banked data ROM 2 (banks 4 and 5)
NK2_05.ROM              CPU banked data ROM 3 (banks 6 and 7)
NK2_06.ROM              Sound CPU program ROM (encrypted)
NK2_07.ROM              Sprites data ROM 1
NK2_08.ROM              Sprites data ROM 0
NK2_09.ROM              Raw PCM samples (complete?)
NK2_10.ROM              Background data ROM 1
NK2_11.ROM              Background data ROM 0
NK2_12.ROM              Foreground data ROM

*** MEMORY MAP ***

0000-7fff       Main CPU ROM
8000-bfff       Banked CPU ROM
c000-c7ff       I/O
c800-cdff       Color RAM
d000-d7ff       "FRONT" tile map
d800-dfff       "BACK" tile map
e000-efff       Main RAM
f400-f7ff	??? screen frame ???
f800-f9ff	CPU Stack
fa00-ffff       Sprite registers (misc RAM)


1) CPU

1 OSC 12 Mhz
1 OSC 5 Mhz
2 YM 2203C.......CaBBe!.

The Z80 runs in IM0,the game expects execution of RST10 each
frame.

Game level maps,additional code and data are available to main
program via CPU banking at lacations 8000-bf00

The encryption scheme for ROM 6(alternate version) is very simple,
I was surprised by such a large ROM (64k) for sound.
The first half contains opcodes only (see machine 1 pin)
while the second 32k chunk is for immediate data.
Opcode and Data keep their relative positions as well.



2) I/O

c000    "KEYCOIN" button

        76543210
        || |  ||
        || |  |^-COIN 1
        || |  ^--COIN 2
        || ^-----TEST MODE (on the fly,to be pressed during boot)
        |^-------START 1
        ^--------START 2


c001	"PAD1"
c002	"PAD2"

        76543210
          ||||||
          |||||^-RIGHT
          ||||^--LEFT
          |||^---DOWN
          ||^----UP
          |^-----FIRE 0
          ^------FIRE 1


c003    "DIPSW1"

        76543210
        ||||||||
        |||||||^-UNUSED?
        ||||||^-->EXTRA
        |||||^--->LIVES
        ||||^----CONTINUE MODE
        |||^-----DEMO SOUND
        ||^------NORMAL/HARD
        |^-------LIVES 3/4
        ^--------ENGLISH/JAP


c004    "DIPSW2"

        76543210
        ||||||||
        |||||||^-TEST MODE
        ||||||^--TABLE/UPRIGHT
        |||||^---"CREDIT SERVICE"
        ||||^---->
        |||^----->>
        ||^------>>> coin/credit configurations
        |^------->>
        ^-------->

c200	Sound command?
	This byte is written when game plays sound effects...
	it is set when music or sound effects (both pcm and fm) are triggered;
	I guess it is read by another CPU,then.

c201	Unknown,but used.

c202    Bank selector (0 to 7)

c203    Sprite 'overdraw'
        this is the most interesting feature of the game,when set
        the sprites drawn in the previous frame remain on the
        screen,so the game can perform special effects such as the
        'close up' when you die or the "infinite balls" appearing
        when an extra weapon is collected.
        Appears to work like a xor mask,a sprite removes older
        sprite 'bitmap' when present;other memory locations connected to
	this function may be f400-f7ff...should be investigated more.
        -mmmh... I believe this is sci-fiction for a non-bitmap game...

C208    Scroll X  0-7

C209    Scroll X  MSB

C20A    Scroll Y  0-7

C20B    Scroll Y  MSB

C20C    Background ENABLE
        when set to zero the background is totally black,but
        sprites are drawn correctly.


3) COLOR RAM

The palette system is dynamic,the game can show up to 768 different
colors on screen.

Palette depth is 12 bits as usual,two consecutive bytes are used for each
color code.

format: RRRRGGGGBBBB0000

Colors are organized in palettes,since graphics is 4 bits (16 colors)
each palette takes 32 bytes,the three different layers,BACK,SPRITES and
FRONT don't share any color,each has its own 256 color space,hence the
768 colors on screen.

c800-c9ff       Background palettes
ca00-cbff       Sprites palettes
cc00-cdff       Foreground palettes


4) TILE-BASED LAYERS

The tile format for background and foreground is the same,the
only difference is that background tiles are 16x16 pixels while foreground
tiles are only 8x8.

Background virtual screen is 512x512 pixels scrollable.

Two consecutive tiles bytes identify one tile.

        O7 O6 O5 O4 O3 O2 O1 O0         gfx Offset
        O9 O8 FY FX C3 C2 C1 C0         Attibute

        O= GFX offset (1024 tiles)
        F= Flip X and Y
        C= Color palette selector


5) SPRITES

Five bytes identify each sprite,but the registers actually used
are placed at intervals of 16.
Some of the remaining bytes are used (e.g. fa00),their meaning is totally
unknown to me,they seem to be related to the surprising additional sprite
feature of the game,but maybe they're just random writes in RAM.

The first sprite data is located at fa0b,then fa1b and so on.


0b      Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0         Y coord
0c      X7 X6 X5 X4 X3 X2 X1 X0         X coord
0d      O9 O8 FY FX -- -- EN X8         hi gfx - FLIP - Enable - hi X
0e      O7 O6 O5 O4 O3 O2 O1 O0         gfx - offset
0f      -- -- -- -- C3 C2 C1 C0         color

        Y= Y coordinate
        X= X coordinate (X8 is used to clip sprite on the left)
        O= Gfx offset (1024 sprites)
        F= Flip
       EN= Enable (this is maybe the Y8 coordinate bit,but it isn't set
           accordingly in the test mode
        C= Color palette selector

***************************************************************************/


#include "driver.h"
#include "vidhrdw/generic.h"

WRITE_HANDLER( ninjakd2_bgvideoram_w );
WRITE_HANDLER( ninjakd2_fgvideoram_w );
WRITE_HANDLER( ninjakd2_sprite_overdraw_w );
WRITE_HANDLER( ninjakd2_background_enable_w );
int  ninjakd2_vh_start(void);
void ninjakd2_vh_stop(void);
void ninjakd2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char 	*ninjakd2_scrolly_ram;
extern unsigned char 	*ninjakd2_scrollx_ram;
extern unsigned char 	*ninjakd2_bgenable_ram;
extern unsigned char 	*ninjakd2_spoverdraw_ram;
extern unsigned char 	*ninjakd2_spriteram;
extern unsigned char 	*ninjakd2_background_videoram;
extern unsigned char 	*ninjakd2_foreground_videoram;
extern size_t ninjakd2_spriteram_size;
extern size_t ninjakd2_backgroundram_size;
extern size_t ninjakd2_foregroundram_size;

static int ninjakd2_bank_latch = 255;



int ninjakd2_init_samples(const struct MachineSound *msound)
{
	int i,n;
	unsigned char *source = memory_region(REGION_SOUND1);
	struct GameSamples *samples;
	int sample_info [9][2] = { {0x0000,0x0A00},{0x0A00,0x1D00},{0x2700,0x1700},
	{0x3E00,0x1500},{0x5300,0x0B00},{0x5E00,0x0A00},{0x6800,0x0E00},{0x7600,0x1E00},{0xF000,0x0400} };

	if ((Machine->samples = (struct GameSamples*)malloc(sizeof(struct GameSamples) + 9 * sizeof(struct GameSample *))) == NULL)
		return 1;

	samples = Machine->samples;
	samples->total = 8;

	for (i=0;i<8;i++)
	{
		if ((samples->sample[i] = (struct GameSample*)malloc(sizeof(struct GameSample) + (sample_info[i][1]))) == NULL)
			return 1;

		samples->sample[i]->length = sample_info[i][1];
		samples->sample[i]->smpfreq = 16000;	/* 16 kHz */
		samples->sample[i]->resolution = 8;
		for (n=0; n<sample_info[i][1]; n++)
			samples->sample[i]->data[n] = source[sample_info[i][0]+n] ^ 0x80;
	}

	/*	The samples are now ready to be used.  They are a 8 bit, 16 khz samples. 	 */

	return 0;
}


int ninjakd2_interrupt(void)
{
	return 0x00d7;	/* RST 10h */
}

READ_HANDLER( ninjakd2_bankselect_r )
{
	return ninjakd2_bank_latch;
}

WRITE_HANDLER( ninjakd2_bankselect_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);
	int bankaddress;

	if (data != ninjakd2_bank_latch)
	{
		ninjakd2_bank_latch = data;

		bankaddress = 0x10000 + ((data & 0x7) * 0x4000);
		cpu_setbank(1,&RAM[bankaddress]);	 /* Select 8 banks of 16k */
	}
}

WRITE_HANDLER( ninjakd2_pcm_play_w )
{
	int i;
	int sample_no[9] = { 0x00,0x0A,0x27,0x3E,0x53,0x5E,0x68,0x76,0xF0 };

	for(i=0;i<9;i++)
	 if (sample_no[i]==data) break;

	if (i==8)
		sample_stop(0);
	else
		sample_start(0,i,0);
}

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 },
	{ 0xc000, 0xc000, input_port_2_r },
	{ 0xc001, 0xc001, input_port_0_r },
	{ 0xc002, 0xc002, input_port_1_r },
	{ 0xc003, 0xc003, input_port_3_r },
	{ 0xc004, 0xc004, input_port_4_r },
	{ 0xc200, 0xc200, MRA_RAM },
	{ 0xc201, 0xc201, MRA_RAM },		// unknown but used
	{ 0xc202, 0xc202, ninjakd2_bankselect_r },
	{ 0xc203, 0xc203, MRA_RAM },
	{ 0xc208, 0xc209, MRA_RAM },
	{ 0xc20a, 0xc20b, MRA_RAM },
	{ 0xc20c, 0xc20c, MRA_RAM },
	{ 0xc800, 0xffff, MRA_RAM },
	{ -1 }	/* end of table */
};


static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc200, 0xc200, soundlatch_w },
	{ 0xc201, 0xc201, MWA_RAM },		// unknown but used
	{ 0xc202, 0xc202, ninjakd2_bankselect_w },
	{ 0xc203, 0xc203, ninjakd2_sprite_overdraw_w, &ninjakd2_spoverdraw_ram },
	{ 0xc208, 0xc209, MWA_RAM, &ninjakd2_scrollx_ram },
	{ 0xc20a, 0xc20b, MWA_RAM, &ninjakd2_scrolly_ram },
	{ 0xc20c, 0xc20c, ninjakd2_background_enable_w, &ninjakd2_bgenable_ram },
	{ 0xc800, 0xcdff, paletteram_RRRRGGGGBBBBxxxx_swap_w, &paletteram },
	{ 0xd000, 0xd7ff, ninjakd2_fgvideoram_w, &ninjakd2_foreground_videoram, &ninjakd2_foregroundram_size },
	{ 0xd800, 0xdfff, ninjakd2_bgvideoram_w, &ninjakd2_background_videoram, &ninjakd2_backgroundram_size },
	{ 0xe000, 0xf9ff, MWA_RAM },
	{ 0xfa00, 0xffff, MWA_RAM, &ninjakd2_spriteram, &ninjakd2_spriteram_size },
	{ -1 }	/* end of table */
};


static struct MemoryReadAddress snd_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM },
	{ 0xc000, 0xc7ff, MRA_RAM },
	{ 0xe000, 0xe000, soundlatch_r },
	{ 0xefee, 0xefee, MRA_NOP },
	{ -1 }	/* end of table */
};


static struct MemoryWriteAddress snd_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM },
	{ 0xc000, 0xc7ff, MWA_RAM },
	{ 0xf000, 0xf000, ninjakd2_pcm_play_w },	/* PCM SAMPLE OFFSET*256 */
	{ 0xeff5, 0xeff6, MWA_NOP },			/* SAMPLE FREQUENCY ??? */
	{ 0xefee, 0xefee, MWA_NOP },			/* CHIP COMMAND ?? */
	{ -1 }	/* end of table */
};

static struct IOWritePort snd_writeport[] =
{
	{ 0x0000, 0x0000, YM2203_control_port_0_w },
	{ 0x0001, 0x0001, YM2203_write_port_0_w },
	{ 0x0080, 0x0080, YM2203_control_port_1_w },
	{ 0x0081, 0x0081, YM2203_write_port_1_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( ninjakd2 )
    PORT_START
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

    PORT_START    /* player 2 controls */
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_COCKTAIL )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_COCKTAIL )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_COCKTAIL )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_COCKTAIL )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

    PORT_START
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE )	/* keep pressed during boot to enter service mode */
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

    PORT_START  /* dsw0 */
    PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
    PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On ) )
    PORT_DIPNAME( 0x06, 0x06, DEF_STR( Bonus_Life ) )
    PORT_DIPSETTING(    0x04, "20000 50000" )
    PORT_DIPSETTING(    0x06, "30000 50000" )
    PORT_DIPSETTING(    0x02, "50000 100000" )
    PORT_DIPSETTING(    0x00, "None" )
    PORT_DIPNAME( 0x08, 0x08, "Allow Continue" )
    PORT_DIPSETTING(    0x00, DEF_STR( No ) )
    PORT_DIPSETTING(    0x08, DEF_STR( Yes )  )
    PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
    PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x00, DEF_STR( On )  )
    PORT_DIPNAME( 0x20, 0x20, DEF_STR( Difficulty ) )
    PORT_DIPSETTING(    0x20, "Normal" )
    PORT_DIPSETTING(    0x00, "Hard" )
    PORT_DIPNAME( 0x40, 0x40, DEF_STR( Lives ) )
    PORT_DIPSETTING(    0x40, "3" )
    PORT_DIPSETTING(    0x00, "4" )
    PORT_DIPNAME( 0x80, 0x00, "Language" )
    PORT_DIPSETTING(    0x00, "English" )
    PORT_DIPSETTING(    0x80, "Japanese" )

    PORT_START  /* dsw1 */
	PORT_SERVICE( 0x01, IP_ACTIVE_LOW )
    PORT_DIPNAME( 0x02, 0x00, DEF_STR( Cabinet ) )
    PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
    PORT_DIPSETTING(    0x02, DEF_STR( Cocktail ) )
    PORT_DIPNAME( 0x04, 0x00, "Credit Service" )
    PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
    PORT_DIPSETTING(    0x04, DEF_STR( On ) )
    PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coin_B ) )
    PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0x18, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
    PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Coin_A ) )
    PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
    PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
    PORT_DIPSETTING(    0x40, DEF_STR( 3C_1C ) )
    PORT_DIPSETTING(    0x60, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(    0xe0, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
    PORT_DIPSETTING(    0xa0, DEF_STR( 1C_3C ) )
    PORT_DIPSETTING(    0x80, DEF_STR( 1C_4C ) )
INPUT_PORTS_END



static struct GfxLayout charlayout =
{
	8,8,     /* 8*8 characters */
	1024,    /* 1024 characters */
	4,       /* 4 bits per pixel */
	{0,1,2,3 }, /* the bitplanes are packed in one nibble */
	{0, 4, 16384*8+0, 16384*8+4, 8, 12, 16384*8+8, 16384*8+12 },
	{16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7 },
	8*16
};

static struct GfxLayout spritelayout =
{
	16,16,   /* 16*16 characters */
	1024,    /* 1024 sprites */
	4,       /* 4 bits per pixel */
	{0,1,2,3}, /* the bitplanes are packed in one nibble */
	{0,  4,  65536*8+0,  65536*8+4,  8, 12,  65536*8+8, 65536*8+12,
		16*8+0, 16*8+4, 16*8+65536*8+0, 16*8+65536*8+4, 16*8+8, 16*8+12, 16*8+65536*8+8, 16*8+65536*8+12},
	{16*0, 16*1, 16*2, 16*3, 16*4, 16*5, 16*6, 16*7,
		32*8+16*0, 32*8+16*1, 32*8+16*2, 32*8+16*3, 32*8+16*4, 32*8+16*5, 32*8+16*6, 32*8+16*7},
	8*64
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &spritelayout,  0*16, 16 },
	{ REGION_GFX2, 0, &spritelayout, 16*16, 16 },
	{ REGION_GFX3, 0, &charlayout,   32*16, 16 },
	{ -1 } /* end of array */
};

static struct Samplesinterface samples_interface =
{
	1,	/* 1 channel */
	25	/* volume */
};

static struct CustomSound_interface custom_interface =
{
	ninjakd2_init_samples,
	0,
	0
};

/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_irq_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_interface =
{
	2, 	 /* 2 chips */
	1500000, /* 12000000/8 MHz */
	{ YM2203_VOL(25,25), YM2203_VOL(25,25) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};


static struct MachineDriver machine_driver_ninjakd2 =
{
	{
		{
			CPU_Z80,
			6000000,		/* 12000000/2 ??? */
			readmem,writemem,0,0,	/* very sensitive to these settings */
			ninjakd2_interrupt,1
		}
	},
	60, 10000,			/* frames per second, vblank duration */
	10,				/* single CPU, no need for interleaving */
	0,
	32*8, 32*8,
	{ 0*8, 32*8-1, 4*8, 28*8-1 },
	gfxdecodeinfo,
	48*16,48*16,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ninjakd2_vh_start,
	ninjakd2_vh_stop,
	ninjakd2_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

static struct MachineDriver machine_driver_ninjak2a =
{
	{
		{
			CPU_Z80,
			6000000,		/* 12000000/2 ??? */
			readmem,writemem,0,0,	/* very sensitive to these settings */
			ninjakd2_interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,		/* 12000000/3 ??? */
			snd_readmem,snd_writemem,0,snd_writeport,
			ignore_interrupt,0,
		}
	},
	60, 10000,	/* frames per second, vblank duration */
	10,
	0,
	32*8, 32*8,
	{ 0*8, 32*8-1, 4*8, 28*8-1 },
	gfxdecodeinfo,
	48*16,48*16,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	ninjakd2_vh_start,
	ninjakd2_vh_stop,
	ninjakd2_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_SAMPLES,
			&samples_interface
		},
		{
			SOUND_CUSTOM,	/* actually initializes the samples */
			&custom_interface
		}
	}
};



ROM_START( ninjakd2 )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "nk2_01.rom",   0x00000, 0x8000, 0x3cdbb906 )
	ROM_LOAD( "nk2_02.rom",   0x10000, 0x8000, 0xb5ce9a1a )
	ROM_LOAD( "nk2_03.rom",   0x18000, 0x8000, 0xad275654 )
	ROM_LOAD( "nk2_04.rom",   0x20000, 0x8000, 0xe7692a77 )
	ROM_LOAD( "nk2_05.rom",   0x28000, 0x8000, 0x5dac9426 )

	ROM_REGION( 0x10000, REGION_CPU2 )
	ROM_LOAD( "nk2_06.rom",   0x0000, 0x10000, 0xd3a18a79 )  // sound z80 code encrypted

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_11.rom",   0x00000, 0x4000, 0x41a714b3 )	/* background tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_10.rom",   0x08000, 0x4000, 0xc913c4ab )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_08.rom",   0x00000, 0x4000, 0x1b79c50a )	/* sprites tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_07.rom",   0x08000, 0x4000, 0x0be5cd13 )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_12.rom",   0x00000, 0x02000, 0xdb5657a9 )	/* foreground tiles */
	ROM_CONTINUE(             0x04000, 0x02000)
	ROM_CONTINUE(             0x02000, 0x02000)
	ROM_CONTINUE(             0x06000, 0x02000)

	ROM_REGION( 0x10000, REGION_SOUND1 )
	ROM_LOAD( "nk2_09.rom",   0x0000, 0x10000, 0xc1d2d170 )	/* raw pcm samples */
ROM_END

ROM_START( ninjak2a )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "nk2_01.bin",   0x00000, 0x8000, 0xe6adca65 )
	ROM_LOAD( "nk2_02.bin",   0x10000, 0x8000, 0xd9284bd1 )
	ROM_LOAD( "nk2_03.rom",   0x18000, 0x8000, 0xad275654 )
	ROM_LOAD( "nk2_04.rom",   0x20000, 0x8000, 0xe7692a77 )
	ROM_LOAD( "nk2_05.bin",   0x28000, 0x8000, 0x960725fb )

	ROM_REGION( 2*0x10000, REGION_CPU2 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "nk2_06.bin",   0x10000, 0x8000, 0x7bfe6c9e )	/* decrypted opcodes */
	ROM_CONTINUE(             0x00000, 0x8000 )				/* decrypted data */

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_11.rom",   0x00000, 0x4000, 0x41a714b3 )	/* background tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_10.rom",   0x08000, 0x4000, 0xc913c4ab )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_08.rom",   0x00000, 0x4000, 0x1b79c50a )	/* sprites tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_07.rom",   0x08000, 0x4000, 0x0be5cd13 )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_12.rom",   0x00000, 0x02000, 0xdb5657a9 )	/* foreground tiles */
	ROM_CONTINUE(             0x04000, 0x02000)
	ROM_CONTINUE(             0x02000, 0x02000)
	ROM_CONTINUE(             0x06000, 0x02000)

	ROM_REGION( 0x10000, REGION_SOUND1 )
	ROM_LOAD( "nk2_09.rom",   0x0000, 0x10000, 0xc1d2d170 )	/* raw pcm samples */
ROM_END

ROM_START( ninjak2b )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "1.3s",         0x00000, 0x8000, 0xcb4f4624 )
	ROM_LOAD( "2.3q",         0x10000, 0x8000, 0x0ad0c100 )
	ROM_LOAD( "nk2_03.rom",   0x18000, 0x8000, 0xad275654 )
	ROM_LOAD( "nk2_04.rom",   0x20000, 0x8000, 0xe7692a77 )
	ROM_LOAD( "nk2_05.rom",   0x28000, 0x8000, 0x5dac9426 )

	ROM_REGION( 2*0x10000, REGION_CPU2 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "nk2_06.bin",   0x10000, 0x8000, 0x7bfe6c9e )	/* decrypted opcodes */
	ROM_CONTINUE(             0x00000, 0x8000 )				/* decrypted data */

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_11.rom",   0x00000, 0x4000, 0x41a714b3 )	/* background tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_10.rom",   0x08000, 0x4000, 0xc913c4ab )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_08.rom",   0x00000, 0x4000, 0x1b79c50a )	/* sprites tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_07.rom",   0x08000, 0x4000, 0x0be5cd13 )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_12.rom",   0x00000, 0x02000, 0xdb5657a9 )	/* foreground tiles */
	ROM_CONTINUE(             0x04000, 0x02000)
	ROM_CONTINUE(             0x02000, 0x02000)
	ROM_CONTINUE(             0x06000, 0x02000)

	ROM_REGION( 0x10000, REGION_SOUND1 )
	ROM_LOAD( "nk2_09.rom",   0x0000, 0x10000, 0xc1d2d170 )	/* raw pcm samples */
ROM_END

ROM_START( rdaction )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "1.3u",  	      0x00000, 0x8000, 0x5c475611 )
	ROM_LOAD( "2.3s",         0x10000, 0x8000, 0xa1e23bd2 )
	ROM_LOAD( "nk2_03.rom",   0x18000, 0x8000, 0xad275654 )
	ROM_LOAD( "nk2_04.rom",   0x20000, 0x8000, 0xe7692a77 )
	ROM_LOAD( "nk2_05.bin",   0x28000, 0x8000, 0x960725fb )

	ROM_REGION( 2*0x10000, REGION_CPU2 )	/* 64k for code + 64k for decrypted opcodes */
	ROM_LOAD( "nk2_06.bin",   0x10000, 0x8000, 0x7bfe6c9e )	/* decrypted opcodes */
	ROM_CONTINUE(             0x00000, 0x8000 )				/* decrypted data */

	ROM_REGION( 0x20000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_11.rom",   0x00000, 0x4000, 0x41a714b3 )	/* background tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_10.rom",   0x08000, 0x4000, 0xc913c4ab )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x20000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "nk2_08.rom",   0x00000, 0x4000, 0x1b79c50a )	/* sprites tiles */
	ROM_CONTINUE(             0x10000, 0x4000)
	ROM_CONTINUE(             0x04000, 0x4000)
	ROM_CONTINUE(             0x14000, 0x4000)
	ROM_LOAD( "nk2_07.rom",   0x08000, 0x4000, 0x0be5cd13 )
	ROM_CONTINUE(             0x18000, 0x4000)
	ROM_CONTINUE(             0x0c000, 0x4000)
	ROM_CONTINUE(             0x1c000, 0x4000)

	ROM_REGION( 0x08000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "12.5n",        0x00000, 0x02000, 0x0936b365 )	/* foreground tiles */
	ROM_CONTINUE(             0x04000, 0x02000)
	ROM_CONTINUE(             0x02000, 0x02000)
	ROM_CONTINUE(             0x06000, 0x02000)

	ROM_REGION( 0x10000, REGION_SOUND1 )
	ROM_LOAD( "nk2_09.rom",   0x0000, 0x10000, 0xc1d2d170 )	/* raw pcm samples */
ROM_END



void init_ninjak2a(void)
{
	unsigned char *rom = memory_region(REGION_CPU2);
	int diff = memory_region_length(REGION_CPU2) / 2;

	memory_set_opcode_base(1,rom+diff);
}



GAMEX(1987, ninjakd2, 0,        ninjakd2, ninjakd2, 0,        ROT0, "UPL", "Ninja Kid II (set 1)", GAME_NO_SOUND )	/* sound program is encrypted */
GAME( 1987, ninjak2a, ninjakd2, ninjak2a, ninjakd2, ninjak2a, ROT0, "UPL", "Ninja Kid II (set 2)" )
GAME( 1987, ninjak2b, ninjakd2, ninjak2a, ninjakd2, ninjak2a, ROT0, "UPL", "Ninja Kid II (set 3)" )
GAME( 1987, rdaction, ninjakd2, ninjak2a, ninjakd2, ninjak2a, ROT0, "UPL (World Games license)", "Rad Action" )

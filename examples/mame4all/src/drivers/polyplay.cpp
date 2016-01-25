#include "../vidhrdw/polyplay.cpp"
#include "../sndhrdw/polyplay.cpp"

/***************************************************************************

      Poly-Play
	  (c) 1985 by VEB Polytechnik Karl-Marx-Stadt

      driver by Martin Buchholz (buchholz@mail.uni-greifswald.de)

      Very special thanks to the following people, each one of them spent
	  some of their spare time to make this driver working:
	  - Juergen Oppermann and Volker Hann for electronical assistance,
	    repair work and ROM dumping.
	  - Jan-Ole Christian from the Videogamemuseum in Berlin, which houses
	    one of the last existing Poly-Play arcade automatons. He also
		provided me with schematics and service manuals.


memory map:

0000 - 03ff OS ROM
0400 - 07ff Game ROM (used for Abfahrtslauf)
0800 - 0cff Menu Screen ROM

0d00 - 0fff work RAM

1000 - 4fff GAME ROM (pcb 2 - Abfahrtslauf          (1000 - 1bff),
                              Hirschjagd            (1c00 - 27ff),
							  Hase und Wolf         (2800 - 3fff),
                              Schmetterlingsfang    (4000 - 4fff)
5000 - 8fff GAME ROM (pcb 1 - Schiessbude           (5000 - 5fff)
                              Autorennen            (6000 - 73ff)
							  opto-akust. Merkspiel (7400 - 7fff)
							  Wasserrohrbruch       (8000 - 8fff)

e800 - ebff character ROM (chr 00..7f) 1 bit per pixel
ec00 - f7ff character RAM (chr 80..ff) 3 bit per pixel
f800 - ffff video RAM

I/O ports:

read:

83        IN1
          used as hardware random number generator

84        IN0
          bit 0 = fire button
          bit 1 = right
          bit 2 = left
          bit 3 = up
          bit 4 = down
          bit 5 = unused
          bit 6 = Summe Spiele
          bit 7 = coinage (+IRQ to make the game acknowledge it)

85        bit 0-4 = light organ (unemulated :)) )
          bit 5-7 = sound parameter (unemulated, it's very difficult to
		            figure out how those work)

86        ???

87        PIO Control register

write:
80	      Sound Channel 1
81        Sound Channel 2
82        generates 40 Hz timer for timeout in game title screens
83        generates main 75 Hz timer interrupt

The Poly-Play has a simple bookmarking system which can be activated
setting Bit 6 of PORTA (Summe Spiele) to low. It reads a double word
from 0c00 and displays it on the screen.
I currently haven't figured out how the I/O port handling for the book-
mark system works.

Uniquely the Poly-Play has a light organ which totally confuses you whilst
playing the automaton. Bits 1-5 of PORTB control the organ but it's not
emulated now. ;)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

static unsigned char *polyplay_ram;

void polyplay_reset(void);

/* video hardware access */
extern unsigned char *polyplay_characterram;
void polyplay_init_palette(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void polyplay_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
READ_HANDLER( polyplay_characterram_r );
WRITE_HANDLER( polyplay_characterram_w );

/* I/O Port handling */
READ_HANDLER( polyplay_input_read );
READ_HANDLER( polyplay_random_read );

/* sound handling */
void poly_sound(void);
void set_channel1(int active);
void set_channel2(int active);
int prescale1, prescale2;
static int channel1_active;
static int channel1_const;
static int channel2_active;
static int channel2_const;
static void init_polyplay_sound(void);
void play_channel1(int data);
void play_channel2(int data);
int  polyplay_sh_start(const struct MachineSound *msound);
void polyplay_sh_stop(void);
void polyplay_sh_update(void);

/* timer handling */
static void polyplay_timer(int param);
int timer2_active;
WRITE_HANDLER( polyplay_start_timer2 );
WRITE_HANDLER( polyplay_sound_channel );


/* Polyplay Sound Interface */
static struct CustomSound_interface custom_interface =
{
	polyplay_sh_start,
	polyplay_sh_stop,
	polyplay_sh_update
};


void polyplay_reset(void)
{
	channel1_active = 0;
	channel1_const = 0;
	channel2_active = 0;
	channel2_const = 0;
	set_channel1(0);
	play_channel1(0);
	set_channel2(0);
	play_channel2(0);
}


/* work RAM access */
WRITE_HANDLER( polyplay_ram_w )
{
	polyplay_ram[offset] = data;
}

READ_HANDLER( polyplay_ram_r )
{
	return polyplay_ram[offset];
}

/* memory mapping */
static struct MemoryReadAddress polyplay_readmem[] =
{
	{ 0x0000, 0x0bff, MRA_ROM },
	{ 0x0c00, 0x0fff, polyplay_ram_r },
	{ 0x1000, 0x8fff, MRA_ROM },

	{ 0xe800, 0xebff, MRA_ROM},
	{ 0xec00, 0xf7ff, polyplay_characterram_r },
	{ 0xf800, 0xffff, videoram_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress polyplay_writemem[] =
{
	{ 0x0000, 0x0bff, MWA_ROM },
	{ 0x0c00, 0x0fff, polyplay_ram_w, &polyplay_ram },
	{ 0x1000, 0x8fff, MWA_ROM },

	{ 0xe800, 0xebff, MWA_ROM },
	{ 0xec00, 0xf7ff, polyplay_characterram_w, &polyplay_characterram },
	{ 0xf800, 0xffff, videoram_w, &videoram, &videoram_size },
	{ -1 }  /* end of table */
};


/* port mapping */
static struct IOReadPort readport_polyplay[] =
{
	{ 0x84, 0x84, polyplay_input_read },
	{ 0x83, 0x83, polyplay_random_read },
	{ -1 }  /* end of table */
};

static struct IOWritePort writeport_polyplay[] =
{	{ 0x80, 0x81, polyplay_sound_channel },
	{ 0x82, 0x82, polyplay_start_timer2 },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( polyplay )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_SERVICE, "Bookkeeping Info", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT_IMPULSE( 0x80, IP_ACTIVE_LOW, IPT_COIN1, 10)
INPUT_PORTS_END


WRITE_HANDLER( polyplay_sound_channel )
{
	switch(offset) {
	case 0x00:
		if (channel1_const) {
			if (data <= 1) {
				set_channel1(0);
			}
			channel1_const = 0;
			play_channel1(data*prescale1);

		}
		else {
			prescale1 = (data & 0x20) ? 16 : 1;
			if (data & 0x04) {
				set_channel1(1);
				channel1_const = 1;
			}
			if ((data == 0x41) || (data == 0x65) || (data == 0x45)) {
				set_channel1(0);
				play_channel1(0);
			}
		}
		break;
	case 0x01:
		if (channel2_const) {
			if (data <= 1) {
				set_channel2(0);
			}
			channel2_const = 0;
			play_channel2(data*prescale2);

		}
		else {
			prescale2 = (data & 0x20) ? 16 : 1;
			if (data & 0x04) {
				set_channel2(1);
				channel2_const = 1;
			}
			if ((data == 0x41) || (data == 0x65) || (data == 0x45)) {
				set_channel2(0);
				play_channel2(0);
			}
		}
		break;
	}
}

WRITE_HANDLER( polyplay_start_timer2 )
{
	if (data == 0x03) {
		timer2_active = 0;
	}
	else {
		if (data == 0xb5) {
			timer_set(TIME_IN_HZ(40), 1, polyplay_timer);
			timer2_active = 1;
		}
	}
}

/* random number generator */
READ_HANDLER( polyplay_random_read )
{
	return rand() % 0xff;
}

/* graphic sturctures */
static struct GfxLayout charlayout_1_bit =
{
	8,8,	/* 8*8 characters */
	128,	/* 128 characters */
	1,	    /* 1 bit per pixel */
	{ 0 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout charlayout_3_bit =
{
	8,8,	/* 8*8 characters */
	128,	/* 128 characters */
	3,	    /* 3 bit per pixel */
	{ 0, 128*8*8, 128*8*8 + 128*8*8 },    /* offset for each bitplane */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },	/* pretty straightforward layout */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 0, 0xe800, &charlayout_1_bit, 0, 2 },
	{ 0, 0xec00, &charlayout_3_bit, 2, 8 },
	{ -1 }	/* end of array */
};


/* the machine driver */

#define MACHINEDRIVER(NAME, MEM, PORT)				\
static struct MachineDriver machine_driver_##NAME =	\
{													\
	/* basic machine hardware */					\
	{												\
		{											\
			CPU_Z80,								\
			9830400/4,								\
			MEM##_readmem,MEM##_writemem,           \
			readport_##PORT,writeport_polyplay,	    \
			ignore_interrupt, 1					\
		}											\
	},												\
	50, 0,	/* frames per second, vblank duration */ \
	0,	/* single CPU, no need for interleaving */	\
	polyplay_reset,									\
													\
	/* video hardware */							\
	64*8, 32*8, { 0*8, 64*8-1, 0*8, 32*8-1 },		\
	gfxdecodeinfo,									\
	64, 64,											\
	polyplay_init_palette,					        \
													\
	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,			\
	0,												\
	generic_vh_start,								\
	generic_vh_stop,								\
	polyplay_vh_screenrefresh,						\
													\
	/* sound hardware */							\
	0,0,0,0,											\
	{							                    \
		{						                    \
			SOUND_CUSTOM,		                    \
			&custom_interface	                    \
		}						                    \
	}							                    \
};

MACHINEDRIVER( polyplay, polyplay, polyplay )



/* ROM loading and mapping */
ROM_START( polyplay )
	ROM_REGION( 0x10000, REGION_CPU1 )	/* 64k for code */
	ROM_LOAD( "cpu_0000.37",       0x0000, 0x0400, 0x87884c5f )
	ROM_LOAD( "cpu_0400.36",       0x0400, 0x0400, 0xd5c84829 )
	ROM_LOAD( "cpu_0800.35",       0x0800, 0x0400, 0x5f36d08e )
	ROM_LOAD( "2_-_1000.14",       0x1000, 0x0400, 0x950dfcdb )
	ROM_LOAD( "2_-_1400.10",       0x1400, 0x0400, 0x829f74ca )
	ROM_LOAD( "2_-_1800.6",        0x1800, 0x0400, 0xb69306f5 )
	ROM_LOAD( "2_-_1c00.2",        0x1c00, 0x0400, 0xaede2280 )
	ROM_LOAD( "2_-_2000.15",       0x2000, 0x0400, 0x6c7ad0d8 )
	ROM_LOAD( "2_-_2400.11",       0x2400, 0x0400, 0xbc7462f0 )
	ROM_LOAD( "2_-_2800.7",        0x2800, 0x0400, 0x9ccf1958 )
	ROM_LOAD( "2_-_2c00.3",        0x2c00, 0x0400, 0x21827930 )
	ROM_LOAD( "2_-_3000.16",       0x3000, 0x0400, 0xb3b3c0ec )
	ROM_LOAD( "2_-_3400.12",       0x3400, 0x0400, 0xbd416cd0 )
	ROM_LOAD( "2_-_3800.8",        0x3800, 0x0400, 0x1c470b7c )
	ROM_LOAD( "2_-_3c00.4",        0x3c00, 0x0400, 0xb8354a19 )
	ROM_LOAD( "2_-_4000.17",       0x4000, 0x0400, 0x1e01041e )
	ROM_LOAD( "2_-_4400.13",       0x4400, 0x0400, 0xfe4d8959 )
	ROM_LOAD( "2_-_4800.9",        0x4800, 0x0400, 0xc45f1d9d )
	ROM_LOAD( "2_-_4c00.5",        0x4c00, 0x0400, 0x26950ad6 )
	ROM_LOAD( "1_-_5000.30",       0x5000, 0x0400, 0x9f5e2ba1 )
	ROM_LOAD( "1_-_5400.26",       0x5400, 0x0400, 0xb5f9a780 )
	ROM_LOAD( "1_-_5800.22",       0x5800, 0x0400, 0xd973ad12 )
	ROM_LOAD( "1_-_5c00.18",       0x5c00, 0x0400, 0x9c22ea79 )
	ROM_LOAD( "1_-_6000.31",       0x6000, 0x0400, 0x245c49ca )
	ROM_LOAD( "1_-_6400.27",       0x6400, 0x0400, 0x181e427e )
	ROM_LOAD( "1_-_6800.23",       0x6800, 0x0400, 0x8a6c1f97 )
	ROM_LOAD( "1_-_6c00.19",       0x6c00, 0x0400, 0x77901dc9 )
	ROM_LOAD( "1_-_7000.32",       0x7000, 0x0400, 0x83ffbe57 )
	ROM_LOAD( "1_-_7400.28",       0x7400, 0x0400, 0xe2a66531 )
	ROM_LOAD( "1_-_7800.24",       0x7800, 0x0400, 0x1d0803ef )
	ROM_LOAD( "1_-_7c00.20",       0x7c00, 0x0400, 0x17dfa7e4 )
	ROM_LOAD( "1_-_8000.33",       0x8000, 0x0400, 0x6ee02375 )
	ROM_LOAD( "1_-_8400.29",       0x8400, 0x0400, 0x9db09598 )
	ROM_LOAD( "1_-_8800.25",       0x8800, 0x0400, 0xca2f963f )
	ROM_LOAD( "1_-_8c00.21",       0x8c00, 0x0400, 0x0c7dec2d )
	ROM_LOAD( "char.1",            0xe800, 0x0400, 0x5242dd6b )
ROM_END


/* interrupt handling, the game runs in IM 2 */
READ_HANDLER( polyplay_input_read )
{
	int inp = input_port_0_r(offset);

	if ((inp & 0x80) == 0) {    /* Coin inserted */
		cpu_cause_interrupt(0, 0x50);
		coin_counter_w(0, 1);
		timer_set(TIME_IN_SEC(1), 2, polyplay_timer);
	}

	return inp;
}

static void polyplay_timer(int param)
{
	switch(param) {
	case 0:
		cpu_cause_interrupt(0, 0x4e);
		break;
	case 1:
		if (timer2_active) {
			timer_set(TIME_IN_HZ(40), 1, polyplay_timer);
			cpu_cause_interrupt(0, 0x4c);
		}
		break;
	case 2:
		coin_counter_w(0, 0);
		break;
	}
}

/* initialization */
static void init_polyplay_sound(void)
{
	timer_init();
	timer_pulse(TIME_IN_HZ(75), 0, polyplay_timer);
	timer2_active = 0;
}

/* game driver */
GAME( 1985, polyplay, 0, polyplay, polyplay, polyplay_sound, ROT0, "VEB Polytechnik Karl-Marx-Stadt", "Poly-Play" )

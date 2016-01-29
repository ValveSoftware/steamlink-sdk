#include "../machine/tnzs.cpp"
#include "../vidhrdw/tnzs.cpp"

/***************************************************************************

The New Zealand Story driver, used for tnzs & tnzs2.

TODO: - Find out how the hardware credit-counter works (MPU)
	  - Verify dip switches
	  - Fix video offsets (See Dr Toppel in Flip-Screen - also
	       affects Chuka Taisen)
	  - Video scroll side flicker in Chuka Taisen, Insector X and Dr Toppel

	Arkanoid 2:
	  - What do writes at $f400 do ?
	  - Why does the game zero the $fd00 area ?
	Extrmatn:
	  - What do reads from $f600 do ? (discarded)
	Chuka Taisen:
	  - What do writes at  $f400 do ? (value 40h)
	  - What do reads from $f600 do in service mode ?
	Dr Toppel:
	  - What do writes at  $f400 do ? (value 40h)
	  - What do reads from $f600 do in service mode ?

****************************************************************************

extrmatn and arkanoi2 have a special test mode. The correct procedure to make
it succeed is as follows:
- enter service mode
- on the color test screen, press 2 (player 2 start)
- set dip switch 1 and dip switch 2 so that they read 00000001
- reset the emulation, and skip the previous step.
- press 3 (coin 1). Text at the bottom will change to "CHECKING NOW".
- use all the inputs, including tilt, until all inputs are OK
- press 3 (coin 1) - to confirm that coin lockout 1 works
- press 3 (coin 1) - to confirm that coin lockout 2 works
- set dip switch 1 to 00000000
- set dip switch 1 to 10101010
- set dip switch 1 to 11111111
- set dip switch 2 to 00000000
- set dip switch 2 to 10101010
- set dip switch 2 to 11111111
- speaker should now output a tone
- press 3 (coin 1) , to confirm that OPN works
- press 3 (coin 1) , to confirm that SSGCH1 works
- press 3 (coin 1) , to confirm that SSGCH2 works
- press 3 (coin 1) , to confirm that SSGCH3 works
- finished ("CHECK ALL OK!")

****************************************************************************

The New Zealand Story memory map (preliminary)

CPU #1
0000-7fff ROM
8000-bfff banked - banks 0-1 RAM; banks 2-7 ROM
c000-dfff object RAM, including:
  c000-c1ff sprites (code, low byte)
  c200-c3ff sprites (x-coord, low byte)
  c400-c5ff tiles (code, low byte)

  d000-d1ff sprites (code, high byte)
  d200-d3ff sprites (x-coord and colour, high byte)
  d400-d5ff tiles (code, high byte)
  d600-d7ff tiles (colour)
e000-efff RAM shared with CPU #2
f000-ffff VDC RAM, including:
  f000-f1ff sprites (y-coord)
  f200-f2ff scrolling info
  f300-f301 vdc controller
  f302-f303 scroll x-coords (high bits)
  f600      bankswitch
  f800-fbff palette

CPU #2
0000-7fff ROM
8000-9fff banked ROM
a000      bankswitch
b000-b001 YM2203 interface (with DIPs on YM2203 ports)
c000-c001 I8742 MCU
e000-efff RAM shared with CPU #1
f000-f003 inputs (used only by Arkanoid 2)

****************************************************************************/
/***************************************************************************

				Arkanoid 2 - Revenge of Doh!
					(C) 1987 Taito

						driver by

				Luca Elia (eliavit@unina.it)
				Mirko Buffoni

- The game doesn't write to f800-fbff (static palette)



			Interesting routines (main cpu)
			-------------------------------

1ed	prints the test screen (first string at 206)

47a	prints dipsw1&2 e 1p&2p paddleL values:
	e821		IN DIPSW1		e823-4	1P PaddleL (lo-hi)
	e822		IN DIPSW2		e825-6	2P PaddleL (lo-hi)

584	prints OK or NG on each entry:
	if (*addr)!=0 { if (*addr)!=2 OK else NG }
	e880	1P PADDLEL		e88a	IN SERVICE
	e881	1P PADDLER		e88b	IN TILT
	e882	1P BUTTON		e88c	OUT LOCKOUT1
	e883	1P START		e88d	OUT LOCKOUT2
	e884	2P PADDLEL		e88e	IN DIP-SW1
	e885	2P PADDLER		e88f	IN DIP-SW2
	e886	2P BUTTON		e890	SND OPN
	e887	2P START		e891	SND SSGCH1
	e888	IN COIN1		e892	SND SSGCH2
	e889	IN COIN2		e893	SND SSGCH3

672	prints a char
715	prints a string (0 terminated)

		Shared Memory (values written mainly by the sound cpu)
		------------------------------------------------------

e001=dip-sw A 	e399=coin counter value		e72c-d=1P paddle (lo-hi)
e002=dip-sw B 	e3a0-2=1P score/10 (BCD)	e72e-f=2P paddle (lo-hi)
e008=level=2*(shown_level-1)+x <- remember it's a binary tree (42 last)
e7f0=country code(from 9fde in sound rom)
e807=counter, reset by sound cpu, increased by main cpu each vblank
e80b=test progress=0(start) 1(first 8) 2(all ok) 3(error)
ec09-a~=ed05-6=xy pos of cursor in hi-scores
ec81-eca8=hi-scores(8bytes*5entries)

addr	bit	name		active	addr	bit	name		active
e72d	6	coin[1]		low		e729	1	2p select	low
		5	service		high			0	1p select	low
		4	coin[2]		low

addr	bit	name		active	addr	bit	name		active
e730	7	tilt		low		e7e7	4	1p fire		low
										0	2p fire		low

			Interesting routines (sound cpu)
			--------------------------------

4ae	check starts	B73,B7a,B81,B99	coin related
8c1	check coins		62e lockout check		664	dsw check

			Interesting locations (sound cpu)
			---------------------------------

d006=each bit is on if a corresponding location (e880-e887) has changed
d00b=(c001)>>4=tilt if 0E (security sequence must be reset?)
addr	bit	name		active
d00c	7	tilt
		6	?service?
		5	coin2		low
		4	coin1		low

d00d=each bit is on if the corresponding location (e880-e887) is 1 (OK)
d00e=each of the 4 MSBs is on if ..
d00f=FF if tilt, 00 otherwise
d011=if 00 checks counter, if FF doesn't
d23f=input port 1 value

***************************************************************************/
/***************************************************************************

Kageki
(c) 1988 Taito Corporation

Driver by Takahiro Nogi (nogi@kt.rim.or.jp) 1999/11/06

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"



/* prototypes for functions in ../machine/tnzs.c */
unsigned char *tnzs_objram, *tnzs_workram;
unsigned char *tnzs_vdcram, *tnzs_scrollram;
void init_extrmatn(void);
void init_arkanoi2(void);
void init_drtoppel(void);
void init_chukatai(void);
void init_tnzs(void);
void init_insectx(void);
void init_kageki(void);
READ_HANDLER( arkanoi2_sh_f000_r );
void tnzs_init_machine(void);
int tnzs_interrupt (void);
READ_HANDLER( tnzs_mcu_r );
READ_HANDLER( tnzs_workram_r );
READ_HANDLER( tnzs_workram_sub_r );
WRITE_HANDLER( tnzs_workram_w );
WRITE_HANDLER( tnzs_workram_sub_w );
WRITE_HANDLER( tnzs_mcu_w );
WRITE_HANDLER( tnzs_bankswitch_w );
WRITE_HANDLER( tnzs_bankswitch1_w );


/* prototypes for functions in ../vidhrdw/tnzs.c */
int tnzs_vh_start(void);
void tnzs_vh_stop(void);
void arkanoi2_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void arkanoi2_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void tnzs_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);



/* max samples */
#define	MAX_SAMPLES	0x2f

int kageki_init_samples(const struct MachineSound *msound)
{
	struct GameSamples *samples;
	unsigned char *scan, *src, *dest;
	int start, size;
	int i, n;

	size = sizeof(struct GameSamples) + MAX_SAMPLES * sizeof(struct GameSamples *);

	if ((Machine->samples = (struct GameSamples*)malloc(size)) == NULL) return 1;

	samples = Machine->samples;
	samples->total = MAX_SAMPLES;

	for (i = 0; i < samples->total; i++)
	{
		src = memory_region(REGION_SOUND1) + 0x0090;
		start = (src[(i * 2) + 1] * 256) + src[(i * 2)];
		scan = &src[start];
		size = 0;

		// check sample length
		while (1)
		{
			if (*scan++ == 0x00)
			{
				break;
			} else {
				size++;
			}
		}
		if ((samples->sample[i] = (struct GameSample*)malloc(sizeof(struct GameSample) + size * sizeof(unsigned char))) == NULL) return 1;

		if (start < 0x100) start = size = 0;

		samples->sample[i]->smpfreq = 7000;	/* 7 KHz??? */
		samples->sample[i]->resolution = 8;	/* 8 bit */
		samples->sample[i]->length = size;

		// signed 8-bit sample to unsigned 8-bit sample convert
		dest = (unsigned char *)samples->sample[i]->data;
		scan = &src[start];
		for (n = 0; n < size; n++)
		{
			*dest++ = ((*scan++) ^ 0x80);
		}
	//	logerror("samples num:%02X ofs:%04X lng:%04X\n", i, start, size);
	}

	return 0;
}


static int kageki_csport_sel = 0;
static READ_HANDLER( kageki_csport_r )
{
	int	dsw, dsw1, dsw2;

	dsw1 = readinputport(0); 		// DSW1
	dsw2 = readinputport(1); 		// DSW2

	switch (kageki_csport_sel)
	{
		case	0x00:			// DSW2 5,1 / DSW1 5,1
			dsw = (((dsw2 & 0x10) >> 1) | ((dsw2 & 0x01) << 2) | ((dsw1 & 0x10) >> 3) | ((dsw1 & 0x01) >> 0));
			break;
		case	0x01:			// DSW2 7,3 / DSW1 7,3
			dsw = (((dsw2 & 0x40) >> 3) | ((dsw2 & 0x04) >> 0) | ((dsw1 & 0x40) >> 5) | ((dsw1 & 0x04) >> 2));
			break;
		case	0x02:			// DSW2 6,2 / DSW1 6,2
			dsw = (((dsw2 & 0x20) >> 2) | ((dsw2 & 0x02) << 1) | ((dsw1 & 0x20) >> 4) | ((dsw1 & 0x02) >> 1));
			break;
		case	0x03:			// DSW2 8,4 / DSW1 8,4
			dsw = (((dsw2 & 0x80) >> 4) | ((dsw2 & 0x08) >> 1) | ((dsw1 & 0x80) >> 6) | ((dsw1 & 0x08) >> 3));
			break;
		default:
			dsw = 0x00;
		//	logerror("kageki_csport_sel error !! (0x%08X)\n", kageki_csport_sel);
	}

	return (dsw & 0xff);
}

static WRITE_HANDLER( kageki_csport_w )
{
	char mess[80];

	if (data > 0x3f)
	{
		// read dipsw port
		kageki_csport_sel = (data & 0x03);
	} else {
		if (data > MAX_SAMPLES)
		{
			// stop samples
			sample_stop(0);
			sprintf(mess, "VOICE:%02X STOP", data);
		} else {
			// play samples
			sample_start(0, data, 0);
			sprintf(mess, "VOICE:%02X PLAY", data);
		}
	//	usrintf_showmessage(mess);
	}
}


static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0xbfff, MRA_BANK1 }, /* ROM + RAM */
	{ 0xc000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xefff, tnzs_workram_r },	/* WORK RAM (shared by the 2 z80's */
	{ 0xf000, 0xf1ff, MRA_RAM },	/* VDC RAM */
	{ 0xf600, 0xf600, MRA_NOP },	/* ? */
	{ 0xf800, 0xfbff, MRA_RAM },	/* not in extrmatn and arkanoi2 (PROMs instead) */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0x8000, 0xbfff, MWA_BANK1 },	/* ROM + RAM */
	{ 0xc000, 0xdfff, MWA_RAM, &tnzs_objram },
	{ 0xe000, 0xefff, tnzs_workram_w, &tnzs_workram },
	{ 0xf000, 0xf1ff, MWA_RAM, &tnzs_vdcram },
	{ 0xf200, 0xf3ff, MWA_RAM, &tnzs_scrollram }, /* scrolling info */
	{ 0xf400, 0xf400, MWA_NOP },	/* ? */
	{ 0xf600, 0xf600, tnzs_bankswitch_w },
	{ 0xf800, 0xfbff, paletteram_xRRRRRGGGGGBBBBB_w, &paletteram },	/* not in extrmatn and arkanoi2 (PROMs instead) */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sub_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_BANK2 },
	{ 0xb000, 0xb000, YM2203_status_port_0_r },
	{ 0xb001, 0xb001, YM2203_read_port_0_r },
	{ 0xc000, 0xc001, tnzs_mcu_r },	/* plain input ports in insectx (memory handler */
									/* changed in insectx_init() ) */
	{ 0xd000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xefff, tnzs_workram_sub_r },
	{ 0xf000, 0xf003, arkanoi2_sh_f000_r },	/* paddles in arkanoid2/plumppop; the ports are */
						/* read but not used by the other games, and are not read at */
						/* all by insectx. */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sub_writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xa000, tnzs_bankswitch1_w },
	{ 0xb000, 0xb000, YM2203_control_port_0_w },
	{ 0xb001, 0xb001, YM2203_write_port_0_w },
	{ 0xc000, 0xc001, tnzs_mcu_w },	/* not present in insectx */
	{ 0xd000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xefff, tnzs_workram_sub_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress kageki_sub_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_BANK2 },
	{ 0xb000, 0xb000, YM2203_status_port_0_r },
	{ 0xb001, 0xb001, YM2203_read_port_0_r },
	{ 0xc000, 0xc000, input_port_2_r },
	{ 0xc001, 0xc001, input_port_3_r },
	{ 0xc002, 0xc002, input_port_4_r },
	{ 0xd000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xefff, tnzs_workram_sub_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress kageki_sub_writemem[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xa000, tnzs_bankswitch1_w },
	{ 0xb000, 0xb000, YM2203_control_port_0_w },
	{ 0xb001, 0xb001, YM2203_write_port_0_w },
	{ 0xd000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xefff, tnzs_workram_sub_w },
	{ -1 }  /* end of table */
};

/* the bootleg board is different, it has a third CPU (and of course no mcu) */

static WRITE_HANDLER( tnzsb_sound_command_w )
{
	soundlatch_w(offset,data);
	cpu_cause_interrupt(2,0xff);
}

static struct MemoryReadAddress tnzsb_readmem1[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0x8000, 0x9fff, MRA_BANK2 },
	{ 0xb002, 0xb002, input_port_0_r },
	{ 0xb003, 0xb003, input_port_1_r },
	{ 0xc000, 0xc000, input_port_2_r },
	{ 0xc001, 0xc001, input_port_3_r },
	{ 0xc002, 0xc002, input_port_4_r },
	{ 0xd000, 0xdfff, MRA_RAM },
	{ 0xe000, 0xefff, tnzs_workram_sub_r },
	{ 0xf000, 0xf003, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress tnzsb_writemem1[] =
{
	{ 0x0000, 0x9fff, MWA_ROM },
	{ 0xa000, 0xa000, tnzs_bankswitch1_w },
	{ 0xb004, 0xb004, tnzsb_sound_command_w },
	{ 0xd000, 0xdfff, MWA_RAM },
	{ 0xe000, 0xefff, tnzs_workram_sub_w },
	{ 0xf000, 0xf3ff, paletteram_xRRRRRGGGGGBBBBB_w, &paletteram },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress tnzsb_readmem2[] =
{
	{ 0x0000, 0x7fff, MRA_ROM },
	{ 0xc000, 0xdfff, MRA_RAM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress tnzsb_writemem2[] =
{
	{ 0x0000, 0x7fff, MWA_ROM },
	{ 0xc000, 0xdfff, MWA_RAM },
	{ -1 }  /* end of table */
};

static struct IOReadPort tnzsb_readport[] =
{
	{ 0x00, 0x00, YM2203_status_port_0_r  },
	{ 0x02, 0x02, soundlatch_r  },
	{ -1 }	/* end of table */
};

static struct IOWritePort tnzsb_writeport[] =
{
	{ 0x00, 0x00, YM2203_control_port_0_w  },
	{ 0x01, 0x01, YM2203_write_port_0_w  },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( extrmatn )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
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

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( arkanoi2 )
	PORT_START		/* DSW1 - IN2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )

	PORT_START		/* DSW2 - IN3 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50k 150k" )
	PORT_DIPSETTING(    0x0c, "100k 200k" )
	PORT_DIPSETTING(    0x04, "50k Only" )
	PORT_DIPSETTING(    0x08, "100k Only" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )

	PORT_START		/* IN1 - read at c000 (sound cpu) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* empty */

	PORT_START		/* empty */

	PORT_START		/* spinner 1 - read at f000/1 */
	PORT_ANALOG( 0x0fff, 0x0000, IPT_DIAL, 70, 15, 0, 0 )
	PORT_BIT   ( 0x1000, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT   ( 0x2000, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT   ( 0x4000, IP_ACTIVE_LOW,  IPT_COIN1 )
	PORT_BIT   ( 0x8000, IP_ACTIVE_LOW,  IPT_TILT )	/* arbitrarily assigned, handled by the mcu */

	PORT_START		/* spinner 2 - read at f002/3 */
	PORT_ANALOG( 0x0fff, 0x0000, IPT_DIAL | IPF_PLAYER2, 70, 15, 0, 0 )
	PORT_BIT   ( 0xf000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( ark2us )
	PORT_START		/* DSW1 - IN2 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START		/* DSW2 - IN3 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Normal" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Very Hard" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50k 150k" )
	PORT_DIPSETTING(    0x0c, "100k 200k" )
	PORT_DIPSETTING(    0x04, "50k Only" )
	PORT_DIPSETTING(    0x08, "100k Only" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x80, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )

	PORT_START		/* IN1 - read at c000 (sound cpu) */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* empty */

	PORT_START		/* empty */

	PORT_START		/* spinner 1 - read at f000/1 */
	PORT_ANALOG( 0x0fff, 0x0000, IPT_DIAL, 70, 15, 0, 0 )
	PORT_BIT   ( 0x1000, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT   ( 0x2000, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT   ( 0x4000, IP_ACTIVE_LOW,  IPT_COIN1 )
	PORT_BIT   ( 0x8000, IP_ACTIVE_LOW,  IPT_TILT )	/* arbitrarily assigned, handled by the mcu */

	PORT_START		/* spinner 2 - read at f002/3 */
	PORT_ANALOG( 0x0fff, 0x0000, IPT_DIAL | IPF_PLAYER2, 70, 15, 0, 0 )
	PORT_BIT   ( 0xf000, IP_ACTIVE_LOW,  IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( plumppop )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_CHEAT )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_CHEAT | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START		/* spinner 1 - read at f000/1 */
	PORT_ANALOG( 0xffff, 0x0000, IPT_DIAL, 70, 15, 0, 0 )

	PORT_START		/* spinner 2 - read at f002/3 */
	PORT_ANALOG( 0xffff, 0x0000, IPT_DIAL | IPF_PLAYER2, 70, 15, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( drtoppel )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0c, "30000" )
	PORT_DIPSETTING(    0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Unknown ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( chukatai )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
/* Bonus life awards are to be verified
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 150000" )
	PORT_DIPSETTING(    0x0c, "70000 200000" )
	PORT_DIPSETTING(    0x04, "100000 250000" )
	PORT_DIPSETTING(    0x08, "200000 300000" )
*/
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( tnzs )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 150000" )
	PORT_DIPSETTING(    0x0c, "70000 200000" )
	PORT_DIPSETTING(    0x04, "100000 250000" )
	PORT_DIPSETTING(    0x08, "200000 300000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( tnzsb )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "50000 150000" )
	PORT_DIPSETTING(    0x0c, "70000 200000" )
	PORT_DIPSETTING(    0x04, "100000 250000" )
	PORT_DIPSETTING(    0x08, "200000 300000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( tnzs2 )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "10000 100000" )
	PORT_DIPSETTING(    0x0c, "10000 150000" )
	PORT_DIPSETTING(    0x08, "10000 200000" )
	PORT_DIPSETTING(    0x04, "10000 300000" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x20, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPSETTING(    0x10, "5" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( insectx )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_6C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x30, "3" )
	PORT_DIPSETTING(    0x20, "4" )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

INPUT_PORTS_START( kageki )
	PORT_START		/* DSW A */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )

	PORT_START		/* DSW B */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x02, "Easy" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x01, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Yes ) )

	PORT_START		/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START		/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START		/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


static struct GfxLayout arkanoi2_charlayout =
{
	16,16,
	4096,
	4,
	{ 3*4096*32*8, 2*4096*32*8, 1*4096*32*8, 0*4096*32*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0,8*8+1,8*8+2,8*8+3,8*8+4,8*8+5,8*8+6,8*8+7},
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8
};

static struct GfxLayout tnzs_charlayout =
{
	16,16,
	8192,
	4,
	{ 3*8192*32*8, 2*8192*32*8, 1*8192*32*8, 0*8192*32*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8
};

static struct GfxLayout insectx_charlayout =
{
	16,16,
	8192,
	4,
	{ 8, 0, 8192*64*8+8, 8192*64*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*16+0, 8*16+1, 8*16+2, 8*16+3, 8*16+4, 8*16+5, 8*16+6, 8*16+7 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
		16*16, 17*16, 18*16, 19*16, 20*16, 21*16, 22*16, 23*16 },
	64*8
};

static struct GfxDecodeInfo arkanoi2_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &arkanoi2_charlayout, 0, 32 },
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo tnzs_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &tnzs_charlayout, 0, 32 },
	{ -1 }	/* end of array */
};

static struct GfxDecodeInfo insectx_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &insectx_charlayout, 0, 32 },
	{ -1 }	/* end of array */
};



static struct YM2203interface ym2203_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz ??? */
	{ YM2203_VOL(30,30) },
	{ input_port_0_r },		/* DSW1 connected to port A */
	{ input_port_1_r },		/* DSW2 connected to port B */
	{ 0 },
	{ 0 }
};


/* handler called by the 2203 emulator when the internal timers cause an IRQ */
static void irqhandler(int irq)
{
	cpu_set_nmi_line(2,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203b_interface =
{
	1,			/* 1 chip */
	3000000,	/* 3 MHz ??? */
	{ YM2203_VOL(100,100) },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ irqhandler }
};

static struct YM2203interface kageki_ym2203_interface =
{
	1,					/* 1 chip */
	3000000,				/* 12000000/4 ??? */
	{ YM2203_VOL(35, 15) },
	{ kageki_csport_r },
	{ 0 },
	{ 0 },
	{ kageki_csport_w },
};

static struct Samplesinterface samples_interface =
{
	1,					/* 1 channel */
	100					/* volume */
};

static struct CustomSound_interface custom_interface =
{
	kageki_init_samples,
	0,
	0
};


static struct MachineDriver machine_driver_arkanoi2 =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			8000000,	/* ?? Hz (only crystal is 12MHz) */
						/* 8MHz is wrong, but extrmatn doesn't work properly at 6MHz */
			readmem,writemem,0,0,
			tnzs_interrupt,1
		},
		{
			CPU_Z80,
			6000000,	/* ?? Hz */
			sub_readmem,sub_writemem,0,0,
			interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,		/* video frequency (Hz), duration */
	100,							/* cpu slices */
	tnzs_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	arkanoi2_gfxdecodeinfo,
	512, 512,
	arkanoi2_vh_convert_color_prom,		/* convert color p-roms */

	VIDEO_TYPE_RASTER,
	0,
	tnzs_vh_start,
	tnzs_vh_stop,
	arkanoi2_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

static struct MachineDriver machine_driver_drtoppel =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			12000000/2,		/* 6.0 MHz ??? - Main board Crystal is 12Mhz */
			readmem,writemem,0,0,
			tnzs_interrupt,1
		},
		{
			CPU_Z80,
			12000000/2,		/* 6.0 MHz ??? - Main board Crystal is 12Mhz */
			sub_readmem,sub_writemem,0,0,
			interrupt,1
		},
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,		/* video frequency (Hz), duration */
	100,							/* cpu slices */
	tnzs_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	tnzs_gfxdecodeinfo,
	512, 512,
	arkanoi2_vh_convert_color_prom,		/* convert color bproms */

	VIDEO_TYPE_RASTER,
	0,
	tnzs_vh_start,
	tnzs_vh_stop,
	arkanoi2_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

static struct MachineDriver machine_driver_tnzs =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			12000000/2,		/* 6.0 MHz ??? - Main board Crystal is 12Mhz */
			readmem,writemem,0,0,
			tnzs_interrupt,1
		},
		{
			CPU_Z80,
			12000000/2,		/* 6.0 MHz ??? - Main board Crystal is 12Mhz */
			sub_readmem,sub_writemem,0,0,
			interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	200,	/* 100 CPU slices per frame - an high value to ensure proper */
			/* synchronization of the CPUs */
	tnzs_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	tnzs_gfxdecodeinfo,
	512, 512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	tnzs_vh_start,
	tnzs_vh_stop,
	tnzs_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

static struct MachineDriver machine_driver_tnzsb =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,		/* 6 Mhz(?) */
			readmem,writemem,0,0,
			tnzs_interrupt,1
		},
		{
			CPU_Z80,
			6000000,		/* 6 Mhz(?) */
			tnzsb_readmem1,tnzsb_writemem1,0,0,
			interrupt,1
		},
		{
			CPU_Z80,
			4000000,		/* 4 Mhz??? */
			tnzsb_readmem2,tnzsb_writemem2,tnzsb_readport,tnzsb_writeport,
			ignore_interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	200,	/* 100 CPU slices per frame - an high value to ensure proper */
			/* synchronization of the CPUs */
	tnzs_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	tnzs_gfxdecodeinfo,
	512, 512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	tnzs_vh_start,
	tnzs_vh_stop,
	tnzs_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203b_interface
		}
	}
};

static struct MachineDriver machine_driver_insectx =
{
	/* basic machine hardware */
	{
		{
			CPU_Z80,
			6000000,	/* 6 Mhz(?) */
			readmem,writemem,0,0,
			tnzs_interrupt,1
		},
		{
			CPU_Z80,
			6000000,	/* 6 Mhz(?) */
			sub_readmem,sub_writemem,0,0,
			interrupt,1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	200,	/* 100 CPU slices per frame - an high value to ensure proper */
			/* synchronization of the CPUs */
	tnzs_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	insectx_gfxdecodeinfo,
	512, 512,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	tnzs_vh_start,
	tnzs_vh_stop,
	tnzs_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		}
	}
};

static struct MachineDriver machine_driver_kageki =
{
	{
		{
			CPU_Z80,
			6000000,		/* 12000000/2 ??? */
			readmem, writemem, 0, 0,
			tnzs_interrupt, 1
		},
		{
			CPU_Z80,
			4000000,		/* 12000000/3 ??? */
			kageki_sub_readmem, kageki_sub_writemem, 0, 0,
			interrupt, 1
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	200,	/* 200 CPU slices per frame - an high value to ensure proper */
		/* synchronization of the CPUs */
	tnzs_init_machine,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	tnzs_gfxdecodeinfo,
	512, 512,
	0,

	VIDEO_TYPE_RASTER|VIDEO_MODIFIES_PALETTE,
	0,
	tnzs_vh_start,
	tnzs_vh_stop,
	tnzs_vh_screenrefresh,

	/* sound hardware */
	0, 0, 0, 0,
	{
		{
			SOUND_YM2203,
			&kageki_ym2203_interface
		},
		{
			SOUND_SAMPLES,
			&samples_interface
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

ROM_START( extrmatn )
	ROM_REGION( 0x30000, REGION_CPU1 )				/* Region 0 - main cpu */
	ROM_LOAD( "b06-20.bin", 0x00000, 0x08000, 0x04e3fc1f )
	ROM_CONTINUE(           0x18000, 0x08000 )				/* banked at 8000-bfff */
	ROM_LOAD( "b06-21.bin", 0x20000, 0x10000, 0x1614d6a2 )	/* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )				/* Region 2 - sound cpu */
	ROM_LOAD( "b06-06.bin", 0x00000, 0x08000, 0x744f2c84 )
	ROM_CONTINUE(           0x10000, 0x08000 )	/* banked at 8000-9fff */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b06-01.bin", 0x00000, 0x20000, 0xd2afbf7e )
	ROM_LOAD( "b06-02.bin", 0x20000, 0x20000, 0xe0c2757a )
	ROM_LOAD( "b06-03.bin", 0x40000, 0x20000, 0xee80ab9d )
	ROM_LOAD( "b06-04.bin", 0x60000, 0x20000, 0x3697ace4 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "b06-09.bin", 0x00000, 0x200, 0xf388b361 )	/* hi bytes */
	ROM_LOAD( "b06-08.bin", 0x00200, 0x200, 0x10c9aac3 )	/* lo bytes */
ROM_END

ROM_START( arkanoi2 )
	ROM_REGION( 0x30000, REGION_CPU1 )				/* Region 0 - main cpu */
	ROM_LOAD( "a2-05.rom",  0x00000, 0x08000, 0x136edf9d )
	ROM_CONTINUE(           0x18000, 0x08000 )			/* banked at 8000-bfff */
	/* 20000-2ffff empty */

	ROM_REGION( 0x18000, REGION_CPU2 )				/* Region 2 - sound cpu */
	ROM_LOAD( "a2-13.rom",  0x00000, 0x08000, 0xe8035ef1 )
	ROM_CONTINUE(           0x10000, 0x08000 )			/* banked at 8000-9fff */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a2-m01.bin", 0x00000, 0x20000, 0x2ccc86b4 )
	ROM_LOAD( "a2-m02.bin", 0x20000, 0x20000, 0x056a985f )
	ROM_LOAD( "a2-m03.bin", 0x40000, 0x20000, 0x274a795f )
	ROM_LOAD( "a2-m04.bin", 0x60000, 0x20000, 0x9754f703 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "b08-08.bin", 0x00000, 0x200, 0xa4f7ebd9 )	/* hi bytes */
	ROM_LOAD( "b08-07.bin", 0x00200, 0x200, 0xea34d9f7 )	/* lo bytes */
ROM_END

ROM_START( ark2us )
	ROM_REGION( 0x30000, REGION_CPU1 )				/* Region 0 - main cpu */
	ROM_LOAD( "b08-11.bin", 0x00000, 0x08000, 0x99555231 )
	ROM_CONTINUE(           0x18000, 0x08000 )			/* banked at 8000-bfff */
	/* 20000-2ffff empty */

	ROM_REGION( 0x18000, REGION_CPU2 )				/* Region 2 - sound cpu */
	ROM_LOAD( "b08-12.bin", 0x00000, 0x08000, 0xdc84e27d )
	ROM_CONTINUE(           0x10000, 0x08000 )			/* banked at 8000-9fff */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a2-m01.bin", 0x00000, 0x20000, 0x2ccc86b4 )
	ROM_LOAD( "a2-m02.bin", 0x20000, 0x20000, 0x056a985f )
	ROM_LOAD( "a2-m03.bin", 0x40000, 0x20000, 0x274a795f )
	ROM_LOAD( "a2-m04.bin", 0x60000, 0x20000, 0x9754f703 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "b08-08.bin", 0x00000, 0x200, 0xa4f7ebd9 )	/* hi bytes */
	ROM_LOAD( "b08-07.bin", 0x00200, 0x200, 0xea34d9f7 )	/* lo bytes */
ROM_END

ROM_START( ark2jp )
	ROM_REGION( 0x30000, REGION_CPU1 )				/* Region 0 - main cpu */
	ROM_LOAD( "a2-05.rom",  0x00000, 0x08000, 0x136edf9d )
	ROM_CONTINUE(           0x18000, 0x08000 )			/* banked at 8000-bfff */
	/* 20000-2ffff empty */

	ROM_REGION( 0x18000, REGION_CPU2 )				/* Region 2 - sound cpu */
	ROM_LOAD( "b08-06",     0x00000, 0x08000, 0xadfcd40c )
	ROM_CONTINUE(           0x10000, 0x08000 )			/* banked at 8000-9fff */

	ROM_REGION( 0x80000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a2-m01.bin", 0x00000, 0x20000, 0x2ccc86b4 )
	ROM_LOAD( "a2-m02.bin", 0x20000, 0x20000, 0x056a985f )
	ROM_LOAD( "a2-m03.bin", 0x40000, 0x20000, 0x274a795f )
	ROM_LOAD( "a2-m04.bin", 0x60000, 0x20000, 0x9754f703 )

	ROM_REGION( 0x0400, REGION_PROMS )
	ROM_LOAD( "b08-08.bin", 0x00000, 0x200, 0xa4f7ebd9 )	/* hi bytes */
	ROM_LOAD( "b08-07.bin", 0x00200, 0x200, 0xea34d9f7 )	/* lo bytes */
ROM_END

ROM_START( plumppop )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 64k + bankswitch areas for the first CPU */
	ROM_LOAD( "a98-09.bin", 0x00000, 0x08000, 0x107f9e06 )
	ROM_CONTINUE(           0x18000, 0x08000 )				/* banked at 8000-bfff */
	ROM_LOAD( "a98-10.bin", 0x20000, 0x10000, 0xdf6e6af2 )	/* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "a98-11.bin", 0x00000, 0x08000, 0xbc56775c )
	ROM_CONTINUE(           0x10000, 0x08000 )		/* banked at 8000-9fff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "a98-01.bin", 0x00000, 0x10000, 0xf3033dca )
	ROM_RELOAD(             0x10000, 0x10000 )
	ROM_LOAD( "a98-02.bin", 0x20000, 0x10000, 0xf2d17b0c )
	ROM_RELOAD(             0x30000, 0x10000 )
	ROM_LOAD( "a98-03.bin", 0x40000, 0x10000, 0x1a519b0a )
	ROM_RELOAD(             0x40000, 0x10000 )
	ROM_LOAD( "a98-04.bin", 0x60000, 0x10000, 0xb64501a1 )
	ROM_RELOAD(             0x70000, 0x10000 )
	ROM_LOAD( "a98-05.bin", 0x80000, 0x10000, 0x45c36963 )
	ROM_RELOAD(             0x90000, 0x10000 )
	ROM_LOAD( "a98-06.bin", 0xa0000, 0x10000, 0xe075341b )
	ROM_RELOAD(             0xb0000, 0x10000 )
	ROM_LOAD( "a98-07.bin", 0xc0000, 0x10000, 0x8e16cd81 )
	ROM_RELOAD(             0xd0000, 0x10000 )
	ROM_LOAD( "a98-08.bin", 0xe0000, 0x10000, 0xbfa7609a )
	ROM_RELOAD(             0xf0000, 0x10000 )

	ROM_REGION( 0x0400, REGION_PROMS )		/* color proms */
	ROM_LOAD( "a98-13.bpr", 0x0000, 0x200, 0x7cde2da5 )	/* hi bytes */
	ROM_LOAD( "a98-12.bpr", 0x0200, 0x200, 0x90dc9da7 )	/* lo bytes */
ROM_END

ROM_START( drtoppel )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 64k + bankswitch areas for the first CPU */
	ROM_LOAD( "b19-09.bin", 0x00000, 0x08000, 0x3e654f82 )
	ROM_CONTINUE(           0x18000, 0x08000 )				/* banked at 8000-bfff */
	ROM_LOAD( "b19-10.bin", 0x20000, 0x10000, 0x7e72fd25 )	/* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "b19-11.bin", 0x00000, 0x08000, 0x524dc249 )
	ROM_CONTINUE(           0x10000, 0x08000 )		/* banked at 8000-9fff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b19-01.bin", 0x00000, 0x20000, 0xa7e8a0c1 )
	ROM_LOAD( "b19-02.bin", 0x20000, 0x20000, 0x790ae654 )
	ROM_LOAD( "b19-03.bin", 0x40000, 0x20000, 0x495c4c5a )
	ROM_LOAD( "b19-04.bin", 0x60000, 0x20000, 0x647007a0 )
	ROM_LOAD( "b19-05.bin", 0x80000, 0x20000, 0x49f2b1a5 )
	ROM_LOAD( "b19-06.bin", 0xa0000, 0x20000, 0x2d39f1d0 )
	ROM_LOAD( "b19-07.bin", 0xc0000, 0x20000, 0x8bb06f41 )
	ROM_LOAD( "b19-08.bin", 0xe0000, 0x20000, 0x3584b491 )

	ROM_REGION( 0x0400, REGION_PROMS )		/* color proms */
	ROM_LOAD( "b19-13.bin", 0x0000, 0x200, 0x6a547980 )	/* hi bytes */
	ROM_LOAD( "b19-12.bin", 0x0200, 0x200, 0x5754e9d8 )	/* lo bytes */
ROM_END

ROM_START( chukatai )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 64k + bankswitch areas for the first CPU */
	ROM_LOAD( "b44.10", 0x00000, 0x08000, 0x8c69e008 )
	ROM_CONTINUE(       0x18000, 0x08000 )				/* banked at 8000-bfff */
	ROM_LOAD( "b44.11", 0x20000, 0x10000, 0x32484094 )  /* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "b44.12", 0x00000, 0x08000, 0x0600ace6 )
	ROM_CONTINUE(       0x10000, 0x08000 )	/* banked at 8000-9fff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b44-01.a13", 0x00000, 0x20000, 0xaae7b3d5 )
	ROM_LOAD( "b44-02.a12", 0x20000, 0x20000, 0x7f0b9568 )
	ROM_LOAD( "b44-03.a10", 0x40000, 0x20000, 0x5a54a3b9 )
	ROM_LOAD( "b44-04.a08", 0x60000, 0x20000, 0x3c5f544b )
	ROM_LOAD( "b44-05.a07", 0x80000, 0x20000, 0xd1b7e314 )
	ROM_LOAD( "b44-06.a05", 0xa0000, 0x20000, 0x269978a8 )
	ROM_LOAD( "b44-07.a04", 0xc0000, 0x20000, 0x3e0e737e )
	ROM_LOAD( "b44-08.a02", 0xe0000, 0x20000, 0x6cb1e8fc )
ROM_END

ROM_START( tnzs )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 64k + bankswitch areas for the first CPU */
	ROM_LOAD( "nzsb5310.bin", 0x00000, 0x08000, 0xa73745c6 )
	ROM_CONTINUE(             0x18000, 0x18000 )		/* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "nzsb5311.bin", 0x00000, 0x08000, 0x9784d443 )
	ROM_CONTINUE(             0x10000, 0x08000 )		/* banked at 8000-9fff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	/* ROMs taken from another set (the ones from this set were read incorrectly) */
	ROM_LOAD( "nzsb5316.bin", 0x00000, 0x20000, 0xc3519c2a )
	ROM_LOAD( "nzsb5317.bin", 0x20000, 0x20000, 0x2bf199e8 )
	ROM_LOAD( "nzsb5318.bin", 0x40000, 0x20000, 0x92f35ed9 )
	ROM_LOAD( "nzsb5319.bin", 0x60000, 0x20000, 0xedbb9581 )
	ROM_LOAD( "nzsb5322.bin", 0x80000, 0x20000, 0x59d2aef6 )
	ROM_LOAD( "nzsb5323.bin", 0xa0000, 0x20000, 0x74acfb9b )
	ROM_LOAD( "nzsb5320.bin", 0xc0000, 0x20000, 0x095d0dc0 )
	ROM_LOAD( "nzsb5321.bin", 0xe0000, 0x20000, 0x9800c54d )
ROM_END

ROM_START( tnzsb )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 64k + bankswitch areas for the first CPU */
	ROM_LOAD( "nzsb5324.bin", 0x00000, 0x08000, 0xd66824c6 )
	ROM_CONTINUE(             0x18000, 0x18000 )		/* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "nzsb5325.bin", 0x00000, 0x08000, 0xd6ac4e71 )
	ROM_CONTINUE(             0x10000, 0x08000 )		/* banked at 8000-9fff */

	ROM_REGION( 0x10000, REGION_CPU3 )	/* 64k for the third CPU */
	ROM_LOAD( "nzsb5326.bin", 0x00000, 0x10000, 0xcfd5649c )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	/* ROMs taken from another set (the ones from this set were read incorrectly) */
	ROM_LOAD( "nzsb5316.bin", 0x00000, 0x20000, 0xc3519c2a )
	ROM_LOAD( "nzsb5317.bin", 0x20000, 0x20000, 0x2bf199e8 )
	ROM_LOAD( "nzsb5318.bin", 0x40000, 0x20000, 0x92f35ed9 )
	ROM_LOAD( "nzsb5319.bin", 0x60000, 0x20000, 0xedbb9581 )
	ROM_LOAD( "nzsb5322.bin", 0x80000, 0x20000, 0x59d2aef6 )
	ROM_LOAD( "nzsb5323.bin", 0xa0000, 0x20000, 0x74acfb9b )
	ROM_LOAD( "nzsb5320.bin", 0xc0000, 0x20000, 0x095d0dc0 )
	ROM_LOAD( "nzsb5321.bin", 0xe0000, 0x20000, 0x9800c54d )
ROM_END

ROM_START( tnzs2 )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 64k + bankswitch areas for the first CPU */
	ROM_LOAD( "ns_c-11.rom",  0x00000, 0x08000, 0x3c1dae7b )
	ROM_CONTINUE(             0x18000, 0x18000 )		/* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "ns_e-3.rom",   0x00000, 0x08000, 0xc7662e96 )
	ROM_CONTINUE(             0x10000, 0x08000 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "ns_a13.rom",   0x00000, 0x20000, 0x7e0bd5bb )
	ROM_LOAD( "ns_a12.rom",   0x20000, 0x20000, 0x95880726 )
	ROM_LOAD( "ns_a10.rom",   0x40000, 0x20000, 0x2bc4c053 )
	ROM_LOAD( "ns_a08.rom",   0x60000, 0x20000, 0x8ff8d88c )
	ROM_LOAD( "ns_a07.rom",   0x80000, 0x20000, 0x291bcaca )
	ROM_LOAD( "ns_a05.rom",   0xa0000, 0x20000, 0x6e762e20 )
	ROM_LOAD( "ns_a04.rom",   0xc0000, 0x20000, 0xe1fd1b9d )
	ROM_LOAD( "ns_a02.rom",   0xe0000, 0x20000, 0x2ab06bda )
ROM_END

ROM_START( insectx )
	ROM_REGION( 0x30000, REGION_CPU1 )	/* 64k + bankswitch areas for the first CPU */
	ROM_LOAD( "insector.u32", 0x00000, 0x08000, 0x18eef387 )
	ROM_CONTINUE(             0x18000, 0x18000 )		/* banked at 8000-bfff */

	ROM_REGION( 0x18000, REGION_CPU2 )	/* 64k for the second CPU */
	ROM_LOAD( "insector.u38", 0x00000, 0x08000, 0x324b28c9 )
	ROM_CONTINUE(             0x10000, 0x08000 )		/* banked at 8000-9fff */

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "insector.r15", 0x00000, 0x80000, 0xd00294b1 )
	ROM_LOAD( "insector.r16", 0x80000, 0x80000, 0xdb5a7434 )
ROM_END

ROM_START( kageki )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "b35-16.11c",  0x00000, 0x08000, 0xa4e6fd58 )	/* US ver */
	ROM_CONTINUE(            0x18000, 0x08000 )
	ROM_LOAD( "b35-10.9c",   0x20000, 0x10000, 0xb150457d )

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "b35-17.43e",  0x00000, 0x08000, 0xfdd9c246 )	/* US ver */
	ROM_CONTINUE(            0x10000, 0x08000 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b35-01.13a",  0x00000, 0x20000, 0x01d83a69 )
	ROM_LOAD( "b35-02.12a",  0x20000, 0x20000, 0xd8af47ac )
	ROM_LOAD( "b35-03.10a",  0x40000, 0x20000, 0x3cb68797 )
	ROM_LOAD( "b35-04.8a",   0x60000, 0x20000, 0x71c03f91 )
	ROM_LOAD( "b35-05.7a",   0x80000, 0x20000, 0xa4e20c08 )
	ROM_LOAD( "b35-06.5a",   0xa0000, 0x20000, 0x3f8ab658 )
	ROM_LOAD( "b35-07.4a",   0xc0000, 0x20000, 0x1b4af049 )
	ROM_LOAD( "b35-08.2a",   0xe0000, 0x20000, 0xdeb2268c )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "b35-15.98g",  0x00000, 0x10000, 0xe6212a0f )	/* US ver */
ROM_END

ROM_START( kagekij )
	ROM_REGION( 0x30000, REGION_CPU1 )
	ROM_LOAD( "b35-09j.11c", 0x00000, 0x08000, 0x829637d5 )	/* JP ver */
	ROM_CONTINUE(            0x18000, 0x08000 )
	ROM_LOAD( "b35-10.9c",   0x20000, 0x10000, 0xb150457d )

	ROM_REGION( 0x18000, REGION_CPU2 )
	ROM_LOAD( "b35-11j.43e", 0x00000, 0x08000, 0x64d093fc )	/* JP ver */
	ROM_CONTINUE(            0x10000, 0x08000 )

	ROM_REGION( 0x100000, REGION_GFX1 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "b35-01.13a",  0x00000, 0x20000, 0x01d83a69 )
	ROM_LOAD( "b35-02.12a",  0x20000, 0x20000, 0xd8af47ac )
	ROM_LOAD( "b35-03.10a",  0x40000, 0x20000, 0x3cb68797 )
	ROM_LOAD( "b35-04.8a",   0x60000, 0x20000, 0x71c03f91 )
	ROM_LOAD( "b35-05.7a",   0x80000, 0x20000, 0xa4e20c08 )
	ROM_LOAD( "b35-06.5a",   0xa0000, 0x20000, 0x3f8ab658 )
	ROM_LOAD( "b35-07.4a",   0xc0000, 0x20000, 0x1b4af049 )
	ROM_LOAD( "b35-08.2a",   0xe0000, 0x20000, 0xdeb2268c )

	ROM_REGION( 0x10000, REGION_SOUND1 )	/* samples */
	ROM_LOAD( "b35-12j.98g", 0x00000, 0x10000, 0x184409f1 )	/* JP ver */
ROM_END



GAME( 1987, extrmatn, 0,        arkanoi2, extrmatn, extrmatn, ROT270, "[Taito] World Games", "Extermination (US)" )
GAME( 1987, arkanoi2, 0,        arkanoi2, arkanoi2, arkanoi2, ROT270, "Taito Corporation Japan", "Arkanoid - Revenge of DOH (World)" )
GAME( 1987, ark2us,   arkanoi2, arkanoi2, ark2us,   arkanoi2, ROT270, "Taito America Corporation (Romstar license)", "Arkanoid - Revenge of DOH (US)" )
GAME( 1987, ark2jp,   arkanoi2, arkanoi2, ark2us,   arkanoi2, ROT270, "Taito Corporation", "Arkanoid - Revenge of DOH (Japan)" )
GAME( 1987, plumppop, 0,        drtoppel, plumppop, drtoppel, ROT0,   "Taito Corporation", "Plump Pop (Japan)" )
GAME( 1987, drtoppel, 0,        drtoppel, drtoppel, drtoppel, ROT90,  "Taito Corporation", "Dr. Toppel's Tankentai (Japan)" )
GAME( 1988, chukatai, 0,        tnzs,     chukatai, chukatai, ROT0,   "Taito Corporation", "Chuka Taisen (Japan)" )
GAME( 1988, tnzs,     0,        tnzs,     tnzs,     tnzs,     ROT0,   "Taito Corporation", "The NewZealand Story (Japan)" )
GAME( 1988, tnzsb,    tnzs,     tnzsb,    tnzsb,    tnzs,     ROT0,   "bootleg", "The NewZealand Story (World, bootleg)" )
GAME( 1988, tnzs2,    tnzs,     tnzs,     tnzs2,    tnzs,     ROT0,   "Taito Corporation Japan", "The NewZealand Story 2 (World)" )
GAME( 1989, insectx,  0,        insectx,  insectx,  insectx,  ROT0,   "Taito Corporation Japan", "Insector X (World)" )
GAME( 1988, kageki,   0,        kageki,   kageki,   kageki,   ROT90,  "Taito America Corporation (Romstar license)", "Kageki (US)" )
GAME( 1988, kagekij,  kageki,   kageki,   kageki,   kageki,   ROT90,  "Taito Corporation", "Kageki (Japan)" )

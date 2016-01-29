/* Fire Truck (C) Atari 1978 */

#include "driver.h"
#include "vidhrdw/generic.h"

static UINT8 firetruck_steer1, firetruck_steer2;
static UINT8 firetruck_invert_display;
static UINT8 firetruck_bit7_flags;
static UINT8 firetruck_bit0_flags;

static struct GfxLayout char_layout =
{
	16,16,	/* character size */
	0x20,	/* number of chars */
	1,		/* bits per pixel */
	{ 4 },
	{
		24,25,26,27,
		0,1,2,3,
		8,9,10,11,
		16,17,18,19
	},
	{
		0*64,0*64+32,1*64,1*64+32,
		2*64,2*64+32,3*64,3*64+32,
		4*64,4*64+32,5*64,5*64+32,
		6*64,6*64+32,7*64,7*64+32
	},
	8*64
};

static struct GfxLayout tile_layout =
{
	16,16,		/* character size */
	0x40,		/* number of characters */
	1,			/* bits per pixel */
	{ 0 },
	{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
	  8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 },
	8*32
};

static struct GfxLayout tail_layout =
{
	64,64,	/* character size */
	0x8,	/* number of characters */
	1,		/* bits per pixel */
	{ 0 },
	{
		0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
		16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
		32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
		48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
	},
	{
		0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
		8*64, 9*64, 10*64, 11*64, 12*64, 13*64, 14*64, 15*64,
		0x400+0*64, 0x400+1*64, 0x400+2*64, 0x400+3*64,
		0x400+4*64, 0x400+5*64, 0x400+6*64, 0x400+7*64,
		0x400+8*64, 0x400+9*64, 0x400+10*64, 0x400+11*64,
		0x400+12*64, 0x400+13*64, 0x400+14*64, 0x400+15*64,
		0x800+0*64, 0x800+1*64, 0x800+2*64, 0x800+3*64,
		0x800+4*64, 0x800+5*64, 0x800+6*64, 0x800+7*64,
		0x800+8*64, 0x800+9*64, 0x800+10*64, 0x800+11*64,
		0x800+12*64, 0x800+13*64, 0x800+14*64, 0x800+15*64,
		0xc00+0*64, 0xc00+1*64, 0xc00+2*64, 0xc00+3*64,
		0xc00+4*64, 0xc00+5*64, 0xc00+6*64, 0xc00+7*64,
		0xc00+8*64, 0xc00+9*64, 0xc00+10*64, 0xc00+11*64,
		0xc00+12*64, 0xc00+13*64, 0xc00+14*64, 0xc00+15*64
	},
	16*64*4
};

static struct GfxLayout cab_layout1 =
{
	32,32,	/* character size */
	0x4,	/* number of characters */
	1,		/* bits per pixel */
	{ 4 },
	{
		0,1,2,3,8,9,10,11,
		16,17,18,19,24,25,26,27,
		32,33,34,35,40,41,42,43,
		48,49,50,51,56,57,58,59
	},
	{
		0*64, 1*64, 2*64, 3*64,
		4*64, 5*64, 6*64, 7*64,
		8*64, 9*64, 10*64, 11*64,
		12*64, 13*64, 14*64, 15*64,
		0x400+0*64, 0x400+1*64, 0x400+2*64, 0x400+3*64,
		0x400+4*64, 0x400+5*64, 0x400+6*64, 0x400+7*64,
		0x400+8*64, 0x400+9*64, 0x400+10*64, 0x400+11*64,
		0x400+12*64, 0x400+13*64, 0x400+14*64, 0x400+15*64
	},
	16*64*2
};

static struct GfxLayout cab_layout2 =
{
	32,32,	/* character size */
	0x4,	/* number of characters */
	1,		/* bits per pixel */
	{ 4 },
	{
		0*64, 1*64, 2*64, 3*64,
		4*64, 5*64, 6*64, 7*64,
		8*64, 9*64, 10*64, 11*64,
		12*64, 13*64, 14*64, 15*64,
		0x400+0*64, 0x400+1*64, 0x400+2*64, 0x400+3*64,
		0x400+4*64, 0x400+5*64, 0x400+6*64, 0x400+7*64,
		0x400+8*64, 0x400+9*64, 0x400+10*64, 0x400+11*64,
		0x400+12*64, 0x400+13*64, 0x400+14*64, 0x400+15*64
	},
	{
		0,1,2,3,8,9,10,11,
		16,17,18,19,24,25,26,27,
		32,33,34,35,40,41,42,43,
		48,49,50,51,56,57,58,59
	},
	16*64*2
};

static struct GfxDecodeInfo firetruck_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &char_layout,	0, 0x8 },
	{ REGION_GFX2, 0, &tile_layout,	0, 0x8 },
	{ REGION_GFX3, 0, &tail_layout,	0, 0x8 },
	{ REGION_GFX4, 0, &cab_layout1,	0, 0x8 },
	{ REGION_GFX4, 0, &cab_layout2,	0, 0x8 },
	{ -1 }
};

static unsigned char palette[] =
{
	0x00,0x00,0x00,
	0x55,0x55,0x55,
	0xaa,0xaa,0xaa,
	0xff,0xff,0xff
};

static unsigned short colortable[] =
{
	0x00, 0x00,
	0x01, 0x00,
	0x02, 0x00,
	0x03, 0x00,
	0x03, 0x03,
	0x02, 0x03,
	0x01, 0x03,
	0x00, 0x03
};


/************************************************************************/
/* firetrk Sound System Analog emulation by K.Wilkins Feb 2001          */
/* Questions/Suggestions to mame@dysfunction.demon.co.uk                */
/* Modified and added superbug/montecar sounds.  Jan 2003 D.R.          */
/************************************************************************/

const struct discrete_lfsr_desc firetrk_lfsr={
	16,			/* Bit Length */
	0,			/* Reset Value */
	0,			/* Use Bit 0 as XOR input 0 */
	14,			/* Use Bit 14 as XOR input 1 */
	DISC_LFSR_XNOR,		/* Feedback stage1 is XNOR */
	DISC_LFSR_OR,		/* Feedback stage2 is just stage 1 output OR with external feed */
	DISC_LFSR_REPLACE,	/* Feedback stage3 replaces the shifted register contents */
	0x000001,		/* Everything is shifted into the first bit only */
	0,			/* Output is not inverted */
	15			/* Output bit */
};

/* Nodes - Inputs */
#define FIRETRUCK_MOTORSND_DATA		NODE_01
#define FIRETRUCK_HORNSND_EN		NODE_02
#define FIRETRUCK_SIRENSND_DATA		NODE_03
#define FIRETRUCK_CRASHSND_DATA		NODE_04
#define FIRETRUCK_SKIDSND_EN		NODE_05
#define FIRETRUCK_BELLSND_EN		NODE_06
#define FIRETRUCK_ATTRACT_EN		NODE_07
#define FIRETRUCK_XTNDPLY_EN		NODE_08
/* Nodes - Sounds */
#define FIRETRUCK_MOTORSND		NODE_11
#define FIRETRUCK_HORNSND		NODE_12
#define FIRETRUCK_SIRENSND		NODE_13
#define FIRETRUCK_CRASHSND		NODE_14
#define FIRETRUCK_SKIDSND		NODE_15
#define FIRETRUCK_BELLSND		NODE_16
#define FIRETRUCK_NOISE			NODE_17
#define FIRETRUCK_XTNDPLY		NODE_18

static DISCRETE_SOUND_START(firetrk_sound_interface)
	/************************************************/
	/* Firetruck sound system: 7 Sound Sources      */
	/*                     Relative Volume          */
	/*    1) Horn (Button)     21.36%               */
	/*    2) Motor             12.70%               */
	/*    3) Siren              3.13%               */
	/*    4) Crash             45.22%               */
	/*    5) Screech/Skid      14.24%               */
	/*    6) Bell              21.36%               */
	/*    7) Xtnd             100.00%               */
	/* Relative volumes calculated from gain        */
	/* formula for inverting amplifier.             */
	/*                                              */
	/*  FireTruck Discrete sound mapping via:       */
	/*     discrete_sound_w($register,value)        */
	/*  $00 - Motorsound frequency                  */
	/*  $01 - Hornsound enable                      */
	/*  $02 - Siren frequency                       */
	/*  $03 - Crash volume                          */
	/*  $04 - Skid enable                           */
	/*  $05 - Bell enable                           */
	/*  $06 - Attract mode                          */
	/*  $07 - Extend sound                          */
	/*                                              */
	/************************************************/

	/************************************************/
	/* Input register mapping for firetruck         */
	/************************************************/
	/*                   NODE                 ADDR  MASK   GAIN    OFFSET  INIT */
	DISCRETE_INPUTX(FIRETRUCK_MOTORSND_DATA  ,0x00,0x000f, -1.0   , 15.0,   0.0)
	DISCRETE_INPUT (FIRETRUCK_HORNSND_EN     ,0x01,0x000f,                  0.0)
	DISCRETE_INPUTX(FIRETRUCK_SIRENSND_DATA  ,0x02,0x000f, -1.0   , 15.0,   0.0)
	DISCRETE_INPUTX(FIRETRUCK_CRASHSND_DATA  ,0x03,0x000f, -1.0   , 15.0,   0.0)
	DISCRETE_INPUT (FIRETRUCK_SKIDSND_EN     ,0x04,0x000f,                  0.0)
	DISCRETE_INPUT (FIRETRUCK_BELLSND_EN     ,0x05,0x000f,                  0.0)
	DISCRETE_INPUT (FIRETRUCK_ATTRACT_EN     ,0x06,0x000f,                  0.0)
	DISCRETE_INPUT (FIRETRUCK_XTNDPLY_EN     ,0x07,0x000f,                  0.0)

	/************************************************/
	/* Motor sound circuit is based on a 556 VCO    */
	/* with the input frequency set by the MotorSND */
	/* latch (4 bit). This freqency is then used to */
	/* driver a modulo 12 counter, with div6 & div12*/
	/* summed as the output of the circuit.         */
	/* VCO Output is Sq wave = 95-458Hz             */
	/*  F1 freq - (Div6)                            */
	/*  F2 freq = (Div12)                           */
	/* To generate the frequency we take the freq.  */
	/* diff. and /15 to get all the steps between   */
	/* 0 - 15.  Then add the low frequency and send */
	/* that value to a squarewave generator.        */
	/* Also as the frequency changes, it ramps due  */
	/* to a 10uf capacitor on the R-ladder.         */
	/* Note the VCO freq. is controlled by a 250k   */
	/* pot.  The freq. used here is for the pot set */
	/* to 125k.  The low freq is allways the same.  */
	/* This adjusts the high end.                   */
	/* 0k = 289Hz.   250k = 4500Hz                  */
	/************************************************/
	DISCRETE_RCFILTER(NODE_80, 1, FIRETRUCK_MOTORSND_DATA, 123000, 10e-6)
	DISCRETE_ADJUSTMENT(NODE_81, 1, (289.0-95.0)/12/15, (4500.0-95.0)/12/15, (458.0-95.0)/12/15, DISC_LOGADJ, "Motor RPM")
	DISCRETE_MULTIPLY(NODE_82, 1, NODE_80, NODE_81)

	DISCRETE_GAIN(NODE_20, NODE_82, 2)	/* F1 = /12*2 = /6 */
	DISCRETE_ADDER2(NODE_21, 1, NODE_20, 95.0/6)
	DISCRETE_SQUAREWAVE(NODE_22, 1, NODE_21, (127.0/2), 50.0, 0, 0)
	DISCRETE_RCFILTER(NODE_28, 1, NODE_22, 10000, 1e-7)

	DISCRETE_ADDER2(NODE_24, 1, NODE_82, 95.0/12)	/* F2 = /12 */
	DISCRETE_SQUAREWAVE(NODE_25, 1, NODE_24, (127.0/2), 50.0, 0, 0)
	DISCRETE_RCFILTER(NODE_29, 1, NODE_25, 10000, 1e-7)

	DISCRETE_ADDER2(FIRETRUCK_MOTORSND, FIRETRUCK_ATTRACT_EN, NODE_28, NODE_29)

	/************************************************/
	/* Horn, this is taken from the 64V signal.     */
	/*  64V = HSYNC/128                             */
	/*      = 15750/128                             */
	/************************************************/
	DISCRETE_SQUAREWAVE(NODE_30, FIRETRUCK_ATTRACT_EN, 15750/128, 213.6, 50.0, 0, 0)
	DISCRETE_ONOFF(FIRETRUCK_HORNSND, FIRETRUCK_HORNSND_EN, NODE_30)

	/************************************************/
	/* Siren is built around a 556 based VCO, the   */
	/* 4 bit input value is smoothed between trans- */
	/* itions by a 10u capacitor with around a 0.5s */
	/* time constant, modelled with an RC filter.   */
	/* 0000 = 666Hz with 35% duty cycle             */
	/* 1111 = 526Hz with 63% duty cycle             */
	/* Input register does the inversion of sense   */
	/* to map this to:                              */
	/* 0000 = 526Hz with 37% duty cycle             */
	/* 1111 = 666Hz with 65% duty cycle             */
	/* Duty cycle is inverted 100-x to make things  */
	/* a little simpler and it doesnt affect sound. */
	/************************************************/
	DISCRETE_RCFILTER(NODE_40, 1, FIRETRUCK_SIRENSND_DATA, 250000, 1e-6)	/* Input smoothing */

	DISCRETE_GAIN(NODE_41, NODE_40, (666.0-526.0) /15)			/* Frequency modelling */
	DISCRETE_ADDER2(NODE_42, 1, NODE_41, 526.0)

	DISCRETE_GAIN(NODE_43, NODE_40, (65.0-37.0)/15)				/* Duty Cycle modelling */
	DISCRETE_ADDER2(NODE_44, 1, NODE_43, 37.0)

	DISCRETE_SQUAREWAVE(FIRETRUCK_SIRENSND, FIRETRUCK_ATTRACT_EN, NODE_42, 31.3, NODE_44, 0, 0)	/* VCO */

	/************************************************/
	/* Crash circuit is built around a noise        */
	/* generator built from 2 shift registers that  */
	/* are clocked by the 2V signal.                */
	/* 2V = HSYNC/4                                 */
	/*    = 15750/4                                 */
	/* Output is binary weighted with 4 bits of     */
	/* crash volume.                                */
	/* Volume is inverted by input register mapping */
	/************************************************/
	DISCRETE_LOGIC_INVERT(NODE_50, 1, FIRETRUCK_ATTRACT_EN)
	DISCRETE_LFSR_NOISE(FIRETRUCK_NOISE, FIRETRUCK_ATTRACT_EN, NODE_50, 15750.0/4, 1.0, 0, 0, &firetrk_lfsr)

	DISCRETE_MULTIPLY(NODE_51, 1, FIRETRUCK_NOISE, FIRETRUCK_CRASHSND_DATA)
	DISCRETE_GAIN(FIRETRUCK_CRASHSND, NODE_51, 452.2/15)

	/************************************************/
	/* Skid circuit takes the noise output from     */
	/* the crash circuit and applies +ve feedback   */
	/* to cause oscillation. There is also an RC    */
	/* filter on the input to the feedback cct.     */
	/* RC is 2.2K & 2.2uF                           */
	/* Feedback cct is modelled by using the RC out */
	/* as the frequency input on a VCO,             */
	/* breadboarded freq range as:                  */
	/*  0 = 1380Hz, 32% duty                        */
	/*  1 =  626Hz, 13% duty                        */
	/************************************************/
	DISCRETE_INVERT(NODE_60, FIRETRUCK_NOISE)
	DISCRETE_RCFILTER(NODE_61, 1, NODE_60, 2200, 2.2e-6)
	DISCRETE_GAIN(NODE_62, NODE_61, 1380.0-626.0)
	DISCRETE_ADDER2(NODE_63, 1, NODE_62, 626.0+((1380.0-626.0)/2))	/* Frequency */
	DISCRETE_GAIN(NODE_64, NODE_61, 32.0-13.0)
	DISCRETE_ADDER2(NODE_65, 1, NODE_64, 13.0+((32.0-13.0)/2))	/* Duty */
	DISCRETE_SQUAREWAVE(FIRETRUCK_SKIDSND, FIRETRUCK_SKIDSND_EN, NODE_63, 142.4, NODE_65, 0, 0.0)

	/************************************************/
	/* Bell circuit -                               */
	/* The Hsync signal is put into a div 16        */
	/* counter.                                     */
	/************************************************/
	DISCRETE_GAIN(NODE_70, FIRETRUCK_BELLSND_EN, 213.6)
	DISCRETE_RCDISC(NODE_71, NODE_70, 500, 33000, 1.0e-5)

	DISCRETE_SQUAREWAVE(FIRETRUCK_BELLSND, 1, 15750.0/16, NODE_71, 50.0, 0, 0.0)

	/************************************************/
	/* Extended play circuit is just the 8V signal  */
	/* 8V = HSYNC/16                                */
	/*    = 15750/16                                */
	/************************************************/
	DISCRETE_SQUAREWAVE(FIRETRUCK_XTNDPLY, FIRETRUCK_XTNDPLY_EN, 15750.0/16, 1000.0, 50.0, 0, 0.0)

	/************************************************/
	/* Combine all 7 sound sources.                 */
	/* Add some final gain to get to a good sound   */
	/* level.                                       */
	/************************************************/
	DISCRETE_ADDER4(NODE_91, 1, FIRETRUCK_MOTORSND, FIRETRUCK_HORNSND, FIRETRUCK_SIRENSND, FIRETRUCK_CRASHSND)
	DISCRETE_ADDER4(NODE_92, 1, NODE_91, FIRETRUCK_SKIDSND, FIRETRUCK_BELLSND, FIRETRUCK_XTNDPLY)
	DISCRETE_GAIN(NODE_90, NODE_92, 65534.0/(127.0+213.6+31.3+452.2+142.4+213.6+1000))

	DISCRETE_OUTPUT(NODE_90, 100)
DISCRETE_SOUND_END

static void init_palette(unsigned char *game_palette, unsigned short *game_colortable,const unsigned char *color_prom)
{
	memcpy(game_palette,palette,sizeof(palette));
	memcpy(game_colortable,colortable,sizeof(colortable));
}

static struct osd_bitmap *buf;

int firetruck_vh_init( void )
{
	buf = osd_alloc_bitmap(
			Machine->scrbitmap->width,
			Machine->scrbitmap->height,
			Machine->scrbitmap->depth );

	if( buf ) return 0;

	return -1;
}

void firetruck_vh_stop( void )
{
	osd_free_bitmap( buf );
}

static void draw_sprites( struct osd_bitmap *bitmap )
{
	int color		= firetruck_invert_display?3:7; /* invert display */
	int track_color = Machine->pens[firetruck_invert_display?0:3];
	int bgcolor		= firetruck_invert_display?4:0;
	int cabrot		= videoram[0x1080];
	int hpos		= videoram[0x1460];
	int vpos		= videoram[0x1480];
	int tailrot		= videoram[0x14a0];
	int pen;
	int sx, sy;
	int x,y;
	int flipx,flipy;
	int pitch;

	pitch = (bitmap->depth==8)?bitmap->width:(bitmap->width*2);

	/* This isn't the most efficient way to handle collision detection, but it works:
	 *
	 *	1. draw background
	 *	2. copy background to screen-sized private buffer
	 *	3. draw sprites using background color
	 *	4. compare buffer and background; if they differ, a collision occured
	 *	5. use the color of the background pixel where a collision occured to
	 *		flag collision type appropriately.
	 *	6. draw sprites normally
	 *
	 */
	for( sy=0; sy<bitmap->height; sy++ )
	{
		memcpy( buf->line[sy], bitmap->line[sy], pitch );
	}

	flipx = tailrot&0x08;
	flipy = tailrot&0x10;
	sx = flipx?(hpos-47):(255-hpos-47);
	sy = flipy?(sy = vpos-47):(255-vpos-47);

	drawgfx( bitmap,
		Machine->gfx[2],
		tailrot&0x07, /* code */
		bgcolor,
		flipx,flipy,
		sx,sy,
		&Machine->visible_area,
		TRANSPARENCY_PEN,0 );

	drawgfx( bitmap,
		Machine->gfx[(cabrot&0x10)?3:4],
		cabrot&0x03, /* code */
		bgcolor,
		cabrot&0x04, /* flipx */
		cabrot&0x08, /* flipy */
		124,120,
		&Machine->visible_area,
		TRANSPARENCY_PEN,0 );

	if( bitmap->depth==8 )
	{
		for( y=0; y<bitmap->height; y++ )
		{
			for( x=0; x<bitmap->width; x++ )
			{
				pen = buf->line[y][x];
				if( pen != bitmap->line[y][x] )
				{
					if( pen == track_color )
					{
						firetruck_bit7_flags |= 0x40; /* crash */
					}
					else
					{
						firetruck_bit0_flags |= 0x40; /* skid */
					}
				}
			}
		}
	}
	else
	{
		for( y=0; y<bitmap->height; y++ )
		{
			for( x=0; x<bitmap->width; x++ )
			{
				pen = ((UINT16 *)buf->line[y])[x];
				if( pen != ((UINT16 *)bitmap->line[y])[x] )
				{
					if( pen == track_color )
					{
						firetruck_bit7_flags |= 0x40; /* crash */
					}
					else
					{
						firetruck_bit0_flags |= 0x40; /* skid */
					}
				}
			}
		}
	}

	drawgfx( bitmap,
		Machine->gfx[2],
		tailrot&0x07, /* code */
		color,
		flipx,flipy,
		sx,sy,
		&Machine->visible_area,
		TRANSPARENCY_PEN,0 );

	drawgfx( bitmap,
		Machine->gfx[(cabrot&0x10)?3:4],
		cabrot&0x03, /* code */
		color,
		cabrot&0x04, /* flipx */
		cabrot&0x08, /* flipy */
		124,120,
		&Machine->visible_area,
		TRANSPARENCY_PEN,0 );
}

static void draw_text( struct osd_bitmap *bitmap )
{
	int color = firetruck_invert_display?3:7; /* invert display */
	int x,y,tile_number;
	const UINT8 *source = videoram;

	for( x=0; x<=256-16; x+=256-16 )
	{
		for( y=0; y<256; y+=16 )
		{
			tile_number = *source++;

			drawgfx( bitmap,
				Machine->gfx[0],
				tile_number,
				color,
				0,0, /* flip */
				x,y,
				&Machine->visible_area,
				TRANSPARENCY_NONE,0 );
		}
	}
}

static void draw_background( struct osd_bitmap *bitmap )
{
	int color = firetruck_invert_display?4:0; /* invert display */
	int pvpload = videoram[0x1000];
	int phpload = videoram[0x1020];

	int x,y,tile_number;
	const UINT8 *source = videoram+0x800;

	for( y=0; y<256; y+=16 )
	{
		for( x=0; x<256; x+=16 )
		{
			tile_number = *source++;

			drawgfx( bitmap,
				Machine->gfx[1],
				tile_number&0x3f,
				color+(tile_number>>6), /* color */
				0,0, /* flip */
				(x-phpload)&0xff,(y-pvpload)&0xff,
				&Machine->visible_area,
				TRANSPARENCY_NONE,0 );
		}
	}
}

void firetruck_vh_screenrefresh( struct osd_bitmap *bitmap, int fullrefresh )
{
	draw_background( bitmap );
	draw_text( bitmap );
	draw_sprites( bitmap );

	// Map horn button onto discrete sound emulation
	discrete_sound_w(0x01,input_port_6_r(0));
}

static READ_HANDLER( firetruck_dsw_r )
{
	int dip = readinputport(0);
	int data = (dip&0x3)<<2; /* coinage */
	switch (offset)
	{
	case 0x00:  data |= (dip>>6)&0x03; break; /* Language */
	case 0x01:  data |= (dip>>4)&0x03; break; /* Game Time */
	case 0x02:  data |= (dip>>2)&0x03; break; /* Extended Play */
	}
	return data;
}

static READ_HANDLER( firetruck_io_r )
{
	data8_t data, bit0, bit6, bit7;
	int bit,steer_poll;

	bit0 = readinputport(1);
	bit6 = readinputport(2);
	bit7 = readinputport(3);
	bit = 1<<offset;

	steer_poll = ((INT8)(readinputport(4) - firetruck_steer1))/4;
	if( steer_poll )
	{
		if( steer_poll<0 ) bit0 |= 0x04;
		firetruck_bit7_flags &= ~0x04;
	}

	steer_poll = ((INT8)(readinputport(5) - firetruck_steer2))/4;
	if( steer_poll )
	{
		if( steer_poll<0 ) bit0 |= 0x08;
		firetruck_bit7_flags &= ~0x08;
	}

	bit0 |= firetruck_bit0_flags;
	bit7 |= firetruck_bit7_flags;

	data = 0x00;
	if( bit0 & bit ) data |= 0x01;
	if( bit6 & bit ) data |= 0x40;
	if( bit7 & bit ) data |= 0x80;
	return data;
}

static WRITE_HANDLER( steer_reset_w )
{
	firetruck_bit7_flags |= 0x0c;
	firetruck_steer1 = readinputport(4);
	firetruck_steer2 = readinputport(5);
}

static WRITE_HANDLER( crash_reset_w )
{
	firetruck_bit7_flags &= ~0x40;
}

static WRITE_HANDLER( skid_reset_w )
{
	firetruck_bit0_flags &= ~0x40;
	// Clear skip sound output, Preset to D-Type, fed into NAND with crash noise
	discrete_sound_w(0x04,0x00);
}

static WRITE_HANDLER( firetruck_out_w )
{
	firetruck_invert_display = data&0x04;
	//		x-------	0x80 BELLOUT
	//		-x------	0x40 unused?
	//		--x-----	0x20 LED for START button
	//		---X----	0x10 ATTRACT (disables horn circuit)
	//		----x---	0x08 (LED for track select button?)
	//		-----x--	0x04 FLASH - inverts screen
	//		------x-	0x02 unused?
	//		-------x	0x01 LED for START button
	discrete_sound_w(0x06,!(data&0x10));	// Attract
	discrete_sound_w(0x05,(data&0x80)?0x00:0x01);	// Bell
}

static WRITE_HANDLER( firetruck_motorsnd_w )
{
	//		xxxx----	0xf0 Siren Frequency
	//		----xxxx	0x0f Motor Frequency
	discrete_sound_w(0x00,data&0x0f);
	discrete_sound_w(0x02,(data&0xf0)>>4);
}

static WRITE_HANDLER( firetruck_crashsnd_w )
{
	//		xxxx----	0xf0 Crash Volume
	discrete_sound_w(0x03,(data&0xf0)>>4);
}

static WRITE_HANDLER( firetruck_skidsnd_w )
{
	//		Write starts the skid sound, Clear input to D-Type, fed into NAND with crash noise
	discrete_sound_w(0x04,0x01);
}

static WRITE_HANDLER( firetruck_xtndply_w )
{
	//		-------x	0x01 Extend play sound
	discrete_sound_w(0x07,!(data&0x01));
}



static struct MemoryReadAddress firetruck_readmem[] =
{ 
	{ 0x0000, 0x01ff, MRA_RAM },
	{ 0x0800, 0x08ff, MRA_RAM },
	{ 0x1800, 0x1807, firetruck_io_r },
	{ 0x1c00, 0x1c02, firetruck_dsw_r },
	{ 0x2000, 0x3fff, MRA_ROM },
	{ 0xf000, 0xffff, MRA_ROM },
        { -1 }  /* end of table */
};


static struct MemoryWriteAddress firetruck_writemem[] =
{ 
	{ 0x0000, 0x01ff, MWA_RAM, &videoram },
	{ 0x0800, 0x08ff, MWA_RAM },				/* PRAM */
	{ 0x1000, 0x1000, MWA_RAM },				/* PVPLOAD */
	{ 0x1020, 0x1020, MWA_RAM },				/* PHPLOAD */
	{ 0x1048, 0x1048, crash_reset_w },			/* CRASHRESET */
	{ 0x1060, 0x1060, skid_reset_w },			/* SKIDRESET */
	{ 0x1080, 0x1080, MWA_RAM },				/* CABROT */
	{ 0x10a0, 0x10a0, steer_reset_w },			/* STEERRESET */
	{ 0x10c0, 0x10c0, MWA_RAM },				/* WATCHDOGRESET */
	{ 0x10e0, 0x10e0, MWA_RAM },				/* ARROWOFF */
	{ 0x1400, 0x1400, firetruck_motorsnd_w },	/* MOTORSND */
	{ 0x1420, 0x1420, firetruck_crashsnd_w }, 	/* CRASHSND */
	{ 0x1440, 0x1440, firetruck_skidsnd_w },	/* SKIDSND */
	{ 0x1460, 0x1460, MWA_RAM },				/* HPOS */
	{ 0x1480, 0x1480, MWA_RAM },				/* VPOS */
	{ 0x14a0, 0x14a0, MWA_RAM },				/* TAILROT */
	{ 0x14c0, 0x14c0, firetruck_out_w },		/* OUT */
	{ 0x14e0, 0x14e0, firetruck_xtndply_w },	/* XTNDPLY */
	{ 0x1800, 0x1807, MWA_RAM },				/* ? */
	{ 0x2000, 0x3fff, MWA_ROM },
	{ 0xf000, 0xffff, MWA_ROM },
        { -1 }  /* end of table */
};


int firetruck_interrupt( void )
{
	if( cpu_getiloops()==0 )
	{
		/* NMI is triggered by VBLANK */
		if( readinputport(1)&0x80 )
		{
			/* disable NMI while in test mode */
			return ignore_interrupt();
		}
		else
		{
			return nmi_interrupt();
		}
	}
	else
	{
		return interrupt(); /* IRQ */
	}
}

static struct MachineDriver machine_driver_firetruck =
{
	{
		{
			CPU_M6808,
			1008000,
				/* 1MHz during normal operation */
				/* 750Khz during self-test sequence */
			firetruck_readmem,firetruck_writemem,0,0,
			firetruck_interrupt,2
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame */
	0,
	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 2*8, 30*8-1 },
	firetruck_gfxdecodeinfo,
	sizeof(palette)/3,sizeof(colortable)/2,
	init_palette,
	VIDEO_TYPE_RASTER,
	0,
	firetruck_vh_init,
	firetruck_vh_stop,
	firetruck_vh_screenrefresh,
	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DISCRETE,
			&firetrk_sound_interface
		}
	}
};

void init_firetruck( void )
{
	/* combine the 4 bit program ROMs */
	UINT8 *pMem = memory_region( REGION_CPU1 );
	int i;
	for( i=0; i<0x1000; i++ )
	{
		int msn = pMem[0x10000+i];
		int lsn = pMem[0x10000+i+0x1000];
		int data = (msn<<4)|lsn;
		pMem[i+0x3000] = data;
		pMem[i+0xf000] = data;
	}
}

ROM_START( firetrk )
	ROM_REGION( 0x12000, REGION_CPU1 )
	ROM_LOAD( "32823-02.c1", 0x2000, 0x800, 0x9570bdd3 )	/* ROM0 */
	ROM_LOAD( "32824-01.d1", 0x2800, 0x800, 0xa5fc5629 )	/* ROM1 */
	ROM_LOAD( "32816-01.k1", 0x10000, 0x800, 0xc0535598 )	/* ROM2a */
	ROM_LOAD( "32815-01.j1", 0x10800, 0x800, 0x506ee759 )	/* ROM3a */
	ROM_LOAD( "32820-01.k2", 0x11000, 0x800, 0x5733f9ed )	/* ROM2b */
	ROM_LOAD( "32819-01.j2", 0x11800, 0x800, 0xf1c3fa87 )	/* ROM3b */

	ROM_REGION( 0x800, REGION_GFX1| REGIONFLAG_DISPOSE )
	ROM_LOAD( "32827-01.r3", 0x000, 0x800, 0xcca31d2b ) /* characters */

	ROM_REGION( 0x800, REGION_GFX2| REGIONFLAG_DISPOSE )
	ROM_LOAD( "32828-02.f5", 0x000, 0x800, 0x68ef5f19 ) /* tiles */

	ROM_REGION( 0x1000, REGION_GFX3| REGIONFLAG_DISPOSE )
	ROM_LOAD( "32829-01.j5", 0x000, 0x800, 0xe7267d71 ) /* Trailer */
	ROM_LOAD( "32830-01.l5", 0x800, 0x800, 0xe4d8b685 )

	ROM_REGION( 0x400, REGION_GFX4| REGIONFLAG_DISPOSE )
	ROM_LOAD( "32831-01.p7", 0x000, 0x400, 0xbb8d144f ) /* Cab */
ROM_END

INPUT_PORTS_START( firetruck )
	PORT_START
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	/* Extended Play DIP affects required points to earn extended play.
	 * This interacts with the Game Time setting. */
	PORT_DIPNAME( 0x0c, 0x04, "Extended Play" )
	PORT_DIPSETTING(    0x00, "No Extended Play" )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x08, "Medium" )
	PORT_DIPSETTING(    0x0c, "Hard" )
	PORT_DIPNAME( 0x30, 0x10, "Game Time")
	PORT_DIPSETTING(    0x00, "60 Seconds" )
	PORT_DIPSETTING(    0x10, "90 Seconds" )
	PORT_DIPSETTING(    0x20, "120 Seconds" )
	PORT_DIPSETTING(    0x30, "150 Seconds" )
	PORT_DIPNAME( 0xc0, 0x00, "Game Language")
	PORT_DIPSETTING(    0x00, "English" )
	PORT_DIPSETTING(    0x40, "French" )
	PORT_DIPSETTING(    0x80, "Spanish" )
	PORT_DIPSETTING(    0xc0, "German" )

	PORT_START /* bit 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,	IPT_UNUSED )	/* SPARE */
	PORT_BITX(0x02, IP_ACTIVE_HIGH,	IPT_BUTTON1,"Gas",IP_KEY_DEFAULT,IP_JOY_DEFAULT )	/* GAS */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,	IPT_UNUSED )	/* STEER DIR1 */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,	IPT_UNUSED )	/* STEER DIR2 */
	PORT_BITX(0x10, IP_ACTIVE_LOW,	IPT_BUTTON2,"Bell",IP_KEY_DEFAULT,IP_JOY_DEFAULT )	/* BELL */
	PORT_BIT( 0x20, IP_ACTIVE_HIGH,	IPT_TILT )		/* SLAM */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,	IPT_UNUSED )	/* SKIDIN */
	PORT_BITX(0x80, IP_ACTIVE_HIGH,	IPT_SERVICE|IPF_TOGGLE, DEF_STR(Service_Mode), KEYCODE_F2, IP_JOY_NONE )

	PORT_START /* bit 6 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH,	IPT_START1 )	/* START1 */
	PORT_BIT( 0x02, IP_ACTIVE_HIGH,	IPT_START2 )	/* START2 */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,	IPT_START3 )	/* START3 */
	PORT_BITX(0x08, IP_ACTIVE_HIGH,	IPT_BUTTON3, "Track Select", IP_KEY_DEFAULT, IP_JOY_DEFAULT)
	PORT_BIT( 0x10, IP_ACTIVE_LOW,	IPT_UNUSED )	/* SPARE */
	PORT_BIT( 0x20, IP_ACTIVE_LOW,	IPT_VBLANK )	/* VBLANK */
	PORT_DIPNAME( 0x40, 0x40, "Cabinet Type" )		/* CABINET */
	PORT_DIPSETTING(	0x00, "Single Player" )
	PORT_DIPSETTING(	0x40, "2 Players" )
	PORT_BITX(0x80, IP_ACTIVE_HIGH,	IPT_SERVICE, "Diag Hold", KEYCODE_F6, IP_JOY_NONE)

	PORT_START /* bit 7 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,	IPT_UNUSED )	/* SPARE */
	PORT_BIT( 0x02, IP_ACTIVE_LOW,	IPT_UNUSED )	/* SPARE */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH,	IPT_UNUSED )	/* STEER FLAG1 */
	PORT_BIT( 0x08, IP_ACTIVE_HIGH,	IPT_UNUSED )	/* STEER FLAG2 */
	PORT_BIT( 0x10, IP_ACTIVE_LOW,	IPT_COIN1 )		/* COIN1 */
	PORT_BIT( 0x20, IP_ACTIVE_LOW,	IPT_COIN2 )		/* COIN1 */
	PORT_BIT( 0x40, IP_ACTIVE_HIGH,	IPT_UNUSED )	/* CRASHIN */
	PORT_BITX(0x80, IP_ACTIVE_HIGH,	IPT_SERVICE, "Diag Step", KEYCODE_F1, IP_JOY_NONE)

	PORT_START /* p1 steering */
	PORT_ANALOG ( 0xff, 0x80, IPT_DIAL, 100, 0, 0 ,255 )

	PORT_START /* p2 steering */
	PORT_ANALOG ( 0xff, 0x80, IPT_DIAL | IPF_PLAYER2, 100, 0, 0, 255 )

	PORT_START /* Mechanical Horn switch */
	PORT_BITX(0x01, IP_ACTIVE_HIGH, IPT_BUTTON4,"Horn Button",IP_KEY_DEFAULT,IP_JOY_DEFAULT)

INPUT_PORTS_END

/*           rom      parent    machine		inp			init */
GAME( 1978, firetrk, 0,        firetruck,	firetruck,	firetruck,	ROT270, "Atari", "Fire Truck")

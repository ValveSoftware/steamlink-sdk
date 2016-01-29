/*
 * avgdvg.c: Atari DVG and AVG simulators
 *
 * Copyright 1991, 1992, 1996 Eric Smith
 *
 * Modified for the MAME project 1997 by
 * Brad Oliver, Bernd Wiebelt, Aaron Giles, Andrew Caldwell
 *
 * 971108 Disabled vector timing routines, introduced an ugly (but fast!)
 *        busy flag hack instead. BW
 * 980202 New anti aliasing code by Andrew Caldwell (.ac)
 * 980206 New (cleaner) busy flag handling.
 *        Moved LBO's buffered point into generic vector code. BW
 * 980212 Introduced timing code based on Aaron timer routines. BW
 * 980318 Better color handling, Bzone and MHavoc clipping. BW
 *
 * Battlezone uses a red overlay for the top of the screen and a green one
 * for the rest. There is a circuit to clip color 0 lines extending to the
 * red zone. This is emulated now. Thanks to Neil Bradley for the info. BW
 *
 * Frame and interrupt rates (Neil Bradley) BW
 * ~60 fps/4.0ms: Asteroid, Asteroid Deluxe
 * ~40 fps/4.0ms: Lunar Lander
 * ~40 fps/4.1ms: Battle Zone
 * ~45 fps/5.4ms: Space Duel, Red Baron
 * ~30 fps/5.4ms: StarWars
 *
 * Games with self adjusting framerate
 *
 * 4.1ms: Black Widow, Gravitar
 * 4.1ms: Tempest
 * Major Havoc
 * Quantum
 *
 * TODO: accurate vector timing (need timing diagramm)
 */

#include "driver.h"
#include "avgdvg.h"
#include "vector.h"

#define VEC_SHIFT 16	/* fixed for the moment */
#define BRIGHTNESS 12   /* for maximum brightness, use 16! */


/* the screen is red above this Y coordinate */
#define BZONE_TOP 0x0050
#define BZONE_CLIP \
	vector_add_clip (xmin<<VEC_SHIFT, BZONE_TOP<<VEC_SHIFT, \
					xmax<<VEC_SHIFT, ymax<<VEC_SHIFT)
#define BZONE_NOCLIP \
	vector_add_clip (xmin<<VEC_SHIFT, ymin <<VEC_SHIFT, \
					xmax<<VEC_SHIFT, ymax<<VEC_SHIFT)

#define MHAVOC_YWINDOW 0x0048
#define MHAVOC_CLIP \
	vector_add_clip (xmin<<VEC_SHIFT, MHAVOC_YWINDOW<<VEC_SHIFT, \
					xmax<<VEC_SHIFT, ymax<<VEC_SHIFT)
#define MHAVOC_NOCLIP \
	vector_add_clip (xmin<<VEC_SHIFT, ymin <<VEC_SHIFT, \
					xmax<<VEC_SHIFT, ymax<<VEC_SHIFT)

static int vectorEngine = USE_DVG;
static int flipword = 0; /* little/big endian issues */
static int busy = 0;     /* vector engine busy? */
static int colorram[16]; /* colorram entries */

/* These hold the X/Y coordinates the vector engine uses */
static int width; int height;
static int xcenter; int ycenter;
static int xmin; int xmax;
static int ymin; int ymax;


int vector_updates; /* avgdvg_go_w()'s per Mame frame, should be 1 */

static int vg_step = 0;    /* single step the vector generator */
static int total_length;   /* length of all lines drawn in a frame */

#define MAXSTACK 8 	/* Tempest needs more than 4     BW 210797 */

/* AVG commands */
#define VCTR 0
#define HALT 1
#define SVEC 2
#define STAT 3
#define CNTR 4
#define JSRL 5
#define RTSL 6
#define JMPL 7
#define SCAL 8

/* DVG commands */
#define DVCTR 0x01
#define DLABS 0x0a
#define DHALT 0x0b
#define DJSRL 0x0c
#define DRTSL 0x0d
#define DJMPL 0x0e
#define DSVEC 0x0f

#define twos_comp_val(num,bits) ((num&(1<<(bits-1)))?(num|~((1<<bits)-1)):(num&((1<<bits)-1)))

char *avg_mnem[] = { "vctr", "halt", "svec", "stat", "cntr",
			 "jsrl", "rtsl", "jmpl", "scal" };

char *dvg_mnem[] = { "????", "vct1", "vct2", "vct3",
		     "vct4", "vct5", "vct6", "vct7",
		     "vct8", "vct9", "labs", "halt",
		     "jsrl", "rtsl", "jmpl", "svec" };

/* ASG 971210 -- added banks and modified the read macros to use them */
#define BANK_BITS 13
#define BANK_SIZE (1<<BANK_BITS)
#define NUM_BANKS (0x4000/BANK_SIZE)
#define VECTORRAM(offset) (vectorbank[(offset)>>BANK_BITS][(offset)&(BANK_SIZE-1)])
static unsigned char *vectorbank[NUM_BANKS];

#define map_addr(n) (((n)<<1))
#define memrdwd(offset) (VECTORRAM(offset) | (VECTORRAM(offset+1)<<8))
/* The AVG used by Star Wars reads the bytes in the opposite order */
#define memrdwd_flip(offset) (VECTORRAM(offset+1) | (VECTORRAM(offset)<<8))
#define max(x,y) (((x)>(y))?(x):(y))


INLINE void vector_timer (int deltax, int deltay)
{
	deltax = abs (deltax);
	deltay = abs (deltay);
	total_length += max (deltax, deltay) >> VEC_SHIFT;
}

INLINE void dvg_vector_timer (int scale)
{
	total_length += scale;
}

static void dvg_generate_vector_list(void)
{
	int pc;
	int sp;
	int stack [MAXSTACK];

	int scale;
	int statz;

	int currentx, currenty;

	int done = 0;

	int firstwd;
	int secondwd = 0; /* Initialize to tease the compiler */
	int opcode;

	int x, y;
	int z, temp;
	int a;

	int deltax, deltay;

	vector_clear_list();
	pc = 0;
	sp = 0;
	scale = 0;
	statz = 0;

	currentx = 0;
	currenty = 0;

	while (!done)
	{

#ifdef VG_DEBUG
		if (vg_step)
		{
	  		logerror("Current beam position: (%d, %d)\n",
				currentx, currenty);
	  		getchar();
		}
#endif

		firstwd = memrdwd (map_addr (pc));
		opcode = firstwd >> 12;
#ifdef VG_DEBUG
		logerror("%4x: %4x ", map_addr (pc), firstwd);
#endif
		pc++;
		if ((opcode >= 0 /* DVCTR */) && (opcode <= DLABS))
		{
			secondwd = memrdwd (map_addr (pc));
			pc++;
#ifdef VG_DEBUG
			logerror("%s ", dvg_mnem [opcode]);
			logerror("%4x  ", secondwd);
#endif
		}
#ifdef VG_DEBUG
		else logerror("Illegal opcode ");
#endif

		switch (opcode)
		{
			case 0:
#ifdef VG_DEBUG
	 			logerror("Error: DVG opcode 0!  Addr %4x Instr %4x %4x\n", map_addr (pc-2), firstwd, secondwd);
				done = 1;
				break;
#endif
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
	  			y = firstwd & 0x03ff;
				if (firstwd & 0x400)
					y=-y;
				x = secondwd & 0x3ff;
				if (secondwd & 0x400)
					x=-x;
				z = secondwd >> 12;
#ifdef VG_DEBUG
				logerror("(%d,%d) z: %d scal: %d", x, y, z, opcode);
#endif
	  			temp = ((scale + opcode) & 0x0f);
	  			if (temp > 9)
					temp = -1;
	  			deltax = (x << VEC_SHIFT) >> (9-temp);		/* ASG 080497 */
				deltay = (y << VEC_SHIFT) >> (9-temp);		/* ASG 080497 */
	  			currentx += deltax;
				currenty -= deltay;
				dvg_vector_timer(temp);

				/* ASG 080497, .ac JAN2498 - V.V */
				if (translucency)
					z = z * BRIGHTNESS;
				else
					if (z) z = (z << 4) | 0x0f;
				vector_add_point (currentx, currenty, colorram[1], z);

				break;

			case DLABS:
				x = twos_comp_val (secondwd, 12);
				y = twos_comp_val (firstwd, 12);
	  			scale = (secondwd >> 12);
				currentx = ((x-xmin) << VEC_SHIFT);		/* ASG 080497 */
				currenty = ((ymax-y) << VEC_SHIFT);		/* ASG 080497 */
#ifdef VG_DEBUG
				logerror("(%d,%d) scal: %d", x, y, secondwd >> 12);
#endif
				break;

			case DHALT:
#ifdef VG_DEBUG
				if ((firstwd & 0x0fff) != 0)
      				logerror("(%d?)", firstwd & 0x0fff);
#endif
				done = 1;
				break;

			case DJSRL:
				a = firstwd & 0x0fff;
#ifdef VG_DEBUG
				logerror("%4x", map_addr(a));
#endif
				stack [sp] = pc;
				if (sp == (MAXSTACK - 1))
	    			{
					//logerror("\n*** Vector generator stack overflow! ***\n");
					done = 1;
					sp = 0;
				}
				else
					sp++;
				pc = a;
				break;

			case DRTSL:
#ifdef VG_DEBUG
				if ((firstwd & 0x0fff) != 0)
					 logerror("(%d?)", firstwd & 0x0fff);
#endif
				if (sp == 0)
	    			{
					//logerror("\n*** Vector generator stack underflow! ***\n");
					done = 1;
					sp = MAXSTACK - 1;
				}
				else
					sp--;
				pc = stack [sp];
				break;

			case DJMPL:
				a = firstwd & 0x0fff;
#ifdef VG_DEBUG
				logerror("%4x", map_addr(a));
#endif
				pc = a;
				break;

			case DSVEC:
				y = firstwd & 0x0300;
				if (firstwd & 0x0400)
					y = -y;
				x = (firstwd & 0x03) << 8;
				if (firstwd & 0x04)
					x = -x;
				z = (firstwd >> 4) & 0x0f;
				temp = 2 + ((firstwd >> 2) & 0x02) + ((firstwd >>11) & 0x01);
	  			temp = ((scale + temp) & 0x0f);
				if (temp > 9)
					temp = -1;
#ifdef VG_DEBUG
				logerror("(%d,%d) z: %d scal: %d", x, y, z, temp);
#endif

				deltax = (x << VEC_SHIFT) >> (9-temp);	/* ASG 080497 */
				deltay = (y << VEC_SHIFT) >> (9-temp);	/* ASG 080497 */
	  			currentx += deltax;
				currenty -= deltay;
				dvg_vector_timer(temp);

				/* ASG 080497, .ac JAN2498 */
				if (translucency)
					z = z * BRIGHTNESS;
				else
					if (z) z = (z << 4) | 0x0f;
				vector_add_point (currentx, currenty, colorram[1], z);
				break;

			default:
				//logerror("Unknown DVG opcode found\n");
				done = 1;
		}
#ifdef VG_DEBUG
      		logerror("\n");
#endif
	}
}

/*
Atari Analog Vector Generator Instruction Set

Compiled from Atari schematics and specifications
Eric Smith  7/2/92
---------------------------------------------

NOTE: The vector generator is little-endian.  The instructions are 16 bit
      words, which need to be stored with the least significant byte in the
      lower (even) address.  They are shown here with the MSB on the left.

The stack allows four levels of subroutine calls in the TTL version, but only
three levels in the gate array version.

inst  bit pattern          description
----  -------------------  -------------------
VCTR  000- yyyy yyyy yyyy  normal vector
      zzz- xxxx xxxx xxxx
HALT  001- ---- ---- ----  halt - does CNTR also on newer hardware
SVEC  010y yyyy zzzx xxxx  short vector - don't use zero length
STAT  0110 ---- zzzz cccc  status
SCAL  0111 -bbb llll llll  scaling
CNTR  100- ---- dddd dddd  center
JSRL  101a aaaa aaaa aaaa  jump to subroutine
RTSL  110- ---- ---- ----  return
JMPL  111a aaaa aaaa aaaa  jump

-     unused bits
x, y  relative x and y coordinates in two's complement (5 or 13 bit,
      5 bit quantities are scaled by 2, so x=1 is really a length 2 vector.
z     intensity, 0 = blank, 1 means use z from STAT instruction,  2-7 are
      doubled for actual range of 4-14
c     color
b     binary scaling, multiplies all lengths by 2**(1-b), 0 is double size,
      1 is normal, 2 is half, 3 is 1/4, etc.
l     linear scaling, multiplies all lengths by 1-l/256, don't exceed $80
d     delay time, use $40
a     address (word address relative to base of vector memory)

Notes:

Quantum:
        the VCTR instruction has a four bit Z field, that is not
        doubled.  The value 2 means use Z from STAT instruction.

        the SVEC instruction can't be used

Major Havoc:
        SCAL bit 11 is used for setting a Y axis window.

        STAT bit 11 is used to enable "sparkle" color.
        STAT bit 10 inverts the X axis of vectors.
        STAT bits 9 and 8 are the Vector ROM bank select.

Star Wars:
        STAT bits 10, 9, and 8 are used directly for R, G, and B.
*/

static void avg_generate_vector_list (void)
{

	int pc;
	int sp;
	int stack [MAXSTACK];

	int scale;
	int statz   = 0;
	int sparkle = 0;
	int xflip   = 0;

	int color   = 0;
	int bz_col  = -1; /* Battle Zone color selection */
	int ywindow = -1; /* Major Havoc Y-Window */

	int currentx, currenty;
	int done    = 0;

	int firstwd, secondwd;
	int opcode;

	int x, y, z=0, b, l, d, a;

	int deltax, deltay;


	pc = 0;
	sp = 0;
	statz = 0;
	color = 0;

	if (flipword)
	{
		firstwd = memrdwd_flip (map_addr (pc));
		secondwd = memrdwd_flip (map_addr (pc+1));
	}
	else
	{
		firstwd = memrdwd (map_addr (pc));
		secondwd = memrdwd (map_addr (pc+1));
	}
	if ((firstwd == 0) && (secondwd == 0))
	{
		//logerror("VGO with zeroed vector memory\n");
		return;
	}

	/* kludge to bypass Major Havoc's empty frames. BW 980216 */
	if (vectorEngine == USE_AVG_MHAVOC && firstwd == 0xafe2)
		return;

	scale = 0;          /* ASG 080497 */
	currentx = xcenter; /* ASG 080497 */ /*.ac JAN2498 */
	currenty = ycenter; /* ASG 080497 */ /*.ac JAN2498 */

	vector_clear_list();

	while (!done)
	{

#ifdef VG_DEBUG
		if (vg_step) getchar();
#endif

		if (flipword) firstwd = memrdwd_flip (map_addr (pc));
		else          firstwd = memrdwd      (map_addr (pc));

		opcode = firstwd >> 13;
#ifdef VG_DEBUG
		logerror("%4x: %4x ", map_addr (pc), firstwd);
#endif
		pc++;
		if (opcode == VCTR)
		{
			if (flipword) secondwd = memrdwd_flip (map_addr (pc));
			else          secondwd = memrdwd      (map_addr (pc));
			pc++;
#ifdef VG_DEBUG
			logerror("%4x  ", secondwd);
#endif
		}
#ifdef VG_DEBUG
		else logerror("      ");
#endif

		if ((opcode == STAT) && ((firstwd & 0x1000) != 0))
			opcode = SCAL;

#ifdef VG_DEBUG
		logerror("%s ", avg_mnem [opcode]);
#endif

		switch (opcode)
		{
			case VCTR:

				if (vectorEngine == USE_AVG_QUANTUM)
				{
					x = twos_comp_val (secondwd, 12);
					y = twos_comp_val (firstwd, 12);
				}
				else
				{
					/* These work for all other games. */
					x = twos_comp_val (secondwd, 13);
					y = twos_comp_val (firstwd, 13);
				}
				z = (secondwd >> 12) & ~0x01;

				/* z is the maximum DAC output, and      */
				/* the 8 bit value from STAT does some   */
				/* fine tuning. STATs of 128 should give */
				/* highest intensity. */
				if (vectorEngine == USE_AVG_SWARS)
				{
					if (translucency)
						z = (statz * z) / 12;
					else
						z = (statz * z) >> 3;
					if (z > 0xff)
						z = 0xff;
				}
				else
				{
					if (z == 2)
						z = statz;
						if (translucency)
							z = z * BRIGHTNESS;
						else
							if (z) z = (z << 4) | 0x1f;
				}

				deltax = x * scale;
				if (xflip) deltax = -deltax;

				deltay = y * scale;
				currentx += deltax;
				currenty -= deltay;
				vector_timer(deltax, deltay);

				if (sparkle)
				{
					color = rand() & 0x07;
				}

				if ((vectorEngine == USE_AVG_BZONE) && (bz_col != 0))
				{
					if (currenty < (BZONE_TOP<<16))
						color = 4;
					else
						color = 2;
				}

				vector_add_point (currentx, currenty, colorram[color], z);

#ifdef VG_DEBUG
				logerror("VCTR x:%d y:%d z:%d statz:%d", x, y, z, statz);
#endif
				break;

			case SVEC:
				x = twos_comp_val (firstwd, 5) << 1;
				y = twos_comp_val (firstwd >> 8, 5) << 1;
				z = ((firstwd >> 4) & 0x0e);

				if (vectorEngine == USE_AVG_SWARS)
				{
					if (translucency)
						z = (statz * z) / 12;
					else
						z = (statz * z) >> 3;
					if (z > 0xff) z = 0xff;
				}
				else
				{
					if (z == 2)
						z = statz;
						if (translucency)
							z = z * BRIGHTNESS;
						else
							if (z) z = (z << 4) | 0x1f;
				}

				deltax = x * scale;
				if (xflip) deltax = -deltax;

				deltay = y * scale;
				currentx += deltax;
				currenty -= deltay;
				vector_timer(deltax, deltay);

				if (sparkle)
				{
					color = rand() & 0x07;
				}

				vector_add_point (currentx, currenty, colorram[color], z);

#ifdef VG_DEBUG
				logerror("SVEC x:%d y:%d z:%d statz:%d", x, y, z, statz);
#endif
				break;

			case STAT:
				if (vectorEngine == USE_AVG_SWARS)
				{
					/* color code 0-7 stored in top 3 bits of `color' */
					color=(char)((firstwd & 0x0700)>>8);
					statz = (firstwd) & 0xff;
				}
				else
				{
					color = (firstwd) & 0x000f;
					statz = (firstwd >> 4) & 0x000f;
					if (vectorEngine == USE_AVG_TEMPEST)
								sparkle = !(firstwd & 0x0800);
					if (vectorEngine == USE_AVG_MHAVOC)
					{
						sparkle = (firstwd & 0x0800);
						xflip = firstwd & 0x0400;
						/* Bank switch the vector ROM for Major Havoc */
						vectorbank[1] = &memory_region(REGION_CPU1)[0x18000 + ((firstwd & 0x300) >> 8) * 0x2000];
					}
					if (vectorEngine == USE_AVG_BZONE)
					{
						bz_col = color;
						if (color == 0)
						{
							BZONE_CLIP;
							color = 2;
						}
						else
						{
							BZONE_NOCLIP;
						}
					}
				}
#ifdef VG_DEBUG
				logerror("STAT: statz: %d color: %d", statz, color);
				if (xflip || sparkle)
					logerror("xflip: %02x  sparkle: %02x\n", xflip, sparkle);
#endif

				break;

			case SCAL:
				b = ((firstwd >> 8) & 0x07)+8;
				l = (~firstwd) & 0xff;
				scale = (l << VEC_SHIFT) >> b;		/* ASG 080497 */

				/* Y-Window toggle for Major Havoc BW 980318 */
				if (vectorEngine == USE_AVG_MHAVOC)
				{
					if (firstwd & 0x0800)
					{
						//logerror("CLIP %d\n", firstwd & 0x0800);
						if (ywindow == 0)
						{
							ywindow = 1;
							MHAVOC_CLIP;
						}
						else
						{
							ywindow = 0;
							MHAVOC_NOCLIP;
						}
					}
				}
#ifdef VG_DEBUG
				logerror("bin: %d, lin: ", b);
				if (l > 0x80)
					logerror("(%d?)", l);
				else
					logerror("%d", l);
				logerror(" scale: %f", (scale/(float)(1<<VEC_SHIFT)));
#endif
				break;

			case CNTR:
				d = firstwd & 0xff;
#ifdef VG_DEBUG
				if (d != 0x40) logerror("%d", d);
#endif
				currentx = xcenter ;  /* ASG 080497 */ /*.ac JAN2498 */
				currenty = ycenter ;  /* ASG 080497 */ /*.ac JAN2498 */
				vector_add_point (currentx, currenty, 0, 0);
				break;

			case RTSL:
#ifdef VG_DEBUG
				if ((firstwd & 0x1fff) != 0)
					logerror("(%d?)", firstwd & 0x1fff);
#endif
				if (sp == 0)
				{
					//logerror("\n*** Vector generator stack underflow! ***\n");
					done = 1;
					sp = MAXSTACK - 1;
				}
				else
					sp--;

				pc = stack [sp];
				break;

			case HALT:
#ifdef VG_DEBUG
				if ((firstwd & 0x1fff) != 0)
					logerror("(%d?)", firstwd & 0x1fff);
#endif
				done = 1;
				break;

			case JMPL:
				a = firstwd & 0x1fff;
#ifdef VG_DEBUG
				logerror("%4x", map_addr(a));
#endif
				/* if a = 0x0000, treat as HALT */
				if (a == 0x0000)
					done = 1;
				else
					pc = a;
				break;

			case JSRL:
				a = firstwd & 0x1fff;
#ifdef VG_DEBUG
				logerror("%4x", map_addr(a));
#endif
				/* if a = 0x0000, treat as HALT */
				if (a == 0x0000)
					done = 1;
				else
				{
					stack [sp] = pc;
					if (sp == (MAXSTACK - 1))
					{
						//logerror("\n*** Vector generator stack overflow! ***\n");
						done = 1;
						sp = 0;
					}
					else
						sp++;

					pc = a;
				}
				break;

			default:
				//logerror("internal error\n");
				break;
		}
#ifdef VG_DEBUG
		logerror("\n");
#endif
	}
}


int avgdvg_done (void)
{
	if (busy)
		return 0;
	else
		return 1;
}

static void avgdvg_clr_busy (int dummy)
{
	busy = 0;
}

WRITE_HANDLER( avgdvg_go_w )
{

	if (busy)
		return;

	vector_updates++;
	total_length = 1;
	busy = 1;

	if (vectorEngine == USE_DVG)
	{
		dvg_generate_vector_list();
		timer_set (TIME_IN_NSEC(4500) * total_length, 1, avgdvg_clr_busy);
	}
	else
	{
		avg_generate_vector_list();
		if (total_length > 1)
			timer_set (TIME_IN_NSEC(1500) * total_length, 1, avgdvg_clr_busy);
		/* this is for Major Havoc */
		else
		{
			vector_updates--;
			busy = 0;
		}
	}
}

WRITE_HANDLER( avgdvg_reset_w )
{
	avgdvg_clr_busy (0);
}

int avgdvg_init (int vgType)
{
	int i;

	if (vectorram_size == 0)
	{
		//logerror("Error: vectorram_size not initialized\n");
		return 1;
	}

	/* ASG 971210 -- initialize the pages */
	for (i = 0; i < NUM_BANKS; i++)
		vectorbank[i] = vectorram + (i<<BANK_BITS);
	if (vgType == USE_AVG_MHAVOC)
		vectorbank[1] = &memory_region(REGION_CPU1)[0x18000];

	vectorEngine = vgType;
	if ((vectorEngine<AVGDVG_MIN) || (vectorEngine>AVGDVG_MAX))
	{
		//logerror("Error: unknown Atari Vector Game Type\n");
		return 1;
	}

	if (vectorEngine==USE_AVG_SWARS)
		flipword=1;
#ifndef LSB_FIRST
	else if (vectorEngine==USE_AVG_QUANTUM)
		flipword=1;
#endif
	else
		flipword=0;

	vg_step = 0;

	busy = 0;

	xmin=Machine->visible_area.min_x;
	ymin=Machine->visible_area.min_y;
	xmax=Machine->visible_area.max_x;
	ymax=Machine->visible_area.max_y;
	width=xmax-xmin;
	height=ymax-ymin;

	xcenter=((xmax+xmin)/2) << VEC_SHIFT; /*.ac JAN2498 */
	ycenter=((ymax+ymin)/2) << VEC_SHIFT; /*.ac JAN2498 */

	vector_set_shift (VEC_SHIFT);

	if (vector_vh_start())
		return 1;

	return 0;
}

/*
 * These functions initialise the colors for all atari games.
 */

#define RED   0x04
#define GREEN 0x02
#define BLUE  0x01
#define WHITE RED|GREEN|BLUE

static void shade_fill (unsigned char *palette, int rgb, int start_index, int end_index, int start_inten, int end_inten)
{
	int i, inten, index_range, inten_range;

	index_range = end_index-start_index;
	inten_range = end_inten-start_inten;
	for (i = start_index; i <= end_index; i++)
	{
		inten = start_inten + (inten_range) * (i-start_index) / (index_range);
		palette[3*i  ] = (rgb & RED  )? inten : 0;
		palette[3*i+1] = (rgb & GREEN)? inten : 0;
		palette[3*i+2] = (rgb & BLUE )? inten : 0;
	}
}

#define VEC_PAL_WHITE	1
#define VEC_PAL_AQUA	2
#define VEC_PAL_BZONE	3
#define VEC_PAL_MULTI	4
#define VEC_PAL_SWARS	5
#define VEC_PAL_ASTDELUX	6

/* Helper function to construct the color palette for the Atari vector
 * games. DO NOT reference this function from the Gamedriver or
 * MachineDriver. Use "avg_init_palette_XXXXX" instead. */
void avg_init_palette (int paltype, unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	int i,j,k;

	int trcl1[] = { 0,0,2,2,1,1 };
	int trcl2[] = { 1,2,0,1,0,2 };
	int trcl3[] = { 2,1,1,0,2,0 };

	/* initialize the first 8 colors with the basic colors */
	/* Only these are selected by writes to the colorram. */
	for (i = 0; i < 8; i++)
	{
		palette[3*i  ] = (i & RED  ) ? 0xff : 0;
		palette[3*i+1] = (i & GREEN) ? 0xff : 0;
		palette[3*i+2] = (i & BLUE ) ? 0xff : 0;
	}

	/* initialize the colorram */
	for (i = 0; i < 16; i++)
		colorram[i] = i & 0x07;

	/* fill the rest of the 256 color entries depending on the game */
	switch (paltype)
	{
		/* Black and White vector colors (Asteroids,Omega Race) .ac JAN2498 */
		case  VEC_PAL_WHITE:
			shade_fill (palette, RED|GREEN|BLUE, 8, 128+8, 0, 255);
			colorram[1] = 7; /* BW games use only color 1 (== white) */
			break;

		/* Monochrome Aqua colors (Asteroids Deluxe,Red Baron) .ac JAN2498 */
		case  VEC_PAL_ASTDELUX:
			/* Use backdrop if present MLR OCT0598 */
			backdrop_load("astdelux.png", 32, Machine->drv->total_colors-32);
			if (artwork_backdrop!=NULL)
			{
				shade_fill (palette, GREEN|BLUE, 8, 23, 1, 254);
				/* Some more anti-aliasing colors. */
				shade_fill (palette, GREEN|BLUE, 24, 31, 1, 254);
				for (i=0; i<8; i++)
					palette[(24+i)*3]=80;
				memcpy (palette+3*artwork_backdrop->start_pen, artwork_backdrop->orig_palette,
					3*artwork_backdrop->num_pens_used);
			}
			else
				shade_fill (palette, GREEN|BLUE, 8, 128+8, 1, 254);
			colorram[1] =  3; /* for Asteroids */
			break;

		case  VEC_PAL_AQUA:
			shade_fill (palette, GREEN|BLUE, 8, 128+8, 1, 254);
			colorram[0] =  3; /* for Red Baron */
			break;

		/* Monochrome Green/Red vector colors (Battlezone) .ac JAN2498 */
		case  VEC_PAL_BZONE:
			shade_fill (palette, RED  ,  8, 23, 1, 254);
			shade_fill (palette, GREEN, 24, 31, 1, 254);
			shade_fill (palette, WHITE, 32, 47, 1, 254);
			/* Use backdrop if present MLR OCT0598 */
			backdrop_load("bzone.png", 48, Machine->drv->total_colors-48);
			if (artwork_backdrop!=NULL)
				memcpy (palette+3*artwork_backdrop->start_pen, artwork_backdrop->orig_palette, 3*artwork_backdrop->num_pens_used);
			break;

		/* Colored games (Major Havoc, Star Wars, Tempest) .ac JAN2498 */
		case  VEC_PAL_MULTI:
		case  VEC_PAL_SWARS:
			/* put in 40 shades for red, blue and magenta */
			shade_fill (palette, RED       ,   8,  47, 10, 250);
			shade_fill (palette, BLUE      ,  48,  87, 10, 250);
			shade_fill (palette, RED|BLUE  ,  88, 127, 10, 250);

			/* put in 20 shades for yellow and green */
			shade_fill (palette, GREEN     , 128, 147, 10, 250);
			shade_fill (palette, RED|GREEN , 148, 167, 10, 250);

			/* and 14 shades for cyan and white */
			shade_fill (palette, BLUE|GREEN, 168, 181, 10, 250);
			shade_fill (palette, WHITE     , 182, 194, 10, 250);

			/* Fill in unused gaps with more anti-aliasing colors. */
			/* There are 60 slots available.           .ac JAN2498 */
			i=195;
			for (j=0; j<6; j++)
			{
				for (k=7; k<=16; k++)
				{
					palette[3*i+trcl1[j]] = ((256*k)/16)-1;
					palette[3*i+trcl2[j]] = ((128*k)/16)-1;
					palette[3*i+trcl3[j]] = 0;
					i++;
				}
			}
			break;
		default:
			//logerror("Wrong palette type in avgdvg.c");
			break;
	}
}

/* A macro for the palette_init functions */
#define VEC_PAL_INIT(name, paltype) \
void avg_init_palette_##name (unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom) \
{ avg_init_palette (paltype, palette, colortable, color_prom); }

/* The functions referenced from gamedriver */
VEC_PAL_INIT(white,    VEC_PAL_WHITE)
VEC_PAL_INIT(aqua ,    VEC_PAL_AQUA )
VEC_PAL_INIT(bzone,    VEC_PAL_BZONE)
VEC_PAL_INIT(multi,    VEC_PAL_MULTI)
VEC_PAL_INIT(swars,    VEC_PAL_SWARS)
VEC_PAL_INIT(astdelux, VEC_PAL_ASTDELUX )


/* If you want to use the next two functions, please make sure that you have
 * a fake GfxLayout, otherwise you'll crash */
static WRITE_HANDLER( colorram_w )
{
	colorram[offset & 0x0f] = data & 0x0f;
}

/*
 * Tempest, Major Havoc and Quantum select colors via a 16 byte colorram.
 * What's more, they have a different ordering of the rgbi bits than the other
 * color avg games.
 * We need translation tables.
 */

WRITE_HANDLER( tempest_colorram_w )
{
#if 0 /* with low intensity bit */
	int trans[]= { 7, 15, 3, 11, 6, 14, 2, 10, 5, 13, 1,  9, 4, 12, 0,  8 };
#else /* high intensity */
	int trans[]= { 7,  7, 3,  3, 6,  6, 2,  2, 5,  5, 1,  1, 4,  4, 0,  0 };
#endif
	colorram_w (offset, trans[data & 0x0f]);
}

WRITE_HANDLER( mhavoc_colorram_w )
{
#if 0 /* with low intensity bit */
	int trans[]= { 7, 6, 5, 4, 15, 14, 13, 12, 3, 2, 1, 0, 11, 10, 9, 8 };
#else /* high intensity */
	int trans[]= { 7, 6, 5, 4,  7,  6,  5,  4, 3, 2, 1, 0,  3,  2, 1, 0 };
#endif
	//logerror("colorram: %02x: %02x\n", offset, data);
	colorram_w (offset , trans[data & 0x0f]);
}


WRITE_HANDLER( quantum_colorram_w )
{
/* Notes on colors:
offset:				color:			color (game):
0 - score, some text		0 - black?
1 - nothing?			1 - blue
2 - nothing?			2 - green
3 - Quantum, streaks		3 - cyan
4 - text/part 1 player		4 - red
5 - part 2 of player		5 - purple
6 - nothing?			6 - yellow
7 - part 3 of player		7 - white
8 - stars			8 - black
9 - nothing?			9 - blue
10 - nothing?			10 - green
11 - some text, 1up, like 3	11 - cyan
12 - some text, like 4
13 - nothing?			13 - purple
14 - nothing?
15 - nothing?

1up should be blue
score should be red
high score - white? yellow?
level # - green
*/

	int trans[]= { 7/*white*/, 0, 3, 1/*blue*/, 2/*green*/, 5, 6, 4/*red*/,
		       7/*white*/, 0, 3, 1/*blue*/, 2/*green*/, 5, 6, 4/*red*/};

	colorram_w (offset >> 1, trans[data & 0x0f]);
}

/***************************************************************************

  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function, it will be called by
  the main emulation engine.

***************************************************************************/

int dvg_start(void)
{
	if (artwork_backdrop)
	{
		backdrop_refresh(artwork_backdrop);
		backdrop_refresh_tables (artwork_backdrop);
	}

	return avgdvg_init (USE_DVG);
}

int avg_start(void)
{
	return avgdvg_init (USE_AVG);
}

int avg_start_starwars(void)
{
	return avgdvg_init (USE_AVG_SWARS);
}

int avg_start_tempest(void)
{
	return avgdvg_init (USE_AVG_TEMPEST);
}

int avg_start_mhavoc(void)
{
	return avgdvg_init (USE_AVG_MHAVOC);
}

int avg_start_bzone(void)
{
	if (artwork_backdrop)
	{
		backdrop_refresh(artwork_backdrop);
		backdrop_refresh_tables (artwork_backdrop);
	}

	return avgdvg_init (USE_AVG_BZONE);
}

int avg_start_quantum(void)
{
	return avgdvg_init (USE_AVG_QUANTUM);
}

int avg_start_redbaron(void)
{
	return avgdvg_init (USE_AVG_RBARON);
}

void avg_stop(void)
{
	busy = 0;
	vector_clear_list();

	vector_vh_stop();
}

void dvg_stop(void)
{
	busy = 0;
	vector_clear_list();

	vector_vh_stop();
}


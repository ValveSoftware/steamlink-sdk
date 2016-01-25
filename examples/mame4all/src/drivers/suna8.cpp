#include "../vidhrdw/suna8.cpp"

/***************************************************************************

							-=  SunA 8 Bit Games =-

					driver by	Luca Elia (l.elia@tin.it)


Main  CPU:		Encrypted Z80 (Epoxy Module)
Sound CPU:		Z80 [Music]  +  Z80 [4 Bit PCM, Optional]
Sound Chips:	AY8910  +  YM3812/YM2203  + DAC x 4 [Optional]


---------------------------------------------------------------------------
Year + Game         Game     PCB         Epoxy CPU    Notes
---------------------------------------------------------------------------
88  Hard Head       KRB-14   60138-0083  S562008      Encryption + Protection
88  Rough Ranger	K030087  ?           S562008
90  Star Fighter    ?        ?           ?            Not Working
91  Hard Head 2     ?        ?           T568009      Not Working
92  Brick Zone      ?        ?           Yes          Not Working
---------------------------------------------------------------------------

To Do:

- Pen marking
- Samples playing in hardhead, rranger, starfigh (AY8910 ports A&B?)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

extern int suna8_text_dim; /* specifies format of text layer */

extern data8_t suna8_rombank, suna8_spritebank, suna8_palettebank;
extern data8_t suna8_unknown;

/* Functions defined in vidhrdw: */

WRITE_HANDLER( suna8_spriteram_w );			// for debug
WRITE_HANDLER( suna8_banked_spriteram_w );	// for debug

READ_HANDLER( suna8_banked_paletteram_r );
READ_HANDLER( suna8_banked_spriteram_r );

WRITE_HANDLER( suna8_banked_paletteram_w );
WRITE_HANDLER( brickzn_banked_paletteram_w );

int  suna8_vh_start_textdim0(void);
int  suna8_vh_start_textdim8(void);
int  suna8_vh_start_textdim12(void);
void suna8_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);


/***************************************************************************


								ROMs Decryption


***************************************************************************/
static void rom_decode(void)
{
	/* invert the graphics bits on the playfield and motion objects */
	for (int i = 0; i < memory_region_length(REGION_GFX1); i++)
		memory_region(REGION_GFX1)[i] ^= 0xff;
}

/***************************************************************************
								Hard Head
***************************************************************************/

static void init_hardhead(void)
{
	data8_t *RAM = memory_region(REGION_CPU1);
	int i;

	for (i = 0; i < 0x8000; i++)
	{
		data8_t x = RAM[i];
		if (!   ( (i & 0x800) && ((~i & 0x400) ^ ((i & 0x4000)>>4)) )	)
		{
			x	=	x ^ 0x58;
			x	=	(((x & (1<<0))?1:0)<<0) |
					(((x & (1<<1))?1:0)<<1) |
					(((x & (1<<2))?1:0)<<2) |
					(((x & (1<<4))?1:0)<<3) |
					(((x & (1<<3))?1:0)<<4) |
					(((x & (1<<5))?1:0)<<5) |
					(((x & (1<<6))?1:0)<<6) |
					(((x & (1<<7))?1:0)<<7);
		}
		RAM[i] = x;
	}
	rom_decode();
}

/* Non encrypted bootleg */
static void init_hardhedb( void )
{
	/* patch ROM checksum (ROM1 fails self test) */
	memory_region( REGION_CPU1 )[0x1e5b] = 0xAF;
	rom_decode();
}

/***************************************************************************
								Brick Zone
***************************************************************************/

/* !! BRICKZN3 !! */

static int is_special(int i)
{
	if (i & 0x400)
	{
		switch ( i & 0xf )
		{
			case 0x1:
			case 0x5:	return 1;
			default:	return 0;
		}
	}
	else
	{
		switch ( i & 0xf )
		{
			case 0x3:
			case 0x7:
			case 0x8:
			case 0xc:	return 1;
			default:	return 0;
		}
	}
}

void init_brickzn3(void)
{
	data8_t	*RAM	=	memory_region(REGION_CPU1);
	size_t	size	=	memory_region_length(REGION_CPU1)/2;
	int i;

	memory_set_opcode_base(0,RAM + size);

	/* Opcodes */

	for (i = 0; i < 0x50000; i++)
	{
		int encry;
		data8_t x = RAM[i];

		data8_t mask = 0x90;

	if (i >= 0x8000)
	{
		switch ( i & 0xf )
		{
//825b  ->  see 715a!
//8280  ->  see 7192!
//8280:	e=2 m=90
//8281:	e=2 m=90
//8283:	e=2 m=90
//8250:	e=0
//8262:	e=0
//9a42:	e=0
//9a43:	e=0
//8253:	e=0
			case 0x0:
			case 0x1:
			case 0x2:
			case 0x3:
				if (i & 0x40)	encry = 0;
				else			encry = 2;
				break;
//828c:	e=0
//9a3d:	e=0
//825e:	e=0
//826e:	e=0
//9a3f:	e=0
			case 0xc:
			case 0xd:
			case 0xe:
			case 0xf:
				encry = 0;
				break;
//8264:	e=2 m=90
//9a44:	e=2 m=90
//8255:	e=2 m=90
//8255:	e=2 m=90
//8285:	e=2 m=90
//9a37:	e=2 m=90
//8268:	e=2 m=90
//9a3a:	e=2 m=90
//825b:	e=2 m=90
			case 0x4:
			case 0x5:
			case 0x6:
			case 0x7:
			case 0x8:
			case 0xa:
			case 0xb:
			default:
				encry = 2;
		}
	}
	else
	if (	((i >= 0x0730) && (i <= 0x076f)) ||
			((i >= 0x4540) && (i <= 0x455f)) ||
			((i >= 0x79d9) && (i <= 0x7a09)) ||
			((i >= 0x72f3) && (i <= 0x7320))	)
	{
		if ( !is_special(i) )
		{
			mask = 0x10;
			encry = 1;
		}
		else
			encry = 0;
	}
	else
	{

		switch ( i & 0xf )
		{
//0000: e=1 m=90
//0001: e=1 m=90
//0012: e=1 m=90

//00c0: e=1 m=10
//0041: e=1 m=10
//0042: e=1 m=10
//0342: e=1 m=10

//05a0: e=1 m=90
//04a1: e=2 m=90
//04b1: e=2 m=90
//05a1: e=2 m=90
//05a2: e=1 m=90

//0560: e=1 m=10
//0441: e=0
//0571: e=0
//0562: e=1 m=10
			case 0x1:
				switch( i & 0x440 )
				{
					case 0x000:	encry = 1;	mask = 0x90;	break;
					case 0x040:	encry = 1;	mask = 0x10;	break;
					case 0x400:	encry = 2;	mask = 0x90;	break;
					default:
					case 0x440:	encry = 0;					break;
				}
				break;

			case 0x0:
			case 0x2:
				switch( i & 0x440 )
				{
					case 0x000:	encry = 1;	mask = 0x90;	break;
					case 0x040:	encry = 1;	mask = 0x10;	break;
					case 0x400:	encry = 1;	mask = 0x90;	break;
					default:
					case 0x440:	encry = 1;	mask = 0x10;	break;
				}
				break;

			case 0x3:
//003: e=2 m=90
//043: e=0
//6a3: e=2 m=90
//643: e=1 m=10
//5d3: e=1 m=10
				switch( i & 0x440 )
				{
					case 0x000:	encry = 2;	mask = 0x90;	break;
					case 0x040:	encry = 0;					break;
					case 0x400:	encry = 1;	mask = 0x90;	break;
					default:
					case 0x440:	encry = 1;	mask = 0x10;	break;
				}
				break;

			case 0x5:
//015: e=1 m=90
//045: e=1 m=90
//5b5: e=2 m=90
//5d5: e=2 m=90
				if (i & 0x400)	encry = 2;
				else			encry = 1;
				break;


			case 0x7:
			case 0x8:
				if (i & 0x400)	{	encry = 1;	mask = 0x90;	}
				else			{	encry = 2;	}
				break;

			case 0xc:
				if (i & 0x400)	{	encry = 1;	mask = 0x10;	}
				else			{	encry = 0;	}
				break;

			case 0xd:
			case 0xe:
			case 0xf:
				mask = 0x10;
				encry = 1;
				break;

			default:
				encry = 1;
		}
	}
		switch (encry)
		{
			case 1:
				x	^=	mask;
				x	=	(((x & (1<<1))?1:0)<<0) |
						(((x & (1<<0))?1:0)<<1) |
						(((x & (1<<6))?1:0)<<2) |
						(((x & (1<<5))?1:0)<<3) |
						(((x & (1<<4))?1:0)<<4) |
						(((x & (1<<3))?1:0)<<5) |
						(((x & (1<<2))?1:0)<<6) |
						(((x & (1<<7))?1:0)<<7);
				break;

			case 2:
				x	^=	mask;
				x	=	(((x & (1<<0))?1:0)<<0) |	// swap
						(((x & (1<<1))?1:0)<<1) |
						(((x & (1<<6))?1:0)<<2) |
						(((x & (1<<5))?1:0)<<3) |
						(((x & (1<<4))?1:0)<<4) |
						(((x & (1<<3))?1:0)<<5) |
						(((x & (1<<2))?1:0)<<6) |
						(((x & (1<<7))?1:0)<<7);
				break;
		}

		RAM[i + size] = x;
	}


	/* Data */

	for (i = 0; i < 0x8000; i++)
	{
		data8_t x = RAM[i];

		if ( !is_special(i) )
		{
			x	^=	0x10;
			x	=	(((x & (1<<1))?1:0)<<0) |
					(((x & (1<<0))?1:0)<<1) |
					(((x & (1<<6))?1:0)<<2) |
					(((x & (1<<5))?1:0)<<3) |
					(((x & (1<<4))?1:0)<<4) |
					(((x & (1<<3))?1:0)<<5) |
					(((x & (1<<2))?1:0)<<6) |
					(((x & (1<<7))?1:0)<<7);
		}

		RAM[i] = x;
	}


/* !!!!!! PATCHES !!!!!! */

RAM[0x3337+size] = 0xc9;	// RET Z -> RET (to avoid: jp $C800)
//RAM[0x3338+size] = 0x00;	// jp $C800 -> NOP
//RAM[0x3339+size] = 0x00;	// jp $C800 -> NOP
//RAM[0x333a+size] = 0x00;	// jp $C800 -> NOP

RAM[0x1406+size] = 0x00;	// HALT -> NOP (NMI source??)
RAM[0x2487+size] = 0x00;	// HALT -> NOP
RAM[0x256c+size] = 0x00;	// HALT -> NOP

	rom_decode();
}



/***************************************************************************
								Hard Head 2
***************************************************************************/

INLINE data8_t hardhea2_decrypt(data8_t x, int encry, int mask)
{
		switch( encry )
		{
		case 1:
			x	^=	mask;
			return	(((x & (1<<0))?1:0)<<0) |
					(((x & (1<<1))?1:0)<<1) |
					(((x & (1<<2))?1:0)<<2) |
					(((x & (1<<4))?1:0)<<3) |
					(((x & (1<<3))?1:0)<<4) |
					(((x & (1<<7))?1:0)<<5) |
					(((x & (1<<6))?1:0)<<6) |
					(((x & (1<<5))?1:0)<<7);
		case 2:
			x	^=	mask;
			return	(((x & (1<<0))?1:0)<<0) |
					(((x & (1<<1))?1:0)<<1) |
					(((x & (1<<2))?1:0)<<2) |
					(((x & (1<<3))?1:0)<<3) |	// swap
					(((x & (1<<4))?1:0)<<4) |
					(((x & (1<<7))?1:0)<<5) |
					(((x & (1<<6))?1:0)<<6) |
					(((x & (1<<5))?1:0)<<7);
		case 3:
			x	^=	mask;
			return	(((x & (1<<0))?1:0)<<0) |
					(((x & (1<<1))?1:0)<<1) |
					(((x & (1<<2))?1:0)<<2) |
					(((x & (1<<4))?1:0)<<3) |
					(((x & (1<<3))?1:0)<<4) |
					(((x & (1<<5))?1:0)<<5) |
					(((x & (1<<6))?1:0)<<6) |
					(((x & (1<<7))?1:0)<<7);
		case 0:
		default:
			return x;
		}
}

static void init_hardhea2(void)
{
	data8_t	*RAM	=	memory_region(REGION_CPU1);
	size_t	size	=	memory_region_length(REGION_CPU1)/2;
	data8_t x;
	int i,encry,mask;

	memory_set_opcode_base(0,RAM + size);

	/* Opcodes */

	for (i = 0; i < 0x8000; i++)
	{
// Address lines scrambling
		switch (i & 0x7000)
		{
		case 0x4000:
		case 0x5000:
			break;
		default:
			if ((i & 0xc0) == 0x40)
			{
				int j = (i & ~0xc0) | 0x80;
				x		=	RAM[j];
				RAM[j]	=	RAM[i];
				RAM[i]	=	x;
			}
		}

		x		=	RAM[i];

		switch (i & 0x7000)
		{
		case 0x0000:
		case 0x6000:
			encry	=	1;
			switch ( i & 0x401 )
			{
			case 0x400:	mask = 0x41;	break;
			default:
			case 0x401:	mask = 0x45;
			}
			break;

		case 0x2000:
		case 0x4000:
			switch ( i & 0x401 )
			{
			case 0x000:	mask = 0x45;	encry = 1;	break;
			case 0x001:	mask = 0x04;	encry = 1;	break;
			case 0x400:	mask = 0x41;	encry = 3;	break;
			default:
			case 0x401:	mask = 0x45;	encry = 1;	break;
			}
			break;

		case 0x7000:
			switch ( i & 0x401 )
			{
			case 0x001:	mask = 0x45;	encry = 1;	break;
			default:
			case 0x000:
			case 0x400:
			case 0x401:	mask = 0x41;	encry = 3;	break;
			}
			break;

		case 0x1000:
		case 0x3000:
		case 0x5000:
			encry	=	1;
			switch ( i & 0x401 )
			{
			case 0x000:	mask = 0x41;	break;
			case 0x001:	mask = 0x45;	break;
			case 0x400:	mask = 0x41;	break;
			default:
			case 0x401:	mask = 0x41;
			}
			break;

		default:
			mask = 0x41;
			encry = 1;
		}

		RAM[i+size] = hardhea2_decrypt(x,encry,mask);
	}


	/* Data */

	for (i = 0; i < 0x8000; i++)
	{
		x		=	RAM[i];
		mask	=	0x41;
		switch (i & 0x7000)
		{
		case 0x2000:
		case 0x4000:
		case 0x7000:
			encry	=	0;
			break;
		default:
			encry	=	2;
		}

		RAM[i] = hardhea2_decrypt(x,encry,mask);
	}

	for (i = 0x00000; i < 0x40000; i++)
	{
// Address lines scrambling
		switch (i & 0x3f000)
		{
/*
0x1000 to scramble:
		dump				screen
rom10:	0y, 1y, 2n, 3n		0y,1y,2n,3n
		4n?,5n, 6n, 7n		4n,5n,6n,7n
		8?, 9n, an, bn		8n,9n,an,bn
		cy, dy, ey?,		cy,dy,en,fn
rom11:						n
rom12:						n
rom13:	0?, 1y, 2n, 3n		?,?,?,? (palettes)
		4n, 5n, 6n, 7?		?,?,n,n (intro anim)
		8?, 9n?,an, bn		?,?,?,?
		cn, dy, en, fn		y,y,n,n
*/
		case 0x00000:
		case 0x01000:
		case 0x0c000:
		case 0x0d000:

		case 0x30000:
		case 0x31000:
		case 0x3c000:
		case 0x3d000:
			if ((i & 0xc0) == 0x40)
			{
				int j = (i & ~0xc0) | 0x80;
				x				=	RAM[j+0x10000];
				RAM[j+0x10000]	=	RAM[i+0x10000];
				RAM[i+0x10000]	=	x;
			}
		}
	}

	rom_decode();
}


/***************************************************************************
								Star Fighter
***************************************************************************/

/* SAME AS HARDHEA2 */
INLINE data8_t starfigh_decrypt(data8_t x, int encry, int mask)
{
		switch( encry )
		{
		case 1:
			x	^=	mask;
			return	(((x & (1<<0))?1:0)<<0) |
					(((x & (1<<1))?1:0)<<1) |
					(((x & (1<<2))?1:0)<<2) |
					(((x & (1<<4))?1:0)<<3) |
					(((x & (1<<3))?1:0)<<4) |
					(((x & (1<<7))?1:0)<<5) |
					(((x & (1<<6))?1:0)<<6) |
					(((x & (1<<5))?1:0)<<7);
		case 2:
			x	^=	mask;
			return	(((x & (1<<0))?1:0)<<0) |
					(((x & (1<<1))?1:0)<<1) |
					(((x & (1<<2))?1:0)<<2) |
					(((x & (1<<3))?1:0)<<3) |	// swap
					(((x & (1<<4))?1:0)<<4) |
					(((x & (1<<7))?1:0)<<5) |
					(((x & (1<<6))?1:0)<<6) |
					(((x & (1<<5))?1:0)<<7);
		case 3:
			x	^=	mask;
			return	(((x & (1<<0))?1:0)<<0) |
					(((x & (1<<1))?1:0)<<1) |
					(((x & (1<<2))?1:0)<<2) |
					(((x & (1<<4))?1:0)<<3) |
					(((x & (1<<3))?1:0)<<4) |
					(((x & (1<<5))?1:0)<<5) |
					(((x & (1<<6))?1:0)<<6) |
					(((x & (1<<7))?1:0)<<7);
		case 0:
		default:
			return x;
		}
}

void init_rranger(void)
{
	rom_decode();
}

void init_starfigh(void)
{
	data8_t	*RAM	=	memory_region(REGION_CPU1);
	size_t	size	=	memory_region_length(REGION_CPU1)/2;
	data8_t x;
	int i,encry,mask;

	memory_set_opcode_base(0,RAM + size);

	/* Opcodes */

	for (i = 0; i < 0x8000; i++)
	{
// Address lines scrambling
		switch (i & 0x7000)
		{
		case 0x0000:
		case 0x1000:
		case 0x2000:
		case 0x3000:
		case 0x4000:
		case 0x5000:
			if ((i & 0xc0) == 0x40)
			{
				int j = (i & ~0xc0) | 0x80;
				x		=	RAM[j];
				RAM[j]	=	RAM[i];
				RAM[i]	=	x;
			}
			break;
		case 0x6000:
		default:
			break;
		}

		x		=	RAM[i];

		switch (i & 0x7000)
		{
		case 0x2000:
		case 0x4000:
			switch ( i & 0x0c00 )
			{
			case 0x0400:	mask = 0x40;	encry = 3;	break;
			case 0x0800:	mask = 0x04;	encry = 1;	break;
			default:		mask = 0x44;	encry = 1;	break;
			}
			break;

		case 0x0000:
		case 0x1000:
		case 0x3000:
		case 0x5000:
		default:
			mask = 0x45;
			encry = 1;
		}

		RAM[i+size] = starfigh_decrypt(x,encry,mask);
	}


	/* Data */

	for (i = 0; i < 0x8000; i++)
	{
		x		=	RAM[i];

		switch (i & 0x7000)
		{
		case 0x2000:
		case 0x4000:
		case 0x7000:
			encry = 0;
			break;
		case 0x0000:
		case 0x1000:
		case 0x3000:
		case 0x5000:
		case 0x6000:
		default:
			mask = 0x45;
			encry = 2;
		}

		RAM[i] = starfigh_decrypt(x,encry,mask);
	}

	rom_decode();
}


/***************************************************************************


								Protection


***************************************************************************/

/***************************************************************************
								Hard Head
***************************************************************************/

static data8_t protection_val;

static READ_HANDLER( hardhead_protection_r )
{
	if (protection_val & 0x80)
		return	((~offset & 0x20)			?	0x20 : 0) |
				((protection_val & 0x04)	?	0x80 : 0) |
				((protection_val & 0x01)	?	0x04 : 0);
	else
		return	((~offset & 0x20)					?	0x20 : 0) |
				(((offset ^ protection_val) & 0x01)	?	0x84 : 0);
}

static WRITE_HANDLER( hardhead_protection_w )
{
	if (data & 0x80)	protection_val = data;
	else				protection_val = offset & 1;
}


/***************************************************************************


							Memory Maps - Main CPU


***************************************************************************/

/***************************************************************************
								Hard Head
***************************************************************************/

static data8_t *hardhead_ip;

static READ_HANDLER( hardhead_ip_r )
{
	switch (*hardhead_ip)
	{
		case 0:	return readinputport(0);
		case 1:	return readinputport(1);
		case 2:	return readinputport(2);
		case 3:	return readinputport(3);
		default:
			//logerror("CPU #0 - PC %04X: Unknown IP read: %02X\n",cpu_get_pc(),hardhead_ip);
			return 0xff;
	}
}

/*
	765- ----	Unused (eg. they go into hardhead_flipscreen_w)
	---4 ----
	---- 3210	ROM Bank
*/
static WRITE_HANDLER( hardhead_bankswitch_w )
{
	data8_t *RAM = memory_region(REGION_CPU1);
	int bank = data & 0x0f;

	//if (data & ~0xef) 	logerror("CPU #0 - PC %04X: unknown bank bits: %02X\n",cpu_get_pc(),data);

	RAM = &RAM[0x4000 * bank + 0x10000];
	cpu_setbank(1, RAM);
}


/*
	765- ----
	---4 3---	Coin Lockout
	---- -2--	Flip Screen
	---- --10
*/
static WRITE_HANDLER( hardhead_flipscreen_w )
{
	flip_screen_w( 0,   data & 0x04);
	coin_lockout_w ( 0,	data & 0x08);
	coin_lockout_w ( 1,	data & 0x10);
}

static struct MemoryReadAddress  hardhead_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM				},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK1				},	// Banked ROM
	{ 0xc000, 0xd7ff, MRA_RAM				},	// RAM
	{ 0xd800, 0xd9ff, MRA_RAM				},	// Palette
	{ 0xda00, 0xda00, hardhead_ip_r			},	// Input Ports
	{ 0xda80, 0xda80, soundlatch2_r			},	// From Sound CPU
	{ 0xdd80, 0xddff, hardhead_protection_r	},	// Protection
	{ 0xe000, 0xffff, MRA_RAM				},	// Sprites
	{-1}
};

static struct MemoryWriteAddress  hardhead_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM				},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM				},	// Banked ROM
	{ 0xc000, 0xd7ff, MWA_RAM				},	// RAM
	{ 0xd800, 0xd9ff, paletteram_RRRRGGGGBBBBxxxx_swap_w, &paletteram	},	// Palette
	{ 0xda00, 0xda00, MWA_RAM, &hardhead_ip	},	// Input Port Select
	{ 0xda80, 0xda80, hardhead_bankswitch_w	},	// ROM Banking
	{ 0xdb00, 0xdb00, soundlatch_w			},	// To Sound CPU
	{ 0xdb80, 0xdb80, hardhead_flipscreen_w	},	// Flip Screen + Coin Lockout
	{ 0xdc00, 0xdc00, MWA_NOP				},	// <- R	(after bank select)
	{ 0xdc80, 0xdc80, MWA_NOP				},	// <- R (after bank select)
	{ 0xdd00, 0xdd00, MWA_NOP				},	// <- R (after ip select)
	{ 0xdd80, 0xddff, hardhead_protection_w	},	// Protection
	{ 0xe000, 0xffff, suna8_spriteram_w, &spriteram	},	// Sprites
	{-1}
};

static struct IOReadPort hardhead_readport[] =
{
	{ 0x00, 0x00, IORP_NOP	},	// ? IRQ Ack
	{-1}
};

static struct IOWritePort hardhead_writeport[] =
{
	{-1}
};

/***************************************************************************
								Rough Ranger
***************************************************************************/

/*
	76-- ----	Coin Lockout
	--5- ----	Flip Screen
	---4 ----	ROM Bank
	---- 3---
	---- -210	ROM Bank
*/
static WRITE_HANDLER( rranger_bankswitch_w )
{
	data8_t *RAM = memory_region(REGION_CPU1);
	int bank = data & 0x07;
	if ((~data & 0x10) && (bank >= 4))	bank += 4;

	//if (data & ~0xf7) 	logerror("CPU #0 - PC %04X: unknown bank bits: %02X\n",cpu_get_pc(),data);

	RAM = &RAM[0x4000 * bank + 0x10000];

	cpu_setbank(1, RAM);

	flip_screen_w (0, data & 0x20);
	coin_lockout_w ( 0,	data & 0x40);
	coin_lockout_w ( 1,	data & 0x80);
}

/*
	7--- ----	1 -> Garbled title (another romset?)
	-654 ----
	---- 3---	1 -> No sound (soundlatch full?)
	---- -2--
	---- --1-	1 -> Interlude screens
	---- ---0
*/
static READ_HANDLER( rranger_soundstatus_r )
{
	return 0x02;
}

static struct MemoryReadAddress rranger_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM				},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK1				},	// Banked ROM
	{ 0xc000, 0xc000, watchdog_reset_r		},	// Watchdog (Tested!)
	{ 0xc002, 0xc002, input_port_0_r		},	// P1 (Inputs)
	{ 0xc003, 0xc003, input_port_1_r		},	// P2
	{ 0xc004, 0xc004, rranger_soundstatus_r	},	// Latch Status?
	{ 0xc200, 0xc200, MRA_NOP				},	// Protection?
	{ 0xc280, 0xc280, input_port_2_r		},	// DSW 1
	{ 0xc2c0, 0xc2c0, input_port_3_r		},	// DSW 2
	{ 0xc600, 0xc7ff, MRA_RAM				},	// Palette
	{ 0xc800, 0xdfff, MRA_RAM				},	// RAM
	{ 0xe000, 0xffff, MRA_RAM				},	// Sprites
	{-1}
};

static struct MemoryWriteAddress rranger_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM				},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM				},	// Banked ROM
	{ 0xc000, 0xc000, soundlatch_w			},	// To Sound CPU
	{ 0xc002, 0xc002, rranger_bankswitch_w	},	// ROM Banking
	{ 0xc200, 0xc200, MWA_NOP				},	// Protection?
	{ 0xc280, 0xc280, MWA_NOP				},	// ? NMI Ack
	{ 0xc600, 0xc7ff, paletteram_RRRRGGGGBBBBxxxx_swap_w, &paletteram	},	// Palette
	{ 0xc800, 0xdfff, MWA_RAM				},	// RAM
	{ 0xe000, 0xffff, suna8_spriteram_w, &spriteram	},	// Sprites
	{-1}
};

static struct IOReadPort  rranger_readport[] =
{
	{ 0x00, 0x00, IORP_NOP	},	// ? IRQ Ack
	{-1}
};

static struct IOWritePort rranger_writeport[] =
{
	{-1}
};

/***************************************************************************
								Brick Zone
***************************************************************************/

/*
?
*/
static READ_HANDLER( brickzn_c140_r )
{
	return 0xff;
}

/*
*/
static WRITE_HANDLER( brickzn_palettebank_w )
{
	suna8_palettebank = (data >> 1) & 1;
	//if (data & ~0x02) 	logerror("CPU #0 - PC %04X: unknown palettebank bits: %02X\n",cpu_get_pc(),data);

	/* Also used as soundlatch - depending on c0c0? */
	soundlatch_w(0,data);
}

/*
	7654 32--
	---- --1-	Ram Bank
	---- ---0	Flip Screen
*/
static WRITE_HANDLER( brickzn_spritebank_w )
{
	suna8_spritebank = (data >> 1) & 1;
	//if (data & ~0x03) 	logerror("CPU #0 - PC %04X: unknown spritebank bits: %02X\n",cpu_get_pc(),data);
	flip_screen_w (0, data & 0x01 );
}

static WRITE_HANDLER( brickzn_unknown_w )
{
	suna8_unknown = data;
}

/*
	7654 ----
	---- 3210	ROM Bank
*/
static WRITE_HANDLER( brickzn_rombank_w )
{
	data8_t *RAM = memory_region(REGION_CPU1);
	int bank = data & 0x0f;

	//if (data & ~0x0f) 	logerror("CPU #0 - PC %04X: unknown rom bank bits: %02X\n",cpu_get_pc(),data);

	RAM = &RAM[0x4000 * bank + 0x10000];

	cpu_setbank(1, RAM);
	suna8_rombank = data;
}

static struct MemoryReadAddress  brickzn_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK1					},	// Banked ROM
	{ 0xc100, 0xc100, input_port_0_r			},	// P1 (Buttons)
	{ 0xc101, 0xc101, input_port_1_r			},	// P2
	{ 0xc102, 0xc102, input_port_2_r			},	// DSW 1
	{ 0xc103, 0xc103, input_port_3_r			},	// DSW 2
	{ 0xc108, 0xc108, input_port_4_r			},	// P1 (Analog)
	{ 0xc10c, 0xc10c, input_port_5_r			},	// P2
	{ 0xc140, 0xc140, brickzn_c140_r			},	// ???
	{ 0xc600, 0xc7ff, suna8_banked_paletteram_r	},	// Palette (Banked)
	{ 0xc800, 0xdfff, MRA_RAM					},	// RAM
	{ 0xe000, 0xffff, suna8_banked_spriteram_r	},	// Sprites (Banked)
	{-1}
};

static struct MemoryWriteAddress  brickzn_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM						},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM						},	// Banked ROM
	{ 0xc040, 0xc040, brickzn_rombank_w				},	// ROM Bank
	{ 0xc060, 0xc060, brickzn_spritebank_w			},	// Sprite  RAM Bank + Flip Screen
	{ 0xc0a0, 0xc0a0, brickzn_palettebank_w			},	// Palette RAM Bank + ?
	{ 0xc0c0, 0xc0c0, brickzn_unknown_w				},	// ???
	{ 0xc600, 0xc7ff, brickzn_banked_paletteram_w	},	// Palette (Banked)
	{ 0xc800, 0xdfff, MWA_RAM						},	// RAM
	{ 0xe000, 0xffff, suna8_banked_spriteram_w		},	// Sprites (Banked)
	{-1}
};


static struct IOReadPort  brickzn_readport[] =
{
	{-1}
};

static struct IOWritePort brickzn_writeport[] =
{
	{-1}
};

/***************************************************************************
								Hard Head 2
***************************************************************************/

static data8_t suna8_nmi_enable;

/* Probably wrong: */
static WRITE_HANDLER( hardhea2_nmi_w )
{
	suna8_nmi_enable = data & 0x01;
	//if (data & ~0x01) 	logerror("CPU #0 - PC %04X: unknown nmi bits: %02X\n",cpu_get_pc(),data);
}

/*
	7654 321-
	---- ---0	Flip Screen
*/
static WRITE_HANDLER( hardhea2_flipscreen_w )
{
	flip_screen_w(0, data & 0x01);
	//if (data & ~0x01) 	logerror("CPU #0 - PC %04X: unknown flipscreen bits: %02X\n",cpu_get_pc(),data);
}

WRITE_HANDLER( hardhea2_leds_w )
{
	osd_led_w(0, data & 0x01);
	osd_led_w(1, data & 0x02);
	coin_counter_w(0, data & 0x04);
	//if (data & ~0x07)	logerror("CPU#0  - PC %06X: unknown leds bits: %02X\n",cpu_get_pc(),data);
}

/*
	7654 32--
	---- --1-	Ram Bank
	---- ---0	Ram Bank?
*/
static WRITE_HANDLER( hardhea2_spritebank_w )
{
	suna8_spritebank = (data >> 1) & 1;
	//if (data & ~0x02) 	logerror("CPU #0 - PC %04X: unknown spritebank bits: %02X\n",cpu_get_pc(),data);
}

static READ_HANDLER( hardhea2_c080_r )
{
	return 0xff;
}

/*
	7654 ----
	---- 3210	ROM Bank
*/
static WRITE_HANDLER( hardhea2_rombank_w )
{
	data8_t *RAM = memory_region(REGION_CPU1);
	int bank = data & 0x0f;

	//if (data & ~0x0f) 	logerror("CPU #0 - PC %04X: unknown rom bank bits: %02X\n",cpu_get_pc(),data);

	RAM = &RAM[0x4000 * bank + 0x10000];

	cpu_setbank(1, RAM);
	suna8_rombank = data;
}

static struct MemoryReadAddress  hardhea2_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK1					},	// Banked ROM
	{ 0xc000, 0xc000, input_port_0_r			},	// P1 (Inputs)
	{ 0xc001, 0xc001, input_port_1_r			},	// P2
	{ 0xc002, 0xc002, input_port_2_r			},	// DSW 1
	{ 0xc003, 0xc003, input_port_3_r			},	// DSW 2
	{ 0xc080, 0xc080, hardhea2_c080_r			},	// ???
	{ 0xc600, 0xc7ff, paletteram_r				},	// Palette (Banked??)
	{ 0xc800, 0xdfff, MRA_RAM					},	// RAM
	{ 0xe000, 0xffff, suna8_banked_spriteram_r	},	// Sprites (Banked)
	{-1}
};

static struct MemoryWriteAddress  hardhea2_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM					},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM					},	// Banked ROM
	{ 0xc200, 0xc200, hardhea2_spritebank_w		},	// Sprite RAM Bank
	{ 0xc280, 0xc280, hardhea2_rombank_w		},	// ROM Bank (?mirrored up to c2ff?)
	{ 0xc300, 0xc300, hardhea2_flipscreen_w		},	// Flip Screen
	{ 0xc380, 0xc380, hardhea2_nmi_w			},	// ? NMI related ?
	{ 0xc400, 0xc400, hardhea2_leds_w			},	// Leds + Coin Counter
	{ 0xc480, 0xc480, MWA_NOP					},	// ~ROM Bank
	{ 0xc500, 0xc500, soundlatch_w				},	// To Sound CPU
	{ 0xc600, 0xc7ff, paletteram_RRRRGGGGBBBBxxxx_swap_w, &paletteram	},	// Palette (Banked??)
	{ 0xc800, 0xdfff, MWA_RAM					},	// RAM
	{ 0xe000, 0xffff, suna8_banked_spriteram_w	},	// Sprites (Banked)
	{-1}
};

static struct IOReadPort  hardhea2_readport[] =
{
	{-1}
};

static struct IOWritePort hardhea2_writeport[] =
{
	{-1}
};

/***************************************************************************
								Star Fighter
***************************************************************************/

static data8_t spritebank_latch;
static WRITE_HANDLER( starfigh_spritebank_latch_w )
{
	spritebank_latch = (data >> 2) & 1;
	//if (data & ~0x04) 	logerror("CPU #0 - PC %04X: unknown spritebank bits: %02X\n",cpu_get_pc(),data);
}

static WRITE_HANDLER( starfigh_spritebank_w )
{
	suna8_spritebank = spritebank_latch;
}

static struct MemoryReadAddress  starfigh_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0x8000, 0xbfff, MRA_BANK1					},	// Banked ROM
	{ 0xc000, 0xc000, input_port_0_r			},	// P1 (Inputs)
	{ 0xc001, 0xc001, input_port_1_r			},	// P2
	{ 0xc002, 0xc002, input_port_2_r			},	// DSW 1
	{ 0xc003, 0xc003, input_port_3_r			},	// DSW 2
	{ 0xc600, 0xc7ff, suna8_banked_paletteram_r	},	// Palette (Banked??)
	{ 0xc800, 0xdfff, MRA_RAM					},	// RAM
	{ 0xe000, 0xffff, suna8_banked_spriteram_r	},	// Sprites (Banked)
	{-1}
};

static struct MemoryWriteAddress  starfigh_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM						},	// ROM
	{ 0x8000, 0xbfff, MWA_ROM						},	// Banked ROM
	{ 0xc200, 0xc200, starfigh_spritebank_w			},	// Sprite RAM Bank
	{ 0xc380, 0xc3ff, starfigh_spritebank_latch_w	},	// Sprite RAM Bank
	{ 0xc280, 0xc280, hardhea2_rombank_w			},	// ROM Bank (?mirrored up to c2ff?)
	{ 0xc300, 0xc300, hardhea2_flipscreen_w			},	// Flip Screen
	{ 0xc400, 0xc400, hardhea2_leds_w				},	// Leds + Coin Counter
	{ 0xc500, 0xc500, soundlatch_w					},	// To Sound CPU
	{ 0xc600, 0xc7ff, paletteram_RRRRGGGGBBBBxxxx_swap_w, &paletteram	},	// Palette (Banked??)
	{ 0xc800, 0xdfff, MWA_RAM						},	// RAM
	{ 0xe000, 0xffff, suna8_banked_spriteram_w		},	// Sprites (Banked)
	{-1}
};

static struct IOReadPort starfigh_readport[] =
{
	{-1}
};

static struct IOWritePort starfigh_writeport[] =
{
	{-1}
};


/***************************************************************************


							Memory Maps - Sound CPU(s)


***************************************************************************/

/***************************************************************************
								Hard Head
***************************************************************************/

static struct MemoryReadAddress  hardhead_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0xc000, 0xc7ff, MRA_RAM					},	// RAM
	{ 0xc800, 0xc800, YM3812_status_port_0_r 	},	// ? unsure
	{ 0xd800, 0xd800, soundlatch_r				},	// From Main CPU
	{-1}
};

static struct MemoryWriteAddress  hardhead_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM					},	// ROM
	{ 0xc000, 0xc7ff, MWA_RAM					},	// RAM
	{ 0xd000, 0xd000, soundlatch2_w				},	//
	{ 0xa000, 0xa000, YM3812_control_port_0_w	},	// YM3812
	{ 0xa001, 0xa001, YM3812_write_port_0_w		},
	{ 0xa002, 0xa002, AY8910_control_port_0_w	},	// AY8910
	{ 0xa003, 0xa003, AY8910_write_port_0_w		},
	{-1}
};

static struct IOReadPort  hardhead_sound_readport[] =
{
	{ 0x01, 0x01, IORP_NOP	},	// ? IRQ Ack
	{-1}
};

static struct IOWritePort hardhead_sound_writeport[] =
{
	{-1}
};


/***************************************************************************
								Rough Ranger
***************************************************************************/

static struct MemoryReadAddress  rranger_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0xc000, 0xc7ff, MRA_RAM					},	// RAM
	{ 0xd800, 0xd800, soundlatch_r				},	// From Main CPU
	{-1}
};

static struct MemoryWriteAddress  rranger_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM					},	// ROM
	{ 0xc000, 0xc7ff, MWA_RAM					},	// RAM
	{ 0xd000, 0xd000, soundlatch2_w				},	//
	{ 0xa000, 0xa000, YM2203_control_port_0_w	},	// YM2203
	{ 0xa001, 0xa001, YM2203_write_port_0_w		},
	{ 0xa002, 0xa002, YM2203_control_port_1_w	},	// AY8910
	{ 0xa003, 0xa003, YM2203_write_port_1_w		},
	{-1}
};

static struct IOReadPort  rranger_sound_readport[] =
{
	{-1}
};

static struct IOWritePort rranger_sound_writeport[] =
{
	{-1}
};


/***************************************************************************
								Brick Zone
***************************************************************************/

static struct MemoryReadAddress  brickzn_sound_readmem[] =
{
	{ 0x0000, 0xbfff, MRA_ROM					},	// ROM
	{ 0xe000, 0xe7ff, MRA_RAM					},	// RAM
	{ 0xf800, 0xf800, soundlatch_r				},	// From Main CPU
	{-1}
};

static struct MemoryWriteAddress  brickzn_sound_writemem[] =
{
	{ 0x0000, 0xbfff, MWA_ROM					},	// ROM
	{ 0xc000, 0xc000, YM3812_control_port_0_w	},	// YM3812
	{ 0xc001, 0xc001, YM3812_write_port_0_w		},
	{ 0xc002, 0xc002, AY8910_control_port_0_w	},	// AY8910
	{ 0xc003, 0xc003, AY8910_write_port_0_w		},
	{ 0xe000, 0xe7ff, MWA_RAM					},	// RAM
	{ 0xf000, 0xf000, soundlatch2_w				},	// To PCM CPU
	{-1}
};

static struct IOReadPort  brickzn_sound_readport[] =
{
	{-1}
};

static struct IOWritePort brickzn_sound_writeport[] =
{
	{-1}
};


/* PCM Z80 , 4 DACs (4 bits per sample), NO RAM !! */

static struct MemoryReadAddress brickzn_pcm_readmem[] =
{
	{ 0x0000, 0xffff, MRA_ROM	},	// ROM
	{-1}
};

static struct MemoryWriteAddress  brickzn_pcm_writemem[] =
{
	{ 0x0000, 0xffff, MWA_ROM	},	// ROM
	{-1}
};

static WRITE_HANDLER( brickzn_pcm_w )
{
	DAC_data_w( offset & 3, (data & 0xf) * 0x11 );
}

static struct IOReadPort  brickzn_pcm_readport[] =
{
	{ 0x00, 0x00, soundlatch2_r		},	// From Sound CPU
	{-1}
};

static struct IOWritePort  brickzn_pcm_writeport[] =
{
	{ 0x00, 0x03, brickzn_pcm_w			},	// 4 x DAC
	{-1}
};


/***************************************************************************


								Input Ports


***************************************************************************/

#define JOY(_n_) \
	PORT_BIT(  0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER##_n_ ) \
	PORT_BIT(  0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER##_n_ ) \
	PORT_BIT(  0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER##_n_ ) \
	PORT_BIT(  0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER##_n_ ) \
	PORT_BIT(  0x10, IP_ACTIVE_LOW, IPT_BUTTON1        | IPF_PLAYER##_n_ ) \
	PORT_BIT(  0x20, IP_ACTIVE_LOW, IPT_BUTTON2        | IPF_PLAYER##_n_ ) \
	PORT_BIT(  0x40, IP_ACTIVE_LOW, IPT_START##_n_ ) \
	PORT_BIT(  0x80, IP_ACTIVE_LOW, IPT_COIN##_n_  )

/***************************************************************************
								Hard Head
***************************************************************************/

INPUT_PORTS_START( hardhead )

	PORT_START	// IN0 - Player 1 - $da00 (ip = 0)
	JOY(1)

	PORT_START	// IN1 - Player 2 - $da00 (ip = 1)
	JOY(2)

	PORT_START	// IN2 - DSW 1 - $da00 (ip = 2)
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x0e, 0x0e, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x0e, "No Bonus" )
	PORT_DIPSETTING(    0x0c, "10K" )
	PORT_DIPSETTING(    0x0a, "20K" )
	PORT_DIPSETTING(    0x08, "50K" )
	PORT_DIPSETTING(    0x06, "50K, Every 50K" )
	PORT_DIPSETTING(    0x04, "100K, Every 50K" )
	PORT_DIPSETTING(    0x02, "100K, Every 100K" )
	PORT_DIPSETTING(    0x00, "200K, Every 100K" )
	PORT_DIPNAME( 0x70, 0x70, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_4C ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	// IN3 - DSW 2 - $da00 (ip = 3)
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Play Together" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x18, "2" )
	PORT_DIPSETTING(    0x10, "3" )
	PORT_DIPSETTING(    0x08, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xe0, "Easiest" )
	PORT_DIPSETTING(    0xc0, "Very Easy" )
	PORT_DIPSETTING(    0xa0, "Easy" )
	PORT_DIPSETTING(    0x80, "Moderate" )
	PORT_DIPSETTING(    0x60, "Normal" )
	PORT_DIPSETTING(    0x40, "Harder" )
	PORT_DIPSETTING(    0x20, "Very Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
INPUT_PORTS_END

/***************************************************************************
								Rough Ranger
***************************************************************************/

INPUT_PORTS_START( rranger )

	PORT_START	// IN0 - Player 1 - $c002
	JOY(1)

	PORT_START	// IN1 - Player 2 - $c003
	JOY(2)

	PORT_START	// IN2 - DSW 1 - $c280
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "10K" )
	PORT_DIPSETTING(    0x28, "30K" )
	PORT_DIPSETTING(    0x20, "50K" )
	PORT_DIPSETTING(    0x18, "50K, Every 50K" )
	PORT_DIPSETTING(    0x10, "100K, Every 50K" )
	PORT_DIPSETTING(    0x08, "100K, Every 100K" )
	PORT_DIPSETTING(    0x00, "100K, Every 200K" )
	PORT_DIPSETTING(    0x38, "None" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xc0, "Normal" )
	PORT_DIPSETTING(    0x80, "Hard" )
	PORT_DIPSETTING(    0x40, "Harder" )
	PORT_DIPSETTING(    0x00, "Hardest" )

	PORT_START	// IN3 - DSW 2 - $c2c0
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Play Together" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x08, 0x08, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x30, "2" )
	PORT_DIPSETTING(    0x20, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BITX(    0x80, 0x80, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

INPUT_PORTS_END


/***************************************************************************
								Brick Zone
***************************************************************************/

INPUT_PORTS_START( brickzn )

	PORT_START	// IN0 - Player 1 - $c100
	JOY(1)

	PORT_START	// IN1 - Player 2 - $c101
	JOY(2)

	PORT_START	// IN2 - DSW 1 - $c102
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )	// rom 38:b840
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x38, "Easiest" )
	PORT_DIPSETTING(    0x30, "Very Easy" )
	PORT_DIPSETTING(    0x28, "Easy" )
	PORT_DIPSETTING(    0x20, "Moderate" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, "Harder" )
	PORT_DIPSETTING(    0x08, "Very Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
//	PORT_BITX(    0x40, 0x40, IPT_DIPSWITCH_NAME | IPF_CHEAT, "Invulnerability", IP_KEY_NONE, IP_JOY_NONE )
//	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
//	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_SERVICE(       0x40, IP_ACTIVE_LOW )	// + Invulnerability
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	// IN3 - DSW 2 - $c103
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Play Together" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "10K" )
	PORT_DIPSETTING(    0x28, "30K" )
	PORT_DIPSETTING(    0x18, "50K, Every 50K" )
	PORT_DIPSETTING(    0x20, "50K" )
	PORT_DIPSETTING(    0x10, "100K, Every 50K" )
	PORT_DIPSETTING(    0x08, "100K, Every 100K" )
	PORT_DIPSETTING(    0x00, "200K, Every 100K" )
	PORT_DIPSETTING(    0x38, "None" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x00, "5" )

	PORT_START	// IN4 - Player 1 - $c108
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_REVERSE, 50, 0, 0, 0)

	PORT_START	// IN5 - Player 2 - $c10c
	PORT_ANALOG( 0xff, 0x00, IPT_TRACKBALL_X | IPF_REVERSE, 50, 0, 0, 0)

INPUT_PORTS_END


/***************************************************************************
						Hard Head 2 / Star Fighter
***************************************************************************/

INPUT_PORTS_START( hardhea2 )

	PORT_START	// IN0 - Player 1 - $c000
	JOY(1)

	PORT_START	// IN1 - Player 2 - $c001
	JOY(2)

	PORT_START	// IN2 - DSW 1 - $c002
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x38, "Easiest" )
	PORT_DIPSETTING(    0x30, "Very Easy" )
	PORT_DIPSETTING(    0x28, "Easy" )
	PORT_DIPSETTING(    0x20, "Moderate" )
	PORT_DIPSETTING(    0x18, "Normal" )
	PORT_DIPSETTING(    0x10, "Harder" )
	PORT_DIPSETTING(    0x08, "Very Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_SERVICE(       0x40, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	// IN3 - DSW 2 - $c003
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x04, 0x04, "Play Together" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "10K" )
	PORT_DIPSETTING(    0x28, "30K" )
	PORT_DIPSETTING(    0x18, "50K, Every 50K" )
	PORT_DIPSETTING(    0x20, "50K" )
	PORT_DIPSETTING(    0x10, "100K, Every 50K" )
	PORT_DIPSETTING(    0x08, "100K, Every 100K" )
	PORT_DIPSETTING(    0x00, "200K, Every 100K" )
	PORT_DIPSETTING(    0x38, "None" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0xc0, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x00, "5" )

INPUT_PORTS_END

/***************************************************************************


								Graphics Layouts


***************************************************************************/

/* 8x8x4 tiles (2 bitplanes per ROM) */
static struct GfxLayout layout_8x8x4 =
{
	8,8,
	RGN_FRAC(1,2),
	4,
	{ RGN_FRAC(1,2) + 0, RGN_FRAC(1,2) + 4, 0, 4 },
	{ 3,2,1,0, 11,10,9,8},
	{ STEP8(0,16) },
	8*8*2
};

static struct GfxDecodeInfo suna8_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_8x8x4, 0, 16 }, // [0] Sprites
	{ -1 }
};



/***************************************************************************


								Machine Drivers


***************************************************************************/

static void soundirq(int state)
{
	cpu_set_irq_line(1, 0, state);
}

/* In games with only 2 CPUs, port A&B of the AY8910 are probably used
   for sample playing. */

/***************************************************************************
								Hard Head
***************************************************************************/

/* 1 x 24 MHz crystal */

static struct AY8910interface hardhead_ay8910_interface =
{
	1,
	4000000,	/* ? */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3812interface hardhead_ym3812_interface =
{
	1,
	4000000,	/* ? */
	{ 50 },
	{  0 },		/* IRQ Line */
};


static struct MachineDriver machine_driver_hardhead =
{
	{
		{
			CPU_Z80,
			4000000,					/* ? */
			hardhead_readmem, hardhead_writemem,
			hardhead_readport,hardhead_writeport,
			interrupt, 1	/* No NMI */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,					/* ? */
			hardhead_sound_readmem, hardhead_sound_writemem,
			hardhead_sound_readport,hardhead_sound_writeport,
			interrupt, 4	/* No NMI */
		}
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0+16, 256-16-1 },
	suna8_gfxdecodeinfo,
	256, 256,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	suna8_vh_start_textdim12,
	0,
	suna8_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{	SOUND_YM3812,	&hardhead_ym3812_interface	},
		{	SOUND_AY8910,	&hardhead_ay8910_interface	}
	}
};



/***************************************************************************
								Rough Ranger
***************************************************************************/

/* 1 x 24 MHz crystal */

/* 2203 + 8910 */
static struct YM2203interface rranger_ym2203_interface =
{
	2,
	4000000,	/* ? */
	{ YM2203_VOL(50,50), YM2203_VOL(50,50) },
	{ 0,0 },	/* Port A Read  */
	{ 0,0 },	/* Port B Read  */
	{ 0,0 },	/* Port A Write */
	{ 0,0 },	/* Port B Write */
	{ 0,0 }		/* IRQ handler  */
};

static struct MachineDriver machine_driver_rranger =
{
	{
		{
			CPU_Z80,
			4000000,					/* ? */
			rranger_readmem,  rranger_writemem,
			rranger_readport, rranger_writeport,
			interrupt, 1	/* IRQ & NMI ! */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,					/* ? */
			rranger_sound_readmem,  rranger_sound_writemem,
			rranger_sound_readport, rranger_sound_writeport,
			interrupt, 4	/* NMI = retn */
		}
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0+16, 256-16-1 },
	suna8_gfxdecodeinfo,
	256, 256,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	suna8_vh_start_textdim8,
	0,
	suna8_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{	SOUND_YM2203,	&rranger_ym2203_interface	},
	}
};


/***************************************************************************
								Brick Zone
***************************************************************************/

/* 1 x 24 MHz crystal */

static struct AY8910interface brickzn_ay8910_interface =
{
	1,
	4000000,	/* ? */
	{ 33 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3812interface brickzn_ym3812_interface =
{
	1,
	4000000,	/* ? */
	{ 33 },
	{ soundirq },	/* IRQ Line */
};

static struct DACinterface brickzn_dac_interface =
{
	4,
	{	MIXER(17,MIXER_PAN_LEFT), MIXER(17,MIXER_PAN_RIGHT),
		MIXER(17,MIXER_PAN_LEFT), MIXER(17,MIXER_PAN_RIGHT)	}
};

int brickzn_interrupt(void)
{
	if (cpu_getiloops())	return Z80_NMI_INT;
	else					return Z80_IRQ_INT;
}

static struct MachineDriver machine_driver_brickzn =
{
	{
		{
			CPU_Z80,
			4000000,					/* ? */
			brickzn_readmem, brickzn_writemem,
			brickzn_readport,brickzn_writeport,
//			brickzn_interrupt, 2
interrupt, 1	// nmi breaks ramtest but is needed!
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,					/* ? */
			brickzn_sound_readmem, brickzn_sound_writemem,
			brickzn_sound_readport,brickzn_sound_writeport,
			ignore_interrupt, 1	/* IRQ by YM3812; No NMI */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,					/* ? */
			brickzn_pcm_readmem, brickzn_pcm_writemem,
			brickzn_pcm_readport,brickzn_pcm_writeport,
			ignore_interrupt, 1	/* No interrupts */
		}
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0+16, 256-16-1 },
	suna8_gfxdecodeinfo,
256*2, 256*2,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	suna8_vh_start_textdim0,
	0,
	suna8_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{	SOUND_YM3812,	&brickzn_ym3812_interface	},
		{	SOUND_AY8910,	&brickzn_ay8910_interface	},
		{	SOUND_DAC,		&brickzn_dac_interface		},
	}
};


/***************************************************************************
								Hard Head 2
***************************************************************************/

/* 1 x 24 MHz crystal */

int hardhea2_interrupt(void)
{
	if (cpu_getiloops())
		if (suna8_nmi_enable)	return Z80_NMI_INT;
		else					return ignore_interrupt();
	else					return Z80_IRQ_INT;
}

static struct MachineDriver machine_driver_hardhea2 =
{
	{
		{
			CPU_Z80,	/* SUNA T568009 */
			4000000,					/* ? */
			hardhea2_readmem, hardhea2_writemem,
			hardhea2_readport,hardhea2_writeport,
			hardhea2_interrupt, 2	/* IRQ & NMI */
		},

		/* The sound section is identical to that of brickzn */

		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,					/* ? */
			brickzn_sound_readmem, brickzn_sound_writemem,
			brickzn_sound_readport,brickzn_sound_writeport,
			ignore_interrupt, 1	/* IRQ by YM3812; No NMI */
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,					/* ? */
			brickzn_pcm_readmem, brickzn_pcm_writemem,
			brickzn_pcm_readport,brickzn_pcm_writeport,
			ignore_interrupt, 1	/* No interrupts */
		}
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0+16, 256-16-1 },
	suna8_gfxdecodeinfo,
	256, 256,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	suna8_vh_start_textdim0,
	0,
	suna8_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{	SOUND_YM3812,	&brickzn_ym3812_interface	},
		{	SOUND_AY8910,	&brickzn_ay8910_interface	},
		{	SOUND_DAC,		&brickzn_dac_interface		},
	}
};


/***************************************************************************
								Star Fighter
***************************************************************************/

static struct AY8910interface starfigh_ay8910_interface =
{
	1,
	4000000,	/* ? */
	{ 50 },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3812interface starfigh_ym3812_interface =
{
	1,
	4000000,	/* ? */
	{ 50 },
	{  0 },
};

static struct MachineDriver machine_driver_starfigh =
{
	{
		{
			CPU_Z80,
			4000000,					/* ? */
			starfigh_readmem, starfigh_writemem,
			starfigh_readport,starfigh_writeport,
			brickzn_interrupt, 2	/* IRQ & NMI */
		},

		/* The sound section is identical to that of hardhead */

		{
			CPU_Z80 | CPU_AUDIO_CPU,
			4000000,					/* ? */
			hardhead_sound_readmem, hardhead_sound_writemem,
			hardhead_sound_readport,hardhead_sound_writeport,
			interrupt, 4	/* No NMI */
		}
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	0,

	/* video hardware */
	256, 256, { 0, 256-1, 0+16, 256-16-1 },
	suna8_gfxdecodeinfo,
	256, 256,
	0,
	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	suna8_vh_start_textdim0,
	0,
	suna8_vh_screenrefresh,

	/* sound hardware */
	SOUND_SUPPORTS_STEREO,0,0,0,
	{
		{	SOUND_YM3812,	&starfigh_ym3812_interface	},
		{	SOUND_AY8910,	&starfigh_ay8910_interface	}
	}
};


/***************************************************************************


								ROMs Loading


***************************************************************************/

/***************************************************************************

									Hard Head

Location  Type    File ID  Checksum
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
L5       27C256     P1       1327   [ main program ]
K5       27C256     P2       50B1   [ main program ]
J5       27C256     P3       CF73   [ main program ]
I5       27C256     P4       DE86   [ main program ]
D5       27C256     P5       94D1   [  background  ]
A5       27C256     P6       C3C7   [ motion obj.  ]
L7       27C256     P7       A7B8   [ main program ]
K7       27C256     P8       5E53   [ main program ]
J7       27C256     P9       35FC   [ main program ]
I7       27C256     P10      8F9A   [ main program ]
D7       27C256     P11      931C   [  background  ]
A7       27C256     P12      2EED   [ motion obj.  ]
H9       27C256     P13      5CD2   [ snd program  ]
M9       27C256     P14      5576   [  sound data  ]

Note:  Game   No. KRB-14
       PCB    No. 60138-0083

Main processor  -  Custom security block (battery backed) CPU No. S562008

Sound processor -  Z80
                -  YM3812
                -  AY-3-8910

24 MHz crystal

***************************************************************************/

ROM_START( hardhead )
	ROM_REGION( 0x48000, REGION_CPU1 ) /* Main Z80 Code */
	ROM_LOAD( "p1",  0x00000, 0x8000, 0xc6147926 )	// 1988,9,14
	ROM_LOAD( "p2",  0x10000, 0x8000, 0xfaa2cf9a )
	ROM_LOAD( "p3",  0x18000, 0x8000, 0x3d24755e )
	ROM_LOAD( "p4",  0x20000, 0x8000, 0x0241ac79 )
	ROM_LOAD( "p7",  0x28000, 0x8000, 0xbeba8313 )
	ROM_LOAD( "p8",  0x30000, 0x8000, 0x211a9342 )
	ROM_LOAD( "p9",  0x38000, 0x8000, 0x2ad430c4 )
	ROM_LOAD( "p10", 0x40000, 0x8000, 0xb6894517 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "p13", 0x0000, 0x8000, 0x493c0b41 )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "p5",  0x00000, 0x8000, 0xe9aa6fba )
	ROM_RELOAD(      0x08000, 0x8000             )
	ROM_LOAD( "p6",  0x10000, 0x8000, 0x15d5f5dd )
	ROM_RELOAD(      0x18000, 0x8000             )
	ROM_LOAD( "p11", 0x20000, 0x8000, 0x055f4c29 )
	ROM_RELOAD(      0x28000, 0x8000             )
	ROM_LOAD( "p12", 0x30000, 0x8000, 0x9582e6db )
	ROM_RELOAD(      0x38000, 0x8000             )

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "p14", 0x0000, 0x8000, 0x41314ac1 )
ROM_END

ROM_START( hardhedb )
	ROM_REGION( 0x48000, REGION_CPU1 ) /* Main Z80 Code */
	ROM_LOAD( "9_1_6l.rom", 0x00000, 0x8000, 0x750e6aee )	// 1988,9,14 (already decrypted)
	ROM_LOAD( "p2",  0x10000, 0x8000, 0xfaa2cf9a )
	ROM_LOAD( "p3",  0x18000, 0x8000, 0x3d24755e )
	ROM_LOAD( "p4",  0x20000, 0x8000, 0x0241ac79 )
	ROM_LOAD( "p7",  0x28000, 0x8000, 0xbeba8313 )
	ROM_LOAD( "p8",  0x30000, 0x8000, 0x211a9342 )
	ROM_LOAD( "p9",  0x38000, 0x8000, 0x2ad430c4 )
	ROM_LOAD( "p10", 0x40000, 0x8000, 0xb6894517 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "p13", 0x0000, 0x8000, 0x493c0b41 )
//	ROM_LOAD( "2_13_9h.rom", 0x00000, 0x8000, 0x1b20e5ec )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "p5",  0x00000, 0x8000, 0xe9aa6fba )
	ROM_RELOAD(      0x08000, 0x8000             )
	ROM_LOAD( "p6",  0x10000, 0x8000, 0x15d5f5dd )
	ROM_RELOAD(      0x18000, 0x8000             )
	ROM_LOAD( "p11", 0x20000, 0x8000, 0x055f4c29 )
	ROM_RELOAD(      0x28000, 0x8000             )
	ROM_LOAD( "p12", 0x30000, 0x8000, 0x9582e6db )
	ROM_RELOAD(      0x38000, 0x8000             )

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "p14", 0x0000, 0x8000, 0x41314ac1 )
ROM_END


/***************************************************************************

							Rough Ranger / Super Ranger

(SunA 1988)
K030087

 24MHz    6  7  8  9  - 10 11 12 13   sw1  sw2



   6264
   5    6116
   4    6116                         6116
   3    6116                         14
   2    6116                         Z80A
   1                        6116     8910
                 6116  6116          2203
                                     15
 Epoxy CPU
                            6116


---------------------------
Super Ranger by SUNA (1988)
---------------------------

Location   Type    File ID  Checksum
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
E2        27C256     R1      28C0    [ main program ]
F2        27C256     R2      73AD    [ main program ]
H2        27C256     R3      8B7A    [ main program ]
I2        27C512     R4      77BE    [ main program ]
J2        27C512     R5      6121    [ main program ]
P5        27C256     R6      BE0E    [  background  ]
P6        27C256     R7      BD5A    [  background  ]
P7        27C256     R8      4605    [ motion obj.  ]
P8        27C256     R9      7097    [ motion obj.  ]
P9        27C256     R10     3B9F    [  background  ]
P10       27C256     R11     2AE8    [  background  ]
P11       27C256     R12     8B6D    [ motion obj.  ]
P12       27C256     R13     927E    [ motion obj.  ]
J13       27C256     R14     E817    [ snd program  ]
E13       27C256     R15     54EE    [ sound data   ]

Note:  Game model number K030087

Hardware:

Main processor  -  Custom security block (battery backed)  CPU No. S562008

Sound processor - Z80
                - YM2203C
                - AY-3-8910

***************************************************************************/

ROM_START( rranger )
	ROM_REGION( 0x48000, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "1",  0x00000, 0x8000, 0x4fb4f096 )	// V 2.0 1988,4,15
	ROM_LOAD( "2",  0x10000, 0x8000, 0xff65af29 )
	ROM_LOAD( "3",  0x18000, 0x8000, 0x64e09436 )
	ROM_LOAD( "r4", 0x30000, 0x8000, 0x4346fae6 )
	ROM_CONTINUE(   0x20000, 0x8000             )
	ROM_LOAD( "r5", 0x38000, 0x8000, 0x6a7ca1c3 )
	ROM_CONTINUE(   0x28000, 0x8000             )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "14", 0x0000, 0x8000, 0x11c83aa1 )

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "15", 0x0000, 0x8000, 0x28c2c87e )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "6",  0x00000, 0x8000, 0x57543643 )
	ROM_LOAD( "7",  0x08000, 0x8000, 0x9f35dbfa )
	ROM_LOAD( "8",  0x10000, 0x8000, 0xf400db89 )
	ROM_LOAD( "9",  0x18000, 0x8000, 0xfa2a11ea )
	ROM_LOAD( "10", 0x20000, 0x8000, 0x42c4fdbf )
	ROM_LOAD( "11", 0x28000, 0x8000, 0x19037a7b )
	ROM_LOAD( "12", 0x30000, 0x8000, 0xc59c0ec7 )
	ROM_LOAD( "13", 0x38000, 0x8000, 0x9809fee8 )
ROM_END

ROM_START( sranger )
	ROM_REGION( 0x48000, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "r1", 0x00000, 0x8000, 0x4eef1ede )	// V 2.0 1988,4,15
	ROM_LOAD( "2",  0x10000, 0x8000, 0xff65af29 )
	ROM_LOAD( "3",  0x18000, 0x8000, 0x64e09436 )
	ROM_LOAD( "r4", 0x30000, 0x8000, 0x4346fae6 )
	ROM_CONTINUE(   0x20000, 0x8000             )
	ROM_LOAD( "r5", 0x38000, 0x8000, 0x6a7ca1c3 )
	ROM_CONTINUE(   0x28000, 0x8000             )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "14", 0x0000, 0x8000, 0x11c83aa1 )

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "15", 0x0000, 0x8000, 0x28c2c87e )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "r6",  0x00000, 0x8000, 0x4f11fef3 )
	ROM_LOAD( "7",   0x08000, 0x8000, 0x9f35dbfa )
	ROM_LOAD( "8",   0x10000, 0x8000, 0xf400db89 )
	ROM_LOAD( "9",   0x18000, 0x8000, 0xfa2a11ea )
	ROM_LOAD( "r10", 0x20000, 0x8000, 0x1b204d6b )
	ROM_LOAD( "11",  0x28000, 0x8000, 0x19037a7b )
	ROM_LOAD( "12",  0x30000, 0x8000, 0xc59c0ec7 )
	ROM_LOAD( "13",  0x38000, 0x8000, 0x9809fee8 )
ROM_END

ROM_START( srangerb )
	ROM_REGION( 0x48000, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "r1bt", 0x00000, 0x8000, 0x40635e7c )	// NYWACORPORATION LTD 88-1-07
	ROM_LOAD( "2",    0x10000, 0x8000, 0xff65af29 )
	ROM_LOAD( "3",    0x18000, 0x8000, 0x64e09436 )
	ROM_LOAD( "r4",   0x30000, 0x8000, 0x4346fae6 )
	ROM_CONTINUE(     0x20000, 0x8000             )
	ROM_LOAD( "r5",   0x38000, 0x8000, 0x6a7ca1c3 )
	ROM_CONTINUE(     0x28000, 0x8000             )
	ROM_LOAD( "r5bt", 0x28000, 0x8000, BADCRC(0xf7f391b5) )	// wrong length

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "14", 0x0000, 0x8000, 0x11c83aa1 )

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "15", 0x0000, 0x8000, 0x28c2c87e )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "r6",  0x00000, 0x8000, 0x4f11fef3 )
	ROM_LOAD( "7",   0x08000, 0x8000, 0x9f35dbfa )
	ROM_LOAD( "8",   0x10000, 0x8000, 0xf400db89 )
	ROM_LOAD( "9",   0x18000, 0x8000, 0xfa2a11ea )
	ROM_LOAD( "r10", 0x20000, 0x8000, 0x1b204d6b )
	ROM_LOAD( "11",  0x28000, 0x8000, 0x19037a7b )
	ROM_LOAD( "12",  0x30000, 0x8000, 0xc59c0ec7 )
	ROM_LOAD( "13",  0x38000, 0x8000, 0x9809fee8 )
ROM_END

ROM_START( srangerw )
	ROM_REGION( 0x48000, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "1",  0x00000, 0x8000, 0x2287d3fc )	// 88,2,28
	ROM_LOAD( "2",  0x10000, 0x8000, 0xff65af29 )
	ROM_LOAD( "3",  0x18000, 0x8000, 0x64e09436 )
	ROM_LOAD( "r4", 0x30000, 0x8000, 0x4346fae6 )
	ROM_CONTINUE(   0x20000, 0x8000             )
	ROM_LOAD( "r5", 0x38000, 0x8000, 0x6a7ca1c3 )
	ROM_CONTINUE(   0x28000, 0x8000             )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sound Z80 Code */
	ROM_LOAD( "14", 0x0000, 0x8000, 0x11c83aa1 )

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "15", 0x0000, 0x8000, 0x28c2c87e )

	ROM_REGION( 0x40000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "6",  0x00000, 0x8000, 0x312ecda6 )
	ROM_LOAD( "7",  0x08000, 0x8000, 0x9f35dbfa )
	ROM_LOAD( "8",  0x10000, 0x8000, 0xf400db89 )
	ROM_LOAD( "9",  0x18000, 0x8000, 0xfa2a11ea )
	ROM_LOAD( "10", 0x20000, 0x8000, 0x8731abc6 )
	ROM_LOAD( "11", 0x28000, 0x8000, 0x19037a7b )
	ROM_LOAD( "12", 0x30000, 0x8000, 0xc59c0ec7 )
	ROM_LOAD( "13", 0x38000, 0x8000, 0x9809fee8 )
ROM_END


/***************************************************************************

									Brick Zone

SUNA ELECTRONICS IND CO., LTD

CPU Z0840006PSC (ZILOG)

Chrystal : 24.000 Mhz

Sound CPU : Z084006PSC (ZILOG) + AY3-8910A

Warning ! This game has a 'SUNA' protection block :-(

-

(c) 1992 Suna Electronics

2 * Z80B

AY-3-8910
YM3812

24 MHz crystal

Large epoxy(?) module near the cpu's.

***************************************************************************/

ROM_START( brickzn )
	ROM_REGION( 0x50000 * 2, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "brickzon.009", 0x00000, 0x08000, 0x1ea68dea )	// V5.0 1992,3,3
	ROM_RELOAD(               0x50000, 0x08000             )
	ROM_LOAD( "brickzon.008", 0x10000, 0x20000, 0xc61540ba )
	ROM_RELOAD(               0x60000, 0x20000             )
	ROM_LOAD( "brickzon.007", 0x30000, 0x20000, 0xceed12f1 )
	ROM_RELOAD(               0x80000, 0x20000             )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Music Z80 Code */
	ROM_LOAD( "brickzon.010", 0x00000, 0x10000, 0x4eba8178 )

	ROM_REGION( 0x10000, REGION_CPU3 )		/* PCM Z80 Code */
	ROM_LOAD( "brickzon.011", 0x00000, 0x10000, 0x6c54161a )

	ROM_REGION( 0xc0000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "brickzon.002", 0x00000, 0x20000, 0x241f0659 )
	ROM_LOAD( "brickzon.001", 0x20000, 0x20000, 0x6970ada9 )
	ROM_LOAD( "brickzon.003", 0x40000, 0x20000, 0x2e4f194b )
	ROM_LOAD( "brickzon.005", 0x60000, 0x20000, 0x118f8392 )
	ROM_LOAD( "brickzon.004", 0x80000, 0x20000, 0x2be5f335 )
	ROM_LOAD( "brickzon.006", 0xa0000, 0x20000, 0xbbf31081 )

	ROM_REGION( 0x0200 * 2, REGION_USER1 )	/* Palette RAM Banks */
	ROM_REGION( 0x2000 * 2, REGION_USER2 )	/* Sprite  RAM Banks */
ROM_END

ROM_START( brickzn3 )
	ROM_REGION( 0x50000 * 2, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "39",           0x00000, 0x08000, 0x043380bd )	// V3.0 1992,1,23
	ROM_RELOAD(               0x50000, 0x08000             )
	ROM_LOAD( "38",           0x10000, 0x20000, 0xe16216e8 )
	ROM_RELOAD(               0x60000, 0x20000             )
	ROM_LOAD( "brickzon.007", 0x30000, 0x20000, 0xceed12f1 )
	ROM_RELOAD(               0x80000, 0x20000             )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Music Z80 Code */
	ROM_LOAD( "brickzon.010", 0x00000, 0x10000, 0x4eba8178 )

	ROM_REGION( 0x10000, REGION_CPU3 )		/* PCM Z80 Code */
	ROM_LOAD( "brickzon.011", 0x00000, 0x10000, 0x6c54161a )

	ROM_REGION( 0xc0000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "35",           0x00000, 0x20000, 0xb463dfcf )
	ROM_LOAD( "brickzon.004", 0x20000, 0x20000, 0x2be5f335 )
	ROM_LOAD( "brickzon.006", 0x40000, 0x20000, 0xbbf31081 )
	ROM_LOAD( "32",           0x60000, 0x20000, 0x32dbf2dd )
	ROM_LOAD( "brickzon.001", 0x80000, 0x20000, 0x6970ada9 )
	ROM_LOAD( "brickzon.003", 0xa0000, 0x20000, 0x2e4f194b )

	ROM_REGION( 0x0200 * 2, REGION_USER1 )	/* Palette RAM Banks */
	ROM_REGION( 0x2000 * 2, REGION_USER2 )	/* Sprite  RAM Banks */
ROM_END



/***************************************************************************

								Hard Head 2

These ROMS are all 27C512

ROM 1 is at Location 1N
ROM 2 ..............1o
ROM 3 ..............1Q
ROM 4...............3N
ROM 5.............. 4N
ROM 6...............4o
ROM 7...............4Q
ROM 8...............6N
ROM 10..............H5
ROM 11..............i5
ROM 12 .............F7
ROM 13..............H7
ROM 15..............N10

These ROMs are 27C256

ROM 9...............F5
ROM 14..............C8

Game uses 2 Z80B processors and a Custom Sealed processor (assumed)
Labeled "SUNA T568009"

Sound is a Yamaha YM3812 and a  AY-3-8910A

3 RAMS are 6264LP- 10   and 5) HM6116K-90 rams  (small package)

24 mhz

***************************************************************************/

ROM_START( hardhea2 )
	ROM_REGION( 0x50000 * 2, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "hrd-hd9",  0x00000, 0x08000, 0x69c4c307 )	// V 2.0 1991,2,12
	ROM_RELOAD(           0x50000, 0x08000             )
	ROM_LOAD( "hrd-hd10", 0x10000, 0x10000, 0x77ec5b0a )
	ROM_RELOAD(           0x60000, 0x10000             )
	ROM_LOAD( "hrd-hd11", 0x20000, 0x10000, 0x12af8f8e )
	ROM_RELOAD(           0x70000, 0x10000             )
	ROM_LOAD( "hrd-hd12", 0x30000, 0x10000, 0x35d13212 )
	ROM_RELOAD(           0x80000, 0x10000             )
	ROM_LOAD( "hrd-hd13", 0x40000, 0x10000, 0x3225e7d7 )
	ROM_RELOAD(           0x90000, 0x10000             )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Music Z80 Code */
	ROM_LOAD( "hrd-hd14", 0x00000, 0x08000, 0x79a3be51 )

	ROM_REGION( 0x10000, REGION_CPU3 )		/* PCM Z80 Code */
	ROM_LOAD( "hrd-hd15", 0x00000, 0x10000, 0xbcbd88c3 )

	ROM_REGION( 0x80000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "hrd-hd1",  0x00000, 0x10000, 0x7e7b7a58 )
	ROM_LOAD( "hrd-hd2",  0x10000, 0x10000, 0x303ec802 )
	ROM_LOAD( "hrd-hd3",  0x20000, 0x10000, 0x3353b2c7 )
	ROM_LOAD( "hrd-hd4",  0x30000, 0x10000, 0xdbc1f9c1 )
	ROM_LOAD( "hrd-hd5",  0x40000, 0x10000, 0xf738c0af )
	ROM_LOAD( "hrd-hd6",  0x50000, 0x10000, 0xbf90d3ca )
	ROM_LOAD( "hrd-hd7",  0x60000, 0x10000, 0x992ce8cb )
	ROM_LOAD( "hrd-hd8",  0x70000, 0x10000, 0x359597a4 )

	ROM_REGION( 0x0200 * 2, REGION_USER1 )	/* Palette RAM Banks */
	ROM_REGION( 0x2000 * 2, REGION_USER2 )	/* Sprite  RAM Banks */
ROM_END


/***************************************************************************

								Star Fighter

***************************************************************************/

ROM_START( starfigh )
	ROM_REGION( 0x50000 * 2, REGION_CPU1 )		/* Main Z80 Code */
	ROM_LOAD( "suna3.bin", 0x00000, 0x08000, 0xf93802c6 )	// V.1
	ROM_RELOAD(            0x50000, 0x08000             )
	ROM_LOAD( "suna2.bin", 0x10000, 0x10000, 0xfcfcf08a )
	ROM_RELOAD(            0x60000, 0x10000             )
	ROM_LOAD( "suna1.bin", 0x20000, 0x10000, 0x6935fcdb )
	ROM_RELOAD(            0x70000, 0x10000             )
	ROM_LOAD( "suna5.bin", 0x30000, 0x10000, 0x50c072a4 )	// 0xxxxxxxxxxxxxxx = 0xFF (ROM Test: OK)
	ROM_RELOAD(            0x80000, 0x10000             )
	ROM_LOAD( "suna4.bin", 0x40000, 0x10000, 0x3fe3c714 )	// clear text here
	ROM_RELOAD(            0x90000, 0x10000             )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Music Z80 Code */
	ROM_LOAD( "suna14.bin", 0x0000, 0x8000, 0xae3b0691 )

	ROM_REGION( 0x8000, REGION_SOUND1 )	/* Samples */
	ROM_LOAD( "suna15.bin", 0x0000, 0x8000, 0xfa510e94 )

	ROM_REGION( 0x100000, REGION_GFX1 )	/* Sprites */
	ROM_LOAD( "suna9.bin",  0x00000, 0x10000, 0x54c0ca3d )
	ROM_RELOAD(             0x20000, 0x10000             )
	ROM_LOAD( "suna8.bin",  0x10000, 0x10000, 0x4313ba40 )
	ROM_RELOAD(             0x30000, 0x10000             )
	ROM_LOAD( "suna7.bin",  0x40000, 0x10000, 0xad8d0f21 )
	ROM_RELOAD(             0x60000, 0x10000             )
	ROM_LOAD( "suna6.bin",  0x50000, 0x10000, 0x6d8f74c8 )
	ROM_RELOAD(             0x70000, 0x10000             )
	ROM_LOAD( "suna13.bin", 0x80000, 0x10000, 0xceff00ff )
	ROM_RELOAD(             0xa0000, 0x10000             )
	ROM_LOAD( "suna12.bin", 0x90000, 0x10000, 0x7aaa358a )
	ROM_RELOAD(             0xb0000, 0x10000             )
	ROM_LOAD( "suna11.bin", 0xc0000, 0x10000, 0x47d6049c )
	ROM_RELOAD(             0xe0000, 0x10000             )
	ROM_LOAD( "suna10.bin", 0xd0000, 0x10000, 0x4a33f6f3 )
	ROM_RELOAD(             0xf0000, 0x10000             )

	ROM_REGION( 0x0200 * 2, REGION_USER1 )	/* Palette RAM Banks */
	ROM_REGION( 0x2000 * 2, REGION_USER2 )	/* Sprite  RAM Banks */
ROM_END


/***************************************************************************


								Games Drivers


***************************************************************************/

/* Working Games */
GAMEX( 1988, rranger,  0,        rranger,  rranger,  rranger,  ROT0_16BIT,  "SunA", "Rough Ranger (v2.0, Sharp Image license)", GAME_IMPERFECT_SOUND )
GAMEX( 1988, hardhead, 0,        hardhead, hardhead, hardhead, ROT0_16BIT,  "SunA", "Hard Head",           GAME_IMPERFECT_SOUND )
GAMEX( 1988, hardhedb, hardhead, hardhead, hardhead, hardhedb, ROT0_16BIT,  "SunA", "Hard Head (Bootleg)", GAME_IMPERFECT_SOUND )

/* Non Working Games */
GAMEX( 1988, sranger,  rranger,  rranger,  rranger,	0,         ROT0_16BIT,  "SunA", "Super Ranger (v2.0)",         GAME_NOT_WORKING )
GAMEX( 1988, srangerb, rranger,  rranger,  rranger,	0,         ROT0_16BIT,  "SunA", "Super Ranger (NIWA Bootleg)", GAME_NOT_WORKING )
GAMEX( 1988, srangerw, rranger,  rranger,  rranger,	0,         ROT0_16BIT,  "SunA", "Super Ranger (WDK License)",  GAME_NOT_WORKING )
GAMEX( 1990, starfigh, 0,        starfigh, hardhea2, starfigh, ROT90_16BIT, "SunA", "Star Fighter (v1)",   GAME_NOT_WORKING )
GAMEX( 1991, hardhea2, 0,        hardhea2, hardhea2, hardhea2, ROT0_16BIT,  "SunA", "Hard Head 2 (v2.0)",  GAME_NOT_WORKING )
GAMEX( 1992, brickzn,  0,        brickzn,  brickzn,  brickzn3, ROT90_16BIT, "SunA", "Brick Zone (v5.0)",   GAME_NOT_WORKING )
GAMEX( 1992, brickzn3, brickzn,  brickzn,  brickzn,  brickzn3, ROT90_16BIT, "SunA", "Brick Zone (v3.0)",   GAME_NOT_WORKING )

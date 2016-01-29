#include "../vidhrdw/wecleman.cpp"

/***************************************************************************
						WEC Le Mans 24  &   Hot Chase

					      (C)   1986 & 1988 Konami

					driver by	Luca Elia (eliavit@unina.it)


----------------------------------------------------------------------
Hardware				Main 	Sub		Sound	Sound Chips
----------------------------------------------------------------------
[WEC Le Mans 24]		68000	68000	Z-80	YM2151 YM3012 1x007232

[Hot Chase]				68000	68000	68B09E	              3x007232

 [CPU PCB GX763 350861B]
 						007641	007770	3x007232	051550

 [VID PCB GX763 350860A AI AM-1]
 						007634	007635	3x051316	007558	007557
----------------------------------------------------------------------


----------------------------------------------------------------
Main CPU				[WEC Le Mans 24]		[Hot Chase]
----------------------------------------------------------------
ROM				R		000000-03ffff			<
Work RAM		RW		040000-043fff			040000-063fff*
?				RW		060000-060007			-
Blitter			 W		080000-080011			<
Page RAM		RW		100000-103fff			-
Text RAM		RW		108000-108fff			-
Palette RAM		RW		110000-110fff			110000-111fff**
Shared RAM		RW		124000-127fff			120000-123fff
Sprites RAM		RW		130000-130fff			<
Input Ports		RW		1400xx-1400xx			<
Background		RW		-						100000-100fff
Background Ctrl	 W		-						101000-10101f
Foreground		RW		-						102000-102fff
Foreground Ctrl	 W		-						103000-10301f

* weird					** only half used

----------------------------------------------------------------
Sub CPU					[WEC Le Mans 24]		[Hot Chase]
----------------------------------------------------------------

ROM				R		000000-00ffff			000000-01ffff
Work RAM		RW		-						060000-060fff
Road RAM		RW		060000-060fff			020000-020fff
Shared RAM		RW		070000-073fff			040000-043fff


---------------------------------------------------------------------------
								Game code
							[WEC Le Mans 24]
---------------------------------------------------------------------------

					Interesting locations (main cpu)
					--------------------------------

There's some 68000 assembly code in ASCII around d88 :-)

040000+
7-9				*** hi score/10 (BCD 3 bytes) ***
b-d				*** score/10 (BCD 3 bytes) ***
1a,127806		<- 140011.b
1b,127807		<- 140013.b
1c,127808		<- 140013.b
1d,127809		<- 140015.b
1e,12780a		<- 140017.b
1f				<- 140013.b
30				*** credits ***
3a,3b,3c,3d		<-140021.b
3a = accelerator   3b = ??   3c = steering   3d = table

d2.w			-> 108f24 fg y scroll
112.w			-> 108f26 bg y scroll

16c				influences 140031.b
174				screen address
180				input port selection (->140003.b ->140021.b)
181				->140005.b
185				bit 7 high -> must copy sprite data to 130000
1dc+(1da).w		->140001.b

40a.w,c.w		*** time (BCD) ***
411				EF if brake, 0 otherwise
416				?
419				gear: 0=lo,1=hi
41e.w			speed related ->127880
424.w			speed BCD
43c.w			accel?
440.w			level?

806.w			scrollx related
80e.w			scrolly related

c08.b			routine select: 1>1e1a4	2>1e1ec	3>1e19e	other>1e288 (map screen)

117a.b			selected letter when entering name in hi-scores
117e.w			cycling color in hi-scores

12c0.w			?time,pos,len related?
12c2.w
12c4.w
12c6.w

1400-1bff		color data (0000-1023 chars)
1c00-23ff		color data (1024-2047 sprites?)

2400			Sprite data: 40 entries x  4 bytes =  100
2800			Sprite data: 40 entries x 40 bytes = 1000
3800			Sprite data: 40 entries x 10 bytes =  400

					Interesting routines (main cpu)
					-------------------------------

804				mem test
818				end mem test (cksums at 100, addresses at A90)
82c				other cpu test
a0c				rom test
c1a				prints string (a1)
1028			end test
204c			print 4*3 box of chars to a1, made up from 2 2*6 (a0)=0xLR (Left,Righ index)
4e62			raws in the fourth page of chars
6020			test screen (print)
60d6			test screen
62c4			motor test?
6640			print input port values ( 6698 = scr_disp.w,ip.b,bit.b[+/-] )

819c			prepares sprite data
8526			blitter: 42400->130000
800c			8580	sprites setup on map screen

1833a			cycle cols on hi-scores
18514			hiscores: main loop
185e8			hiscores: wheel selects letter

TRAP#0			prints string: A0-> addr.l, attr.w, (char.b)*, 0

IRQs			1,3,6]	602
				2,7]	1008->12dc	ORI.W    #$2700,(A7) RTE
				4]	1004->124c
				5]	106c->1222	calls sequence: $3d24 $1984 $28ca $36d2 $3e78




					Interesting locations (sub cpu)
					-------------------------------

					Interesting routines (sub cpu)
					------------------------------

1028	'wait for command' loop.
1138	lev4 irq
1192	copies E0*4 bytes: (a1)+ -> (a0)+





---------------------------------------------------------------------------
								 Game code
								[Hot Chase]
---------------------------------------------------------------------------

This game has been probably coded by the same programmers of WEC Le Mans 24
It shares some routines and there is the (hidden?) string "WEC 2" somewhere

							Main CPU		Sub CPU

Interrupts:		1, 7] 		FFFFFFFF		FFFFFFFF
				2,3,4,5,6]	221c			1288

Self Test:
 0] pause,120002==55,pause,120002==AA,pause,120002==CC, (on error set bit d7.0)
 6] 60000-63fff(d7.1),40000-41fff(d7.2)
 8] 40000/2<-chksum 0-20000(e/o);40004/6<-chksum 20000-2ffff(e/o) (d7.3456)
 9] chksums from sub cpu: even->40004	odd->(40006)	(d7.78)
 A] 110000-111fff(even)(d7.9),102000-102fff(odd)(d7.a)
 C] 100000-100fff(odd)(d7.b),pause,pause,pause
10] 120004==0(d7.c),120006==0(d7.d),130000-1307ff(first $A of every $10 bytes only)(d7.e),pause
14] 102000<-hw screen+(d7==0)? jmp 1934/1000
15] 195c start of game


					Interesting locations (main cpu)
					--------------------------------

60024.b			<- !140017.b (DSW 1 - coinage)
60025.b			<- !140015.b (DSW 2 - options)
6102c.w			*** time ***

					Interesting routines (main cpu)
					-------------------------------

18d2			(d7.d6)?print BAD/OK to (a5)+, jmp(D0)
1d58			print d2.w to (a4)+, jmp(a6)
580c			writes at 60000
61fc			print test strings
18cbe			print "game over"




---------------------------------------------------------------------------
								   Issues
							  [WEC Le Mans 24]
---------------------------------------------------------------------------

- Wrong colours (only the text layer is ok at the moment. Note that the top
  half of colours is written by the blitter, 16 colours a time, the bottom
  half by the cpu, 8 colours a time)
- The parallactic scrolling is sometimes wrong

---------------------------------------------------------------------------
								   Issues
								[Hot Chase]
---------------------------------------------------------------------------

- Samples pitch is too low
- No zoom and rotation of the layers

---------------------------------------------------------------------------
							   Common Issues
---------------------------------------------------------------------------

- One ROM unused (32K in hotchase, 16K in wecleman)
- Incomplete DSWs
- No shadow sprites
- Sprite ram is not cleared by the game and no sprite list end-marker
  is written. We cope with that with an hack in the Blitter but there
  must be a register to do the trick


***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/konamiic.h"
#include "cpu/m6809/m6809.h"

/* Variables only used here: */

static unsigned char *sharedram, *blitter_regs;
static int multiply_reg[2];



/* Variables that vidhrdw has acces to: */

int wecleman_selected_ip, wecleman_irqctrl;


/* Variables defined in vidhrdw: */

extern unsigned char *wecleman_pageram, *wecleman_txtram, *wecleman_roadram, *wecleman_unknown;
extern size_t wecleman_roadram_size;
extern int wecleman_bgpage[4], wecleman_fgpage[4], *wecleman_gfx_bank;


/* Functions defined in vidhrdw: */

WRITE_HANDLER( paletteram_SBGRBBBBGGGGRRRR_word_w );

READ_HANDLER( wecleman_pageram_r );
WRITE_HANDLER( wecleman_pageram_w );
READ_HANDLER( wecleman_txtram_r );
WRITE_HANDLER( wecleman_txtram_w );
void wecleman_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int  wecleman_vh_start(void);

void hotchase_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
int  hotchase_vh_start(void);
void hotchase_vh_stop(void);



/* This macro is used to decipher the gfx ROMs */

#define BITSWAP(_from,_len,_14,_13,_12,_11,_10,_f,_e,_d,_c,_b,_a,_9,_8,_7,_6,_5,_4,_3,_2,_1,_0)\
{	unsigned char *buffer; \
	unsigned char *src = _from; \
	if ((buffer = (unsigned char*)malloc(_len))) \
	{ \
		for (i = 0 ; i <= _len ; i++) \
			buffer[i] = \
			 src[(((i & (1 << _0))?(1<<0x0):0) + \
				 ((i & (1 << _1))?(1<<0x1):0) + \
				 ((i & (1 << _2))?(1<<0x2):0) + \
				 ((i & (1 << _3))?(1<<0x3):0) + \
				 ((i & (1 << _4))?(1<<0x4):0) + \
				 ((i & (1 << _5))?(1<<0x5):0) + \
				 ((i & (1 << _6))?(1<<0x6):0) + \
				 ((i & (1 << _7))?(1<<0x7):0) + \
				 ((i & (1 << _8))?(1<<0x8):0) + \
				 ((i & (1 << _9))?(1<<0x9):0) + \
				 ((i & (1 << _a))?(1<<0xa):0) + \
				 ((i & (1 << _b))?(1<<0xb):0) + \
				 ((i & (1 << _c))?(1<<0xc):0) + \
				 ((i & (1 << _d))?(1<<0xd):0) + \
				 ((i & (1 << _e))?(1<<0xe):0) + \
				 ((i & (1 << _f))?(1<<0xf):0) + \
				 ((i & (1 << _10))?(1<<0x10):0) + \
				 ((i & (1 << _11))?(1<<0x11):0) + \
				 ((i & (1 << _12))?(1<<0x12):0) + \
				 ((i & (1 << _13))?(1<<0x13):0) + \
				 ((i & (1 << _14))?(1<<0x14):0))]; \
		memcpy(src, buffer, _len); \
		free(buffer); \
	} \
}



/***************************************************************************
							Common routines
***************************************************************************/


/* 140005.b (WEC Le Mans 24 Schematics)

 COMMAND
 ___|____
|   CK  8|--/			7
| LS273 7| TV-KILL		6
|       6| SCR-VCNT		5
|       5| SCR-HCNT		4
|   5H  4| SOUND-RST	3
|       3| SOUND-ON		2
|       2| NSUBRST		1
|       1| SUBINT		0
|__CLR___|
    |
  NEXRES

 Schems: SUBRESET does a RST+HALT
		 Sub CPU IRQ 4 generated by SUBINT, no other IRQs
*/

static WRITE_HANDLER( irqctrl_w )
{

//	logerror("CPU #0 - PC = %06X - $140005 <- %02X (old value: %02X)\n",cpu_get_pc(), data&0xFF, old_data&0xFF);

//	Bit 0 : SUBINT
	if ( (wecleman_irqctrl & 1) && (!(data & 1)) )	// 1->0 transition
		cpu_set_irq_line(1,4,HOLD_LINE);


//	Bit 1 : NSUBRST
	if (data & 2)
		cpu_set_reset_line(1,CLEAR_LINE);
	else
		cpu_set_reset_line(1,ASSERT_LINE);


//	Bit 2 : SOUND-ON
//	Bit 3 : SOUNDRST
//	Bit 4 : SCR-HCNT
//	Bit 5 : SCR-VCNT
//	Bit 6 : TV-KILL

	wecleman_irqctrl = data;	// latch the value
}





static READ_HANDLER( accelerator_r )
{
#define MAX_ACCEL 0x80

	return (readinputport(4) & 1) ? MAX_ACCEL : 0;
}


/* This function allows the gear to be handled using two buttons
   A macro is needed because wecleman sees the high gear when a
   bit is on, hotchase when a bit is off */

#define READ_GEAR(_name_,_high_gear_) \
READ_HANDLER( _name_ ) \
{ \
	static int ret = (_high_gear_ ^ 1) << 5; /* start with low gear */ \
	switch ( (readinputport(4) >> 2) & 3 ) \
	{ \
		case 1 : ret = (_high_gear_ ^ 1) << 5;	break;	/* low gear */ \
		case 2 : ret = (_high_gear_    ) << 5;	break;	/*  high gear */ \
	} \
	return (ret | readinputport(0));	/* previous value */ \
}

READ_GEAR(wecleman_gear_r,1)
READ_GEAR(hotchase_gear_r,0)



/* 140003.b (usually paired with a write to 140021.b)

	Bit:

	7-------	?
	-65-----	input selection (0-3)
	---43---	?
	-----2--	start light
	------10	? out 1/2

*/
static WRITE_HANDLER( selected_ip_w )
{
	wecleman_selected_ip = data;	// latch the value
}


/* $140021.b - Return the previously selected input port's value */
static READ_HANDLER( selected_ip_r )
{
	switch ( (wecleman_selected_ip >> 5) & 3 )
	{													// From WEC Le Mans Schems:
		case 0: 	return accelerator_r(offset);		// Accel - Schems: Accelevr
		case 1: 	return 0xffff;						// ????? - Schems: Not Used
		case 2:		return input_port_5_r(offset);		// Wheel - Schems: Handlevr
		case 3:		return 0xffff;						// Table - Schems: Turnvr

		default:	return 0xffff;
	}
}



/* Data is read from and written to *sharedram* */
static READ_HANDLER( sharedram_r )				{ return READ_WORD(&sharedram[offset]); }
static WRITE_HANDLER( sharedram_w )	{ COMBINE_WORD_MEM(&sharedram[offset], data); }


/* Data is read from and written to *spriteram* */
static READ_HANDLER( spriteram_word_r )			{ return READ_WORD(&spriteram[offset]); }
static WRITE_HANDLER( spriteram_word_w )	{ COMBINE_WORD_MEM(&spriteram[offset], data); }




/*	Word Blitter	-	Copies data around (Work RAM, Sprite RAM etc.)
						It's fed with a list of blits to do

	Offset:

	00.b		? Number of words - 1 to add to address per transfer
	01.b		? logic function / blit mode
	02.w		? (always 0)
	04.l		Source address (Base address of source data)
	08.l		List of blits address
	0c.l		Destination address
	01.b		? Number of transfers
	10.b		Triggers the blit
	11.b		Number of words per transfer

	The list contains 4 bytes per blit:

	Offset:

	00.w		?
	02.w		offset from Base address

*/

static READ_HANDLER( blitter_r )
{
	return READ_WORD(&blitter_regs[offset]);
}

static WRITE_HANDLER( blitter_w )
{
	COMBINE_WORD_MEM(&blitter_regs[offset],data);

	/* do a blit if $80010.b has been written */
	if ((offset == 0x10) && (data&0x00FF0000))
	{
		/* 80000.b = ?? usually 0 - other values: 02 ; 00 - ? logic function ? */
		/* 80001.b = ?? usually 0 - other values: 3f ; 01 - ? height ? */
		int minterm		=	(READ_WORD(&blitter_regs[0x0]) & 0xFF00 ) >> 8;
		int list_len	=	(READ_WORD(&blitter_regs[0x0]) & 0x00FF ) >> 0;

		/* 80002.w = ?? always 0 - ? increment per horizontal line ? */
		/* no proof at all, it's always 0 */
//		int srcdisp		=	READ_WORD(&blitter_regs[0x2])&0xFF00;
//		int destdisp	=	READ_WORD(&blitter_regs[0x2])&0x00FF;

		/* 80004.l = source data address */
		int src  =	(READ_WORD(&blitter_regs[0x4])<<16)+
					 READ_WORD(&blitter_regs[0x6]);

		/* 80008.l = list of blits address */
		int list =	(READ_WORD(&blitter_regs[0x8])<<16)+
				 	 READ_WORD(&blitter_regs[0xA]);

		/* 8000C.l = destination address */
		int dest =	(READ_WORD(&blitter_regs[0xC])<<16)+
					 READ_WORD(&blitter_regs[0xE]);

		/* 80010.b = number of words to move */
		int size =	(READ_WORD(&blitter_regs[0x10]))&0x00FF;

#if 0
		{
			int i;
			logerror("Blitter (PC = %06X): ",cpu_get_pc());
			for (i=0;i<0x12;i+=2) logerror("%04X ",READ_WORD(&blitter_regs[i]) );
			logerror("\n");
		}
#endif

		/* Word aligned transfers only */
		src  &= (~1);	list &= (~1);	dest &= (~1);


		/* Two minterms / blit modes are used */
		if (minterm != 2)
		{
			/* One single blit */
			for ( ; size > 0 ; size--)
			{
				/* maybe slower than a memcpy but safer (and errors are logged) */
				cpu_writemem24bew_word(dest,cpu_readmem24bew_word(src));
				src += 2;		dest += 2;
			}
//			src  += srcdisp;	dest += destdisp;
		}
		else
		{
			/* Number of blits in the list */
			for ( ; list_len > 0 ; list_len-- )
			{
			int j;

				/* Read offset of source from the list of blits */
				int addr = src + cpu_readmem24bew_word( list + 2 );

				for (j = size; j > 0; j--)
				{
					cpu_writemem24bew_word(dest,cpu_readmem24bew_word(addr));
					dest += 2;	addr += 2;
				}
				dest += 16-size*2;	/* hack for the blit to Sprites RAM */
				list +=  4;
			}

			/* hack for the blit to Sprites RAM - Sprite list end-marker */
			cpu_writemem24bew_word(dest,0xFFFF);
		}
	} /* end blit */
}



/*
**
**	Main cpu data
**
**
*/



/***************************************************************************
								WEC Le Mans 24
***************************************************************************/

static WRITE_HANDLER( wecleman_soundlatch_w );

static struct MemoryReadAddress wecleman_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM				},	// ROM
	{ 0x040000, 0x043fff, MRA_BANK1				},	// RAM
	{ 0x060000, 0x060007, MRA_BANK2				},	// Video Registers? (only 60006.w is read)
	{ 0x080000, 0x080011, blitter_r				},	// Blitter (reading is for debug)
	{ 0x100000, 0x103fff, wecleman_pageram_r	},	// Background Layers
	{ 0x108000, 0x108fff, wecleman_txtram_r		},	// Text Layer
	{ 0x110000, 0x110fff, paletteram_word_r		},	// Palette
	{ 0x124000, 0x127fff, sharedram_r			},	// Shared with sub CPU
	{ 0x130000, 0x130fff, spriteram_word_r		},	// Sprites
	// Input Ports:
	{ 0x140010, 0x140011, wecleman_gear_r		},	// Coins + brake + gear
	{ 0x140012, 0x140013, input_port_1_r		},	// ??
	{ 0x140014, 0x140015, input_port_2_r		},	// DSW
	{ 0x140016, 0x140017, input_port_3_r		},	// DSW
	{ 0x140020, 0x140021, selected_ip_r			},	// Accelerator or Wheel or ..
	{ -1 }
};

static struct MemoryWriteAddress wecleman_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM											},	// ROM (03c000-03ffff used as RAM sometimes!)
	{ 0x040000, 0x043fff, MWA_BANK1											},	// RAM
	{ 0x060000, 0x060007, MWA_BANK2, &wecleman_unknown						},	// Video Registers?
	{ 0x080000, 0x080011, blitter_w, &blitter_regs							},	// Blitter
	{ 0x100000, 0x103fff, wecleman_pageram_w, &wecleman_pageram				},	// Background Layers
	{ 0x108000, 0x108fff, wecleman_txtram_w, &wecleman_txtram				},	// Text Layer
	{ 0x110000, 0x110fff, paletteram_SBGRBBBBGGGGRRRR_word_w, &paletteram	},	// Palette
	{ 0x124000, 0x127fff, sharedram_w, &sharedram							},	// Shared with main CPU
	{ 0x130000, 0x130fff, spriteram_word_w, &spriteram, &spriteram_size		},	// Sprites
	{ 0x140000, 0x140001, wecleman_soundlatch_w						},	// To sound CPU
	{ 0x140002, 0x140003, selected_ip_w								},	// Selects accelerator / wheel / ..
	{ 0x140004, 0x140005, irqctrl_w									},	// Main CPU controls the other CPUs
	{ 0x140006, 0x140007, MWA_NOP									},	// Watchdog reset
	{ 0x140020, 0x140021, MWA_NOP									},	// Paired with writes to $140003
	{ 0x140030, 0x140031, MWA_BANK3									},	// ??
	{ -1 }
};









/***************************************************************************
								Hot Chase
***************************************************************************/

static READ_HANDLER( hotchase_K051316_0_r )
{
	return K051316_0_r(offset >> 1);
}

static READ_HANDLER( hotchase_K051316_1_r )
{
	return K051316_1_r(offset >> 1);
}

static WRITE_HANDLER( hotchase_K051316_0_w )
{
	if ((data & 0x00ff0000) == 0)
		K051316_0_w(offset >> 1, data & 0xff);
}

static WRITE_HANDLER( hotchase_K051316_1_w )
{
	if ((data & 0x00ff0000) == 0)
		K051316_1_w(offset >> 1, data & 0xff);
}

static WRITE_HANDLER( hotchase_K051316_ctrl_0_w )
{
	if ((data & 0x00ff0000) == 0)
		K051316_ctrl_0_w(offset >> 1, data & 0xff);
}

static WRITE_HANDLER( hotchase_K051316_ctrl_1_w )
{
	if ((data & 0x00ff0000) == 0)
		K051316_ctrl_1_w(offset >> 1, data & 0xff);
}


WRITE_HANDLER( hotchase_soundlatch_w );

static struct MemoryReadAddress hotchase_readmem[] =
{
	{ 0x000000, 0x03ffff, MRA_ROM				},	// ROM
	{ 0x040000, 0x063fff, MRA_BANK1				},	// RAM (weird size!?)
	{ 0x080000, 0x080011, MRA_BANK2				},	// Blitter
	{ 0x100000, 0x100fff, hotchase_K051316_0_r },	// Background
	{ 0x102000, 0x102fff, hotchase_K051316_1_r },	// Foreground
	{ 0x110000, 0x111fff, paletteram_word_r		},	// Palette (only the first 2048 colors used)
	{ 0x120000, 0x123fff, sharedram_r			},	// Shared with sub CPU
	{ 0x130000, 0x130fff, spriteram_word_r		},	// Sprites
	// Input Ports:
	{ 0x140006, 0x140007, MRA_NOP				},	// Watchdog reset
	{ 0x140010, 0x140011, hotchase_gear_r		},	// Coins + brake + gear
	{ 0x140012, 0x140013, input_port_1_r		},	// ?? bit 4 from sound cpu
	{ 0x140014, 0x140015, input_port_2_r		},	// DSW 2
	{ 0x140016, 0x140017, input_port_3_r		},	// DSW 1
	{ 0x140020, 0x140021, selected_ip_r			},	// Accelerator or Wheel or ..
//	{ 0x140022, 0x140023, MRA_NOP				},	// ??
	{ -1 }
};

static struct MemoryWriteAddress hotchase_writemem[] =
{
	{ 0x000000, 0x03ffff, MWA_ROM								},	// ROM
	{ 0x040000, 0x063fff, MWA_BANK1								},	// RAM (weird size!?)
	{ 0x080000, 0x080011, blitter_w, &blitter_regs				},	// Blitter
	{ 0x100000, 0x100fff, hotchase_K051316_0_w },		// Background
	{ 0x101000, 0x10101f, hotchase_K051316_ctrl_0_w },	// Background Ctrl
	{ 0x102000, 0x102fff, hotchase_K051316_1_w },		// Foreground
	{ 0x103000, 0x10301f, hotchase_K051316_ctrl_1_w },	// Foreground Ctrl
	{ 0x110000, 0x111fff, paletteram_SBGRBBBBGGGGRRRR_word_w, &paletteram	},	// Palette
	{ 0x120000, 0x123fff, sharedram_w, &sharedram							},	// Shared with sub CPU
	{ 0x130000, 0x130fff, spriteram_word_w, &spriteram, &spriteram_size		},	// Sprites
	// Input Ports:
	{ 0x140000, 0x140001, hotchase_soundlatch_w					},	// To sound CPU
	{ 0x140002, 0x140003, selected_ip_w							},	// Selects accelerator / wheel / ..
	{ 0x140004, 0x140005, irqctrl_w								},	// Main CPU controls the other CPUs
	{ 0x140020, 0x140021, MWA_NOP								},	// Paired with writes to $140003
//	{ 0x140030, 0x140031, MWA_NOP								},	// ??
	{ -1 }
};






/*
**
**	Sub cpu data
**
**
*/

/***************************************************************************
								WEC Le Mans 24
***************************************************************************/

static struct MemoryReadAddress wecleman_sub_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM		},	// ROM
	{ 0x060000, 0x060fff, MRA_BANK8		},	// Road
	{ 0x070000, 0x073fff, &sharedram_r	},	// RAM (Shared with main CPU)
	{ -1 }
};

static struct MemoryWriteAddress wecleman_sub_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM		},	// ROM
	{ 0x060000, 0x060fff, MWA_BANK8, &wecleman_roadram, &wecleman_roadram_size },	// Road
	{ 0x070000, 0x073fff, sharedram_w	},	// RAM (Shared with main CPU)
	{ -1 }
};












/***************************************************************************
								Hot Chase
***************************************************************************/


static struct MemoryReadAddress hotchase_sub_readmem[] =
{
	{ 0x000000, 0x01ffff, MRA_ROM		},	// ROM
	{ 0x020000, 0x020fff, MRA_BANK7		},	// Road
	{ 0x060000, 0x060fff, MRA_BANK8		},	// RAM
	{ 0x040000, 0x043fff, &sharedram_r	},	// Shared with main CPU
	{ -1 }
};

static struct MemoryWriteAddress hotchase_sub_writemem[] =
{
	{ 0x000000, 0x01ffff, MWA_ROM		},	// ROM
	{ 0x020000, 0x020fff, MWA_BANK7, &wecleman_roadram, &wecleman_roadram_size },	// Road
	{ 0x060000, 0x060fff, MWA_BANK8		},	// RAM
	{ 0x040000, 0x043fff, sharedram_w	},	// Shared with main CPU
	{ -1 }
};










/*
**
**	Sound cpu data
**
**
*/

/***************************************************************************
								WEC Le Mans 24
***************************************************************************/

/* 140001.b */
WRITE_HANDLER( wecleman_soundlatch_w )
{
	soundlatch_w(0,data & 0xFF);
	cpu_set_irq_line(2,0, HOLD_LINE);
}


/* Protection - an external multiplyer connected to the sound CPU */
READ_HANDLER( multiply_r )
{
	return (multiply_reg[0] * multiply_reg[1]) & 0xFF;
}
WRITE_HANDLER( multiply_w )
{
	multiply_reg[offset] = data;
}


/*	K007232 registers reminder:

[Ch A]	[Ch B]		[Meaning]
00		06			address step	(low  byte)
01		07			address step	(high byte, max 1)
02		08			sample address	(low  byte)
03		09			sample address	(mid  byte)
04		0a			sample address	(high byte, max 1 -> max rom size: $20000)
05		0b			Reading this byte triggers the sample

[Ch A & B]
0c					volume
0d					play sample once or looped (2 channels -> 2 bits (0&1))

** sample playing ends when a byte with bit 7 set is reached **/

WRITE_HANDLER( wecleman_K007232_bank_w )
{
	K007232_bankswitch(0, memory_region(REGION_SOUND1),
						  memory_region((data & 1) ? REGION_SOUND1 : REGION_SOUND2) );
}

static struct MemoryReadAddress wecleman_sound_readmem[] =
{
	{ 0x0000, 0x7fff, MRA_ROM					},	// ROM
	{ 0x8000, 0x83ff, MRA_RAM					},	// RAM
	{ 0x9000, 0x9000, multiply_r				},	// Protection
	{ 0xa000, 0xa000, soundlatch_r				},	// From main CPU
	{ 0xb000, 0xb00d, K007232_read_port_0_r		},	// K007232 (Reading offset 5/b triggers the sample)
	{ 0xc001, 0xc001, YM2151_status_port_0_r	},	// YM2151
	{ -1 }
};

static struct MemoryWriteAddress wecleman_sound_writemem[] =
{
	{ 0x0000, 0x7fff, MWA_ROM					},	// ROM
	{ 0x8000, 0x83ff, MWA_RAM					},	// RAM
//	{ 0x8500, 0x8500, MWA_NOP					},	// incresed with speed (global volume)?
	{ 0x9000, 0x9001, multiply_w				},	// Protection
//	{ 0x9006, 0x9006, MWA_NOP					},	// ?
	{ 0xb000, 0xb00d, K007232_write_port_0_w	},	// K007232
	{ 0xc000, 0xc000, YM2151_register_port_0_w	},	// YM2151
	{ 0xc001, 0xc001, YM2151_data_port_0_w		},
	{ 0xf000, 0xf000, wecleman_K007232_bank_w	},	// Samples banking
	{ -1 }
};


/***************************************************************************
								Hot Chase
***************************************************************************/

/* 140001.b */
WRITE_HANDLER( hotchase_soundlatch_w )
{
	soundlatch_w(0,data & 0xFF);
	cpu_set_irq_line(2,M6809_IRQ_LINE, HOLD_LINE);
}

static struct K007232_interface hotchase_k007232_interface =
{
	3,
	{ REGION_SOUND1, REGION_SOUND2, REGION_SOUND3 },
	{ K007232_VOL( 33,MIXER_PAN_CENTER, 33,MIXER_PAN_CENTER ),
	  K007232_VOL( 33,MIXER_PAN_LEFT,   33,MIXER_PAN_RIGHT  ),
	  K007232_VOL( 33,MIXER_PAN_LEFT,   33,MIXER_PAN_RIGHT  ) },
	{ 0,0,0 }
};

WRITE_HANDLER( hotchase_sound_control_w )
{
	int reg[8];

	reg[offset] = data;

	switch (offset)
	{
		case 0x0:	/* Change volume of voice A (l&r speaker at once) */
		case 0x2:	/* for 3 chips.. */
		case 0x4:
								// chip, channel (l/r), volA, volB
			K007232_set_volume( offset / 2,	0, (data >> 4) * 0x11, (reg[offset^1] >> 4) * 0x11);
			K007232_set_volume( offset / 2,	1, (data & 15) * 0x11, (reg[offset^1] & 15) * 0x11);
			break;

		case 0x1:	/* Change volume of voice B (l&r speaker at once) */
		case 0x3:	/* for 3 chips.. */
		case 0x5:
								// chip, channel (l/r), volA, volB
			K007232_set_volume( offset / 2,	0, (reg[offset^1] >> 4) * 0x11, (data >> 4) * 0x11);
			K007232_set_volume( offset / 2,	1, (reg[offset^1] & 15) * 0x11, (data & 15) * 0x11);
			break;

		case 0x06:	/* Bankswitch for chips 0 & 1 */
		{
			unsigned char *RAM0 = memory_region(hotchase_k007232_interface.bank[0]);
			unsigned char *RAM1 = memory_region(hotchase_k007232_interface.bank[1]);

			int bank0_a = (data >> 1) & 1;
			int bank1_a = (data >> 2) & 1;
			int bank0_b = (data >> 3) & 1;
			int bank1_b = (data >> 4) & 1;
			// bit 6: chip 2 - ch0 ?
			// bit 7: chip 2 - ch1 ?

			K007232_bankswitch(0, &RAM0[bank0_a*0x20000], &RAM0[bank0_b*0x20000]);
			K007232_bankswitch(1, &RAM1[bank1_a*0x20000], &RAM1[bank1_b*0x20000]);
		}
		break;

		case 0x07:	/* Bankswitch for chip 2 */
		{
			unsigned char *RAM2 = memory_region(hotchase_k007232_interface.bank[2]);

			int bank2_a = (data >> 0) & 7;
			int bank2_b = (data >> 3) & 7;

			K007232_bankswitch(2, &RAM2[bank2_a*0x20000], &RAM2[bank2_b*0x20000]);
		}
		break;
	}
}


/* Read and write handlers for one K007232 chip:
   even and odd register are mapped swapped */

#define HOTCHASE_K007232_RW(_chip_) \
READ_HANDLER( hotchase_K007232_##_chip_##_r ) \
{ \
	return K007232_read_port_##_chip_##_r(offset ^ 1); \
} \
WRITE_HANDLER( hotchase_K007232_##_chip_##_w ) \
{ \
	K007232_write_port_##_chip_##_w(offset ^ 1, data); \
} \

/* 3 x K007232 */
HOTCHASE_K007232_RW(0)
HOTCHASE_K007232_RW(1)
HOTCHASE_K007232_RW(2)



static struct MemoryReadAddress hotchase_sound_readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM					},	// RAM
	{ 0x1000, 0x100d, hotchase_K007232_0_r		},	// 3 x  K007232
	{ 0x2000, 0x200d, hotchase_K007232_1_r		},
	{ 0x3000, 0x300d, hotchase_K007232_2_r		},
	{ 0x6000, 0x6000, soundlatch_r				},	// From main CPU (Read on IRQ)
	{ 0x8000, 0xffff, MRA_ROM					},	// ROM
	{ -1 }
};

static struct MemoryWriteAddress hotchase_sound_writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM					},	// RAM
	{ 0x1000, 0x100d, hotchase_K007232_0_w		},	// 3 x K007232
	{ 0x2000, 0x200d, hotchase_K007232_1_w		},
	{ 0x3000, 0x300d, hotchase_K007232_2_w		},
	{ 0x4000, 0x4007, hotchase_sound_control_w	},	// Sound volume, banking, etc.
	{ 0x5000, 0x5000, MWA_NOP					},	// ? (written with 0 on IRQ, 1 on FIRQ)
	{ 0x7000, 0x7000, MWA_NOP					},	// Command acknowledge ?
	{ 0x8000, 0xffff, MWA_ROM					},	// ROM
	{ -1 }
};





/***************************************************************************

							Input Ports

***************************************************************************/

// Fake input port to read the status of the four buttons
// Used to implement both the accelerator and the shift using 2 buttons

#define BUTTONS_STATUS \
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) \
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) \
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 ) \
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON4 )




#define DRIVING_WHEEL \
 PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X | IPF_CENTER, 50, 5, 0, 0xff)




#define CONTROLS_AND_COINS(_default_) \
	PORT_BIT(  0x01, _default_, IPT_COIN1   ) \
	PORT_BIT(  0x02, _default_, IPT_COIN2   ) \
	PORT_BITX( 0x04, _default_, IPT_SERVICE, DEF_STR( Service_Mode ), KEYCODE_F2, IP_JOY_NONE ) \
	PORT_BIT(  0x08, _default_, IPT_COIN3   )					/* Called "service" */ \
	PORT_BIT(  0x10, _default_, IPT_START1  )					/* Start */ \
/*	PORT_BIT(  0x20, _default_, IPT_BUTTON3 | IPF_TOGGLE ) */	/* Shift (we handle this with 2 buttons) */ \
	PORT_BIT(  0x40, _default_, IPT_BUTTON2 )					/* Brake */ \
	PORT_BIT(  0x80, _default_, IPT_UNKNOWN )					/* ? */


/***************************************************************************
								WEC Le Mans 24
***************************************************************************/

INPUT_PORTS_START( wecleman )

	PORT_START      /* IN0 - Controls and Coins - $140011.b */
	CONTROLS_AND_COINS(IP_ACTIVE_HIGH)

	PORT_START      /* IN1 - Motor? - $140013.b */
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_UNKNOWN )	// ? right sw
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_UNKNOWN )	// ? left  sw
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_UNKNOWN )	// ? thermo
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )	// ? from sound cpu ?
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_UNKNOWN )

	PORT_START	/* IN2 - DSW A (Coinage) - $140015.b */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Unknown ) )

	PORT_START	/* IN3 - DSW B (options) - $140017.b */
	PORT_DIPNAME( 0x01, 0x01, "Speed Unit" )
	PORT_DIPSETTING(    0x01, "Km/h" )
	PORT_DIPSETTING(    0x00, "mph" )
	PORT_DIPNAME( 0x02, 0x02, "Unknown B-1" )	// single
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown B-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x18, "Easy" )			// 66 seconds at the start
	PORT_DIPSETTING(    0x10, "Normal" )		// 64
	PORT_DIPSETTING(    0x08, "Hard" )			// 62
	PORT_DIPSETTING(    0x00, "Hardest" )		// 60
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Unknown B-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown B-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN4 - Fake input port - Buttons status */
	BUTTONS_STATUS

	PORT_START	/* IN5 - Driving Wheel - $140021.b (2) */
	DRIVING_WHEEL

INPUT_PORTS_END













/***************************************************************************
								Hot Chase
***************************************************************************/

INPUT_PORTS_START( hotchase )

	PORT_START      /* IN0 - Controls and Coins - $140011.b */
	CONTROLS_AND_COINS(IP_ACTIVE_LOW)

	PORT_START      /* IN1 - Motor? - $140013.b */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )	// ? right sw
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	// ? left  sw
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	// ? thermo
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNKNOWN )	// ? from sound cpu ?
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 - DSW 2 (options) - $140015.b */
	PORT_DIPNAME( 0x01, 0x01, "Unknown 2-0" )	// single
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Unknown 2-1" )	// single (wheel related)
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Unknown 2-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, "Unknown 2-3&4" )
	PORT_DIPSETTING(    0x18, "0" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x08, "8" )
	PORT_DIPSETTING(    0x00, "c" )
	PORT_DIPNAME( 0x20, 0x20, "Unknown 2-5" )	// single
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
/* wheel <-> brake ; accel -> start */
	PORT_DIPNAME( 0x40, 0x40, "Unknown 2-6" )	// single (wheel<->brake)
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Unknown 2-7" )	// single
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* IN3 - DSW 1 (Coinage) - $140017.b */
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x00, "1 Coin/99 Credits" )
//	PORT_DIPSETTING(    0x40, "0C_0C" )	// Coin B insertion freezes the game!

	PORT_START	/* IN4 - Fake input port - Buttons status */
	BUTTONS_STATUS

	PORT_START	/* IN5 - Driving Wheel - $140021.b (2) */
	DRIVING_WHEEL

INPUT_PORTS_END









/***************************************************************************

								Graphics Layout

***************************************************************************/


/***************************************************************************
								WEC Le Mans 24
***************************************************************************/

static struct GfxLayout wecleman_bg_layout =
{
	8,8,
	8*0x8000*3/(8*8*3),
	3,
	{ 0,0x8000*8,0x8000*8*2 },
	{0,7,6,5,4,3,2,1},
	{0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8},
	8*8
};

/* We draw the road, made of 512 pixel lines, using 64x1 tiles */
static struct GfxLayout wecleman_road_layout =
{
	64,1,
	8*0x4000*3/(64*1*3),
	3,
	{ 0x4000*8*2,0x4000*8*1,0x4000*8*0 },
	{0,7,6,5,4,3,2,1,
	 8,15,14,13,12,11,10,9,
	 16,23,22,21,20,19,18,17,
	 24,31,30,29,28,27,26,25,

	 0+32,7+32,6+32,5+32,4+32,3+32,2+32,1+32,
	 8+32,15+32,14+32,13+32,12+32,11+32,10+32,9+32,
	 16+32,23+32,22+32,21+32,20+32,19+32,18+32,17+32,
	 24+32,31+32,30+32,29+32,28+32,27+32,26+32,25+32},
	{0},
	64*1
};


static struct GfxDecodeInfo wecleman_gfxdecodeinfo[] =
{
//	  REGION_GFX1 holds sprite, which are not decoded here
	{ REGION_GFX2, 0, &wecleman_bg_layout,   0, 2048/8 }, // [0] bg + fg + txt
	{ REGION_GFX3, 0, &wecleman_road_layout, 0, 2048/8 }, // [1] road
	{ -1 }
};



/***************************************************************************
								Hot Chase
***************************************************************************/


/* We draw the road, made of 512 pixel lines, using 64x1 tiles */
static struct GfxLayout hotchase_road_layout =
{
	64,1,
	8*0x20000/(64*1*4),
	4,
	{ 0, 1, 2, 3 },
	{0*4,1*4,2*4,3*4,4*4,5*4,6*4,7*4,
	 8*4,9*4,10*4,11*4,12*4,13*4,14*4,15*4,
	 16*4,17*4,18*4,19*4,20*4,21*4,22*4,23*4,
	 24*4,25*4,26*4,27*4,28*4,29*4,30*4,31*4,

	 32*4,33*4,34*4,35*4,36*4,37*4,38*4,39*4,
	 40*4,41*4,42*4,43*4,44*4,45*4,46*4,47*4,
	 48*4,49*4,50*4,51*4,52*4,53*4,54*4,55*4,
	 56*4,57*4,58*4,59*4,60*4,61*4,62*4,63*4},
	{0},
	64*1*4
};


static struct GfxDecodeInfo hotchase_gfxdecodeinfo[] =
{
//	REGION_GFX1 holds sprite, which are not decoded here
//	REGION_GFX2 and 3 are for the 051316
	{ REGION_GFX4, 0, &hotchase_road_layout, 0x70*16, 16 },	// road
	{ -1 }
};




/***************************************************************************
								WEC Le Mans 24
***************************************************************************/




static int wecleman_interrupt( void )
{
	if (cpu_getiloops() == 0)	return 4;	/* once */
	else						return 5;	/* to read input ports */
}


static struct YM2151interface ym2151_interface =
{
	1,
	3579545,	/* same as sound cpu */
	{ 80 },
	{ 0  }
};



static struct K007232_interface wecleman_k007232_interface =
{
	1,
	{ REGION_SOUND1 },	/* but the 2 channels use different ROMs !*/
	{ K007232_VOL( 20,MIXER_PAN_LEFT, 20,MIXER_PAN_RIGHT ) },
	{0}
};



void wecleman_init_machine(void)
{
	K007232_bankswitch(0,	memory_region(REGION_SOUND1), /* the 2 channels use different ROMs */
							memory_region(REGION_SOUND2) );
}


static struct MachineDriver machine_driver_wecleman =
{
	{
		{
			CPU_M68000,
			10000000,		/* Schems show 10MHz */
			wecleman_readmem,wecleman_writemem,0,0,
			wecleman_interrupt, 5 + 1,	/* in order to read the inputs once per frame */
		},
		{
			CPU_M68000,
			10000000,		/* Schems show 10MHz */
			wecleman_sub_readmem,wecleman_sub_writemem,0,0,
			ignore_interrupt,1,		/* lev 4 irq generated by main CPU */
		},
		{
/* Schems: can be reset, no nmi, soundlatch, 3.58MHz */
			CPU_Z80 | CPU_AUDIO_CPU,
			3579545,
			wecleman_sound_readmem,wecleman_sound_writemem,0,0,
			ignore_interrupt,1, /* irq caused by main cpu */
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	wecleman_init_machine,

	/* video hardware */
	320, 224, { 0, 320-1, 0, 224-1 },

	wecleman_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	wecleman_vh_start,
	0,
	wecleman_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2151,
			&ym2151_interface
		},
		{
			SOUND_K007232,
			&wecleman_k007232_interface
		},
	}
};





/***************************************************************************
								Hot Chase
***************************************************************************/



void hotchase_init_machine(void)		{						}
int  hotchase_interrupt( void )			{return 4;				}
int  hotchase_sound_interrupt(void)		{return M6809_INT_FIRQ;	}

static struct MachineDriver machine_driver_hotchase =
{
	{
		{
			CPU_M68000,
			10000000,		/* 10 MHz - PCB is drawn in one set's readme */
			hotchase_readmem,hotchase_writemem,0,0,
			hotchase_interrupt,1,
		},
		{
			CPU_M68000,
			10000000,		/* 10 MHz - PCB is drawn in one set's readme */
			hotchase_sub_readmem,hotchase_sub_writemem,0,0,
			ignore_interrupt,1,		/* lev 4 irq generated by main CPU */
		},
		{
			CPU_M6809 | CPU_AUDIO_CPU,
			3579545,		/* 3.579 MHz - PCB is drawn in one set's readme */
			hotchase_sound_readmem,hotchase_sound_writemem,0,0,
			hotchase_sound_interrupt,8, /* FIRQ, while IRQ is caused by main cpu */
										/* Amuse: every 2 ms */
		},
	},
	60,DEFAULT_60HZ_VBLANK_DURATION,
	1,
	hotchase_init_machine,

	/* video hardware */
	320, 224, { 0, 320-1, 0, 224-1 },

	hotchase_gfxdecodeinfo,
	2048, 2048,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	hotchase_vh_start,
	hotchase_vh_stop,
	hotchase_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_K007232,
			&hotchase_k007232_interface
		}
	}
};



/***************************************************************************

								ROMs Loading

***************************************************************************/



/***************************************************************************
								WEC Le Mans 24
***************************************************************************/

ROM_START( wecleman )

	ROM_REGION( 0x40000, REGION_CPU1 )		/* Main CPU Code */
	ROM_LOAD_EVEN( "602f08.17h", 0x00000, 0x10000, 0x493b79d3 )
	ROM_LOAD_ODD ( "602f11.23h", 0x00000, 0x10000, 0x6bb4f1fa )
	ROM_LOAD_EVEN( "602a09.18h", 0x20000, 0x10000, 0x8a9d756f )
	ROM_LOAD_ODD ( "602a10.22h", 0x20000, 0x10000, 0x569f5001 )

	ROM_REGION( 0x10000, REGION_CPU2 )		/* Sub CPU Code */
	ROM_LOAD_EVEN( "602a06.18a", 0x00000, 0x08000, 0xe12c0d11 )
	ROM_LOAD_ODD(  "602a07.20a", 0x00000, 0x08000, 0x47968e51 )

	ROM_REGION( 0x10000, REGION_CPU3 )		/* Sound CPU Code */
	ROM_LOAD( "602a01.6d",  0x00000, 0x08000, 0xdeafe5f1 )

	ROM_REGION( 0x200000 * 2, REGION_GFX1 )	/* x2, do not dispose */
	ROM_LOAD( "602a25.12e", 0x000000, 0x20000, 0x0eacf1f9 )	// zooming sprites
	ROM_LOAD( "602a26.14e", 0x020000, 0x20000, 0x2182edaf )
	ROM_LOAD( "602a27.15e", 0x040000, 0x20000, 0xb22f08e9 )
	ROM_LOAD( "602a28.17e", 0x060000, 0x20000, 0x5f6741fa )
	ROM_LOAD( "602a21.6e",  0x080000, 0x20000, 0x8cab34f1 )
	ROM_LOAD( "602a22.7e",  0x0a0000, 0x20000, 0xe40303cb )
	ROM_LOAD( "602a23.9e",  0x0c0000, 0x20000, 0x75077681 )
	ROM_LOAD( "602a24.10e", 0x0e0000, 0x20000, 0x583dadad )
	ROM_LOAD( "602a17.12c", 0x100000, 0x20000, 0x31612199 )
	ROM_LOAD( "602a18.14c", 0x120000, 0x20000, 0x3f061a67 )
	ROM_LOAD( "602a19.15c", 0x140000, 0x20000, 0x5915dbc5 )
	ROM_LOAD( "602a20.17c", 0x160000, 0x20000, 0xf87e4ef5 )
	ROM_LOAD( "602a13.6c",  0x180000, 0x20000, 0x5d3589b8 )
	ROM_LOAD( "602a14.7c",  0x1a0000, 0x20000, 0xe3a75f6c )
	ROM_LOAD( "602a15.9c",  0x1c0000, 0x20000, 0x0d493c9f )
	ROM_LOAD( "602a16.10c", 0x1e0000, 0x20000, 0xb08770b3 )

	ROM_REGION( 0x18000, REGION_GFX2 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "602a31.26g", 0x000000, 0x08000, 0x01fa40dd )	// layers
	ROM_LOAD( "602a30.24g", 0x008000, 0x08000, 0xbe5c4138 )
	ROM_LOAD( "602a29.23g", 0x010000, 0x08000, 0xf1a8d33e )

	ROM_REGION( 0x0c000, REGION_GFX3 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "602a04.11e", 0x000000, 0x08000, 0xade9f359 ) // road
	ROM_LOAD( "602a05.13e", 0x008000, 0x04000, 0xf22b7f2b )

	ROM_REGION( 0x20000, REGION_SOUND1 )	/* Samples (Channel A) */
	ROM_LOAD( "602a03.10a", 0x00000, 0x20000, 0x31392b01 )

	ROM_REGION( 0x20000, REGION_SOUND2 )	/* Samples (Channel B) */
	ROM_LOAD( "602a02.8a",  0x00000, 0x20000, 0xe2be10ae )

	ROM_REGION( 0x04000, REGION_USER1 )
	ROM_LOAD( "602a12.1a",  0x000000, 0x04000, 0x77b9383d )	// ??

ROM_END



void wecleman_unpack_sprites(void)
{
	const int region		=	REGION_GFX1;	// sprites

	const unsigned int len	=	memory_region_length(region);
	unsigned char *src		=	memory_region(region) + len / 2 - 1;
	unsigned char *dst		=	memory_region(region) + len - 1;

	while(dst > src)
	{
		unsigned char data = *src--;
		if( (data&0xf0) == 0xf0 ) data &= 0x0f;
		if( (data&0x0f) == 0x0f ) data &= 0xf0;
		*dst-- = data & 0xF;	*dst-- = data >> 4;
	}
}



/* Unpack sprites data and do some patching */
void init_wecleman(void)
{
	unsigned char *RAM;
	int i;

/* Optional code patches */

	/* Main CPU patches */
	RAM = memory_region(REGION_CPU1);
//	WRITE_WORD (&RAM[0x08c2],0x601e);	// faster self test

	/* Sub CPU patches */
	RAM = memory_region(REGION_CPU2);

	/* Sound CPU patches */
	RAM = memory_region(REGION_CPU3);


/* Decode GFX Roms - Compensate for the address lines scrambling */

	/*	Sprites - decrypting the sprites nearly KILLED ME!
		It's been the main cause of the delay of this driver ...
		I hope you'll appreciate this effort!	*/

	/* let's swap even and odd *pixels* of the sprites */
	RAM = memory_region(REGION_GFX1);
	for (i = 0; i < memory_region_length(REGION_GFX1); i ++)
	{
		int x = RAM[i];
		/* TODO: could be wrong, colors have to be fixed.       */
		/* The only certain thing is that 87 must convert to f0 */
		/* otherwise stray lines appear, made of pens 7 & 8     */
		x = ((x & 0x07) << 5) | ((x & 0xf8) >> 3);
		RAM[i] = x;
	}

	BITSWAP(memory_region(REGION_GFX1), memory_region_length(REGION_GFX1),
			0,1,20,19,18,17,14,9,16,6,4,7,8,15,10,11,13,5,12,3,2)

	/* Now we can unpack each nibble of the sprites into a pixel (one byte) */
	wecleman_unpack_sprites();



	/* Bg & Fg & Txt */
	BITSWAP(memory_region(REGION_GFX2), memory_region_length(REGION_GFX2),
			20,19,18,17,16,15,12,7,14,4,2,5,6,13,8,9,11,3,10,1,0);



	/* Road */
	BITSWAP(memory_region(REGION_GFX3), memory_region_length(REGION_GFX3),
			20,19,18,17,16,15,14,7,12,4,2,5,6,13,8,9,11,3,10,1,0);
}





/***************************************************************************
								Hot Chase
***************************************************************************/


ROM_START( hotchase )
	ROM_REGION( 0x40000, REGION_CPU1 )			/* Main Code */
	ROM_LOAD_EVEN( "763k05", 0x000000, 0x010000, 0xf34fef0b )
	ROM_LOAD_ODD ( "763k04", 0x000000, 0x010000, 0x60f73178 )
	ROM_LOAD_EVEN( "763k03", 0x020000, 0x010000, 0x28e3a444 )
	ROM_LOAD_ODD ( "763k02", 0x020000, 0x010000, 0x9510f961 )

	ROM_REGION( 0x20000, REGION_CPU2 )			/* Sub Code */
	ROM_LOAD_EVEN( "763k07", 0x000000, 0x010000, 0xae12fa90 )
	ROM_LOAD_ODD ( "763k06", 0x000000, 0x010000, 0xb77e0c07 )

	ROM_REGION( 0x10000, REGION_CPU3 )			/* Sound Code */
	ROM_LOAD( "763f01", 0x8000, 0x8000, 0x4fddd061 )

	ROM_REGION( 0x300000 * 2, REGION_GFX1 )	/* x2, do not dispose */
	ROM_LOAD( "763e17", 0x000000, 0x080000, 0x8db4e0aa )	// zooming sprites
	ROM_LOAD( "763e20", 0x080000, 0x080000, 0xa22c6fce )
	ROM_LOAD( "763e18", 0x100000, 0x080000, 0x50920d01 )
	ROM_LOAD( "763e21", 0x180000, 0x080000, 0x77e0e93e )
	ROM_LOAD( "763e19", 0x200000, 0x080000, 0xa2622e56 )
	ROM_LOAD( "763e22", 0x280000, 0x080000, 0x967c49d1 )

	ROM_REGION( 0x20000, REGION_GFX2 )
	ROM_LOAD( "763e14", 0x000000, 0x020000, 0x60392aa1 )	// bg

	ROM_REGION( 0x10000, REGION_GFX3 )
	ROM_LOAD( "763a13", 0x000000, 0x010000, 0x8bed8e0d )	// fg (patched)

	ROM_REGION( 0x20000, REGION_GFX4 | REGIONFLAG_DISPOSE )
	ROM_LOAD( "763e15", 0x000000, 0x020000, 0x7110aa43 )	// road

	ROM_REGION( 0x40000, REGION_SOUND1 )		/* Samples */
	ROM_LOAD( "763e11", 0x000000, 0x040000, 0x9d99a5a7 )	// 2 banks

	ROM_REGION( 0x40000, REGION_SOUND2 )		/* Samples */
	ROM_LOAD( "763e10", 0x000000, 0x040000, 0xca409210 )	// 2 banks

	ROM_REGION( 0x100000, REGION_SOUND3 )		/* Samples */
	ROM_LOAD( "763e08", 0x000000, 0x080000, 0x054a9a63 )	// 4 banks
	ROM_LOAD( "763e09", 0x080000, 0x080000, 0xc39857db )	// 4 banks

	ROM_REGION( 0x08000, REGION_USER1 )
	ROM_LOAD( "763a12", 0x000000, 0x008000, 0x05f1e553 )	// ??
ROM_END




/*	Important: you must leave extra space when listing sprite ROMs
	in a ROM module definition.  This routine unpacks each sprite nibble
	into a byte, doubling the memory consumption. */

void hotchase_sprite_decode( int num_banks, int bank_size )
{
	unsigned char *base, *temp;
	int i;

	base = memory_region(REGION_GFX1);	// sprites
	temp = (unsigned char*)malloc( bank_size );
	if( !temp ) return;

	for( i = num_banks; i >0; i-- ){
		unsigned char *finish	= base + 2*bank_size*i;
		unsigned char *dest 	= finish - 2*bank_size;

		unsigned char *p1 = temp;
		unsigned char *p2 = temp+bank_size/2;

		unsigned char data;

		memcpy (temp, base+bank_size*(i-1), bank_size);

		do {
			data = *p1++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;
			data = *p1++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;


			data = *p2++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;
			data = *p2++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;
		} while( dest<finish );
	}
	free( temp );
}




/* Unpack sprites data and do some patching */
void init_hotchase(void)
{
	unsigned char *RAM;
	int i;

/* Optional code patches */

	/* Main CPU patches */
	RAM = memory_region(REGION_CPU1);
	WRITE_WORD (&RAM[0x1140],0x0015);	WRITE_WORD (&RAM[0x195c],0x601A);	// faster self test

	/* Sub CPU patches */
	RAM = memory_region(REGION_CPU2);

	/* Sound CPU patches */
	RAM = memory_region(REGION_CPU3);


/* Decode GFX Roms */

	/* Let's swap even and odd bytes of the sprites gfx roms */
	RAM = memory_region(REGION_GFX1);
	for (i = 0; i < memory_region_length(REGION_GFX1); i += 2)
	{
		int x = RAM[i];
		RAM[i] = RAM[i+1];
		RAM[i+1] = x;
	}

	/* Now we can unpack each nibble of the sprites into a pixel (one byte) */
	hotchase_sprite_decode(3,0x80000*2);	// num banks, bank len


	/* Let's copy the second half of the fg layer gfx (charset) over the first */
	RAM = memory_region(REGION_GFX3);
	memcpy(&RAM[0], &RAM[0x10000/2], 0x10000/2);

}



/***************************************************************************

								Game driver(s)

***************************************************************************/

GAMEX(1986, wecleman, 0, wecleman, wecleman, wecleman, ROT0, "Konami", "WEC Le Mans 24", GAME_WRONG_COLORS )
GAME( 1988, hotchase, 0, hotchase, hotchase, hotchase, ROT0, "Konami", "Hot Chase" )

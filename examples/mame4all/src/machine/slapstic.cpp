/*************************************************************************

  Atari Slapstic decoding helper

*************************************************************************

Slapstic FAQ - ver 0.2
Frank Palazzolo
03/13/98

(Please send comments or corrections
 to me at palazzol@home.com)
----------------------------------

0) Credits:

I would like to thank the Motorola 68HC11 Evaluation
Board.  Anything is possible with one of these :)

Special Thanks to:
Aaron Giles, Mike Balfour, and Tim Lindquist, and the rest of
the MAME Team

1) What is a slapstic?

The slapstic was a chip made by Atari, which was used for
bank-switching and security in some coin-operated video games.

2) What games used one?

Empire Strikes Back
Tetris
Gauntlet
Gauntlet II
Marble Madness
Peter Packrat
Indiana Jones & the Temple of Doom
Road Runner
Road Blasters
Paperboy
APB
720 Degrees
Super Sprint
Championship Sprint
Rampart
Vindicators Part II
Xybots
Race Drivin'

3) What is the pinout?

(It is a small 20 pin DIP package)

Pin #	Function
----------------------
1	A10 (I)
2	A11 (I)
3	A12 (I)
4	A13 (I)
5	A14 (I)
6	~CS (I)
7	CLK (I)
8	+5V
9	BS1 (O)
10	BS0 (O)
11	Gnd
12	A1 (I)
13	A2 (I)
14	A3 (I)
15	A4 (I)
16	A5 (I)
17	A6 (I)
18	A7 (I)
19	A8 (I)
20	A9 (I)

4) What did it do / How did it work?

This chip would sit on the address bus of a game CPU, and control which one
of 4 possible banks of ROM would be selected into a given memory area based on
a combination of accesses to that memory region.

Note1: All addresses are relative to the Slapstic's memory space, not necessarily the game's
Note2: Reset Address = $0000 for all chips
Note3: Addresses with a * next to them have been deduced, but not verified on real hardware

Chip #     Game                Bank Select Addresses      Disable Mask Ignore Mask   Secondary    Secondary Bank Select
                                (0)    (1)    (2)    (3)  (MS 10 bits) (LS 7 bits)    Enable    (0)    (1)    (2)    (3)
----------------------------------------------------------------------------------------------------------------------------
137412-101 ESB/Tetris          $0080, $0090, $00a0, $00b0    $1540        $00??       *$1dfe  *$1b5c *$1b5d *$1b5e *$1b5f
137412-103 Marble Madness      $0040, $0050, $0060, $0070    $34c0        $002d        $3d14   $3d24, $3d25, $3d26, $3d27
137412-104 Gauntlet            $0020, $0028, $0030, $0038    $3d90        $0069       *$3735  *$3764,*$3765,*$3766,*$3767
137412-105 Indiana Jones &     $0010, $0014, $0018, $001c    $35b0        $003d        $0092,  $00a4, $00a5, $00a6, $00a7
           Paperboy
137412-106 Gauntlet II         $0008, $000a, $000c, $000e    $3da0        $002b       *$0052  *$0064,*$0065,*$0066,*$0067
137412-107 Peter Packrat &     $0018, $001a, $001c, $001e    $00a0        $006b        $3d52,  $3d64, $3d65, $3d66, $3d67
           Xybots &
           2-player Gauntlet &
           720 Degrees
137412-108 Road Runner &       $0028, $002a, $002c, $002e    $0060        $001f        $????   $????, $????, $????, $????
           Super Sprint
137412-109 Championship Sprint $0008, $000a, $000c, $000e    $3da0        $002b        $0052   $0064, $0065, $0066, $0067
137412-110 Road Blasters &     $0040, $0050, $0060, $0070    $34c0        $002d        $3d14   $3d24, $3d25, $3d26, $3d27
           APB
137412-111 Pit Fighter         $0042, $0052, $0062, $0072    $???0        $000a
137412-116 Hydra &             $0044, $004c, $0054, $005c    $???0        $0069
           Cyberball 2072 Tournament
137412-117 Race Drivin'
137412-118 Vindicators Part II $0014, $0034, $0054, $0074    $???0        $0002       *$1950  *$1958,*$1960,*$1968,*$1970
           & Rampart

Surprisingly, the slapstic appears to have used DRAM cells to store
the current bank.  After 5 or 6 seconds without a clock, the chip reverts
to bank 3, with the chip reset (bank select addresses are enabled)
Typically, the slapstic region is accessed often enough to cause a
problem.

"Normal" operating mode of this chip is something like this:

Access $0000 (bank switching is now enabled for one switch)
Access $xxxx ... (n times)
Access $0008 (switch to bank 0 command)
Access $1234 (During the access to location $1234,
              the bank will be switched to 0, on the
              rising edge of the clock pulse.

Note1: It appears that the access during the bank switch
       comes from the new bank, but this may depend on
       the processor inteface in question.
Note2: The "reset" state of the part is equivalent to the part having
       been accessed at location $0

The details of the state machine representing the chip are
documented in the MAME code.

*/


#include <stdio.h>
#include "driver.h"


/*************************************
 *
 *	Structure of slapstic params
 *
 *************************************/

struct slapstic_params
{
	int reset;
	int bank0, bank1, bank2, bank3;
	int disable;
	int ignore;
	int senable;
	int sbank0, sbank1, sbank2, sbank3;
};



/*************************************
 *
 *	Constants
 *
 *************************************/

#define DISABLE_MASK 0x3ff0
#define IGNORE_MASK  0x007f
#define UNKNOWN      0xffff

enum state_type { ENABLED, DISABLED, IGNORE, SPECIAL };

#define LOG_SLAPSTIC 0



/*************************************
 *
 *	The master table
 *
 *************************************/

static struct slapstic_params slapstic_table[18] =
{
	/* 137412-101 ESB/Tetris */
	{ 0x0000, 0x0080, 0x0090, 0x00a0, 0x00b0, 0x1540,UNKNOWN, 0x1dfe, 0x1b5c, 0x1b5d, 0x1b5e, 0x1b5f },
	/* 137412-102 ???? */
	{ 0x0000,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-103 Marble Madness */
	{ 0x0000, 0x0040, 0x0050, 0x0060, 0x0070, 0x34c0, 0x002d, 0x3d14, 0x3d24, 0x3d25, 0x3d26, 0x3d27 },
	/* 137412-104 Gauntlet */
/*	{ 0x0000, 0x0020, 0x0028, 0x0030, 0x0038, 0x3d90, 0x0069, 0x3735, 0x3764, 0x3765, 0x3766, 0x3767 },*/
/* EC990621 Gauntlet fix */
	{ 0x0000, 0x0020, 0x0028, 0x0030, 0x0038, 0x3da0, 0x0069, 0x3735, 0x3764, 0x3765, 0x3766, 0x3767 },
/* EC990621 end of Gauntlet fix */
	/* 137412-105 Indiana Jones/Paperboy */
	{ 0x0000, 0x0010, 0x0014, 0x0018, 0x001c, 0x35b0, 0x003d, 0x0092, 0x00a4, 0x00a5, 0x00a6, 0x00a7 },
	/* 137412-106 Gauntlet II */
/*	{ 0x0000, 0x0008, 0x000a, 0x000c, 0x000e, 0x3da0, 0x002b, 0x0052, 0x0064, 0x0065, 0x0066, 0x0067 },*/
/* NS990620 Gauntlet II fix */
	{ 0x0000, 0x0008, 0x000a, 0x000c, 0x000e, 0x3db0, 0x002b, 0x0052, 0x0064, 0x0065, 0x0066, 0x0067 },
/* NS990620 end of Gauntlet II fix */
	/* 137412-107 Peter Packrat/Xybots/2-player Gauntlet/720 Degrees */
/*	{ 0x0000, 0x0018, 0x001a, 0x001c, 0x001e, 0x00a0, 0x006b, 0x3d52, 0x3d64, 0x3d65, 0x3d66, 0x3d67 },*/
/* NS990622 Xybots fix */
	{ 0x0000, 0x0018, 0x001a, 0x001c, 0x001e, 0x00b0, 0x006b, 0x3d52, 0x3d64, 0x3d65, 0x3d66, 0x3d67 },
/* NS990622 end of Xybots fix */
	/* 137412-108 Road Runner/Super Sprint */
	{ 0x0000, 0x0028, 0x002a, 0x002c, 0x002e, 0x0060, 0x001f,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-109 Championship Sprint */
	{ 0x0000, 0x0008, 0x000a, 0x000c, 0x000e, 0x3da0, 0x002b, 0x0052, 0x0064, 0x0065, 0x0066, 0x0067 },
	/* 137412-110 Road Blasters/APB */
	{ 0x0000, 0x0040, 0x0050, 0x0060, 0x0070, 0x34c0, 0x002d, 0x3d14, 0x3d24, 0x3d25, 0x3d26, 0x3d27 },
	/* 137412-111 Pit Fighter */
	{ 0x0000, 0x0042, 0x0052, 0x0062, 0x0072,UNKNOWN, 0x000a,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-112 ???? */
	{ 0x0000,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-113 ???? */
	{ 0x0000,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-114 ???? */
	{ 0x0000,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-115 ???? */
	{ 0x0000,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-116 Hydra/Cyberball 2072 Tournament */
	{ 0x0000, 0x0044, 0x004c, 0x0054, 0x005c,UNKNOWN, 0x0069,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-117 Race Drivin' */
	{ 0x0000,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN },
	/* 137412-118 Vindicators II/Rampart */
/*	{ 0x0000, 0x0014, 0x0034, 0x0054, 0x0074,UNKNOWN, 0x0002, 0x1950, 0x1958, 0x1960, 0x1968, 0x1970 },*/
/* EC990622 Rampart fix */
	{ 0x0000, 0x0014, 0x0034, 0x0054, 0x0074, 0x30e0, 0x0002, 0x1958, 0x1959, 0x195a, 0x195b, 0x195c },
/* EC990622 end of Rampart fix */
};



/*************************************
 *
 *	Statics
 *
 *************************************/

static struct slapstic_params *slapstic;

static enum state_type state;
static INT8 next_bank;
static INT8 extra_bank;
static INT8 current_bank;
static UINT8 version;

#if LOG_SLAPSTIC
	static void slapstic_log(offs_t offset);
	static FILE *slapsticlog;
#else
	#define slapstic_log(o)
#endif



/*************************************
 *
 *	Initialization
 *
 *************************************/

void slapstic_init(int chip)
{
	/* only a small number of chips are known to exist */
	if (chip < 101 || chip > 118)
		return;

	/* set up a pointer to the parameters */
	version = chip;
	slapstic = slapstic_table + (chip - 101);

	/* reset the chip */
	state = ENABLED;
	next_bank = extra_bank = -1;

	/* the 111 and later chips seem to reset to bank 0 */
	if (chip < 111)
		current_bank = 3;
	else
		current_bank = 0;
}


void slapstic_reset(void)
{
	slapstic_init(version);
}



/*************************************
 *
 *	Returns active bank without tweaking
 *
 *************************************/

int slapstic_bank(void)
{
	return current_bank;
}



/*************************************
 *
 *	Call this before every access
 *
 *************************************/

int slapstic_tweak(offs_t offset)
{
	/* switch banks now if one is pending */
	if (next_bank != -1)
	{
		current_bank = next_bank;
		next_bank = -1;
		extra_bank = -1;
	}

	/* state machine */
	switch (state)
	{
		/* ENABLED state: the chip has been activated and is ready for a bankswitch */
		case ENABLED:
			if ((offset & DISABLE_MASK) == slapstic->disable)
			{
				state = DISABLED;
				/* NS990620 Gauntlet II fix */
				if (extra_bank != -1)
					next_bank = extra_bank;
				/* NS990620 end of Gauntlet II fix */
			}
			else if ((offset & IGNORE_MASK) == slapstic->ignore)
			{
				state = IGNORE;
			}
			else if (offset == slapstic->bank0)
			{
				state = DISABLED;
				if (extra_bank == -1)
					next_bank = 0;
				else
					next_bank = extra_bank;
			}
			else if (offset == slapstic->bank1)
			{
				state = DISABLED;
				if (extra_bank == -1)
					next_bank = 1;
				else
					next_bank = extra_bank;
			}
			else if (offset == slapstic->bank2)
			{
				state = DISABLED;
				if (extra_bank == -1)
					next_bank = 2;
				else
					next_bank = extra_bank;
			}
			else if (offset == slapstic->bank3)
			{
				state = DISABLED;
				if (extra_bank == -1)
					next_bank = 3;
				else
					next_bank = extra_bank;
			}
			else if (offset == slapstic->reset)
			{
				next_bank = -1;
				extra_bank = -1;
			}
			/* This is the transition which has */
			/* not been verified on the HW yet */
			else if (offset == slapstic->senable)
			{
				state = SPECIAL;
			}
			break;

		/* DISABLED state: everything is ignored except a reset */
		case DISABLED:
			if (offset == slapstic->reset)
			{
				state = ENABLED;
				next_bank = -1;
				extra_bank = -1;
			}
			break;

		/* IGNORE state: next access is interpreted differently */
		case IGNORE:
			if (offset == slapstic->senable)
			{
				state = SPECIAL;
			}
			else
			{
				state = ENABLED;
			}
			break;

		/* SPECIAL state: the special alternate bank switch override method is being used */
		case SPECIAL:
			if (offset == slapstic->sbank0)
			{
				state = ENABLED;
				extra_bank = 0;
			}
			else if (offset == slapstic->sbank1)
			{
				state = ENABLED;
				extra_bank = 1;
			}
			else if (offset == slapstic->sbank2)
			{
				state = ENABLED;
				extra_bank = 2;
			}
			else if (offset == slapstic->sbank3)
			{
				state = ENABLED;
				extra_bank = 3;
			}
			else if (offset == slapstic->reset)
			{
				state = ENABLED;
				next_bank = -1;
				extra_bank = -1;
			}
			else
			{
				state = ENABLED;
			}
			break;
	}

	/* log this access */
	slapstic_log(offset);

	/* return the active bank */
	return current_bank;
}



/*************************************
 *
 *	Debugging
 *
 *************************************/

#if LOG_SLAPSTIC
static void slapstic_log(offs_t offset)
{
	if (!slapsticlog)
		slapsticlog = fopen("slapstic.log", "w");
	if (slapsticlog)
	{
		fprintf(slapsticlog, "%06X: %04X B=%d ", cpu_getpreviouspc(), offset, current_bank);
		switch (state)
		{
			case ENABLED:
				fprintf(slapsticlog, "ENABLED\n");
				break;
			case DISABLED:
				fprintf(slapsticlog, "DISABLED\n");
				break;
			case SPECIAL:
				fprintf(slapsticlog, "SPECIAL\n");
				break;
			case IGNORE:
				fprintf(slapsticlog, "IGNORE\n");
				break;
		}
	}
}
#endif

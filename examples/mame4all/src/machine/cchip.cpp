/***************************************************************************

   cchip.c

This file contains routines to interface with the Taito Controller Chip
version 1. It's currently used by Superman and Mega Blade.

According to Richard Bush, the C-Chip is an encrypted Z80 which communicates
with the main board as a protection feature.

In Superman, it's main purpose is to handle player inputs and coins and
pass commands along to the sound chip.

The 68k queries the c-chip, which passes back $100 bytes of 68k code which
are then executed in RAM. To get around this, we hack in our own code to
communicate with the sound board, since we are familiar with the interface
as it's used in Rastan and Super Space Invaders '91.

It is believed that the NOPs in the 68k code seem to be there to supply
the necessary cycles to the cchip to switch banks.

This code requires that the player & coin inputs be in input ports 4-6.

***************************************************************************/

#include "driver.h"

static int cchip1_bank;
static int cchip[3];

/* Circular buffer, not FILO. Doh!. */
static unsigned char cchip1_code[256] =
{
	0x48, 0xe7, 0x80, 0x80,				/* MOVEM.L D0/A0,-(A7)    ( Preserve Regs ) */
	0x20, 0x6d, 0x1c, 0x40,				/* MOVEA.L ($1C40,A5),A0  ( Load sound pointer in A0 ) */
	0x30, 0x2f, 0x00, 0x0c,				/* MOVE.W ($0C,A7),D0	  ( Fetch sound number ) */
	0x10, 0x80,							/* MOVE.B D0,(A0)		  ( Store it on sound pointer ) */
	0x52, 0x88,							/* ADDQ.W #1,A0			  ( Increment sound pointer ) */
	0x20, 0x3c, 0x00, 0xf0, 0x1c, 0x40, /* MOVE.L #$F01C40,D0	  ( Load top of buffer in D0 ) */
    0xb1, 0xc0,							/* CMPA.L   D0,A0		  ( Are we there yet? ) */
    0x66, 0x04,							/* BNE.S    *+$6		  ( No, we arent, skip next line ) */
	0x41, 0xed, 0x1c, 0x20,				/* LEA      ($1C20,A5),A0 ( Point to the start of the buffer ) */
	0x2b, 0x48, 0x1c, 0x40,				/* MOVE.L A0,($1C40,A5)   ( Store new sound pointer ) */
	0x4c, 0xdf, 0x01, 0x01,				/* MOVEM.L (A7)+, D0/A0	  ( Restore Regs ) */
	0x4e, 0x75,							/* RTS					  ( Return ) */
};

void cchip1_init_machine(void)
{
	/* init custom cchip values */
	cchip[0] = cchip[1] = cchip[2] = 0;

	/* make sure we point to controls */
	cchip1_bank = 0;
}

READ_HANDLER( cchip1_r )
{
	int ret = 0;

	switch (offset)
	{
		case 0x000:
			/* Player 1 */
			if (cchip1_bank == 1)
				ret = cchip1_code[offset/2];
			else
				if (cchip[0])
				{
					ret = cchip[0];
					cchip[0] = 0;
				}
				else
					ret = readinputport (4);
			break;
		case 0x002:
			/* Player 2 */
			if (cchip1_bank == 1)
				ret = cchip1_code[offset/2];
			else
				if (cchip[1])
				{
					ret = cchip[1];
					cchip[1] = 0;
				}
				else
					ret = readinputport (5);
			break;
		case 0x004:
			/* Coins */
			if (cchip1_bank == 1)
				ret = cchip1_code[offset/2];
			else
				if (cchip[2])
				{
					ret = cchip[2];
					cchip[2] = 0;
				}
				else
					ret = readinputport (6);
			break;
		case 0x802:
			/* C-Chip ID */
			ret =  0x01;
			break;
		case 0xc00:
			ret =  cchip1_bank;
			break;
		default:
			if (offset < 0x1f0 && cchip1_bank == 1)
				ret = cchip1_code[offset/2];
			else
			{
				ret = 0xff;
			}
			break;
	}

	return ret;
}

WRITE_HANDLER( cchip1_w )
{
	switch (offset)
	{
		case 0x0000:
			if ((data & 0xff) == 0x4a)
				cchip[0] = 0x47;
			else cchip[0] = data;
			break;

		case 0x0002:
			if ((data & 0xff) == 0x46)
				cchip[1] = 0x57;
			else cchip[1] = data;
			break;

		case 0x0004:
			if ((data & 0xff) == 0x34)
				cchip[2] = 0x4b;
			else cchip[2] = data;
			break;
		case 0xc00:
			cchip1_bank = data & 0x07;
			break;
		default:
			break;
	}
}


/* Mega Blast */
unsigned char *cchip_ram;

READ_HANDLER( cchip2_r )
{
    int ret = 0;

    switch (offset)
    {
        case 0x802:
            /* C-Chip ID */
            ret = 0x01;
            break;
        default:
            ret = cchip_ram[offset];
            break;
    }

    return ret;
}

WRITE_HANDLER( cchip2_w )
{
    cchip_ram[offset] = data;
}

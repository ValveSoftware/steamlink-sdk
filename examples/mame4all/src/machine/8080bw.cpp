/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"


static int shift_data1,shift_data2,shift_amount;


WRITE_HANDLER( invaders_shift_amount_w )
{
	shift_amount = data;
}

WRITE_HANDLER( invaders_shift_data_w )
{
	shift_data2 = shift_data1;
	shift_data1 = data;
}


#define SHIFT  (((((shift_data1 << 8) | shift_data2) << (shift_amount & 0x07)) >> 8) & 0xff)


READ_HANDLER( invaders_shift_data_r )
{
	return SHIFT;
}

READ_HANDLER( invaders_shift_data_rev_r )
{
	int	ret = SHIFT;

	ret = ((ret & 0x01) << 7)
	    | ((ret & 0x02) << 5)
	    | ((ret & 0x04) << 3)
	    | ((ret & 0x08) << 1)
	    | ((ret & 0x10) >> 1)
	    | ((ret & 0x20) >> 3)
	    | ((ret & 0x40) >> 5)
	    | ((ret & 0x80) >> 7);

	return ret;
}

READ_HANDLER( invaders_shift_data_comp_r )
{
	return SHIFT ^ 0xff;
}


int invaders_interrupt(void)
{
	static int count;

	count++;

	if (count & 1)
		return 0x00cf;  /* RST 08h */
	else
		return 0x00d7;  /* RST 10h */
}

/****************************************************************************
	Extra / Different functions for Boot Hill                (MJC 300198)
****************************************************************************/

READ_HANDLER( boothill_shift_data_r )
{
	if (shift_amount < 0x10)
		return invaders_shift_data_r(0);
    else
    	return invaders_shift_data_rev_r(0);
}

/* Grays binary again! */

static const int boothill_controller_table[8] =
{
	0x00, 0x40, 0x60, 0x70, 0x30, 0x10, 0x50, 0x50
};

READ_HANDLER( boothill_port_0_r )
{
    return (input_port_0_r(0) & 0x8f) | boothill_controller_table[input_port_3_r(0) >> 5];
}

READ_HANDLER( boothill_port_1_r )
{
    return (input_port_1_r(0) & 0x8f) | boothill_controller_table[input_port_4_r(0) >> 5];
}


static const int gunfight_controller_table[8] =
{
	0x10, 0x50, 0x70, 0x30, 0x20, 0x60, 0x40, 0x00
};

READ_HANDLER( gunfight_port_0_r )
{
    return (input_port_0_r(0) & 0x8f) | (gunfight_controller_table[input_port_3_r(0) >> 5]);
}

READ_HANDLER( gunfight_port_1_r )
{
    return (input_port_1_r(0) & 0x8f) | (gunfight_controller_table[input_port_4_r(0) >> 5]);
}

/*
 * Space Encounters uses rotary controllers on input ports 0 & 1
 * each controller responds 0-63 for reading, with bit 7 as
 * fire button.
 *
 * The controllers return Grays binary, so I use a table
 * to translate my simple counter into it!
 */

static const int graybit6_controller_table[64] =
{
    0  , 1  , 3  , 2  , 6  , 7  , 5  , 4  ,
    12 , 13 , 15 , 14 , 10 , 11 , 9  , 8  ,
    24 , 25 , 27 , 26 , 30 , 31 , 29 , 28 ,
    20 , 21 , 23 , 22 , 18 , 19 , 17 , 16 ,
    48 , 49 , 51 , 50 , 54 , 55 , 53 , 52 ,
    60 , 61 , 63 , 62 , 58 , 59 , 57 , 56 ,
    40 , 41 , 43 , 42 , 46 , 47 , 45 , 44 ,
    36 , 37 , 39 , 38 , 34 , 35 , 33 , 32
};

READ_HANDLER( spcenctr_port_0_r )
{
    return (input_port_0_r(0) & 0xc0) + (graybit6_controller_table[input_port_0_r(0) & 0x3f] ^ 0x3f);
}

READ_HANDLER( spcenctr_port_1_r )
{
    return (input_port_1_r(0) & 0xc0) + (graybit6_controller_table[input_port_1_r(0) & 0x3f] ^ 0x3f);
}


READ_HANDLER( seawolf_port_1_r )
{
	return (input_port_0_r(0) & 0xe0) + graybit6_controller_table[input_port_0_r(0) & 0x1f];
}


static int desertgu_controller_select;

READ_HANDLER( desertgu_port_1_r )
{
	return readinputport(desertgu_controller_select ? 0 : 2);
}

WRITE_HANDLER( desertgu_controller_select_w )
{
	desertgu_controller_select = data & 0x08;
}

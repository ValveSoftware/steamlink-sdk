/***************************************************************************

Atari Sprint2 machine

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

int sprint2_collision1_data = 0;
int sprint2_collision2_data = 0;
int sprint2_gear1 = 1;
int sprint2_gear2 = 1;

static int sprint2_steering_buf1 = 0;
static int sprint2_steering_buf2 = 0;
static int sprint2_steering_val1 = 0xFF;
static int sprint2_steering_val2 = 0xFF;

/***************************************************************************
Read Ports

Sprint1 looks for the following:
   AAAAAAAA          D                   D
   76543210          6                   7
   00101000 ($28)    n/a                 1st1 (player 1 1st gear)
   00101001 ($29)    n/a                 1st1 (player 1 2nd gear)
   00101010 ($2A)    n/a                 1st1 (player 1 3rd gear)
   00101100 ($2B)    n/a                 Gas1 (player 1 accelerator)
   00101101 ($2C)    n/a                 Self Test
   00101110 ($2D)    n/a                 Start1

   00x10x00          Track Cycle (DIP)   Oil Slick (DIP)
   00x10x01          Mode 0 $ (DIP)      Mode 1 $ (DIP)
   00x10x10          Spare (DIP?)        Ext Play (DIP)
   00x10x11          Time 0 (DIP)        Time 1 (DIP)

We remap our input ports because if we didn't, we'd need 16 ports for this.
***************************************************************************/

READ_HANDLER( sprint1_read_ports_r )
{
	int gear;

	gear=input_port_1_r(0);
	if (gear & 0x01)                sprint2_gear1=1;
	else if (gear & 0x02)           sprint2_gear1=2;
	else if (gear & 0x04)           sprint2_gear1=3;
	else if (gear & 0x08)           sprint2_gear1=4;

	switch (offset)
	{
	        /* IN1 */
	        case 0x28:      if (sprint2_gear1==1) return 0x00; else return 0x80;
	        case 0x29:      if (sprint2_gear2==1) return 0x00; else return 0x80;
	        case 0x2A:      if (sprint2_gear1==2) return 0x00; else return 0x80;
	        case 0x2B:      return ((input_port_2_r(0) & 0x01) << 7);
	        case 0x2C:      return ((input_port_2_r(0) & 0x02) << 6);
	        case 0x2D:      return ((input_port_2_r(0) & 0x04) << 5);
	        /* DSW */
	        case 0x10:
	        case 0x14:
	        case 0x30:
	        case 0x34:      return ((input_port_0_r(0) & 0x03) << 6);
	        case 0x11:
	        case 0x15:
	        case 0x31:
	        case 0x35:      return ((input_port_0_r(0) & 0x0C) << 4);
	        case 0x12:
	        case 0x16:
	        case 0x32:
	        case 0x36:      return ((input_port_0_r(0) & 0x30) << 2);
	        case 0x13:
	        case 0x17:
	        case 0x33:
	        case 0x37:      return ((input_port_0_r(0) & 0xC0) << 0);

	        /* Just in case */
	        default:        return 0xFF;
	}
}

/***************************************************************************
Read Ports

Sprint2 looks for the following:
   AAAAAAAA          D                   D
   76543210          6                   7
   00011000 ($18)    n/a                 1st1 (player 1 1st gear)
   00011001 ($19)    n/a                 1st2 (player 2 1st gear)
   00011010 ($1A)    n/a                 2nd1 (player 1 2nd gear)
   00011011 ($1B)    n/a                 2nd2 (player 2 2nd gear)
   00011100 ($1C)    n/a                 3rd1 (player 1 3rd gear)
   00011101 ($1D)    n/a                 3rd2 (player 2 3rd gear)

   00101000 ($28)    n/a                 Gas1 (player 1 accelerator)
   00101001 ($29)    n/a                 Gas2 (player 2 accelerator)
   00101010 ($2A)    n/a                 Self Test
   00101100 ($2B)    n/a                 Start1
   00101101 ($2C)    n/a                 Start2
   00101110 ($2D)    n/a                 Track Select Button

   00x10x00          Track Cycle (DIP)   Oil Slick (DIP)
   00x10x01          Mode 0 $ (DIP)      Mode 1 $ (DIP)
   00x10x10          Spare (DIP?)        Ext Play (DIP)
   00x10x11          Time 0 (DIP)        Time 1 (DIP)

We remap our input ports because if we didn't, we'd need 16 ports for this.
***************************************************************************/

READ_HANDLER( sprint2_read_ports_r )
{
	int gear;

	gear=input_port_1_r(0);
	if (gear & 0x01)                sprint2_gear1=1;
	else if (gear & 0x02)           sprint2_gear1=2;
	else if (gear & 0x04)           sprint2_gear1=3;
	else if (gear & 0x08)           sprint2_gear1=4;

	if (gear & 0x10)                sprint2_gear2=1;
	else if (gear & 0x20)           sprint2_gear2=2;
	else if (gear & 0x40)           sprint2_gear2=3;
	else if (gear & 0x80)           sprint2_gear2=4;


	switch (offset)
	{
	        /* IN0 */
	        case 0x28:      return ((input_port_2_r(0) & 0x01) << 7);
	        case 0x29:      return ((input_port_2_r(0) & 0x02) << 6);
	        case 0x2A:      return ((input_port_2_r(0) & 0x04) << 5);
	        case 0x2C:      return ((input_port_2_r(0) & 0x08) << 4);
	        case 0x2D:      return ((input_port_2_r(0) & 0x10) << 3);
	        case 0x2E:      return ((input_port_2_r(0) & 0x20) << 2);
	        /* IN1 */
	        case 0x18:      if (sprint2_gear1==1) return 0x00; else return 0x80;
	        case 0x19:      if (sprint2_gear2==1) return 0x00; else return 0x80;
	        case 0x1A:      if (sprint2_gear1==2) return 0x00; else return 0x80;
	        case 0x1B:      if (sprint2_gear2==2) return 0x00; else return 0x80;
	        case 0x1C:      if (sprint2_gear1==3) return 0x00; else return 0x80;
	        case 0x1D:      if (sprint2_gear2==3) return 0x00; else return 0x80;
	        /* DSW */
	        case 0x10:
	        case 0x14:
	        case 0x30:
	        case 0x34:      return ((input_port_0_r(0) & 0x03) << 6);
	        case 0x11:
	        case 0x15:
	        case 0x31:
	        case 0x35:      return ((input_port_0_r(0) & 0x0C) << 4);
	        case 0x12:
	        case 0x16:
	        case 0x32:
	        case 0x36:      return ((input_port_0_r(0) & 0x30) << 2);
	        case 0x13:
	        case 0x17:
	        case 0x33:
	        case 0x37:      return ((input_port_0_r(0) & 0xC0) << 0);

	        /* Just in case */
	        default:        return 0xFF;
	}
}

/***************************************************************************
Sync

When reading from SYNC:
   D4 = ATTRACT
   D5 = VRESET
   D6 = VBLANK*
   D7 = some alternating signal!?!

The only one of these I really understand is the VBLANK...
***************************************************************************/
READ_HANDLER( sprint2_read_sync_r )
{
	static int ac_line=0x00;

	ac_line=(ac_line+1) % 3;
	if (ac_line==0)
	        return ((input_port_3_r(0) & 0x7f) | 0x80);
	else
	        return (input_port_3_r(0) & 0x7F);
}



/***************************************************************************
Coin inputs - Nothing special here.
***************************************************************************/
READ_HANDLER( sprint2_coins_r )
{
	return (input_port_4_r(0));
}



/***************************************************************************
Steering

When D7 is low, the steering wheel has moved.
If D6 is low, it moved left.  If D6 is high, it moved right.
Be sure to keep returning a direction until steering_reset is called,
because D6 and D7 are apparently checked at different times, and a
change in-between can affect the direction you move.
***************************************************************************/
READ_HANDLER( sprint2_steering1_r )
{
	static int last_val=0;
	int this_val;
	int delta;

	this_val=input_port_5_r(0);

	delta=this_val-last_val;
	last_val=this_val;
	if (delta>128) delta-=256;
	else if (delta<-128) delta+=256;
	/* Divide by four to make our steering less sensitive */
	sprint2_steering_buf1+=(delta/4);

	if (sprint2_steering_buf1>0)
	{
	        sprint2_steering_buf1--;
	        sprint2_steering_val1=0x7F;
	}
	else if (sprint2_steering_buf1<0)
	{
	        sprint2_steering_buf1++;
	        sprint2_steering_val1=0x3F;
	}

	return sprint2_steering_val1;
}

READ_HANDLER( sprint2_steering2_r )
{
	static int last_val=0;
	int this_val;
	int delta;

	this_val=input_port_6_r(0);
	delta=this_val-last_val;
	last_val=this_val;
	if (delta>128) delta-=256;
	else if (delta<-128) delta+=256;
	/* Divide by four to make our steering less sensitive */
	sprint2_steering_buf2+=(delta/4);

	if (sprint2_steering_buf2>0)
	{
	        sprint2_steering_buf2--;
	        sprint2_steering_val2=0x7F;
	}
	else if (sprint2_steering_buf2<0)
	{
	        sprint2_steering_buf2++;
	        sprint2_steering_val2=0x3F;
	}

	return sprint2_steering_val2;
}

WRITE_HANDLER( sprint2_steering_reset1_w )
{
    sprint2_steering_val1=0xFF;
}

WRITE_HANDLER( sprint2_steering_reset2_w )
{
    sprint2_steering_val2=0xFF;
}



/***************************************************************************
Collisions

D6=1, skid.  D7=1, crash.

Note:  collisions are actually being set in vidhrdw/sprint2.c
***************************************************************************/
READ_HANDLER( sprint2_collision1_r )
{
	return sprint2_collision1_data;
}

READ_HANDLER( sprint2_collision2_r )
{
	return sprint2_collision2_data;
}

WRITE_HANDLER( sprint2_collision_reset1_w )
{
	sprint2_collision1_data=0;
}

WRITE_HANDLER( sprint2_collision_reset2_w )
{
	sprint2_collision2_data=0;
}

/***************************************************************************
Lamps
***************************************************************************/
WRITE_HANDLER( sprint2_lamp1_w )
{
	osd_led_w(0,(data>0));
}

WRITE_HANDLER( sprint2_lamp2_w )
{
	osd_led_w(1,(data>0));
}




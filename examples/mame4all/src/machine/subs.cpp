/***************************************************************************

Subs machine

If you have any questions about how this driver works, don't hesitate to
ask.  - Mike Balfour (mab22@po.cwru.edu)
***************************************************************************/

#include "driver.h"

static int subs_steering_buf1;
static int subs_steering_buf2;
static int subs_steering_val1;
static int subs_steering_val2;

/***************************************************************************
subs_init_machine
***************************************************************************/
void subs_init_machine(void)
{
	subs_steering_buf1 = 0;
	subs_steering_buf2 = 0;
	subs_steering_val1 = 0x00;
	subs_steering_val2 = 0x00;
}

/***************************************************************************
subs_interrupt
***************************************************************************/
int subs_interrupt(void)
{
	/* only do NMI interrupt if not in TEST mode */
	if ((input_port_2_r(0) & 0x40)==0x40)
		return nmi_interrupt();

	return 0;
}

/***************************************************************************
Steering

When D7 is high, the steering wheel has moved.
If D6 is high, it moved left.  If D6 is low, it moved right.
Be sure to keep returning a direction until steer_reset is called.
***************************************************************************/
static int subs_steering_1(void)
{
        static int last_val=0;
        int this_val;
        int delta;

        this_val=input_port_3_r(0);

        delta=this_val-last_val;
        last_val=this_val;
        if (delta>128) delta-=256;
        else if (delta<-128) delta+=256;
        /* Divide by four to make our steering less sensitive */
        subs_steering_buf1+=(delta/4);

        if (subs_steering_buf1>0)
        {
                subs_steering_buf1--;
                subs_steering_val1=0xC0;
        }
        else if (subs_steering_buf1<0)
        {
                subs_steering_buf1++;
                subs_steering_val1=0x80;
        }

        return subs_steering_val1;
}

static int subs_steering_2(void)
{
        static int last_val=0;
        int this_val;
        int delta;

        this_val=input_port_4_r(0);

        delta=this_val-last_val;
        last_val=this_val;
        if (delta>128) delta-=256;
        else if (delta<-128) delta+=256;
        /* Divide by four to make our steering less sensitive */
        subs_steering_buf2+=(delta/4);

        if (subs_steering_buf2>0)
        {
                subs_steering_buf2--;
                subs_steering_val2=0xC0;
        }
        else if (subs_steering_buf2<0)
        {
                subs_steering_buf2++;
                subs_steering_val2=0x80;
        }

        return subs_steering_val2;
}

/***************************************************************************
subs_steer_reset
***************************************************************************/
WRITE_HANDLER( subs_steer_reset_w )
{
    subs_steering_val1 = 0x00;
    subs_steering_val2 = 0x00;
}

/***************************************************************************
subs_control_r
***************************************************************************/
READ_HANDLER( subs_control_r )
{
	int inport = input_port_1_r(offset);

	switch (offset & 0x07)
	{
		case 0x00:		return ((inport & 0x01) << 7);	/* diag step */
		case 0x01:		return ((inport & 0x02) << 6);	/* diag hold */
		case 0x02:		return ((inport & 0x04) << 5);	/* slam */
		case 0x03:		return ((inport & 0x08) << 4);	/* spare */
		case 0x04:		return ((subs_steering_1() & 0x40) << 1);	/* steer dir 1 */
		case 0x05:		return ((subs_steering_1() & 0x80) << 0);	/* steer flag 1 */
		case 0x06:		return ((subs_steering_2() & 0x40) << 1);	/* steer dir 2 */
		case 0x07:		return ((subs_steering_2() & 0x80) << 0);	/* steer flag 2 */
	}

	return 0;
}

/***************************************************************************
subs_coin_r
***************************************************************************/
READ_HANDLER( subs_coin_r )
{
	int inport = input_port_2_r(offset);

	switch (offset & 0x07)
	{
		case 0x00:		return ((inport & 0x01) << 7);	/* coin 1 */
		case 0x01:		return ((inport & 0x02) << 6);	/* start 1 */
		case 0x02:		return ((inport & 0x04) << 5);	/* coin 2 */
		case 0x03:		return ((inport & 0x08) << 4);	/* start 2 */
		case 0x04:		return ((inport & 0x10) << 3);	/* VBLANK */
		case 0x05:		return ((inport & 0x20) << 2);	/* fire 1 */
		case 0x06:		return ((inport & 0x40) << 1);	/* test */
		case 0x07:		return ((inport & 0x80) << 0);	/* fire 2 */
	}

	return 0;
}

/***************************************************************************
subs_options_r
***************************************************************************/
READ_HANDLER( subs_options_r )
{
	int opts = input_port_0_r(offset);

	switch (offset & 0x03)
	{
		case 0x00:		return ((opts & 0xC0) >> 6);		/* language */
		case 0x01:		return ((opts & 0x30) >> 4);		/* credits */
		case 0x02:		return ((opts & 0x0C) >> 2);		/* game time */
		case 0x03:		return ((opts & 0x03) >> 0);		/* extended time */
	}

	return 0;
}

/***************************************************************************
subs_lamp1_w
***************************************************************************/
WRITE_HANDLER( subs_lamp1_w )
{
	if ((offset & 0x01) == 1)
	{
		osd_led_w(0,0);
		//logerror("LED 0 OFF\n");
	}
	else
	{
		osd_led_w(0,1);
		//logerror("LED 0 ON\n");
	}
}

/***************************************************************************
subs_lamp2_w
***************************************************************************/
WRITE_HANDLER( subs_lamp2_w )
{
	if ((offset & 0x01) == 1)
	{
		osd_led_w(1,0);
		//logerror("LED 1 OFF\n");
	}
	else
	{
		osd_led_w(1,1);
		//logerror("LED 1 ON\n");
	}
}

/***************************************************************************
TODO: sub sound functions
***************************************************************************/

WRITE_HANDLER( subs_sonar2_w )
{
}

WRITE_HANDLER( subs_sonar1_w )
{
}

WRITE_HANDLER( subs_crash_w )
{
}

WRITE_HANDLER( subs_explode_w )
{
}

WRITE_HANDLER( subs_noise_reset_w )
{
}


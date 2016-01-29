/***************************************************************************

  machine.c

  Functions to emulate a prototypical ticket dispenser hardware.

  Right now, this is an *extremely* basic ticket dispenser.
  TODO:	Active Bit may not be Bit 7 in all applications.
  	    Add a ticket dispenser interface instead of passing a bunch
		of arguments to ticket_dispenser_init.
		Add sound, graphical output?
***************************************************************************/

#include "driver.h"
#include "machine/ticket.h"

/*#define DEBUG_TICKET*/

extern unsigned int dispensed_tickets;

static int status;
static int power;
static int time_msec;
static int motoron;
static int ticketdispensed;
static int ticketnotdispensed;
static void *timer;

static int active_bit = 0x80;

/* Callback routine used during ticket dispensing */
static void ticket_dispenser_toggle(int parm);


/***************************************************************************
  ticket_dispenser_init

***************************************************************************/
void ticket_dispenser_init(int msec, int motoronhigh, int statusactivehigh)
{
	time_msec          = msec;
	motoron            = motoronhigh  ? active_bit : 0;
	ticketdispensed    = statusactivehigh ? active_bit : 0;
	ticketnotdispensed = ticketdispensed ^ active_bit;

	status = ticketnotdispensed;
	dispensed_tickets = 0;
	power  = 0x00;
}

/***************************************************************************
  ticket_dispenser_r
***************************************************************************/
READ_HANDLER( ticket_dispenser_r )
{
#ifdef DEBUG_TICKET
	logerror("PC: %04X  Ticket Status Read = %02X\n", cpu_get_pc(), status);
#endif
	return status;
}

/***************************************************************************
  ticket_dispenser_w
***************************************************************************/
WRITE_HANDLER( ticket_dispenser_w )
{
	/* On an activate signal, start dispensing! */
	if ((data & active_bit) == motoron)
	{
		if (!power)
		{
#ifdef DEBUG_TICKET
			logerror("PC: %04X  Ticket Power On\n", cpu_get_pc());
#endif
			timer = timer_set (TIME_IN_MSEC(time_msec), 0, ticket_dispenser_toggle);
			power = 1;

			status = ticketnotdispensed;
		}
	}
	else
	{
		if (power)
		{
#ifdef DEBUG_TICKET
			logerror("PC: %04X  Ticket Power Off\n", cpu_get_pc());
#endif
			timer_remove(timer);
			osd_led_w(2,0);
			power = 0;
		}
	}
}


/***************************************************************************
  ticket_dispenser_toggle

  How I think this works:
  When a ticket dispenses, there is N milliseconds of status = high,
  and N milliseconds of status = low (a wait cycle?).
***************************************************************************/
static void ticket_dispenser_toggle(int parm)
{

	/* If we still have power, keep toggling ticket states. */
	if (power)
	{
		status ^= active_bit;
#ifdef DEBUG_TICKET
		logerror("Ticket Status Changed to %02X\n", status);
#endif
		timer = timer_set (TIME_IN_MSEC(time_msec), 0, ticket_dispenser_toggle);
	}

	if (status == ticketdispensed)
	{
		osd_led_w(2,1);
		dispensed_tickets++;

#ifdef DEBUG_TICKET
		logerror("Ticket Dispensed\n");
#endif
	}
	else
	{
		osd_led_w(2,0);
	}
}

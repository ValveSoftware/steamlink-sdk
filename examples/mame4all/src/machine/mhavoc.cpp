/***************************************************************************

  mhavoc.c (machine)

  Functions to emulate general aspects of the machine
  (RAM, ROM, interrupts, I/O ports)

***************************************************************************/
#include "driver.h"
#include "vidhrdw/avgdvg.h"
#include "cpu/m6502/m6502.h"

static int gamma_data;
static int alpha_data;
static int alpha_rcvd;
static int alpha_xmtd;
static int gamma_rcvd;
static int gamma_xmtd;

static int bank_select;
static int player_1;

#define LS161_CLOCK 2*5000

static void *gamma_timer = NULL;
static void mhavoc_gamma_irq(int param);

WRITE_HANDLER( mhavoc_ram_banksel_w )
{
	static int bank[2] = { 0x20200, 0x20800 };
	unsigned char *RAM = memory_region(REGION_CPU1);

	data&=0x01;
	//logerror("Alpha RAM select: %02x\n",data);
	cpu_setbank (1, &RAM[bank[data]]);
}

WRITE_HANDLER( mhavoc_rom_banksel_w )
{
	static int bank[4] = { 0x10000, 0x12000, 0x14000, 0x16000 };
	unsigned char *RAM = memory_region(REGION_CPU1);


	data &= 0x03;

	//logerror("Alpha ROM select: %02x\n",data);
	cpu_setbank (2, &RAM[bank[data]]);
}

void mhavoc_init_machine (void)
{
	/* Set all the banks to the right place */
	mhavoc_ram_banksel_w (0,0);
	mhavoc_rom_banksel_w (0,0);
	bank_select = -1;
	alpha_data=0;
	gamma_data=0;
	alpha_rcvd=0;
	alpha_xmtd=0;
	gamma_rcvd=0;
	gamma_xmtd=0;
	player_1 = 0;
	if (gamma_timer)
			timer_remove(gamma_timer);
	gamma_timer = timer_pulse(TIME_IN_HZ(LS161_CLOCK/16), 0, mhavoc_gamma_irq);
}

/* Read from the gamma processor */
READ_HANDLER( mhavoc_gamma_r )
{
	//logerror("  reading from gamma processor: %02x (%d %d)\n", gamma_data, alpha_rcvd, gamma_xmtd);
	alpha_rcvd=1;
	gamma_xmtd=0;
	return gamma_data;
}

/* Read from the alpha processor */
READ_HANDLER( mhavoc_alpha_r )
{
	//logerror("\t\t\t\t\treading from alpha processor: %02x (%d %d)\n", alpha_data, gamma_rcvd, alpha_xmtd);
	gamma_rcvd=1;
	alpha_xmtd=0;
	return alpha_data;
}

/* Write to the gamma processor */
WRITE_HANDLER( mhavoc_gamma_w )
{
	//logerror("  writing to gamma processor: %02x (%d %d)\n", data, gamma_rcvd, alpha_xmtd);
	gamma_rcvd=0;
	alpha_xmtd=1;
	alpha_data = data;
	cpu_cause_interrupt (1, M6502_INT_NMI);

	/* the sound CPU needs to reply in 250microseconds (according to Neil Bradley) */
	timer_set (TIME_IN_USEC(250), 0, 0);
}

/* Write to the alpha processor */
WRITE_HANDLER( mhavoc_alpha_w )
{
	//logerror("\t\t\t\t\twriting to alpha processor: %02x %d %d\n", data, alpha_rcvd, gamma_xmtd);
	alpha_rcvd=0;
	gamma_xmtd=1;
	gamma_data = data;
}

/* Simulates frequency and vector halt */
READ_HANDLER( mhavoc_port_0_r )
{
	int res;

	res = readinputport(0);
	if (player_1)
		res = (res & 0x3f) | (readinputport (5) & 0xc0);

	/* Emulate the 2.4Khz source on bit 2 (divide 2.5Mhz by 1024) */
	if (cpu_gettotalcycles() & 0x400)
		res &=~0x02;
	else
		res|=0x02;

	if (avgdvg_done())
		res |=0x01;
	else
		res &=~0x01;

	if (gamma_rcvd==1)
		res |=0x08;
	else
		res &=~0x08;

	if (gamma_xmtd==1)
		res |=0x04;
	else
		res &=~0x04;

	return (res & 0xff);
}

READ_HANDLER( mhavoc_port_1_r )
{
	int res;

	res=readinputport(1);

	if (alpha_rcvd==1)
		res |=0x02;
	else
		res &=~0x02;

	if (alpha_xmtd==1)
		res |=0x01;
	else
		res &=~0x01;

	return (res & 0xff);
}

WRITE_HANDLER( mhavoc_out_0_w )
{
	if (!(data & 0x08))
	{
		//logerror("\t\t\t\t*** resetting gamma processor. ***\n");
		cpu_set_reset_line(1,PULSE_LINE);
		alpha_rcvd=0;
		alpha_xmtd=0;
		gamma_rcvd=0;
		gamma_xmtd=0;
	}
	player_1 = data & 0x20;
	/* Emulate the roller light (Blinks on fatal errors) */
	osd_led_w (2, data & 0x01);
}

WRITE_HANDLER( mhavoc_out_1_w )
{
	osd_led_w (1, data & 0x01);
	osd_led_w (0, (data & 0x02)>>1);
}

static void mhavoc_gamma_irq(int param)
{
	cpu_set_irq_line(1,0,HOLD_LINE);
}

WRITE_HANDLER( mhavoc_irqack_w )
{
	timer_reset( gamma_timer, TIME_IN_HZ(LS161_CLOCK/16));
	cpu_set_irq_line(1,0,CLEAR_LINE);
}

/***************************************************************************
					ToaPlan  (1988-1991 hardware)
 ***************************************************************************/

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "cpu/tms32010/tms32010.h"

#define CLEAR 0
#define ASSERT 1


READ_HANDLER( video_ofs3_r );
WRITE_HANDLER( video_ofs3_w );
WRITE_HANDLER( toaplan1_videoram3_w );

int toaplan1_coin_count; /* coin count increments on startup ? , so dont count it */

int toaplan1_int_enable;
static int unk;
static int credits;
static int latch;
static int dsp_execute;
static unsigned int dsp_addr_w, main_ram_seg;

extern unsigned char *toaplan1_sharedram;



READ_HANDLER( demonwld_dsp_r )
{
	/* DSP can read data from main CPU RAM via DSP IO port 1 */

	unsigned int input_data = 0;

	switch (main_ram_seg) {
		case 0xc00000:	input_data = READ_WORD(&(cpu_bankbase[1][(dsp_addr_w)])); break;

		default:		//logerror("DSP PC:%04x Warning !!! IO reading from %08x (port 1)\n",cpu_getpreviouspc(),main_ram_seg + dsp_addr_w);
		    break;
	}
	//logerror("DSP PC:%04x IO read %04x at %08x (port 1)\n",cpu_getpreviouspc(),input_data,main_ram_seg + dsp_addr_w);
	return input_data;
}

WRITE_HANDLER( demonwld_dsp_w )
{
	if (offset == 0) {
		/* This sets the main CPU RAM address the DSP should */
		/*		read/write, via the DSP IO port 0 */
		/* Top three bits of data need to be shifted left 9 places */
		/*		to select which memory bank from main CPU address */
		/*		space to use */
		/* Lower thirteen bits of this data is shifted left one position */
		/*		to move it to an even address word boundary */

		dsp_addr_w = ((data & 0x1fff) << 1);
		main_ram_seg = ((data & 0xe000) << 9);
		//logerror("DSP PC:%04x IO write %04x (%08x) at port 0\n",cpu_getpreviouspc(),data,main_ram_seg + dsp_addr_w);
	}
	if (offset == 1) {
		/* Data written to main CPU RAM via DSP IO port 1*/

		dsp_execute = 0;
		switch (main_ram_seg) {
			case 0xc00000:	WRITE_WORD(&(cpu_bankbase[1][(dsp_addr_w)]),data);
							if ((dsp_addr_w < 3) && (data == 0)) dsp_execute = 1; break;
			default:		//logerror("DSP PC:%04x Warning !!! IO writing to %08x (port 1)\n",cpu_getpreviouspc(),main_ram_seg + dsp_addr_w);
			    break;
		}
		//logerror("DSP PC:%04x IO write %04x at %08x (port 1)\n",cpu_getpreviouspc(),data,main_ram_seg + dsp_addr_w);
	}
	if (offset == 3) {
		/* data 0xffff	means inhibit BIO line to DSP and enable  */
		/*				communication to main processor */
		/*				Actually only DSP data bit 15 controls this */
		/* data 0x0000	means set DSP BIO line active and disable */
		/*				communication to main processor*/
		//logerror("DSP PC:%04x IO write %04x at port 3\n",cpu_getpreviouspc(),data);
		if (data & 0x8000) {
			cpu_set_irq_line(2, TMS320C10_ACTIVE_BIO, CLEAR_LINE);
		}
		if (data == 0) {
			if (dsp_execute) {
				//logerror("Turning 68000 on\n");
				timer_suspendcpu(0, CLEAR, SUSPEND_REASON_HALT);
				dsp_execute = 0;
			}
			cpu_set_irq_line(2, TMS320C10_ACTIVE_BIO, ASSERT_LINE);
		}
	}
}

WRITE_HANDLER( demonwld_dsp_ctrl_w )
{
#if 0
	logerror("68000:%08x  Writing %08x to %08x.\n",cpu_get_pc() ,data ,0xe0000a + offset);
#endif

	switch (data) {
		case 0x0000: 	/* This means assert the INT line to the DSP */
						//logerror("Turning DSP on and 68000 off\n");
						timer_suspendcpu(2, CLEAR, SUSPEND_REASON_HALT);
						cpu_set_irq_line(2, TMS320C10_ACTIVE_INT, ASSERT_LINE);
						timer_suspendcpu(0, ASSERT, SUSPEND_REASON_HALT);
						break;
		case 0x0001: 	/* This means inhibit the INT line to the DSP */
						//logerror("Turning DSP off\n");
						cpu_set_irq_line(2, TMS320C10_ACTIVE_INT, CLEAR_LINE);
						timer_suspendcpu(2, ASSERT, SUSPEND_REASON_HALT);
						break;
		default:		//logerror("68000:%04x  writing unknown command %08x to %08x\n",cpu_getpreviouspc() ,data ,0xe0000a + offset);
		    break;
	}
}



int toaplan1_interrupt(void)
{
	if (toaplan1_int_enable)
	{
		return MC68000_IRQ_4;
	}
	return MC68000_INT_NONE;
}

WRITE_HANDLER( toaplan1_int_enable_w )
{
	toaplan1_int_enable = data;
}

READ_HANDLER( toaplan1_unk_r )
{
	return unk ^= 1;
}

READ_HANDLER( samesame_port_6_r )
{
	/* Bit 0x80 is secondary CPU (HD647180) ready signal */
	//logerror("PC:%04x Warning !!! IO reading from $14000a\n",cpu_getpreviouspc());
	return (0x80 | input_port_6_r(0));
}

READ_HANDLER( vimana_input_port_5_r )
{
	int data, p;

	p = input_port_5_r(0);

	latch ^= p;
	data = (latch & p );

	/* simulate the mcu keeping track of credits */
	/* latch is so is doesn't add more than one */
	/* credit per keypress */

	if (data & 0x18)
	{
		credits++ ;
	}

	latch = p;

	return p;
}

READ_HANDLER( vimana_mcu_r )
{
	int data = 0 ;
	switch (offset >> 1)
	{
		case 0:
			data = 0xff;
			break;
		case 1:
			data = 0;
			break;
		case 2:
			data = credits;
			break;
	}
	return data;
}
WRITE_HANDLER( vimana_mcu_w )
{
	switch (offset >> 1)
	{
		case 0:
			break;
		case 1:
			break;
		case 2:
			credits = data;
			break;
	}
}

READ_HANDLER( toaplan1_shared_r )
{
	return toaplan1_sharedram[offset>>1];
}

WRITE_HANDLER( toaplan1_shared_w )
{
	toaplan1_sharedram[offset>>1] = data;
}

void toaplan1_init_machine(void)
{
	dsp_addr_w = dsp_execute = 0;
	main_ram_seg = 0;
	toaplan1_int_enable = 0;
	unk = 0;
	credits = 0;
	latch = 0;
	toaplan1_coin_count = 0;
	coin_lockout_global_w(0,0);
}

WRITE_HANDLER( rallybik_coin_w )
{
	switch (data) {
		case 0x08: if (toaplan1_coin_count) { coin_counter_w(0,1); coin_counter_w(0,0); } break;
		case 0x09: if (toaplan1_coin_count) { coin_counter_w(2,1); coin_counter_w(2,0); } break;
		case 0x0a: if (toaplan1_coin_count) { coin_counter_w(1,1); coin_counter_w(1,0); } break;
		case 0x0b: if (toaplan1_coin_count) { coin_counter_w(3,1); coin_counter_w(3,0); } break;
		case 0x0c: coin_lockout_w(0,1); coin_lockout_w(2,1); break;
		case 0x0d: coin_lockout_w(0,0); coin_lockout_w(2,0); break;
		case 0x0e: coin_lockout_w(1,1); coin_lockout_w(3,1); break;
		case 0x0f: coin_lockout_w(1,0); coin_lockout_w(3,0); toaplan1_coin_count=1; break;
	}
}

WRITE_HANDLER( toaplan1_coin_w )
{
	//logerror("Z80 writing %02x to coin control\n",data);
	/* This still isnt too clear yet. */
	/* Coin C has no coin lock ? */
	/* Are some outputs for lights ? (no space on JAMMA for it though) */

	switch (data) {
		case 0xee: coin_counter_w(1,1); coin_counter_w(1,0); break; /* Count slot B */
		case 0xed: coin_counter_w(0,1); coin_counter_w(0,0); break; /* Count slot A */
	/* The following are coin counts after coin-lock active (faulty coin-lock ?) */
		case 0xe2: coin_counter_w(1,1); coin_counter_w(1,0); coin_lockout_w(1,1); break;
		case 0xe1: coin_counter_w(0,1); coin_counter_w(0,0); coin_lockout_w(0,1); break;

		case 0xec: coin_lockout_global_w(0,0); break;	/* ??? count games played */
		case 0xe8: break;	/* ??? Maximum credits reached with coin/credit ratio */
		case 0xe4: break;	/* ??? Reset coin system */

		case 0x0c: coin_lockout_global_w(0,0); break;	/* Unlock all coin slots */
		case 0x08: coin_lockout_w(2,0); break;	/* Unlock coin slot C */
		case 0x09: coin_lockout_w(0,0); break;	/* Unlock coin slot A */
		case 0x0a: coin_lockout_w(1,0); break;	/* Unlock coin slot B */

		case 0x02: coin_lockout_w(1,1); break;	/* Lock coin slot B */
		case 0x01: coin_lockout_w(0,1); break;	/* Lock coin slot A */
		case 0x00: coin_lockout_global_w(0,1); break;	/* Lock all coin slots */
	}
}

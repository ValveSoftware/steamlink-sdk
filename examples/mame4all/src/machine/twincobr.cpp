/****************************************************************************
 *	Twin Cobra																*
 *	Communications and memory functions between shared CPU memory spaces	*
 ****************************************************************************/

#include "driver.h"
#include "cpu/tms32010/tms32010.h"

#define LOG_DSP_CALLS 0
#define CLEAR 0
#define ASSERT 1



unsigned char *twincobr_68k_dsp_ram;
unsigned char *twincobr_sharedram;
unsigned char *wardner_mainram;

extern unsigned char *spriteram;
extern unsigned char *paletteram;


extern int twincobr_fg_rom_bank;
extern int twincobr_bg_ram_bank;
extern int twincobr_display_on;
extern int twincobr_flip_screen;
extern int twincobr_flip_x_base;
extern int twincobr_flip_y_base;
extern int wardner_sprite_hack;

static int coin_count;	/* coin count increments on startup ? , so stop it */
static int dsp_execute;
static unsigned int dsp_addr_w, main_ram_seg;
int toaplan_main_cpu;   /* Main CPU type.  0 = 68000, 1 = Z80 */
#if LOG_DSP_CALLS
static char *toaplan_cpu_type[2] = { "68K" , "Z80" };
#endif

int twincobr_intenable;
int fsharkbt_8741;


void fsharkbt_reset_8741_mcu(void)
{
	/* clean out high score tables in these game hardware */
	int twincobr_cnt;
	int twinc_hisc_addr[12] =
	{
		0x15a4, 0x15a8, 0x170a, 0x170c, /* Twin Cobra */
		0x1282, 0x1284, 0x13ea, 0x13ec, /* Kyukyo Tiger */
		0x016c, 0x0170, 0x02d2, 0x02d4	/* Flying shark */
	};
	for (twincobr_cnt=0; twincobr_cnt < 12; twincobr_cnt++)
	{
		WRITE_WORD(&twincobr_68k_dsp_ram[(twinc_hisc_addr[twincobr_cnt])],0xffff);
	}

	toaplan_main_cpu = 0;		/* 68000 */
	twincobr_display_on = 0;
	fsharkbt_8741 = -1;
	twincobr_intenable = 0;
	dsp_addr_w = dsp_execute = 0;
	main_ram_seg = 0;

	/* coin count increments on startup ? , so stop it */
	coin_count = 0;

	/* blank out the screen */
	osd_clearbitmap(Machine->scrbitmap);
}

void wardner_reset(void)
{
	/* clean out high score tables in these game hardware */
	wardner_mainram[0x0117] = 0xff;
	wardner_mainram[0x0118] = 0xff;
	wardner_mainram[0x0119] = 0xff;
	wardner_mainram[0x011a] = 0xff;
	wardner_mainram[0x011b] = 0xff;
	wardner_mainram[0x0170] = 0xff;
	wardner_mainram[0x0171] = 0xff;
	wardner_mainram[0x0172] = 0xff;

	toaplan_main_cpu = 1;		/* Z80 */
	twincobr_intenable = 0;
	twincobr_display_on = 1;
	dsp_addr_w = dsp_execute = 0;
	main_ram_seg = 0;

	/* coin count increments on startup ? , so stop it */
	coin_count = 0;

	/* blank out the screen */
	osd_clearbitmap(Machine->scrbitmap);
}


READ_HANDLER( twincobr_dsp_r )
{
	/* DSP can read data from main CPU RAM via DSP IO port 1 */

	unsigned int input_data = 0;
	switch (main_ram_seg) {
		case 0x30000:	input_data = READ_WORD(&twincobr_68k_dsp_ram[dsp_addr_w]); break;
		case 0x40000:	input_data = READ_WORD(&spriteram[dsp_addr_w]); break;
		case 0x50000:	input_data = READ_WORD(&paletteram[dsp_addr_w]); break;
		case 0x7000:	input_data = wardner_mainram[dsp_addr_w] + (wardner_mainram[dsp_addr_w+1]<<8); break;
		case 0x8000:	input_data = spriteram[dsp_addr_w] + (spriteram[dsp_addr_w+1]<<8); break;
		case 0xa000:	input_data = paletteram[dsp_addr_w] + (paletteram[dsp_addr_w+1]<<8); break;
		default:		//logerror("DSP PC:%04x Warning !!! IO reading from %08x (port 1)\n",cpu_getpreviouspc(),main_ram_seg + dsp_addr_w);
		    break;
	}
#if LOG_DSP_CALLS
	logerror("DSP PC:%04x IO read %04x at %08x (port 1)\n",cpu_getpreviouspc(),input_data,main_ram_seg + dsp_addr_w);
#endif
	return input_data;
}

READ_HANDLER( fsharkbt_dsp_r )
{
	/* Flying Shark bootleg uses IO port 2 */
	/* DSP reads data from an extra MCU (8741) at IO port 2 */
	/* Boot-leggers using their own copy protection ?? */
	/* Port is read three times during startup. First and last data */
	/*	 read must equal, but second read data must be different */
	fsharkbt_8741 += 1;
#if LOG_DSP_CALLS
	logerror("DSP PC:%04x IO read %04x from 8741 MCU (port 2)\n",cpu_getpreviouspc(),(fsharkbt_8741 & 0x08));
#endif
	return (fsharkbt_8741 & 1);
}

WRITE_HANDLER( twincobr_dsp_w )
{
	if (offset == 0) {
		/* This sets the main CPU RAM address the DSP should */
		/*		read/write, via the DSP IO port 0 */
		/* Top three bits of data need to be shifted left 3 places */
		/*		to select which memory bank from main CPU address */
		/*		space to use */
		/* Lower thirteen bits of this data is shifted left one position */
		/*		to move it to an even address word boundary */

		dsp_addr_w = ((data & 0x1fff) << 1);
		main_ram_seg = ((data & 0xe000) << 3);
		if (toaplan_main_cpu == 1) {		/* Z80 */
			dsp_addr_w &= 0xfff;
			if (main_ram_seg == 0x30000) main_ram_seg = 0x7000;
			if (main_ram_seg == 0x40000) main_ram_seg = 0x8000;
			if (main_ram_seg == 0x50000) main_ram_seg = 0xa000;
		}
#if LOG_DSP_CALLS
		logerror("DSP PC:%04x IO write %04x (%08x) at port 0\n",cpu_getpreviouspc(),data,main_ram_seg + dsp_addr_w);
#endif
	}
	if (offset == 1) {
		/* Data written to main CPU RAM via DSP IO port 1*/
		dsp_execute = 0;
		switch (main_ram_seg) {
			case 0x30000:	WRITE_WORD(&twincobr_68k_dsp_ram[dsp_addr_w],data);
								if ((dsp_addr_w < 3) && (data == 0)) dsp_execute = 1; break;
			case 0x40000:	WRITE_WORD(&spriteram[dsp_addr_w],data); break;
			case 0x50000:	WRITE_WORD(&paletteram[dsp_addr_w],data); break;
			case 0x7000:	wardner_mainram[dsp_addr_w] = data & 0xff;
							wardner_mainram[dsp_addr_w + 1] = (data >> 8) & 0xff;
							if ((dsp_addr_w < 3) && (data == 0)) dsp_execute = 1; break;
			case 0x8000:	spriteram[dsp_addr_w] = data & 0xff;
							spriteram[dsp_addr_w + 1] = (data >> 8) & 0xff;break;
			case 0xa000:	paletteram[dsp_addr_w] = data & 0xff;
							paletteram[dsp_addr_w + 1] = (data >> 8) & 0xff; break;
			default:		//logerror("DSP PC:%04x Warning !!! IO writing to %08x (port 1)\n",cpu_getpreviouspc(),main_ram_seg + dsp_addr_w);
			    break;
		}
#if LOG_DSP_CALLS
		logerror("DSP PC:%04x IO write %04x at %08x (port 1)\n",cpu_getpreviouspc(),data,main_ram_seg + dsp_addr_w);
#endif
	}
	if (offset == 2) {
		/* Flying Shark bootleg DSP writes data to an extra MCU (8741) at IO port 2 */
#if 0
		logerror("DSP PC:%04x IO write from DSP RAM:%04x to 8741 MCU (port 2)\n",cpu_getpreviouspc(),fsharkbt_8741);
#endif
	}
	if (offset == 3) {
		/* data 0xffff	means inhibit BIO line to DSP and enable  */
		/*				communication to main processor */
		/*				Actually only DSP data bit 15 controls this */
		/* data 0x0000	means set DSP BIO line active and disable */
		/*				communication to main processor*/
#if LOG_DSP_CALLS
		logerror("DSP PC:%04x IO write %04x at port 3\n",cpu_getpreviouspc(),data);
#endif
		if (data & 0x8000) {
			cpu_set_irq_line(2, TMS320C10_ACTIVE_BIO, CLEAR_LINE);
		}
		if (data == 0) {
			if (dsp_execute) {
#if LOG_DSP_CALLS
				logerror("Turning %s on\n",toaplan_cpu_type[toaplan_main_cpu]);
#endif
				timer_suspendcpu(0, CLEAR, SUSPEND_REASON_HALT);
				dsp_execute = 0;
			}
			cpu_set_irq_line(2, TMS320C10_ACTIVE_BIO, ASSERT_LINE);
		}
	}
}

READ_HANDLER( twincobr_68k_dsp_r )
{
	return READ_WORD(&twincobr_68k_dsp_ram[offset]);
}

WRITE_HANDLER( twincobr_68k_dsp_w )
{
#if LOG_DSP_CALLS
	if (offset < 10) logerror("%s:%08x write %08x at %08x\n",toaplan_cpu_type[toaplan_main_cpu],cpu_get_pc(),data,0x30000+offset);
#endif
	COMBINE_WORD_MEM(&twincobr_68k_dsp_ram[offset],data);
}


WRITE_HANDLER( wardner_mainram_w )
{
#if 0
	if ((offset == 4) && (data != 4)) logerror("CPU #0:%04x  Writing %02x to %04x of main RAM (DSP command number)\n",cpu_get_pc(),data, offset + 0x7000);
#endif
	wardner_mainram[offset] = data;

}
READ_HANDLER( wardner_mainram_r )
{
	return wardner_mainram[offset];
}


WRITE_HANDLER( twincobr_7800c_w )
{
#if 0
	logerror("%s:%08x  Writing %08x to %08x.\n",toaplan_cpu_type[toaplan_main_cpu],cpu_get_pc(),data,toaplan_port_type[toaplan_main_cpu] - offset);
#endif

	if (toaplan_main_cpu == 1) {
		if (data == 0x0c) { data = 0x1c; wardner_sprite_hack=0; }	/* Z80 ? */
		if (data == 0x0d) { data = 0x1d; wardner_sprite_hack=1; }	/* Z80 ? */
	}

	switch (data) {
		case 0x0004: twincobr_intenable = 0; break;
		case 0x0005: twincobr_intenable = 1; break;
		case 0x0006: twincobr_flip_screen = 0; twincobr_flip_x_base=0x037; twincobr_flip_y_base=0x01e; break;
		case 0x0007: twincobr_flip_screen = 1; twincobr_flip_x_base=0x085; twincobr_flip_y_base=0x0f2; break;
		case 0x0008: twincobr_bg_ram_bank = 0x0000; break;
		case 0x0009: twincobr_bg_ram_bank = 0x2000; break;
		case 0x000a: twincobr_fg_rom_bank = 0x0000; break;
		case 0x000b: twincobr_fg_rom_bank = 0x1000; break;
		case 0x000e: twincobr_display_on  = 0x0000; break; /* Turn display off */
		case 0x000f: twincobr_display_on  = 0x0001; break; /* Turn display on */
		case 0x000c: if (twincobr_display_on) {
						/* This means assert the INT line to the DSP */
#if LOG_DSP_CALLS
						logerror("Turning DSP on and %s off\n",toaplan_cpu_type[toaplan_main_cpu]);
#endif
						timer_suspendcpu(2, CLEAR, SUSPEND_REASON_HALT);
						cpu_set_irq_line(2, TMS320C10_ACTIVE_INT, ASSERT_LINE);
						timer_suspendcpu(0, ASSERT, SUSPEND_REASON_HALT);
					} break;
		case 0x000d: if (twincobr_display_on) {
						/* This means inhibit the INT line to the DSP */
#if LOG_DSP_CALLS
						logerror("Turning DSP off\n");
#endif
						cpu_set_irq_line(2, TMS320C10_ACTIVE_INT, CLEAR_LINE);
						timer_suspendcpu(2, ASSERT, SUSPEND_REASON_HALT);
					} break;
	}
}



READ_HANDLER( twincobr_sharedram_r )
{
	return twincobr_sharedram[offset / 2];
}

WRITE_HANDLER( twincobr_sharedram_w )
{
	twincobr_sharedram[offset / 2] = data;
}

WRITE_HANDLER( fshark_coin_dsp_w )
{
#if 0
	if (data > 1)
		logerror("%s:%08x  Writing %08x to %08x.\n",toaplan_cpu_type[toaplan_main_cpu],cpu_get_pc(),data,toaplan_port_type[toaplan_main_cpu] - offset);
#endif
	switch (data) {
		case 0x08: if (coin_count) { coin_counter_w(0,1); coin_counter_w(0,0); } break;
		case 0x09: if (coin_count) { coin_counter_w(2,1); coin_counter_w(2,0); } break;
		case 0x0a: if (coin_count) { coin_counter_w(1,1); coin_counter_w(1,0); } break;
		case 0x0b: if (coin_count) { coin_counter_w(3,1); coin_counter_w(3,0); } break;
		case 0x0c: coin_lockout_w(0,1); coin_lockout_w(2,1); break;
		case 0x0d: coin_lockout_w(0,0); coin_lockout_w(2,0); break;
		case 0x0e: coin_lockout_w(1,1); coin_lockout_w(3,1); break;
		case 0x0f: coin_lockout_w(1,0); coin_lockout_w(3,0); coin_count=1; break;
		case 0x00:	/* This means assert the INT line to the DSP */
#if LOG_DSP_CALLS
					logerror("Turning DSP on and %s off\n",toaplan_cpu_type[toaplan_main_cpu]);
#endif
					timer_suspendcpu(2, CLEAR, SUSPEND_REASON_HALT);
					cpu_set_irq_line(2, TMS320C10_ACTIVE_INT, ASSERT_LINE);
					timer_suspendcpu(0, ASSERT, SUSPEND_REASON_HALT);
					break;
		case 0x01:	/* This means inhibit the INT line to the DSP */
#if LOG_DSP_CALLS
					logerror("Turning DSP off\n");
#endif
					cpu_set_irq_line(2, TMS320C10_ACTIVE_INT, CLEAR_LINE);
					timer_suspendcpu(2, ASSERT, SUSPEND_REASON_HALT);
					break;
	}
}

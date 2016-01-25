#include "driver.h"
#include "cpu/tms34010/tms34010.h"

extern unsigned char *exterm_code_rom;
unsigned char *exterm_master_speedup, *exterm_slave_speedup;

static int aimpos1, aimpos2;


WRITE_HANDLER( exterm_host_data_w )
{
	tms34010_host_w(1, offset / TOBYTE(0x00100000), data);
}


READ_HANDLER( exterm_host_data_r )
{
	return tms34010_host_r(1, TMS34010_HOST_DATA);
}


READ_HANDLER( exterm_coderom_r )
{
    return READ_WORD(&exterm_code_rom[offset]);
}


READ_HANDLER( exterm_input_port_0_1_r )
{
	int hi = input_port_1_r(offset);
	if (!(hi & 2)) aimpos1++;
	if (!(hi & 1)) aimpos1--;
	aimpos1 &= 0x3f;

	return ((hi & 0x80) << 8) | (aimpos1 << 8) | input_port_0_r(offset);
}

READ_HANDLER( exterm_input_port_2_3_r )
{
	int hi = input_port_3_r(offset);
	if (!(hi & 2)) aimpos2++;
	if (!(hi & 1)) aimpos2--;
	aimpos2 &= 0x3f;

	return (aimpos2 << 8) | input_port_2_r(offset);
}

WRITE_HANDLER( exterm_output_port_0_w )
{
	/* All the outputs are activated on the rising edge */

	static int last = 0;

	/* Bit 0-1= Resets analog controls */
	if ((data & 0x0001) && !(last & 0x0001))
	{
		aimpos1 = 0;
	}

	if ((data & 0x0002) && !(last & 0x0002))
	{
		aimpos2 = 0;
	}

	/* Bit 13 = Resets the slave CPU */
	if ((data & 0x2000) && !(last & 0x2000))
	{
		cpu_set_reset_line(1,PULSE_LINE);
	}

	/* Bits 14-15 = Coin counters */
	coin_counter_w(0, data & 0x8000);
	coin_counter_w(1, data & 0x4000);

	last = data;
}


READ_HANDLER( exterm_master_speedup_r )
{
	int value = READ_WORD(&exterm_master_speedup[offset]);

	/* Suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == 0xfff4d9b0 && !value)
	{
		cpu_spinuntil_int();
	}

	return value;
}

WRITE_HANDLER( exterm_slave_speedup_w )
{
	/* Suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == 0xfffff050)
	{
		cpu_spinuntil_int();
	}

	WRITE_WORD(&exterm_slave_speedup[offset], data);
}

READ_HANDLER( exterm_sound_dac_speedup_r )
{
	unsigned char *RAM = memory_region(REGION_CPU3);
	int value = RAM[0x0007];

	/* Suspend cpu if it's waiting for an interrupt */
	if (cpu_get_pc() == 0x8e79 && !value)
	{
		cpu_spinuntil_int();
	}

	return value;
}

READ_HANDLER( exterm_sound_ym2151_speedup_r )
{
	/* Doing this won't flash the LED, but we're not emulating that anyhow, so
	   it doesn't matter */

	unsigned char *RAM = memory_region(REGION_CPU4);
	int value = RAM[0x02b6];

	/* Suspend cpu if it's waiting for an interrupt */
	if (  cpu_get_pc() == 0x8179 &&
		!(value & 0x80) &&
		  RAM[0x00bc] == RAM[0x00bb] &&
		  RAM[0x0092] == 0x00 &&
		  RAM[0x0093] == 0x00 &&
		!(RAM[0x0004] & 0x80))
	{
		cpu_spinuntil_int();
	}

	return value;
}


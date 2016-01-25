/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"


READ_HANDLER( scramble_input_port_2_r )
{
	int res;


	res = readinputport(2);

/*logerror("%04x: read IN2\n",cpu_get_pc());*/

	/* avoid protection */
	if (cpu_get_pc() == 0x00e4) res &= 0x7f;

	return res;
}



READ_HANDLER( scramble_protection_r )
{
	//logerror("%04x: read protection\n",cpu_get_pc());

	return 0x6f;
}

READ_HANDLER( scramblk_protection_r )
{
	switch (cpu_get_pc())
	{
	case 0x00a8: return 0xf0;
	case 0x00be: return 0xb0;
	case 0x0c1d: return 0xf0;
	case 0x0c6a: return 0xb0;
	case 0x0ceb: return 0x40;
	case 0x0d37: return 0x60;
	case 0x1ca2: return 0x00;  /* I don't think it's checked */
	case 0x1d7e: return 0xb0;
	default:
		//logerror("%04x: read protection\n",cpu_get_pc());
		return 0;
	}
}

READ_HANDLER( scramblb_protection_1_r )
{
	switch (cpu_get_pc())
	{
	case 0x01da: return 0x80;
	case 0x01e4: return 0x00;
	default:
		//logerror("%04x: read protection 1\n",cpu_get_pc());
		return 0;
	}
}

READ_HANDLER( scramblb_protection_2_r )
{
	switch (cpu_get_pc())
	{
	case 0x01ca: return 0x90;
	default:
		//logerror("%04x: read protection 2\n",cpu_get_pc());
		return 0;
	}
}


READ_HANDLER( mariner_protection_1_r )
{
	return 7;
}
READ_HANDLER( mariner_protection_2_r )
{
	return 3;
}


READ_HANDLER( mariner_pip_r )
{
	//logerror("PC %04x: read port 2\n",cpu_get_pc());
	if (cpu_get_pc() == 0x015a) return 0xff;
	else if (cpu_get_pc() == 0x0886) return 0x05;
	else return 0;
}

READ_HANDLER( mariner_pap_r )
{
	//logerror("PC %04x: read port 3\n",cpu_get_pc());
	if (cpu_get_pc() == 0x015d) return 0x04;
	else return 0;
}



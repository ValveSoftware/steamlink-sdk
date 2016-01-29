#include "driver.h"


unsigned char *mexico86_protection_ram;


/***************************************************************************

 Mexico 86 68705 protection interface

 The following is ENTIRELY GUESSWORK!!!

***************************************************************************/

int mexico86_m68705_interrupt(void)
{
	/* I don't know how to handle the interrupt line so I just toggle it every time. */
	if (cpu_getiloops() & 1)
		cpu_set_irq_line(2,0,CLEAR_LINE);
	else
		cpu_set_irq_line(2,0,ASSERT_LINE);

    return 0;
}



static unsigned char portA_in,portA_out,ddrA;

READ_HANDLER( mexico86_68705_portA_r )
{
//logerror("%04x: 68705 port A read %02x\n",cpu_get_pc(),portA_in);
	return (portA_out & ddrA) | (portA_in & ~ddrA);
}

WRITE_HANDLER( mexico86_68705_portA_w )
{
//logerror("%04x: 68705 port A write %02x\n",cpu_get_pc(),data);
	portA_out = data;
}

WRITE_HANDLER( mexico86_68705_ddrA_w )
{
	ddrA = data;
}



/*
 *  Port B connections:
 *
 *  all bits are logical 1 when read (+5V pullup)
 *
 *  0   W  enables latch which holds data from main Z80 memory
 *  1   W  loads the latch which holds the low 8 bits of the address of
 *               the main Z80 memory location to access
 *  2   W  0 = read input ports, 1 = access Z80 memory
 *  3   W  clocks main Z80 memory access
 *  4   W  selects Z80 memory access direction (0 = write 1 = read)
 *  5   W  clocks a flip-flop which causes IRQ on the main Z80
 *  6   W  not used?
 *  7   W  not used?
 */

static unsigned char portB_in,portB_out,ddrB;

READ_HANDLER( mexico86_68705_portB_r )
{
	return (portB_out & ddrB) | (portB_in & ~ddrB);
}

static int address,latch;

WRITE_HANDLER( mexico86_68705_portB_w )
{
//logerror("%04x: 68705 port B write %02x\n",cpu_get_pc(),data);

	if ((ddrB & 0x01) && (~data & 0x01) && (portB_out & 0x01))
	{
		portA_in = latch;
	}
	if ((ddrB & 0x02) && (data & 0x02) && (~portB_out & 0x02)) /* positive edge trigger */
	{
		address = portA_out;
//if (address >= 0x80) logerror("%04x: 68705 address %02x\n",cpu_get_pc(),portA_out);
	}
	if ((ddrB & 0x08) && (~data & 0x08) && (portB_out & 0x08))
	{
		if (data & 0x10)	/* read */
		{
			if (data & 0x04)
			{
//logerror("%04x: 68705 read %02x from address %04x\n",cpu_get_pc(),shared[0x800+address],address);
				latch = mexico86_protection_ram[address];
			}
			else
			{
//logerror("%04x: 68705 read input port %04x\n",cpu_get_pc(),address);
				latch = readinputport((address & 1) + 1);
			}
		}
		else	/* write */
		{
//logerror("%04x: 68705 write %02x to address %04x\n",cpu_get_pc(),portA_out,address);
				mexico86_protection_ram[address] = portA_out;
		}
	}
	if ((ddrB & 0x20) && (data & 0x20) && (~portB_out & 0x20))
	{
		cpu_irq_line_vector_w(0,0,mexico86_protection_ram[0]);
//		cpu_set_irq_line(0,0,HOLD_LINE);
		cpu_set_irq_line(0,0,PULSE_LINE);
	}
	/*if ((ddrB & 0x40) && (~data & 0x40) && (portB_out & 0x40))
	{
logerror("%04x: 68705 unknown port B bit %02x\n",cpu_get_pc(),data);
	}
	if ((ddrB & 0x80) && (~data & 0x80) && (portB_out & 0x80))
	{
logerror("%04x: 68705 unknown port B bit %02x\n",cpu_get_pc(),data);
	}*/

	portB_out = data;
}

WRITE_HANDLER( mexico86_68705_ddrB_w )
{
	ddrB = data;
}

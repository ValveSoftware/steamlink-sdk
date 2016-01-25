/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"

static unsigned char jedi_control_num = 0;
unsigned char jedi_soundlatch;
unsigned char jedi_soundacklatch;
unsigned char jedi_com_stat;

WRITE_HANDLER( jedi_rom_banksel_w )
{
	unsigned char *RAM = memory_region(REGION_CPU1);

    if (data & 0x01) cpu_setbank (1, &RAM[0x10000]);
    if (data & 0x02) cpu_setbank (1, &RAM[0x14000]);
    if (data & 0x04) cpu_setbank (1, &RAM[0x18000]);
}

WRITE_HANDLER( jedi_sound_reset_w )
{
    if (data & 1)
		cpu_set_reset_line(1,CLEAR_LINE);
    else
		cpu_set_reset_line(1,ASSERT_LINE);
}

READ_HANDLER( jedi_control_r ) {

    if (jedi_control_num == 0)
        return readinputport (2);
    else if (jedi_control_num == 2)
        return readinputport (3);
    return 0;
}

WRITE_HANDLER( jedi_control_w ) {

    jedi_control_num = offset;
}


WRITE_HANDLER( jedi_soundlatch_w ) {
    jedi_soundlatch = data;
    jedi_com_stat |= 0x80;
}

WRITE_HANDLER( jedi_soundacklatch_w ) {
    jedi_soundacklatch = data;
    jedi_com_stat |= 0x40;
}

READ_HANDLER( jedi_soundlatch_r ) {
    jedi_com_stat &= 0x7F;
    return jedi_soundlatch;
}

READ_HANDLER( jedi_soundacklatch_r ) {
    jedi_com_stat &= 0xBF;
    return jedi_soundacklatch;
}

READ_HANDLER( jedi_soundstat_r ) {
    return jedi_com_stat;
}

READ_HANDLER( jedi_mainstat_r ) {
    unsigned char d;

    d = (jedi_com_stat & 0xC0) >> 1;
    d = d | (input_port_1_r(0) & 0x80);
    d = d | 0x1B;
    return d;
}


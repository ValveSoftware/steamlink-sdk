/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"


int lwings_bank_register=0xff;

WRITE_HANDLER( lwings_bankswitch_w ){
	unsigned char *RAM = memory_region(REGION_CPU1);
	int bank = (data>>1)&0x3;
	cpu_setbank(1,&RAM[0x10000 + bank*0x4000]);

	lwings_bank_register=data;
}

int lwings_interrupt(void){
	return 0x00d7; /* RST 10h */
}

int avengers_interrupt( void ){ /* hack */
	static int n;
	if (keyboard_pressed(KEYCODE_S)){ /* test code */
		while (keyboard_pressed(KEYCODE_S))
		{}
		n++;
		n&=0x0f;
		ADPCM_trigger(0, n);
	}

	if( lwings_bank_register & 0x08 ){ /* NMI enable */
		static int s;
		s=!s;
		if( s ){
			return interrupt();
			//cpu_cause_interrupt(0, 0xd7);
		}
		else {
			return Z80_NMI_INT;
		}
	}

	return Z80_IGNORE_INT;
}


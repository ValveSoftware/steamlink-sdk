/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/z80/z80.h"

unsigned char *xevious_sharedram;
static unsigned char interrupt_enable_1,interrupt_enable_2,interrupt_enable_3;

static unsigned char *rom2a;
static unsigned char *rom2b;
static unsigned char *rom2c;
static int xevious_bs[2];

static void *nmi_timer;

WRITE_HANDLER( xevious_halt_w );

/* namco stick number array */
/*
  Input bitmap
    bit0 = UP	 KEY
    bit1 = RIGHT KEY
    bit2 = DOWN  KEY
    bit3 = LEFT  KEY

  Output direction
	      0
	    7	1
	  6   8   2
	    5	3
	      4
 */
unsigned char namco_key[16] =
/*  LDRU,LDR,LDU,LD ,LRU,LR ,LU , L ,DRU,DR ,DU , D ,RU , R , U ,NON  */
  {   5 , 5 , 5 , 5 , 7 , 6 , 7 , 6 , 3 , 3 , 4 , 4 , 1 , 2 , 0 , 8 };

void xevious_init_machine(void)
{
	rom2a = memory_region(REGION_GFX4);
	rom2b = memory_region(REGION_GFX4)+0x1000;
	rom2c = memory_region(REGION_GFX4)+0x3000;

	nmi_timer = 0;

	xevious_halt_w (0, 0);
}

/* emulation for schematic 9B */
WRITE_HANDLER( xevious_bs_w )
{
	xevious_bs[offset & 0x01] = data;
}

READ_HANDLER( xevious_bb_r )
{
	int adr_2b,adr_2c;
	int dat1,dat2;


	/* get BS to 12 bit data from 2A,2B */
	adr_2b = ((xevious_bs[1]&0x7e)<<6)|((xevious_bs[0]&0xfe)>>1);
	if( adr_2b & 1 ){
		/* high bits select */
		dat1 = ((rom2a[adr_2b>>1]&0xf0)<<4)|rom2b[adr_2b];
	}else{
	    /* low bits select */
	    dat1 = ((rom2a[adr_2b>>1]&0x0f)<<8)|rom2b[adr_2b];
	}
	adr_2c = (dat1 & 0x1ff)<<2;
	if( offset & 0x01 )
		adr_2c += (1<<11);	/* signal 4H to A11 */
	if( (xevious_bs[0]&1) ^ ((dat1>>10)&1) )
		adr_2c |= 1;
	if( (xevious_bs[1]&1) ^ ((dat1>>9)&1) )
		adr_2c |= 2;
	if( offset & 0x01 ){
		/* return BB1 */
		dat2 = rom2c[adr_2c];
	}else{
		/* return BB0 */
		dat2 =rom2c[adr_2c];
		/* swap bit 6 & 7 */
		dat2 = (dat2 & 0x3f) | ((dat2 & 0x80) >> 1) | ((dat2 & 0x40) << 1);
		/* flip x & y */
		dat2 ^= (dat1 >> 4) & 0x40;
		dat2 ^= (dat1 >> 2) & 0x80;
	}
	return dat2;
}

READ_HANDLER( xevious_sharedram_r )
{
	return xevious_sharedram[offset];
}

WRITE_HANDLER( xevious_sharedram_w )
{
	xevious_sharedram[offset] = data;
}



READ_HANDLER( xevious_dsw_r )
{
	int bit0,bit1;

	bit0 = (input_port_0_r(0) >> offset) & 1;
	bit1 = (input_port_1_r(0) >> offset) & 1;

	return bit0 | (bit1 << 1);
}

/***************************************************************************

 Emulate the custom IO chip.

***************************************************************************/
static int customio_command;
static int mode,credits;
static int auxcoinpercred,auxcredpercoin;
static int leftcoinpercred,leftcredpercoin;
static int rightcoinpercred,rightcredpercoin;
static unsigned char customio[16];


WRITE_HANDLER( xevious_customio_data_w )
{
	customio[offset] = data;

//logerror("%04x: custom IO offset %02x data %02x\n",cpu_get_pc(),offset,data);

	switch (customio_command)
	{
		case 0xa1:
			if (offset == 0)
			{
				if (data == 0x05)
					mode = 1;	/* go into switch mode */
				else	/* go into credit mode */
				{
					credits = 0;	/* this is a good time to reset the credits counter */
					mode = 0;
				}
			}
			else if (offset == 7)
			{
				auxcoinpercred = customio[1];
				auxcredpercoin = customio[2];
				leftcoinpercred = customio[3];
				leftcredpercoin = customio[4];
				rightcoinpercred = customio[5];
				rightcredpercoin = customio[6];
			}
			break;

		case 0x68:
			if (offset == 6)
			{
				/* it is not known how the parameters control the explosion. */
				/* We just use samples. */
				if (memcmp(customio,"\x40\x40\x40\x01\xff\x00\x20",7) == 0)
					sample_start (0, 0, 0);
				else if (memcmp(customio,"\x30\x40\x00\x02\xdf\x00\x10",7) == 0)
					sample_start (0, 1, 0);
			}
			break;
	}
}


READ_HANDLER( xevious_customio_data_r )
{
	/*if (customio_command != 0x71)
		logerror("%04x: custom IO read offset %02x\n",cpu_get_pc(),offset);*/

	switch (customio_command)
	{
		case 0x71:	/* read input */
		case 0xb1:	/* only issued after 0xe1 (go into credit mode) */
			if (offset == 0)
			{
				if (mode)	/* switch mode */
				{
					/* bit 7 is the service switch */
					return readinputport(4);
				}
				else	/* credits mode: return number of credits in BCD format */
				{
					int in;
					static int leftcoininserted;
					static int rightcoininserted;
					static int auxcoininserted;


					in = readinputport(4);

					/* check if the user inserted a coin */
					if (leftcoinpercred > 0)
					{
						if ((in & 0x10) == 0 && credits < 99)
						{
							leftcoininserted++;
							if (leftcoininserted >= leftcoinpercred)
							{
								credits += leftcredpercoin;
								leftcoininserted = 0;
							}
						}
						if ((in & 0x20) == 0 && credits < 99)
						{
							rightcoininserted++;
							if (rightcoininserted >= rightcoinpercred)
							{
								credits += rightcredpercoin;
								rightcoininserted = 0;
							}
						}
						if ((in & 0x40) == 0 && credits < 99)
						{
							auxcoininserted++;
							if (auxcoininserted >= auxcoinpercred)
							{
								credits += auxcredpercoin;
								auxcoininserted = 0;
							}
						}
					}
					else credits = 2;


					/* check for 1 player start button */
					if ((in & 0x04) == 0)
						if (credits >= 1) credits--;

					/* check for 2 players start button */
					if ((in & 0x08) == 0)
						if (credits >= 2) credits -= 2;

					return (credits / 10) * 16 + credits % 10;
				}
			}
			else if (offset == 1)
			{
				int in;


				in = readinputport(2);	/* player 1 input */
				if (mode == 0)	/* convert joystick input only when in credits mode */
					in = namco_key[in & 0x0f] | (in & 0xf0);
				return in;
			}
			else if (offset == 2)
			{
				int in;


				in = readinputport(3);	/* player 2 input */
				if (mode == 0)	/* convert joystick input only when in credits mode */
					in = namco_key[in & 0x0f] | (in & 0xf0);
				return in;
			}

			break;

		case 0x74:		/* protect data read ? */
			if (offset == 3)
			{
				if (customio[0] == 0x80 || customio[0] == 0x10)
					return 0x05;	/* 1st check */
				else
					return 0x95;  /* 2nd check */
			}
			else return 0x00;
			break;
	}

	return -1;
}


READ_HANDLER( xevious_customio_r )
{
	return customio_command;
}

void xevious_nmi_generate (int param)
{
	cpu_cause_interrupt (0, Z80_NMI_INT);
}


WRITE_HANDLER( xevious_customio_w )
{
	/*if (data != 0x10 && data != 0x71)
		logerror("%04x: custom IO command %02x\n",cpu_get_pc(),data);*/

	customio_command = data;

	switch (data)
	{
		case 0x10:
			if (nmi_timer) timer_remove (nmi_timer);
			nmi_timer = 0;
			return; /* nop */
	}

	nmi_timer = timer_pulse (TIME_IN_USEC (50), 0, xevious_nmi_generate);
}



WRITE_HANDLER( xevious_halt_w )
{
	if (data & 1)
	{
		cpu_set_reset_line(1,CLEAR_LINE);
		cpu_set_reset_line(2,CLEAR_LINE);
	}
	else
	{
		cpu_set_reset_line(1,ASSERT_LINE);
		cpu_set_reset_line(2,ASSERT_LINE);
	}
}



WRITE_HANDLER( xevious_interrupt_enable_1_w )
{
	interrupt_enable_1 = (data&1);
}



int xevious_interrupt_1(void)
{
	if (interrupt_enable_1) return interrupt();
	else return ignore_interrupt();
}



WRITE_HANDLER( xevious_interrupt_enable_2_w )
{
	interrupt_enable_2 = data & 1;
}



int xevious_interrupt_2(void)
{
	if (interrupt_enable_2) return interrupt();
	else return ignore_interrupt();
}



WRITE_HANDLER( xevious_interrupt_enable_3_w )
{
	interrupt_enable_3 = !(data & 1);
}



int xevious_interrupt_3(void)
{
	if (interrupt_enable_3) return nmi_interrupt();
	else return ignore_interrupt();
}

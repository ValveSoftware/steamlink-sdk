/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

Bosconian scoring info

I/O controller command 64h update score values: (set by game code)

60h = switch to player 1
68h = switch to player 2

81h =   10  Asteroid
83h =   20  Cosmo-Mine
87h =   50  I-Type
88h =   60  P-Type
89h =   70  E-Type
8Dh =  200  Spy Ship
93h =  200  Bonus
95h =  300  Bonus
96h =  400  Bonus
98h =  600  Bonus
9Ah =  800  Bonus
A0h =  500  I-Type Formation
A1h = 1000  P-Type Formation
A2h = 1500  E-Type Formation
A3h = 2000  Bonus
A5h = 3000  Bonus
A6h = 4000  Bonus
A7h = 5000  Bonus
A8h = 6000  Bonus
A9h = 7000  Bonus
B7h =  100  I-Type Leader
B8h =  120  P-Type Leader
B9h =  140  E-Type Leader

Bonuses are given at the end of a round if the game is set to auto
difficulty and the round is completed on one life. Bonus values are:

 100x3  95h
 100x4  96h
 100x8  9Ah
 200x4  96h,96h
 200x8  9Ah,9Ah
 300x8  A3h,96h
 400x8  A5h,93h
 500x8  A3h,A3h
 600x8  A6h,9Ah
 700x8  A7h,98h
 800x8  A8h,96h
 900x8  A9h,93h
1000x8  A6h,A6h


I/O controller command 84h set bonus values: (set by game code)

Byte 0: always 10h
Byte 1: indicator (20h=first bonus, 30h=interval bonus, others=unknown)
Byte 2: BCD score (--ss----)
Byte 3: BCD score (----ss--)
Byte 4: BCD score (------ss)

Indicator values 20h and 30h are sent once during startup based upon
the dip switch settings, other values are sent during gameplay.
The default bonus setting is 20000, 70000, and every 70000.


I/O controller command 94h read score returned value: (read by game code)

Byte 0: BCD score (fs------) and flags
Byte 1: BCD score (--ss----)
Byte 2: BCD score (----ss--)
Byte 3: BCD score (------ss)

Flags: 80h=high score, 40h=first bonus, 20h=interval bonus


Scores should be reset to 0 on I/O controller command C1h.

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"


unsigned char *bosco_sharedram;
static unsigned char interrupt_enable_1,interrupt_enable_2,interrupt_enable_3;
static int		HiScore;
int		Score,Score1,Score2;
int		NextBonus,NextBonus1,NextBonus2;
int		FirstBonus,IntervalBonus;

static int credits;

void bosco_sample_play(int, int);
void bosco_vh_interrupt(void);

static void *nmi_timer_1, *nmi_timer_2;

WRITE_HANDLER( bosco_halt_w );

void bosco_init_machine(void)
{
	credits = 0;
	HiScore = 20000;
	nmi_timer_1 = nmi_timer_2 = 0;
	bosco_halt_w (0, 0);

	memory_region(REGION_CPU1)[0x8c00] = 1;
	memory_region(REGION_CPU1)[0x8c01] = 1;
}


READ_HANDLER( bosco_sharedram_r )
{
	return bosco_sharedram[offset];
}



WRITE_HANDLER( bosco_sharedram_w )
{
	bosco_sharedram[offset] = data;
}



READ_HANDLER( bosco_dsw_r )
{
	int bit0,bit1;


	bit0 = (input_port_0_r(0) >> offset) & 1;
	bit1 = (input_port_1_r(0) >> offset) & 1;

	return bit0 | (bit1 << 1);
}



/***************************************************************************

 Emulate the custom IO chip.

***************************************************************************/
static int customio_command_1;
static unsigned char customio_1[16];
static int mode;


WRITE_HANDLER( bosco_customio_data_1_w )
{
	customio_1[offset] = data;

//logerror("%04x: custom IO 1 offset %02x data %02x\n",cpu_get_pc(),offset,data);

	switch (customio_command_1)
	{
		case 0x48:
			if (offset == 1)
			{
				switch(customio_1[0])
				{
					case 0x20:	 //		Mid Bang
						sample_start (0, 0, 0);
						break;
					case 0x10:	 //		Big Bang
						sample_start (1, 1, 0);
						break;
					case 0x50:	 //		Shot
						sample_start (2, 2, 0);
						break;
				}
			}
			break;

		case 0x64:
			if (offset == 0)
			{
				switch(customio_1[0])
				{
					case 0x60:	/* 1P Score */
						Score2 = Score;
						Score = Score1;
						NextBonus2 = NextBonus;
						NextBonus = NextBonus1;
						break;
					case 0x68:	/* 2P Score */
						Score1 = Score;
						Score = Score2;
						NextBonus1 = NextBonus;
						NextBonus = NextBonus2;
						break;
					case 0x81:
						Score += 10;
						break;
					case 0x83:
						Score += 20;
						break;
					case 0x87:
						Score += 50;
						break;
					case 0x88:
						Score += 60;
						break;
					case 0x89:
						Score += 70;
						break;
					case 0x8D:
						Score += 200;
						break;
					case 0x93:
						Score += 200;
						break;
					case 0x95:
						Score += 300;
						break;
					case 0x96:
						Score += 400;
						break;
					case 0x98:
						Score += 600;
						break;
					case 0x9A:
						Score += 800;
						break;
					case 0xA0:
						Score += 500;
						break;
					case 0xA1:
						Score += 1000;
						break;
					case 0xA2:
						Score += 1500;
						break;
					case 0xA3:
						Score += 2000;
						break;
					case 0xA5:
						Score += 3000;
						break;
					case 0xA6:
						Score += 4000;
						break;
					case 0xA7:
						Score += 5000;
						break;
					case 0xA8:
						Score += 6000;
						break;
					case 0xA9:
						Score += 7000;
						break;
					case 0xB7:
						Score += 100;
						break;
					case 0xB8:
						Score += 120;
						break;
					case 0xB9:
						Score += 140;
						break;
					default:
						//logerror("unknown score: %02x\n",customio_1[0]);
					break;
				}
			}
			break;
		case 0x84:
			if (offset == 2)
			{
				int hi = (data / 16);
				int mid = (data % 16);
				if (customio_1[1] == 0x20)
					FirstBonus = (hi * 100000) + (mid * 10000);
				if (customio_1[1] == 0x30)
					IntervalBonus = (hi * 100000) + (mid * 10000);
			}
			else if (offset == 3)
			{
				int lo = (data / 16);
				if (customio_1[1] == 0x20)
					FirstBonus = FirstBonus + (lo * 1000);
				if (customio_1[1] == 0x30)
					IntervalBonus = IntervalBonus + (lo * 1000);
			}
			break;
	}
}


READ_HANDLER( bosco_customio_data_1_r )
{
	switch (customio_command_1)
	{
		case 0x71:
			if (offset == 0)
			{
				int p4 = readinputport (4);

				/* check if the user inserted a coin */
				if ((p4 & 0x10) == 0 && credits < 99)
					credits++;

				/* check if the user inserted a coin */
				if ((p4 & 0x20) == 0 && credits < 99)
					credits++;

				/* check if the user inserted a coin */
				if ((p4 & 0x40) == 0 && credits < 99)
					credits++;

				/* check for 1 player start button */
				if ((p4 & 0x04) == 0 && credits >= 1)
					credits--;

				/* check for 2 players start button */
				if ((p4 & 0x08) == 0 && credits >= 2)
					credits -= 2;

				if (mode)	/* switch mode */
					return (p4 & 0x80);
				else	/* credits mode: return number of credits in BCD format */
					return (credits / 10) * 16 + credits % 10;
			}
			else if (offset == 1)
			{
				int in = readinputport(2), dir;

			/*
				  Direction is returned as shown below:
								0
							7		1
						6				2
							5		3
								4
				  For the previous direction return 8.
			 */
				dir = 8;
				if ((in & 0x01) == 0)		/* up */
				{
					if ((in & 0x02) == 0)	/* right */
						dir = 1;
					else if ((in & 0x08) == 0) /* left */
						dir = 7;
					else
						dir = 0;
				}
				else if ((in & 0x04) == 0)	/* down */
				{
					if ((in & 0x02) == 0)	/* right */
						dir = 3;
					else if ((in & 0x08) == 0) /* left */
						dir = 5;
					else
						dir = 4;
				}
				else if ((in & 0x02) == 0)	/* right */
					dir = 2;
				else if ((in & 0x08) == 0) /* left */
					dir = 6;

				/* check fire (both impulse and hold, boscomd2 has autofire) */
				dir |= (in & 0x30);

				return dir;
			}
			break;

		case 0x94:
			if (offset == 0)
			{
				int flags = 0;
				int lo = (Score / 1000000) % 10;
				if (Score >= HiScore)
				{
					HiScore = Score;
					flags |= 0x80;
				}
				if (Score >= NextBonus)
				{
					if (NextBonus == FirstBonus)
					{
						NextBonus = IntervalBonus;
						flags |= 0x40;
					}
					else
					{
						NextBonus += IntervalBonus;
						flags |= 0x20;
					}
				}
				return lo | flags;
			}
			else if (offset == 1)
			{
				int hi = (Score / 100000) % 10;
				int lo = (Score / 10000) % 10;
				return (hi * 16) + lo;
			}
			else if (offset == 2)
			{
				int hi = (Score / 1000) % 10;
				int lo = (Score / 100) % 10;
				return (hi * 16) + lo;
			}

			else if (offset == 3)
			{
				int hi = (Score / 10) % 10;
				int lo = Score % 10;
				return (hi * 16) + lo;
			}
			break;

		case 0x91:
			if (offset <= 2)
				return 0;
			break;
	}

	return -1;
}


READ_HANDLER( bosco_customio_1_r )
{
	return customio_command_1;
}

void bosco_nmi_generate_1 (int param)
{
	cpu_cause_interrupt (0, Z80_NMI_INT );
}

WRITE_HANDLER( bosco_customio_1_w )
{
	//if (data != 0x10)
	//	logerror("%04x: custom IO 1 command %02x\n",cpu_get_pc(),data);

	customio_command_1 = data;

	switch (data)
	{
		case 0x10:
			if (nmi_timer_1) timer_remove (nmi_timer_1);
			nmi_timer_1 = 0;
			return;

		case 0x61:
			mode = 1;
			break;

		case 0xC1:
			Score = 0;
			Score1 = 0;
			Score2 = 0;
			NextBonus = FirstBonus;
			NextBonus1 = FirstBonus;
			NextBonus2 = FirstBonus;
			break;

		case 0xC8:
			break;

		case 0x84:
			break;

		case 0x91:
			mode = 0;
			break;

		case 0xa1:
			mode = 1;
			break;
	}

	nmi_timer_1 = timer_pulse (TIME_IN_USEC (50), 0, bosco_nmi_generate_1);
}



/***************************************************************************

 Emulate the second (!) custom IO chip.

***************************************************************************/
static int customio_command_2;
static unsigned char customio_2[16];

WRITE_HANDLER( bosco_customio_data_2_w )
{
	customio_2[offset] = data;

//logerror("%04x: custom IO 2 offset %02x data %02x\n",cpu_get_pc(),offset,data);
	switch (customio_command_2)
	{
		case 0x82:
			if (offset == 2)
			{
				switch(customio_2[0])
				{
					case 1: // Blast Off
						bosco_sample_play(0x0020 * 2, 0x08D7 * 2);
						break;
					case 2: // Alert, Alert
						bosco_sample_play(0x8F7 * 2, 0x0906 * 2);
						break;
					case 3: // Battle Station
						bosco_sample_play(0x11FD * 2, 0x07DD * 2);
						break;
					case 4: // Spy Ship Sighted
						bosco_sample_play(0x19DA * 2, 0x07DE * 2);
						break;
					case 5: // Condition Red
						bosco_sample_play(0x21B8 * 2, 0x079F * 2);
						break;
				}
			}
			break;
	}
}


READ_HANDLER( bosco_customio_data_2_r )
{
	switch (customio_command_2)
	{
		case 0x91:
			if (offset == 2)
				return cpu_readmem16(0x89cc);
			else if (offset <= 3)
				return 0;
			break;
	}

	return -1;
}


READ_HANDLER( bosco_customio_2_r )
{
	return customio_command_2;
}

void bosco_nmi_generate_2 (int param)
{
	cpu_cause_interrupt (1, Z80_NMI_INT);
}

WRITE_HANDLER( bosco_customio_2_w )
{
	//if (data != 0x10)
	//	logerror("%04x: custom IO 2 command %02x\n",cpu_get_pc(),data);

	customio_command_2 = data;

	switch (data)
	{
		case 0x10:
			if (nmi_timer_2) timer_remove (nmi_timer_2);
			nmi_timer_2 = 0;
			return;
	}

	nmi_timer_2 = timer_pulse (TIME_IN_USEC (50), 0, bosco_nmi_generate_2);
}






WRITE_HANDLER( bosco_halt_w )
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



WRITE_HANDLER( bosco_interrupt_enable_1_w )
{
	interrupt_enable_1 = (data&1);
}



int bosco_interrupt_1(void)
{
	bosco_vh_interrupt();	/* update the background stars position */

	if (interrupt_enable_1) return interrupt();
	else return ignore_interrupt();
}



WRITE_HANDLER( bosco_interrupt_enable_2_w )
{
	interrupt_enable_2 = data & 1;
}



int bosco_interrupt_2(void)
{
	if (interrupt_enable_2) return interrupt();
	else return ignore_interrupt();
}



WRITE_HANDLER( bosco_interrupt_enable_3_w )
{
	interrupt_enable_3 = !(data & 1);
}



int bosco_interrupt_3(void)
{
	if (interrupt_enable_3) return nmi_interrupt();
	else return ignore_interrupt();
}

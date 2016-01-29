/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"


unsigned char *superpac_sharedram;
unsigned char *superpac_customio_1,*superpac_customio_2;

static int coin1, coin2, credits, start1, start2;

static int crednum[] = { 1, 2, 3, 6, 7, 1, 3, 1 };
static int credden[] = { 1, 1, 1, 1, 1, 2, 2, 3 };

void superpac_init_machine(void)
{
	/* Reset all flags */
	coin1 = coin2 = start1 = start2 = credits = 0;
}


READ_HANDLER( superpac_sharedram_r )
{
	return superpac_sharedram[offset];
}

WRITE_HANDLER( superpac_sharedram_w )
{
	superpac_sharedram[offset] = data;
}


WRITE_HANDLER( superpac_reset_2_w )
{
	cpu_set_reset_line(1,PULSE_LINE);
}


void superpac_update_credits (void)
{
	int val = readinputport (3) & 0x0f, temp;
	if (val & 1)
	{
		if (!coin1) credits++, coin1++;
	}
	else coin1 = 0;

	if (val & 2)
	{
		if (!coin2) credits++, coin2++;
	}
	else coin2 = 0;

	temp = readinputport (1) & 7;
	val = readinputport (3) >> 4;
	if (val & 1)
	{
		if (!start1 && credits >= credden[temp]) credits -= credden[temp], start1++;
	}
	else start1 = 0;

	if (val & 2)
	{
		if (!start2 && credits >= 2 * credden[temp]) credits -= 2 * credden[temp], start2++;
	}
	else start2 = 0;
}


READ_HANDLER( superpac_customio_1_r )
{
	int val, temp;

	/* default return value */
	val = superpac_customio_1[offset];

	superpac_update_credits();

	switch (superpac_customio_1[8])
	{
	/* mode 1 & 3 are used by Pac & Pal, and returns actual important values */
	case 1:
	case 3:
		switch (offset)
		{
		case 0:
			val = readinputport (3) & 0x0f;
			break;

		case 1:
			val = readinputport (2) & 0x0f;
			break;

		case 2:
			val = readinputport (4) & 0x0f;
			break;

		case 3:
			val = (readinputport (3) >> 4) & 3;
			val |= val << 2;

			/* I don't know the exact mix, but the low bit is used both for
			   the fire button and for player 1 start; I'm just ORing for now */
			val |= readinputport (2) >> 4;
			break;

		case 4:
		case 5:
		case 6:
		case 7:
			val = 0x0f;
			break;
		}

		break;

	/* mode 4 is the standard, and returns actual important values */
	case 4:
		switch (offset)
		{
		case 0:
			temp = readinputport (1) & 7;
			val = (credits * crednum[temp] / credden[temp]) / 10;
			break;

		case 1:
			temp = readinputport (1) & 7;
			val = (credits * crednum[temp] / credden[temp]) % 10;
			break;

		case 4:
			val = readinputport (2) & 0x0f;
			break;

		case 5:
			val = readinputport (2) >> 4;
			break;

		case 6:
			val = readinputport (4) & 0x0f;
			break;

		case 7:
			val = readinputport (4) >> 4;
			break;
		}

		break;

	/* mode 8 is the test mode: always return 0 for these locations */
	case 8:
		credits = 0;
		if (offset >= 9 && offset <= 15)
			val = 0;
		break;
	}

	return val;
}


READ_HANDLER( superpac_customio_2_r )
{
	int val;

	/* default return value */
	val = superpac_customio_2[offset];

	switch (superpac_customio_2[8])
	{
	/* mode 3 is the standard for Pac & Pal, and returns actual important values */
	case 3:
		switch (offset)
		{
		case 0:
		case 1:
		case 2:
		case 3:
			val = 0;
			break;

		case 4:
			val = readinputport (0) & 0x0f;
			break;

		case 5:
			val = readinputport (1) >> 4;
			break;

		case 6:
			val = readinputport (1) & 0x0f;
			break;

		case 7:
			val = (readinputport (3) >> 4) & 0x0c;
			break;
		}

		break;

	/* mode 9 is the standard, and returns actual important values */
	case 9:
		switch (offset)
		{
		case 0:
			val = readinputport (1) & 0x0f;
			break;

		case 1:
			val = readinputport (1) >> 4;
			break;

		case 2:
			val = 0;
			break;

		case 3:
			val = readinputport (0) & 0x0f;
			break;

		case 4:
			val = readinputport (0) >> 4;
			break;

		case 5:
			val = 0;
			break;

		case 6:
			val = (readinputport (3) >> 4) & 0x0c;
			break;

		case 7:
			val = 0;
			break;
		}

		break;

	/* mode 8 is the test mode: always return 0 for these locations */
	case 8:
		credits = 0;
		if (offset >= 9 && offset <= 15)
			val = 0;
		break;
	}

	return val;
}


WRITE_HANDLER( superpac_interrupt_enable_w )
{
	interrupt_enable_w(0, offset);
}


WRITE_HANDLER( superpac_cpu_enable_w )
{
	cpu_set_halt_line(1, offset ? CLEAR_LINE : ASSERT_LINE);
}

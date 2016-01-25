/*
 *	Emulation for the NEC PD4990A.
 *
 *	The PD4990A is a serial I/O Calendar & Clock IC used in the
 *      NEO GEO and probably a couple of other machines.
 */

#include "driver.h"
#include "machine/pd4990a.h"

/* Set the data in the chip to Monday 09/09/73 00:00:00     */
/* If you ever read this Leejanne, you know what I mean :-) */
int seconds=0x00;		/* BCD */
int minutes=0x00;		/* BCD */
int hours=0x00;		/* BCD */
int days=0x09;		/* BCD */
int month=9;		/* Hexadecimal form */
int year=0x73;		/* BCD */
int weekday=1;		/* BCD */

int retraces=0;			/* Assumes 60 retraces a second */
int coinflip=0;			/* Pulses a bit in order to simulate */
					/* test output */
int outputbit=0;
int bitno=0;

void addretrace (void) {
	coinflip ^= 1;
	retraces++;
	if (retraces == 60) {
		retraces = 0;
		seconds++;
		if ((seconds & 0x0f) == 10) {
			seconds &= 0xf0;
			seconds += 0x10;
			if ( seconds == 0x60) {
				seconds = 0;
				minutes++;
				if ( (minutes & 0x0f) == 10) {
					minutes &= 0xf0;
					minutes += 0x10;
					if (minutes == 0x60) {
						minutes = 0;
						hours++;
						if ( (hours & 0x0f) == 10 ){
							hours &= 0xf0;
							hours += 0x10;
						}
						if (hours == 0x24) {
							hours = 0;
							increment_day();
						}
					}
				}
			}
		}
	}
}

void	increment_day(void)
{
	int	real_year;

	days++;
	if ((days & 0x0f) == 10)
	{
		days &= 0xf0;
		days += 0x10;
	}

	weekday++;
	if (weekday==7)
		weekday=0;

	switch(month)
	{
	case	1:
	case	3:
	case	5:
	case	7:
	case	8:
	case	10:
	case	12:
		if (days==0x32)
		{
			days=1;
			increment_month();
		}
		break;
	case	2:
		real_year = (year>>4)*10 + (year&0xf);
		if (real_year%4)	/* There are some rare exceptions, but I don't care ... */
		{
			if (days==0x29)
			{
				days=1;
				increment_month();
			}
		}
		else
		{
			if (days==0x30)
			{
				days=1;
				increment_month();
			}
		}
		break;
	case	4:
	case	6:
	case	9:
	case	11:
		if (days==0x31)
		{
			days=1;
			increment_month();
		}
		break;
	}
}

void	increment_month(void)
{
	month++;
	if (month==13) {
		month=1;
		year++;
		if ((year & 0x0f) == 10) {
			year &= 0xf0;
			year += 0x10;
		}
		if (year == 0xA0)		/* Happy new year 2000 !!! */
			year = 0;
	}
}

int read_4990_testbit(void) {
	return (coinflip);
}

int read_4990_databit(void)
{
	return (outputbit);
}

WRITE_HANDLER( write_4990_control_w )
{

	data &= 0xff;

	switch (data) {
		case 0x00:	/* Load Register */
				switch(bitno) {
					case 0x00:
					case 0x01:
					case 0x02:
					case 0x03:
					case 0x04:
					case 0x05:
					case 0x06:
					case 0x07: outputbit=(seconds >> bitno) & 0x01;
						   break;
					case 0x08:
					case 0x09:
					case 0x0a:
					case 0x0b:
					case 0x0c:
					case 0x0d:
					case 0x0e:
					case 0x0f: outputbit=(minutes >> (bitno-8)) & 0x01;
						   break;
					case 0x10:
					case 0x11:
					case 0x12:
					case 0x13:
					case 0x14:
					case 0x15:
					case 0x16:
					case 0x17: outputbit=(hours >> (bitno-16)) & 0x01;
						   break;
					case 0x18:
					case 0x19:
					case 0x1a:
					case 0x1b:
					case 0x1c:
					case 0x1d:
					case 0x1e:
					case 0x1f: outputbit=(days >> (bitno-24)) & 0x01;
						   break;
					case 0x20:
					case 0x21:
					case 0x22:
					case 0x23: outputbit=(weekday >> (bitno-32)) & 0x01;
						   break;
					case 0x24:
					case 0x25:
					case 0x26:
					case 0x27: outputbit=(month >> (bitno-36)) & 0x01;
						   break;
					case 0x28:
					case 0x29:
					case 0x2a:
					case 0x2b:
					case 0x2c:
					case 0x2d:
					case 0x2e:
					case 0x2f: outputbit=(year >> (bitno-40)) & 0x01;
						   break;

				}
				break;

		case 0x04:	/* Start afresh with shifting */
				bitno=0;
				break;

		case 0x02:	/* shift one position */
				bitno++;

		default:	/* Unhandled value */
				break;
	}
}

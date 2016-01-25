/*************************************************************************

  Rainbow Island C-Chip Protection

*************************************************************************/

#include "driver.h"

/*************************************
 *
 *		Interrupt handler
 *
 *************************************/

int rainbow_interrupt(void)
{
	return 4;  /* Interrupt vector 4 */
}

/*************************************
 *
 * Rainbow C-Chip, Protection
 *
 *************************************/

static int FrameBank,
           ChipOffset=0;

/*************************************
 *
 * Writes to C-Chip - Important Bits
 *
 *************************************/

WRITE_HANDLER( rainbow_c_chip_w )
{
  switch(offset+1)
  {
  	case 0x001: if ((data & 0xff) == 1)	ChipOffset = 0x4950;
       			break;

    case 0x01b: ChipOffset = 0;
       			break;

    case 0xc01: FrameBank = (data & 0xff) << 1;
    			break;
  }
}

/*************************************
 *
 * Reads from C-Chip
 *
 *************************************/

extern int mrh_bank1(int address);

READ_HANDLER( rainbow_c_chip_r )
{
  unsigned char *CROM = memory_region(REGION_USER1);	/* C-Chip Dump */

  int Address;
  int Data1,Data2;
  int ans;

  /* Start with standard return from rom image */

  Address = ChipOffset
          + FrameBank
          + ((READ_WORD(cpu_bankbase[1] + 0x1048) & 0xFF) << 4);

  Data2   = (CROM[Address] << 8) + CROM[Address+1];
  Data1   = ((CROM[Address+2] << 8) + CROM[Address+3]) - Data2;

  if (Data1 == 0)
  {
  	ans = 0;
  }
  else
  {
  	if (Data1 <= (offset >> 1))
    {
  	  ans = 0;
    }
    else
    {
      ans = CROM[(offset >> 1) + Data2];
    }
  }

  /* Overrides for specific locations */

  switch(offset+1)
  {
				/* Input Ports */

  	case 0x007: if (FrameBank==0) ans=input_port_2_r(offset);
                break;

    case 0x009: if (FrameBank==0) ans=input_port_3_r(offset);
                break;

    case 0x00b: if (FrameBank==0) ans=input_port_4_r(offset);
                break;

    case 0x00d: if (FrameBank==0) ans=input_port_5_r(offset);
                break;


			    /* Program expects the following results */

    case 0x001: ans=0xff;					/* Won't draw screen until */
                break;						/*   this is set to 0xff   */
                							/*    Countdown Timer ?    */

  	case 0x201: ans=0xff;					/* Level Data Ready */
                break;

    case 0x803: ans=0x01;					/* C-Chip Check ? */
                break;


                /* These are taken from a lookup table */
                /* in the bootleg, and not from C-Chip */

    case 0x295: ans=0;						/* G Below */
			    break;

    case 0x297: ans=0;						/* G Right */
			    break;

    case 0x299: ans=0x10;					/* O below */
			    break;

    case 0x29b: ans=0x10;					/* O Right */
			    break;

    case 0x29d: ans=0x20;					/* A Below */
			    break;

    case 0x29f: ans=0x20;					/* A Right */
			    break;

    case 0x2a1: ans=0x30;					/* L Below */
			    break;

    case 0x2a3: ans=0x38;					/* L Right */
			    break;

    case 0x2a5: ans=0x40;					/* I Below */
			    break;

    case 0x2a7: ans=0x50;					/* I Right */
			    break;

    case 0x2a9: ans=0x50;					/* N Below */
			    break;

    case 0x2ab: ans=0x60;					/* N Right */
			    break;
  }

  return ans;
}

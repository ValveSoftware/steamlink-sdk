/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"

unsigned char *phozon_snd_sharedram;
unsigned char *phozon_spriteram;
unsigned char *phozon_customio_1, *phozon_customio_2;
static int credits, coincounter1, coincounter2;

void phozon_init_machine( void )
{
    credits = coincounter1 = coincounter2 = 0;
	cpu_set_halt_line(1, CLEAR_LINE);
	cpu_set_halt_line(2, CLEAR_LINE);
}

/* memory handlers */
READ_HANDLER( phozon_spriteram_r ){
    return phozon_spriteram[offset];
}

WRITE_HANDLER( phozon_spriteram_w ){
   phozon_spriteram[offset] = data;
}

READ_HANDLER( phozon_snd_sharedram_r ){
    return phozon_snd_sharedram[offset];
}

WRITE_HANDLER( phozon_snd_sharedram_w ){
    phozon_snd_sharedram[offset] = data;
}

/* cpu control functions */
WRITE_HANDLER( phozon_cpu2_enable_w ){
	cpu_set_halt_line(1, offset ? CLEAR_LINE : ASSERT_LINE);
}

WRITE_HANDLER( phozon_cpu3_enable_w ){
	cpu_set_halt_line(2, offset ? CLEAR_LINE : ASSERT_LINE);
}

WRITE_HANDLER( phozon_cpu3_reset_w ){
	cpu_set_reset_line(2,PULSE_LINE);
}

/************************************************************************************
*																					*
*           Phozon custom I/O chips (preliminary)										*
*																					*
************************************************************************************/

WRITE_HANDLER( phozon_customio_1_w )
{
	phozon_customio_1[offset] = data;
}

WRITE_HANDLER( phozon_customio_2_w )
{
    phozon_customio_2[offset] = data;
}

static int credmoned [] = { 1, 1, 1, 1, 1, 2, 2, 3 };
static int monedcred [] = { 1, 2, 3, 6, 7, 1, 3, 1 };

READ_HANDLER( phozon_customio_1_r )
{
    int mode, val, temp1, temp2;

    mode = phozon_customio_1[8];
    if (mode == 3)	/* normal mode */
    {
        switch (offset)
        {
            case 0:     /* Coin slots */
            {
                static int lastval;

                val = (readinputport( 2 ) >> 4) & 0x03;
                temp1 = readinputport( 0 ) & 0x07;
                temp2 = (readinputport( 0 ) >> 5) & 0x07;

                /* bit 0 is a trigger for the coin slot 1 */
                if ((val & 1) && ((val ^ lastval) & 1))
                {
                    coincounter1++;
                    if (coincounter1 >= credmoned[temp1])
                    {
                        credits += monedcred [temp1];
                        coincounter1 -= credmoned [temp1];
                    }
                }
                /* bit 1 is a trigger for the coin slot 2 */
                if ((val & 2) && ((val ^ lastval) & 2))
                {
                    coincounter2++;
                    if (coincounter2 >= credmoned[temp2])
                    {
                        credits += monedcred [temp2];
                        coincounter2 -= credmoned [temp2];
                    }
                }

                if (credits > 99)
                    credits = 99;

                return lastval = val;
            }
                break;
            case 1:
            {
                static int lastval;

                val = readinputport( 2 ) & 0x03;
                temp1 = readinputport( 0 ) & 0x07;
                temp2 = (readinputport( 0 ) >> 5) & 0x07;

                /* bit 0 is a trigger for the 1 player start */
                if ((val & 1) && ((val ^ lastval) & 1))
                {
                    if (credits > 0)
                        credits--;
                    else
                        val &= ~1;   /* otherwise you can start with no credits! */
                }
                /* bit 1 is a trigger for the 2 player start */
                if ((val & 2) && ((val ^ lastval) & 2))
                {
                    if (credits >= 2)
                        credits -= 2;
                    else
                        val &= ~2;   /* otherwise you can start with no credits! */
                }
                return lastval = val;
            }
                break;
            case 2:
                return (credits / 10);      /* high BCD of credits */
                break;
            case 3:
                return (credits % 10);      /* low BCD of credits */
                break;
            case 4:
                return (readinputport( 3 ) & 0x0f);   /* 1P controls */
                break;
            case 5:
                return (readinputport( 4 ) & 0x03);   /* 1P button 1 */
                break;
            default:
                return 0x0;
        }
    }
    else if (mode == 5)	/* IO tests */
    {
        switch (offset)
        {
			case 0x00: val = 0x00; break;
			case 0x01: val = 0x02; break;
			case 0x02: val = 0x03; break;
			case 0x03: val = 0x04; break;
			case 0x04: val = 0x05; break;
			case 0x05: val = 0x06; break;
			case 0x06: val = 0x0c; break;
			case 0x07: val = 0x0a; break;
            default:
				val = phozon_customio_1[offset];
        }
    }
	else if (mode == 1)	/* test mode controls */
    {
        switch (offset)
        {
			case 4:
				return (readinputport( 2 ) & 0x03);	/* start 1 & 2 */
				break;
			case 5:
				return (readinputport( 3 ) &0x0f);	/* 1P controls */
				break;
			case 7:
				return (readinputport( 4 ) & 0x03);	/* 1P button 1 */
				break;
			default:
				return phozon_customio_1[offset];
        }
    }
	else
		val = phozon_customio_1[offset];
    return val;
}

READ_HANDLER( phozon_customio_2_r )
{
    int mode, val;

    mode = phozon_customio_2[8];
    if (mode == 8)	/* IO tests */
    {
        switch (offset)
        {
			case 0x00: val = 0x01; break;
			case 0x01: val = 0x0c; break;
            default:
				val = phozon_customio_2[offset];
        }
    }
	else if (mode == 9)
    {
        switch (offset)	/* TODO: coinage B & check bonus life bits */
        {
            case 0:
                val = (readinputport( 0 ) & 0x08) >> 3;		/* lives (bit 0) */
				val |= (readinputport( 0 ) & 0x01) << 2;	/* coinage A (bit 0) */
				val |= (readinputport( 0 ) & 0x04) << 1;	/* coinage A (bit 2) */
                break;
            case 1:
				val = (readinputport( 0 ) & 0x10) >> 4;		/* lives (bit 1) */
				val |= (readinputport( 1 ) & 0xc0) >> 5;	/* bonus life (bits 1 & 0) */
				val |= (readinputport( 0 ) & 0x02) << 2;	/* coinage A (bit 1) */
                break;
			case 2:
				val = (readinputport( 1 ) & 0x07) << 1;		/* rank */
                break;
			case 4:	/* some bits of coinage B (not implemented yet) */
				val = 0;
                break;
            case 6:
                val = readinputport( 1 ) & 0x08;			/* test mode */
				val |= (readinputport( 2 ) & 0x80) >> 5;	/* cabinet */
                break;
            default:
				val = phozon_customio_2[offset];
        }
	}
	else
		val = phozon_customio_2[offset];
	return val;
}

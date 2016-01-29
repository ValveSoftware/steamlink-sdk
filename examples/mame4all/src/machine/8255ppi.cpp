/* INTEL 8255 PPI I/O chip */


/* NOTE: When port is input,  then data present on the ports
  outputs is 0x0ff */

/* KT 10/01/2000 - Added bit set/reset feature for control port */
/*               - Added more accurate port i/o data handling */
/*               - Added output reset when control mode is programmed */



#include "driver.h"
#include "machine/8255ppi.h"

static ppi8255_interface	*intf; /* local copy of the intf */

typedef struct {
	int		groupA_mode;
	int		groupB_mode;
        /* input output status */
	int		io[3];
        /* data written to ports */
	int		latch[3];
        /* control */
        int		control;
} ppi8255;

static ppi8255 chips[MAX_8255];

void ppi8255_init( ppi8255_interface *intfce ) {
	int i;

	intf = intfce; /* keep a local pointer to the interface */


	for ( i = 0; i < intf->num; i++ ) {
		chips[i].groupA_mode = 0; /* group a mode */
		chips[i].groupB_mode = 0; /* group b mode */
		chips[i].io[0] = 0xff; /* all inputs */
		chips[i].io[1] = 0xff; /* all inputs */
		chips[i].io[2] = 0xff; /* all inputs */
		chips[i].latch[0] = 0x00; /* clear latch */
		chips[i].latch[1] = 0x00; /* clear latch */
		chips[i].latch[2] = 0x00; /* clear latch */
		chips[i].control = 0x1b;
	}
}

int ppi8255_r( int which, int offset ) {
	ppi8255		*chip;

	/* Some bounds checking */
	if ( which > intf->num ) {
		//logerror("Attempting to read an unmapped 8255 chip\n" );
		return 0;
	}

	if ( offset > 3 ) {
		//logerror("Attempting to read an invalid 8255 register\n" );
		return 0;
	}

	chip = &chips[which];


	switch( offset )
        {
		case 0: /* Port A read */
			if ( chip->io[0] == 0 )
                        {
                                /* output */
				return chip->latch[0];
                        }
                        else
                        {
                                /* input */
				if ( intf->portA_r )
					return (*intf->portA_r)( which );
			}
		break;

                case 1: /* Port B read */
			if ( chip->io[1] == 0 )
                        {
                                /* output */
				return chip->latch[1];
                        }
                        else
                        {
                                /* input */
				if ( intf->portB_r )
					return (*intf->portB_r)( which );
			}
		break;

		case 2: /* Port C read */
                {
                        int input = 0;

                        /* read data */
                        if (intf->portC_r)
                                input = (*intf->portC_r)(which);

                        /* return data - combination of input and latched output depending on
                        the input/output status of each half of port C */
                        return ((chip->latch[2] & ~chip->io[2]) | (input & chip->io[2]));
                }

		case 3: /* Control word */
                        return 0x0ff;

                        //return chip->control;
		break;
	}

	//logerror("8255 chip %d: Port %c is being read but has no handler", which, 'A' + offset );

	return 0x00;
}

#define PPI8255_PORT_A_WRITE() \
{ \
        int write_data; \
  \
        write_data = (chip->latch[0] & ~chip->io[0]) | \
                        (0x0ff & chip->io[0]); \
  \
        if (intf->portA_w) \
             (*intf->portA_w)(which, write_data); \
}

#define PPI8255_PORT_B_WRITE() \
{ \
        int write_data; \
  \
        write_data = (chip->latch[1] & ~chip->io[1]) | \
                        (0x0ff & chip->io[1]); \
  \
        if (intf->portB_w) \
             (*intf->portB_w)(which, write_data); \
}

#define PPI8255_PORT_C_WRITE() \
{ \
        int write_data; \
  \
        write_data = (chip->latch[2] & ~chip->io[2]) | \
                        (0x0ff & chip->io[2]); \
  \
        if (intf->portC_w) \
             (*intf->portC_w)(which, write_data); \
}

void ppi8255_w( int which, int offset, int data ) {
	ppi8255		*chip;

	/* Some bounds checking */
	if ( which > intf->num ) {
		//logerror("Attempting to write an unmapped 8255 chip\n" );
		return;
	}

	if ( offset > 3 ) {
		//logerror("Attempting to write an invalid 8255 register\n" );
		return;
	}

	chip = &chips[which];

        /* store written data */
        chip->latch[offset] = data;

	switch( offset )
        {
                case 0: /* Port A write */
                PPI8255_PORT_A_WRITE();
                return;

		case 1: /* Port B write */
                PPI8255_PORT_B_WRITE();
                return;

		case 2: /* Port C write */
                PPI8255_PORT_C_WRITE();
                return;

		case 3: /* Control word */

			if ( data & 0x80 )
                        {
                        /* mode set */
                        chip->control = data;

                        chip->groupA_mode = ( data >> 5 ) & 3;
                        chip->groupB_mode = ( data >> 2 ) & 1;

                        //if ( chip->groupA_mode != 0 || chip->groupB_mode != 0 ) {
                        //        logerror("8255 chip %d: Setting an unsupported mode!\n", which );
                        //}

                        /* Port A direction */
			if ( data & 0x10 )
                        {
                                /* input */
				chip->io[0] = 0xff;
                        }
			else
                        {
                                /* output */
                                chip->io[0] = 0x00;
                        }

			/* Port B direction */
			if ( data & 0x02 )
				chip->io[1] = 0xff;
			else
				chip->io[1] = 0x00;

			/* Port C upper direction */
			if ( data & 0x08 )
				chip->io[2] |= 0xf0;
			else
				chip->io[2] &= 0x0f;

			/* Port C lower direction */
			if ( data & 0x01 )
				chip->io[2] |= 0x0f;
			else
				chip->io[2] &= 0xf0;

                        /* KT: 25-Dec-99 - 8255 resets latches when mode set */
                        chip->latch[0] = chip->latch[1] = chip->latch[2] = 0;

                        PPI8255_PORT_A_WRITE();
                        PPI8255_PORT_B_WRITE();
                        PPI8255_PORT_C_WRITE();

                        }
                        else
                        {
                                /* KT: 25-Dec-99 - Added port C bit set/reset feature */
                                /* bit set/reset */
                                int bit;

                                bit = (data>>1) & 0x07;

                                if (data & 1)
                                {
                                        /* set bit */
                                        chip->latch[2] |= (1<<bit);
                                }
                                else
                                {
                                        /* bit reset */
                                        chip->latch[2] &= ~(1<<bit);
                                }

                                PPI8255_PORT_C_WRITE();
			}
                        return;
		break;
	}

	//logerror("8255 chip %d: Port %c is being written to but has no handler", which, 'A' + offset );
}

/* Helpers */
READ_HANDLER( ppi8255_0_r ) { return ppi8255_r( 0, offset ); }
READ_HANDLER( ppi8255_1_r ) { return ppi8255_r( 1, offset ); }
READ_HANDLER( ppi8255_2_r ) { return ppi8255_r( 2, offset ); }
READ_HANDLER( ppi8255_3_r ) { return ppi8255_r( 3, offset ); }
WRITE_HANDLER( ppi8255_0_w ) { ppi8255_w( 0, offset, data ); }
WRITE_HANDLER( ppi8255_1_w ) { ppi8255_w( 1, offset, data ); }
WRITE_HANDLER( ppi8255_2_w ) { ppi8255_w( 2, offset, data ); }
WRITE_HANDLER( ppi8255_3_w ) { ppi8255_w( 3, offset, data ); }

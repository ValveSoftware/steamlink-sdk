#ifndef TMS34061_H
#define TMS34061_H

/****************************************************************************
 *																			*
 *	Function prototypes and constants used by the TMS34061 emulator			*
 *																			*
 *  Created by Zsolt Vasvari on 5/26/1998.	                                *
 *																			*
 ****************************************************************************/

/* Callback prototypes */

/* Return the function code (FS0-FS2) selected by this offset */
typedef int  (*TMS34061_getfunction_t)  (int offset);

/* Return the row address (RA0-RA8) selected by this offset */
typedef int  (*TMS34061_getrowaddress_t)(int offset);

/* Return the column address (CA0-CA8) selected by this offset */
typedef int  (*TMS34061_getcoladdress_t)(int offset);

/* Function called to get a pixel */
typedef int  (*TMS34061_getpixel_t)(int col, int row);

/* Function called to set a pixel */
typedef void (*TMS34061_setpixel_t)(int col, int row, int pixel);



struct TMS34061interface
{
	//int reg_addr_mode;               /* One of the addressing mode constants above */
    TMS34061_getfunction_t   getfunction;
	TMS34061_getrowaddress_t getrowaddress;
    TMS34061_getcoladdress_t getcoladdress;
    TMS34061_getpixel_t      getpixel;
    TMS34061_setpixel_t      setpixel;
    int cpu;                         /* Which CPU is the TMS34061 causing interrupts on */
    int (*vertical_interrupt)(void); /* Function called on a vertical interrupt */
};


/* Initializes the emulator */
int TMS34061_start(struct TMS34061interface *interface);

/* Cleans up the emulation */
void TMS34061_stop(void);

/* Writes to the 34061 */
WRITE_HANDLER( TMS34061_w );

/* Reads from the 34061 */
READ_HANDLER( TMS34061_r );

/* Checks whether the display is inhibited */
int TMS34061_display_blanked(void);

#endif

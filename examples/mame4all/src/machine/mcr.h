/***************************************************************************

	mcr.c

	Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
	I/O ports)

	Tapper machine started by Chris Kirmse

***************************************************************************/

#include "machine/6821pia.h"


extern INT16 spyhunt_scrollx, spyhunt_scrolly;
extern timer_tm mcr68_timing_factor;



/************ Generic MCR routines ***************/

extern Z80_DaisyChain mcr_daisy_chain[];
extern UINT8 mcr_cocktail_flip;

void mcr_init_machine(void);
void mcr68_init_machine(void);
void zwackery_init_machine(void);

int mcr_interrupt(void);
int mcr68_interrupt(void);

WRITE_HANDLER( mcr_control_port_w );
WRITE_HANDLER( mcr_scroll_value_w );

WRITE_HANDLER( mcr68_6840_upper_w );
WRITE_HANDLER( mcr68_6840_lower_w );
READ_HANDLER( mcr68_6840_upper_r );
READ_HANDLER( mcr68_6840_lower_r );



/************ Generic character and sprite definition ***************/

extern struct GfxLayout mcr_bg_layout;
extern struct GfxLayout mcr_sprite_layout;

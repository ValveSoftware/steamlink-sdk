/*****************************************************************
machine\swmathbx.h

This file is Copyright 1997, Steve Baines.

Release 2.0 (5 August 1997)

See drivers\starwars.c for notes

******************************************************************/


void init_starwars(void);

void run_mbox(void);
void init_swmathbox (void);

/* Read handlers */
READ_HANDLER( reh_r );
READ_HANDLER( rel_r );
READ_HANDLER( prng_r );

/* Write handlers */
WRITE_HANDLER( prngclr_w );
WRITE_HANDLER( mw0_w );
WRITE_HANDLER( mw1_w );
WRITE_HANDLER( mw2_w );
WRITE_HANDLER( swmathbx_w );


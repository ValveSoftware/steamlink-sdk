#ifndef _8255PPI_H_
#define _8255PPI_H_

#define MAX_8255 4

typedef struct
{
	int num;							 /* number of PPIs to emulate */
	int (*portA_r)( int chip );
	int (*portB_r)( int chip );
	int (*portC_r)( int chip );
	void (*portA_w)( int chip, int data );
	void (*portB_w)( int chip, int data );
	void (*portC_w)( int chip, int data );
} ppi8255_interface;

/* Init */
void ppi8255_init( ppi8255_interface *intfce);

/* Read/Write */
int ppi8255_r ( int which, int offset );
void ppi8255_w( int which, int offset, int data );

/* Helpers */
READ_HANDLER( ppi8255_0_r );
READ_HANDLER( ppi8255_1_r );
READ_HANDLER( ppi8255_2_r );
READ_HANDLER( ppi8255_3_r );
WRITE_HANDLER( ppi8255_0_w );
WRITE_HANDLER( ppi8255_1_w );
WRITE_HANDLER( ppi8255_2_w );
WRITE_HANDLER( ppi8255_3_w );
#endif

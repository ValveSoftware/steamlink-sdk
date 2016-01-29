/*********************************************************

	Konami 054539 PCM Sound Chip

*********************************************************/
#ifndef __K054539_H__
#define __K054539_H__

#define MAX_054539 2

struct K054539interface {
	int num;									/* number of chips */
	int clock;									/* clock (usually 48000) */
	int region[MAX_054539];						/* memory regions of sample ROM(s) */
	int mixing_level[MAX_054539][2];			/* Mixing levels */
	void (*apan[MAX_054539])(double, double);	/* Callback for analog output mixing levels (0..1 for each channel) */
	void (*irq[MAX_054539])( void );
};


int K054539_sh_start( const struct MachineSound *msound );
void K054539_sh_stop( void );
WRITE_HANDLER( K054539_0_w );
READ_HANDLER( K054539_0_r );
WRITE_HANDLER( K054539_1_w );
READ_HANDLER( K054539_1_r );

#endif /* __K054539_H__ */

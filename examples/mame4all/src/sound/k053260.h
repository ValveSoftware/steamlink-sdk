/*********************************************************

	Konami 053260 PCM/ADPCM Sound Chip

*********************************************************/
#ifndef __K053260_H__
#define __K053260_H__

struct K053260_interface {
	int clock;					/* clock */
	int region;					/* memory region of sample ROM(s) */
	int mixing_level[2];		/* volume */
	void (*irq)( int param );	/* called on SH1 complete cycle ( clock / 32 ) */
};


int K053260_sh_start( const struct MachineSound *msound );
void K053260_sh_stop( void );
WRITE_HANDLER( K053260_w );
READ_HANDLER( K053260_r );

#endif /* __K053260_H__ */

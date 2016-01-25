#ifndef UPD7759S_H
#define UPD7759S_H

#define MAX_UPD7759 2

/* There are two modes for the uPD7759, selected through the !MD pin.
   This is the mode select input.  High is stand alone, low is slave.
   We're making the assumption that nobody switches modes through
   software. */

#define UPD7759_STANDALONE_MODE     1
#define UPD7759_SLAVE_MODE			0

#define UPD7759_STANDARD_CLOCK 640000

struct UPD7759_interface
{
	int num;		/* num of upd chips */
	int clock_rate;
	int volume[MAX_UPD7759];
	int region[MAX_UPD7759]; 	/* memory region from which the samples came */
	int mode;		/* standalone or slave mode */
	void (*irqcallback[MAX_UPD7759])(int param);	/* for slave mode only */
};

int UPD7759_sh_start (const struct MachineSound *msound);
void UPD7759_sh_stop (void);

void UPD7759_reset_w (int num, int data);
void UPD7759_message_w (int num, int which);
void UPD7759_start_w (int num, int playback);
int UPD7759_busy_r (int num);
int UPD7759_data_r (int num, int offs);

WRITE_HANDLER( UPD7759_0_message_w );
WRITE_HANDLER( UPD7759_0_start_w );
READ_HANDLER( UPD7759_0_busy_r );
READ_HANDLER( UPD7759_0_data_r );
READ_HANDLER( UPD7759_1_data_r );

#endif


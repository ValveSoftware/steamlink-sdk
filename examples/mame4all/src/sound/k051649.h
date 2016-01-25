#ifndef k051649_h
#define k051649_h

struct k051649_interface
{
	int master_clock;	/* master clock */
	int volume;			/* playback volume */
};

int K051649_sh_start(const struct MachineSound *msound);
void K051649_sh_stop(void);

WRITE_HANDLER( K051649_waveform_w );
WRITE_HANDLER( K051649_volume_w );
WRITE_HANDLER( K051649_frequency_w );
WRITE_HANDLER( K051649_keyonoff_w );

#endif

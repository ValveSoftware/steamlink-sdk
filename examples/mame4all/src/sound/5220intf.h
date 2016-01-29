#ifndef intf5220_h
#define intf5220_h

struct TMS5220interface
{
	int baseclock;				/* clock rate = 80 * output sample rate,     */
								/* usually 640000 for 8000 Hz sample rate or */
								/* usually 800000 for 10000 Hz sample rate.  */
	int mixing_level;
	void (*irq)(int state);		/* IRQ callback function */
};

int tms5220_sh_start(const struct MachineSound *msound);
void tms5220_sh_stop(void);
void tms5220_sh_update(void);

WRITE_HANDLER( tms5220_data_w );
READ_HANDLER( tms5220_status_r );
int tms5220_ready_r(void);
int tms5220_int_r(void);

void tms5220_reset(void);
void tms5220_set_frequency(int frequency);

#endif


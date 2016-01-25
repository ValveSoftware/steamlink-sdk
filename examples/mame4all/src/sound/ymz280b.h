/**********************************************************************************************
 *
 *   Yamaha YMZ280B driver
 *   by Aaron Giles
 *
 **********************************************************************************************/

#ifndef YMZ280B_H
#define YMZ280B_H

#define MAX_YMZ280B 			2

struct YMZ280Binterface
{
	int num;                  						/* total number of chips */
	int baseclock[MAX_YMZ280B];						/* input clock */
	int region[MAX_YMZ280B];						/* memory region where the sample ROM lives */
	int mixing_level[MAX_YMZ280B];					/* master volume */
	void (*irq_callback[MAX_YMZ280B])(int state);	/* irq callback */
};

int YMZ280B_sh_start(const struct MachineSound *msound);
void YMZ280B_sh_stop(void);
void YMZ280B_set_sample_base(int which, void *base);

READ_HANDLER( YMZ280B_status_0_r );
READ_HANDLER( YMZ280B_status_1_r );
WRITE_HANDLER( YMZ280B_register_0_w );
WRITE_HANDLER( YMZ280B_register_1_w );
WRITE_HANDLER( YMZ280B_data_0_w );
WRITE_HANDLER( YMZ280B_data_1_w );

#endif

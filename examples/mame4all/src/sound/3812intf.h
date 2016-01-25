#ifndef YM3812INTF_H
#define YM3812INTF_H


#define MAX_3812 2
#define MAX_8950 MAX_3812

struct YM3812interface
{
	int num;
	int baseclock;
	int mixing_level[MAX_3812];
	void (*handler[MAX_3812])(int linestate);
};

struct Y8950interface
{
	int num;
	int baseclock;
	int mixing_level[MAX_8950];
	void (*handler[MAX_8950])(int linestate);
	/* Y8950 */
	int rom_region[MAX_8950]; /* delta-T ADPCM ROM region */
	mem_read_handler keyboardread[MAX_8950];
	mem_write_handler keyboardwrite[MAX_8950];
	mem_read_handler portread[MAX_8950];
	mem_write_handler portwrite[MAX_8950];
};

#define YM3526interface YM3812interface

/* YM3812 */
READ_HANDLER( YM3812_status_port_0_r );
WRITE_HANDLER( YM3812_control_port_0_w );
WRITE_HANDLER( YM3812_write_port_0_w );
READ_HANDLER( YM3812_status_port_1_r );
WRITE_HANDLER( YM3812_control_port_1_w );
WRITE_HANDLER( YM3812_write_port_1_w );

int YM3812_sh_start(const struct MachineSound *msound);
void YM3812_sh_stop(void);
void YM3812_sh_reset(void);

/* YM3526 */
#define YM3526_status_port_0_r YM3812_status_port_0_r
#define YM3526_control_port_0_w YM3812_control_port_0_w
#define YM3526_write_port_0_w YM3812_write_port_0_w
#define YM3526_status_port_1_r YM3812_status_port_1_r
#define YM3526_control_port_1_w YM3812_control_port_1_w
#define YM3526_write_port_1_w YM3812_write_port_1_w
int YM3526_sh_start(const struct MachineSound *msound);
#define YM3526_sh_stop YM3812_sh_stop
#define YM3526_shupdate YM3812_sh_update

/* Y8950 */
#define Y8950_status_port_0_r YM3812_status_port_0_r
#define Y8950_control_port_0_w YM3812_control_port_0_w
READ_HANDLER( Y8950_read_port_0_r );
#define Y8950_write_port_0_w YM3812_write_port_0_w
#define Y8950_status_port_1_r YM3812_status_port_1_r
#define Y8950_control_port_1_w YM3812_control_port_1_w
READ_HANDLER( Y8950_read_port_1_r );
#define Y8950_write_port_1_w YM3812_write_port_1_w
int Y8950_sh_start(const struct MachineSound *msound);
#define Y8950_sh_stop YM3812_sh_stop
#define Y8950_shupdate YM3812_sh_update

#endif

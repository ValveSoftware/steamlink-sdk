#ifndef YM2413INTF_H
#define YM2413INTF_H

#define MAX_2413 MAX_3812

#define YM2413interface YM3812interface

READ_HANDLER( YM2413_status_port_0_r );
WRITE_HANDLER( YM2413_register_port_0_w );
WRITE_HANDLER( YM2413_data_port_0_w );

int  YM2413_sh_start(const struct MachineSound *msound);
void YM2413_sh_stop(void);

#endif


/*
 *	Header file for the PD4990A Serial I/O calendar & clock.
 */
void addretrace (void);
int read_4990_testbit(void);
int read_4990_databit(void);
WRITE_HANDLER( write_4990_control_w );
void increment_day(void);
void increment_month(void);

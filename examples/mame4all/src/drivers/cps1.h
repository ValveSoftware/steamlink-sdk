#ifndef _CPS1_H_
#define _CPS1_H_

extern unsigned char * cps1_gfxram;     /* Video RAM */
extern unsigned char * cps1_output;     /* Output ports */
extern size_t cps1_gfxram_size;
extern size_t cps1_output_size;

READ_HANDLER( cps1_eeprom_port_r );
WRITE_HANDLER( cps1_eeprom_port_w );

READ_HANDLER( cps1_output_r );
WRITE_HANDLER( cps1_output_w );

int  cps1_vh_start(void);
void cps1_vh_stop(void);
void cps1_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void cps1_eof_callback(void);

#endif

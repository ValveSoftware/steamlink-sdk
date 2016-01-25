#ifndef m_atari_vg_h
#define m_atari_vg_h

/***************************************************************************

  atari_vg.h

  Generic functions used by the Atari Vector games

***************************************************************************/

READ_HANDLER( atari_vg_earom_r );
WRITE_HANDLER( atari_vg_earom_w );
WRITE_HANDLER( atari_vg_earom_ctrl_w );
void atari_vg_earom_handler(void *file,int read_or_write);

#endif

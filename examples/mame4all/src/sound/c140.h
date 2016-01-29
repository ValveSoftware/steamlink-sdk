/* C140.h */

#ifndef _NAMCO_C140_
#define _NAMCO_C140_

int C140_sh_start( const struct MachineSound *msound );
void C140_sh_stop( void );
void C140_sh_update( void );
READ_HANDLER( C140_r );
WRITE_HANDLER( C140_w );

struct C140interface {
    int frequency;
    int region;
    int mixing_level;
};

#endif

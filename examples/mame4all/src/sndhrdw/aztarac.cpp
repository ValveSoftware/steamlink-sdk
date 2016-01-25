/*
 * Aztarac soundboard interface emulation
 *
 * Jul 25 1999 by Mathis Rosenhauer
 *
 */

#include "driver.h"
#include "cpu/z80/z80.h"

static int sound_command, sound_status;

READ_HANDLER( aztarac_sound_r )
{
    if (Machine->sample_rate)
        return sound_status & 0x01;
    else
        return 1;
}

WRITE_HANDLER( aztarac_sound_w )
{
    sound_command = data;
    sound_status ^= 0x21;
    if (sound_status & 0x20)
        cpu_cause_interrupt( 1, Z80_IRQ_INT );
}

READ_HANDLER( aztarac_snd_command_r )
{
    sound_status |= 0x01;
    sound_status &= ~0x20;
    return sound_command;
}

READ_HANDLER( aztarac_snd_status_r )
{
    return sound_status & ~0x01;
}

WRITE_HANDLER( aztarac_snd_status_w )
{
    sound_status &= ~0x10;
}

int aztarac_snd_timed_irq (void)
{
    sound_status ^= 0x10;

    if (sound_status & 0x10)
        return Z80_IRQ_INT;
    else
        return Z80_IGNORE_INT;
}

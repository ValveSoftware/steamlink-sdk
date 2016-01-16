#include "wheels.h"
#include <stdio.h>

int get_nativemode_cmd_DFP(cmdstruct *c)
{
    c->cmds[0][0] = 0xf8;
    c->cmds[0][1] = 0x01;
    c->cmds[0][2] = 0x00;
    c->cmds[0][3] = 0x00;
    c->cmds[0][4] = 0x00;
    c->cmds[0][5] = 0x00;
    c->cmds[0][6] = 0x00;
    c->cmds[0][7] = 0x00;
    c->numCmds = 1;
    return 0;
}

int get_nativemode_cmd_DFGT(cmdstruct *c)
{
    c->cmds[0][0] = 0xf8;
    c->cmds[0][1] = 0x0a;
    c->cmds[0][2] = 0x00;
    c->cmds[0][3] = 0x00;
    c->cmds[0][4] = 0x00;
    c->cmds[0][5] = 0x00;
    c->cmds[0][6] = 0x00;
    c->cmds[0][7] = 0x00;

    c->cmds[1][0] = 0xf8;
    c->cmds[1][1] = 0x09;
    c->cmds[1][2] = 0x03;
    c->cmds[1][3] = 0x01;
    c->cmds[1][4] = 0x00;
    c->cmds[1][5] = 0x00;
    c->cmds[1][6] = 0x00;
    c->cmds[1][7] = 0x00;

    c->numCmds = 2;
    return 0;
}

int get_nativemode_cmd_G25(cmdstruct *c)
{
    c->cmds[0][0] = 0xf8;
    c->cmds[0][1] = 0x10;
    c->cmds[0][2] = 0x00;
    c->cmds[0][3] = 0x00;
    c->cmds[0][4] = 0x00;
    c->cmds[0][5] = 0x00;
    c->cmds[0][6] = 0x00;
    c->cmds[0][7] = 0x00;
    c->numCmds = 1;
    return 0;
}

int get_nativemode_cmd_G27(cmdstruct *c)
{
    c->cmds[0][0] = 0xf8;
    c->cmds[0][1] = 0x0a;
    c->cmds[0][2] = 0x00;
    c->cmds[0][3] = 0x00;
    c->cmds[0][4] = 0x00;
    c->cmds[0][5] = 0x00;
    c->cmds[0][6] = 0x00;
    c->cmds[0][7] = 0x00;

    c->cmds[1][0] = 0xf8;
    c->cmds[1][1] = 0x09;
    c->cmds[1][2] = 0x04;
    c->cmds[1][3] = 0x01;
    c->cmds[1][4] = 0x00;
    c->cmds[1][5] = 0x00;
    c->cmds[1][6] = 0x00;
    c->cmds[1][7] = 0x00;

    c->numCmds = 2;
    return 0;
}

/* used by DFGT, G25, G27 */
int get_range_cmd(cmdstruct *c, int range)
{
    c->cmds[0][0] = 0xf8;
    c->cmds[0][1] = 0x81;
    c->cmds[0][2] = range & 0x00ff;
    c->cmds[0][3] = (range & 0xff00)>>8;
    c->cmds[0][4] = 0x00;
    c->cmds[0][5] = 0x00;
    c->cmds[0][6] = 0x00;
    c->cmds[0][7] = 0x00;
    c->numCmds = 1;
    return 0;
}

/* used by DFP
 *
 * Credits go to MadCatX, slim.one and lbondar for finding out correct formula
 * See http://www.lfsforum.net/showthread.php?p=1593389#post1593389
 *     http://www.lfsforum.net/showthread.php?p=1595604#post1595604
 *     http://www.lfsforum.net/showthread.php?p=1603971#post1603971
 */
int get_range_cmd2(cmdstruct *c, int range)
{
    /* Prepare command A */
    c->cmds[0][0] = 0xf8;
    c->cmds[0][1] = 0x00;   /* Set later */
    c->cmds[0][2] = 0x00;
    c->cmds[0][3] = 0x00;
    c->cmds[0][4] = 0x00;
    c->cmds[0][5] = 0x00;
    c->cmds[0][6] = 0x00;
    c->cmds[0][7] = 0x00;

    /* Prepare command B */
    c->cmds[1][0] = 0x81;
    c->cmds[1][1] = 0x0b;
    c->cmds[1][2] = 0x00;
    c->cmds[1][3] = 0x00;
    c->cmds[1][4] = 0x00;
    c->cmds[1][5] = 0x00;
    c->cmds[1][6] = 0x00;
    c->cmds[1][7] = 0x00;

    c->numCmds = 2;

    int rampLeft, rampRight, fullRange;

    if (range > 200) {
        c->cmds[0][1] = 0x03;

        if (range == 900)      /* Do not limit the range */
            return 0;
        else
            fullRange = 900;
    } else {
        c->cmds[0][1] = 0x02;

        if (range == 200)        /* Do not limit the range */
            return 0;
        else
            fullRange = 200;
    }

    /* Construct range limiting command */
    rampLeft = (((fullRange - range + 1) * 2047) / fullRange);
    rampRight = 0xfff - rampLeft;

    c->cmds[1][2] = rampLeft >> 4;
    c->cmds[1][3] = rampRight >> 4;
    c->cmds[1][4] = 0xff;
    c->cmds[1][5] = (rampRight & 0xe) << 4 | (rampLeft & 0xe);
    c->cmds[1][6] = 0xff;

    return 0;
}

/* used by all wheels */
int get_autocenter_cmd(cmdstruct *c, int centerforce, int rampspeed)
{
    c->cmds[0][0] = 0xfe;
    c->cmds[0][1] = 0x0d;
    c->cmds[0][2] = rampspeed & 0x0f;
    c->cmds[0][3] = rampspeed & 0x0f;
    c->cmds[0][4] = centerforce & 0xff;
    c->cmds[0][5] = 0x00;
    c->cmds[0][6] = 0x00;
    c->cmds[0][7] = 0x00;

    c->numCmds = 1;
    return 0;
}


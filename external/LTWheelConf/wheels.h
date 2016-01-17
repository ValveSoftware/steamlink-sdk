/*
 *    ltwheelconf - configure logitech racing wheels
 *
 *    Copyright (C) 2011  Michael Bauer <michael@m-bauer.org>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef wheels_h
#define wheels_h

#define VID_LOGITECH 0x046d

typedef struct {
    // reserve space for max. 4 command strings, 8 chars each
    unsigned char cmds[4][8];
    // how many command strings are actually used
    unsigned int numCmds;
} cmdstruct;

typedef struct {
    char shortname[255];
    char name[255];
    unsigned int restricted_pid;
    unsigned int native_pid;
    unsigned int min_rotation;
    unsigned int max_rotation;
    unsigned int revision;               /* aka bcdDevice - Device Release Number according to HID specs */
    int (*get_nativemode_cmd)(cmdstruct *c);
    int (*get_range_cmd)(cmdstruct *c, int range);
    int (*get_autocenter_cmd)(cmdstruct *c, int centerforce, int rampspeed);
}wheelstruct;


int get_nativemode_cmd_DFP(cmdstruct *c);
int get_nativemode_cmd_DFGT(cmdstruct *c);
int get_nativemode_cmd_G25(cmdstruct *c);
int get_nativemode_cmd_G27(cmdstruct *c);
int get_range_cmd(cmdstruct *c, int range);
int get_range_cmd2(cmdstruct *c, int range);
int get_autocenter_cmd(cmdstruct *c, int centerforce, int rampspeed);


static const wheelstruct wheels[] = {
    {
        "DF",
        "Driving Force",
        0xc294,
        0xc294,
        40,
        240,
        0,
        0,
        0,
        0
    },
    {
        "MR",
        "Momo Racing",
        0xcA03,
        0xcA03,
        40,
        240,
        0x0019,
        0,
        0,
        &get_autocenter_cmd
    },
    {
        "MF",
        "Momo Force",
        0xc294,
        0xc295,
        40,
        240,
        0,
        0,
        0,
        0
    },
    {
        "DFP",
        "Driving Force Pro",
        0xc294,
        0xc298,
        0,
        900,
        0x1106,
        &get_nativemode_cmd_DFP,
        &get_range_cmd2,
        &get_autocenter_cmd
    },
    {
        "G25",
        "G25",
        0xc294,
        0xc299,
        40,
        900,
        0x1222,
        &get_nativemode_cmd_G25,
        &get_range_cmd,
        &get_autocenter_cmd
    },
    {
        "DFGT",
        "Driving Force GT",
        0xc294,
        0xc29A,
        40,
        900,
        0,
        &get_nativemode_cmd_DFGT,
        &get_range_cmd,
        &get_autocenter_cmd
    },
    {
        "G27",
        "G27",
        0xc294,
        0xc29B,
        40,
        900,
        0,
        &get_nativemode_cmd_G27,
        &get_range_cmd,
        &get_autocenter_cmd
    }
};

#endif

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


#define VERSION "0.2.7"

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wheels.h"
#include "wheelfunctions.h"

/* Globals */
int verbose_flag = 0;


void help() {
    printf ( "%s","\nltwheelconf " VERSION " - Configure Logitech Driving Force Pro, G25, G27 wheels\n\
    \n\
    ltwheelconf Copyright (C) 2011 Michael Bauer\n\
    This program comes with ABSOLUTELY NO WARRANTY.\n\
    \n\
    General Options: \n\
    -h, --help                  This help text\n\
    -v, --verbose               Verbose output\n\
                                Use -vv to get debug messages from libusb\n\
    -l, --list                  List all found/supported devices\n\
    \n\
    Wheel configuration: \n\
    -w, --wheel=shortname       Which wheel is connected. Supported values:\n\
        -> 'DF'   (Driving Force)\n\
        -> 'MR'   (Momo Racing)\n\
        -> 'MF'   (Momo Force)\n\
        -> 'DFP'  (Driving Force Pro)\n\
        -> 'DFGT' (Driving Force GT)\n\
        -> 'G25'  (G25)\n\
        -> 'G27'  (G27)\n\
    -n, --nativemode            Set wheel to native mode (separate axes, full wheel range, clutch pedal, H-shifter)\n\
    -x, --reset                 Reset the wheel. This is basically the same like unplugging & replugging it.\n\
                                Note:\n\
                                    -> Requires wheel to be in native (-n) mode!\n\
    -r, --range=degrees         Set wheel rotation range (up to 900 degrees).\n\
                                Note:\n\
                                    -> Requires wheel to be in native (-n) mode!\n\
    -a, --autocenter=value      Set autocenter bypassing hid driver. Value should be between 0 and 255 (0 -> no autocenter, 255 -> max autocenter force). \n\
                                Together with the rampspeed setting this allows much finer control of the autocenter behaviour as using the generic input interface.\n\
                                Note: \n\
                                    -> Requires parameter '--rampspeed'\n\
    -s, --rampspeed             Use in conjuntion with the --autocenter parameter. Specify how fast the autocenter force should increase when rotating wheel.\n\
                                Value should be between 0 and 7\n\
                                Low value means the centering force is increasing only slightly when turning the wheel.\n\
                                High value means the centering force is increasing very fast when turning the wheel.\n\
    -b, --altautocenter=value   Set autocenter force using generic input interface. Value should be between 0 and 100 (0 -> no autocenter, 100 -> max autocenter force). \n\
                                Use this if --autocenter does not work for your device.\n\
                                Note: \n\
                                    -> Requires parameter '--device' to specify the input device\n\
                                    -> Only works with kernel >= 2.6.39\n\
                                    -> The rampspeed can not be influenced using this method\n\
    -g, --gain=value            Set forcefeedback gain. Value should be between 0 and 100 (0 -> no gain, 100 -> max gain). \n\
                                Note: \n\
                                    -> Requires parameter '--device' to specify the input device\n\
    -d, --device=inputdevice    Specify inputdevice for force-feedback related configuration (--gain and --altautocenter)\n\
    \n\
    Note: You can freely combine all configuration options.\n\
    \n\
    Examples:\n\
    Put wheel into native mode:\n\
    $ sudo ltwheelconf --wheel G25 --nativemode\n\
    Set wheel rotation range to 900 degree:\n\
    $ sudo ltwheelconf --wheel G27 --range 900 \n\
    Set moderate autocenter:\n\
    $ sudo ltwheelconf --wheel DFP --autocenter 100 --rampspeed 1\n\
    Disable autocenter completely:\n\
    $ sudo ltwheelconf --wheel G25 --autocenter 0 --rampspeed 0\n\
    Set native mode, disable autocenter and set wheel rotation range of 540 degrees in one call:\n\
    $ sudo ltwheelconf --wheel G25 --nativemode --range 540 --autocenter 0 --rampspeed 0\n\
    \n\
    Contact: michael@m-bauer.org\n\
    \n");
}

int main (int argc, char **argv)
{
    unsigned short int range = 0;
    unsigned short int centerforce = 0;
    unsigned short int gain = 0;
    int do_validate_wheel = 0;
    int do_native = 0;
    int do_range = 0;
    int do_autocenter = 0;
    int do_alt_autocenter = 0;
    int do_gain = 0;
    int do_list = 0;
    int rampspeed = -1;
    int do_help = 0;
    int do_reset = 0;
    char device_file_name[128];
    char shortname[255];
    memset(device_file_name, 0, sizeof(device_file_name));
    verbose_flag = 0;

    static struct option long_options[] =
    {
        {"verbose",         no_argument,       0,               'v'},
        {"help",            no_argument,       0,               'h'},
        {"list",            no_argument,       0,               'l'},
        {"wheel",           required_argument, 0,               'w'},
        {"nativemode",      no_argument,       0,               'n'},
        {"range",           required_argument, 0,               'r'},
        {"altautocenter",   required_argument, 0,               'b'},
        {"autocenter",      required_argument, 0,               'a'},
        {"rampspeed",       required_argument, 0,               's'},
        {"gain",            required_argument, 0,               'g'},
        {"device",          required_argument, 0,               'd'},
        {"reset",           no_argument,       0,               'x'},
        {0,                 0,                 0,               0  }
    };

    while (optind < argc) {
        int index = -1;
        int result = getopt_long (argc, argv, "vhlw:nr:a:g:d:s:b:x",
                                  long_options, &index);

        if (result == -1)
            break; /* end of list */

            switch (result) {
                case 'v':
                    verbose_flag++;
                    break;
                case 'n':
                    do_native = 1;
                    break;
                case 'r':
                    range = atoi(optarg);
                    do_range = 1;
                    break;
                case 'a':
                    centerforce = atoi(optarg);
                    do_autocenter = 1;
                    do_alt_autocenter = 0;
                    break;
                case 'b':
                    centerforce = atoi(optarg);
                    do_autocenter = 0;
                    do_alt_autocenter = 1;
                    break;
                case 's':
                    rampspeed = atoi(optarg);
                    break;
                case 'g':
                    gain = atoi(optarg);
                    do_gain = 1;
                    break;
                case 'd':
                    strncpy(device_file_name, optarg, 128);
                    break;
                case 'l':
                    do_list = 1;
                    break;
                case 'w':
                    strncpy(shortname, optarg, 255);
                    do_validate_wheel = 1;
                    break;
                case 'x':
                    do_reset = 1;
                    break;
                case '?':
                default:
                    do_help = 1;
                    break;
            }

    }

    if (argc > 1)
    {
        libusb_init(NULL);
        if (verbose_flag > 1)
            libusb_set_debug(0, 3);

        int wait_for_udev = 0;
        wheelstruct* wheel = 0;

        if (do_help) {
            help();
        } else if (do_list) {
            // list all devices, ignore other options...
            list_devices();
        } else {
            if (do_validate_wheel) {
                int numWheels = sizeof(wheels)/sizeof(wheelstruct);
                int i = 0;
                for (i=0; i < numWheels; i++) {
                    if (strncasecmp(wheels[i].shortname, shortname, 255) == 0) {
                        // found matching wheel
                        wheel = &(wheels[i]);
                        break;
                    }
                }
                if (!wheel) {
                    printf("Wheel \"%s\" not supported. Did you spell the shortname correctly?\n", shortname);
                }
            }

            if (do_reset) {
                if (!wheel) {
                    printf("Please provide --wheel parameter!\n");
                } else {
                    reset_wheel(wheel);
                    wait_for_udev = 1;
                }
            }

            if (do_native) {
                if (!wheel) {
                    printf("Please provide --wheel parameter!\n");
                } else {
                    set_native_mode(wheel);
                    wait_for_udev = 1;
                }
            }

            if (do_range) {
                if (!wheel) {
                    printf("Please provide --wheel parameter!\n");
                } else {
                    set_range(wheel, clamprange(wheel, range));
                    wait_for_udev = 1;
                }
            }

            if (do_autocenter) {
                if (!wheel) {
                    printf("Please provide --wheel parameter!\n");
                } else {
                    if (centerforce == 0) {
                        set_autocenter(wheel, centerforce, 0);
                        wait_for_udev = 1;
                    } else if (rampspeed == -1) {
                        printf("Please provide '--rampspeed' parameter\n");
                    } else {
                        set_autocenter(wheel, centerforce, rampspeed);
                        wait_for_udev = 1;
                    }
                }
            }

            if (do_alt_autocenter) {
                if (strlen(device_file_name)) {
                    alt_set_autocenter(centerforce, device_file_name, wait_for_udev);
                    wait_for_udev = 0;
                } else {
                    printf("Please provide the according event interface for your wheel using '--device' parameter (E.g. '--device /dev/input/event0')\n");
                }
            }

            if (do_gain) {
                if (strlen(device_file_name)) {
                    set_gain(gain, device_file_name, wait_for_udev);
                    wait_for_udev = 0;
                } else {
                    printf("Please provide the according event interface for your wheel using '--device' parameter (E.g. '--device /dev/input/event0')\n");
                }
            }
        }
        libusb_exit(NULL);
    } else {
        // display usage information if no arguments given
        help();
    }
    exit(0);
}

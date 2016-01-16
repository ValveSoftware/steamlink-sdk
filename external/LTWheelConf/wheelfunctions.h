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

#ifndef wheelfunctions_h
#define wheelfunctions_h

#include <libusb-1.0/libusb.h>


/*
 * Native method to set autcenter behaviour of LT wheels.
 *
 * Based on a post by "anrp" on the vdrift forum:
 * http://vdrift.net/Forum/viewtopic.php?t=412&postdays=0&postorder=asc&start=60
 *
 *  fe0b0101ff - centering spring, slow spring ramp
 *  ____^^____ - left ramp speed
 *  ______^^__ - right ramp speed
 *  ________^^ - overall strength
 *
 * Rampspeed seems to be limited to 0-7 only.
 */
int set_autocenter(wheelstruct* w, int centerforce, int rampspeed);

/*
 * Set maximum rotation range of wheel in degrees
 * G25/G27/DFP support up to 900 degrees.
 */
int set_range(wheelstruct* w, unsigned short int range);

/*
 * Clamp range value to be in allowed range for specified wheel
 */
unsigned short int clamprange(wheelstruct* w, unsigned short int range);

/*
 * Search and list all known/supported wheels
 */
void list_devices();

/*
 * Send custom command to USB device using interrupt transfer
 */
int send_command(libusb_device_handle *handle, cmdstruct command );

/*
 * Logitech wheels are in a kind of restricted mode when initially connected via usb.
 * In this restricted mode
 *  - axes for throttle and brake are always combined
 *  - rotation range is limited to 300 degrees
 *  - clutch pedal of G25/G27 does not work
 *  - H-gate shifter of G25/G27 does not work
 *
 * In restricted mode the wheels register on USB with pid 0xc294.
 * In native mode they register on USB with pid 0xc298 (DFP) or 0xc299 (G25/G27)
 *
 * This function takes care to switch the wheel to "native" mode with no restrictions.
 *
 */
int set_native_mode(wheelstruct* w);

/*
 * Generic method to set autocenter force of any wheel device recognized by kernel
 * This method does not allow to set the rampspeed and force individually.
 *
 * Looking at hid-lgff.c i think the kernel driver actually might do the wrong thing by modifying
 * the rampspeed instead of the general force...?
 */
int alt_set_autocenter(int centerforce, char *device_file_name, int wait_for_udev);

/*
 * Generic method to set forces gain for all effects
 */
int set_gain(int gain, char *device_file_name, int wait_for_udev);

/*
 * Reset the wheel, similar like unplug-replug cycle
 */
int reset_wheel(wheelstruct* w);

#endif
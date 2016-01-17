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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/input.h>

#include "wheels.h"
#include "wheelfunctions.h"

#define TRANSFER_WAIT_TIMEOUT_MS 5000
#define CONFIGURE_WAIT_SEC 3
#define UDEV_WAIT_SEC 2

/* Globals */
extern int verbose_flag;

void print_cmd(char *result, unsigned char cmd[8]) {
    sprintf(result, "%02X %02X %02X %02X %02X %02X %02X %02X", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7]);
}


void list_devices() {
    libusb_device_handle *handle = 0;
    libusb_device *dev = 0;
    struct libusb_device_descriptor desc;
    memset(&desc, 0, sizeof(desc));
    unsigned char descString[255];
    memset(&descString, 0, sizeof(descString));
    int numWheels = sizeof(wheels)/sizeof(wheelstruct);

    int numFound = 0;
    int i = 0;
    wheelstruct w = wheels[i];
    for (i = 0; i < numWheels; i++) {
        w = wheels[i];
        printf("Scanning for \"%s\": ", w.name);
        handle = libusb_open_device_with_vid_pid(NULL, VID_LOGITECH, w.native_pid);
        if (handle != 0) {
            dev = libusb_get_device(handle);
            if (dev != 0) {
                int ret = libusb_get_device_descriptor(dev, &desc);
                if (ret == 0) {
                    numFound++;
                    libusb_get_string_descriptor_ascii(handle, desc.iProduct, descString, 255);
                    printf("\t\tFound \"%s\", release number %x, %04x:%04x (bus %d, device %d)",
                           descString, desc.bcdDevice, desc.idVendor, desc.idProduct,
                           libusb_get_bus_number(dev), libusb_get_device_address(dev));
                } else {
                    perror("Get device descriptor");
                }
            } else {
                perror ("Get device");
            }
            libusb_close(handle);
        }
        printf("\n");
    }
    printf("Found %d devices.\n", numFound);
}

int send_command(libusb_device_handle *handle, cmdstruct command ) {
    if (command.numCmds == 0) {
        printf( "send_command: Empty command provided! Not sending anything...\n");
        return 0;
    }

    int stat;
    stat = libusb_detach_kernel_driver(handle, 0);
    if ((stat < 0 ) || verbose_flag) perror("Detach kernel driver");

    stat = libusb_claim_interface( handle, 0 );
    if ( (stat < 0) || verbose_flag) perror("Claiming USB interface");

    int transferred = 0;

    // send all command strings provided in command
    int cmdCount;
    for (cmdCount=0; cmdCount < command.numCmds; cmdCount++) {
        if (verbose_flag) {
            char raw_string[255];
            print_cmd(raw_string, command.cmds[cmdCount]);
            printf("\tSending string:   \"%s\"\n", raw_string);
        }
        stat = libusb_interrupt_transfer( handle, 1, command.cmds[cmdCount], sizeof( command.cmds[cmdCount] ), &transferred, TRANSFER_WAIT_TIMEOUT_MS );
        if ( (stat < 0) || verbose_flag) perror("Sending USB command");
    }

    /* In case the command just sent caused the device to switch from restricted mode to native mode
     * the following two commands will fail due to invalid device handle (because the device changed
     * its pid on the USB bus).
     * So it is not possible anymore to release the interface and re-attach kernel driver.
     * I am not sure if this produces a memory leak within libusb, but i do not think there is another
     * solution possible...
     */
    stat = libusb_release_interface(handle, 0 );
    if (stat != LIBUSB_ERROR_NO_DEVICE) { // silently ignore "No such device" error due to reasons explained above.
        if ( (stat < 0) || verbose_flag) {
            perror("Releasing USB interface.");
        }
    }

    stat = libusb_attach_kernel_driver( handle, 0);
    if (stat != LIBUSB_ERROR_NO_DEVICE) { // silently ignore "No such device" error due to reasons explained above.
        if ( (stat < 0) || verbose_flag) {
            perror("Reattaching kernel driver");
        }
    }
    return 0;
}

int set_native_mode(wheelstruct* w)
{
    // first check if wheel has restriced/native mode at all
    if (w->native_pid == w->restricted_pid) {
        printf( "%s is always in native mode.\n", w->name);
        return 0;
    }

    // check if wheel is already in native mode
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(NULL, VID_LOGITECH, w->native_pid);
    if ( handle != NULL ) {
        printf( "Found a %s already in native mode.\n", w->name);
        return 0;
    }

    // try to get handle to device in restricted mode
    handle = libusb_open_device_with_vid_pid(NULL, VID_LOGITECH, w->restricted_pid );
    if ( handle == NULL ) {
        printf( "Can not find %s in restricted mode (PID %x). This should not happen :-(\n", w->name, w->restricted_pid);
        return -1;
    }

    // check if we know how to set native mode
    if (!w->get_nativemode_cmd) {
        printf( "Sorry, do not know how to set %s into native mode.\n", w->name);
        return -1;
    }

    cmdstruct c;
    memset(&c, 0, sizeof(c));
    w->get_nativemode_cmd(&c);
    send_command(handle, c);

    // wait until wheel reconfigures to new PID...
    sleep(CONFIGURE_WAIT_SEC);

    // If above command was successfully we should now find the wheel in extended mode
    handle = libusb_open_device_with_vid_pid(NULL, VID_LOGITECH, w->native_pid);
    if ( handle != NULL ) {
        printf ( "%s is now set to native mode.\n", w->name);
    } else {
        // this should not happen, just in case
        printf ( "Unable to set %s to native mode.\n", w->name );
        return -1;
    }

    return 0;
}


short unsigned int clamprange(wheelstruct* w, short unsigned int range)
{
    if (range < w->min_rotation) {
        printf("Minimum range for %s is %d degrees.\n", w->name, w->min_rotation);
        range = w->min_rotation;
    }
    if (range > w->max_rotation) {
        range = w->max_rotation;
        printf("Maximum range for %s is %d degrees.\n", w->name, w->max_rotation);
    }
    return range;
}


int set_range(wheelstruct* w, short unsigned int range)
{
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(NULL, VID_LOGITECH, w->native_pid );
    if ( handle == NULL ) {
        printf ( "%s not found. Make sure it is set to native mode (use --native).\n", w->name);
        return -1;
    }

    if (!w->get_range_cmd) {
        printf( "Sorry, do not know how to set rotation range for %s.\n", w->name);
        return -1;
    }

    cmdstruct c;
    memset(&c, 0, sizeof(c));
    w->get_range_cmd(&c, range);
    send_command(handle, c);

    printf ("Wheel rotation range of %s is now set to %d degrees.\n", w->name, range);
    return 0;

}


int set_autocenter(wheelstruct* w, int centerforce, int rampspeed)
{
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(NULL, VID_LOGITECH, w->native_pid );
    if ( handle == NULL ) {
        printf ( "%s not found. Make sure it is set to native mode (use --native).\n", w->name);
        return -1;
    }

    if (!w->get_autocenter_cmd) {
        printf( "Sorry, do not know how to set autocenter force for %s. Please try generic implementation using --alt_autocenter.\n", w->name);
        return -1;
    }

    cmdstruct c;
    memset(&c, 0, sizeof(c));
    w->get_autocenter_cmd(&c, centerforce, rampspeed);
    send_command(handle, c);

    printf ("Autocenter for %s is now set to %d with rampspeed %d.\n", w->name, centerforce, rampspeed);
    return 0;
}

int alt_set_autocenter(int centerforce, char *device_file_name, int wait_for_udev) {
    if (verbose_flag) printf ( "Device %s: Setting autocenter force to %d.\n", device_file_name, centerforce );

    /* sleep UDEV_WAIT_SEC seconds to allow udev to set up device nodes due to kernel
     * driver re-attaching while setting native mode or wheel range before
     */
    if (wait_for_udev) sleep(UDEV_WAIT_SEC);

    /* Open device */
    int fd = open(device_file_name, O_RDWR);
    if (fd == -1) {
        perror("Open device file");
        return -1;
    }

    if (centerforce >= 0 && centerforce <= 100) {
        struct input_event ie;
        ie.type = EV_FF;
        ie.code = FF_AUTOCENTER;
        ie.value = 0xFFFFUL * centerforce/100;
        if (write(fd, &ie, sizeof(ie)) == -1) {
            perror("set auto-center");
            return -1;
        }
    }
    printf ("Wheel autocenter force is now set to %d.\n", centerforce);
    return 0;
}


int set_gain(int gain, char *device_file_name, int wait_for_udev) {
    if (verbose_flag) printf ( "Device %s: Setting FF gain to %d.\n", device_file_name, gain);

    /* sleep UDEV_WAIT_SEC seconds to allow udev to set up device nodes due to kernel
     * driver re-attaching while setting native mode or wheel range before
     */
    if (wait_for_udev) sleep(UDEV_WAIT_SEC);

    /* Open device */
    int fd = open(device_file_name, O_RDWR);
    if (fd == -1) {
        perror("Open device file");
        return -1;
    }

    if (gain >= 0 && gain <= 100) {
        struct input_event ie;
        ie.type = EV_FF;
        ie.code = FF_GAIN;
        ie.value = 0xFFFFUL * gain / 100;
        if (write(fd, &ie, sizeof(ie)) == -1) {
            perror("set gain");
            return -1;
        }
    }
    printf ("Wheel forcefeedback gain is now set to %d.\n", gain);
    return 0;
}

int reset_wheel(wheelstruct* w)
{
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(NULL, VID_LOGITECH, w->native_pid );
    if ( handle == NULL ) {
        printf ( "%s not found. Make sure it is set to native mode (use --native).\n", w->name);
        return -1;
    }
    return libusb_reset_device(handle);
}


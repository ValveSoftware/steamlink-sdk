/*
 *
 *  hh2serial GPS
 *
 *  Copyright (C) 2007-2012  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/device.h>
#include <connman/log.h>

static struct connman_device *hh2serial_device;

static int set_reg(char *key, char *value)
{
	FILE *reg_file;

	reg_file = fopen(key, "w");
	if (!reg_file)
		return -errno;

	fprintf(reg_file, "%s", value);
	fclose(reg_file);
	return 0;
}

static int hh2serial_probe(struct connman_device *device)
{
	return 0;
}

static void hh2serial_remove(struct connman_device *device)
{
}

static int hh2serial_enable(struct connman_device *device)
{
	DBG("");

	set_reg("/sys/class/gpio/expo_rt", "85");
	set_reg("/sys/class/gpio/gpio85/direction", "out");
	set_reg("/sys/class/gpio/gpio85/value", "1");

	return 0;
}

static int hh2serial_disable(struct connman_device *device)
{
	DBG("");

	set_reg("/sys/class/gpio/expo_rt", "85");
	set_reg("/sys/class/gpio/gpio85/direction", "out");
	set_reg("/sys/class/gpio/gpio85/value", "0");

	return 0;
}

static struct connman_device_driver hh2serial_device_driver = {
	.name           = "hh2serial GPS",
	.type           = CONNMAN_DEVICE_TYPE_GPS,
	.enable         = hh2serial_enable,
	.disable        = hh2serial_disable,
	.probe		= hh2serial_probe,
	.remove		= hh2serial_remove,
};

static int hh2serial_init(void)
{
	int err;

	err = connman_device_driver_register(&hh2serial_device_driver);
	if (err < 0)
		return err;

	hh2serial_device = connman_device_create("hh2serial_gps",
						CONNMAN_DEVICE_TYPE_GPS);
	if (!hh2serial_device) {
		connman_device_driver_unregister(&hh2serial_device_driver);
		return -ENODEV;
	}

	err = connman_device_register(hh2serial_device);
	if (err < 0) {
		connman_device_unref(hh2serial_device);
		return err;
	}

	return 0;
}

static void hh2serial_exit(void)
{
	if (hh2serial_device) {
		connman_device_unregister(hh2serial_device);
		connman_device_unref(hh2serial_device);
	}

	connman_device_driver_unregister(&hh2serial_device_driver);
}

CONNMAN_PLUGIN_DEFINE(hh2serial_gps, "hh2serial GPS", VERSION,
		CONNMAN_PLUGIN_PRIORITY_LOW, hh2serial_init, hh2serial_exit)

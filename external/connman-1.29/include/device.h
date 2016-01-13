/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2014  Intel Corporation. All rights reserved.
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

#ifndef __CONNMAN_DEVICE_H
#define __CONNMAN_DEVICE_H

#include <connman/network.h>
#include <connman/service.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:device
 * @title: Device premitives
 * @short_description: Functions for handling devices
 */

enum connman_device_type {
	CONNMAN_DEVICE_TYPE_UNKNOWN   = 0,
	CONNMAN_DEVICE_TYPE_ETHERNET  = 1,
	CONNMAN_DEVICE_TYPE_WIFI      = 2,
	CONNMAN_DEVICE_TYPE_BLUETOOTH = 3,
	CONNMAN_DEVICE_TYPE_CELLULAR  = 4,
	CONNMAN_DEVICE_TYPE_GPS       = 5,
	CONNMAN_DEVICE_TYPE_GADGET    = 6,
	CONNMAN_DEVICE_TYPE_VENDOR    = 10000,
};

#define CONNMAN_DEVICE_PRIORITY_LOW      -100
#define CONNMAN_DEVICE_PRIORITY_DEFAULT     0
#define CONNMAN_DEVICE_PRIORITY_HIGH      100

struct connman_device;

struct connman_device *connman_device_create(const char *node,
						enum connman_device_type type);

#define connman_device_ref(device) \
	connman_device_ref_debug(device, __FILE__, __LINE__, __func__)

#define connman_device_unref(device) \
	connman_device_unref_debug(device, __FILE__, __LINE__, __func__)

struct connman_device *
connman_device_ref_debug(struct connman_device *device,
			const char *file, int line, const char *caller);
void connman_device_unref_debug(struct connman_device *device,
			const char *file, int line, const char *caller);

enum connman_device_type connman_device_get_type(struct connman_device *device);
void connman_device_set_index(struct connman_device *device, int index);
int connman_device_get_index(struct connman_device *device);
void connman_device_set_interface(struct connman_device *device,
						const char *interface);

void connman_device_set_ident(struct connman_device *device,
						const char *ident);
const char *connman_device_get_ident(struct connman_device *device);

int connman_device_set_powered(struct connman_device *device,
						bool powered);
bool connman_device_get_powered(struct connman_device *device);
int connman_device_set_scanning(struct connman_device *device,
				enum connman_service_type type, bool scanning);
bool connman_device_get_scanning(struct connman_device *device);
void connman_device_reset_scanning(struct connman_device *device);

int connman_device_set_disconnected(struct connman_device *device,
						bool disconnected);
bool connman_device_get_disconnected(struct connman_device *device);

int connman_device_set_string(struct connman_device *device,
					const char *key, const char *value);
const char *connman_device_get_string(struct connman_device *device,
							const char *key);

int connman_device_add_network(struct connman_device *device,
					struct connman_network *network);
struct connman_network *connman_device_get_network(struct connman_device *device,
							const char *identifier);
int connman_device_remove_network(struct connman_device *device,
					struct connman_network *network);

int connman_device_register(struct connman_device *device);
void connman_device_unregister(struct connman_device *device);

void *connman_device_get_data(struct connman_device *device);
void connman_device_set_data(struct connman_device *device, void *data);

int connman_device_set_regdom(struct connman_device *device,
						const char *alpha2);
void connman_device_regdom_notify(struct connman_device *device,
					int result, const char *alpha2);
struct connman_device *connman_device_create_from_index(int index);
struct connman_device *connman_device_find_by_index(int index);
int connman_device_reconnect_service(struct connman_device *device);

struct connman_device_driver {
	const char *name;
	enum connman_device_type type;
	int priority;
	int (*probe) (struct connman_device *device);
	void (*remove) (struct connman_device *device);
	int (*enable) (struct connman_device *device);
	int (*disable) (struct connman_device *device);
	int (*scan)(enum connman_service_type type,
			struct connman_device *device,
			const char *ssid, unsigned int ssid_len,
			const char *identity, const char* passphrase,
			const char *security, void *user_data);
	int (*set_regdom) (struct connman_device *device,
						const char *alpha2);
};

int connman_device_driver_register(struct connman_device_driver *driver);
void connman_device_driver_unregister(struct connman_device_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_DEVICE_H */

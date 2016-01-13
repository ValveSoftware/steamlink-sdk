/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2013  Intel Corporation. All rights reserved.
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

#ifndef __CONNMAN_PROVISION_H
#define __CONNMAN_PROVISION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:provision
 * @title: Provisioned configuration premitives
 * @short_description: Functions for provision configuration handling
 */

struct connman_config_entry {
	char *ident;
	char *name;
	void *ssid;
	unsigned int ssid_len;
	bool hidden;
};

int connman_config_provision_mutable_service(GKeyFile *keyfile);
struct connman_config_entry **connman_config_get_entries(const char *type);
void connman_config_free_entries(struct connman_config_entry **entries);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_PROVISION_H */

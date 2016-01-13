/*
 *
 *  Connection Manager
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

#ifndef __CONNMAN_UTSNAME_H
#define __CONNMAN_UTSNAME_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:utsname
 * @title: utsname premitives
 * @short_description: Functions for handling utsname
 */

const char *connman_utsname_get_hostname(void);

struct connman_utsname_driver {
	const char *name;
	int priority;
	const char * (*get_hostname) (void);
	int (*set_hostname) (const char *hostname);
	int (*set_domainname) (const char *domainname);
};

int connman_utsname_driver_register(struct connman_utsname_driver *driver);
void connman_utsname_driver_unregister(struct connman_utsname_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_UTSNAME_H */

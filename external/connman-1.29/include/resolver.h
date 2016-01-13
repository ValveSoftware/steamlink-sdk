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

#ifndef __CONNMAN_RESOLVER_H
#define __CONNMAN_RESOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:resolver
 * @title: Resolver premitives
 * @short_description: Functions for registering resolver modules
 */

int connman_resolver_append(int index, const char *domain,
							const char *server);
int connman_resolver_append_lifetime(int index, const char *domain,
				     const char *server, unsigned int lifetime);
int connman_resolver_remove(int index, const char *domain,
							const char *server);
int connman_resolver_remove_all(int index);

void connman_resolver_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_RESOLVER_H */

/*
 *
 *  Resolver library with GLib integration
 *
 *  Copyright (C) 2009-2012  Intel Corporation. All rights reserved.
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

#ifndef __G_RESOLV_H
#define __G_RESOLV_H

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _GResolv;

typedef struct _GResolv GResolv;

typedef enum {
	G_RESOLV_RESULT_STATUS_SUCCESS,
	G_RESOLV_RESULT_STATUS_ERROR,
	G_RESOLV_RESULT_STATUS_NO_RESPONSE,
	G_RESOLV_RESULT_STATUS_FORMAT_ERROR,
	G_RESOLV_RESULT_STATUS_SERVER_FAILURE,
	G_RESOLV_RESULT_STATUS_NAME_ERROR,
	G_RESOLV_RESULT_STATUS_NOT_IMPLEMENTED,
	G_RESOLV_RESULT_STATUS_REFUSED,
} GResolvResultStatus;

typedef void (*GResolvResultFunc)(GResolvResultStatus status,
					char **results, gpointer user_data);

typedef void (*GResolvDebugFunc)(const char *str, gpointer user_data);

GResolv *g_resolv_new(int index);

GResolv *g_resolv_ref(GResolv *resolv);
void g_resolv_unref(GResolv *resolv);

void g_resolv_set_debug(GResolv *resolv,
				GResolvDebugFunc func, gpointer user_data);

bool g_resolv_add_nameserver(GResolv *resolv, const char *address,
					uint16_t port, unsigned long flags);
void g_resolv_flush_nameservers(GResolv *resolv);

guint g_resolv_lookup_hostname(GResolv *resolv, const char *hostname,
				GResolvResultFunc func, gpointer user_data);

bool g_resolv_cancel_lookup(GResolv *resolv, guint id);

bool g_resolv_set_address_family(GResolv *resolv, int family);

#ifdef __cplusplus
}
#endif

#endif /* __G_RESOLV_H */

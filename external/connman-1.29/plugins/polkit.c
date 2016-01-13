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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdbus.h>

#define CONNMAN_API_SUBJECT_TO_CHANGE
#include <connman/plugin.h>
#include <connman/dbus.h>

static const GDBusSecurityTable polkit_security[] = {
	{ CONNMAN_PRIVILEGE_MODIFY, "net.connman.modify",
				G_DBUS_SECURITY_FLAG_BUILTIN |
				G_DBUS_SECURITY_FLAG_ALLOW_INTERACTION },
	{ CONNMAN_PRIVILEGE_SECRET, "net.connman.secret",
				G_DBUS_SECURITY_FLAG_BUILTIN |
				G_DBUS_SECURITY_FLAG_ALLOW_INTERACTION },
	{ }
};

static int polkit_init(void)
{
	g_dbus_register_security(polkit_security);

	return 0;
}

static void polkit_exit(void)
{
	g_dbus_unregister_security(polkit_security);
}

CONNMAN_PLUGIN_DEFINE(polkit, "PolicyKit authorization plugin", VERSION,
		CONNMAN_PLUGIN_PRIORITY_DEFAULT, polkit_init, polkit_exit)

/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2013  Intel Corporation. All rights reserved.
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

#include <errno.h>
#include <string.h>

#include <gdbus.h>

#include "connman.h"

DBusMessage *__connman_error_failed(DBusMessage *msg, int errnum)
{
	const char *str = strerror(errnum);

	switch (errnum) {
	case ESRCH:
		return __connman_error_not_registered(msg);
	case ENXIO:
		return __connman_error_not_found(msg);
	case EACCES:
		return __connman_error_permission_denied(msg);
	case EEXIST:
		return __connman_error_already_exists(msg);
	case EINVAL:
		return __connman_error_invalid_arguments(msg);
	case ENOSYS:
		return __connman_error_not_implemented(msg);
	case ENOLINK:
		return __connman_error_no_carrier(msg);
	case ENOTUNIQ:
		return __connman_error_not_unique(msg);
	case EOPNOTSUPP:
		return __connman_error_not_supported(msg);
	case ECONNABORTED:
		return __connman_error_operation_aborted(msg);
	case EISCONN:
		return __connman_error_already_connected(msg);
	case ENOTCONN:
	case ECONNREFUSED:
		return __connman_error_not_connected(msg);
	case ETIMEDOUT:
		return __connman_error_operation_timeout(msg);
	case EALREADY:
	case EINPROGRESS:
		return __connman_error_in_progress(msg);
	case ENOKEY:
		return __connman_error_passphrase_required(msg);
	}

	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
					".Failed", "%s", str);
}

DBusMessage *__connman_error_invalid_arguments(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".InvalidArguments", "Invalid arguments");
}

DBusMessage *__connman_error_permission_denied(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".PermissionDenied", "Permission denied");
}

DBusMessage *__connman_error_passphrase_required(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".PassphraseRequired", "Passphrase required");
}

DBusMessage *__connman_error_not_registered(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
					".NotRegistered", "Not registered");
}

DBusMessage *__connman_error_not_unique(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
					".NotUnique", "Not unique");
}

DBusMessage *__connman_error_not_supported(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
					".NotSupported", "Not supported");
}

DBusMessage *__connman_error_not_implemented(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
					".NotImplemented", "Not implemented");
}

DBusMessage *__connman_error_not_found(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
						".NotFound", "Not found");
}

DBusMessage *__connman_error_no_carrier(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
						".NoCarrier", "No carrier");
}

DBusMessage *__connman_error_in_progress(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
						".InProgress", "In progress");
}

DBusMessage *__connman_error_already_exists(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".AlreadyExists", "Already exists");
}

DBusMessage *__connman_error_already_enabled(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".AlreadyEnabled", "Already enabled");
}

DBusMessage *__connman_error_already_disabled(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".AlreadyDisabled", "Already disabled");
}

DBusMessage *__connman_error_already_connected(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".AlreadyConnected", "Already connected");
}

DBusMessage *__connman_error_not_connected(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
					".NotConnected", "Not connected");
}
DBusMessage *__connman_error_operation_aborted(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".OperationAborted", "Operation aborted");
}

DBusMessage *__connman_error_operation_timeout(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".OperationTimeout", "Operation timeout");
}

DBusMessage *__connman_error_invalid_service(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".InvalidService", "Invalid service");
}

DBusMessage *__connman_error_invalid_property(DBusMessage *msg)
{
	return g_dbus_create_error(msg, CONNMAN_ERROR_INTERFACE
				".InvalidProperty", "Invalid property");
}

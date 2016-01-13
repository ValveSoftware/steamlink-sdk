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

#include <gdbus.h>

#include "connman.h"

static DBusConnection *connection = NULL;

static GSList *notifier_list = NULL;
static GHashTable *service_hash = NULL;

static const char *notifier_state;

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_notifier *notifier1 = a;
	const struct connman_notifier *notifier2 = b;

	return notifier2->priority - notifier1->priority;
}

/**
 * connman_notifier_register:
 * @notifier: notifier module
 *
 * Register a new notifier module
 *
 * Returns: %0 on success
 */
int connman_notifier_register(struct connman_notifier *notifier)
{
	DBG("notifier %p name %s", notifier, notifier->name);

	notifier_list = g_slist_insert_sorted(notifier_list, notifier,
							compare_priority);

	return 0;
}

/**
 * connman_notifier_unregister:
 * @notifier: notifier module
 *
 * Remove a previously registered notifier module
 */
void connman_notifier_unregister(struct connman_notifier *notifier)
{
	DBG("notifier %p name %s", notifier, notifier->name);

	notifier_list = g_slist_remove(notifier_list, notifier);
}

static int connected[MAX_CONNMAN_SERVICE_TYPES];
static int online[MAX_CONNMAN_SERVICE_TYPES];

static bool notifier_is_online(void)
{
	unsigned int i;

	__sync_synchronize();
	for (i = 0; i < MAX_CONNMAN_SERVICE_TYPES; i++) {
		if (online[i] > 0)
			return true;
	}

	return false;
}

bool __connman_notifier_is_connected(void)
{
	unsigned int i;

	__sync_synchronize();
	for (i = 0; i < MAX_CONNMAN_SERVICE_TYPES; i++) {
		if (connected[i] > 0)
			return true;
	}

	return false;
}

static const char *evaluate_notifier_state(void)
{
	if (notifier_is_online())
		return "online";

	if (__connman_notifier_is_connected())
		return "ready";

	if (__connman_technology_get_offlinemode())
		return "offline";

	return "idle";
}

const char *__connman_notifier_get_state(void)
{
	return notifier_state;
}

static void state_changed(void)
{
	const char *state;

	state = evaluate_notifier_state();

	if (g_strcmp0(state, notifier_state) == 0)
		return;

	notifier_state = state;

	connman_dbus_property_changed_basic(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "State",
					DBUS_TYPE_STRING, &notifier_state);
}

void __connman_notifier_connect(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	}

	if (__sync_fetch_and_add(&connected[type], 1) == 0) {
		__connman_technology_set_connected(type, true);
		state_changed();
	}
}

void __connman_notifier_enter_online(enum connman_service_type type)
{
	DBG("type %d", type);

	if (__sync_fetch_and_add(&online[type], 1) == 0)
		state_changed();
}

void __connman_notifier_leave_online(enum connman_service_type type)
{
	DBG("type %d", type);

	if (__sync_fetch_and_sub(&online[type], 1) == 1)
		state_changed();
}

void __connman_notifier_disconnect(enum connman_service_type type)
{
	DBG("type %d", type);

	__sync_synchronize();
	if (connected[type] == 0) {
		connman_error("notifier disconnect underflow");
		return;
	}

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_GPS:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_GADGET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
	case CONNMAN_SERVICE_TYPE_P2P:
		break;
	}

	if (__sync_fetch_and_sub(&connected[type], 1) != 1)
		return;

	__connman_technology_set_connected(type, false);
	state_changed();
}

void __connman_notifier_default_changed(struct connman_service *service)
{
	GSList *list;

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->default_changed)
			notifier->default_changed(service);
	}
}

void __connman_notifier_service_add(struct connman_service *service,
					const char *name)
{
	GSList *list;

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->service_add)
			notifier->service_add(service, name);
	}
}

void __connman_notifier_service_remove(struct connman_service *service)
{
	GSList *list;

	if (g_hash_table_lookup(service_hash, service)) {
		/*
		 * This is a tempory check for consistency. It can be
		 * removed when there are no reports for the following
		 * error message.
		 */
		connman_error("Service state machine inconsistency detected.");

		g_hash_table_remove(service_hash, service);
	}

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->service_remove)
			notifier->service_remove(service);
	}
}

void __connman_notifier_proxy_changed(struct connman_service *service)
{
	GSList *list;

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->proxy_changed)
			notifier->proxy_changed(service);
	}
}

static void offlinemode_changed(dbus_bool_t enabled)
{
	DBG("enabled %d", enabled);

	connman_dbus_property_changed_basic(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "OfflineMode",
						DBUS_TYPE_BOOLEAN, &enabled);
}

void __connman_notifier_offlinemode(bool enabled)
{
	GSList *list;

	DBG("enabled %d", enabled);

	offlinemode_changed(enabled);
	state_changed();

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->offline_mode)
			notifier->offline_mode(enabled);
	}
}

static void notify_idle_state(bool idle)
{
	GSList *list;

	DBG("idle %d", idle);

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->idle_state)
			notifier->idle_state(idle);
	}
}

void __connman_notifier_service_state_changed(struct connman_service *service,
					enum connman_service_state state)
{
	GSList *list;
	unsigned int old_size;
	bool found;

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->service_state_changed)
			notifier->service_state_changed(service, state);
	}

	old_size = g_hash_table_size(service_hash);
	found = g_hash_table_lookup(service_hash, service);

	switch (state) {
	case CONNMAN_SERVICE_STATE_UNKNOWN:
	case CONNMAN_SERVICE_STATE_FAILURE:
	case CONNMAN_SERVICE_STATE_DISCONNECT:
	case CONNMAN_SERVICE_STATE_IDLE:
		if (!found)
			break;

		g_hash_table_remove(service_hash, service);
		if (old_size == 1)
			notify_idle_state(true);

		break;
	case CONNMAN_SERVICE_STATE_ASSOCIATION:
	case CONNMAN_SERVICE_STATE_CONFIGURATION:
	case CONNMAN_SERVICE_STATE_READY:
	case CONNMAN_SERVICE_STATE_ONLINE:
		if (found)
			break;

		g_hash_table_insert(service_hash, service, service);
		if (old_size == 0)
			notify_idle_state(false);

		break;
	}
}

void __connman_notifier_ipconfig_changed(struct connman_service *service,
					struct connman_ipconfig *ipconfig)
{
	GSList *list;

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->ipconfig_changed)
			notifier->ipconfig_changed(service, ipconfig);
	}
}

int __connman_notifier_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();

	service_hash = g_hash_table_new_full(g_direct_hash, g_direct_equal,
						NULL, NULL);

	notifier_state = evaluate_notifier_state();

	return 0;
}

void __connman_notifier_cleanup(void)
{
	DBG("");

	g_hash_table_destroy(service_hash);
	service_hash = NULL;

	dbus_connection_unref(connection);
}

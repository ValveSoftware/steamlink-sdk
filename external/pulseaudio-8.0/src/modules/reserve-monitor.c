/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: t -*-*/

/***
  Copyright 2009 Lennart Poettering

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
***/

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "reserve-monitor.h"
#include "reserve.h"

struct rm_monitor {
	int ref;

	char *device_name;
	char *service_name;
	char *match;

	DBusConnection *connection;

	unsigned busy:1;
	unsigned filtering:1;
	unsigned matching:1;

	rm_change_cb_t change_cb;
	void *userdata;
};

#define SERVICE_PREFIX "org.freedesktop.ReserveDevice1."

#define SERVICE_FILTER				\
	"type='signal',"			\
	"sender='" DBUS_SERVICE_DBUS "',"	\
	"interface='" DBUS_INTERFACE_DBUS "',"	\
	"member='NameOwnerChanged',"		\
	"arg0='%s'"

static unsigned get_busy(
	DBusConnection *c,
	const char *name_owner) {

	const char *un;

	if (!name_owner || !*name_owner)
		return FALSE;

	/* If we ourselves own the device, then don't consider this 'busy' */
	if ((un = dbus_bus_get_unique_name(c)))
		if (strcmp(name_owner, un) == 0)
			return FALSE;

	return TRUE;
}

static DBusHandlerResult filter_handler(
	DBusConnection *c,
	DBusMessage *s,
	void *userdata) {

	rm_monitor *m;
	DBusError error;

	dbus_error_init(&error);

	m = userdata;
	assert(m->ref >= 1);

	if (dbus_message_is_signal(s, "org.freedesktop.DBus", "NameOwnerChanged")) {
		const char *name, *old, *new;

		if (!dbus_message_get_args(
			    s,
			    &error,
			    DBUS_TYPE_STRING, &name,
			    DBUS_TYPE_STRING, &old,
			    DBUS_TYPE_STRING, &new,
			    DBUS_TYPE_INVALID))
			goto invalid;

		if (strcmp(name, m->service_name) == 0) {
			unsigned old_busy = m->busy;

			m->busy = get_busy(c, new);

			if (m->busy != old_busy && m->change_cb) {
				m->ref++;
				m->change_cb(m);
				rm_release(m);
			}
		}
	}

invalid:
	dbus_error_free(&error);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int rm_watch(
	rm_monitor **_m,
	DBusConnection *connection,
	const char *device_name,
	rm_change_cb_t change_cb,
	DBusError *error)  {

	rm_monitor *m = NULL;
	char *name_owner;
	int r;
	DBusError _error;

	if (!error)
		error = &_error;

	dbus_error_init(error);

	if (!_m)
		return -EINVAL;

	if (!connection)
		return -EINVAL;

	if (!device_name)
		return -EINVAL;

	if (!(m = calloc(sizeof(rm_monitor), 1)))
		return -ENOMEM;

	m->ref = 1;

	if (!(m->device_name = strdup(device_name))) {
		r = -ENOMEM;
		goto fail;
	}

	m->connection = dbus_connection_ref(connection);
	m->change_cb = change_cb;

	if (!(m->service_name = malloc(sizeof(SERVICE_PREFIX) + strlen(device_name)))) {
		r = -ENOMEM;
		goto fail;
	}
	sprintf(m->service_name, SERVICE_PREFIX "%s", m->device_name);

	if (!(dbus_connection_add_filter(m->connection, filter_handler, m, NULL))) {
		r = -ENOMEM;
		goto fail;
	}

	m->filtering = 1;

	if (!(m->match = malloc(sizeof(SERVICE_FILTER) - 2 + strlen(m->service_name)))) {
		r = -ENOMEM;
		goto fail;
	}

	sprintf(m->match, SERVICE_FILTER, m->service_name);
	dbus_bus_add_match(m->connection, m->match, error);

	if (dbus_error_is_set(error)) {
		r = -EIO;
		goto fail;
	}

	m->matching = 1;

	if ((r = rd_dbus_get_name_owner(m->connection, m->service_name, &name_owner, error)) < 0)
		goto fail;

	m->busy = get_busy(m->connection, name_owner);
	free(name_owner);

	*_m = m;
	return 0;

fail:
	if (&_error == error)
		dbus_error_free(&_error);

	if (m)
		rm_release(m);

	return r;
}

void rm_release(rm_monitor *m) {
	if (!m)
		return;

	assert(m->ref > 0);

	if (--m->ref > 0)
		return;

	if (m->matching)
		dbus_bus_remove_match(
			m->connection,
			m->match,
			NULL);

	if (m->filtering)
		dbus_connection_remove_filter(
			m->connection,
			filter_handler,
			m);

	free(m->device_name);
	free(m->service_name);
	free(m->match);

	if (m->connection)
		dbus_connection_unref(m->connection);

	free(m);
}

int rm_busy(rm_monitor *m) {
	if (!m)
		return -EINVAL;

	assert(m->ref > 0);

	return m->busy;
}

void rm_set_userdata(rm_monitor *m, void *userdata) {

	if (!m)
		return;

	assert(m->ref > 0);
	m->userdata = userdata;
}

void* rm_get_userdata(rm_monitor *m) {

	if (!m)
		return NULL;

	assert(m->ref > 0);

	return m->userdata;
}

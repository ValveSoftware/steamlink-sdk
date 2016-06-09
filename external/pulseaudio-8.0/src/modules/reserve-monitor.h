/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: t -*-*/

#ifndef fooreservemonitorhfoo
#define fooreservemonitorhfoo

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

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rm_monitor rm_monitor;

/* Prototype for a function that is called whenever the reservation
 * device of a device changes. Use rm_monitor_busy() to find out the
 * new state.*/
typedef void (*rm_change_cb_t)(rm_monitor *m);

/* Creates a monitor for watching the lock status of a device. Returns
 * 0 on success, a negative errno style return value on error.  The
 * DBus error might be set as well if the error was caused D-Bus. */
int rm_watch(
	rm_monitor **m,              /* On success a pointer to the newly allocated rm_device object will be filled in here */
	DBusConnection *connection,  /* Session bus (when D-Bus learns about user busses we should switch to user busses) */
	const char *device_name,     /* The device to monitor, e.g. "Audio0" */
	rm_change_cb_t change_cb,    /* Will be called whenever the lock status changes. May be NULL */
	DBusError *error);           /* If we fail due to a D-Bus related issue the error will be filled in here. May be NULL. */

/* Free a rm_monitor object */
void rm_release(rm_monitor *m);

/* Checks whether the device is currently reserved, and returns 1
 * then, 0 if not, negative errno style error code value on error. */
int rm_busy(rm_monitor *m);

/* Attach a userdata pointer to an rm_monitor */
void rm_set_userdata(rm_monitor *m, void *userdata);

/* Query the userdata pointer from an rm_monitor. Returns NULL if no
 * userdata was set. */
void* rm_get_userdata(rm_monitor *m);

#ifdef __cplusplus
}
#endif

#endif

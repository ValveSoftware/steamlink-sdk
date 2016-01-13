/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012,2014  BMW Car IT GmbH.
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

#ifndef __CONNMAN_INOTIFY_H
#define __CONNMAN_INOTIFY_H

struct inotify_event;

typedef void (* inotify_event_cb) (struct inotify_event *event,
					const char *ident);

int connman_inotify_register(const char *path, inotify_event_cb callback);
void connman_inotify_unregister(const char *path, inotify_event_cb callback);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_INOTIFY_H */

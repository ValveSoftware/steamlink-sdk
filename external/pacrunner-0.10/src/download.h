/*
 *
 *  PACrunner - Proxy configuration daemon
 *
 *  Copyright (C) 2010-2011  Intel Corporation. All rights reserved.
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

typedef void (* pacrunner_download_cb) (char *content, void *user_data);

struct pacrunner_download_driver {
	const char *name;
	int (*download) (const char *interface, const char *url,
			pacrunner_download_cb callback, void *user_data);
};

int pacrunner_download_driver_register(struct pacrunner_download_driver *driver);
void pacrunner_download_driver_unregister(struct pacrunner_download_driver *driver);

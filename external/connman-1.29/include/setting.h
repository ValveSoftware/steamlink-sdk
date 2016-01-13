/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012  Intel Corporation. All rights reserved.
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

#ifndef __CONNMAN_SETTING_H
#define __CONNMAN_SETTING_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool connman_setting_get_bool(const char *key);
char **connman_setting_get_string_list(const char *key);
unsigned int *connman_setting_get_uint_list(const char *key);

unsigned int connman_timeout_input_request(void);
unsigned int connman_timeout_browser_launch(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_SETTING_H */

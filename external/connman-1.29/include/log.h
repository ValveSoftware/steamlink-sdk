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

#ifndef __CONNMAN_LOG_H
#define __CONNMAN_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:log
 * @title: Logging premitives
 * @short_description: Functions for logging error and debug information
 */

void connman_info(const char *format, ...)
				__attribute__((format(printf, 1, 2)));
void connman_warn(const char *format, ...)
				__attribute__((format(printf, 1, 2)));
void connman_error(const char *format, ...)
				__attribute__((format(printf, 1, 2)));
void connman_debug(const char *format, ...)
				__attribute__((format(printf, 1, 2)));

#define connman_warn_once(fmt, arg...) do {		\
	static bool printed;				\
	if (!printed) {					\
		connman_warn(fmt, ## arg);		\
		printed = true;				\
	}						\
} while (0)

struct connman_debug_desc {
	const char *name;
	const char *file;
#define CONNMAN_DEBUG_FLAG_DEFAULT (0)
#define CONNMAN_DEBUG_FLAG_PRINT   (1 << 0)
#define CONNMAN_DEBUG_FLAG_ALIAS   (1 << 1)
	unsigned int flags;
} __attribute__((aligned(8)));

#define CONNMAN_DEBUG_DEFINE(name) \
	static struct connman_debug_desc __debug_alias_ ## name \
	__attribute__((used, section("__debug"), aligned(8))) = { \
		#name, __FILE__, CONNMAN_DEBUG_FLAG_ALIAS \
	};

/**
 * DBG:
 * @fmt: format string
 * @arg...: list of arguments
 *
 * Simple macro around connman_debug() which also include the function
 * name it is called in.
 */
#define DBG(fmt, arg...) do { \
	static struct connman_debug_desc __connman_debug_desc \
	__attribute__((used, section("__debug"), aligned(8))) = { \
		.file = __FILE__, .flags = CONNMAN_DEBUG_FLAG_DEFAULT, \
	}; \
	if (__connman_debug_desc.flags & CONNMAN_DEBUG_FLAG_PRINT) \
		connman_debug("%s:%s() " fmt, \
					__FILE__, __FUNCTION__ , ## arg); \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* __CONNMAN_LOG_H */

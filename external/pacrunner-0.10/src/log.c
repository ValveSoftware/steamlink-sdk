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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <syslog.h>

#include "pacrunner.h"

void pacrunner_info(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_INFO, format, ap);

	va_end(ap);
}

void pacrunner_warn(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_WARNING, format, ap);

	va_end(ap);
}

void pacrunner_error(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_ERR, format, ap);

	va_end(ap);
}

void pacrunner_debug(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_DEBUG, format, ap);

	va_end(ap);
}

extern struct pacrunner_debug_desc __start___debug[];
extern struct pacrunner_debug_desc __stop___debug[];

static gchar **enabled = NULL;

static bool is_enabled(struct pacrunner_debug_desc *desc)
{
	int i;

	if (!enabled)
		return false;

	for (i = 0; enabled[i]; i++) {
		if (desc->name && g_pattern_match_simple(enabled[i],
							desc->name))
			return true;
		if (desc->file && g_pattern_match_simple(enabled[i],
							desc->file))
			return true;
	}

	return false;
}

int __pacrunner_log_init(const char *debug, gboolean detach)
{
	int option = LOG_NDELAY | LOG_PID;
	struct pacrunner_debug_desc *desc;
	const char *name = NULL, *file = NULL;

	if (debug)
		enabled = g_strsplit_set(debug, ":, ", 0);

	for (desc = __start___debug; desc < __stop___debug; desc++) {
		if (file || name) {
			if (g_strcmp0(desc->file, file) == 0) {
				if (!desc->name)
					desc->name = name;
			} else
				file = NULL;
		}

		if (is_enabled(desc))
			desc->flags |= PACRUNNER_DEBUG_FLAG_PRINT;
	}

	if (!detach)
		option |= LOG_PERROR;

	openlog("pacrunner", option, LOG_DAEMON);

	syslog(LOG_INFO, "PAC Runner version %s", VERSION);

	return 0;
}

void __pacrunner_log_cleanup(void)
{
	syslog(LOG_INFO, "Exit");

	closelog();

	g_strfreev(enabled);
}

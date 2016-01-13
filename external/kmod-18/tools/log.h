/*
 * kmod - log infrastructure
 *
 * Copyright (C) 2012-2013  ProFUSION embedded systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <syslog.h>

#include "kmod.h"

void log_open(bool use_syslog);
void log_close(void);
void log_printf(int prio, const char *fmt, ...) _printf_format_(2, 3);
#define CRIT(...) log_printf(LOG_CRIT, __VA_ARGS__)
#define ERR(...) log_printf(LOG_ERR, __VA_ARGS__)
#define WRN(...) log_printf(LOG_WARNING, __VA_ARGS__)
#define INF(...) log_printf(LOG_INFO, __VA_ARGS__)
#define DBG(...) log_printf(LOG_DEBUG, __VA_ARGS__)

struct kmod_ctx;
void log_setup_kmod_log(struct kmod_ctx *ctx, int priority);

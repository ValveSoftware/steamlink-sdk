/*
 * kmod - one tool to rule them all
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
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

#pragma once

#include "macro.h"

struct kmod_cmd {
	const char *name;
	int (*cmd)(int argc, char *argv[]);
	const char *help;
};

extern const struct kmod_cmd kmod_cmd_compat_lsmod;
extern const struct kmod_cmd kmod_cmd_compat_rmmod;
extern const struct kmod_cmd kmod_cmd_compat_insmod;
extern const struct kmod_cmd kmod_cmd_compat_modinfo;
extern const struct kmod_cmd kmod_cmd_compat_modprobe;
extern const struct kmod_cmd kmod_cmd_compat_depmod;

extern const struct kmod_cmd kmod_cmd_list;
extern const struct kmod_cmd kmod_cmd_static_nodes;

#include "log.h"

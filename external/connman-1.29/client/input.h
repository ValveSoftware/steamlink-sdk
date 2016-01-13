/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2012-2014  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef __CONNMANCTL_INPUT_H
#define __CONNMANCTL_INPUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void __connmanctl_quit(void);
bool __connmanctl_is_interactive(void);
void __connmanctl_save_rl(void);
void __connmanctl_redraw_rl(void);
int __connmanctl_input_calc_level(void);
void __connmanctl_input_lookup_end(void);
typedef void (* connmanctl_input_func_t) (char *input, void *user_data);
void __connmanctl_agent_mode(const char *prompt,
		connmanctl_input_func_t input_handler, void *user_data);
void __connmanctl_command_mode(void);
int __connmanctl_input_init(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __CONNMANCTL_INPUT_H */

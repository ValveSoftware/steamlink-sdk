/* Copyright (C) 2009. Free Software Foundation, Inc.
   Contributed by Xinliang David Li (davidxl@google.com) and
                  Raksit Ashok  (raksit@google.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_L_IPO_H
#define GCC_L_IPO_H

/* Used in profile-gen  */
extern unsigned ggc_total_memory; /* in KB */

/* Primary module's id (non-zero). If no module-info was read in, this will
   be zero.  */
extern unsigned primary_module_id;

/* The macro to test if the compilation is in light weight IPO mode.
   In this mode, the source module being compiled will be compiled
   together with 0 or more auxiliary modules.  */
#define L_IPO_COMP_MODE (primary_module_id != 0)

/* The macro to test if the current module being parsed is the
   primary source module.  */
#define L_IPO_IS_PRIMARY_MODULE (current_module_id == primary_module_id)

/* The macro to test if the current module being parsed is an
   auxiliary source module.  */
#define L_IPO_IS_AUXILIARY_MODULE (L_IPO_COMP_MODE && current_module_id \
                             && current_module_id != primary_module_id)

/* Current module id.  */
extern unsigned current_module_id;
extern bool include_all_aux;
extern struct gcov_module_info **module_infos;
extern int is_last_module (unsigned mod_id);

extern unsigned num_in_fnames;
extern int at_eof;
extern bool parser_parsing_start;

void push_module_scope (void);
void pop_module_scope (void);
tree lipo_save_decl (tree src);
void lipo_restore_decl (tree, tree);
void add_decl_to_current_module_scope (tree decl, void *b);
int lipo_cmp_type (tree t1, tree t2);
tree get_type_or_decl_name (tree);
int equivalent_struct_types_for_tbaa (const_tree t1, const_tree t2);
void lipo_link_and_fixup (void);
extern void copy_defined_module_set (tree, tree);
extern bool is_parsing_done_p (void);
extern void record_module_name (unsigned int, const char *);
extern const char* get_module_name (unsigned int);

#endif

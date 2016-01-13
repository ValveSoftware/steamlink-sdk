/*
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002, 2003 Paolo Maggi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef UNDOVIEW_UNDO_MANAGER_H_
#define UNDOVIEW_UNDO_MANAGER_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_UNDO_MANAGER            (gtk_source_undo_manager_get_type())

#define GTK_SOURCE_UNDO_MANAGER(obj)            \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_SOURCE_TYPE_UNDO_MANAGER, GtkSourceUndoManager))

#define GTK_SOURCE_UNDO_MANAGER_CLASS(klass)    \
  (G_TYPE_CHECK_CLASS_CAST((klass), GTK_SOURCE_TYPE_UNDO_MANAGER, GtkSourceUndoManagerClass))

#define GTK_SOURCE_IS_UNDO_MANAGER(obj)         \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_SOURCE_TYPE_UNDO_MANAGER))

#define GTK_SOURCE_IS_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_SOURCE_TYPE_UNDO_MANAGER))

#define GTK_SOURCE_UNDO_MANAGER_GET_CLASS(obj)  \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_SOURCE_TYPE_UNDO_MANAGER, GtkSourceUndoManagerClass))

typedef struct _GtkSourceUndoManager          GtkSourceUndoManager;
typedef struct _GtkSourceUndoManagerClass   GtkSourceUndoManagerClass;

typedef struct _GtkSourceUndoManagerPrivate   GtkSourceUndoManagerPrivate;

struct _GtkSourceUndoManager
{
  GObject base;

  GtkSourceUndoManagerPrivate *priv;
};

struct _GtkSourceUndoManagerClass
{
  GObjectClass parent_class;

  /* Signals */
  void (*can_undo)(GtkSourceUndoManager *um, gboolean can_undo);
  void (*can_redo)(GtkSourceUndoManager *um, gboolean can_redo);
};

GType            gtk_source_undo_manager_get_type(void) G_GNUC_CONST;

GtkSourceUndoManager*   gtk_source_undo_manager_new(GtkTextBuffer     *buffer);

gboolean    gtk_source_undo_manager_can_undo(const GtkSourceUndoManager *um);
gboolean    gtk_source_undo_manager_can_redo(const GtkSourceUndoManager *um);

void      gtk_source_undo_manager_undo(GtkSourceUndoManager   *um);
void      gtk_source_undo_manager_redo(GtkSourceUndoManager   *um);

void      gtk_source_undo_manager_begin_not_undoable_action(GtkSourceUndoManager  *um);
void      gtk_source_undo_manager_end_not_undoable_action(GtkSourceUndoManager  *um);

gint      gtk_source_undo_manager_get_max_undo_levels(GtkSourceUndoManager   *um);
void      gtk_source_undo_manager_set_max_undo_levels(GtkSourceUndoManager   *um,
                        gint        undo_levels);

G_END_DECLS

#endif  // UNDOVIEW_UNDO_MANAGER_H_


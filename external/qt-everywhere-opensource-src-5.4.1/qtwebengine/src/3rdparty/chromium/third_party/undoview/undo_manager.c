/*
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "undo_manager.h"

#define DEFAULT_MAX_UNDO_LEVELS 25

typedef struct _GtkSourceUndoAction GtkSourceUndoAction;
typedef struct _GtkSourceUndoInsertAction GtkSourceUndoInsertAction;
typedef struct _GtkSourceUndoDeleteAction GtkSourceUndoDeleteAction;

typedef enum {
  GTK_SOURCE_UNDO_ACTION_INSERT,
  GTK_SOURCE_UNDO_ACTION_DELETE,
} GtkSourceUndoActionType;

/*
 * We use offsets instead of GtkTextIters because the last ones
 * require to much memory in this context without giving us any advantage.
 */

struct _GtkSourceUndoInsertAction {
  gint pos;
  gchar *text;
  gint length;
  gint chars;
};

struct _GtkSourceUndoDeleteAction {
  gint start;
  gint end;
  gchar *text;
  gboolean forward;
};

struct _GtkSourceUndoAction {
  GtkSourceUndoActionType action_type;

  union {
    GtkSourceUndoInsertAction insert;
    GtkSourceUndoDeleteAction delete;
  } action;

  gint order_in_group;

  /* It is TRUE whether the action can be merged with the following action. */
  guint mergeable : 1;

  /* It is TRUE whether the action is marked as "modified".
   * An action is marked as "modified" if it changed the
   * state of the buffer from "not modified" to "modified". Only the first
   * action of a group can be marked as modified.
   * There can be a single action marked as "modified" in the actions list.
   */
  guint modified : 1;
};

/* INVALID is a pointer to an invalid action */
#define INVALID ((void *) "IA")

struct _GtkSourceUndoManagerPrivate {
  GtkTextBuffer *document;

  GList* actions;
  gint next_redo;

  gint actions_in_current_group;

  gint running_not_undoable_actions;

  gint num_of_groups;

  gint max_undo_levels;

  guint can_undo : 1;
  guint can_redo : 1;

  /* It is TRUE whether, while undoing an action of the current group (with order_in_group > 1),
   * the state of the buffer changed from "not modified" to "modified".
   */
  guint modified_undoing_group : 1;

  /* Pointer to the action (in the action list) marked as "modified".
   * It is NULL when no action is marked as "modified".
   * It is INVALID when the action marked as "modified" has been removed
   * from the action list (freeing the list or resizing it) */
  GtkSourceUndoAction *modified_action;
};

enum {
  CAN_UNDO,
  CAN_REDO,
  LAST_SIGNAL
};

#if !defined(NDEBUG)
static void
print_state(GtkSourceUndoManager* um)
{
  fprintf(stderr, "\n***\n");
  GList* actions = um->priv->actions;

  for (; actions; actions = g_list_next(actions)) {
    GtkSourceUndoAction* act = actions->data;
    fprintf(stderr, "* type = %s\n", act->action_type == GTK_SOURCE_UNDO_ACTION_DELETE ?
        "delete" : "insert");

    fprintf(stderr, "\ttext = %s\n", act->action_type == GTK_SOURCE_UNDO_ACTION_DELETE
        ? act->action.delete.text : act->action.insert.text);
    fprintf(stderr, "\torder = %d\n", act->order_in_group);
  }

  fprintf(stderr, "* next redo: %d\n", um->priv->next_redo);
  fprintf(stderr, "* num of groups: %d\n", um->priv->num_of_groups);
  fprintf(stderr, "* actions in group: %d\n", um->priv->actions_in_current_group);
}
#endif

static void gtk_source_undo_manager_class_init(GtkSourceUndoManagerClass *klass);
static void gtk_source_undo_manager_init(GtkSourceUndoManager *um);
static void gtk_source_undo_manager_finalize(GObject *object);

static void gtk_source_undo_manager_insert_text_handler(GtkTextBuffer *buffer,
                  GtkTextIter *pos,
                                         const gchar *text,
                  gint length,
                  GtkSourceUndoManager *um);
static void gtk_source_undo_manager_delete_range_handler(GtkTextBuffer *buffer,
                  GtkTextIter *start,
                                          GtkTextIter *end,
                  GtkSourceUndoManager *um);
static void gtk_source_undo_manager_begin_user_action_handler(GtkTextBuffer *buffer,
                 GtkSourceUndoManager *um);
static void gtk_source_undo_manager_modified_changed_handler(GtkTextBuffer *buffer,
                 GtkSourceUndoManager *um);

static void gtk_source_undo_manager_free_action_list(GtkSourceUndoManager *um);

static void gtk_source_undo_manager_add_action(GtkSourceUndoManager *um,
                                                const GtkSourceUndoAction *undo_action);
static void gtk_source_undo_manager_free_first_n_actions(GtkSourceUndoManager *um,
                 gint n);
static void gtk_source_undo_manager_check_list_size(GtkSourceUndoManager *um);

static gboolean gtk_source_undo_manager_merge_action(GtkSourceUndoManager *um,
                                               const GtkSourceUndoAction *undo_action);

static GObjectClass *parent_class = NULL;
static guint undo_manager_signals [LAST_SIGNAL] = { 0 };

GType
gtk_source_undo_manager_get_type(void) {
  static GType undo_manager_type = 0;

    if(undo_manager_type == 0)
      {
          static const GTypeInfo our_info =
          {
            sizeof(GtkSourceUndoManagerClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc) gtk_source_undo_manager_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(GtkSourceUndoManager),
            0, /* n_preallocs */
            (GInstanceInitFunc) gtk_source_undo_manager_init,
            NULL /* value_table */
          };

          undo_manager_type = g_type_register_static(G_TYPE_OBJECT,
              "GtkSourceUndoManager",
              &our_info,
              0);
      }

  return undo_manager_type;
}

static void
gtk_source_undo_manager_class_init(GtkSourceUndoManagerClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  parent_class = g_type_class_peek_parent(klass);

  object_class->finalize = gtk_source_undo_manager_finalize;

  klass->can_undo = NULL;
  klass->can_redo = NULL;

  undo_manager_signals[CAN_UNDO] =
       g_signal_new("can_undo",
            G_OBJECT_CLASS_TYPE(object_class),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(GtkSourceUndoManagerClass, can_undo),
            NULL, NULL,
            g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE,
            1,
            G_TYPE_BOOLEAN);

  undo_manager_signals[CAN_REDO] =
       g_signal_new("can_redo",
            G_OBJECT_CLASS_TYPE(object_class),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(GtkSourceUndoManagerClass, can_redo),
            NULL, NULL,
            g_cclosure_marshal_VOID__BOOLEAN,
            G_TYPE_NONE,
            1,
            G_TYPE_BOOLEAN);
}

static void
gtk_source_undo_manager_init(GtkSourceUndoManager *um) {
  um->priv = g_new0(GtkSourceUndoManagerPrivate, 1);

  um->priv->actions = NULL;
  um->priv->next_redo = 0;

  um->priv->can_undo = FALSE;
  um->priv->can_redo = FALSE;

  um->priv->running_not_undoable_actions = 0;

  um->priv->num_of_groups = 0;

  um->priv->max_undo_levels = DEFAULT_MAX_UNDO_LEVELS;

  um->priv->modified_action = NULL;

  um->priv->modified_undoing_group = FALSE;
}

static void
gtk_source_undo_manager_finalize(GObject *object) {
  GtkSourceUndoManager *um;

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(object));

  um = GTK_SOURCE_UNDO_MANAGER(object);

  g_return_if_fail(um->priv != NULL);

  if(um->priv->actions != NULL)
    gtk_source_undo_manager_free_action_list(um);

  g_signal_handlers_disconnect_by_func(G_OBJECT(um->priv->document),
        G_CALLBACK(gtk_source_undo_manager_delete_range_handler),
        um);

  g_signal_handlers_disconnect_by_func(G_OBJECT(um->priv->document),
        G_CALLBACK(gtk_source_undo_manager_insert_text_handler),
        um);

  g_signal_handlers_disconnect_by_func(G_OBJECT(um->priv->document),
        G_CALLBACK(gtk_source_undo_manager_begin_user_action_handler),
        um);

  g_signal_handlers_disconnect_by_func(G_OBJECT(um->priv->document),
        G_CALLBACK(gtk_source_undo_manager_modified_changed_handler),
        um);

  g_free(um->priv);

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

GtkSourceUndoManager*
gtk_source_undo_manager_new(GtkTextBuffer* buffer) {
  GtkSourceUndoManager *um;

  um = GTK_SOURCE_UNDO_MANAGER(g_object_new(GTK_SOURCE_TYPE_UNDO_MANAGER, NULL));

  g_return_val_if_fail(um->priv != NULL, NULL);
  um->priv->document = buffer;

  g_signal_connect(G_OBJECT(buffer), "insert_text",
        G_CALLBACK(gtk_source_undo_manager_insert_text_handler),
        um);

  g_signal_connect(G_OBJECT(buffer), "delete_range",
        G_CALLBACK(gtk_source_undo_manager_delete_range_handler),
        um);

  g_signal_connect(G_OBJECT(buffer), "begin_user_action",
        G_CALLBACK(gtk_source_undo_manager_begin_user_action_handler),
        um);

  g_signal_connect(G_OBJECT(buffer), "modified_changed",
        G_CALLBACK(gtk_source_undo_manager_modified_changed_handler),
        um);
  return um;
}

void
gtk_source_undo_manager_begin_not_undoable_action(GtkSourceUndoManager *um) {
  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);

  ++um->priv->running_not_undoable_actions;
}

static void
gtk_source_undo_manager_end_not_undoable_action_internal(GtkSourceUndoManager *um) {
  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);

  g_return_if_fail(um->priv->running_not_undoable_actions > 0);

  --um->priv->running_not_undoable_actions;
}

void
gtk_source_undo_manager_end_not_undoable_action(GtkSourceUndoManager *um) {
  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);

  gtk_source_undo_manager_end_not_undoable_action_internal(um);

  if(um->priv->running_not_undoable_actions == 0)
  {
    gtk_source_undo_manager_free_action_list(um);

    um->priv->next_redo = -1;

    if(um->priv->can_undo)
    {
      um->priv->can_undo = FALSE;
      g_signal_emit(G_OBJECT(um),
               undo_manager_signals [CAN_UNDO],
               0,
               FALSE);
    }

    if(um->priv->can_redo)
    {
      um->priv->can_redo = FALSE;
      g_signal_emit(G_OBJECT(um),
               undo_manager_signals [CAN_REDO],
               0,
               FALSE);
    }
  }
}

gboolean
gtk_source_undo_manager_can_undo(const GtkSourceUndoManager *um) {
  g_return_val_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um), FALSE);
  g_return_val_if_fail(um->priv != NULL, FALSE);

  return um->priv->can_undo;
}

gboolean
gtk_source_undo_manager_can_redo(const GtkSourceUndoManager *um) {
  g_return_val_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um), FALSE);
  g_return_val_if_fail(um->priv != NULL, FALSE);

  return um->priv->can_redo;
}

static void
set_cursor(GtkTextBuffer *buffer, gint cursor) {
  GtkTextIter iter;

  /* Place the cursor at the requested position */
  gtk_text_buffer_get_iter_at_offset(buffer, &iter, cursor);
  gtk_text_buffer_place_cursor(buffer, &iter);
}

static void
insert_text(GtkTextBuffer *buffer, gint pos, const gchar *text, gint len) {
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_offset(buffer, &iter, pos);
  gtk_text_buffer_insert(buffer, &iter, text, len);
}

static void
delete_text(GtkTextBuffer *buffer, gint start, gint end) {
  GtkTextIter start_iter;
  GtkTextIter end_iter;

  gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);

  if(end < 0)
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
  else
    gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);

  gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
}

static gchar*
get_chars(GtkTextBuffer *buffer, gint start, gint end) {
  GtkTextIter start_iter;
  GtkTextIter end_iter;

  gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);

  if(end < 0)
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
  else
    gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);

  return gtk_text_buffer_get_slice(buffer, &start_iter, &end_iter, TRUE);
}

void
gtk_source_undo_manager_undo(GtkSourceUndoManager *um) {
  GtkSourceUndoAction *undo_action;
  gboolean modified = FALSE;

  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);
  g_return_if_fail(um->priv->can_undo);

  um->priv->modified_undoing_group = FALSE;

  gtk_source_undo_manager_begin_not_undoable_action(um);

  do
  {
    undo_action = g_list_nth_data(um->priv->actions, um->priv->next_redo + 1);
    g_return_if_fail(undo_action != NULL);

    /* undo_action->modified can be TRUE only if undo_action->order_in_group <= 1 */
    g_return_if_fail((undo_action->order_in_group <= 1) ||
        ((undo_action->order_in_group > 1) && !undo_action->modified));

    if(undo_action->order_in_group <= 1)
    {
      /* Set modified to TRUE only if the buffer did not change its state from
       * "not modified" to "modified" undoing an action(with order_in_group > 1)
       * in current group. */
      modified =(undo_action->modified && !um->priv->modified_undoing_group);
    }

    switch(undo_action->action_type)
    {
      case GTK_SOURCE_UNDO_ACTION_DELETE:
        insert_text(
          um->priv->document,
          undo_action->action.delete.start,
          undo_action->action.delete.text,
          strlen(undo_action->action.delete.text));

        if(undo_action->action.delete.forward)
          set_cursor(
            um->priv->document,
            undo_action->action.delete.start);
        else
          set_cursor(
            um->priv->document,
            undo_action->action.delete.end);

        break;

      case GTK_SOURCE_UNDO_ACTION_INSERT:
        delete_text(
          um->priv->document,
          undo_action->action.insert.pos,
          undo_action->action.insert.pos +
            undo_action->action.insert.chars);

        set_cursor(
          um->priv->document,
          undo_action->action.insert.pos);
        break;

      default:
        /* Unknown action type. */
        g_return_if_reached();
    }

    ++um->priv->next_redo;

  } while(undo_action->order_in_group > 1);

  if(modified)
  {
    --um->priv->next_redo;
    gtk_text_buffer_set_modified(um->priv->document, FALSE);
    ++um->priv->next_redo;
  }

  gtk_source_undo_manager_end_not_undoable_action_internal(um);

  um->priv->modified_undoing_group = FALSE;

  if(!um->priv->can_redo)
  {
    um->priv->can_redo = TRUE;
    g_signal_emit(G_OBJECT(um),
             undo_manager_signals [CAN_REDO],
             0,
             TRUE);
  }

  if(um->priv->next_redo >=(gint)(g_list_length(um->priv->actions) - 1))
  {
    um->priv->can_undo = FALSE;
    g_signal_emit(G_OBJECT(um),
             undo_manager_signals [CAN_UNDO],
             0,
             FALSE);
  }
}

void
gtk_source_undo_manager_redo(GtkSourceUndoManager *um) {
  GtkSourceUndoAction *undo_action;
  gboolean modified = FALSE;

  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);
  g_return_if_fail(um->priv->can_redo);

  undo_action = g_list_nth_data(um->priv->actions, um->priv->next_redo);
  g_return_if_fail(undo_action != NULL);

  gtk_source_undo_manager_begin_not_undoable_action(um);

  do
  {
    if(undo_action->modified)
    {
      g_return_if_fail(undo_action->order_in_group <= 1);
      modified = TRUE;
    }

    --um->priv->next_redo;

    switch(undo_action->action_type)
    {
      case GTK_SOURCE_UNDO_ACTION_DELETE:
        delete_text(
          um->priv->document,
          undo_action->action.delete.start,
          undo_action->action.delete.end);

        set_cursor(
          um->priv->document,
          undo_action->action.delete.start);

        break;

      case GTK_SOURCE_UNDO_ACTION_INSERT:
        set_cursor(
          um->priv->document,
          undo_action->action.insert.pos);

        insert_text(
          um->priv->document,
          undo_action->action.insert.pos,
          undo_action->action.insert.text,
          undo_action->action.insert.length);

        break;

      default:
        /* Unknown action type */
        ++um->priv->next_redo;
        g_return_if_reached();
    }

    if(um->priv->next_redo < 0)
      undo_action = NULL;
    else
      undo_action = g_list_nth_data(um->priv->actions, um->priv->next_redo);

  } while((undo_action != NULL) &&(undo_action->order_in_group > 1));

  if(modified)
  {
    ++um->priv->next_redo;
    gtk_text_buffer_set_modified(um->priv->document, FALSE);
    --um->priv->next_redo;
  }

  gtk_source_undo_manager_end_not_undoable_action_internal(um);

  if(um->priv->next_redo < 0)
  {
    um->priv->can_redo = FALSE;
    g_signal_emit(G_OBJECT(um), undo_manager_signals [CAN_REDO], 0, FALSE);
  }

  if(!um->priv->can_undo)
  {
    um->priv->can_undo = TRUE;
    g_signal_emit(G_OBJECT(um), undo_manager_signals [CAN_UNDO], 0, TRUE);
  }
}

static void
gtk_source_undo_action_free(GtkSourceUndoAction *action) {
  if(action == NULL)
    return;

  if(action->action_type == GTK_SOURCE_UNDO_ACTION_INSERT)
    g_free(action->action.insert.text);
  else if(action->action_type == GTK_SOURCE_UNDO_ACTION_DELETE)
    g_free(action->action.delete.text);
  else
    g_return_if_reached();

  g_free(action);
}

static void
gtk_source_undo_manager_free_action_list(GtkSourceUndoManager *um) {
  GList *l;

  l = um->priv->actions;

  while(l != NULL)
  {
    GtkSourceUndoAction *action = l->data;

    if(action->order_in_group == 1)
      --um->priv->num_of_groups;
    um->priv->actions_in_current_group = action->order_in_group - 1;

    if(action->modified)
      um->priv->modified_action = INVALID;

    gtk_source_undo_action_free(action);

    l = g_list_next(l);
  }

  g_list_free(um->priv->actions);
  um->priv->actions = NULL;
}

static void
gtk_source_undo_manager_insert_text_handler(GtkTextBuffer *buffer,
    GtkTextIter *pos,
    const gchar *text,
    gint length,
    GtkSourceUndoManager *um) {
  GtkSourceUndoAction undo_action;

  if(um->priv->running_not_undoable_actions > 0)
    return;

  undo_action.action_type = GTK_SOURCE_UNDO_ACTION_INSERT;

  undo_action.action.insert.pos = gtk_text_iter_get_offset(pos);
  undo_action.action.insert.text =(gchar*) text;
  undo_action.action.insert.length = length;
  undo_action.action.insert.chars = g_utf8_strlen(text, length);

  if((undo_action.action.insert.chars > 1) ||(g_utf8_get_char(text) == '\n'))

           undo_action.mergeable = FALSE;
  else
    undo_action.mergeable = TRUE;

  undo_action.modified = FALSE;

  gtk_source_undo_manager_add_action(um, &undo_action);
}

static void
gtk_source_undo_manager_delete_range_handler(GtkTextBuffer *buffer,
    GtkTextIter *start,
    GtkTextIter *end,
    GtkSourceUndoManager *um) {
  GtkSourceUndoAction undo_action;
  GtkTextIter insert_iter;

  if(um->priv->running_not_undoable_actions > 0)
    return;

  undo_action.action_type = GTK_SOURCE_UNDO_ACTION_DELETE;

  gtk_text_iter_order(start, end);

  undo_action.action.delete.start = gtk_text_iter_get_offset(start);
  undo_action.action.delete.end = gtk_text_iter_get_offset(end);

  undo_action.action.delete.text = get_chars(
            buffer,
            undo_action.action.delete.start,
            undo_action.action.delete.end);

  /* figure out if the user used the Delete or the Backspace key */
  gtk_text_buffer_get_iter_at_mark(buffer, &insert_iter,
            gtk_text_buffer_get_insert(buffer));
  if(gtk_text_iter_get_offset(&insert_iter) <= undo_action.action.delete.start)
    undo_action.action.delete.forward = TRUE;
  else
    undo_action.action.delete.forward = FALSE;

  if(((undo_action.action.delete.end - undo_action.action.delete.start) > 1) ||
      (g_utf8_get_char(undo_action.action.delete.text ) == '\n'))
           undo_action.mergeable = FALSE;
  else
    undo_action.mergeable = TRUE;

  undo_action.modified = FALSE;

  gtk_source_undo_manager_add_action(um, &undo_action);

  g_free(undo_action.action.delete.text);

}

static void
gtk_source_undo_manager_begin_user_action_handler(GtkTextBuffer *buffer, GtkSourceUndoManager *um) {
  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);

  if(um->priv->running_not_undoable_actions > 0)
    return;

  um->priv->actions_in_current_group = 0;
}

static void
gtk_source_undo_manager_add_action(GtkSourceUndoManager *um,
            const GtkSourceUndoAction *undo_action) {
  GtkSourceUndoAction* action;

  if(um->priv->next_redo >= 0)
  {
    gtk_source_undo_manager_free_first_n_actions(um, um->priv->next_redo + 1);
  }

  um->priv->next_redo = -1;

  if(!gtk_source_undo_manager_merge_action(um, undo_action))
  {
    action = g_new(GtkSourceUndoAction, 1);
    *action = *undo_action;

    if(action->action_type == GTK_SOURCE_UNDO_ACTION_INSERT)
      action->action.insert.text = g_strndup(undo_action->action.insert.text, undo_action->action.insert.length);
    else if(action->action_type == GTK_SOURCE_UNDO_ACTION_DELETE)
      action->action.delete.text = g_strdup(undo_action->action.delete.text);
    else
    {
      g_free(action);
      g_return_if_reached();
    }

    ++um->priv->actions_in_current_group;
    action->order_in_group = um->priv->actions_in_current_group;

    if(action->order_in_group == 1)
      ++um->priv->num_of_groups;

    um->priv->actions = g_list_prepend(um->priv->actions, action);
  }

  gtk_source_undo_manager_check_list_size(um);

  if(!um->priv->can_undo)
  {
    um->priv->can_undo = TRUE;
    g_signal_emit(G_OBJECT(um), undo_manager_signals [CAN_UNDO], 0, TRUE);
  }

  if(um->priv->can_redo)
  {
    um->priv->can_redo = FALSE;
    g_signal_emit(G_OBJECT(um), undo_manager_signals [CAN_REDO], 0, FALSE);
  }
}

static void
gtk_source_undo_manager_free_first_n_actions(GtkSourceUndoManager *um,
                gint n) {
  gint i;

  if(um->priv->actions == NULL)
    return;

  for(i = 0; i < n; i++)
  {
    GtkSourceUndoAction *action = g_list_first(um->priv->actions)->data;

    if(action->order_in_group == 1)
      --um->priv->num_of_groups;
    um->priv->actions_in_current_group = action->order_in_group - 1;

    if(action->modified)
      um->priv->modified_action = INVALID;

    gtk_source_undo_action_free(action);

    um->priv->actions = g_list_delete_link(um->priv->actions,
              um->priv->actions);

    if(um->priv->actions == NULL)
      return;
  }
}

static void
gtk_source_undo_manager_check_list_size(GtkSourceUndoManager *um) {
  gint undo_levels;

  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);

  undo_levels = gtk_source_undo_manager_get_max_undo_levels(um);

  if(undo_levels < 1)
    return;

  if(um->priv->num_of_groups > undo_levels)
  {
    GtkSourceUndoAction *undo_action;
    GList *last;

    last = g_list_last(um->priv->actions);
    undo_action =(GtkSourceUndoAction*) last->data;

    do
    {
      GList *tmp;

      if(undo_action->order_in_group == 1)
        --um->priv->num_of_groups;
      um->priv->actions_in_current_group = undo_action->order_in_group - 1;

      if(undo_action->modified)
        um->priv->modified_action = INVALID;

      gtk_source_undo_action_free(undo_action);

      tmp = g_list_previous(last);
      um->priv->actions = g_list_delete_link(um->priv->actions, last);
      last = tmp;
      g_return_if_fail(last != NULL);

      undo_action =(GtkSourceUndoAction*) last->data;

    } while((undo_action->order_in_group > 1) ||
      (um->priv->num_of_groups > undo_levels));
  }
}

/**
 * gtk_source_undo_manager_merge_action:
 * @um: a #GtkSourceUndoManager.
 * @undo_action: a #GtkSourceUndoAction.
 *
 * This function tries to merge the undo action at the top of
 * the stack with a new undo action. So when we undo for example
 * typing, we can undo the whole word and not each letter by itself.
 *
 * Return Value: %TRUE is merge was successful, %FALSE otherwise.
 **/
static gboolean
gtk_source_undo_manager_merge_action(GtkSourceUndoManager *um,
              const GtkSourceUndoAction *undo_action) {
  GtkSourceUndoAction *last_action;

  g_return_val_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um), FALSE);
  g_return_val_if_fail(um->priv != NULL, FALSE);

  if(um->priv->actions == NULL)
    return FALSE;

  last_action =(GtkSourceUndoAction*) g_list_nth_data(um->priv->actions, 0);

  if(!last_action->mergeable)
    return FALSE;

  if((!undo_action->mergeable) ||
     (undo_action->action_type != last_action->action_type))
  {
    last_action->mergeable = FALSE;
    return FALSE;
  }

  if(undo_action->action_type == GTK_SOURCE_UNDO_ACTION_DELETE)
  {
    if((last_action->action.delete.forward != undo_action->action.delete.forward) ||
       ((last_action->action.delete.start != undo_action->action.delete.start) &&
        (last_action->action.delete.start != undo_action->action.delete.end)))
    {
      last_action->mergeable = FALSE;
      return FALSE;
    }

    if(last_action->action.delete.start == undo_action->action.delete.start)
    {
      gchar *str;

#define L (last_action->action.delete.end - last_action->action.delete.start - 1)
#define g_utf8_get_char_at(p,i) g_utf8_get_char(g_utf8_offset_to_pointer((p),(i)))

      /* Deleted with the delete key */
      if((g_utf8_get_char(undo_action->action.delete.text) != ' ') &&
         (g_utf8_get_char(undo_action->action.delete.text) != '\t') &&
                           ((g_utf8_get_char_at(last_action->action.delete.text, L) == ' ') ||
          (g_utf8_get_char_at(last_action->action.delete.text, L) == '\t')))
      {
        last_action->mergeable = FALSE;
        return FALSE;
      }

      str = g_strdup_printf("%s%s", last_action->action.delete.text,
        undo_action->action.delete.text);

      g_free(last_action->action.delete.text);
      last_action->action.delete.end +=(undo_action->action.delete.end -
                 undo_action->action.delete.start);
      last_action->action.delete.text = str;
    }
    else
    {
      gchar *str;

      /* Deleted with the backspace key */
      if((g_utf8_get_char(undo_action->action.delete.text) != ' ') &&
         (g_utf8_get_char(undo_action->action.delete.text) != '\t') &&
                           ((g_utf8_get_char(last_action->action.delete.text) == ' ') ||
          (g_utf8_get_char(last_action->action.delete.text) == '\t')))
      {
        last_action->mergeable = FALSE;
        return FALSE;
      }

      str = g_strdup_printf("%s%s", undo_action->action.delete.text,
        last_action->action.delete.text);

      g_free(last_action->action.delete.text);
      last_action->action.delete.start = undo_action->action.delete.start;
      last_action->action.delete.text = str;
    }
  }
  else if(undo_action->action_type == GTK_SOURCE_UNDO_ACTION_INSERT)
  {
    gchar* str;

#define I (last_action->action.insert.chars - 1)

    if((undo_action->action.insert.pos !=
          (last_action->action.insert.pos + last_action->action.insert.chars)) ||
       ((g_utf8_get_char(undo_action->action.insert.text) != ' ') &&
         (g_utf8_get_char(undo_action->action.insert.text) != '\t') &&
        ((g_utf8_get_char_at(last_action->action.insert.text, I) == ' ') ||
         (g_utf8_get_char_at(last_action->action.insert.text, I) == '\t')))
       )
    {
      last_action->mergeable = FALSE;
      return FALSE;
    }

    str = g_strdup_printf("%s%s", last_action->action.insert.text,
        undo_action->action.insert.text);

    g_free(last_action->action.insert.text);
    last_action->action.insert.length += undo_action->action.insert.length;
    last_action->action.insert.text = str;
    last_action->action.insert.chars += undo_action->action.insert.chars;

  }
  else
    /* Unknown action inside undo merge encountered */
    g_return_val_if_reached(TRUE);

  return TRUE;
}

gint
gtk_source_undo_manager_get_max_undo_levels(GtkSourceUndoManager *um) {
  g_return_val_if_fail(um != NULL, 0);
  g_return_val_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um), 0);

  return um->priv->max_undo_levels;
}

void
gtk_source_undo_manager_set_max_undo_levels(GtkSourceUndoManager *um,
                 gint max_undo_levels) {
  gint old_levels;

  g_return_if_fail(um != NULL);
  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));

  old_levels = um->priv->max_undo_levels;
  um->priv->max_undo_levels = max_undo_levels;

  if(max_undo_levels < 1)
    return;

  if(old_levels > max_undo_levels)
  {
    /* strip redo actions first */
    while(um->priv->next_redo >= 0 &&(um->priv->num_of_groups > max_undo_levels))
    {
      gtk_source_undo_manager_free_first_n_actions(um, 1);
      um->priv->next_redo--;
    }

    /* now remove undo actions if necessary */
    gtk_source_undo_manager_check_list_size(um);

    /* emit "can_undo" and/or "can_redo" if appropiate */
    if(um->priv->next_redo < 0 && um->priv->can_redo)
    {
      um->priv->can_redo = FALSE;
      g_signal_emit(G_OBJECT(um), undo_manager_signals [CAN_REDO], 0, FALSE);
    }

    if(um->priv->can_undo &&
        um->priv->next_redo >=(gint)(g_list_length(um->priv->actions) - 1))
    {
      um->priv->can_undo = FALSE;
      g_signal_emit(G_OBJECT(um), undo_manager_signals [CAN_UNDO], 0, FALSE);
    }
  }
}

static void
gtk_source_undo_manager_modified_changed_handler(GtkTextBuffer *buffer,
                                                GtkSourceUndoManager *um) {
  GtkSourceUndoAction *action;
  GList *list;

  g_return_if_fail(GTK_SOURCE_IS_UNDO_MANAGER(um));
  g_return_if_fail(um->priv != NULL);

  if(um->priv->actions == NULL)
    return;

  list = g_list_nth(um->priv->actions, um->priv->next_redo + 1);

  if(list != NULL)
    action =(GtkSourceUndoAction*) list->data;
  else
    action = NULL;

  if(gtk_text_buffer_get_modified(buffer) == FALSE)
  {
    if(action != NULL)
      action->mergeable = FALSE;

    if(um->priv->modified_action != NULL)
    {
      if(um->priv->modified_action != INVALID)
        um->priv->modified_action->modified = FALSE;

      um->priv->modified_action = NULL;
    }

    return;
  }

  if(action == NULL)
  {
    g_return_if_fail(um->priv->running_not_undoable_actions > 0);

    return;
  }

  /* gtk_text_buffer_get_modified(buffer) == TRUE */

  g_return_if_fail(um->priv->modified_action == NULL);

  if(action->order_in_group > 1)
    um->priv->modified_undoing_group = TRUE;

  while(action->order_in_group > 1)
  {
    list = g_list_next(list);
    g_return_if_fail(list != NULL);

    action =(GtkSourceUndoAction*) list->data;
    g_return_if_fail(action != NULL);
  }

  action->modified = TRUE;
  um->priv->modified_action = action;
}


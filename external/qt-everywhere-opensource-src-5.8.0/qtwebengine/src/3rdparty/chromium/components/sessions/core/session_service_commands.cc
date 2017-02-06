// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_service_commands.h"

#include <stdint.h>
#include <string.h>
#include <utility>
#include <vector>

#include "base/pickle.h"
#include "components/sessions/core/base_session_service_commands.h"
#include "components/sessions/core/base_session_service_delegate.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_types.h"

namespace sessions {

// Identifier for commands written to file.
static const SessionCommand::id_type kCommandSetTabWindow = 0;
// OBSOLETE Superseded by kCommandSetWindowBounds3.
// static const SessionCommand::id_type kCommandSetWindowBounds = 1;
static const SessionCommand::id_type kCommandSetTabIndexInWindow = 2;
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromBack = 5;
static const SessionCommand::id_type kCommandUpdateTabNavigation = 6;
static const SessionCommand::id_type kCommandSetSelectedNavigationIndex = 7;
static const SessionCommand::id_type kCommandSetSelectedTabInIndex = 8;
static const SessionCommand::id_type kCommandSetWindowType = 9;
// OBSOLETE Superseded by kCommandSetWindowBounds3. Except for data migration.
// static const SessionCommand::id_type kCommandSetWindowBounds2 = 10;
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromFront = 11;
static const SessionCommand::id_type kCommandSetPinnedState = 12;
static const SessionCommand::id_type kCommandSetExtensionAppID = 13;
static const SessionCommand::id_type kCommandSetWindowBounds3 = 14;
static const SessionCommand::id_type kCommandSetWindowAppName = 15;
static const SessionCommand::id_type kCommandTabClosed = 16;
static const SessionCommand::id_type kCommandWindowClosed = 17;
static const SessionCommand::id_type kCommandSetTabUserAgentOverride = 18;
static const SessionCommand::id_type kCommandSessionStorageAssociated = 19;
static const SessionCommand::id_type kCommandSetActiveWindow = 20;
static const SessionCommand::id_type kCommandLastActiveTime = 21;
// OBSOLETE Superseded by kCommandSetWindowWorkspace2.
// static const SessionCommand::id_type kCommandSetWindowWorkspace = 22;
static const SessionCommand::id_type kCommandSetWindowWorkspace2 = 23;

namespace {

// Various payload structures.
struct ClosedPayload {
  SessionID::id_type id;
  int64_t close_time;
};

struct WindowBoundsPayload2 {
  SessionID::id_type window_id;
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  bool is_maximized;
};

struct WindowBoundsPayload3 {
  SessionID::id_type window_id;
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  int32_t show_state;
};

typedef SessionID::id_type ActiveWindowPayload;

struct IDAndIndexPayload {
  SessionID::id_type id;
  int32_t index;
};

typedef IDAndIndexPayload TabIndexInWindowPayload;

typedef IDAndIndexPayload TabNavigationPathPrunedFromBackPayload;

typedef IDAndIndexPayload SelectedNavigationIndexPayload;

typedef IDAndIndexPayload SelectedTabInIndexPayload;

typedef IDAndIndexPayload WindowTypePayload;

typedef IDAndIndexPayload TabNavigationPathPrunedFromFrontPayload;

struct PinnedStatePayload {
  SessionID::id_type tab_id;
  bool pinned_state;
};

struct LastActiveTimePayload {
  SessionID::id_type tab_id;
  int64_t last_active_time;
};

// Persisted versions of ui::WindowShowState that are written to disk and can
// never change.
enum PersistedWindowShowState {
  // SHOW_STATE_DEFAULT (0) never persisted.
  PERSISTED_SHOW_STATE_NORMAL = 1,
  PERSISTED_SHOW_STATE_MINIMIZED = 2,
  PERSISTED_SHOW_STATE_MAXIMIZED = 3,
  // SHOW_STATE_INACTIVE (4) never persisted.
  PERSISTED_SHOW_STATE_FULLSCREEN = 5,
  PERSISTED_SHOW_STATE_DETACHED_DEPRECATED = 6,
  PERSISTED_SHOW_STATE_DOCKED = 7,
  PERSISTED_SHOW_STATE_END = 7
};

typedef std::map<SessionID::id_type, SessionTab*> IdToSessionTab;
typedef std::map<SessionID::id_type, SessionWindow*> IdToSessionWindow;

// Assert to ensure PersistedWindowShowState is updated if ui::WindowShowState
// is changed.
static_assert(ui::SHOW_STATE_END ==
                  static_cast<ui::WindowShowState>(PERSISTED_SHOW_STATE_END),
              "SHOW_STATE_END must equal PERSISTED_SHOW_STATE_END");

// Returns the show state to store to disk based |state|.
PersistedWindowShowState ShowStateToPersistedShowState(
    ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_NORMAL:
      return PERSISTED_SHOW_STATE_NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return PERSISTED_SHOW_STATE_MINIMIZED;
    case ui::SHOW_STATE_MAXIMIZED:
      return PERSISTED_SHOW_STATE_MAXIMIZED;
    case ui::SHOW_STATE_FULLSCREEN:
      return PERSISTED_SHOW_STATE_FULLSCREEN;
    case ui::SHOW_STATE_DOCKED:
      return PERSISTED_SHOW_STATE_DOCKED;

    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_INACTIVE:
      return PERSISTED_SHOW_STATE_NORMAL;

    case ui::SHOW_STATE_END:
      break;
  }
  NOTREACHED();
  return PERSISTED_SHOW_STATE_NORMAL;
}

// Lints show state values when read back from persited disk.
ui::WindowShowState PersistedShowStateToShowState(int state) {
  switch (state) {
    case PERSISTED_SHOW_STATE_NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case PERSISTED_SHOW_STATE_MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case PERSISTED_SHOW_STATE_MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case PERSISTED_SHOW_STATE_FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case PERSISTED_SHOW_STATE_DOCKED:
      return ui::SHOW_STATE_DOCKED;
    case PERSISTED_SHOW_STATE_DETACHED_DEPRECATED:
      return ui::SHOW_STATE_NORMAL;
  }
  NOTREACHED();
  return ui::SHOW_STATE_NORMAL;
}

// Iterates through the vector updating the selected_tab_index of each
// SessionWindow based on the actual tabs that were restored.
void UpdateSelectedTabIndex(std::vector<SessionWindow*>* windows) {
  for (std::vector<SessionWindow*>::const_iterator i = windows->begin();
       i != windows->end(); ++i) {
    // See note in SessionWindow as to why we do this.
    int new_index = 0;
    for (std::vector<SessionTab*>::const_iterator j = (*i)->tabs.begin();
         j != (*i)->tabs.end(); ++j) {
      if ((*j)->tab_visual_index == (*i)->selected_tab_index) {
        new_index = static_cast<int>(j - (*i)->tabs.begin());
        break;
      }
    }
    (*i)->selected_tab_index = new_index;
  }
}

// Returns the window in windows with the specified id. If a window does
// not exist, one is created.
SessionWindow* GetWindow(SessionID::id_type window_id,
                         IdToSessionWindow* windows) {
  std::map<int, SessionWindow*>::iterator i = windows->find(window_id);
  if (i == windows->end()) {
    SessionWindow* window = new SessionWindow();
    window->window_id.set_id(window_id);
    (*windows)[window_id] = window;
    return window;
  }
  return i->second;
}

// Returns the tab with the specified id in tabs. If a tab does not exist,
// it is created.
SessionTab* GetTab(SessionID::id_type tab_id, IdToSessionTab* tabs) {
  DCHECK(tabs);
  std::map<int, SessionTab*>::iterator i = tabs->find(tab_id);
  if (i == tabs->end()) {
    SessionTab* tab = new SessionTab();
    tab->tab_id.set_id(tab_id);
    (*tabs)[tab_id] = tab;
    return tab;
  }
  return i->second;
}

// Returns an iterator into navigations pointing to the navigation whose
// index matches |index|. If no navigation index matches |index|, the first
// navigation with an index > |index| is returned.
//
// This assumes the navigations are ordered by index in ascending order.
std::vector<sessions::SerializedNavigationEntry>::iterator
  FindClosestNavigationWithIndex(
    std::vector<sessions::SerializedNavigationEntry>* navigations,
    int index) {
  DCHECK(navigations);
  for (std::vector<sessions::SerializedNavigationEntry>::iterator
           i = navigations->begin(); i != navigations->end(); ++i) {
    if (i->index() >= index)
      return i;
  }
  return navigations->end();
}

// Function used in sorting windows. Sorting is done based on window id. As
// window ids increment for each new window, this effectively sorts by creation
// time.
static bool WindowOrderSortFunction(const SessionWindow* w1,
                                    const SessionWindow* w2) {
  return w1->window_id.id() < w2->window_id.id();
}

// Compares the two tabs based on visual index.
static bool TabVisualIndexSortFunction(const SessionTab* t1,
                                       const SessionTab* t2) {
  const int delta = t1->tab_visual_index - t2->tab_visual_index;
  return delta == 0 ? (t1->tab_id.id() < t2->tab_id.id()) : (delta < 0);
}

// Does the following:
// . Deletes and removes any windows with no tabs. NOTE: constrained windows
//   that have been dragged out are of type browser. As such, this preserves any
//   dragged out constrained windows (aka popups that have been dragged out).
// . Sorts the tabs in windows with valid tabs based on the tabs
//   visual order, and adds the valid windows to windows.
void SortTabsBasedOnVisualOrderAndPrune(
    std::map<int, SessionWindow*>* windows,
    std::vector<SessionWindow*>* valid_windows) {
  std::map<int, SessionWindow*>::iterator i = windows->begin();
  while (i != windows->end()) {
    SessionWindow* window = i->second;
    if (window->tabs.empty() || window->is_constrained) {
      delete window;
      windows->erase(i++);
    } else {
      // Valid window; sort the tabs and add it to the list of valid windows.
      std::sort(window->tabs.begin(), window->tabs.end(),
                &TabVisualIndexSortFunction);
      // Otherwise, add the window such that older windows appear first.
      if (valid_windows->empty()) {
        valid_windows->push_back(window);
      } else {
        valid_windows->insert(
            std::upper_bound(valid_windows->begin(), valid_windows->end(),
                             window, &WindowOrderSortFunction),
            window);
      }
      ++i;
    }
  }
}

// Adds tabs to their parent window based on the tab's window_id. This
// ignores tabs with no navigations.
void AddTabsToWindows(std::map<int, SessionTab*>* tabs,
                      std::map<int, SessionWindow*>* windows) {
  DVLOG(1) << "AddTabsToWindws";
  DVLOG(1) << "Tabs " << tabs->size() << ", windows " << windows->size();
  std::map<int, SessionTab*>::iterator i = tabs->begin();
  while (i != tabs->end()) {
    SessionTab* tab = i->second;
    if (tab->window_id.id() && !tab->navigations.empty()) {
      SessionWindow* window = GetWindow(tab->window_id.id(), windows);
      window->tabs.push_back(tab);
      tabs->erase(i++);

      // See note in SessionTab as to why we do this.
      std::vector<sessions::SerializedNavigationEntry>::iterator j =
          FindClosestNavigationWithIndex(&(tab->navigations),
                                         tab->current_navigation_index);
      if (j == tab->navigations.end()) {
        tab->current_navigation_index =
            static_cast<int>(tab->navigations.size() - 1);
      } else {
        tab->current_navigation_index =
            static_cast<int>(j - tab->navigations.begin());
      }
    } else {
      // Never got a set tab index in window, or tabs are empty, nothing
      // to do.
      ++i;
    }
  }
}

// Creates tabs and windows from the commands specified in |data|. The created
// tabs and windows are added to |tabs| and |windows| respectively, with the
// id of the active window set in |active_window_id|. It is up to the caller
// to delete the tabs and windows added to |tabs| and |windows|.
//
// This does NOT add any created SessionTabs to SessionWindow.tabs, that is
// done by AddTabsToWindows.
bool CreateTabsAndWindows(const ScopedVector<SessionCommand>& data,
                          std::map<int, SessionTab*>* tabs,
                          std::map<int, SessionWindow*>* windows,
                          SessionID::id_type* active_window_id) {
  // If the file is corrupt (command with wrong size, or unknown command), we
  // still return true and attempt to restore what we we can.
  DVLOG(1) << "CreateTabsAndWindows";

  for (std::vector<SessionCommand*>::const_iterator i = data.begin();
       i != data.end(); ++i) {
    const SessionCommand::id_type kCommandSetWindowBounds2 = 10;
    const SessionCommand* command = *i;

    DVLOG(1) << "Read command " << (int) command->id();
    switch (command->id()) {
      case kCommandSetTabWindow: {
        SessionID::id_type payload[2];
        if (!command->GetPayload(payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload[1], tabs)->window_id.set_id(payload[0]);
        break;
      }

      // This is here for forward migration only.  New data is saved with
      // |kCommandSetWindowBounds3|.
      case kCommandSetWindowBounds2: {
        WindowBoundsPayload2 payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.window_id, windows)->bounds.SetRect(payload.x,
                                                              payload.y,
                                                              payload.w,
                                                              payload.h);
        GetWindow(payload.window_id, windows)->show_state =
            payload.is_maximized ?
                ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;
        break;
      }

      case kCommandSetWindowBounds3: {
        WindowBoundsPayload3 payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.window_id, windows)->bounds.SetRect(payload.x,
                                                              payload.y,
                                                              payload.w,
                                                              payload.h);
        GetWindow(payload.window_id, windows)->show_state =
            PersistedShowStateToShowState(payload.show_state);
        break;
      }

      case kCommandSetTabIndexInWindow: {
        TabIndexInWindowPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload.id, tabs)->tab_visual_index = payload.index;
        break;
      }

      case kCommandTabClosed:
      case kCommandWindowClosed: {
        ClosedPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        if (command->id() == kCommandTabClosed) {
          delete GetTab(payload.id, tabs);
          tabs->erase(payload.id);
        } else {
          delete GetWindow(payload.id, windows);
          windows->erase(payload.id);
        }
        break;
      }

      case kCommandTabNavigationPathPrunedFromBack: {
        TabNavigationPathPrunedFromBackPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        SessionTab* tab = GetTab(payload.id, tabs);
        tab->navigations.erase(
            FindClosestNavigationWithIndex(&(tab->navigations), payload.index),
            tab->navigations.end());
        break;
      }

      case kCommandTabNavigationPathPrunedFromFront: {
        TabNavigationPathPrunedFromFrontPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)) ||
            payload.index <= 0) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        SessionTab* tab = GetTab(payload.id, tabs);

        // Update the selected navigation index.
        tab->current_navigation_index =
            std::max(-1, tab->current_navigation_index - payload.index);

        // And update the index of existing navigations.
        for (std::vector<sessions::SerializedNavigationEntry>::iterator
                 i = tab->navigations.begin();
             i != tab->navigations.end();) {
          i->set_index(i->index() - payload.index);
          if (i->index() < 0)
            i = tab->navigations.erase(i);
          else
            ++i;
        }
        break;
      }

      case kCommandUpdateTabNavigation: {
        sessions::SerializedNavigationEntry navigation;
        SessionID::id_type tab_id;
        if (!RestoreUpdateTabNavigationCommand(*command,
                                               &navigation,
                                               &tab_id)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        SessionTab* tab = GetTab(tab_id, tabs);
        std::vector<sessions::SerializedNavigationEntry>::iterator i =
            FindClosestNavigationWithIndex(&(tab->navigations),
                                           navigation.index());
        if (i != tab->navigations.end() && i->index() == navigation.index())
          *i = navigation;
        else
          tab->navigations.insert(i, navigation);
        break;
      }

      case kCommandSetSelectedNavigationIndex: {
        SelectedNavigationIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload.id, tabs)->current_navigation_index = payload.index;
        break;
      }

      case kCommandSetSelectedTabInIndex: {
        SelectedTabInIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.id, windows)->selected_tab_index = payload.index;
        break;
      }

      case kCommandSetWindowType: {
        WindowTypePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(payload.id, windows)->is_constrained = false;
        GetWindow(payload.id, windows)->type =
            static_cast<SessionWindow::WindowType>(payload.index);
        break;
      }

      case kCommandSetPinnedState: {
        PinnedStatePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetTab(payload.tab_id, tabs)->pinned = payload.pinned_state;
        break;
      }

      case kCommandSetWindowAppName: {
        SessionID::id_type window_id;
        std::string app_name;
        if (!RestoreSetWindowAppNameCommand(*command, &window_id, &app_name))
          return true;

        GetWindow(window_id, windows)->app_name.swap(app_name);
        break;
      }

      case kCommandSetExtensionAppID: {
        SessionID::id_type tab_id;
        std::string extension_app_id;
        if (!RestoreSetTabExtensionAppIDCommand(*command,
                                                &tab_id,
                                                &extension_app_id)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }

        GetTab(tab_id, tabs)->extension_app_id.swap(extension_app_id);
        break;
      }

      case kCommandSetTabUserAgentOverride: {
        SessionID::id_type tab_id;
        std::string user_agent_override;
        if (!RestoreSetTabUserAgentOverrideCommand(
                *command,
                &tab_id,
                &user_agent_override)) {
          return true;
        }

        GetTab(tab_id, tabs)->user_agent_override.swap(user_agent_override);
        break;
      }

      case kCommandSessionStorageAssociated: {
        std::unique_ptr<base::Pickle> command_pickle(
            command->PayloadAsPickle());
        SessionID::id_type command_tab_id;
        std::string session_storage_persistent_id;
        base::PickleIterator iter(*command_pickle.get());
        if (!iter.ReadInt(&command_tab_id) ||
            !iter.ReadString(&session_storage_persistent_id))
          return true;
        // Associate the session storage back.
        GetTab(command_tab_id, tabs)->session_storage_persistent_id =
            session_storage_persistent_id;
        break;
      }

      case kCommandSetActiveWindow: {
        ActiveWindowPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        *active_window_id = payload;
        break;
      }

      case kCommandLastActiveTime: {
        LastActiveTimePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        SessionTab* tab = GetTab(payload.tab_id, tabs);
        tab->last_active_time =
            base::TimeTicks::FromInternalValue(payload.last_active_time);
        break;
      }

      case kCommandSetWindowWorkspace2: {
        std::unique_ptr<base::Pickle> pickle(command->PayloadAsPickle());
        base::PickleIterator it(*pickle);
        SessionID::id_type window_id;
        std::string workspace;
         if (!it.ReadInt(&window_id) || !it.ReadString(&workspace)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return true;
        }
        GetWindow(window_id, windows)->workspace = workspace;
        break;
      }

      default:
        // TODO(skuhne): This might call back into a callback handler to extend
        // the command set for specific implementations.
        DVLOG(1) << "Failed reading an unknown command " << command->id();
        return true;
    }
  }
  return true;
}

}  // namespace

std::unique_ptr<SessionCommand> CreateSetSelectedTabInWindowCommand(
    const SessionID& window_id,
    int index) {
  SelectedTabInIndexPayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = index;
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetSelectedTabInIndex, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateSetTabWindowCommand(
    const SessionID& window_id,
    const SessionID& tab_id) {
  SessionID::id_type payload[] = { window_id.id(), tab_id.id() };
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetTabWindow, sizeof(payload)));
  memcpy(command->contents(), payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateSetWindowBoundsCommand(
    const SessionID& window_id,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state) {
  WindowBoundsPayload3 payload = { 0 };
  payload.window_id = window_id.id();
  payload.x = bounds.x();
  payload.y = bounds.y();
  payload.w = bounds.width();
  payload.h = bounds.height();
  payload.show_state = ShowStateToPersistedShowState(show_state);
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetWindowBounds3, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateSetTabIndexInWindowCommand(
    const SessionID& tab_id,
    int new_index) {
  TabIndexInWindowPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = new_index;
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetTabIndexInWindow, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateTabClosedCommand(
    const SessionID::id_type tab_id) {
  ClosedPayload payload;
  // Because of what appears to be a compiler bug setting payload to {0} doesn't
  // set the padding to 0, resulting in Purify reporting an UMR when we write
  // the structure to disk. To avoid this we explicitly memset the struct.
  memset(&payload, 0, sizeof(payload));
  payload.id = tab_id;
  payload.close_time = base::Time::Now().ToInternalValue();
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandTabClosed, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateWindowClosedCommand(
    const SessionID::id_type window_id) {
  ClosedPayload payload;
  // See comment in CreateTabClosedCommand as to why we do this.
  memset(&payload, 0, sizeof(payload));
  payload.id = window_id;
  payload.close_time = base::Time::Now().ToInternalValue();
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandWindowClosed, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateSetSelectedNavigationIndexCommand(
    const SessionID& tab_id,
    int index) {
  SelectedNavigationIndexPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = index;
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetSelectedNavigationIndex, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateSetWindowTypeCommand(
    const SessionID& window_id,
    SessionWindow::WindowType type) {
  WindowTypePayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = static_cast<int32_t>(type);
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetWindowType, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreatePinnedStateCommand(
    const SessionID& tab_id,
    bool is_pinned) {
  PinnedStatePayload payload = { 0 };
  payload.tab_id = tab_id.id();
  payload.pinned_state = is_pinned;
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetPinnedState, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateSessionStorageAssociatedCommand(
    const SessionID& tab_id,
    const std::string& session_storage_persistent_id) {
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());
  pickle.WriteString(session_storage_persistent_id);
  return std::unique_ptr<SessionCommand>(
      new SessionCommand(kCommandSessionStorageAssociated, pickle));
}

std::unique_ptr<SessionCommand> CreateSetActiveWindowCommand(
    const SessionID& window_id) {
  ActiveWindowPayload payload = 0;
  payload = window_id.id();
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetActiveWindow, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateLastActiveTimeCommand(
    const SessionID& tab_id,
    base::TimeTicks last_active_time) {
  LastActiveTimePayload payload = {0};
  payload.tab_id = tab_id.id();
  payload.last_active_time = last_active_time.ToInternalValue();
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandLastActiveTime, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateSetWindowWorkspaceCommand(
    const SessionID& window_id,
    const std::string& workspace) {
  base::Pickle pickle;
  pickle.WriteInt(window_id.id());
  pickle.WriteString(workspace);
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSetWindowWorkspace2, pickle));
  return command;
}

std::unique_ptr<SessionCommand> CreateTabNavigationPathPrunedFromBackCommand(
    const SessionID& tab_id,
    int count) {
  TabNavigationPathPrunedFromBackPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = count;
  std::unique_ptr<SessionCommand> command(new SessionCommand(
      kCommandTabNavigationPathPrunedFromBack, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateTabNavigationPathPrunedFromFrontCommand(
    const SessionID& tab_id,
    int count) {
  TabNavigationPathPrunedFromFrontPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = count;
  std::unique_ptr<SessionCommand> command(new SessionCommand(
      kCommandTabNavigationPathPrunedFromFront, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

std::unique_ptr<SessionCommand> CreateUpdateTabNavigationCommand(
    const SessionID& tab_id,
    const sessions::SerializedNavigationEntry& navigation) {
  return CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation,
                                            tab_id.id(),
                                            navigation);
}

std::unique_ptr<SessionCommand> CreateSetTabExtensionAppIDCommand(
    const SessionID& tab_id,
    const std::string& extension_id) {
  return CreateSetTabExtensionAppIDCommand(kCommandSetExtensionAppID,
                                             tab_id.id(),
                                             extension_id);
}

std::unique_ptr<SessionCommand> CreateSetTabUserAgentOverrideCommand(
    const SessionID& tab_id,
    const std::string& user_agent_override) {
  return CreateSetTabUserAgentOverrideCommand(kCommandSetTabUserAgentOverride,
                                                tab_id.id(),
                                                user_agent_override);
}

std::unique_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    const SessionID& window_id,
    const std::string& app_name) {
  return CreateSetWindowAppNameCommand(kCommandSetWindowAppName,
                                         window_id.id(),
                                         app_name);
}

bool ReplacePendingCommand(BaseSessionService* base_session_service,
                           std::unique_ptr<SessionCommand>* command) {
  // We optimize page navigations, which can happen quite frequently and
  // is expensive. And activation is like Highlander, there can only be one!
  if ((*command)->id() != kCommandUpdateTabNavigation &&
      (*command)->id() != kCommandSetActiveWindow) {
    return false;
  }
  for (ScopedVector<SessionCommand>::const_reverse_iterator i =
           base_session_service->pending_commands().rbegin();
       i != base_session_service->pending_commands().rend(); ++i) {
    SessionCommand* existing_command = *i;
    if ((*command)->id() == kCommandUpdateTabNavigation &&
        existing_command->id() == kCommandUpdateTabNavigation) {
      std::unique_ptr<base::Pickle> command_pickle(
          (*command)->PayloadAsPickle());
      base::PickleIterator iterator(*command_pickle);
      SessionID::id_type command_tab_id;
      int command_nav_index;
      if (!iterator.ReadInt(&command_tab_id) ||
          !iterator.ReadInt(&command_nav_index)) {
        return false;
      }
      SessionID::id_type existing_tab_id;
      int existing_nav_index;
      {
        // Creating a pickle like this means the Pickle references the data from
        // the command. Make sure we delete the pickle before the command, else
        // the pickle references deleted memory.
        std::unique_ptr<base::Pickle> existing_pickle(
            existing_command->PayloadAsPickle());
        iterator = base::PickleIterator(*existing_pickle);
        if (!iterator.ReadInt(&existing_tab_id) ||
            !iterator.ReadInt(&existing_nav_index)) {
          return false;
        }
      }
      if (existing_tab_id == command_tab_id &&
          existing_nav_index == command_nav_index) {
        // existing_command is an update for the same tab/index pair. Replace
        // it with the new one. We need to add to the end of the list just in
        // case there is a prune command after the update command.
        base_session_service->EraseCommand(*(i.base() - 1));
        base_session_service->AppendRebuildCommand((std::move(*command)));
        return true;
      }
      return false;
    }
    if ((*command)->id() == kCommandSetActiveWindow &&
        existing_command->id() == kCommandSetActiveWindow) {
      base_session_service->SwapCommand(existing_command,
                                        (std::move(*command)));
      return true;
    }
  }
  return false;
}

bool IsClosingCommand(SessionCommand* command) {
  return command->id() == kCommandTabClosed ||
         command->id() == kCommandWindowClosed;
}

void RestoreSessionFromCommands(const ScopedVector<SessionCommand>& commands,
                                std::vector<SessionWindow*>* valid_windows,
                                SessionID::id_type* active_window_id) {
  std::map<int, SessionTab*> tabs;
  std::map<int, SessionWindow*> windows;

  DVLOG(1) << "RestoreSessionFromCommands " << commands.size();
  if (CreateTabsAndWindows(commands, &tabs, &windows, active_window_id)) {
    AddTabsToWindows(&tabs, &windows);
    SortTabsBasedOnVisualOrderAndPrune(&windows, valid_windows);
    UpdateSelectedTabIndex(valid_windows);
  }
  STLDeleteValues(&tabs);
  // Don't delete contents of windows, that is done by the caller as all
  // valid windows are added to valid_windows.
}

}  // namespace sessions

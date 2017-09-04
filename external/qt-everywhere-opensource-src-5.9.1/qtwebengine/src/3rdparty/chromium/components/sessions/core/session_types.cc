// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_types.h"

#include <stddef.h>

#include "components/sessions/core/session_command.h"

namespace sessions {

//using class SerializedNavigationEntry;

// SessionTab -----------------------------------------------------------------

SessionTab::SessionTab()
    : tab_visual_index(-1),
      current_navigation_index(-1),
      pinned(false) {
}

SessionTab::~SessionTab() {
}

void SessionTab::SetFromSyncData(const sync_pb::SessionTab& sync_data,
                                 base::Time timestamp) {
  window_id.set_id(sync_data.window_id());
  tab_id.set_id(sync_data.tab_id());
  tab_visual_index = sync_data.tab_visual_index();
  current_navigation_index = sync_data.current_navigation_index();
  pinned = sync_data.pinned();
  extension_app_id = sync_data.extension_app_id();
  user_agent_override.clear();
  this->timestamp = timestamp;
  navigations.clear();
  for (int i = 0; i < sync_data.navigation_size(); ++i) {
    navigations.push_back(
        SerializedNavigationEntry::FromSyncData(i, sync_data.navigation(i)));
  }
  session_storage_persistent_id.clear();
  variation_ids.clear();
  for (int i = 0; i < sync_data.variation_id_size(); ++i)
    variation_ids.push_back(sync_data.variation_id(i));
}

sync_pb::SessionTab SessionTab::ToSyncData() const {
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_id(tab_id.id());
  sync_data.set_window_id(window_id.id());
  sync_data.set_tab_visual_index(tab_visual_index);
  sync_data.set_current_navigation_index(current_navigation_index);
  sync_data.set_pinned(pinned);
  sync_data.set_extension_app_id(extension_app_id);
  for (const SerializedNavigationEntry& navigation : navigations) {
    *sync_data.add_navigation() = navigation.ToSyncData();
  }
  for (const variations::VariationID variation_id : variation_ids) {
    sync_data.add_variation_id(variation_id);
  }
  return sync_data;
}

// SessionWindow ---------------------------------------------------------------

SessionWindow::SessionWindow()
    : selected_tab_index(-1),
      type(TYPE_TABBED),
      is_constrained(true),
      show_state(ui::SHOW_STATE_DEFAULT) {}

SessionWindow::~SessionWindow() {}

sync_pb::SessionWindow SessionWindow::ToSyncData() const {
  sync_pb::SessionWindow sync_data;
  sync_data.set_window_id(window_id.id());
  sync_data.set_selected_tab_index(selected_tab_index);
  switch (type) {
    case SessionWindow::TYPE_TABBED:
      sync_data.set_browser_type(
          sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
      break;
    case SessionWindow::TYPE_POPUP:
      sync_data.set_browser_type(
        sync_pb::SessionWindow_BrowserType_TYPE_POPUP);
      break;
    default:
      NOTREACHED() << "Unhandled browser type.";
  }

  for (const auto& tab : tabs)
    sync_data.add_tab(tab->tab_id.id());

  return sync_data;
}

}  // namespace sessions

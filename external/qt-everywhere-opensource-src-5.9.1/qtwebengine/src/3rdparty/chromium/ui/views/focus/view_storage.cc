// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/view_storage.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace views {

// static
ViewStorage* ViewStorage::GetInstance() {
  return base::Singleton<ViewStorage>::get();
}

ViewStorage::ViewStorage() : view_storage_next_id_(0) {
}

ViewStorage::~ViewStorage() {}

int ViewStorage::CreateStorageID() {
  return view_storage_next_id_++;
}

void ViewStorage::StoreView(int storage_id, View* view) {
  DCHECK(view);

  if (id_to_view_.find(storage_id) != id_to_view_.end()) {
    NOTREACHED();
    RemoveView(storage_id);
  }

  id_to_view_[storage_id] = view;
  view_to_ids_[view].push_back(storage_id);
}

View* ViewStorage::RetrieveView(int storage_id) {
  auto iter = id_to_view_.find(storage_id);
  if (iter == id_to_view_.end())
    return nullptr;
  return iter->second;
}

void ViewStorage::RemoveView(int storage_id) {
  EraseView(storage_id, false);
}

void ViewStorage::ViewRemoved(View* removed) {
  // Let's first retrieve the ids for that view.
  auto ids_iter = view_to_ids_.find(removed);

  if (ids_iter == view_to_ids_.end()) {
    // That view is not in the view storage.
    return;
  }

  const std::vector<int>& ids = ids_iter->second;
  DCHECK(!ids.empty());
  EraseView(ids[0], true);
}

void ViewStorage::EraseView(int storage_id, bool remove_all_ids) {
  // Remove the view from id_to_view_location_.
  auto view_iter = id_to_view_.find(storage_id);
  if (view_iter == id_to_view_.end())
    return;

  View* view = view_iter->second;
  id_to_view_.erase(view_iter);

  // Also update view_to_ids_.
  auto ids_iter = view_to_ids_.find(view);
  DCHECK(ids_iter != view_to_ids_.end());
  std::vector<int>& ids = ids_iter->second;

  if (remove_all_ids) {
    for (int id : ids)
      id_to_view_.erase(id);
    view_to_ids_.erase(ids_iter);
  } else if (ids.size() == 1) {
    view_to_ids_.erase(ids_iter);
  } else {
    auto id_iter = std::find(ids.begin(), ids.end(), storage_id);
    DCHECK(id_iter != ids.end());
    ids.erase(id_iter);
  }
}

}  // namespace views

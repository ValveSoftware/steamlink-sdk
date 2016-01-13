// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_model.h"

#include "base/logging.h"
#include "ui/views/view.h"

namespace views {

ViewModel::ViewModel() {
}

ViewModel::~ViewModel() {
  // view are owned by their parent, no need to delete them.
}

void ViewModel::Add(View* view, int index) {
  DCHECK_LE(index, static_cast<int>(entries_.size()));
  DCHECK_GE(index, 0);
  Entry entry;
  entry.view = view;
  entries_.insert(entries_.begin() + index, entry);
}

void ViewModel::Remove(int index) {
  if (index == -1)
    return;

  check_index(index);
  entries_.erase(entries_.begin() + index);
}

void ViewModel::Move(int index, int target_index) {
  DCHECK_LT(index, static_cast<int>(entries_.size()));
  DCHECK_GE(index, 0);
  DCHECK_LT(target_index, static_cast<int>(entries_.size()));
  DCHECK_GE(target_index, 0);

  if (index == target_index)
    return;
  Entry entry(entries_[index]);
  entries_.erase(entries_.begin() + index);
  entries_.insert(entries_.begin() + target_index, entry);
}

void ViewModel::MoveViewOnly(int index, int target_index) {
  if (index == target_index)
    return;
  if (target_index < index) {
    View* view = entries_[index].view;
    for (int i = index; i > target_index; --i)
      entries_[i].view = entries_[i - 1].view;
    entries_[target_index].view = view;
  } else {
    View* view = entries_[index].view;
    for (int i = index; i < target_index; ++i)
      entries_[i].view = entries_[i + 1].view;
    entries_[target_index].view = view;
  }
}

void ViewModel::Clear() {
  Entries entries;
  entries.swap(entries_);
  for (size_t i = 0; i < entries.size(); ++i)
    delete entries[i].view;
}

int ViewModel::GetIndexOfView(const View* view) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].view == view)
      return static_cast<int>(i);
  }
  return -1;
}

}  // namespace views

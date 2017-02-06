// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_VIEW_STORAGE_H_
#define UI_VIEWS_FOCUS_VIEW_STORAGE_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

// This class is a simple storage place for storing/retrieving views.  It is
// used for example in the FocusManager to store/restore focused views when the
// main window becomes active/inactive.
// It automatically removes a view from the storage if the view is removed from
// the tree hierarchy or when the view is destroyed, which ever comes first.
//
// To use it, you first need to create a view storage id that can then be used
// to store/retrieve views.

namespace views {
class View;

class VIEWS_EXPORT ViewStorage {
 public:
  // Returns the global ViewStorage instance.
  // It is guaranted to be non NULL.
  static ViewStorage* GetInstance();

  // Returns a unique storage id that can be used to store/retrieve views.
  int CreateStorageID();

  // Associates |view| with the specified |storage_id|.
  void StoreView(int storage_id, View* view);

  // Returns the view associated with |storage_id| if any, NULL otherwise.
  View* RetrieveView(int storage_id);

  // Removes the view associated with |storage_id| if any.
  void RemoveView(int storage_id);

  // Notifies the ViewStorage that a view was removed from its parent somewhere.
  void ViewRemoved(View* removed);

  size_t view_count() const { return view_to_ids_.size(); }

 private:
  friend struct base::DefaultSingletonTraits<ViewStorage>;

  ViewStorage();
  ~ViewStorage();

  // Removes the view associated with |storage_id|. If |remove_all_ids| is true,
  // all other mapping pointing to the same view are removed as well.
  void EraseView(int storage_id, bool remove_all_ids);

  // Next id for the view storage.
  int view_storage_next_id_;

  // The association id to View used for the view storage.
  std::map<int, View*> id_to_view_;

  // Association View to id, used to speed up view notification removal.
  std::map<View*, std::vector<int>*> view_to_ids_;

  DISALLOW_COPY_AND_ASSIGN(ViewStorage);
};

}  // namespace views

#endif  // UI_VIEWS_FOCUS_VIEW_STORAGE_H_

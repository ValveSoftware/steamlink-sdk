// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_MODEL_H_
#define UI_VIEWS_VIEW_MODEL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/gfx/rect.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// ViewModel is used to track an 'interesting' set of a views. Often times
// during animations views are removed after a delay, which makes for tricky
// coordinate conversion as you have to account for the possibility of the
// indices from the model not lining up with those you expect. This class lets
// you define the 'interesting' views and operate on those views.
class VIEWS_EXPORT ViewModel {
 public:
  ViewModel();
  ~ViewModel();

  // Adds |view| to this model. This does not add |view| to a view hierarchy,
  // only to this model.
  void Add(View* view, int index);

  // Removes the view at the specified index. This does not actually remove the
  // view from the view hierarchy.
  void Remove(int index);

  // Moves the view at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the view at |index| is removed.
  void Move(int index, int target_index);

  // Variant of Move() that leaves the bounds as is. That is, after invoking
  // this the bounds of the view at |target_index| (and all other indices) are
  // exactly the same as the bounds of the view at |target_index| before
  // invoking this.
  void MoveViewOnly(int index, int target_index);

  // Returns the number of views.
  int view_size() const { return static_cast<int>(entries_.size()); }

  // Removes and deletes all the views.
  void Clear();

  // Returns the view at the specified index.
  View* view_at(int index) const {
    check_index(index);
    return entries_[index].view;
  }

  void set_ideal_bounds(int index, const gfx::Rect& bounds) {
    check_index(index);
    entries_[index].ideal_bounds = bounds;
  }

  const gfx::Rect& ideal_bounds(int index) const {
    check_index(index);
    return entries_[index].ideal_bounds;
  }

  // Returns the index of the specified view, or -1 if the view isn't in the
  // model.
  int GetIndexOfView(const View* view) const;

 private:
  struct Entry {
    Entry() : view(NULL) {}

    View* view;
    gfx::Rect ideal_bounds;
  };
  typedef std::vector<Entry> Entries;

#if !defined(NDEBUG)
  void check_index(int index) const {
    DCHECK_LT(index, static_cast<int>(entries_.size()));
    DCHECK_GE(index, 0);
  }
#else
  void check_index(int index) const {}
#endif

  Entries entries_;

  DISALLOW_COPY_AND_ASSIGN(ViewModel);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_MODEL_H_

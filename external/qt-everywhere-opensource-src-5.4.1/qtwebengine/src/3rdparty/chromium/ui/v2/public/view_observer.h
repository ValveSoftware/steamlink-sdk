// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_PUBLIC_VIEW_OBSERVER_H_
#define UI_V2_PUBLIC_VIEW_OBSERVER_H_

#include "ui/v2/public/v2_export.h"

namespace gfx {
class Rect;
}

namespace v2 {

class View;

// Observe View disposition changes. -ing methods are called before the change
// is committed, -ed methods are called after.
class V2_EXPORT ViewObserver {
 public:
  // Whether a notification is being sent before or after some property has
  // changed.
  enum DispositionChangePhase {
    DISPOSITION_CHANGING,
    DISPOSITION_CHANGED
  };

  // Tree.
  struct TreeChangeParams {
    TreeChangeParams();
    View* target;
    View* old_parent;
    View* new_parent;
    View* receiver;
    DispositionChangePhase phase;
  };

  // Called when a node is added or removed. Notifications are sent to the
  // following hierarchies in this order:
  // 1. |target|.
  // 2. |target|'s child hierarchy.
  // 3. |target|'s parent hierarchy in its |old_parent|
  //        (only for Changing notifications).
  // 3. |target|'s parent hierarchy in its |new_parent|.
  //        (only for Changed notifications).
  // This sequence is performed via the OnTreeChange notification below before
  // and after the change is committed.
  virtual void OnViewTreeChange(const TreeChangeParams& params) {}

  // Disposition.
  virtual void OnViewDestroy(View* view, DispositionChangePhase phase) {}
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) {}
  virtual void OnViewVisibilityChange(View* view,
                                      DispositionChangePhase phase) {}

 protected:
  virtual ~ViewObserver() {}
};

}  // namespace v2

#endif  // UI_V2_PUBLIC_VIEW_OBSERVER_H_

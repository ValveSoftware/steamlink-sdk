// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_SRC_VIEW_PRIVATE_H_
#define UI_V2_SRC_VIEW_PRIVATE_H_

#include "base/observer_list.h"
#include "ui/v2/public/view.h"

namespace gfx {
class Rect;
}

namespace v2 {

class ViewObserver;

// Friend of View. Provides a way to access view state for the implementation
// of class View.
class ViewPrivate {
 public:
  explicit ViewPrivate(View* view);
  ~ViewPrivate();

  ObserverList<ViewObserver>* observers() { return &view_->observers_; }

  void ClearParent() { view_->parent_ = NULL; }

  void set_bounds(const gfx::Rect& bounds) { view_->bounds_ = bounds; }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewPrivate);
};

}  // namespace v2

#endif  // UI_V2_SRC_VIEW_PRIVATE_H_

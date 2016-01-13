// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MASKED_VIEW_TARGETER_H_
#define UI_VIEWS_MASKED_VIEW_TARGETER_H_

#include "ui/views/view_targeter.h"
#include "ui/views/views_export.h"

namespace gfx {
class Path;
}

namespace views {

// Derived classes of MaskedViewTargeter are used to define custom-shaped
// hit test regions for a View used in event targeting.
// TODO(tdanderson|sadrul): Some refactoring opportunities may be possible
//                          between this class and MaskedWindowTargeter.
class VIEWS_EXPORT MaskedViewTargeter : public ViewTargeter {
 public:
  explicit MaskedViewTargeter(View* masked_view);
  virtual ~MaskedViewTargeter();

  // Sets the hit-test mask for |view| in |mask| (in |view|'s local
  // coordinate system). Returns whether a valid mask has been set in |mask|.
  virtual bool GetHitTestMask(const View* view, gfx::Path* mask) const = 0;

 protected:
  const View* masked_view() const { return masked_view_; }

  // ui::EventTargeter:
  virtual bool EventLocationInsideBounds(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) const OVERRIDE;

 private:
  View* masked_view_;

  DISALLOW_COPY_AND_ASSIGN(MaskedViewTargeter);
};

}  // namespace views

#endif  // UI_VIEWS_MASKED_VIEW_TARGETER_H_

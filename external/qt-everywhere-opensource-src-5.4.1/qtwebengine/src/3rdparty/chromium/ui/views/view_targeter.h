// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TARGETER_H_
#define UI_VIEWS_VIEW_TARGETER_H_

#include "ui/events/event_targeter.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Contains the logic used to determine the View to which an
// event should be dispatched. A ViewTargeter (or one of its
// derived classes) is installed on a View to specify the
// targeting behaviour to be used for the subtree rooted at
// that View.
class VIEWS_EXPORT ViewTargeter : public ui::EventTargeter {
 public:
  ViewTargeter();
  virtual ~ViewTargeter();

 protected:
  // Returns the location of |event| represented as a rect. If |event| is
  // a gesture event, its bounding box is returned. Otherwise, a 1x1 rect
  // having its origin at the location of |event| is returned.
  gfx::RectF BoundsForEvent(const ui::LocatedEvent& event) const;

  // ui::EventTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE;
  virtual ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                              ui::Event* event) OVERRIDE;
  virtual bool SubtreeCanAcceptEvent(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) const OVERRIDE;
  virtual bool EventLocationInsideBounds(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) const OVERRIDE;

 private:
  View* FindTargetForKeyEvent(View* view, const ui::KeyEvent& key);

  DISALLOW_COPY_AND_ASSIGN(ViewTargeter);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TARGETER_H_

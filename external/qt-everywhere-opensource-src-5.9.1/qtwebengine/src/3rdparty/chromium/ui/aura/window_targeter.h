// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TARGETER_H_
#define UI_AURA_WINDOW_TARGETER_H_

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/events/event_targeter.h"

namespace ui {
class KeyEvent;
class LocatedEvent;
}  // namespace ui

namespace aura {

class Window;

class AURA_EXPORT WindowTargeter : public ui::EventTargeter {
 public:
  WindowTargeter();
  ~WindowTargeter() override;

  // Returns true if |window| or one of its descendants can be a target of
  // |event|. This requires that |window| and its descendants are not
  // prohibited from accepting the event, and that the event is within an
  // actionable region of the target's bounds. Note that the location etc. of
  // |event| is in |window|'s parent's coordinate system.
  virtual bool SubtreeShouldBeExploredForEvent(Window* window,
                                               const ui::LocatedEvent& event);

 protected:
  // Same as FindTargetForEvent(), but used for positional events. The location
  // etc. of |event| are in |root|'s coordinate system. When finding the target
  // for the event, the targeter can mutate the |event| (e.g. change the
  // coordinate to be in the returned target's coordinate system) so that it can
  // be dispatched to the target without any further modification.
  virtual Window* FindTargetForLocatedEvent(Window* window,
                                            ui::LocatedEvent* event);

  // Returns false if neither |window| nor any of its descendants are allowed
  // to accept |event| for reasons unrelated to the event's location or the
  // target's bounds. For example, overrides of this function may consider
  // attributes such as the visibility or enabledness of |window|. Note that
  // the location etc. of |event| is in |window|'s parent's coordinate system.
  virtual bool SubtreeCanAcceptEvent(Window* window,
                                     const ui::LocatedEvent& event) const;

  // Returns whether the location of the event is in an actionable region of the
  // target. Note that the location etc. of |event| is in the |window|'s
  // parent's coordinate system.
  virtual bool EventLocationInsideBounds(Window* target,
                                         const ui::LocatedEvent& event) const;

  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override;

 private:
  Window* FindTargetForKeyEvent(Window* root_window, const ui::KeyEvent& event);
  Window* FindTargetForNonKeyEvent(Window* root_window, ui::Event* event);
  Window* FindTargetInRootWindow(Window* root_window,
                                 const ui::LocatedEvent& event);
  Window* FindTargetForLocatedEventRecursively(Window* root_window,
                                               ui::LocatedEvent* event);

  DISALLOW_COPY_AND_ASSIGN(WindowTargeter);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TARGETER_H_

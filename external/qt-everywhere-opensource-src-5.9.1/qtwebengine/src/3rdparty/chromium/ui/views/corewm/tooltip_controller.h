// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_
#define UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/tooltip_client.h"

namespace aura {
class Window;
}

namespace views {
namespace corewm {

class Tooltip;

namespace test {
class TooltipControllerTestHelper;
}  // namespace test

// TooltipController provides tooltip functionality for aura.
class VIEWS_EXPORT TooltipController : public aura::client::TooltipClient,
                                       public ui::EventHandler,
                                       public aura::WindowObserver {
 public:
  explicit TooltipController(std::unique_ptr<Tooltip> tooltip);
  ~TooltipController() override;

  // Overridden from aura::client::TooltipClient.
  int GetMaxWidth(const gfx::Point& location) const override;
  void UpdateTooltip(aura::Window* target) override;
  void SetTooltipShownTimeout(aura::Window* target, int timeout_in_ms) override;
  void SetTooltipsEnabled(bool enable) override;

  // Overridden from ui::EventHandler.
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnCancelMode(ui::CancelModeEvent* event) override;

  // Overridden from aura::WindowObserver.
  void OnWindowDestroyed(aura::Window* window) override;

  const gfx::Point& mouse_location() const { return curr_mouse_loc_; }

 private:
  friend class test::TooltipControllerTestHelper;

  void TooltipTimerFired();
  void TooltipShownTimerFired();

  // Updates the tooltip if required (if there is any change in the tooltip
  // text, tooltip id or the aura::Window).
  void UpdateIfRequired();

  // Only used in tests.
  bool IsTooltipVisible();

  bool IsDragDropInProgress();

  // Returns true if the cursor is visible.
  bool IsCursorVisible();

  int GetTooltipShownTimeout();

  // Sets tooltip window to |target| if it is different from existing window.
  // Calls RemoveObserver on the existing window if it is not NULL.
  // Calls AddObserver on the new window if it is not NULL.
  void SetTooltipWindow(aura::Window* target);

  aura::Window* tooltip_window_;
  base::string16 tooltip_text_;
  const void* tooltip_id_;

  // These fields are for tracking state when the user presses a mouse button.
  aura::Window* tooltip_window_at_mouse_press_;
  base::string16 tooltip_text_at_mouse_press_;

  std::unique_ptr<Tooltip> tooltip_;

  base::RepeatingTimer tooltip_timer_;

  // Timer to timeout the life of an on-screen tooltip. We hide the tooltip when
  // this timer fires.
  base::OneShotTimer tooltip_shown_timer_;

  // Location of the last event in |tooltip_window_|'s coordinates.
  gfx::Point curr_mouse_loc_;

  bool tooltips_enabled_;

  std::map<aura::Window*, int> tooltip_shown_timeout_map_;

  DISALLOW_COPY_AND_ASSIGN(TooltipController);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_CONTROLLER_H_

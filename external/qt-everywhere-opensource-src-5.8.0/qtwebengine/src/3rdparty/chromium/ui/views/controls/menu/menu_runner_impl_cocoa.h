// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_COCOA_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_COCOA_H_

#include "ui/views/controls/menu/menu_runner_impl_interface.h"

#include <stdint.h>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/time/time.h"

@class MenuController;

namespace views {
namespace internal {

// A menu runner implementation that uses NSMenu to show a context menu.
class VIEWS_EXPORT MenuRunnerImplCocoa : public MenuRunnerImplInterface {
 public:
  explicit MenuRunnerImplCocoa(ui::MenuModel* menu);

  bool IsRunning() const override;
  void Release() override;
  MenuRunner::RunResult RunMenuAt(Widget* parent,
                                  MenuButton* button,
                                  const gfx::Rect& bounds,
                                  MenuAnchorPosition anchor,
                                  int32_t run_types) override;
  void Cancel() override;
  base::TimeTicks GetClosingEventTime() const override;

 private:
  ~MenuRunnerImplCocoa() override;

  // The Cocoa menu controller that this instance is bridging.
  base::scoped_nsobject<MenuController> menu_controller_;

  // Are we in run waiting for it to return?
  bool running_;

  // Set if |running_| and Release() has been invoked.
  bool delete_after_run_;

  // The timestamp of the event which closed the menu - or 0.
  base::TimeTicks closing_event_time_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerImplCocoa);
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_COCOA_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_KEY_EVENT_HANDLER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_KEY_EVENT_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace ui {
class Accelerator;
}

namespace views {

// Handles key events while the menu is open.
class VIEWS_EXPORT MenuKeyEventHandler : public ui::EventHandler {
 public:
  MenuKeyEventHandler();
  ~MenuKeyEventHandler() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuKeyEventHandler);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_KEY_EVENT_HANDLER_H_

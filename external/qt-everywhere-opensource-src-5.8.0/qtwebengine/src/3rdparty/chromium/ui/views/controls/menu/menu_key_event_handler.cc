// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_key_event_handler.h"

#include "ui/aura/env.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/views_delegate.h"

namespace views {

MenuKeyEventHandler::MenuKeyEventHandler() {
  aura::Env::GetInstanceDontCreate()->PrependPreTargetHandler(this);
}

MenuKeyEventHandler::~MenuKeyEventHandler() {
  aura::Env::GetInstanceDontCreate()->RemovePreTargetHandler(this);
}

void MenuKeyEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  MenuController* menu_controller = MenuController::GetActiveInstance();
  DCHECK(menu_controller);
  menu_controller->OnWillDispatchKeyEvent(event);
}

}  // namespace views

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_loop_mac.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/gfx/geometry/point.h"

namespace views {

// static
MenuMessageLoop* MenuMessageLoop::Create() {
  return new MenuMessageLoopMac;
}

MenuMessageLoopMac::MenuMessageLoopMac() {
}

MenuMessageLoopMac::~MenuMessageLoopMac() {
}

void MenuMessageLoopMac::RepostEventToWindow(const ui::LocatedEvent& event,
                                             gfx::NativeWindow window,
                                             const gfx::Point& screen_loc) {
  NOTIMPLEMENTED();
}

void MenuMessageLoopMac::Run(MenuController* controller,
                             Widget* owner,
                             bool nested_menu) {
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow(loop);
  base::RunLoop run_loop;
  run_loop.Run();
}

bool MenuMessageLoopMac::ShouldQuitNow() const {
  return true;
}

void MenuMessageLoopMac::QuitNow() {
  base::MessageLoop::current()->QuitNow();
}

void MenuMessageLoopMac::ClearOwner() {
}

}  // namespace views

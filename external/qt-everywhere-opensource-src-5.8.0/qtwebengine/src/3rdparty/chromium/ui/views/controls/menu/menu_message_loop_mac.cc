// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_loop_mac.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/gfx/geometry/point.h"

namespace views {

// static
MenuMessageLoop* MenuMessageLoop::Create() {
  return new MenuMessageLoopMac;
}

// static
void MenuMessageLoop::RepostEventToWindow(const ui::LocatedEvent* event,
                                          gfx::NativeWindow window,
                                          const gfx::Point& screen_loc) {
  NOTIMPLEMENTED();
}

MenuMessageLoopMac::MenuMessageLoopMac() {}

MenuMessageLoopMac::~MenuMessageLoopMac() {}

void MenuMessageLoopMac::Run(MenuController* controller,
                             Widget* owner,
                             bool nested_menu) {
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow(loop);
  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> reset_run_loop(&run_loop_, &run_loop);
  run_loop.Run();
}

void MenuMessageLoopMac::QuitNow() {
  DCHECK(run_loop_);
  run_loop_->Quit();
}

void MenuMessageLoopMac::ClearOwner() {
}

}  // namespace views

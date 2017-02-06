// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_MAC_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/controls/menu/menu_message_loop.h"

namespace base {
class RunLoop;
}

namespace views {

class MenuMessageLoopMac : public MenuMessageLoop {
 public:
  MenuMessageLoopMac();
  ~MenuMessageLoopMac() override;

  // Overridden from MenuMessageLoop:
  void Run(MenuController* controller,
           Widget* owner,
           bool nested_menu) override;
  void QuitNow() override;
  void ClearOwner() override;

 private:
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MenuMessageLoopMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_MAC_H_

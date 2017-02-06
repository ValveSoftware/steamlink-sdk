// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_AURA_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_AURA_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/controls/menu/menu_message_loop.h"

namespace ui {
class ScopedEventDispatcher;
}

namespace views {

class MenuMessageLoopAura : public MenuMessageLoop {
 public:
  MenuMessageLoopAura();
  ~MenuMessageLoopAura() override;

  // Overridden from MenuMessageLoop:
  void Run(MenuController* controller,
           Widget* owner,
           bool nested_menu) override;
  void QuitNow() override;
  void ClearOwner() override;

 private:
  // Owner of child windows.
  // WARNING: this may be NULL.
  Widget* owner_;

  base::Closure message_loop_quit_;

  DISALLOW_COPY_AND_ASSIGN(MenuMessageLoopAura);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_AURA_H_

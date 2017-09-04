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

namespace views {

class MenuMessageLoopAura : public MenuMessageLoop {
 public:
  MenuMessageLoopAura();
  ~MenuMessageLoopAura() override;

  // MenuMessageLoop:
  void Run() override;
  void QuitNow() override;

 private:
  base::Closure message_loop_quit_;

  DISALLOW_COPY_AND_ASSIGN(MenuMessageLoopAura);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_AURA_H_

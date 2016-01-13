// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_MAC_H_

#include "base/compiler_specific.h"
#include "ui/views/controls/menu/menu_message_loop.h"

namespace views {

class MenuMessageLoopMac : public MenuMessageLoop {
 public:
  MenuMessageLoopMac();
  virtual ~MenuMessageLoopMac();

  // Overridden from MenuMessageLoop:
  virtual void Run(MenuController* controller,
                   Widget* owner,
                   bool nested_menu) OVERRIDE;
  virtual bool ShouldQuitNow() const OVERRIDE;
  virtual void QuitNow() OVERRIDE;
  virtual void RepostEventToWindow(const ui::LocatedEvent& event,
                                   gfx::NativeWindow window,
                                   const gfx::Point& screen_loc) OVERRIDE;
  virtual void ClearOwner() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuMessageLoopMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_MAC_H_

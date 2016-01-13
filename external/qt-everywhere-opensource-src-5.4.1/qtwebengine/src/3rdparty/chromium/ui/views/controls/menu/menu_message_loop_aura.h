// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_AURA_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_AURA_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/menu/menu_message_loop.h"

namespace base {
class MessagePumpDispatcher;
}

namespace ui {
class ScopedEventDispatcher;
}

namespace views {

class MenuMessageLoopAura : public MenuMessageLoop {
 public:
  MenuMessageLoopAura();
  virtual ~MenuMessageLoopAura();

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
  // Owner of child windows.
  // WARNING: this may be NULL.
  Widget* owner_;

  scoped_ptr<ui::ScopedEventDispatcher> nested_dispatcher_;
  base::Closure message_loop_quit_;

  DISALLOW_COPY_AND_ASSIGN(MenuMessageLoopAura);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_LOOP_AURA_H_

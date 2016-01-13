// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_pump_dispatcher_win.h"

#include <windowsx.h>

#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace views {
namespace internal {

MenuMessagePumpDispatcher::MenuMessagePumpDispatcher(MenuController* controller)
    : menu_controller_(controller) {}

MenuMessagePumpDispatcher::~MenuMessagePumpDispatcher() {}

uint32_t MenuMessagePumpDispatcher::Dispatch(const MSG& msg) {
  DCHECK(menu_controller_->IsBlockingRun());

  bool should_quit = false;
  bool should_perform_default = true;
  if (menu_controller_->exit_type() == MenuController::EXIT_ALL ||
      menu_controller_->exit_type() == MenuController::EXIT_DESTROYED) {
    should_quit = true;
  } else {
    // NOTE: we don't get WM_ACTIVATE or anything else interesting in here.
    switch (msg.message) {
      case WM_CONTEXTMENU: {
        MenuItemView* item = menu_controller_->pending_state_.item;
        if (item && item->GetRootMenuItem() != item) {
          gfx::Point screen_loc(0, item->height());
          View::ConvertPointToScreen(item, &screen_loc);
          ui::MenuSourceType source_type = ui::MENU_SOURCE_MOUSE;
          if (GET_X_LPARAM(msg.lParam) == -1 && GET_Y_LPARAM(msg.lParam) == -1)
            source_type = ui::MENU_SOURCE_KEYBOARD;
          item->GetDelegate()->ShowContextMenu(
              item, item->GetCommand(), screen_loc, source_type);
        }
        should_quit = false;
        should_perform_default = false;
        break;
      }

      // NOTE: focus wasn't changed when the menu was shown. As such, don't
      // dispatch key events otherwise the focused window will get the events.
      case WM_KEYDOWN: {
        bool result =
            menu_controller_->OnKeyDown(ui::KeyboardCodeFromNative(msg));
        TranslateMessage(&msg);
        should_perform_default = false;
        should_quit = !result;
        break;
      }
      case WM_CHAR: {
        should_quit = menu_controller_->SelectByChar(
            static_cast<base::char16>(msg.wParam));
        should_perform_default = false;
        break;
      }
      case WM_KEYUP:
      case WM_SYSKEYUP:
        // We may have been shown on a system key, as such don't do anything
        // here. If another system key is pushed we'll get a WM_SYSKEYDOWN and
        // close the menu.
        should_quit = false;
        should_perform_default = false;
        break;

      case WM_CANCELMODE:
      case WM_SYSKEYDOWN:
        // Exit immediately on system keys.
        menu_controller_->Cancel(MenuController::EXIT_ALL);
        should_quit = true;
        should_perform_default = false;
        break;

      default:
        break;
    }
  }

  if (should_quit || menu_controller_->exit_type() != MenuController::EXIT_NONE)
    menu_controller_->TerminateNestedMessageLoop();
  return should_perform_default ? POST_DISPATCH_PERFORM_DEFAULT
                                : POST_DISPATCH_NONE;
}

}  // namespace internal
}  // namespace views

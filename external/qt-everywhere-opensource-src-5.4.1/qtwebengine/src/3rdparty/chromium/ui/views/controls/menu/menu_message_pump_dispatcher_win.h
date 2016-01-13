// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_PUMP_DISPATCHER_WIN_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_PUMP_DISPATCHER_WIN_H_

#include "base/macros.h"
#include "base/message_loop/message_pump_dispatcher.h"

namespace views {

class MenuController;

namespace internal {

// A message-pump dispatcher object used to dispatch events from the nested
// message-loop initiated by the MenuController.
class MenuMessagePumpDispatcher : public base::MessagePumpDispatcher {
 public:
  explicit MenuMessagePumpDispatcher(MenuController* menu_controller);
  virtual ~MenuMessagePumpDispatcher();

 private:
  // base::MessagePumpDispatcher:
  virtual uint32_t Dispatch(const base::NativeEvent& event) OVERRIDE;

  MenuController* menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(MenuMessagePumpDispatcher);
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MESSAGE_PUMP_DISPATCHER_WIN_H_

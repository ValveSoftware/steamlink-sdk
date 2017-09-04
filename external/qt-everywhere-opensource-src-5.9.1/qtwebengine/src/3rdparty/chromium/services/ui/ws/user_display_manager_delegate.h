// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_USER_DISPLAY_MANAGER_DELEGATE_H_
#define SERVICES_UI_WS_USER_DISPLAY_MANAGER_DELEGATE_H_

#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "services/ui/ws/user_id.h"

namespace ui {
namespace ws {

class Display;
class WindowManagerState;

class UserDisplayManagerDelegate {
 public:
  // Gets the frame decorations for the specified user. Returns true if the
  // decorations have been set, false otherwise. |values| may be null.
  virtual bool GetFrameDecorationsForUser(
      const UserId& user_id,
      mojom::FrameDecorationValuesPtr* values) = 0;

 protected:
  virtual ~UserDisplayManagerDelegate() {}
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_USER_DISPLAY_MANAGER_DELEGATE_H_

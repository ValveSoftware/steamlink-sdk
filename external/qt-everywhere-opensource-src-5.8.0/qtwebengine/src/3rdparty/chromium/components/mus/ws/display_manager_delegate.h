// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_
#define COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_

#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/ws/user_id.h"

namespace mus {
namespace ws {

class Display;
class WindowManagerState;

class DisplayManagerDelegate {
 public:
  virtual void OnFirstDisplayReady() = 0;
  virtual void OnNoMoreDisplays() = 0;

  // Gets the frame decorations for the specified user. Returns true if the
  // decorations have been set, false otherwise. |values| may be null.
  virtual bool GetFrameDecorationsForUser(
      const UserId& user_id,
      mojom::FrameDecorationValuesPtr* values) = 0;

  virtual WindowManagerState* GetWindowManagerStateForUser(
      const UserId& user_id) = 0;

 protected:
  virtual ~DisplayManagerDelegate() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_DISPLAY_MANAGER_DELEGATE_H_

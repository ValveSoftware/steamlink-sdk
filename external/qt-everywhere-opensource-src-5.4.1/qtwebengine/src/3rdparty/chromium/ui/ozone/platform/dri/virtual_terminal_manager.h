// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_VIRTUAL_TERMINAL_MANAGER_H_
#define UI_OZONE_PLATFORM_DRI_VIRTUAL_TERMINAL_MANAGER_H_

#include <termios.h>

#include "base/basictypes.h"

namespace ui {

class VirtualTerminalManager {
 public:
  VirtualTerminalManager();
  ~VirtualTerminalManager();

 private:

  int fd_;
  int vt_mode_;
  int previous_keyboard_mode_;
  struct termios terminal_attributes_;

  DISALLOW_COPY_AND_ASSIGN(VirtualTerminalManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_VIRTUAL_TERMINAL_MANAGER_H_

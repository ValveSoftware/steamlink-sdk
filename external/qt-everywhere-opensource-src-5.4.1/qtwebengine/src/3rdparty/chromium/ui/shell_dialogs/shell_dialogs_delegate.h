// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_DELEGATE_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_DELEGATE_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

class SHELL_DIALOGS_EXPORT ShellDialogsDelegate {
 public:
  virtual ~ShellDialogsDelegate() {}

  // Returns true if the window passed in is in the Windows 8 metro
  // environment.
  virtual bool IsWindowInMetro(gfx::NativeWindow window) = 0;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_DELEGATE_H_

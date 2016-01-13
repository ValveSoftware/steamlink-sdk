// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/linux_shell_dialog.h"

namespace {

ui::LinuxShellDialog* g_linux_shell_dialog = NULL;

}  // namespace

namespace ui {

void LinuxShellDialog::SetInstance(LinuxShellDialog* instance) {
  g_linux_shell_dialog = instance;
}

const LinuxShellDialog* LinuxShellDialog::instance() {
  return g_linux_shell_dialog;
}

}  // namespace ui

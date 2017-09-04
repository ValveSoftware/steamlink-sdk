// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_mac.h"

#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

ShellDesktopControllerMac::ShellDesktopControllerMac()
    : app_window_client_(new ShellAppWindowClient), app_window_(NULL) {
  AppWindowClient::Set(app_window_client_.get());
}

ShellDesktopControllerMac::~ShellDesktopControllerMac() {
  // TOOD(yoz): This is actually too late to close app windows (for tests).
  // Maybe this is useful for non-tests.
  CloseAppWindows();
}

gfx::Size ShellDesktopControllerMac::GetWindowSize() {
  // This is the full screen size.
  return display::Screen::GetScreen()->GetPrimaryDisplay().bounds().size();
}

AppWindow* ShellDesktopControllerMac::CreateAppWindow(
    content::BrowserContext* context,
    const Extension* extension) {
  app_window_ = new AppWindow(context, new ShellAppDelegate, extension);
  return app_window_;
}

void ShellDesktopControllerMac::AddAppWindow(gfx::NativeWindow window) {
}

void ShellDesktopControllerMac::RemoveAppWindow(AppWindow* window) {
}

void ShellDesktopControllerMac::CloseAppWindows() {
  if (app_window_) {
    ui::BaseWindow* window = app_window_->GetBaseWindow();
    window->Close();  // Close() deletes |app_window_|.
    app_window_ = NULL;
  }
}

}  // namespace extensions

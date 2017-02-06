// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_DESKTOP_CONTROLLER_H_
#define EXTENSIONS_SHELL_BROWSER_DESKTOP_CONTROLLER_H_

#include "ui/gfx/native_widget_types.h"

namespace aura {
class Window;
class WindowTreeHost;
}

namespace content {
class BrowserContext;
}

namespace gfx {
class Size;
}

namespace extensions {
class AppWindow;
class Extension;
class ShellAppWindow;

// DesktopController is an interface to construct the window environment in
// extensions shell. ShellDesktopControllerAura provides a default
// implementation for app_shell, and other embedders can provide their own.
// TODO(jamescook|oshima): Clean up this interface now that there is only one
// way to create an app window.
class DesktopController {
 public:
  DesktopController();
  virtual ~DesktopController();

  // Returns the single instance of the desktop. (Stateless functions like
  // ShellAppWindowCreateFunction need to be able to access the desktop, so
  // we need a singleton somewhere).
  static DesktopController* instance();

  // Get the size of the window created by this DesktopController. This should
  // typically be full-screen.
  virtual gfx::Size GetWindowSize() = 0;

  // Creates a new app window and adds it to the desktop. The desktop maintains
  // ownership of the window. The window must be closed before |extension| is
  // destroyed.
  virtual AppWindow* CreateAppWindow(content::BrowserContext* context,
                                     const Extension* extension) = 0;

  // Attaches the window to our window hierarchy.
  virtual void AddAppWindow(gfx::NativeWindow window) = 0;

  // Removes the window from the desktop.
  virtual void RemoveAppWindow(AppWindow* window) = 0;

  // Closes and destroys the app windows.
  virtual void CloseAppWindows() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_DESKTOP_CONTROLLER_H_

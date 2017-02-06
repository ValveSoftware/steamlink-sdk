// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class AppWindow;

class AppCurrentWindowInternalExtensionFunction : public SyncExtensionFunction {
 protected:
  ~AppCurrentWindowInternalExtensionFunction() override {}

  // Invoked with the current app window.
  virtual bool RunWithWindow(AppWindow* window) = 0;

 private:
  bool RunSync() override;
};

class AppCurrentWindowInternalFocusFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.focus",
                             APP_CURRENTWINDOWINTERNAL_FOCUS)

 protected:
  ~AppCurrentWindowInternalFocusFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalFullscreenFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.fullscreen",
                             APP_CURRENTWINDOWINTERNAL_FULLSCREEN)

 protected:
  ~AppCurrentWindowInternalFullscreenFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalMaximizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.maximize",
                             APP_CURRENTWINDOWINTERNAL_MAXIMIZE)

 protected:
  ~AppCurrentWindowInternalMaximizeFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalMinimizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.minimize",
                             APP_CURRENTWINDOWINTERNAL_MINIMIZE)

 protected:
  ~AppCurrentWindowInternalMinimizeFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalRestoreFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.restore",
                             APP_CURRENTWINDOWINTERNAL_RESTORE)

 protected:
  ~AppCurrentWindowInternalRestoreFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalDrawAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.drawAttention",
                             APP_CURRENTWINDOWINTERNAL_DRAWATTENTION)

 protected:
  ~AppCurrentWindowInternalDrawAttentionFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalClearAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.clearAttention",
                             APP_CURRENTWINDOWINTERNAL_CLEARATTENTION)

 protected:
  ~AppCurrentWindowInternalClearAttentionFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalShowFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.show",
                             APP_CURRENTWINDOWINTERNAL_SHOW)

 protected:
  ~AppCurrentWindowInternalShowFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalHideFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.hide",
                             APP_CURRENTWINDOWINTERNAL_HIDE)

 protected:
  ~AppCurrentWindowInternalHideFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalSetBoundsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setBounds",
                             APP_CURRENTWINDOWINTERNAL_SETBOUNDS)
 protected:
  ~AppCurrentWindowInternalSetBoundsFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalSetSizeConstraintsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setSizeConstraints",
                             APP_CURRENTWINDOWINTERNAL_SETSIZECONSTRAINTS)
 protected:
  ~AppCurrentWindowInternalSetSizeConstraintsFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalSetIconFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setIcon",
                             APP_CURRENTWINDOWINTERNAL_SETICON)

 protected:
  ~AppCurrentWindowInternalSetIconFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalSetShapeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setShape",
                             APP_CURRENTWINDOWINTERNAL_SETSHAPE)

 protected:
  ~AppCurrentWindowInternalSetShapeFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalSetAlwaysOnTopFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setAlwaysOnTop",
                             APP_CURRENTWINDOWINTERNAL_SETALWAYSONTOP)

 protected:
  ~AppCurrentWindowInternalSetAlwaysOnTopFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

class AppCurrentWindowInternalSetVisibleOnAllWorkspacesFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "app.currentWindowInternal.setVisibleOnAllWorkspaces",
      APP_CURRENTWINDOWINTERNAL_SETVISIBLEONALLWORKSPACES)

 protected:
  ~AppCurrentWindowInternalSetVisibleOnAllWorkspacesFunction() override {}
  bool RunWithWindow(AppWindow* window) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_

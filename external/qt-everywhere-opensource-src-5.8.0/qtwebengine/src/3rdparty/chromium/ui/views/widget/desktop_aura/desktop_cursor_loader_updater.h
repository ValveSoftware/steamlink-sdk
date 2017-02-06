// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_CURSOR_LOADER_UPDATER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_CURSOR_LOADER_UPDATER_H_

#include <memory>

#include "ui/views/views_export.h"

namespace aura {
class RootWindow;
}

namespace display {
class Display;
}

namespace ui {
class CursorLoader;
}

namespace views {

// An interface to optionally update the state of a cursor loader. Only used on
// desktop AuraX11.
class VIEWS_EXPORT DesktopCursorLoaderUpdater {
 public:
  virtual ~DesktopCursorLoaderUpdater() {}

  // Creates a new DesktopCursorLoaderUpdater, or NULL if the platform doesn't
  // support one.
  static std::unique_ptr<DesktopCursorLoaderUpdater> Create();

  // Called when a CursorLoader is created.
  virtual void OnCreate(float device_scale_factor,
                        ui::CursorLoader* loader) = 0;

  // Called when the display has changed (as we may need to reload the cursor
  // assets in response to a device scale factor or rotation change).
  virtual void OnDisplayUpdated(const display::Display& display,
                                ui::CursorLoader* loader) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPLAY_CHANGE_HANDLER_H_

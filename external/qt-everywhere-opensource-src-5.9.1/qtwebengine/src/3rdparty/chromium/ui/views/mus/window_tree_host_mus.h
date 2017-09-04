// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_

#include "base/macros.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/views/mus/mus_export.h"

namespace ui {
class Window;
}

namespace views {

class NativeWidgetMus;

class VIEWS_MUS_EXPORT WindowTreeHostMus : public aura::WindowTreeHostPlatform {
 public:
  WindowTreeHostMus(NativeWidgetMus* native_widget, ui::Window* window);
  ~WindowTreeHostMus() override;
  NativeWidgetMus* native_widget() { return native_widget_; }

 private:
  // aura::WindowTreeHostPlatform:
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;
  gfx::ICCProfile GetICCProfileForCurrentDisplay() override;

  NativeWidgetMus* native_widget_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_

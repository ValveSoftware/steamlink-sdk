// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/widget/desktop_aura/desktop_factory_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

namespace views {

DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  DesktopFactoryOzone* d_factory = DesktopFactoryOzone::GetInstance();

  return d_factory->CreateWindowTreeHost(native_widget_delegate,
                                         desktop_native_widget_aura);
}

// static
ui::NativeTheme* DesktopWindowTreeHost::GetNativeTheme(aura::Window* window) {
  return ui::NativeThemeAura::instance();
}

}  // namespace views

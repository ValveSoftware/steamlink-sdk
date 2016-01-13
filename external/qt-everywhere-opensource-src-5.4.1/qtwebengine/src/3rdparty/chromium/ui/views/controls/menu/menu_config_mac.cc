// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/controls/menu/menu_image_util.h"

namespace views {

void MenuConfig::Init(const ui::NativeTheme* theme) {
  NOTIMPLEMENTED();
}

// static
const MenuConfig& MenuConfig::instance(const ui::NativeTheme* theme) {
  CR_DEFINE_STATIC_LOCAL(
      MenuConfig, mac_instance, (theme ? theme : ui::NativeTheme::instance()));
  return mac_instance;
}

}  // namespace views

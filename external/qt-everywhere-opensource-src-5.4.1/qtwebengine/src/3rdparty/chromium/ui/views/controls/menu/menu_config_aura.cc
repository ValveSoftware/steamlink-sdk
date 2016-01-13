// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "grit/ui_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/controls/menu/menu_image_util.h"

namespace views {

namespace {
#if defined(OS_CHROMEOS)
static const int kMenuCornerRadiusForAura = 2;
#else
static const int kMenuCornerRadiusForAura = 0;
#endif
}  // namespace

#if !defined(OS_WIN)
void MenuConfig::Init(const ui::NativeTheme* theme) {
  if (theme == ui::NativeThemeAura::instance())
    InitAura(theme);
}
#endif

void MenuConfig::InitAura(const ui::NativeTheme* theme) {
  submenu_horizontal_inset = 1;
  arrow_to_edge_padding = 20;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  arrow_width = rb.GetImageNamed(IDR_MENU_HIERARCHY_ARROW).Width();
  gfx::ImageSkia check = GetMenuCheckImage(false);
  check_height = check.height();
  item_min_height = 29;
  separator_spacing_height = 7;
  separator_lower_height = 8;
  separator_upper_height = 8;
  label_to_arrow_padding = 20;
  label_to_minor_text_padding = 20;
  always_use_icon_to_label_padding = true;
  align_arrow_and_shortcut = true;
  offset_context_menus = true;
  corner_radius = kMenuCornerRadiusForAura;
}

#if !defined(OS_WIN)
// static
const MenuConfig& MenuConfig::instance(const ui::NativeTheme* theme) {
  static MenuConfig* views_instance = NULL;
  if (!views_instance)
    views_instance = new MenuConfig(theme ?
        theme : ui::NativeTheme::instance());
  return *views_instance;
}
#endif

}  // namespace views

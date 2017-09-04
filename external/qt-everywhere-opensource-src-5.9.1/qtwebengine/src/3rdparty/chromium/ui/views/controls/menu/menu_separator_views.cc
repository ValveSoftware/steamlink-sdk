// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_config.h"

namespace views {

#if !defined(OS_WIN)
void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  OnPaintAura(canvas);
}
#endif

gfx::Size MenuSeparator::GetPreferredSize() const {
  const MenuConfig& menu_config = MenuConfig::instance();
  int height = menu_config.separator_height;
  switch(type_) {
    case ui::SPACING_SEPARATOR:
      height = menu_config.separator_spacing_height;
      break;
    case ui::LOWER_SEPARATOR:
      height = menu_config.separator_lower_height;
      break;
    case ui::UPPER_SEPARATOR:
      height = menu_config.separator_upper_height;
      break;
    default:
      height = menu_config.separator_height;
      break;
  }
  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   height);
}

gfx::Rect MenuSeparator::GetPaintBounds() {
  int pos = 0;
  const MenuConfig& menu_config = MenuConfig::instance();
  int separator_thickness = menu_config.separator_thickness;
  switch (type_) {
    case ui::LOWER_SEPARATOR:
      pos = height() - separator_thickness;
      break;
    case ui::SPACING_SEPARATOR:
      return gfx::Rect();
    case ui::UPPER_SEPARATOR:
      break;
    default:
      pos = height() / 2;
      break;
  }

  gfx::Rect paint_rect(0, pos, width(), separator_thickness);
  if (menu_config.use_outer_border)
    paint_rect.Inset(1, 0);
  return paint_rect;
}

void MenuSeparator::OnPaintAura(gfx::Canvas* canvas) {
  canvas->FillRect(GetPaintBounds(),
                   GetNativeTheme()->GetSystemColor(
                       ui::NativeTheme::kColorId_MenuSeparatorColor));
}

}  // namespace views

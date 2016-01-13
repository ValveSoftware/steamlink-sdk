// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace views {

void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  const MenuConfig& config = parent_menu_item_->GetMenuConfig();

  if (config.native_theme == ui::NativeThemeAura::instance()) {
    OnPaintAura(canvas);
    return;
  }

  int start_x = 0;
  if (config.render_gutter) {
    // If render_gutter is true, we're on Vista and need to render the
    // gutter, then indent the separator from the gutter.
    gfx::Rect gutter_bounds(MenuItemView::label_start() -
        config.gutter_to_label - config.gutter_width, 0,
        config.gutter_width, height());
    ui::NativeTheme::ExtraParams extra;
    config.native_theme->Paint(
        canvas->sk_canvas(), ui::NativeTheme::kMenuPopupGutter,
        ui::NativeTheme::kNormal, gutter_bounds, extra);
    start_x = gutter_bounds.x() + config.gutter_width;
  }

  gfx::Rect separator_bounds(start_x, 0, width(), height());
  ui::NativeTheme::ExtraParams extra;
  extra.menu_separator.has_gutter = config.render_gutter;
  config.native_theme->Paint(
      canvas->sk_canvas(), ui::NativeTheme::kMenuPopupSeparator,
      ui::NativeTheme::kNormal, separator_bounds, extra);
}

gfx::Size MenuSeparator::GetPreferredSize() const {
  const MenuConfig& config = parent_menu_item_->GetMenuConfig();

  if (config.native_theme == ui::NativeThemeAura::instance())
    return GetPreferredSizeAura();

  return gfx::Size(10,  // Just in case we're the only item in a menu.
                   config.separator_height);
}

}  // namespace views

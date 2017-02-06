// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "ui/display/win/dpi.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace views {

void MenuSeparator::OnPaint(gfx::Canvas* canvas) {
  ui::NativeTheme* native_theme = GetNativeTheme();
  if (native_theme == ui::NativeThemeAura::instance()) {
    OnPaintAura(canvas);
    return;
  }

  gfx::Rect separator_bounds = GetPaintBounds();

  // Hack to get the separator to display correctly on Windows where we may
  // have fractional scales. We move the separator 1 pixel down to ensure that
  // it falls within the clipping rect which is scaled up.
  float device_scale = display::win::GetDPIScale();
  bool is_fractional_scale =
      (device_scale - static_cast<int>(device_scale) != 0);
  if (is_fractional_scale && separator_bounds.y() == 0)
    separator_bounds.set_y(1);

  ui::NativeTheme::ExtraParams extra;
  native_theme->Paint(
      canvas->sk_canvas(), ui::NativeTheme::kMenuPopupSeparator,
      ui::NativeTheme::kNormal, separator_bounds, extra);
}

}  // namespace views

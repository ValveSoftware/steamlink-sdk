// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "base/logging.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_win.h"

using ui::NativeTheme;
using ui::NativeThemeWin;

namespace views {

void MenuConfig::Init(const NativeTheme* theme) {
  if (theme == ui::NativeThemeAura::instance()) {
    InitAura(theme);
    return;
  }

  arrow_color = color_utils::GetSysSkColor(COLOR_MENUTEXT);

  NONCLIENTMETRICS metrics;
  base::win::GetNonClientMetrics(&metrics);
  l10n_util::AdjustUIFont(&(metrics.lfMenuFont));
  {
    base::win::ScopedHFONT new_font(CreateFontIndirect(&metrics.lfMenuFont));
    DLOG_ASSERT(new_font.Get());
    font_list = gfx::FontList(gfx::Font(new_font));
  }
  NativeTheme::ExtraParams extra;
  extra.menu_check.is_radio = false;
  extra.menu_check.is_selected = false;
  gfx::Size check_size = NativeThemeWin::instance()->GetPartSize(
      NativeTheme::kMenuCheck, NativeTheme::kNormal, extra);
  if (!check_size.IsEmpty()) {
    check_width = check_size.width();
    check_height = check_size.height();
  } else {
    check_width = GetSystemMetrics(SM_CXMENUCHECK);
    check_height = GetSystemMetrics(SM_CYMENUCHECK);
  }

  extra.menu_check.is_radio = true;
  gfx::Size radio_size = NativeThemeWin::instance()->GetPartSize(
      NativeTheme::kMenuCheck, NativeTheme::kNormal, extra);
  if (!radio_size.IsEmpty())
    radio_width = radio_size.width();
  else
    radio_width = GetSystemMetrics(SM_CXMENUCHECK);

  gfx::Size arrow_size = NativeThemeWin::instance()->GetPartSize(
      NativeTheme::kMenuPopupArrow, NativeTheme::kNormal, extra);
  if (!arrow_size.IsEmpty()) {
    arrow_width = arrow_size.width();
  } else {
    // Sadly I didn't see a specify metrics for this.
    arrow_width = GetSystemMetrics(SM_CXMENUCHECK);
  }

  BOOL show_cues;
  show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);

  SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &show_delay, 0);
}

// static
const MenuConfig& MenuConfig::instance(const ui::NativeTheme* theme) {
  // NOTE: |theme| may be NULL if used before the menu is running.
  if (!theme || theme == NativeThemeWin::instance()) {
    static MenuConfig* win_instance = NULL;
    if (!win_instance)
      win_instance = new MenuConfig(NativeThemeWin::instance());
    return *win_instance;
  }
  static MenuConfig* views_instance = NULL;
  if (!views_instance)
    views_instance = new MenuConfig(theme);
  return *views_instance;
}

}  // namespace views

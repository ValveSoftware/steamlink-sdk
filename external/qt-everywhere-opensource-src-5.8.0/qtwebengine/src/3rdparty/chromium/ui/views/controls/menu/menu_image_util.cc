// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_image_util.h"

#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"

namespace views {

gfx::ImageSkia GetMenuCheckImage(SkColor icon_color) {
  return gfx::CreateVectorIcon(gfx::VectorIconId::MENU_CHECK, icon_color);
}

gfx::ImageSkia GetRadioButtonImage(bool toggled,
                                   bool hovered,
                                   SkColor default_icon_color) {
  gfx::VectorIconId id = toggled ? gfx::VectorIconId::MENU_RADIO_SELECTED
                                 : gfx::VectorIconId::MENU_RADIO_EMPTY;
  SkColor color =
      toggled && !hovered ? gfx::kGoogleBlue500 : default_icon_color;
  return gfx::CreateVectorIcon(id, kMenuCheckSize, color);
}

gfx::ImageSkia GetSubmenuArrowImage(SkColor icon_color) {
  return gfx::CreateVectorIcon(gfx::VectorIconId::SUBMENU_ARROW, icon_color);
}

}  // namespace views

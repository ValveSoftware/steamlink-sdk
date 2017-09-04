// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/vector_icon_button.h"

#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/vector_icon_button_delegate.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Extra space around the buttons to increase their event target size.
const int kButtonExtraTouchSize = 4;

}  // namespace

VectorIconButton::VectorIconButton(VectorIconButtonDelegate* delegate)
    : views::ImageButton(delegate),
      delegate_(delegate),
      id_(gfx::VectorIconId::VECTOR_ICON_NONE) {
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetFocusPainter(nullptr);
}

VectorIconButton::~VectorIconButton() {}

void VectorIconButton::SetIcon(gfx::VectorIconId id) {
  id_ = id;

  if (!border()) {
    SetBorder(
        views::CreateEmptyBorder(kButtonExtraTouchSize, kButtonExtraTouchSize,
                                 kButtonExtraTouchSize, kButtonExtraTouchSize));
  }
}

void VectorIconButton::OnThemeChanged() {
  SkColor icon_color =
      color_utils::DeriveDefaultIconColor(delegate_->GetVectorIconBaseColor());
  gfx::ImageSkia image = gfx::CreateVectorIcon(id_, icon_color);
  SetImage(views::CustomButton::STATE_NORMAL, &image);
  image = gfx::CreateVectorIcon(id_, SkColorSetA(icon_color, 0xff / 2));
  SetImage(views::CustomButton::STATE_DISABLED, &image);
  set_ink_drop_base_color(icon_color);
}

void VectorIconButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  OnThemeChanged();
}

}  // namespace views

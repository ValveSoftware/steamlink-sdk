// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focus_ring.h"

#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/focusable_border.h"

namespace views {

namespace {

// The stroke width of the focus border in dp.
constexpr float kFocusHaloThicknessDp = 2.f;

// The focus indicator should hug the normal border, when present (as in the
// case of text buttons). Since it's drawn outside the parent view, we have to
// increase the rounding slightly.
constexpr float kFocusHaloCornerRadiusDp =
    FocusableBorder::kCornerRadiusDp + kFocusHaloThicknessDp / 2.f;

FocusRing* GetFocusRing(views::View* parent) {
  for (int i = 0; i < parent->child_count(); ++i) {
    if (parent->child_at(i)->GetClassName() == FocusRing::kViewClassName)
      return static_cast<FocusRing*>(parent->child_at(i));
  }
  return nullptr;
}

}  // namespace

const char FocusRing::kViewClassName[] = "FocusRing";

// static
views::View* FocusRing::Install(views::View* parent,
                                ui::NativeTheme::ColorId override_color_id) {
  DCHECK(parent->HasFocus());
  FocusRing* ring = GetFocusRing(parent);
  if (!ring) {
    ring = new FocusRing();
    parent->AddChildView(ring);
  }
  ring->override_color_id_ = override_color_id;
  ring->Layout();
  ring->SchedulePaint();
  return ring;
}

// static
void FocusRing::Uninstall(views::View* parent) {
  delete GetFocusRing(parent);
}

const char* FocusRing::GetClassName() const {
  return kViewClassName;
}

bool FocusRing::CanProcessEventsWithinSubtree() const {
  return false;
}

void FocusRing::Layout() {
  // The focus ring handles its own sizing, which is simply to fill the parent
  // and extend a little beyond its borders.
  gfx::Rect focus_bounds = parent()->GetLocalBounds();
  focus_bounds.Inset(gfx::Insets(-kFocusHaloThicknessDp));
  SetBoundsRect(focus_bounds);
}

void FocusRing::OnPaint(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(
      SkColorSetA(GetNativeTheme()->GetSystemColor(
                      override_color_id_ != ui::NativeTheme::kColorId_NumColors
                          ? override_color_id_
                          : ui::NativeTheme::kColorId_FocusedBorderColor),
                  0x66));
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(kFocusHaloThicknessDp);
  gfx::RectF rect(GetLocalBounds());
  rect.Inset(gfx::InsetsF(kFocusHaloThicknessDp / 2.f));
  canvas->DrawRoundRect(rect, kFocusHaloCornerRadiusDp, paint);
}

FocusRing::FocusRing()
    : override_color_id_(ui::NativeTheme::kColorId_NumColors) {
  // A layer is necessary to paint beyond the parent's bounds.
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
}

FocusRing::~FocusRing() {}

}  // namespace views

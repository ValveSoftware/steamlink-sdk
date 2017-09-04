// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/toggle_button.h"

#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// Constants are measured in dip.
const int kTrackHeight = 12;
const int kTrackWidth = 28;
// Margins from edge of track to edge of view.
const int kTrackVerticalMargin = 5;
const int kTrackHorizontalMargin = 6;
// Inset from the rounded edge of the thumb to the rounded edge of the track.
const int kThumbInset = 2;

}  // namespace

// Class representing the thumb (the circle that slides horizontally).
class ToggleButton::ThumbView : public InkDropHostView {
 public:
  ThumbView() : color_ratio_(0.) {}
  ~ThumbView() override {}

  void Update(const gfx::Rect& bounds, double color_ratio) {
    SetBoundsRect(bounds);
    color_ratio_ = color_ratio;
    SchedulePaint();
  }

  // Returns the extra space needed to draw the shadows around the thumb. Since
  // the extra space is around the thumb, the insets will be negative.
  static gfx::Insets GetShadowOutsets() {
    return gfx::Insets(-kShadowBlur)
        .Offset(gfx::Vector2d(kShadowOffsetX, kShadowOffsetY));
  }

 protected:
  // views::View:
  bool CanProcessEventsWithinSubtree() const override {
    // Make the thumb behave as part of the parent for event handling.
    return false;
  }

 private:
  static const int kShadowOffsetX = 0;
  static const int kShadowOffsetY = 1;
  static const int kShadowBlur = 2;

  // views::View:
  const char* GetClassName() const override {
    return "ToggleButton::ThumbView";
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const float dsf = canvas->UndoDeviceScaleFactor();
    std::vector<gfx::ShadowValue> shadows;
    gfx::ShadowValue shadow(
        gfx::Vector2d(kShadowOffsetX, kShadowOffsetY), 2 * kShadowBlur,
        SkColorSetA(GetNativeTheme()->GetSystemColor(
                        ui::NativeTheme::kColorId_LabelEnabledColor),
                    0x99));
    shadows.push_back(shadow.Scale(dsf));
    SkPaint thumb_paint;
    thumb_paint.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadows));
    thumb_paint.setAntiAlias(true);
    const SkColor thumb_on_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    const SkColor thumb_off_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DialogBackground);
    const SkAlpha blend = static_cast<SkAlpha>(SK_AlphaOPAQUE * color_ratio_);
    thumb_paint.setColor(
        color_utils::AlphaBlend(thumb_on_color, thumb_off_color, blend));

    // We want the circle to have an integer pixel diameter and to be aligned
    // with pixel boundaries, so we scale dip bounds to pixel bounds and round.
    gfx::RectF thumb_bounds(GetLocalBounds());
    thumb_bounds.Inset(-GetShadowOutsets());
    thumb_bounds.Inset(gfx::InsetsF(0.5f));
    thumb_bounds.Scale(dsf);
    thumb_bounds = gfx::RectF(gfx::ToEnclosingRect(thumb_bounds));
    canvas->DrawCircle(thumb_bounds.CenterPoint(), thumb_bounds.height() / 2.f,
                       thumb_paint);
  }

  // Color ratio between 0 and 1 that controls the thumb color.
  double color_ratio_;

  DISALLOW_COPY_AND_ASSIGN(ThumbView);
};

// static
const char ToggleButton::kViewClassName[] = "ToggleButton";

ToggleButton::ToggleButton(ButtonListener* listener)
    : CustomButton(listener),
      is_on_(false),
      slide_animation_(this),
      thumb_view_(new ThumbView()) {
  slide_animation_.SetSlideDuration(80 /* ms */);
  slide_animation_.SetTweenType(gfx::Tween::LINEAR);
  AddChildView(thumb_view_);
  SetInkDropMode(InkDropMode::ON);
  SetFocusForPlatform();
  set_has_ink_drop_action_on_click(true);
}

ToggleButton::~ToggleButton() {
  // Destroying ink drop early allows ink drop layer to be properly removed,
  SetInkDropMode(InkDropMode::OFF);
}

void ToggleButton::SetIsOn(bool is_on, bool animate) {
  if (is_on_ == is_on)
    return;

  is_on_ = is_on;
  if (!animate) {
    slide_animation_.Reset(is_on_ ? 1.0 : 0.0);
    UpdateThumb();
    SchedulePaint();
  } else if (is_on_) {
    slide_animation_.Show();
  } else {
    slide_animation_.Hide();
  }
}

void ToggleButton::SetFocusPainter(std::unique_ptr<Painter> focus_painter) {
  focus_painter_ = std::move(focus_painter);
}

gfx::Size ToggleButton::GetPreferredSize() const {
  gfx::Rect rect(kTrackWidth, kTrackHeight);
  rect.Inset(gfx::Insets(-kTrackVerticalMargin, -kTrackHorizontalMargin));
  if (border())
    rect.Inset(-border()->GetInsets());
  return rect.size();
}

gfx::Rect ToggleButton::GetTrackBounds() const {
  gfx::Rect track_bounds(GetContentsBounds());
  track_bounds.ClampToCenteredSize(gfx::Size(kTrackWidth, kTrackHeight));
  return track_bounds;
}

gfx::Rect ToggleButton::GetThumbBounds() const {
  gfx::Rect thumb_bounds(GetTrackBounds());
  thumb_bounds.Inset(gfx::Insets(-kThumbInset));
  thumb_bounds.set_x(thumb_bounds.x() +
                     slide_animation_.GetCurrentValue() *
                         (thumb_bounds.width() - thumb_bounds.height()));
  // The thumb is a circle, so the width should match the height.
  thumb_bounds.set_width(thumb_bounds.height());
  thumb_bounds.Inset(ThumbView::GetShadowOutsets());
  return thumb_bounds;
}

void ToggleButton::UpdateThumb() {
  thumb_view_->Update(GetThumbBounds(), slide_animation_.GetCurrentValue());
}

SkColor ToggleButton::GetTrackColor(bool is_on) const {
  const SkAlpha kTrackAlpha = 0x66;
  ui::NativeTheme::ColorId color_id =
      is_on ? ui::NativeTheme::kColorId_ProminentButtonColor
            : ui::NativeTheme::kColorId_LabelEnabledColor;
  return SkColorSetA(GetNativeTheme()->GetSystemColor(color_id), kTrackAlpha);
}

const char* ToggleButton::GetClassName() const {
  return kViewClassName;
}

void ToggleButton::OnPaint(gfx::Canvas* canvas) {
  // Paint the toggle track. To look sharp even at fractional scale factors,
  // round up to pixel boundaries.
  canvas->Save();
  float dsf = canvas->UndoDeviceScaleFactor();
  gfx::RectF track_rect(GetTrackBounds());
  track_rect.Scale(dsf);
  track_rect = gfx::RectF(gfx::ToEnclosingRect(track_rect));
  SkPaint track_paint;
  track_paint.setAntiAlias(true);
  const double color_ratio = slide_animation_.GetCurrentValue();
  track_paint.setColor(color_utils::AlphaBlend(
      GetTrackColor(true), GetTrackColor(false),
      static_cast<SkAlpha>(SK_AlphaOPAQUE * color_ratio)));
  canvas->DrawRoundRect(track_rect, track_rect.height() / 2, track_paint);
  canvas->Restore();

  Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void ToggleButton::OnFocus() {
  CustomButton::OnFocus();
  if (focus_painter_)
    SchedulePaint();
}

void ToggleButton::OnBlur() {
  CustomButton::OnBlur();
  if (focus_painter_)
    SchedulePaint();
}

void ToggleButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateThumb();
}

void ToggleButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  SchedulePaint();
}

void ToggleButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  CustomButton::GetAccessibleNodeData(node_data);

  node_data->role = ui::AX_ROLE_SWITCH;
  if (is_on_)
    node_data->AddStateFlag(ui::AX_STATE_CHECKED);
}

void ToggleButton::NotifyClick(const ui::Event& event) {
  SetIsOn(!is_on(), true);
  CustomButton::NotifyClick(event);
}

void ToggleButton::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  thumb_view_->AddInkDropLayer(ink_drop_layer);
}

void ToggleButton::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  thumb_view_->RemoveInkDropLayer(ink_drop_layer);
}

std::unique_ptr<InkDrop> ToggleButton::CreateInkDrop() {
  std::unique_ptr<InkDropImpl> ink_drop =
      CustomButton::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

std::unique_ptr<InkDropRipple> ToggleButton::CreateInkDropRipple() const {
  gfx::Rect rect = thumb_view_->GetLocalBounds();
  rect.Inset(-ThumbView::GetShadowOutsets());
  return CreateDefaultInkDropRipple(rect.CenterPoint());
}

SkColor ToggleButton::GetInkDropBaseColor() const {
  return GetTrackColor(is_on());
}

void ToggleButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &slide_animation_) {
    // TODO(varkha, estade): The thumb is using its own view. Investigate if
    // repainting in every animation step to update colors could be avoided.
    UpdateThumb();
    SchedulePaint();
    return;
  }
  CustomButton::AnimationProgressed(animation);
}

}  // namespace views

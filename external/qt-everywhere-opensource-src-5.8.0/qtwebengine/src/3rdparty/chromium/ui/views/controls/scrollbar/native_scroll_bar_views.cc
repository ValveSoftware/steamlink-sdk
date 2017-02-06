// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/native_scroll_bar_views.h"

#include "base/logging.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_button.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/native_scroll_bar.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace views {

namespace {

// Wrapper for the scroll buttons.
class ScrollBarButton : public BaseScrollBarButton {
 public:
  enum Type {
    UP,
    DOWN,
    LEFT,
    RIGHT,
  };

  ScrollBarButton(ButtonListener* listener, Type type);
  ~ScrollBarButton() override;

  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override { return "ScrollBarButton"; }

 protected:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  ui::NativeTheme::ExtraParams GetNativeThemeParams() const;
  ui::NativeTheme::Part GetNativeThemePart() const;
  ui::NativeTheme::State GetNativeThemeState() const;

  Type type_;
};

// Wrapper for the scroll thumb
class ScrollBarThumb : public BaseScrollBarThumb {
 public:
  explicit ScrollBarThumb(BaseScrollBar* scroll_bar);
  ~ScrollBarThumb() override;

  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override { return "ScrollBarThumb"; }

 protected:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  ui::NativeTheme::ExtraParams GetNativeThemeParams() const;
  ui::NativeTheme::Part GetNativeThemePart() const;
  ui::NativeTheme::State GetNativeThemeState() const;

  ScrollBar* scroll_bar_;
};

/////////////////////////////////////////////////////////////////////////////
// ScrollBarButton

ScrollBarButton::ScrollBarButton(ButtonListener* listener, Type type)
    : BaseScrollBarButton(listener),
      type_(type) {
  SetFocusBehavior(FocusBehavior::NEVER);
}

ScrollBarButton::~ScrollBarButton() {
}

gfx::Size ScrollBarButton::GetPreferredSize() const {
  return GetNativeTheme()->GetPartSize(GetNativeThemePart(),
                                       GetNativeThemeState(),
                                       GetNativeThemeParams());
}

void ScrollBarButton::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds(GetPreferredSize());
  GetNativeTheme()->Paint(canvas->sk_canvas(), GetNativeThemePart(),
                          GetNativeThemeState(), bounds,
                          GetNativeThemeParams());
}

ui::NativeTheme::ExtraParams
    ScrollBarButton::GetNativeThemeParams() const {
  ui::NativeTheme::ExtraParams params;

  switch (state()) {
    case CustomButton::STATE_HOVERED:
      params.scrollbar_arrow.is_hovering = true;
      break;
    default:
      params.scrollbar_arrow.is_hovering = false;
      break;
  }

  return params;
}

ui::NativeTheme::Part
    ScrollBarButton::GetNativeThemePart() const {
  switch (type_) {
    case UP:
      return ui::NativeTheme::kScrollbarUpArrow;
    case DOWN:
      return ui::NativeTheme::kScrollbarDownArrow;
    case LEFT:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case RIGHT:
      return ui::NativeTheme::kScrollbarRightArrow;
  }

  NOTREACHED();
  return ui::NativeTheme::kScrollbarUpArrow;
}

ui::NativeTheme::State
    ScrollBarButton::GetNativeThemeState() const {
  switch (state()) {
    case CustomButton::STATE_HOVERED:
      return ui::NativeTheme::kHovered;
    case CustomButton::STATE_PRESSED:
      return ui::NativeTheme::kPressed;
    case CustomButton::STATE_DISABLED:
      return ui::NativeTheme::kDisabled;
    case CustomButton::STATE_NORMAL:
      return ui::NativeTheme::kNormal;
    case CustomButton::STATE_COUNT:
      break;
  }

  NOTREACHED();
  return ui::NativeTheme::kNormal;
}

/////////////////////////////////////////////////////////////////////////////
// ScrollBarThumb

ScrollBarThumb::ScrollBarThumb(BaseScrollBar* scroll_bar)
    : BaseScrollBarThumb(scroll_bar),
      scroll_bar_(scroll_bar) {
}

ScrollBarThumb::~ScrollBarThumb() {
}

gfx::Size ScrollBarThumb::GetPreferredSize() const {
  return GetNativeTheme()->GetPartSize(GetNativeThemePart(),
                                       GetNativeThemeState(),
                                       GetNativeThemeParams());
}

void ScrollBarThumb::OnPaint(gfx::Canvas* canvas) {
  const gfx::Rect local_bounds(GetLocalBounds());
  const ui::NativeTheme::State theme_state = GetNativeThemeState();
  const ui::NativeTheme::ExtraParams extra_params(GetNativeThemeParams());
  GetNativeTheme()->Paint(canvas->sk_canvas(),
                          GetNativeThemePart(),
                          theme_state,
                          local_bounds,
                          extra_params);
  const ui::NativeTheme::Part gripper_part = scroll_bar_->IsHorizontal() ?
      ui::NativeTheme::kScrollbarHorizontalGripper :
      ui::NativeTheme::kScrollbarVerticalGripper;
  GetNativeTheme()->Paint(canvas->sk_canvas(), gripper_part, theme_state,
                          local_bounds, extra_params);
}

ui::NativeTheme::ExtraParams ScrollBarThumb::GetNativeThemeParams() const {
  // This gives the behavior we want.
  ui::NativeTheme::ExtraParams params;
  params.scrollbar_thumb.is_hovering =
      (GetState() != CustomButton::STATE_HOVERED);
  return params;
}

ui::NativeTheme::Part ScrollBarThumb::GetNativeThemePart() const {
  if (scroll_bar_->IsHorizontal())
    return ui::NativeTheme::kScrollbarHorizontalThumb;
  return ui::NativeTheme::kScrollbarVerticalThumb;
}

ui::NativeTheme::State ScrollBarThumb::GetNativeThemeState() const {
  switch (GetState()) {
    case CustomButton::STATE_HOVERED:
      return ui::NativeTheme::kHovered;
    case CustomButton::STATE_PRESSED:
      return ui::NativeTheme::kPressed;
    case CustomButton::STATE_DISABLED:
      return ui::NativeTheme::kDisabled;
    case CustomButton::STATE_NORMAL:
      return ui::NativeTheme::kNormal;
    case CustomButton::STATE_COUNT:
      break;
  }

  NOTREACHED();
  return ui::NativeTheme::kNormal;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarViews, public:

const char NativeScrollBarViews::kViewClassName[] = "NativeScrollBarViews";

NativeScrollBarViews::NativeScrollBarViews(NativeScrollBar* scroll_bar)
    : BaseScrollBar(scroll_bar->IsHorizontal(),
                    new ScrollBarThumb(this)),
      native_scroll_bar_(scroll_bar) {
  set_controller(native_scroll_bar_->controller());

  if (native_scroll_bar_->IsHorizontal()) {
    prev_button_ = new ScrollBarButton(this, ScrollBarButton::LEFT);
    next_button_ = new ScrollBarButton(this, ScrollBarButton::RIGHT);

    part_ = ui::NativeTheme::kScrollbarHorizontalTrack;
  } else {
    prev_button_ = new ScrollBarButton(this, ScrollBarButton::UP);
    next_button_ = new ScrollBarButton(this, ScrollBarButton::DOWN);

    part_ = ui::NativeTheme::kScrollbarVerticalTrack;
  }

  state_ = ui::NativeTheme::kNormal;

  AddChildView(prev_button_);
  AddChildView(next_button_);

  prev_button_->set_context_menu_controller(this);
  next_button_->set_context_menu_controller(this);
}

NativeScrollBarViews::~NativeScrollBarViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarViews, View overrides:

void NativeScrollBarViews::Layout() {
  gfx::Size size = prev_button_->GetPreferredSize();
  prev_button_->SetBounds(0, 0, size.width(), size.height());

  if (native_scroll_bar_->IsHorizontal()) {
    next_button_->SetBounds(width() - size.width(), 0,
                            size.width(), size.height());
  } else {
    next_button_->SetBounds(0, height() - size.height(),
                            size.width(), size.height());
  }

  GetThumb()->SetBoundsRect(GetTrackBounds());
}

void NativeScrollBarViews::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetTrackBounds();

  if (bounds.IsEmpty())
    return;

  params_.scrollbar_track.track_x = bounds.x();
  params_.scrollbar_track.track_y = bounds.y();
  params_.scrollbar_track.track_width = bounds.width();
  params_.scrollbar_track.track_height = bounds.height();
  params_.scrollbar_track.classic_state = 0;

  GetNativeTheme()->Paint(canvas->sk_canvas(), part_, state_, bounds, params_);
}

gfx::Size NativeScrollBarViews::GetPreferredSize() const {
  const ui::NativeTheme* theme = native_scroll_bar_->GetNativeTheme();
  if (native_scroll_bar_->IsHorizontal())
    return gfx::Size(0, GetHorizontalScrollBarHeight(theme));
  return gfx::Size(GetVerticalScrollBarWidth(theme), 0);
}

const char* NativeScrollBarViews::GetClassName() const {
  return kViewClassName;
}

int NativeScrollBarViews::GetLayoutSize() const {
  gfx::Size size = prev_button_->GetPreferredSize();
  return IsHorizontal() ? size.height() : size.width();
}

void NativeScrollBarViews::ScrollToPosition(int position) {
  controller()->ScrollToPosition(native_scroll_bar_, position);
}

int NativeScrollBarViews::GetScrollIncrement(bool is_page, bool is_positive) {
  return controller()->GetScrollIncrement(native_scroll_bar_,
                                          is_page,
                                          is_positive);
}

//////////////////////////////////////////////////////////////////////////////
// BaseButton::ButtonListener overrides:

void NativeScrollBarViews::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  if (sender == prev_button_) {
    ScrollByAmount(SCROLL_PREV_LINE);
  } else if (sender == next_button_) {
    ScrollByAmount(SCROLL_NEXT_LINE);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarViews, NativeScrollBarWrapper overrides:

int NativeScrollBarViews::GetPosition() const {
  return BaseScrollBar::GetPosition();
}

View* NativeScrollBarViews::GetView() {
  return this;
}

void NativeScrollBarViews::Update(int viewport_size,
                                  int content_size,
                                  int current_pos) {
  BaseScrollBar::Update(viewport_size, content_size, current_pos);
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBarViews, private:

gfx::Rect NativeScrollBarViews::GetTrackBounds() const {
  gfx::Rect bounds = GetLocalBounds();
  gfx::Size size = prev_button_->GetPreferredSize();
  BaseScrollBarThumb* thumb = GetThumb();

  if (native_scroll_bar_->IsHorizontal()) {
    bounds.set_x(bounds.x() + size.width());
    bounds.set_width(std::max(0, bounds.width() - 2 * size.width()));
    bounds.set_height(thumb->GetPreferredSize().height());
  } else {
    bounds.set_y(bounds.y() + size.height());
    bounds.set_height(std::max(0, bounds.height() - 2 * size.height()));
    bounds.set_width(thumb->GetPreferredSize().width());
  }

  return bounds;
}

////////////////////////////////////////////////////////////////////////////////
// NativewScrollBarWrapper, public:

// static
NativeScrollBarWrapper* NativeScrollBarWrapper::CreateWrapper(
    NativeScrollBar* scroll_bar) {
  return new NativeScrollBarViews(scroll_bar);
}

// static
int NativeScrollBarWrapper::GetHorizontalScrollBarHeight(
    const ui::NativeTheme* theme) {
  ui::NativeTheme::ExtraParams button_params;
  button_params.scrollbar_arrow.is_hovering = false;
  gfx::Size button_size = theme->GetPartSize(
      ui::NativeTheme::kScrollbarLeftArrow,
      ui::NativeTheme::kNormal,
      button_params);

  ui::NativeTheme::ExtraParams thumb_params;
  thumb_params.scrollbar_thumb.is_hovering = false;
  gfx::Size track_size = theme->GetPartSize(
      ui::NativeTheme::kScrollbarHorizontalThumb,
      ui::NativeTheme::kNormal,
      thumb_params);

  return std::max(track_size.height(), button_size.height());
}

// static
int NativeScrollBarWrapper::GetVerticalScrollBarWidth(
    const ui::NativeTheme* theme) {
  ui::NativeTheme::ExtraParams button_params;
  button_params.scrollbar_arrow.is_hovering = false;
  gfx::Size button_size = theme->GetPartSize(
      ui::NativeTheme::kScrollbarUpArrow,
      ui::NativeTheme::kNormal,
      button_params);

  ui::NativeTheme::ExtraParams thumb_params;
  thumb_params.scrollbar_thumb.is_hovering = false;
  gfx::Size track_size = theme->GetPartSize(
      ui::NativeTheme::kScrollbarVerticalThumb,
      ui::NativeTheme::kNormal,
      thumb_params);

  return std::max(track_size.width(), button_size.width());
}

}  // namespace views

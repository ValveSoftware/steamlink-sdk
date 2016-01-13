// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <algorithm>

#include "grit/ui_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"

namespace {

// Insets for the title bar views in pixels.
const int kTitleTopInset = 12;
const int kTitleLeftInset = 19;
const int kTitleBottomInset = 12;
const int kTitleRightInset = 7;

// Get the |vertical| or horizontal amount that |available_bounds| overflows
// |window_bounds|.
int GetOffScreenLength(const gfx::Rect& available_bounds,
                       const gfx::Rect& window_bounds,
                       bool vertical) {
  if (available_bounds.IsEmpty() || available_bounds.Contains(window_bounds))
    return 0;

  //  window_bounds
  //  +---------------------------------+
  //  |             top                 |
  //  |      +------------------+       |
  //  | left | available_bounds | right |
  //  |      +------------------+       |
  //  |            bottom               |
  //  +---------------------------------+
  if (vertical)
    return std::max(0, available_bounds.y() - window_bounds.y()) +
           std::max(0, window_bounds.bottom() - available_bounds.bottom());
  return std::max(0, available_bounds.x() - window_bounds.x()) +
         std::max(0, window_bounds.right() - available_bounds.right());
}

}  // namespace

namespace views {

// static
const char BubbleFrameView::kViewClassName[] = "BubbleFrameView";

// static
gfx::Insets BubbleFrameView::GetTitleInsets() {
  return gfx::Insets(kTitleTopInset, kTitleLeftInset,
                     kTitleBottomInset, kTitleRightInset);
}

BubbleFrameView::BubbleFrameView(const gfx::Insets& content_margins)
    : bubble_border_(NULL),
      content_margins_(content_margins),
      title_(NULL),
      close_(NULL),
      titlebar_extra_view_(NULL) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_ = new Label(base::string16(),
                     rb.GetFontList(ui::ResourceBundle::MediumFont));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(title_);

  close_ = new LabelButton(this, base::string16());
  close_->SetImage(CustomButton::STATE_NORMAL,
                   *rb.GetImageNamed(IDR_CLOSE_DIALOG).ToImageSkia());
  close_->SetImage(CustomButton::STATE_HOVERED,
                   *rb.GetImageNamed(IDR_CLOSE_DIALOG_H).ToImageSkia());
  close_->SetImage(CustomButton::STATE_PRESSED,
                   *rb.GetImageNamed(IDR_CLOSE_DIALOG_P).ToImageSkia());
  close_->SetBorder(scoped_ptr<Border>());
  close_->SetSize(close_->GetPreferredSize());
  close_->SetVisible(false);
  AddChildView(close_);
}

BubbleFrameView::~BubbleFrameView() {}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = GetLocalBounds();
  client_bounds.Inset(GetInsets());
  client_bounds.Inset(bubble_border_->GetInsets());
  return client_bounds;
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return const_cast<BubbleFrameView*>(this)->GetUpdatedWindowBounds(
      gfx::Rect(), client_bounds.size(), false);
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  if (close_->visible() && close_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  // Allow dialogs to show the system menu and be dragged.
  if (GetWidget()->widget_delegate()->AsDialogDelegate()) {
    gfx::Rect sys_rect(0, 0, title_->x(), title_->y());
    sys_rect.set_origin(gfx::Point(GetMirroredXForRect(sys_rect), 0));
    if (sys_rect.Contains(point))
      return HTSYSMENU;
    if (point.y() < title_->bounds().bottom())
      return HTCAPTION;
  }

  return GetWidget()->client_view()->NonClientHitTest(point);
}

void BubbleFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
  // NOTE: this only provides implementations for the types used by dialogs.
  if ((bubble_border_->arrow() != BubbleBorder::NONE &&
       bubble_border_->arrow() != BubbleBorder::FLOAT) ||
      (bubble_border_->shadow() != BubbleBorder::SMALL_SHADOW &&
       bubble_border_->shadow() != BubbleBorder::NO_SHADOW_OPAQUE_BORDER))
    return;

  // Use a window mask roughly matching the border in the image assets.
  static const int kBorderStrokeSize = 1;
  static const SkScalar kCornerRadius = SkIntToScalar(6);
  const gfx::Insets border_insets = bubble_border_->GetInsets();
  SkRect rect = { SkIntToScalar(border_insets.left() - kBorderStrokeSize),
                  SkIntToScalar(border_insets.top() - kBorderStrokeSize),
                  SkIntToScalar(size.width() - border_insets.right() +
                                kBorderStrokeSize),
                  SkIntToScalar(size.height() - border_insets.bottom() +
                                kBorderStrokeSize) };
  if (bubble_border_->shadow() == BubbleBorder::NO_SHADOW_OPAQUE_BORDER) {
    window_mask->addRoundRect(rect, kCornerRadius, kCornerRadius);
  } else {
    static const int kBottomBorderShadowSize = 2;
    rect.fBottom += SkIntToScalar(kBottomBorderShadowSize);
    window_mask->addRect(rect);
  }
}

void BubbleFrameView::ResetWindowControls() {
  close_->SetVisible(GetWidget()->widget_delegate()->ShouldShowCloseButton());
}

void BubbleFrameView::UpdateWindowIcon() {}

void BubbleFrameView::UpdateWindowTitle() {
  title_->SetText(GetWidget()->widget_delegate()->ShouldShowWindowTitle() ?
      GetWidget()->widget_delegate()->GetWindowTitle() : base::string16());
  // Update the close button visibility too, otherwise it's not intialized.
  ResetWindowControls();
}

void BubbleFrameView::SetTitleFontList(const gfx::FontList& font_list) {
  title_->SetFontList(font_list);
}

gfx::Insets BubbleFrameView::GetInsets() const {
  gfx::Insets insets = content_margins_;
  const int title_height = title_->text().empty() ? 0 :
      title_->GetPreferredSize().height() + kTitleTopInset + kTitleBottomInset;
  const int close_height = close_->visible() ? close_->height() : 0;
  insets += gfx::Insets(std::max(title_height, close_height), 0, 0, 0);
  return insets;
}

gfx::Size BubbleFrameView::GetPreferredSize() const {
  return GetSizeForClientSize(GetWidget()->client_view()->GetPreferredSize());
}

gfx::Size BubbleFrameView::GetMinimumSize() const {
  return GetSizeForClientSize(GetWidget()->client_view()->GetMinimumSize());
}

void BubbleFrameView::Layout() {
  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(GetTitleInsets());
  if (bounds.IsEmpty())
    return;

  // The close button top inset is actually smaller than the title top inset.
  close_->SetPosition(gfx::Point(bounds.right() - close_->width(),
                                 bounds.y() - 5));

  gfx::Size title_size(title_->GetPreferredSize());
  const int title_width = std::max(0, close_->x() - bounds.x());
  title_size.SetToMin(gfx::Size(title_width, title_size.height()));
  bounds.set_size(title_size);
  title_->SetBoundsRect(bounds);

  if (titlebar_extra_view_) {
    const int extra_width = close_->x() - title_->bounds().right();
    gfx::Size size = titlebar_extra_view_->GetPreferredSize();
    size.SetToMin(gfx::Size(std::max(0, extra_width), size.height()));
    gfx::Rect titlebar_extra_view_bounds(
        close_->x() - size.width(),
        bounds.y(),
        size.width(),
        bounds.height());
    titlebar_extra_view_bounds.Subtract(bounds);
    titlebar_extra_view_->SetBoundsRect(titlebar_extra_view_bounds);
  }
}

const char* BubbleFrameView::GetClassName() const {
  return kViewClassName;
}

void BubbleFrameView::ChildPreferredSizeChanged(View* child) {
  if (child == titlebar_extra_view_ || child == title_)
    Layout();
}

void BubbleFrameView::OnThemeChanged() {
  UpdateWindowTitle();
  ResetWindowControls();
  UpdateWindowIcon();
}

void BubbleFrameView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  if (bubble_border_ && bubble_border_->use_theme_background_color()) {
    bubble_border_->set_background_color(GetNativeTheme()->
        GetSystemColor(ui::NativeTheme::kColorId_DialogBackground));
    SchedulePaint();
  }
}

void BubbleFrameView::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == close_)
    GetWidget()->Close();
}

void BubbleFrameView::SetBubbleBorder(scoped_ptr<BubbleBorder> border) {
  bubble_border_ = border.get();
  SetBorder(border.PassAs<Border>());

  // Update the background, which relies on the border.
  set_background(new views::BubbleBackground(bubble_border_));
}

void BubbleFrameView::SetTitlebarExtraView(View* view) {
  DCHECK(view);
  DCHECK(!titlebar_extra_view_);
  AddChildView(view);
  titlebar_extra_view_ = view;
}

gfx::Rect BubbleFrameView::GetUpdatedWindowBounds(const gfx::Rect& anchor_rect,
                                                  gfx::Size client_size,
                                                  bool adjust_if_offscreen) {
  gfx::Size size(GetSizeForClientSize(client_size));

  const BubbleBorder::Arrow arrow = bubble_border_->arrow();
  if (adjust_if_offscreen && BubbleBorder::has_arrow(arrow)) {
    // Try to mirror the anchoring if the bubble does not fit on the screen.
    if (!bubble_border_->is_arrow_at_center(arrow)) {
      MirrorArrowIfOffScreen(true, anchor_rect, size);
      MirrorArrowIfOffScreen(false, anchor_rect, size);
    } else {
      const bool mirror_vertical = BubbleBorder::is_arrow_on_horizontal(arrow);
      MirrorArrowIfOffScreen(mirror_vertical, anchor_rect, size);
      OffsetArrowIfOffScreen(anchor_rect, size);
    }
  }

  // Calculate the bounds with the arrow in its updated location and offset.
  return bubble_border_->GetBounds(anchor_rect, size);
}

gfx::Rect BubbleFrameView::GetAvailableScreenBounds(const gfx::Rect& rect) {
  // The bubble attempts to fit within the current screen bounds.
  // TODO(scottmg): Native is wrong. http://crbug.com/133312
  return gfx::Screen::GetNativeScreen()->GetDisplayNearestPoint(
      rect.CenterPoint()).work_area();
}

void BubbleFrameView::MirrorArrowIfOffScreen(
    bool vertical,
    const gfx::Rect& anchor_rect,
    const gfx::Size& client_size) {
  // Check if the bounds don't fit on screen.
  gfx::Rect available_bounds(GetAvailableScreenBounds(anchor_rect));
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));
  if (GetOffScreenLength(available_bounds, window_bounds, vertical) > 0) {
    BubbleBorder::Arrow arrow = bubble_border()->arrow();
    // Mirror the arrow and get the new bounds.
    bubble_border_->set_arrow(
        vertical ? BubbleBorder::vertical_mirror(arrow) :
                   BubbleBorder::horizontal_mirror(arrow));
    gfx::Rect mirror_bounds =
        bubble_border_->GetBounds(anchor_rect, client_size);
    // Restore the original arrow if mirroring doesn't show more of the bubble.
    // Otherwise it should invoke parent's Layout() to layout the content based
    // on the new bubble border.
    if (GetOffScreenLength(available_bounds, mirror_bounds, vertical) >=
        GetOffScreenLength(available_bounds, window_bounds, vertical))
      bubble_border_->set_arrow(arrow);
    else if (parent())
      parent()->Layout();
  }
}

void BubbleFrameView::OffsetArrowIfOffScreen(const gfx::Rect& anchor_rect,
                                             const gfx::Size& client_size) {
  BubbleBorder::Arrow arrow = bubble_border()->arrow();
  DCHECK(BubbleBorder::is_arrow_at_center(arrow));

  // Get the desired bubble bounds without adjustment.
  bubble_border_->set_arrow_offset(0);
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));

  gfx::Rect available_bounds(GetAvailableScreenBounds(anchor_rect));
  if (available_bounds.IsEmpty() || available_bounds.Contains(window_bounds))
    return;

  // Calculate off-screen adjustment.
  const bool is_horizontal = BubbleBorder::is_arrow_on_horizontal(arrow);
  int offscreen_adjust = 0;
  if (is_horizontal) {
    if (window_bounds.x() < available_bounds.x())
      offscreen_adjust = available_bounds.x() - window_bounds.x();
    else if (window_bounds.right() > available_bounds.right())
      offscreen_adjust = available_bounds.right() - window_bounds.right();
  } else {
    if (window_bounds.y() < available_bounds.y())
      offscreen_adjust = available_bounds.y() - window_bounds.y();
    else if (window_bounds.bottom() > available_bounds.bottom())
      offscreen_adjust = available_bounds.bottom() - window_bounds.bottom();
  }

  // For center arrows, arrows are moved in the opposite direction of
  // |offscreen_adjust|, e.g. positive |offscreen_adjust| means bubble
  // window needs to be moved to the right and that means we need to move arrow
  // to the left, and that means negative offset.
  bubble_border_->set_arrow_offset(
      bubble_border_->GetArrowOffset(window_bounds.size()) - offscreen_adjust);
  if (offscreen_adjust)
    SchedulePaint();
}

gfx::Size BubbleFrameView::GetSizeForClientSize(
    const gfx::Size& client_size) const {
  // Accommodate the width of the title bar elements.
  int title_bar_width = GetInsets().width() + border()->GetInsets().width();
  if (!title_->text().empty())
    title_bar_width += kTitleLeftInset + title_->GetPreferredSize().width();
  if (close_->visible())
    title_bar_width += close_->width() + 1;
  if (titlebar_extra_view_ != NULL)
    title_bar_width += titlebar_extra_view_->GetPreferredSize().width();
  gfx::Size size(client_size);
  size.SetToMax(gfx::Size(title_bar_width, 0));
  const gfx::Insets insets(GetInsets());
  size.Enlarge(insets.width(), insets.height());
  return size;
}

}  // namespace views

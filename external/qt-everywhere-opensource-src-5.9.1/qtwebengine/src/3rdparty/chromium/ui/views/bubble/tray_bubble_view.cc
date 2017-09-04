// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/tray_bubble_view.h"

#include <algorithm>

#include "base/macros.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/bubble_window_targeter.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// The sampling time for mouse position changes in ms - which is roughly a frame
// time.
const int kFrameTimeInMS = 30;

BubbleBorder::Arrow GetArrowAlignment(
    TrayBubbleView::AnchorAlignment alignment) {
  if (alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
    return base::i18n::IsRTL() ? BubbleBorder::BOTTOM_LEFT
                               : BubbleBorder::BOTTOM_RIGHT;
  }
  if (alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT)
    return BubbleBorder::LEFT_BOTTOM;
  return BubbleBorder::RIGHT_BOTTOM;
}

}  // namespace

namespace internal {

// Detects any mouse movement. This is needed to detect mouse movements by the
// user over the bubble if the bubble got created underneath the cursor.
class MouseMoveDetectorHost : public MouseWatcherHost {
 public:
  MouseMoveDetectorHost();
  ~MouseMoveDetectorHost() override;

  bool Contains(const gfx::Point& screen_point, MouseEventType type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseMoveDetectorHost);
};

MouseMoveDetectorHost::MouseMoveDetectorHost() {
}

MouseMoveDetectorHost::~MouseMoveDetectorHost() {
}

bool MouseMoveDetectorHost::Contains(const gfx::Point& screen_point,
                                     MouseEventType type) {
  return false;
}

// This mask layer clips the bubble's content so that it does not overwrite the
// rounded bubble corners.
// TODO(miket): This does not work on Windows. Implement layer masking or
// alternate solutions if the TrayBubbleView is needed there in the future.
class TrayBubbleContentMask : public ui::LayerDelegate {
 public:
  explicit TrayBubbleContentMask(int corner_radius);
  ~TrayBubbleContentMask() override;

  ui::Layer* layer() { return &layer_; }

  // Overridden from LayerDelegate.
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;

 private:
  ui::Layer layer_;
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleContentMask);
};

TrayBubbleContentMask::TrayBubbleContentMask(int corner_radius)
    : layer_(ui::LAYER_TEXTURED),
      corner_radius_(corner_radius) {
  layer_.set_delegate(this);
  layer_.SetFillsBoundsOpaquely(false);
}

TrayBubbleContentMask::~TrayBubbleContentMask() {
  layer_.set_delegate(NULL);
}

void TrayBubbleContentMask::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  SkPaint paint;
  paint.setAlpha(255);
  paint.setStyle(SkPaint::kFill_Style);
  gfx::Rect rect(layer()->bounds().size());
  recorder.canvas()->DrawRoundRect(rect, corner_radius_, paint);
}

void TrayBubbleContentMask::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // Redrawing will take care of scale factor change.
}

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public BoxLayout {
 public:
  explicit BottomAlignedBoxLayout(TrayBubbleView* bubble_view)
      : BoxLayout(BoxLayout::kVertical, 0, 0, 0),
        bubble_view_(bubble_view) {
  }

  ~BottomAlignedBoxLayout() override {}

 private:
  void Layout(View* host) override {
    if (host->height() >= host->GetPreferredSize().height() ||
        !bubble_view_->is_gesture_dragging()) {
      BoxLayout::Layout(host);
      return;
    }

    int consumed_height = 0;
    for (int i = host->child_count() - 1;
        i >= 0 && consumed_height < host->height(); --i) {
      View* child = host->child_at(i);
      if (!child->visible())
        continue;
      gfx::Size size = child->GetPreferredSize();
      child->SetBounds(0, host->height() - consumed_height - size.height(),
          host->width(), size.height());
      consumed_height += size.height();
    }
  }

  TrayBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(BottomAlignedBoxLayout);
};

}  // namespace internal

using internal::TrayBubbleContentMask;
using internal::BottomAlignedBoxLayout;

TrayBubbleView::InitParams::InitParams(AnchorAlignment anchor_alignment,
                                       int min_width,
                                       int max_width)
    : anchor_alignment(anchor_alignment),
      min_width(min_width),
      max_width(max_width),
      max_height(0),
      can_activate(false),
      close_on_deactivate(true),
      bg_color(gfx::kPlaceholderColor) {}

TrayBubbleView::InitParams::InitParams(const InitParams& other) = default;

// static
TrayBubbleView* TrayBubbleView::Create(View* anchor,
                                       Delegate* delegate,
                                       InitParams* init_params) {
  return new TrayBubbleView(anchor, delegate, *init_params);
}

TrayBubbleView::TrayBubbleView(View* anchor,
                               Delegate* delegate,
                               const InitParams& init_params)
    : BubbleDialogDelegateView(anchor,
                               GetArrowAlignment(init_params.anchor_alignment)),
      params_(init_params),
      layout_(new BottomAlignedBoxLayout(this)),
      delegate_(delegate),
      preferred_width_(init_params.min_width),
      bubble_border_(new BubbleBorder(arrow(),
                                      BubbleBorder::BIG_SHADOW,
                                      init_params.bg_color)),
      owned_bubble_border_(bubble_border_),
      is_gesture_dragging_(false),
      mouse_actively_entered_(false) {
  bubble_border_->set_alignment(BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_border_->set_paint_arrow(BubbleBorder::PAINT_NONE);
  set_can_activate(params_.can_activate);
  DCHECK(anchor_widget());  // Computed by BubbleDialogDelegateView().
  set_notify_enter_exit_on_child(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  set_margins(gfx::Insets());
  SetPaintToLayer(true);

  bubble_content_mask_.reset(
      new TrayBubbleContentMask(bubble_border_->GetBorderCornerRadius()));

  layout_->SetDefaultFlex(1);
  SetLayoutManager(layout_);
}

TrayBubbleView::~TrayBubbleView() {
  mouse_watcher_.reset();
  // Inform host items (models) that their views are being destroyed.
  if (delegate_)
    delegate_->BubbleViewDestroyed();
}

void TrayBubbleView::InitializeAndShowBubble() {
  layer()->parent()->SetMaskLayer(bubble_content_mask_->layer());

  GetWidget()->Show();
  GetWidget()->GetNativeWindow()->SetEventTargeter(
      std::unique_ptr<ui::EventTargeter>(new BubbleWindowTargeter(this)));
  UpdateBubble();
}

void TrayBubbleView::UpdateBubble() {
  if (GetWidget()) {
    SizeToContents();
    bubble_content_mask_->layer()->SetBounds(layer()->bounds());
    GetWidget()->GetRootView()->SchedulePaint();
  }
}

void TrayBubbleView::SetMaxHeight(int height) {
  params_.max_height = height;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::SetBottomPadding(int padding) {
  layout_->set_inside_border_insets(gfx::Insets(0, 0, padding, 0));
}

void TrayBubbleView::SetWidth(int width) {
  width = std::max(std::min(width, params_.max_width), params_.min_width);
  if (preferred_width_ == width)
    return;
  preferred_width_ = width;
  if (GetWidget())
    SizeToContents();
}

gfx::Insets TrayBubbleView::GetBorderInsets() const {
  return bubble_border_->GetInsets();
}

int TrayBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void TrayBubbleView::OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                              Widget* bubble_widget) const {
  if (delegate_)
    delegate_->OnBeforeBubbleWidgetInit(anchor_widget(), bubble_widget, params);
}

NonClientFrameView* TrayBubbleView::CreateNonClientFrameView(Widget* widget) {
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(
      BubbleDialogDelegateView::CreateNonClientFrameView(widget));
  frame->SetBubbleBorder(std::move(owned_bubble_border_));
  return frame;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

base::string16 TrayBubbleView::GetAccessibleWindowTitle() const {
  return delegate_->GetAccessibleNameForBubble();
}

gfx::Size TrayBubbleView::GetPreferredSize() const {
  return gfx::Size(preferred_width_, GetHeightForWidth(preferred_width_));
}

gfx::Size TrayBubbleView::GetMaximumSize() const {
  gfx::Size size = GetPreferredSize();
  size.set_width(params_.max_width);
  return size;
}

int TrayBubbleView::GetHeightForWidth(int width) const {
  int height = GetInsets().height();
  width = std::max(width - GetInsets().width(), 0);
  for (int i = 0; i < child_count(); ++i) {
    const View* child = child_at(i);
    if (child->visible())
      height += child->GetHeightForWidth(width);
  }

  return (params_.max_height != 0) ?
      std::min(height, params_.max_height) : height;
}

void TrayBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_watcher_.reset();
  if (delegate_ && !(event.flags() & ui::EF_IS_SYNTHESIZED)) {
    // Coming here the user was actively moving the mouse over the bubble and
    // we inform the delegate that we entered. This will prevent the bubble
    // to auto close.
    delegate_->OnMouseEnteredView();
    mouse_actively_entered_ = true;
  } else {
    // Coming here the bubble got shown and the mouse was 'accidentally' over it
    // which is not a reason to prevent the bubble to auto close. As such we
    // do not call the delegate, but wait for the first mouse move within the
    // bubble. The used MouseWatcher will notify use of a movement and call
    // |MouseMovedOutOfHost|.
    mouse_watcher_.reset(new MouseWatcher(
        new views::internal::MouseMoveDetectorHost(),
        this));
    // Set the mouse sampling frequency to roughly a frame time so that the user
    // cannot see a lag.
    mouse_watcher_->set_notify_on_exit_time(
        base::TimeDelta::FromMilliseconds(kFrameTimeInMS));
    mouse_watcher_->Start();
  }
}

void TrayBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  // If there was a mouse watcher waiting for mouse movements we disable it
  // immediately since we now leave the bubble.
  mouse_watcher_.reset();
  // Do not notify the delegate of an exit if we never told it that we entered.
  if (delegate_ && mouse_actively_entered_)
    delegate_->OnMouseExitedView();
}

void TrayBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_ && CanActivate()) {
    node_data->role = ui::AX_ROLE_WINDOW;
    node_data->SetName(delegate_->GetAccessibleNameForBubble());
  }
}

void TrayBubbleView::MouseMovedOutOfHost() {
  // The mouse was accidentally over the bubble when it opened and the AutoClose
  // logic was not activated. Now that the user did move the mouse we tell the
  // delegate to disable AutoClose.
  delegate_->OnMouseEnteredView();
  mouse_actively_entered_ = true;
  mouse_watcher_->Stop();
}

void TrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void TrayBubbleView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    details.parent->SetPaintToLayer(true);
    details.parent->layer()->SetMasksToBounds(true);
  }
}

}  // namespace views

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/tray_bubble_view.h"

#include <algorithm>

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/bubble_window_targeter.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Inset the arrow a bit from the edge.
const int kArrowMinOffset = 20;
const int kBubbleSpacing = 20;

// The new theme adjusts the menus / bubbles to be flush with the shelf when
// there is no bubble. These are the offsets which need to be applied.
const int kArrowOffsetTopBottom = 4;
const int kArrowOffsetLeft = 9;
const int kArrowOffsetRight = -5;
const int kOffsetLeftRightForTopBottomOrientation = 5;

// The sampling time for mouse position changes in ms - which is roughly a frame
// time.
const int kFrameTimeInMS = 30;
}  // namespace

namespace views {

namespace internal {

// Detects any mouse movement. This is needed to detect mouse movements by the
// user over the bubble if the bubble got created underneath the cursor.
class MouseMoveDetectorHost : public MouseWatcherHost {
 public:
  MouseMoveDetectorHost();
  virtual ~MouseMoveDetectorHost();

  virtual bool Contains(const gfx::Point& screen_point,
                        MouseEventType type) OVERRIDE;
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

// Custom border for TrayBubbleView. Contains special logic for GetBounds()
// to stack bubbles with no arrows correctly. Also calculates the arrow offset.
class TrayBubbleBorder : public BubbleBorder {
 public:
  TrayBubbleBorder(View* owner,
                   View* anchor,
                   TrayBubbleView::InitParams params)
      : BubbleBorder(params.arrow, params.shadow, params.arrow_color),
        owner_(owner),
        anchor_(anchor),
        tray_arrow_offset_(params.arrow_offset),
        first_item_has_no_margin_(params.first_item_has_no_margin) {
    set_alignment(params.arrow_alignment);
    set_background_color(params.arrow_color);
    set_paint_arrow(params.arrow_paint_type);
  }

  virtual ~TrayBubbleBorder() {}

  // Overridden from BubbleBorder.
  // Sets the bubble on top of the anchor when it has no arrow.
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const OVERRIDE {
    if (has_arrow(arrow())) {
      gfx::Rect rect =
          BubbleBorder::GetBounds(position_relative_to, contents_size);
      if (first_item_has_no_margin_) {
        if (arrow() == BubbleBorder::BOTTOM_RIGHT ||
            arrow() == BubbleBorder::BOTTOM_LEFT) {
          rect.set_y(rect.y() + kArrowOffsetTopBottom);
          int rtl_factor = base::i18n::IsRTL() ? -1 : 1;
          rect.set_x(rect.x() +
                     rtl_factor * kOffsetLeftRightForTopBottomOrientation);
        } else if (arrow() == BubbleBorder::LEFT_BOTTOM) {
          rect.set_x(rect.x() + kArrowOffsetLeft);
        } else if (arrow() == BubbleBorder::RIGHT_BOTTOM) {
          rect.set_x(rect.x() + kArrowOffsetRight);
        }
      }
      return rect;
    }

    gfx::Size border_size(contents_size);
    gfx::Insets insets = GetInsets();
    border_size.Enlarge(insets.width(), insets.height());
    const int x = position_relative_to.x() +
        position_relative_to.width() / 2 - border_size.width() / 2;
    // Position the bubble on top of the anchor.
    const int y = position_relative_to.y() - border_size.height() +
        insets.height() - kBubbleSpacing;
    return gfx::Rect(x, y, border_size.width(), border_size.height());
  }

  void UpdateArrowOffset() {
    int arrow_offset = 0;
    if (arrow() == BubbleBorder::BOTTOM_RIGHT ||
        arrow() == BubbleBorder::BOTTOM_LEFT) {
      // Note: tray_arrow_offset_ is relative to the anchor widget.
      if (tray_arrow_offset_ ==
          TrayBubbleView::InitParams::kArrowDefaultOffset) {
        arrow_offset = kArrowMinOffset;
      } else {
        const int width = owner_->GetWidget()->GetContentsView()->width();
        gfx::Point pt(tray_arrow_offset_, 0);
        View::ConvertPointToScreen(anchor_->GetWidget()->GetRootView(), &pt);
        View::ConvertPointFromScreen(owner_->GetWidget()->GetRootView(), &pt);
        arrow_offset = pt.x();
        if (arrow() == BubbleBorder::BOTTOM_RIGHT)
          arrow_offset = width - arrow_offset;
        arrow_offset = std::max(arrow_offset, kArrowMinOffset);
      }
    } else {
      if (tray_arrow_offset_ ==
          TrayBubbleView::InitParams::kArrowDefaultOffset) {
        arrow_offset = kArrowMinOffset;
      } else {
        gfx::Point pt(0, tray_arrow_offset_);
        View::ConvertPointToScreen(anchor_->GetWidget()->GetRootView(), &pt);
        View::ConvertPointFromScreen(owner_->GetWidget()->GetRootView(), &pt);
        arrow_offset = pt.y();
        arrow_offset = std::max(arrow_offset, kArrowMinOffset);
      }
    }
    set_arrow_offset(arrow_offset);
  }

 private:
  View* owner_;
  View* anchor_;
  const int tray_arrow_offset_;

  // If true the first item should not get any additional spacing against the
  // anchor (without the bubble tip the bubble should be flush to the shelf).
  const bool first_item_has_no_margin_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleBorder);
};

// This mask layer clips the bubble's content so that it does not overwrite the
// rounded bubble corners.
// TODO(miket): This does not work on Windows. Implement layer masking or
// alternate solutions if the TrayBubbleView is needed there in the future.
class TrayBubbleContentMask : public ui::LayerDelegate {
 public:
  explicit TrayBubbleContentMask(int corner_radius);
  virtual ~TrayBubbleContentMask();

  ui::Layer* layer() { return &layer_; }

  // Overridden from LayerDelegate.
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE;

 private:
  ui::Layer layer_;
  SkScalar corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleContentMask);
};

TrayBubbleContentMask::TrayBubbleContentMask(int corner_radius)
    : layer_(ui::LAYER_TEXTURED),
      corner_radius_(corner_radius) {
  layer_.set_delegate(this);
}

TrayBubbleContentMask::~TrayBubbleContentMask() {
  layer_.set_delegate(NULL);
}

void TrayBubbleContentMask::OnPaintLayer(gfx::Canvas* canvas) {
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(layer()->bounds().size())),
                    corner_radius_, corner_radius_);
  SkPaint paint;
  paint.setAlpha(255);
  paint.setStyle(SkPaint::kFill_Style);
  canvas->DrawPath(path, paint);
}

void TrayBubbleContentMask::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // Redrawing will take care of scale factor change.
}

base::Closure TrayBubbleContentMask::PrepareForLayerBoundsChange() {
  return base::Closure();
}

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public BoxLayout {
 public:
  explicit BottomAlignedBoxLayout(TrayBubbleView* bubble_view)
      : BoxLayout(BoxLayout::kVertical, 0, 0, 0),
        bubble_view_(bubble_view) {
  }

  virtual ~BottomAlignedBoxLayout() {}

 private:
  virtual void Layout(View* host) OVERRIDE {
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

using internal::TrayBubbleBorder;
using internal::TrayBubbleContentMask;
using internal::BottomAlignedBoxLayout;

// static
const int TrayBubbleView::InitParams::kArrowDefaultOffset = -1;

TrayBubbleView::InitParams::InitParams(AnchorType anchor_type,
                                       AnchorAlignment anchor_alignment,
                                       int min_width,
                                       int max_width)
    : anchor_type(anchor_type),
      anchor_alignment(anchor_alignment),
      min_width(min_width),
      max_width(max_width),
      max_height(0),
      can_activate(false),
      close_on_deactivate(true),
      arrow_color(SK_ColorBLACK),
      first_item_has_no_margin(false),
      arrow(BubbleBorder::NONE),
      arrow_offset(kArrowDefaultOffset),
      arrow_paint_type(BubbleBorder::PAINT_NORMAL),
      shadow(BubbleBorder::BIG_SHADOW),
      arrow_alignment(BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE) {
}

// static
TrayBubbleView* TrayBubbleView::Create(gfx::NativeView parent_window,
                                       View* anchor,
                                       Delegate* delegate,
                                       InitParams* init_params) {
  // Set arrow here so that it can be passed to the BubbleView constructor.
  if (init_params->anchor_type == ANCHOR_TYPE_TRAY) {
    if (init_params->anchor_alignment == ANCHOR_ALIGNMENT_BOTTOM) {
      init_params->arrow = base::i18n::IsRTL() ?
          BubbleBorder::BOTTOM_LEFT : BubbleBorder::BOTTOM_RIGHT;
    } else if (init_params->anchor_alignment == ANCHOR_ALIGNMENT_TOP) {
      init_params->arrow = BubbleBorder::TOP_LEFT;
    } else if (init_params->anchor_alignment == ANCHOR_ALIGNMENT_LEFT) {
      init_params->arrow = BubbleBorder::LEFT_BOTTOM;
    } else {
      init_params->arrow = BubbleBorder::RIGHT_BOTTOM;
    }
  } else {
    init_params->arrow = BubbleBorder::NONE;
  }

  return new TrayBubbleView(parent_window, anchor, delegate, *init_params);
}

TrayBubbleView::TrayBubbleView(gfx::NativeView parent_window,
                               View* anchor,
                               Delegate* delegate,
                               const InitParams& init_params)
    : BubbleDelegateView(anchor, init_params.arrow),
      params_(init_params),
      delegate_(delegate),
      preferred_width_(init_params.min_width),
      bubble_border_(NULL),
      is_gesture_dragging_(false),
      mouse_actively_entered_(false) {
  set_parent_window(parent_window);
  set_notify_enter_exit_on_child(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  set_margins(gfx::Insets());
  bubble_border_ = new TrayBubbleBorder(this, GetAnchorView(), params_);
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(true);

  bubble_content_mask_.reset(
      new TrayBubbleContentMask(bubble_border_->GetBorderCornerRadius()));
}

TrayBubbleView::~TrayBubbleView() {
  mouse_watcher_.reset();
  // Inform host items (models) that their views are being destroyed.
  if (delegate_)
    delegate_->BubbleViewDestroyed();
}

void TrayBubbleView::InitializeAndShowBubble() {
  // Must occur after call to BubbleDelegateView::CreateBubble().
  SetAlignment(params_.arrow_alignment);
  bubble_border_->UpdateArrowOffset();

  layer()->parent()->SetMaskLayer(bubble_content_mask_->layer());

  GetWidget()->Show();
  GetWidget()->GetNativeWindow()->SetEventTargeter(
      scoped_ptr<ui::EventTargeter>(new BubbleWindowTargeter(this)));
  UpdateBubble();
}

void TrayBubbleView::UpdateBubble() {
  SizeToContents();
  bubble_content_mask_->layer()->SetBounds(layer()->bounds());
  GetWidget()->GetRootView()->SchedulePaint();
}

void TrayBubbleView::SetMaxHeight(int height) {
  params_.max_height = height;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::SetWidth(int width) {
  width = std::max(std::min(width, params_.max_width), params_.min_width);
  if (preferred_width_ == width)
    return;
  preferred_width_ = width;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::SetArrowPaintType(
    views::BubbleBorder::ArrowPaintType paint_type) {
  bubble_border_->set_paint_arrow(paint_type);
}

gfx::Insets TrayBubbleView::GetBorderInsets() const {
  return bubble_border_->GetInsets();
}

void TrayBubbleView::Init() {
  BoxLayout* layout = new BottomAlignedBoxLayout(this);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_FILL);
  SetLayoutManager(layout);
}

gfx::Rect TrayBubbleView::GetAnchorRect() const {
  if (!delegate_)
    return gfx::Rect();
  return delegate_->GetAnchorRect(anchor_widget(),
                                  params_.anchor_type,
                                  params_.anchor_alignment);
}

bool TrayBubbleView::CanActivate() const {
  return params_.can_activate;
}

NonClientFrameView* TrayBubbleView::CreateNonClientFrameView(Widget* widget) {
  BubbleFrameView* frame = new BubbleFrameView(margins());
  frame->SetBubbleBorder(scoped_ptr<views::BubbleBorder>(bubble_border_));
  return frame;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
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

void TrayBubbleView::GetAccessibleState(ui::AXViewState* state) {
  if (delegate_ && params_.can_activate) {
    state->role = ui::AX_ROLE_WINDOW;
    state->name = delegate_->GetAccessibleNameForBubble();
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
    details.parent->SetFillsBoundsOpaquely(true);
    details.parent->layer()->SetMasksToBounds(true);
  }
}

}  // namespace views

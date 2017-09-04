// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include "base/memory/ptr_util.h"
#include "ui/events/event.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_stub.h"
#include "ui/views/animation/square_ink_drop_ripple.h"

namespace views {
namespace {

// Size used for the default SquareInkDropRipple.
const int kInkDropSize = 24;

// The scale factor to compute the large size of the default
// SquareInkDropRipple.
const float kLargeInkDropScale = 1.333f;

// Default opacity of the ink drop when it is visible.
const float kInkDropVisibleOpacity = 0.175f;

}  // namespace

// static
const int InkDropHostView::kInkDropSmallCornerRadius = 2;
const int InkDropHostView::kInkDropLargeCornerRadius = 4;

// An EventHandler that is guaranteed to be invoked and is not prone to
// InkDropHostView descendents who do not call
// InkDropHostView::OnGestureEvent().  Only one instance of this class can exist
// at any given time for each ink drop host view.
//
// TODO(bruthig): Consider getting rid of this class.
class InkDropHostView::InkDropGestureHandler : public ui::EventHandler {
 public:
  explicit InkDropGestureHandler(InkDropHostView* host_view)
      : target_handler_(new ui::ScopedTargetHandler(host_view, this)),
        host_view_(host_view) {}

  ~InkDropGestureHandler() override {}

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (!host_view_->enabled())
      return;

    InkDropState current_ink_drop_state =
        host_view_->GetInkDrop()->GetTargetInkDropState();

    InkDropState ink_drop_state = InkDropState::HIDDEN;
    switch (event->type()) {
      case ui::ET_GESTURE_TAP_DOWN:
        if (current_ink_drop_state == InkDropState::ACTIVATED)
          return;
        ink_drop_state = InkDropState::ACTION_PENDING;
        // The ui::ET_GESTURE_TAP_DOWN event needs to be marked as handled so
        // that
        // subsequent events for the gesture are sent to |this|.
        event->SetHandled();
        break;
      case ui::ET_GESTURE_LONG_PRESS:
        if (current_ink_drop_state == InkDropState::ACTIVATED)
          return;
        ink_drop_state = InkDropState::ALTERNATE_ACTION_PENDING;
        break;
      case ui::ET_GESTURE_LONG_TAP:
        ink_drop_state = InkDropState::ALTERNATE_ACTION_TRIGGERED;
        break;
      case ui::ET_GESTURE_END:
      case ui::ET_GESTURE_SCROLL_BEGIN:
        if (current_ink_drop_state == InkDropState::ACTIVATED)
          return;
        ink_drop_state = InkDropState::HIDDEN;
        break;
      default:
        return;
    }

    if (ink_drop_state == InkDropState::HIDDEN &&
        (current_ink_drop_state == InkDropState::ACTION_TRIGGERED ||
         current_ink_drop_state == InkDropState::ALTERNATE_ACTION_TRIGGERED ||
         current_ink_drop_state == InkDropState::DEACTIVATED)) {
      // These InkDropStates automatically transition to the HIDDEN state so we
      // don't make an explicit call. Explicitly animating to HIDDEN in this
      // case would prematurely pre-empt these animations.
      return;
    }
    host_view_->AnimateInkDrop(ink_drop_state, event);
  }

 private:
  // Allows |this| to handle all GestureEvents on |host_view_|.
  std::unique_ptr<ui::ScopedTargetHandler> target_handler_;

  // The host view to cache ui::Events to when animating the ink drop.
  InkDropHostView* host_view_;

  DISALLOW_COPY_AND_ASSIGN(InkDropGestureHandler);
};

// static

gfx::Size InkDropHostView::CalculateLargeInkDropSize(
    const gfx::Size& small_size) {
  return gfx::ScaleToCeiledSize(gfx::Size(small_size), kLargeInkDropScale);
}

InkDropHostView::InkDropHostView()
    : ink_drop_mode_(InkDropMode::OFF),
      ink_drop_(nullptr),
      ink_drop_visible_opacity_(kInkDropVisibleOpacity),
      old_paint_to_layer_(false),
      destroying_(false) {}

InkDropHostView::~InkDropHostView() {
  // TODO(bruthig): Improve InkDropImpl to be safer about calling back to
  // potentially destroyed InkDropHosts and remove |destroying_|.
  destroying_ = true;
}

void InkDropHostView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  old_paint_to_layer_ = layer() != nullptr;
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  ink_drop_mask_ = CreateInkDropMask();
  if (ink_drop_mask_)
    ink_drop_layer->SetMaskLayer(ink_drop_mask_->layer());
  layer()->Add(ink_drop_layer);
  layer()->StackAtBottom(ink_drop_layer);
}

void InkDropHostView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  // No need to do anything when called during shutdown, and if a derived
  // class has overridden Add/RemoveInkDropLayer, running this implementation
  // would be wrong.
  if (destroying_)
    return;
  layer()->Remove(ink_drop_layer);
  // Layers safely handle destroying a mask layer before the masked layer.
  ink_drop_mask_.reset();
  SetPaintToLayer(old_paint_to_layer_);
}

std::unique_ptr<InkDrop> InkDropHostView::CreateInkDrop() {
  return CreateDefaultInkDropImpl();
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateInkDropRipple() const {
  return CreateDefaultInkDropRipple(GetLocalBounds().CenterPoint());
}

std::unique_ptr<InkDropHighlight> InkDropHostView::CreateInkDropHighlight()
    const {
  return CreateDefaultInkDropHighlight(
      gfx::RectF(GetLocalBounds()).CenterPoint());
}

std::unique_ptr<InkDropRipple> InkDropHostView::CreateDefaultInkDropRipple(
    const gfx::Point& center_point) const {
  const gfx::Size small_size(kInkDropSize, kInkDropSize);
  std::unique_ptr<InkDropRipple> ripple(new SquareInkDropRipple(
      CalculateLargeInkDropSize(small_size), kInkDropLargeCornerRadius,
      small_size, kInkDropSmallCornerRadius, center_point,
      GetInkDropBaseColor(), ink_drop_visible_opacity()));
  return ripple;
}

std::unique_ptr<InkDropHighlight>
InkDropHostView::CreateDefaultInkDropHighlight(
    const gfx::PointF& center_point) const {
  const gfx::Size small_size(kInkDropSize, kInkDropSize);
  std::unique_ptr<InkDropHighlight> highlight(
      new InkDropHighlight(small_size, kInkDropSmallCornerRadius, center_point,
                           GetInkDropBaseColor()));
  highlight->set_explode_size(CalculateLargeInkDropSize(small_size));
  return highlight;
}

void InkDropHostView::SetInkDropMode(InkDropMode ink_drop_mode) {
  ink_drop_mode_ = ink_drop_mode;
  ink_drop_ = nullptr;

  if (ink_drop_mode_ != InkDropMode::ON)
    gesture_handler_ = nullptr;
  else if (!gesture_handler_)
    gesture_handler_ = base::MakeUnique<InkDropGestureHandler>(this);
}

gfx::Point InkDropHostView::GetInkDropCenterBasedOnLastEvent() const {
  return last_ripple_triggering_event_
             ? last_ripple_triggering_event_->location()
             : GetLocalBounds().CenterPoint();
}

void InkDropHostView::AnimateInkDrop(InkDropState state,
                                     const ui::LocatedEvent* event) {
#if defined(OS_WIN)
  // On Windows, don't initiate ink-drops for touch/gesture events.
  // Additionally, certain event states should dismiss existing ink-drop
  // animations. If the state is already other than HIDDEN, presumably from
  // a mouse or keyboard event, then the state should be allowed. Conversely,
  // if the requested state is ACTIVATED, then it should always be allowed.
  if (event && (event->IsTouchEvent() || event->IsGestureEvent()) &&
      GetInkDrop()->GetTargetInkDropState() == InkDropState::HIDDEN &&
      state != InkDropState::ACTIVATED)
    return;
#endif
  last_ripple_triggering_event_.reset(
      event ? ui::Event::Clone(*event).release()->AsLocatedEvent() : nullptr);
  GetInkDrop()->AnimateToState(state);
}

void InkDropHostView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (ink_drop_)
    ink_drop_->HostSizeChanged(size());
  if (ink_drop_mask_)
    ink_drop_mask_->UpdateLayerSize(size());
}

void InkDropHostView::VisibilityChanged(View* starting_from, bool is_visible) {
  View::VisibilityChanged(starting_from, is_visible);
  if (GetWidget() && !is_visible) {
    GetInkDrop()->AnimateToState(InkDropState::HIDDEN);
    GetInkDrop()->SetHovered(false);
  }
}

void InkDropHostView::OnFocus() {
  views::View::OnFocus();
  GetInkDrop()->SetFocused(true);
}

void InkDropHostView::OnBlur() {
  views::View::OnBlur();
  GetInkDrop()->SetFocused(false);
}

void InkDropHostView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
      GetInkDrop()->SetHovered(true);
      break;
    case ui::ET_MOUSE_EXITED:
      GetInkDrop()->SetHovered(false);
      break;
    case ui::ET_MOUSE_DRAGGED:
      GetInkDrop()->SetHovered(GetLocalBounds().Contains(event->location()));
      break;
    default:
      break;
  }
  View::OnMouseEvent(event);
}

SkColor InkDropHostView::GetInkDropBaseColor() const {
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

std::unique_ptr<views::InkDropMask> InkDropHostView::CreateInkDropMask() const {
  return nullptr;
}

InkDrop* InkDropHostView::GetInkDrop() {
  if (!ink_drop_) {
    if (ink_drop_mode_ == InkDropMode::OFF)
      ink_drop_ = base::MakeUnique<InkDropStub>();
    else
      ink_drop_ = CreateInkDrop();
  }
  return ink_drop_.get();
}

std::unique_ptr<InkDropImpl> InkDropHostView::CreateDefaultInkDropImpl() {
  std::unique_ptr<InkDropImpl> ink_drop =
      base::MakeUnique<InkDropImpl>(this, size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE);
  return ink_drop;
}

std::unique_ptr<InkDropImpl>
InkDropHostView::CreateDefaultFloodFillInkDropImpl() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      InkDropHostView::CreateDefaultInkDropImpl();
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  return ink_drop;
}

}  // namespace views

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/linux/blimp_display_manager.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/app/compositor/browser_compositor.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/public/contents/blimp_contents_view.h"
#include "blimp/client/public/contents/blimp_navigation_controller.h"
#include "ui/events/event.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/events/gestures/motion_event_aura.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/size.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/x11/x11_window.h"

namespace blimp {
namespace {
constexpr int kPointer1Id = 0;
constexpr int kPointer2Id = 1;
constexpr int kZoomOffsetMultiplier = 4;
}  // namespace

namespace client {

BlimpDisplayManager::BlimpDisplayManager(
    BlimpDisplayManagerDelegate* delegate,
    CompositorDependencies* compositor_dependencies)
    : device_pixel_ratio_(1.f),
      delegate_(delegate),
      platform_event_source_(ui::PlatformEventSource::CreateDefault()),
      platform_window_(new ui::X11Window(this)) {
  compositor_ = base::MakeUnique<BrowserCompositor>(compositor_dependencies);
}

BlimpDisplayManager::~BlimpDisplayManager() = default;

void BlimpDisplayManager::SetWindowSize(const gfx::Size& window_size) {
  platform_window_->SetBounds(gfx::Rect(window_size));
}

void BlimpDisplayManager::SetBlimpContents(
    std::unique_ptr<BlimpContents> contents) {
  contents_ = std::move(contents);
  compositor_->SetContentLayer(contents_->GetView()->GetLayer());
  platform_window_->Show();
}

void BlimpDisplayManager::OnBoundsChanged(const gfx::Rect& new_bounds) {
  compositor_->SetSize(new_bounds.size());
  if (contents_) {
    contents_->GetView()->SetSizeAndScale(new_bounds.size(),
                                          device_pixel_ratio_);
  }
}

void BlimpDisplayManager::DispatchEvent(ui::Event* event) {
  if (event->IsMouseWheelEvent()) {
    DispatchMouseWheelEvent(event->AsMouseWheelEvent());
  } else if (event->IsMouseEvent()) {
    DispatchMouseEvent(event->AsMouseEvent());
  }
}

void BlimpDisplayManager::DispatchMotionEventAura(
    ui::MotionEventAura* touch_event_stream,
    ui::EventType event_type,
    int pointer_id,
    int pointer_x,
    int pointer_y) {
  DCHECK(contents_);

  touch_event_stream->OnTouch(
      ui::TouchEvent(event_type, gfx::Point(pointer_x, pointer_y), pointer_id,
                     base::TimeTicks::Now()));
  contents_->GetView()->OnTouchEvent(*touch_event_stream);
}

void BlimpDisplayManager::DispatchMouseWheelEvent(
    ui::MouseWheelEvent* wheel_event) {
  // In order to better simulate the Android experience on the Linux client
  // we convert mousewheel scrolling events into pinch/zoom events.
  bool zoom_out = wheel_event->y_offset() < 0;
  Zoom(wheel_event->x(), wheel_event->y(), wheel_event->y_offset(), zoom_out);
}

void BlimpDisplayManager::Zoom(int pointer_x,
                               int pointer_y,
                               int y_offset,
                               bool zoom_out) {
  int pinch_distance = abs(y_offset * kZoomOffsetMultiplier);
  int pointer2_y = pointer_y;
  if (zoom_out) {
    // Pointers start apart when zooming out.
    pointer2_y -= pinch_distance;
  }

  ui::MotionEventAura touch_event_stream;

  // Place two pointers.
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_PRESSED,
                          kPointer1Id, pointer_x, pointer_y);
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_PRESSED,
                          kPointer2Id, pointer_x, pointer2_y);

  // Pinch pointers. Need to simulate gradually in order for the event to be
  // properly processed.
  for (int pointer2_y_end = pointer2_y + pinch_distance;
       pointer2_y < pointer2_y_end; ++pointer2_y) {
    DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_MOVED,
                            kPointer2Id, pointer_x, pointer2_y);
  }

  // Remove pointers.
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_RELEASED,
                          kPointer1Id, pointer_x, pointer_y);
  DispatchMotionEventAura(&touch_event_stream, ui::ET_TOUCH_RELEASED,
                          kPointer2Id, pointer_x, pointer2_y);
}

ui::MotionEvent::Action MouseEventToAction(ui::MouseEvent* mouse_event) {
  switch (mouse_event->type()) {
    case ui::ET_MOUSE_PRESSED:
      return ui::MotionEvent::ACTION_DOWN;
    case ui::ET_MOUSE_RELEASED:
      return ui::MotionEvent::ACTION_UP;
    case ui::ET_MOUSE_DRAGGED:
      return ui::MotionEvent::ACTION_MOVE;
    default:
      return ui::MotionEvent::ACTION_NONE;
  }
}

void BlimpDisplayManager::DispatchMouseEvent(ui::MouseEvent* mouse_event) {
  ui::MotionEvent::Action action = MouseEventToAction(mouse_event);

  if (action != ui::MotionEvent::ACTION_NONE &&
      mouse_event->IsOnlyLeftMouseButton()) {
    DCHECK(contents_);

    ui::PointerProperties mouse_properties(mouse_event->x(), mouse_event->y(),
                                           0);
    ui::MotionEventGeneric motion_event(action, mouse_event->time_stamp(),
                                        mouse_properties);
    contents_->GetView()->OnTouchEvent(motion_event);
  }
}

void BlimpDisplayManager::OnCloseRequest() {
  DCHECK(contents_);

  contents_->Hide();
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  platform_window_->Close();
}

void BlimpDisplayManager::OnClosed() {
  if (delegate_)
    delegate_->OnClosed();
}

void BlimpDisplayManager::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  DCHECK(contents_);

  device_pixel_ratio_ = device_pixel_ratio;
  contents_->GetView()->SetSizeAndScale(platform_window_->GetBounds().size(),
                                        device_pixel_ratio_);

  if (widget != gfx::kNullAcceleratedWidget) {
    contents_->Show();
    compositor_->SetAcceleratedWidget(widget);
  }
}

void BlimpDisplayManager::OnAcceleratedWidgetDestroyed() {
  DCHECK(contents_);
  contents_->Hide();
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

}  // namespace client
}  // namespace blimp

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/gles2_client_impl.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <math.h>
#include <stdlib.h>

#include "mojo/public/c/gles2/gles2.h"
#include "ui/events/event_constants.h"

namespace mojo {
namespace examples {
namespace {

float CalculateDragDistance(const gfx::PointF& start, const Point& end) {
  return hypot(start.x() - end.x, start.y() - end.y);
}

float GetRandomColor() {
  return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

}

GLES2ClientImpl::GLES2ClientImpl(CommandBufferPtr command_buffer)
    : getting_animation_frames_(false) {
  context_ = MojoGLES2CreateContext(
      command_buffer.PassMessagePipe().release().value(),
      &ContextLostThunk,
      &DrawAnimationFrameThunk,
      this);
  MojoGLES2MakeCurrent(context_);
}

GLES2ClientImpl::~GLES2ClientImpl() {
  MojoGLES2DestroyContext(context_);
}

void GLES2ClientImpl::SetSize(const Size& size) {
  size_ = gfx::Size(size.width, size.height);
  if (size_.IsEmpty())
    return;
  cube_.Init(size_.width(), size_.height());
  RequestAnimationFrames();
}

void GLES2ClientImpl::HandleInputEvent(const Event& event) {
  switch (event.action) {
  case ui::ET_MOUSE_PRESSED:
  case ui::ET_TOUCH_PRESSED:
    if (event.flags & ui::EF_RIGHT_MOUSE_BUTTON)
      break;
    CancelAnimationFrames();
    capture_point_.SetPoint(event.location->x, event.location->y);
    last_drag_point_ = capture_point_;
    drag_start_time_ = GetTimeTicksNow();
    break;
  case ui::ET_MOUSE_DRAGGED:
  case ui::ET_TOUCH_MOVED:
    if (event.flags & ui::EF_RIGHT_MOUSE_BUTTON)
      break;
    if (!getting_animation_frames_) {
      int direction = event.location->y < last_drag_point_.y() ||
          event.location->x > last_drag_point_.x() ? 1 : -1;
      cube_.set_direction(direction);
      cube_.UpdateForDragDistance(
          CalculateDragDistance(last_drag_point_, *event.location));
      cube_.Draw();
      MojoGLES2SwapBuffers();

      last_drag_point_.SetPoint(event.location->x, event.location->y);
    }
    break;
  case ui::ET_MOUSE_RELEASED:
  case ui::ET_TOUCH_RELEASED: {
    if (event.flags & ui::EF_RIGHT_MOUSE_BUTTON) {
      cube_.set_color(GetRandomColor(), GetRandomColor(), GetRandomColor());
      break;
    }
    MojoTimeTicks offset = GetTimeTicksNow() - drag_start_time_;
    float delta = static_cast<float>(offset) / 1000000.;
    cube_.SetFlingMultiplier(
        CalculateDragDistance(capture_point_, *event.location),
        delta);

    capture_point_ = last_drag_point_ = gfx::PointF();
    RequestAnimationFrames();
    break;
  }
  default:
    break;
  }
}

void GLES2ClientImpl::ContextLost() {
  CancelAnimationFrames();
}

void GLES2ClientImpl::ContextLostThunk(void* closure) {
  static_cast<GLES2ClientImpl*>(closure)->ContextLost();
}

void GLES2ClientImpl::DrawAnimationFrame() {
  MojoTimeTicks now = GetTimeTicksNow();
  MojoTimeTicks offset = now - last_time_;
  float delta = static_cast<float>(offset) / 1000000.;
  last_time_ = now;
  cube_.UpdateForTimeDelta(delta);
  cube_.Draw();

  MojoGLES2SwapBuffers();
}

void GLES2ClientImpl::DrawAnimationFrameThunk(void* closure) {
  static_cast<GLES2ClientImpl*>(closure)->DrawAnimationFrame();
}

void GLES2ClientImpl::RequestAnimationFrames() {
  getting_animation_frames_ = true;
  MojoGLES2RequestAnimationFrames(context_);
  last_time_ = GetTimeTicksNow();
}

void GLES2ClientImpl::CancelAnimationFrames() {
  getting_animation_frames_ = false;
  MojoGLES2CancelAnimationFrames(context_);
}

}  // namespace examples
}  // namespace mojo

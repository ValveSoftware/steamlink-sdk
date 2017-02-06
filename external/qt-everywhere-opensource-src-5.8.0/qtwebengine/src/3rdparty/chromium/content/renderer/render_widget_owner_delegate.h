// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_OWNER_DELEGATE_H_
#define CONTENT_RENDERER_RENDER_WIDGET_OWNER_DELEGATE_H_

#include "content/common/content_export.h"

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
}

namespace gfx {
class Point;
}

namespace content {

//
// RenderWidgetOwnerDelegate
//
//  An interface implemented by an object owning a RenderWidget. This is
//  intended to be temporary until the RenderViewImpl and RenderWidget classes
//  are disentangled; see http://crbug.com/583347 and http://crbug.com/478281.
class CONTENT_EXPORT RenderWidgetOwnerDelegate {
 public:
  // The RenderWidget set a color profile.
  virtual void RenderWidgetDidSetColorProfile(
      const std::vector<char>& color_profile) = 0;

  // As in RenderWidgetInputHandlerDelegate.
  virtual void RenderWidgetFocusChangeComplete() = 0;
  virtual bool DoesRenderWidgetHaveTouchEventHandlersAt(
      const gfx::Point& point) const = 0;
  virtual void RenderWidgetDidHandleKeyEvent() = 0;
  virtual bool RenderWidgetWillHandleGestureEvent(
      const blink::WebGestureEvent& event) = 0;
  virtual bool RenderWidgetWillHandleMouseEvent(
      const blink::WebMouseEvent& event) = 0;

  // Painting notifications. DidFlushPaint happens once we've received the ACK
  // that the screen has been updated.
  virtual void RenderWidgetDidFlushPaint() = 0;

 protected:
  virtual ~RenderWidgetOwnerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_OWNER_DELEGATE_H_

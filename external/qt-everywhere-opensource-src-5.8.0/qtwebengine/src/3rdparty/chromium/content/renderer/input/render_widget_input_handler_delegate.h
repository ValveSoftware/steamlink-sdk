// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_DELEGATE_H_
#define CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_DELEGATE_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/common/input/input_event_ack.h"

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace gfx {
class Point;
class Vector2dF;
}

namespace content {

class RenderWidgetInputHandler;

enum class ShowIme { IF_NEEDED, HIDE_IME };

enum class ChangeSource {
  FROM_NON_IME,
  FROM_IME,
};

// Consumers of RenderWidgetInputHandler implement this delegate in order to
// transport input handling information across processes.
class CONTENT_EXPORT RenderWidgetInputHandlerDelegate {
 public:
  // Called when animations due to focus change have completed (if any).
  virtual void FocusChangeComplete() = 0;

  // Check whether the WebWidget has any touch event handlers registered
  // at the given point.
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const = 0;

  // Called to forward a gesture event to the compositor thread, to effect
  // the elastic overscroll effect.
  virtual void ObserveGestureEventAndResult(
      const blink::WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      bool event_processed) = 0;

  // Notifies that a key event was just handled.
  virtual void OnDidHandleKeyEvent() = 0;

  // Notifies that an overscroll was completed from Blink.
  virtual void OnDidOverscroll(const DidOverscrollParams& params) = 0;

  // Called when an ACK is ready to be sent to the input event provider.
  virtual void OnInputEventAck(
      std::unique_ptr<InputEventAck> input_event_ack) = 0;

  // Called when an event with a notify dispatch type
  // (DISPATCH_TYPE_*_NOTIFY_MAIN) of |handled_type| has been processed
  // by the main thread.
  virtual void NotifyInputEventHandled(blink::WebInputEvent::Type handled_type,
                                       InputEventAckState ack_result) = 0;

  // Notifies the delegate of the |input_handler| managing it.
  virtual void SetInputHandler(RenderWidgetInputHandler* input_handler) = 0;

  // |show_ime| should be ShowIme::IF_NEEDED iff the update may cause the ime to
  // be displayed, e.g. after a tap on an input field on mobile.
  // |change_source| should be ChangeSource::FROM_NON_IME when the renderer has
  // to wait for the browser to acknowledge the change before the renderer
  // handles any more IME events. This is when the text change did not originate
  // from the IME in the browser side, such as changes by JavaScript or
  // autofill.
  virtual void UpdateTextInputState(ShowIme show_ime,
                                    ChangeSource change_source) = 0;

  // Notifies that a gesture event is about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit.
  virtual bool WillHandleGestureEvent(const blink::WebGestureEvent& event) = 0;

  // Notifies that a mouse event is about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit or trigger DidHandleMouseEvent().
  virtual bool WillHandleMouseEvent(const blink::WebMouseEvent& event) = 0;

 protected:
  virtual ~RenderWidgetInputHandlerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_DELEGATE_H_

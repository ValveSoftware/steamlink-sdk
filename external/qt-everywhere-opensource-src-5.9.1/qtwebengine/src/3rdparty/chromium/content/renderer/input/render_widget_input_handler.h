// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_
#define CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/input/input_event_ack.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/blink/did_overscroll_params.h"

namespace blink {
struct WebFloatPoint;
struct WebFloatSize;
}

namespace ui {
class LatencyInfo;
}

namespace content {

class RenderWidget;
class RenderWidgetInputHandlerDelegate;

// RenderWidgetInputHandler is an IPC-agnostic input handling class.
// IPC transport code should live in RenderWidget or RenderWidgetMusConnection.
class CONTENT_EXPORT RenderWidgetInputHandler {
 public:
  RenderWidgetInputHandler(RenderWidgetInputHandlerDelegate* delegate,
                           RenderWidget* widget);
  virtual ~RenderWidgetInputHandler();

  // Handle input events from the input event provider.
  virtual void HandleInputEvent(const blink::WebInputEvent& input_event,
                                const ui::LatencyInfo& latency_info,
                                InputEventDispatchType dispatch_type);

  // Handle overscroll from Blink.
  void DidOverscrollFromBlink(
      const blink::WebFloatSize& overscrollDelta,
      const blink::WebFloatSize& accumulatedOverscroll,
      const blink::WebFloatPoint& position,
      const blink::WebFloatSize& velocity);

  bool handling_input_event() const { return handling_input_event_; }
  void set_handling_input_event(bool handling_input_event) {
    handling_input_event_ = handling_input_event;
  }

  blink::WebInputEvent::Type handling_event_type() const {
    return handling_event_type_;
  }

  ui::MenuSourceType context_menu_source_type() const {
    return context_menu_source_type_;
  }
  void set_context_menu_source_type(ui::MenuSourceType source_type) {
    context_menu_source_type_ = source_type;
  }

 private:
  RenderWidgetInputHandlerDelegate* const delegate_;

  RenderWidget* const widget_;

  // Are we currently handling an input event?
  bool handling_input_event_;

  // Used to intercept overscroll notifications while an event is being
  // handled. If the event causes overscroll, the overscroll metadata can be
  // bundled in the event ack, saving an IPC.  Note that we must continue
  // supporting overscroll IPC notifications due to fling animation updates.
  std::unique_ptr<ui::DidOverscrollParams>* handling_event_overscroll_;

  // Type of the input event we are currently handling.
  blink::WebInputEvent::Type handling_event_type_;

  ui::MenuSourceType context_menu_source_type_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  bool suppress_next_char_events_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetInputHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_RENDER_WIDGET_INPUT_HANDLER_H_

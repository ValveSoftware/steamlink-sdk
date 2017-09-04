// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_CLIENT_H_
#define CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_CLIENT_H_

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/scoped_web_input_event.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
class InputHandler;
}

namespace ui {
class LatencyInfo;
class SynchronousInputHandlerProxy;
struct DidOverscrollParams;
}

namespace content {
class InputHandlerManager;

class CONTENT_EXPORT InputHandlerManagerClient {
 public:
  virtual ~InputHandlerManagerClient() {}

  // Called from the main thread.
  virtual void SetInputHandlerManager(InputHandlerManager*) = 0;
  virtual void NotifyInputEventHandled(int routing_id,
                                       blink::WebInputEvent::Type type,
                                       InputEventAckState ack_result) = 0;
  virtual void ProcessRafAlignedInput(int routing_id) = 0;

  // Called from the compositor thread.
  virtual void RegisterRoutingID(int routing_id) = 0;
  virtual void UnregisterRoutingID(int routing_id) = 0;

  // |HandleInputEvent| will respond to overscroll by calling the passed in
  // callback.
  // Otherwise |DidOverscroll| will be fired.
  virtual void DidOverscroll(int routing_id,
                             const ui::DidOverscrollParams& params) = 0;
  virtual void DidStopFlinging(int routing_id) = 0;
  virtual void DispatchNonBlockingEventToMainThread(
      int routing_id,
      ui::ScopedWebInputEvent event,
      const ui::LatencyInfo& latency_info) = 0;

 protected:
  InputHandlerManagerClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandlerManagerClient);
};

class CONTENT_EXPORT SynchronousInputHandlerProxyClient {
 public:
  virtual ~SynchronousInputHandlerProxyClient() {}

  virtual void DidAddSynchronousHandlerProxy(
      int routing_id,
      ui::SynchronousInputHandlerProxy* synchronous_handler) = 0;
  virtual void DidRemoveSynchronousHandlerProxy(int routing_id) = 0;

 protected:
  SynchronousInputHandlerProxyClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SynchronousInputHandlerProxyClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_CLIENT_H_

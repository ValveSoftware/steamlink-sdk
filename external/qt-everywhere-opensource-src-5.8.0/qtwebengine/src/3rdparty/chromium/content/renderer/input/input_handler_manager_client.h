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
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {
class LatencyInfo;
}

namespace cc {
class InputHandler;
}

namespace ui {
class SynchronousInputHandlerProxy;
}

namespace content {
struct DidOverscrollParams;

class CONTENT_EXPORT InputHandlerManagerClient {
 public:
  virtual ~InputHandlerManagerClient() {}

  // The Manager will supply a |handler| when bound to the client. This is valid
  // until the manager shuts down, at which point it supplies a null |handler|.
  // The client should only makes calls to |handler| on the compositor thread.
  typedef base::Callback<
      InputEventAckState(int /*routing_id*/,
                         const blink::WebInputEvent*,
                         ui::LatencyInfo* latency_info)> Handler;

  // Called from the main thread.
  virtual void SetBoundHandler(const Handler& handler) = 0;

  // Called from the compositor thread.
  virtual void RegisterRoutingID(int routing_id) = 0;
  virtual void UnregisterRoutingID(int routing_id) = 0;
  virtual void DidOverscroll(int routing_id,
                             const DidOverscrollParams& params) = 0;
  virtual void DidStartFlinging(int routing_id) = 0;
  virtual void DidStopFlinging(int routing_id) = 0;
  virtual void NotifyInputEventHandled(int routing_id,
                                       blink::WebInputEvent::Type type,
                                       InputEventAckState ack_result) = 0;

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

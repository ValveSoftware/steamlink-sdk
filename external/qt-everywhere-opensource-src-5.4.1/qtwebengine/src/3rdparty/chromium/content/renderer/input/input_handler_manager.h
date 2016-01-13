// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_H_
#define CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/renderer/render_view_impl.h"

namespace base {
class MessageLoopProxy;
}

namespace cc {
class InputHandler;
}

namespace blink {
class WebInputEvent;
}

namespace content {

class InputHandlerWrapper;
class InputHandlerManagerClient;
struct DidOverscrollParams;

// InputHandlerManager class manages InputHandlerProxy instances for
// the WebViews in this renderer.
class InputHandlerManager {
 public:
  // |message_loop_proxy| is the MessageLoopProxy of the compositor thread. Both
  // the underlying MessageLoop and supplied |client| must outlive this object.
  InputHandlerManager(
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      InputHandlerManagerClient* client);
  ~InputHandlerManager();

  // Callable from the main thread only.
  void AddInputHandler(
      int routing_id,
      const base::WeakPtr<cc::InputHandler>& input_handler,
      const base::WeakPtr<RenderViewImpl>& render_view_impl);

  // Callback only from the compositor's thread.
  void RemoveInputHandler(int routing_id);

  // Called from the compositor's thread.
  InputEventAckState HandleInputEvent(int routing_id,
                                      const blink::WebInputEvent* input_event,
                                      ui::LatencyInfo* latency_info);

  // Called from the compositor's thread.
  void DidOverscroll(int routing_id, const DidOverscrollParams& params);

  // Called from the compositor's thread.
  void DidStopFlinging(int routing_id);

 private:
  // Called from the compositor's thread.
  void AddInputHandlerOnCompositorThread(
      int routing_id,
      const scoped_refptr<base::MessageLoopProxy>& main_loop,
      const base::WeakPtr<cc::InputHandler>& input_handler,
      const base::WeakPtr<RenderViewImpl>& render_view_impl);

  typedef base::ScopedPtrHashMap<int,  // routing_id
                                 InputHandlerWrapper> InputHandlerMap;
  InputHandlerMap input_handlers_;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  InputHandlerManagerClient* client_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_MANAGER_H_

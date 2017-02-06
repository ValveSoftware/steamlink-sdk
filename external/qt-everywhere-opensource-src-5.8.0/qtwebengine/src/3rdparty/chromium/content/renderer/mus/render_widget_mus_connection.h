// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MUS_RENDER_WIDGET_MUS_CONNECTION_H_
#define CONTENT_RENDERER_MUS_RENDER_WIDGET_MUS_CONNECTION_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/output/output_surface.h"
#include "components/mus/public/cpp/window_surface.h"
#include "content/common/content_export.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/mus/compositor_mus_connection.h"

namespace content {

class InputHandlerManager;

// Use on main thread.
class CONTENT_EXPORT RenderWidgetMusConnection
    : public RenderWidgetInputHandlerDelegate {
 public:
  // Bind to a WindowTreeClient request.
  void Bind(mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request);

  // Create a cc output surface.
  std::unique_ptr<cc::OutputSurface> CreateOutputSurface();

  static RenderWidgetMusConnection* Get(int routing_id);

  // Get the connection from a routing_id, if the connection doesn't exist,
  // a new connection will be created.
  static RenderWidgetMusConnection* GetOrCreate(int routing_id);

 private:
  friend class CompositorMusConnection;
  friend class CompositorMusConnectionTest;

  explicit RenderWidgetMusConnection(int routing_id);
  ~RenderWidgetMusConnection() override;

  // RenderWidgetInputHandlerDelegate implementation:
  void FocusChangeComplete() override;
  bool HasTouchEventHandlersAt(const gfx::Point& point) const override;
  void ObserveGestureEventAndResult(const blink::WebGestureEvent& gesture_event,
                                    const gfx::Vector2dF& gesture_unused_delta,
                                    bool event_processed) override;
  void OnDidHandleKeyEvent() override;
  void OnDidOverscroll(const DidOverscrollParams& params) override;
  void OnInputEventAck(std::unique_ptr<InputEventAck> input_event_ack) override;
  void NotifyInputEventHandled(blink::WebInputEvent::Type handled_type,
                               InputEventAckState ack_result) override;
  void SetInputHandler(RenderWidgetInputHandler* input_handler) override;
  void UpdateTextInputState(ShowIme show_ime,
                            ChangeSource change_source) override;
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override;
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event) override;

  void OnConnectionLost();
  void OnWindowInputEvent(
      std::unique_ptr<blink::WebInputEvent> input_event,
      const base::Callback<void(mus::mojom::EventResult)>& ack);

  const int routing_id_;
  RenderWidgetInputHandler* input_handler_;
  std::unique_ptr<mus::WindowSurfaceBinding> window_surface_binding_;
  scoped_refptr<CompositorMusConnection> compositor_mus_connection_;

  base::Callback<void(mus::mojom::EventResult)> pending_ack_;

  // Used to verify single threaded access.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetMusConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MUS_RENDER_WIDGET_MUS_CONNECTION_H_

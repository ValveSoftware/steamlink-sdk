// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_
#define CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "services/ui/public/cpp/input_event_handler.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/scoped_web_input_event.h"
#include "ui/events/gestures/motion_event_aura.h"

namespace ui {
struct DidOverscrollParams;
}

namespace content {

class InputHandlerManager;

// CompositorMusConnection manages the connection to the Mandoline UI Service
// (Mus) on the compositor thread. For operations that need to happen on the
// main thread, CompositorMusConnection deals with passing information across
// threads. CompositorMusConnection is constructed on the main thread. By
// default all other methods are assumed to run on the compositor thread unless
// explicited suffixed with OnMainThread.
class CONTENT_EXPORT CompositorMusConnection
    : NON_EXPORTED_BASE(public ui::WindowTreeClientDelegate),
      NON_EXPORTED_BASE(public ui::InputEventHandler),
      public base::RefCountedThreadSafe<CompositorMusConnection> {
 public:
  // Created on main thread.
  CompositorMusConnection(
      int routing_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request,
      InputHandlerManager* input_handler_manager);

  // Attaches the provided |compositor_frame_sink_binding| with the ui::Window
  // for the renderer once it becomes available.
  void AttachCompositorFrameSinkOnMainThread(
      std::unique_ptr<ui::WindowCompositorFrameSinkBinding>
          compositor_frame_sink_binding);

 private:
  friend class CompositorMusConnectionTest;
  friend class base::RefCountedThreadSafe<CompositorMusConnection>;

  ~CompositorMusConnection() override;

  void AttachCompositorFrameSinkOnCompositorThread(
      std::unique_ptr<ui::WindowCompositorFrameSinkBinding>
          compositor_frame_sink_binding);

  void CreateWindowTreeClientOnCompositorThread(
      ui::mojom::WindowTreeClientRequest request);

  void OnConnectionLostOnMainThread();

  void OnWindowInputEventOnMainThread(
      ui::ScopedWebInputEvent web_event,
      const base::Callback<void(ui::mojom::EventResult)>& ack);

  void OnWindowInputEventAckOnMainThread(
      const base::Callback<void(ui::mojom::EventResult)>& ack,
      ui::mojom::EventResult result);

  std::unique_ptr<blink::WebInputEvent> Convert(const ui::Event& event);

  void DeleteWindowTreeClient();

  // WindowTreeClientDelegate implementation:
  void OnEmbed(ui::Window* root) override;
  void OnEmbedRootDestroyed(ui::Window* root) override;
  void OnLostConnection(ui::WindowTreeClient* client) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target) override;

  // InputEventHandler implementation:
  void OnWindowInputEvent(
      ui::Window* window,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(ui::mojom::EventResult)>>*
          ack_callback) override;
  void DidHandleWindowInputEventAndOverscroll(
      std::unique_ptr<base::Callback<void(ui::mojom::EventResult)>>
          ack_callback,
      InputEventAckState ack_state,
      ui::ScopedWebInputEvent web_event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params);

  const int routing_id_;
  // Use this lock when accessing |window_tree_client_|. Lock exists solely for
  // DCHECK in destructor.
  base::Lock window_tree_client_lock_;
  std::unique_ptr<ui::WindowTreeClient> window_tree_client_;
  ui::Window* root_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  InputHandlerManager* const input_handler_manager_;
  std::unique_ptr<ui::WindowCompositorFrameSinkBinding>
      window_compositor_frame_sink_binding_;

  // Stores the current state of the active pointers targeting this object.
  ui::MotionEventAura pointer_state_;

  DISALLOW_COPY_AND_ASSIGN(CompositorMusConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_

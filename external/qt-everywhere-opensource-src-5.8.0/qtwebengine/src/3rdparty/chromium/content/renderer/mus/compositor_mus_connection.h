// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_
#define CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/cpp/window_tree_client_delegate.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class InputHandlerManager;

// CompositorMusConnection manages the connection to the Mandoline UI Service
// (Mus) on the compositor thread. For operations that need to happen on the
// main thread, CompositorMusConnection deals with passing information across
// threads. CompositorMusConnection is constructed on the main thread. By
// default all other methods are assumed to run on the compositor thread unless
// explicited suffixed with OnMainThread.
class CONTENT_EXPORT CompositorMusConnection
    : NON_EXPORTED_BASE(public mus::WindowTreeClientDelegate),
      NON_EXPORTED_BASE(public mus::InputEventHandler),
      public base::RefCountedThreadSafe<CompositorMusConnection> {
 public:
  // Created on main thread.
  CompositorMusConnection(
      int routing_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request,
      InputHandlerManager* input_handler_manager);

  // Attaches the provided |surface_binding| with the mus::Window for the
  // renderer once it becomes available.
  void AttachSurfaceOnMainThread(
      std::unique_ptr<mus::WindowSurfaceBinding> surface_binding);

 private:
  friend class CompositorMusConnectionTest;
  friend class base::RefCountedThreadSafe<CompositorMusConnection>;

  ~CompositorMusConnection() override;

  void AttachSurfaceOnCompositorThread(
      std::unique_ptr<mus::WindowSurfaceBinding> surface_binding);

  void CreateWindowTreeClientOnCompositorThread(
      mus::mojom::WindowTreeClientRequest request);

  void OnConnectionLostOnMainThread();

  void OnWindowInputEventOnMainThread(
      std::unique_ptr<blink::WebInputEvent> web_event,
      const base::Callback<void(mus::mojom::EventResult)>& ack);

  void OnWindowInputEventAckOnMainThread(
      const base::Callback<void(mus::mojom::EventResult)>& ack,
      mus::mojom::EventResult result);

  // WindowTreeClientDelegate implementation:
  void OnWindowTreeClientDestroyed(mus::WindowTreeClient* client) override;
  void OnEmbed(mus::Window* root) override;
  void OnEventObserved(const ui::Event& event, mus::Window* target) override;

  // InputEventHandler implementation:
  void OnWindowInputEvent(
      mus::Window* window,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(mus::mojom::EventResult)>>*
          ack_callback) override;

  const int routing_id_;
  mus::Window* root_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  InputHandlerManager* const input_handler_manager_;
  std::unique_ptr<mus::WindowSurfaceBinding> window_surface_binding_;

  DISALLOW_COPY_AND_ASSIGN(CompositorMusConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MUS_COMPOSITOR_MUS_CONNECTION_H_

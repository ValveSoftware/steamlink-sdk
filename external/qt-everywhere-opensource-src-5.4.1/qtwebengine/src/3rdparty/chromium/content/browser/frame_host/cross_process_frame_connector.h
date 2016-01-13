// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
#define CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_

#include "cc/output/compositor_frame.h"
#include "ui/gfx/rect.h"

namespace blink {
class WebInputEvent;
}

namespace IPC {
class Message;
}

struct FrameHostMsg_BuffersSwappedACK_Params;
struct FrameHostMsg_CompositorFrameSwappedACK_Params;
struct FrameHostMsg_ReclaimCompositorResources_Params;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;

namespace content {
class RenderFrameProxyHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewChildFrame;

// CrossProcessFrameConnector provides the platform view abstraction for
// RenderWidgetHostViewChildFrame allowing RWHVChildFrame to remain ignorant
// of RenderFrameHost.
//
// The RenderWidgetHostView of an out-of-process child frame needs to
// communicate with the RenderFrameProxyHost representing this frame in the
// process of the parent frame. For example, assume you have this page:
//
//   -----------------
//   | frame 1       |
//   |  -----------  |
//   |  | frame 2 |  |
//   |  -----------  |
//   -----------------
//
// If frames 1 and 2 are in process A and B, there are 4 RenderFrameHosts:
//   A1 - RFH for frame 1 in process A
//   B1 - RFPH for frame 1 in process B
//   A2 - RFPH for frame 2 in process A
//   B2 - RFH for frame 2 in process B
//
// B2, having a parent frame in a different process, will have a
// RenderWidgetHostViewChildFrame. This RenderWidgetHostViewChildFrame needs
// to communicate with A2 because the embedding process is an abstract
// for the child frame -- it needs information necessary for compositing child
// frame textures, and also can pass platform messages such as view resizing.
// CrossProcessFrameConnector bridges between B2's
// RenderWidgetHostViewChildFrame and A2 to allow for this communication.
// (Note: B1 is only mentioned for completeness. It is not needed in this
// example.)
//
// CrossProcessFrameConnector objects are owned by the RenderFrameProxyHost
// in the child frame's RenderFrameHostManager corresponding to the parent's
// SiteInstance, A2 in the picture above. When a child frame navigates in a new
// process, set_view() is called to update to the new view.
//
class CrossProcessFrameConnector {
 public:
  // |frame_proxy_in_parent_renderer| corresponds to A2 in the example above.
  explicit CrossProcessFrameConnector(
      RenderFrameProxyHost* frame_proxy_in_parent_renderer);
  virtual ~CrossProcessFrameConnector();

  bool OnMessageReceived(const IPC::Message &msg);

  // |view| corresponds to B2's RenderWidgetHostViewChildFrame in the example
  // above.
  void set_view(RenderWidgetHostViewChildFrame* view);
  RenderWidgetHostViewChildFrame* get_view_for_testing() { return view_; }

  void RenderProcessGone();

  // 'Platform' functionality exposed to RenderWidgetHostViewChildFrame.
  // These methods can forward messages to the child frame proxy in the parent
  // frame's renderer or attempt to handle them within the browser process.
  void ChildFrameBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id);

  void ChildFrameCompositorFrameSwapped(uint32 output_surface_id,
                                        int host_id,
                                        int route_id,
                                        scoped_ptr<cc::CompositorFrame> frame);

  gfx::Rect ChildFrameRect();

 private:
  // Handlers for messages received from the parent frame.
  void OnBuffersSwappedACK(
      const FrameHostMsg_BuffersSwappedACK_Params& params);
  void OnCompositorFrameSwappedACK(
      const FrameHostMsg_CompositorFrameSwappedACK_Params& params);
  void OnReclaimCompositorResources(
      const FrameHostMsg_ReclaimCompositorResources_Params& params);
  void OnForwardInputEvent(const blink::WebInputEvent* event);
  void OnInitializeChildFrame(gfx::Rect frame_rect, float scale_factor);

  void SetDeviceScaleFactor(float scale_factor);
  void SetSize(gfx::Rect frame_rect);

  // The RenderFrameProxyHost that routes messages to the parent frame's
  // renderer process.
  RenderFrameProxyHost* frame_proxy_in_parent_renderer_;

  // The RenderWidgetHostView for the frame. Initially NULL.
  RenderWidgetHostViewChildFrame* view_;

  gfx::Rect child_frame_rect_;
  float device_scale_factor_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_


// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_
#define CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_

#include <stdint.h>

#include "cc/output/compositor_frame.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
class WebGestureEvent;
class WebInputEvent;
}

namespace cc {
class SurfaceId;
struct SurfaceSequence;
}

namespace IPC {
class Message;
}

namespace content {
class RenderFrameProxyHost;
class RenderWidgetHostViewBase;
class RenderWidgetHostViewChildFrame;
class WebCursor;

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
class CONTENT_EXPORT CrossProcessFrameConnector {
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

  virtual void SetChildFrameSurface(const cc::SurfaceId& surface_id,
                                    const gfx::Size& frame_size,
                                    float scale_factor,
                                    const cc::SurfaceSequence& sequence);

  gfx::Rect ChildFrameRect();
  void UpdateCursor(const WebCursor& cursor);
  gfx::Point TransformPointToRootCoordSpace(const gfx::Point& point,
                                            const cc::SurfaceId& surface_id);
  // TransformPointToLocalCoordSpace() can only transform points between
  // surfaces where one is embedded (not necessarily directly) within the
  // other, and will return false if this is not the case. For points that can
  // be in sibling surfaces, they must first be converted to the root
  // surface's coordinate space.
  bool TransformPointToLocalCoordSpace(const gfx::Point& point,
                                       const cc::SurfaceId& original_surface,
                                       const cc::SurfaceId& local_surface_id,
                                       gfx::Point* transformed_point);
  // Returns false if |target_view| and |view_| do not have the same root
  // RenderWidgetHostView.
  bool TransformPointToCoordSpaceForView(const gfx::Point& point,
                                         RenderWidgetHostViewBase* target_view,
                                         const cc::SurfaceId& local_surface_id,
                                         gfx::Point* transformed_point);

  // Pass acked touch events to the root view for gesture processing.
  void ForwardProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                     InputEventAckState ack_result);
  // Gesture events with unused scroll deltas must be bubbled to ancestors
  // who may consume the delta.
  void BubbleScrollEvent(const blink::WebGestureEvent& event);

  // Determines whether the root RenderWidgetHostView (and thus the current
  // page) has focus.
  bool HasFocus();
  // Focuses the root RenderWidgetHostView.
  void FocusRootView();

  // Locks the mouse. Returns true if mouse is locked.
  bool LockMouse();

  // Unlocks the mouse if the mouse is locked.
  void UnlockMouse();

  // Returns the parent RenderWidgetHostView or nullptr it it doesn't have one.
  virtual RenderWidgetHostViewBase* GetParentRenderWidgetHostView();

  // Returns the view for the top-level frame under the same WebContents.
  RenderWidgetHostViewBase* GetRootRenderWidgetHostView();

  // Exposed for tests.
  RenderWidgetHostViewBase* GetRootRenderWidgetHostViewForTesting() {
    return GetRootRenderWidgetHostView();
  }

 private:
  // Handlers for messages received from the parent frame.
  void OnForwardInputEvent(const blink::WebInputEvent* event);
  void OnFrameRectChanged(const gfx::Rect& frame_rect);
  void OnVisibilityChanged(bool visible);
  void OnSatisfySequence(const cc::SurfaceSequence& sequence);
  void OnRequireSequence(const cc::SurfaceId& id,
                         const cc::SurfaceSequence& sequence);

  void SetRect(const gfx::Rect& frame_rect);

  // The RenderFrameProxyHost that routes messages to the parent frame's
  // renderer process.
  RenderFrameProxyHost* frame_proxy_in_parent_renderer_;

  // The RenderWidgetHostView for the frame. Initially NULL.
  RenderWidgetHostViewChildFrame* view_;

  gfx::Rect child_frame_rect_;

  bool is_scroll_bubbling_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_CROSS_PROCESS_FRAME_CONNECTOR_H_


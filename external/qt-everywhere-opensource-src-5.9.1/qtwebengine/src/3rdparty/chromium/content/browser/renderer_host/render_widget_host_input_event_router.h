// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_

#include <stdint.h>

#include <deque>
#include <unordered_map>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "cc/surfaces/surface_hittest_delegate.h"
#include "cc/surfaces/surface_id.h"
#include "content/browser/renderer_host/render_widget_host_view_base_observer.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/vector2d.h"

struct FrameHostMsg_HittestData_Params;

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;
}

namespace gfx {
class Point;
}

namespace ui {
class LatencyInfo;
}

namespace content {

class RenderWidgetHostImpl;
class RenderWidgetHostViewBase;

// Class owned by WebContentsImpl for the purpose of directing input events
// to the correct RenderWidgetHost on pages with multiple RenderWidgetHosts.
// It maintains a mapping of RenderWidgetHostViews to Surface IDs that they
// own. When an input event requires routing based on window coordinates,
// this class requests a Surface hit test from the provided |root_view| and
// forwards the event to the owning RWHV of the returned Surface ID.
class CONTENT_EXPORT RenderWidgetHostInputEventRouter
    : public RenderWidgetHostViewBaseObserver {
 public:
  RenderWidgetHostInputEventRouter();
  ~RenderWidgetHostInputEventRouter() final;

  void OnRenderWidgetHostViewBaseDestroyed(
      RenderWidgetHostViewBase* view) override;

  void RouteMouseEvent(RenderWidgetHostViewBase* root_view,
                       blink::WebMouseEvent* event,
                       const ui::LatencyInfo& latency);
  void RouteMouseWheelEvent(RenderWidgetHostViewBase* root_view,
                            blink::WebMouseWheelEvent* event,
                            const ui::LatencyInfo& latency);
  void RouteGestureEvent(RenderWidgetHostViewBase* root_view,
                         blink::WebGestureEvent* event,
                         const ui::LatencyInfo& latency);
  void RouteTouchEvent(RenderWidgetHostViewBase* root_view,
                       blink::WebTouchEvent *event,
                       const ui::LatencyInfo& latency);

  void BubbleScrollEvent(RenderWidgetHostViewBase* target_view,
                         const blink::WebGestureEvent& event);
  void CancelScrollBubbling(RenderWidgetHostViewBase* target_view);

  void AddFrameSinkIdOwner(const cc::FrameSinkId& id,
                           RenderWidgetHostViewBase* owner);
  void RemoveFrameSinkIdOwner(const cc::FrameSinkId& id);

  bool is_registered(const cc::FrameSinkId& id) {
    return owner_map_.find(id) != owner_map_.end();
  }

  void OnHittestData(const FrameHostMsg_HittestData_Params& params);

  // Returns the RenderWidgetHostImpl inside the |root_view| at |point| where
  // |point| is with respect to |root_view|'s coordinates. If a RWHI is found,
  // the value of |transformed_point| is the coordinate of the point with
  // respect to the RWHI's coordinates. If |root_view| is nullptr, this method
  // will return nullptr and will not modify |transformed_point|.
  RenderWidgetHostImpl* GetRenderWidgetHostAtPoint(
      RenderWidgetHostViewBase* root_view,
      const gfx::Point& point,
      gfx::Point* transformed_point);

 private:
  struct HittestData {
    bool ignored_for_hittest;
  };

  class HittestDelegate : public cc::SurfaceHittestDelegate {
   public:
    HittestDelegate(
        const std::unordered_map<cc::SurfaceId, HittestData, cc::SurfaceIdHash>&
            hittest_data);
    bool RejectHitTarget(const cc::SurfaceDrawQuad* surface_quad,
                         const gfx::Point& point_in_quad_space) override;
    bool AcceptHitTarget(const cc::SurfaceDrawQuad* surface_quad,
                         const gfx::Point& point_in_quad_space) override;

    const std::unordered_map<cc::SurfaceId, HittestData, cc::SurfaceIdHash>&
        hittest_data_;
  };

  using FrameSinkIdOwnerMap = std::unordered_map<cc::FrameSinkId,
                                                 RenderWidgetHostViewBase*,
                                                 cc::FrameSinkIdHash>;
  struct TargetData {
    RenderWidgetHostViewBase* target;
    gfx::Vector2d delta;

    TargetData() : target(nullptr) {}
  };
  using TargetQueue = std::deque<TargetData>;

  void ClearAllObserverRegistrations();

  RenderWidgetHostViewBase* FindEventTarget(RenderWidgetHostViewBase* root_view,
                                            const gfx::Point& point,
                                            gfx::Point* transformed_point);

  void RouteTouchscreenGestureEvent(RenderWidgetHostViewBase* root_view,
                                    blink::WebGestureEvent* event,
                                    const ui::LatencyInfo& latency);
  void RouteTouchpadGestureEvent(RenderWidgetHostViewBase* root_view,
                                 blink::WebGestureEvent* event,
                                 const ui::LatencyInfo& latency);

  // MouseMove/Enter/Leave events might need to be processed by multiple frames
  // in different processes for MouseEnter and MouseLeave event handlers to
  // properly fire. This method determines which RenderWidgetHostViews other
  // than the actual target require notification, and sends the appropriate
  // events to them.
  void SendMouseEnterOrLeaveEvents(blink::WebMouseEvent* event,
                                   RenderWidgetHostViewBase* target,
                                   RenderWidgetHostViewBase* root_view);

  // The following methods take a GestureScrollUpdate event and send a
  // GestureScrollBegin or GestureScrollEnd for wrapping it. This is needed
  // when GestureScrollUpdates are being forwarded for scroll bubbling.
  void SendGestureScrollBegin(RenderWidgetHostViewBase* view,
                              const blink::WebGestureEvent& event);
  void SendGestureScrollEnd(RenderWidgetHostViewBase* view,
                            const blink::WebGestureEvent& event);

  FrameSinkIdOwnerMap owner_map_;
  TargetQueue touchscreen_gesture_target_queue_;
  TargetData touch_target_;
  TargetData touchscreen_gesture_target_;
  TargetData touchpad_gesture_target_;
  TargetData bubbling_gesture_scroll_target_;
  TargetData first_bubbling_scroll_target_;
  // Maintains the same target between mouse down and mouse up.
  TargetData mouse_capture_target_;

  // Tracked for the purpose of generating MouseEnter and MouseLeave events.
  RenderWidgetHostViewBase* last_mouse_move_target_;
  RenderWidgetHostViewBase* last_mouse_move_root_view_;

  int active_touches_;
  // Keep track of when we are between GesturePinchBegin and GesturePinchEnd
  // inclusive, as we need to route these events (and anything in between) to
  // the main frame.
  bool in_touchscreen_gesture_pinch_;
  bool gesture_pinch_did_send_scroll_begin_;
  std::unordered_map<cc::SurfaceId, HittestData, cc::SurfaceIdHash>
      hittest_data_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostInputEventRouter);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           InputEventRouterGestureTargetQueueTest);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           InputEventRouterTouchpadGestureTargetTest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_

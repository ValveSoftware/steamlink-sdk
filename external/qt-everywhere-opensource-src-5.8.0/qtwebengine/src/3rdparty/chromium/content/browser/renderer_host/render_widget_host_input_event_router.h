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
                       blink::WebMouseEvent* event);
  void RouteMouseWheelEvent(RenderWidgetHostViewBase* root_view,
                            blink::WebMouseWheelEvent* event);
  void RouteGestureEvent(RenderWidgetHostViewBase* root_view,
                         blink::WebGestureEvent* event,
                         const ui::LatencyInfo& latency);
  void RouteTouchEvent(RenderWidgetHostViewBase* root_view,
                       blink::WebTouchEvent *event,
                       const ui::LatencyInfo& latency);

  void AddSurfaceIdNamespaceOwner(uint32_t id, RenderWidgetHostViewBase* owner);
  void RemoveSurfaceIdNamespaceOwner(uint32_t id);

  bool is_registered(uint32_t id) {
    return owner_map_.find(id) != owner_map_.end();
  }

  void OnHittestData(const FrameHostMsg_HittestData_Params& params);

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

  using SurfaceIdNamespaceOwnerMap =
      base::hash_map<uint32_t, RenderWidgetHostViewBase*>;
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

  SurfaceIdNamespaceOwnerMap owner_map_;
  TargetQueue touchscreen_gesture_target_queue_;
  TargetData touch_target_;
  TargetData touchscreen_gesture_target_;
  TargetData touchpad_gesture_target_;
  int active_touches_;
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

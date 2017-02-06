// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/resources/returned_resource.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/readback_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class SurfaceFactory;
enum class SurfaceDrawStatus;
}


namespace content {
class CrossProcessFrameConnector;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewChildFrameTest;
class RenderWidgetHostViewGuestSurfaceTest;

// RenderWidgetHostViewChildFrame implements the view for a RenderWidgetHost
// associated with content being rendered in a separate process from
// content that is embedding it. This is not a platform-specific class; rather,
// the embedding renderer process implements the platform containing the
// widget, and the top-level frame's RenderWidgetHostView will ultimately
// manage all native widget interaction.
//
// See comments in render_widget_host_view.h about this class and its members.
class CONTENT_EXPORT RenderWidgetHostViewChildFrame
    : public RenderWidgetHostViewBase,
      public cc::SurfaceFactoryClient,
      public cc::BeginFrameObserver {
 public:
  explicit RenderWidgetHostViewChildFrame(RenderWidgetHost* widget);
  ~RenderWidgetHostViewChildFrame() override;

  void SetCrossProcessFrameConnector(
      CrossProcessFrameConnector* frame_connector);

  // This functions registers single-use callbacks that want to be notified when
  // the next frame is swapped. The callback is triggered by
  // OnSwapCompositorFrame, which is the appropriate time to request pixel
  // readback for the frame that is about to be drawn. Once called, the callback
  // pointer is released.
  // TODO(wjmaclean): We should consider making this available in other view
  // types, such as RenderWidgetHostViewAura.
  void RegisterFrameSwappedCallback(std::unique_ptr<base::Closure> callback);

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  void Focus() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Size GetVisibleViewportSize() const override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void SetBackgroundColor(SkColor color) override;
  gfx::Size GetPhysicalBackingSize() const override;
  bool IsMouseLocked() override;

  // RenderWidgetHostViewBase implementation.
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const ReadbackRequestCallback& callback,
      const SkColorType preferred_color_type) override;
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) override;
  bool CanCopyToVideoFrame() const override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void WheelEventAck(const blink::WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  void OnSwapCompositorFrame(uint32_t output_surface_id,
                             cc::CompositorFrame frame) override;
  // Since the URL of content rendered by this class is not displayed in
  // the URL bar, this method does not need an implementation.
  void ClearCompositorFrame() override {}
  void GetScreenInfo(blink::WebScreenInfo* results) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
  bool LockMouse() override;
  void UnlockMouse() override;
  uint32_t GetSurfaceIdNamespace() override;
  void ProcessKeyboardEvent(const NativeWebKeyboardEvent& event) override;
  void ProcessMouseEvent(const blink::WebMouseEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event,
                              const ui::LatencyInfo& latency) override;
  void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo& latency) override;
  gfx::Point TransformPointToRootCoordSpace(const gfx::Point& point) override;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  ui::AcceleratedWidgetMac* GetAcceleratedWidgetMac() const override;
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override;
  bool SupportsSpeech() const override;
  void SpeakSelection() override;
  bool IsSpeaking() const override;
  void StopSpeaking() override;
#endif  // defined(OS_MACOSX)

  // RenderWidgetHostViewBase implementation.
  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;

  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate, bool for_root_frame) override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* source) override;

  // cc::BeginFrameObserver implementation.
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  const cc::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  void OnSetNeedsBeginFrames(bool needs_begin_frames);

  // Declared 'public' instead of 'protected' here to allow derived classes
  // to Bind() to it.
  void SurfaceDrawn(uint32_t output_surface_id, cc::SurfaceDrawStatus drawn);

  // Exposed for tests.
  bool IsChildFrameForTesting() const override;
  cc::SurfaceId SurfaceIdForTesting() const override;
  CrossProcessFrameConnector* FrameConnectorForTesting() const {
    return frame_connector_;
  }

  void RegisterSurfaceNamespaceId();
  void UnregisterSurfaceNamespaceId();

 protected:
  friend class RenderWidgetHostView;
  friend class RenderWidgetHostViewChildFrameTest;
  friend class RenderWidgetHostViewGuestSurfaceTest;

  // Clears current compositor surface, if one is in use.
  void ClearCompositorSurfaceIfNecessary();

  void ProcessFrameSwappedCallbacks();

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  // Members will become private when RenderWidgetHostViewGuest is removed.
  // The model object.
  RenderWidgetHostImpl* host_;

  // Surface-related state.
  std::unique_ptr<cc::SurfaceIdAllocator> id_allocator_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;
  cc::SurfaceId surface_id_;
  uint32_t next_surface_sequence_;
  uint32_t last_output_surface_id_;
  gfx::Size current_surface_size_;
  float current_surface_scale_factor_;
  gfx::Rect last_screen_rect_;
  uint32_t ack_pending_count_;
  cc::ReturnedResourceArray surface_returned_resources_;

  // frame_connector_ provides a platform abstraction. Messages
  // sent through it are routed to the embedding renderer process.
  CrossProcessFrameConnector* frame_connector_;

  base::WeakPtr<RenderWidgetHostViewChildFrame> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void SubmitSurfaceCopyRequest(const gfx::Rect& src_subrect,
                                const gfx::Size& dst_size,
                                const ReadbackRequestCallback& callback,
                                const SkColorType preferred_color_type);

  using FrameSwappedCallbackList = std::deque<std::unique_ptr<base::Closure>>;
  // Since frame-drawn callbacks are "fire once", we use std::deque to make
  // it convenient to swap() when processing the list.
  FrameSwappedCallbackList frame_swapped_callbacks_;

  // The begin frame source being observed.  Null if none.
  cc::BeginFrameSource* begin_frame_source_;
  cc::BeginFrameArgs last_begin_frame_args_;
  bool observing_begin_frame_source_;
  // The surface id namespace of the parent RenderWidgetHostView.  0 if none.
  uint32_t parent_surface_id_namespace_;

  base::WeakPtrFactory<RenderWidgetHostViewChildFrame> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrame);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_CHILD_FRAME_H_

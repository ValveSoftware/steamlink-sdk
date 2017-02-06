// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "cc/input/selection.h"
#include "cc/output/begin_frame_args.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/android/content_view_core_impl_observer.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/input/stylus_text_selector.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/window_android_observer.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace cc {
class CopyOutputResult;
class Layer;
class SurfaceFactory;
class SurfaceIdAllocator;
enum class SurfaceDrawStatus;
}

namespace blink {
class WebExternalTextureLayer;
class WebTouchEvent;
class WebMouseEvent;
}

namespace content {
class ContentViewCoreImpl;
class ContentViewCoreObserver;
class OverscrollControllerAndroid;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class SynchronousCompositorHost;
struct DidOverscrollParams;
struct NativeWebKeyboardEvent;
struct TextInputState;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public cc::SurfaceFactoryClient,
      public ui::GestureProviderClient,
      public ui::WindowAndroidObserver,
      public DelegatedFrameEvictorClient,
      public StylusTextSelectorClient,
      public ui::TouchSelectionControllerClient,
      public content::ContentViewCoreImplObserver {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCoreImpl* content_view_core);
  ~RenderWidgetHostViewAndroid() override;

  void Blur();

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void Focus() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Size GetVisibleViewportSize() const override;
  gfx::Size GetPhysicalBackingSize() const override;
  bool DoTopControlsShrinkBlinkSize() const override;
  float GetTopControlsHeight() const override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void TextInputStateChanged(const TextInputState& params) override;
  void ImeCancelComposition() override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  void FocusedNodeChanged(bool is_editable_node) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void SetBackgroundColor(SkColor color) override;
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
  void GetScreenInfo(blink::WebScreenInfo* results) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  void OnSetNeedsFlushInput() override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate, bool for_root_frame) override;
  bool LockMouse() override;
  void UnlockMouse() override;
  void OnSwapCompositorFrame(uint32_t output_surface_id,
                             cc::CompositorFrame frame) override;
  void ClearCompositorFrame() override;
  void DidOverscroll(const DidOverscrollParams& params) override;
  void DidStopFlinging() override;
  uint32_t GetSurfaceIdNamespace() override;
  void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                               const SkBitmap& zoomed_bitmap) override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          size_t start_offset,
                                          size_t end_offset) override;
  void OnDidNavigateMainFrameToNewPage() override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  // ui::WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override;
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnVSync(base::TimeTicks frame_time,
               base::TimeDelta vsync_period) override;
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnActivityStopped() override;
  void OnActivityStarted() override;

  // content::ContentViewCoreImplObserver implementation.
  void OnContentViewCoreDestroyed() override;
  void OnAttachedToWindow() override;
  void OnDetachedFromWindow() override;

  // DelegatedFrameEvictor implementation
  void EvictDelegatedFrame() override;

  // StylusTextSelectorClient implementation.
  void OnStylusSelectBegin(float x0, float y0, float x1, float y1) override;
  void OnStylusSelectUpdate(float x, float y) override;
  void OnStylusSelectEnd() override;
  void OnStylusSelectTap(base::TimeTicks time, float x, float y) override;

  // ui::TouchSelectionControllerClient implementation.
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;

  // Non-virtual methods
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);

  void OnStartContentIntent(const GURL& content_url, bool is_main_frame);
  void OnSetNeedsBeginFrames(bool enabled);
  void OnSmartClipDataExtracted(const base::string16& text,
                                const base::string16& html,
                                const gfx::Rect rect);

  bool OnTouchEvent(const ui::MotionEvent& event);
  bool OnTouchHandleEvent(const ui::MotionEvent& event);
  void ResetGestureDetection();
  void SetDoubleTapSupportEnabled(bool enabled);
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  long GetNativeImeAdapter();

  void WasResized();

  void GetScaledContentBitmap(float scale,
                              SkColorType preferred_color_type,
                              gfx::Rect src_subrect,
                              const ReadbackRequestCallback& result_callback);

  scoped_refptr<cc::Layer> CreateDelegatedLayer() const;

  bool HasValidFrame() const;

  void MoveCaret(const gfx::Point& point);
  void DismissTextHandles();
  void SetTextHandlesTemporarilyHidden(bool hidden);
  void OnShowingPastePopup(const gfx::PointF& point);
  void OnShowUnhandledTapUIIfNeeded(int x_dip, int y_dip);

  void SynchronousFrameMetadata(cc::CompositorFrameMetadata frame_metadata);

  void SetOverlayVideoMode(bool enabled);

  typedef base::Callback<
      void(const base::string16& content, int start_offset, int end_offset)>
      TextSurroundingSelectionCallback;
  void SetTextSurroundingSelectionCallback(
      const TextSurroundingSelectionCallback& callback);

  static void OnContextLost();

 private:
  void RunAckCallbacks(cc::SurfaceDrawStatus status);

  void DestroyDelegatedContent();
  void CheckOutputSurfaceChanged(uint32_t output_surface_id);
  void SubmitCompositorFrame(cc::CompositorFrame frame_data);
  void SwapDelegatedFrame(uint32_t output_surface_id,
                          cc::CompositorFrame frame_data);
  void SendDelegatedFrameAck(uint32_t output_surface_id);
  void SendReturnedDelegatedResources(uint32_t output_surface_id);

  void OnFrameMetadataUpdated(
      const cc::CompositorFrameMetadata& frame_metadata);

  void ShowInternal();
  void HideInternal();
  void AttachLayers();
  void RemoveLayers();

  void UpdateBackgroundColor(SkColor color);

  // Called after async screenshot task completes. Scales and crops the result
  // of the copy.
  static void PrepareTextureCopyOutputResult(
      base::WeakPtr<RenderWidgetHostViewAndroid> rwhva,
      scoped_refptr<cc::Layer> readback_layer,
      const gfx::Size& dst_size_in_pixel,
      SkColorType color_type,
      const base::TimeTicks& start_time,
      const ReadbackRequestCallback& callback,
      std::unique_ptr<cc::CopyOutputResult> result);

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(const gfx::Rect& src_subrect_in_pixel,
                               const gfx::Size& dst_size_in_pixel,
                               const ReadbackRequestCallback& callback,
                               const SkColorType color_type);

  // If we have locks on a frame during a ContentViewCore swap or a context
  // lost, the frame is no longer valid and we can safely release all the locks.
  // Use this method to release all the locks.
  void ReleaseLocksOnSurface();

  // Drop any incoming frames from the renderer when there are locks on the
  // current frame.
  void RetainFrame(uint32_t output_surface_id, cc::CompositorFrame frame);

  void InternalSwapCompositorFrame(uint32_t output_surface_id,
                                   cc::CompositorFrame frame);
  void OnLostResources();

  enum VSyncRequestType {
    FLUSH_INPUT = 1 << 0,
    BEGIN_FRAME = 1 << 1,
    PERSISTENT_BEGIN_FRAME = 1 << 2
  };
  void RequestVSyncUpdate(uint32_t requests);
  void StartObservingRootWindow();
  void StopObservingRootWindow();
  void SendBeginFrame(base::TimeTicks frame_time, base::TimeDelta vsync_period);
  bool Animate(base::TimeTicks frame_time);
  void RequestDisallowInterceptTouchEvent();

  bool SyncCompositorOnMessageReceived(const IPC::Message& message);

  void ComputeEventLatencyOSTouchHistograms(const ui::MotionEvent& event);

  // The model object.
  RenderWidgetHostImpl* host_;

  // Used to control action dispatch at the next |OnVSync()| call.
  uint32_t outstanding_vsync_requests_;

  bool is_showing_;

  // Window-specific bits that affect widget visibility.
  bool is_window_visible_;
  bool is_window_activity_started_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  ImeAdapterAndroid ime_adapter_android_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  scoped_refptr<cc::Layer> layer_;

  std::unique_ptr<cc::SurfaceIdAllocator> id_allocator_;
  std::unique_ptr<cc::SurfaceFactory> surface_factory_;
  cc::SurfaceId surface_id_;
  gfx::Size current_surface_size_;
  cc::ReturnedResourceArray surface_returned_resources_;
  gfx::Vector2dF location_bar_content_translation_;
  cc::Selection<gfx::SelectionBound> current_viewport_selection_;

  // The most recent texture size that was pushed to the texture layer.
  gfx::Size texture_size_in_layer_;

  // The output surface id of the last received frame.
  uint32_t last_output_surface_id_;


  std::queue<base::Closure> ack_callbacks_;

  // Used to control and render overscroll-related effects.
  std::unique_ptr<OverscrollControllerAndroid> overscroll_controller_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;

  // Handles gesture based text selection
  StylusTextSelector stylus_text_selector_;

  // Manages selection handle rendering and manipulation.
  // This will always be NULL if |content_view_core_| is NULL.
  std::unique_ptr<ui::TouchSelectionController> selection_controller_;

  // Size to use if we have no backing ContentViewCore
  gfx::Size default_size_;

  const bool using_browser_compositor_;
  std::unique_ptr<SynchronousCompositorHost> sync_compositor_;

  std::unique_ptr<DelegatedFrameEvictor> frame_evictor_;

  size_t locks_on_frame_count_;
  bool observing_root_window_;

  struct LastFrameInfo {
    LastFrameInfo(uint32_t output_id, cc::CompositorFrame output_frame);
    ~LastFrameInfo();
    uint32_t output_surface_id;
    cc::CompositorFrame frame;
  };

  std::unique_ptr<LastFrameInfo> last_frame_info_;

  TextSurroundingSelectionCallback text_surrounding_selection_callback_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

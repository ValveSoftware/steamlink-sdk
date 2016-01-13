// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <map>
#include <queue>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/output/begin_frame_args.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/input/gesture_text_selector.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/base/android/window_android_observer.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"

struct ViewHostMsg_TextInputState_Params;

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;

namespace cc {
class CopyOutputResult;
class DelegatedFrameProvider;
class DelegatedRendererLayer;
class Layer;
}

namespace blink {
class WebExternalTextureLayer;
class WebTouchEvent;
class WebMouseEvent;
}

namespace content {
class ContentViewCoreImpl;
class OverscrollGlow;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct DidOverscrollParams;
struct NativeWebKeyboardEvent;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public cc::DelegatedFrameResourceCollectionClient,
      public ImageTransportFactoryAndroidObserver,
      public ui::GestureProviderClient,
      public ui::WindowAndroidObserver,
      public DelegatedFrameEvictorClient,
      public GestureTextSelectorClient {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCoreImpl* content_view_core);
  virtual ~RenderWidgetHostViewAndroid();

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual gfx::Size GetPhysicalBackingSize() const OVERRIDE;
  virtual float GetOverdrawBottomHeight() const OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void FocusedNodeChanged(bool is_editable_node) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const base::string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void ScrollOffsetChanged() OVERRIDE;
  virtual void AcceleratedSurfaceInitialized(int host_id,
                                             int route_id) OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void SetBackgroundOpaque(bool transparent) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkBitmap::Config config) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result) OVERRIDE;
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) OVERRIDE;
  virtual void OnSetNeedsFlushInput() OVERRIDE;
  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result) OVERRIDE;
  virtual void CreateBrowserAccessibilityManagerIfNeeded() OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) OVERRIDE;
  virtual void DidOverscroll(const DidOverscrollParams& params) OVERRIDE;
  virtual void DidStopFlinging() OVERRIDE;
  virtual void ShowDisambiguationPopup(const gfx::Rect& target_rect,
                                       const SkBitmap& zoomed_bitmap) OVERRIDE;
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      OVERRIDE;
  virtual void LockCompositingSurface() OVERRIDE;
  virtual void UnlockCompositingSurface() OVERRIDE;
  virtual void OnTextSurroundingSelectionResponse(const base::string16& content,
                                                  size_t start_offset,
                                                  size_t end_offset) OVERRIDE;

  // cc::DelegatedFrameResourceCollectionClient implementation.
  virtual void UnusedResourcesAreAvailable() OVERRIDE;

  // ui::GestureProviderClient implementation.
  virtual void OnGestureEvent(const ui::GestureEventData& gesture) OVERRIDE;

  // ui::WindowAndroidObserver implementation.
  virtual void OnCompositingDidCommit() OVERRIDE;
  virtual void OnAttachCompositor() OVERRIDE {}
  virtual void OnDetachCompositor() OVERRIDE;
  virtual void OnVSync(base::TimeTicks frame_time,
                       base::TimeDelta vsync_period) OVERRIDE;
  virtual void OnAnimate(base::TimeTicks begin_frame_time) OVERRIDE;

  // ImageTransportFactoryAndroidObserver implementation.
  virtual void OnLostResources() OVERRIDE;

  // DelegatedFrameEvictor implementation
  virtual void EvictDelegatedFrame() OVERRIDE;

  virtual SkBitmap::Config PreferredReadbackFormat() OVERRIDE;

  // GestureTextSelectorClient implementation.
  virtual void ShowSelectionHandlesAutomatically() OVERRIDE;
  virtual void SelectRange(float x1, float y1, float x2, float y2) OVERRIDE;
  virtual void Unselect() OVERRIDE;
  virtual void LongPress(base::TimeTicks time, float x, float y) OVERRIDE;

  // Non-virtual methods
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendTouchEvent(const blink::WebTouchEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);

  void OnDidChangeBodyBackgroundColor(SkColor color);
  void OnStartContentIntent(const GURL& content_url);
  void OnSetNeedsBeginFrame(bool enabled);
  void OnSmartClipDataExtracted(const base::string16& text,
                                const base::string16& html,
                                const gfx::Rect rect);

  bool OnTouchEvent(const ui::MotionEvent& event);
  void ResetGestureDetection();
  void SetDoubleTapSupportEnabled(bool enabled);
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  long GetNativeImeAdapter();

  void WasResized();

  void GetScaledContentBitmap(
      float scale,
      SkBitmap::Config bitmap_config,
      gfx::Rect src_subrect,
      const base::Callback<void(bool, const SkBitmap&)>& result_callback);

  bool HasValidFrame() const;

  void MoveCaret(const gfx::Point& point);

  void SynchronousFrameMetadata(
      const cc::CompositorFrameMetadata& frame_metadata);

  void SetOverlayVideoMode(bool enabled);

  typedef base::Callback<
      void(const base::string16& content, int start_offset, int end_offset)>
      TextSurroundingSelectionCallback;
  void SetTextSurroundingSelectionCallback(
      const TextSurroundingSelectionCallback& callback);

 private:
  void RunAckCallbacks();

  void DestroyDelegatedContent();
  void SwapDelegatedFrame(uint32 output_surface_id,
                          scoped_ptr<cc::DelegatedFrameData> frame_data);
  void SendDelegatedFrameAck(uint32 output_surface_id);
  void SendReturnedDelegatedResources(uint32 output_surface_id);

  void OnFrameMetadataUpdated(
      const cc::CompositorFrameMetadata& frame_metadata);
  void ComputeContentsSize(const cc::CompositorFrameMetadata& frame_metadata);
  void ResetClipping();
  void ClipContents(const gfx::Rect& clipping, const gfx::Size& content_size);

  void AttachLayers();
  void RemoveLayers();

  // Called after async screenshot task completes. Scales and crops the result
  // of the copy.
  static void PrepareTextureCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::TimeTicks& start_time,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareTextureCopyOutputResultForDelegatedReadback(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::TimeTicks& start_time,
      scoped_refptr<cc::Layer> readback_layer,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(
      const gfx::Rect& src_subrect_in_pixel,
      const gfx::Size& dst_size_in_pixel,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkBitmap::Config config);

  bool IsReadbackConfigSupported(SkBitmap::Config bitmap_config);

  // If we have locks on a frame during a ContentViewCore swap or a context
  // lost, the frame is no longer valid and we can safely release all the locks.
  // Use this method to release all the locks.
  void ReleaseLocksOnSurface();

  // Drop any incoming frames from the renderer when there are locks on the
  // current frame.
  void RetainFrame(uint32 output_surface_id,
                   scoped_ptr<cc::CompositorFrame> frame);

  void InternalSwapCompositorFrame(uint32 output_surface_id,
                                   scoped_ptr<cc::CompositorFrame> frame);

  void SetNeedsAnimate();
  bool Animate(base::TimeTicks frame_time);

  // The model object.
  RenderWidgetHostImpl* host_;

  // Used to track whether this render widget needs a BeginFrame.
  bool needs_begin_frame_;

  bool is_showing_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  ImeAdapterAndroid ime_adapter_android_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;
  scoped_refptr<cc::DelegatedRendererLayer> layer_;

  // The most recent texture size that was pushed to the texture layer.
  gfx::Size texture_size_in_layer_;

  // The most recent content size that was pushed to the texture layer.
  gfx::Size content_size_in_layer_;

  // The device scale of the last received frame.
  float device_scale_factor_;

  // The output surface id of the last received frame.
  uint32_t last_output_surface_id_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_;

  std::queue<base::Closure> ack_callbacks_;

  const bool overscroll_effect_enabled_;
  // Used to render overscroll overlays.
  // Note: |overscroll_effect_| will never be NULL, even if it's never enabled.
  scoped_ptr<OverscrollGlow> overscroll_effect_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;

  // Handles gesture based text selection
  GestureTextSelector gesture_text_selector_;

  bool flush_input_requested_;

  int accelerated_surface_route_id_;

  // Size to use if we have no backing ContentViewCore
  gfx::Size default_size_;

  const bool using_synchronous_compositor_;

  scoped_ptr<DelegatedFrameEvictor> frame_evictor_;

  size_t locks_on_frame_count_;
  bool observing_root_window_;

  struct LastFrameInfo {
    LastFrameInfo(uint32 output_id,
                  scoped_ptr<cc::CompositorFrame> output_frame);
    ~LastFrameInfo();
    uint32 output_surface_id;
    scoped_ptr<cc::CompositorFrame> frame;
  };

  scoped_ptr<LastFrameInfo> last_frame_info_;

  TextSurroundingSelectionCallback text_surrounding_selection_callback_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

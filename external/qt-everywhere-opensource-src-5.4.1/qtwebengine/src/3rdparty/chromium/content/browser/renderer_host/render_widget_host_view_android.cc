// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <android/bitmap.h>

#include "base/android/sys_utils.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/worker_pool.h"
#include "cc/base/latency_info_swap_promise.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "cc/trees/layer_tree_host.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/overscroll_glow.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "skia/ext/image_operations.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/android/window_android.h"
#include "ui/base/android/window_android_compositor.h"
#include "ui/events/gesture_detection/gesture_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

namespace content {

namespace {

const int kUndefinedOutputSurfaceId = -1;

// Used to accomodate finite precision when comparing scaled viewport and
// content widths. While this value may seem large, width=device-width on an N7
// V1 saw errors of ~0.065 between computed window and content widths.
const float kMobileViewportWidthEpsilon = 0.15f;

static const char kAsyncReadBackString[] = "Compositing.CopyFromSurfaceTime";

// Sends an acknowledgement to the renderer of a processed IME event.
void SendImeEventAck(RenderWidgetHostImpl* host) {
  host->Send(new ViewMsg_ImeEventAck(host->GetRoutingID()));
}

void CopyFromCompositingSurfaceFinished(
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    scoped_ptr<SkBitmap> bitmap,
    const base::TimeTicks& start_time,
    scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock,
    bool result) {
  bitmap_pixels_lock.reset();
  release_callback->Run(0, false);
  UMA_HISTOGRAM_TIMES(kAsyncReadBackString,
                      base::TimeTicks::Now() - start_time);
  callback.Run(result, *bitmap);
}

ui::LatencyInfo CreateLatencyInfo(const blink::WebInputEvent& event) {
  ui::LatencyInfo latency_info;
  // The latency number should only be added if the timestamp is valid.
  if (event.timeStampSeconds) {
    const int64 time_micros = static_cast<int64>(
        event.timeStampSeconds * base::Time::kMicrosecondsPerSecond);
    latency_info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
        0,
        0,
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(time_micros),
        1);
  }
  return latency_info;
}

OverscrollGlow::DisplayParameters CreateOverscrollDisplayParameters(
    const cc::CompositorFrameMetadata& frame_metadata) {
  const float scale_factor =
      frame_metadata.page_scale_factor * frame_metadata.device_scale_factor;

  // Compute the size and offsets for each edge, where each effect is sized to
  // the viewport and offset by the distance of each viewport edge to the
  // respective content edge.
  OverscrollGlow::DisplayParameters params;
  params.size = gfx::ScaleSize(frame_metadata.viewport_size, scale_factor);
  params.edge_offsets[EdgeEffect::EDGE_TOP] =
      -frame_metadata.root_scroll_offset.y() * scale_factor;
  params.edge_offsets[EdgeEffect::EDGE_LEFT] =
      -frame_metadata.root_scroll_offset.x() * scale_factor;
  params.edge_offsets[EdgeEffect::EDGE_BOTTOM] =
      (frame_metadata.root_layer_size.height() -
       frame_metadata.root_scroll_offset.y() -
       frame_metadata.viewport_size.height()) * scale_factor;
  params.edge_offsets[EdgeEffect::EDGE_RIGHT] =
      (frame_metadata.root_layer_size.width() -
       frame_metadata.root_scroll_offset.x() -
       frame_metadata.viewport_size.width()) * scale_factor;
  params.device_scale_factor = frame_metadata.device_scale_factor;

  return params;
}

ui::GestureProvider::Config CreateGestureProviderConfig() {
  ui::GestureProvider::Config config = ui::DefaultGestureProviderConfig();
  config.disable_click_delay =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableClickDelay);
  return config;
}

bool HasFixedPageScale(const cc::CompositorFrameMetadata& frame_metadata) {
  return frame_metadata.min_page_scale_factor ==
         frame_metadata.max_page_scale_factor;
}

bool HasMobileViewport(const cc::CompositorFrameMetadata& frame_metadata) {
  float window_width_dip =
      frame_metadata.page_scale_factor * frame_metadata.viewport_size.width();
  float content_width_css = frame_metadata.root_layer_size.width();
  return content_width_css <= window_width_dip + kMobileViewportWidthEpsilon;
}

}  // anonymous namespace

RenderWidgetHostViewAndroid::LastFrameInfo::LastFrameInfo(
    uint32 output_id,
    scoped_ptr<cc::CompositorFrame> output_frame)
    : output_surface_id(output_id), frame(output_frame.Pass()) {}

RenderWidgetHostViewAndroid::LastFrameInfo::~LastFrameInfo() {}

RenderWidgetHostViewAndroid::RenderWidgetHostViewAndroid(
    RenderWidgetHostImpl* widget_host,
    ContentViewCoreImpl* content_view_core)
    : host_(widget_host),
      needs_begin_frame_(false),
      is_showing_(!widget_host->is_hidden()),
      content_view_core_(NULL),
      ime_adapter_android_(this),
      cached_background_color_(SK_ColorWHITE),
      last_output_surface_id_(kUndefinedOutputSurfaceId),
      weak_ptr_factory_(this),
      overscroll_effect_enabled_(!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOverscrollEdgeEffect)),
      overscroll_effect_(OverscrollGlow::Create(overscroll_effect_enabled_)),
      gesture_provider_(CreateGestureProviderConfig(), this),
      gesture_text_selector_(this),
      flush_input_requested_(false),
      accelerated_surface_route_id_(0),
      using_synchronous_compositor_(SynchronousCompositorImpl::FromID(
                                        widget_host->GetProcess()->GetID(),
                                        widget_host->GetRoutingID()) != NULL),
      frame_evictor_(new DelegatedFrameEvictor(this)),
      locks_on_frame_count_(0),
      observing_root_window_(false) {
  host_->SetView(this);
  SetContentViewCore(content_view_core);
  ImageTransportFactoryAndroid::AddObserver(this);
}

RenderWidgetHostViewAndroid::~RenderWidgetHostViewAndroid() {
  ImageTransportFactoryAndroid::RemoveObserver(this);
  SetContentViewCore(NULL);
  DCHECK(ack_callbacks_.empty());
  if (resource_collection_.get())
    resource_collection_->SetClient(NULL);
}


bool RenderWidgetHostViewAndroid::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewAndroid, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartContentIntent, OnStartContentIntent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidChangeBodyBackgroundColor,
                        OnDidChangeBodyBackgroundColor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetNeedsBeginFrame,
                        OnSetNeedsBeginFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SmartClipDataExtracted,
                        OnSmartClipDataExtracted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewAndroid::InitAsChild(gfx::NativeView parent_view) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAndroid::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

RenderWidgetHost*
RenderWidgetHostViewAndroid::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAndroid::WasShown() {
  if (!host_ || !host_->is_hidden())
    return;

  host_->WasShown();

  if (content_view_core_ && !using_synchronous_compositor_) {
    content_view_core_->GetWindowAndroid()->AddObserver(this);
    content_view_core_->GetWindowAndroid()->RequestVSyncUpdate();
    observing_root_window_ = true;
  }
}

void RenderWidgetHostViewAndroid::WasHidden() {
  RunAckCallbacks();

  if (!host_ || host_->is_hidden())
    return;

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host_->WasHidden();

  if (content_view_core_ && !using_synchronous_compositor_) {
    content_view_core_->GetWindowAndroid()->RemoveObserver(this);
    observing_root_window_ = false;
  }
}

void RenderWidgetHostViewAndroid::WasResized() {
  host_->WasResized();
}

void RenderWidgetHostViewAndroid::SetSize(const gfx::Size& size) {
  // Ignore the given size as only the Java code has the power to
  // resize the view on Android.
  default_size_ = size;
}

void RenderWidgetHostViewAndroid::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewAndroid::GetScaledContentBitmap(
    float scale,
    SkBitmap::Config bitmap_config,
    gfx::Rect src_subrect,
    const base::Callback<void(bool, const SkBitmap&)>& result_callback) {
  if (!IsSurfaceAvailableForCopy()) {
    result_callback.Run(false, SkBitmap());
    return;
  }

  gfx::Size bounds = layer_->bounds();
  if (src_subrect.IsEmpty())
    src_subrect = gfx::Rect(bounds);
  DCHECK_LE(src_subrect.width() + src_subrect.x(), bounds.width());
  DCHECK_LE(src_subrect.height() + src_subrect.y(), bounds.height());
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  DCHECK_GT(device_scale_factor, 0);
  gfx::Size dst_size(
      gfx::ToCeiledSize(gfx::ScaleSize(bounds, scale / device_scale_factor)));
  CopyFromCompositingSurface(
      src_subrect, dst_size, result_callback, bitmap_config);
}

bool RenderWidgetHostViewAndroid::HasValidFrame() const {
  if (!content_view_core_)
    return false;
  if (!layer_)
    return false;

  if (texture_size_in_layer_.IsEmpty())
    return false;

  if (!frame_evictor_->HasFrame())
    return false;

  return true;
}

gfx::NativeView RenderWidgetHostViewAndroid::GetNativeView() const {
  return content_view_core_->GetViewAndroid();
}

gfx::NativeViewId RenderWidgetHostViewAndroid::GetNativeViewId() const {
  return reinterpret_cast<gfx::NativeViewId>(
      const_cast<RenderWidgetHostViewAndroid*>(this));
}

gfx::NativeViewAccessible
RenderWidgetHostViewAndroid::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAndroid::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
  // We don't have plugin windows on Android. Do nothing. Note: this is called
  // from RenderWidgetHost::OnUpdateRect which is itself invoked while
  // processing the corresponding message from Renderer.
}

void RenderWidgetHostViewAndroid::Focus() {
  host_->Focus();
  host_->SetInputMethodActive(true);
  if (overscroll_effect_enabled_)
    overscroll_effect_->Enable();
}

void RenderWidgetHostViewAndroid::Blur() {
  host_->ExecuteEditCommand("Unselect", "");
  host_->SetInputMethodActive(false);
  host_->Blur();
  overscroll_effect_->Disable();
}

bool RenderWidgetHostViewAndroid::HasFocus() const {
  if (!content_view_core_)
    return false;  // ContentViewCore not created yet.

  return content_view_core_->HasFocus();
}

bool RenderWidgetHostViewAndroid::IsSurfaceAvailableForCopy() const {
  return HasValidFrame();
}

void RenderWidgetHostViewAndroid::Show() {
  if (is_showing_)
    return;

  is_showing_ = true;
  if (layer_)
    layer_->SetHideLayerAndSubtree(false);

  frame_evictor_->SetVisible(true);
  WasShown();
}

void RenderWidgetHostViewAndroid::Hide() {
  if (!is_showing_)
    return;

  is_showing_ = false;
  if (layer_ && locks_on_frame_count_ == 0)
    layer_->SetHideLayerAndSubtree(true);

  frame_evictor_->SetVisible(false);
  WasHidden();
}

bool RenderWidgetHostViewAndroid::IsShowing() {
  // ContentViewCoreImpl represents the native side of the Java
  // ContentViewCore.  It being NULL means that it is not attached
  // to the View system yet, so we treat this RWHVA as hidden.
  return is_showing_ && content_view_core_;
}

void RenderWidgetHostViewAndroid::LockCompositingSurface() {
  DCHECK(HasValidFrame());
  DCHECK(host_);
  DCHECK(frame_evictor_->HasFrame());
  frame_evictor_->LockFrame();
  locks_on_frame_count_++;
}

void RenderWidgetHostViewAndroid::UnlockCompositingSurface() {
  if (!frame_evictor_->HasFrame() || locks_on_frame_count_ == 0)
    return;

  DCHECK(HasValidFrame());
  frame_evictor_->UnlockFrame();
  locks_on_frame_count_--;

  if (locks_on_frame_count_ == 0) {
    if (last_frame_info_) {
      InternalSwapCompositorFrame(last_frame_info_->output_surface_id,
                                  last_frame_info_->frame.Pass());
      last_frame_info_.reset();
    }

    if (!is_showing_ && layer_)
      layer_->SetHideLayerAndSubtree(true);
  }
}

void RenderWidgetHostViewAndroid::SetTextSurroundingSelectionCallback(
    const TextSurroundingSelectionCallback& callback) {
  // Only one outstanding request is allowed at any given time.
  DCHECK(!callback.is_null());
  text_surrounding_selection_callback_ = callback;
}

void RenderWidgetHostViewAndroid::OnTextSurroundingSelectionResponse(
    const base::string16& content,
    size_t start_offset,
    size_t end_offset) {
  if (text_surrounding_selection_callback_.is_null())
    return;
  text_surrounding_selection_callback_.Run(content, start_offset, end_offset);
  text_surrounding_selection_callback_.Reset();
}

void RenderWidgetHostViewAndroid::ReleaseLocksOnSurface() {
  if (!frame_evictor_->HasFrame()) {
    DCHECK_EQ(locks_on_frame_count_, 0u);
    return;
  }
  while (locks_on_frame_count_ > 0) {
    UnlockCompositingSurface();
  }
  RunAckCallbacks();
}

gfx::Rect RenderWidgetHostViewAndroid::GetViewBounds() const {
  if (!content_view_core_)
    return gfx::Rect(default_size_);

  return gfx::Rect(content_view_core_->GetViewSize());
}

gfx::Size RenderWidgetHostViewAndroid::GetPhysicalBackingSize() const {
  if (!content_view_core_)
    return gfx::Size();

  return content_view_core_->GetPhysicalBackingSize();
}

float RenderWidgetHostViewAndroid::GetOverdrawBottomHeight() const {
  if (!content_view_core_)
    return 0.f;

  return content_view_core_->GetOverdrawBottomHeightDip();
}

void RenderWidgetHostViewAndroid::UpdateCursor(const WebCursor& cursor) {
  // There are no cursors on Android.
}

void RenderWidgetHostViewAndroid::SetIsLoading(bool is_loading) {
  // Do nothing. The UI notification is handled through ContentViewClient which
  // is TabContentsDelegate.
}

long RenderWidgetHostViewAndroid::GetNativeImeAdapter() {
  return reinterpret_cast<intptr_t>(&ime_adapter_android_);
}

void RenderWidgetHostViewAndroid::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  // If the change is not originated from IME (e.g. Javascript, autofill),
  // send back the renderer an acknowledgement, regardless of how we exit from
  // this method.
  base::ScopedClosureRunner ack_caller;
  if (params.is_non_ime_change)
    ack_caller.Reset(base::Bind(&SendImeEventAck, host_));

  if (!IsShowing())
    return;

  content_view_core_->UpdateImeAdapter(
      GetNativeImeAdapter(),
      static_cast<int>(params.type),
      params.value, params.selection_start, params.selection_end,
      params.composition_start, params.composition_end,
      params.show_ime_if_needed, params.is_non_ime_change);
}

void RenderWidgetHostViewAndroid::OnDidChangeBodyBackgroundColor(
    SkColor color) {
  if (cached_background_color_ == color)
    return;

  cached_background_color_ = color;
  if (content_view_core_)
    content_view_core_->OnBackgroundColorChanged(color);
}

void RenderWidgetHostViewAndroid::OnSetNeedsBeginFrame(bool enabled) {
  if (enabled == needs_begin_frame_)
    return;

  TRACE_EVENT1("cc", "RenderWidgetHostViewAndroid::OnSetNeedsBeginFrame",
               "enabled", enabled);
  if (content_view_core_ && enabled)
    content_view_core_->GetWindowAndroid()->RequestVSyncUpdate();

  needs_begin_frame_ = enabled;
}

void RenderWidgetHostViewAndroid::OnStartContentIntent(
    const GURL& content_url) {
  if (content_view_core_)
    content_view_core_->StartContentIntent(content_url);
}

void RenderWidgetHostViewAndroid::OnSmartClipDataExtracted(
    const base::string16& text,
    const base::string16& html,
    const gfx::Rect rect) {
  if (content_view_core_)
    content_view_core_->OnSmartClipDataExtracted(text, html, rect);
}

bool RenderWidgetHostViewAndroid::OnTouchEvent(
    const ui::MotionEvent& event) {
  if (!host_)
    return false;

  if (!gesture_provider_.OnTouchEvent(event))
    return false;

  if (gesture_text_selector_.OnTouchEvent(event)) {
    gesture_provider_.OnTouchEventAck(false);
    return true;
  }

  // Short-circuit touch forwarding if no touch handlers exist.
  if (!host_->ShouldForwardTouchEvent()) {
    const bool event_consumed = false;
    gesture_provider_.OnTouchEventAck(event_consumed);
    return true;
  }

  SendTouchEvent(CreateWebTouchEventFromMotionEvent(event));
  return true;
}

void RenderWidgetHostViewAndroid::ResetGestureDetection() {
  const ui::MotionEvent* current_down_event =
      gesture_provider_.GetCurrentDownEvent();
  if (!current_down_event)
    return;

  scoped_ptr<ui::MotionEvent> cancel_event = current_down_event->Cancel();
  DCHECK(cancel_event);
  OnTouchEvent(*cancel_event);
}

void RenderWidgetHostViewAndroid::SetDoubleTapSupportEnabled(bool enabled) {
  gesture_provider_.SetDoubleTapSupportForPlatformEnabled(enabled);
}

void RenderWidgetHostViewAndroid::SetMultiTouchZoomSupportEnabled(
    bool enabled) {
  gesture_provider_.SetMultiTouchZoomSupportEnabled(enabled);
}

void RenderWidgetHostViewAndroid::ImeCancelComposition() {
  ime_adapter_android_.CancelComposition();
}

void RenderWidgetHostViewAndroid::FocusedNodeChanged(bool is_editable_node) {
  ime_adapter_android_.FocusedNodeChanged(is_editable_node);
}

void RenderWidgetHostViewAndroid::RenderProcessGone(
    base::TerminationStatus status, int error_code) {
  Destroy();
}

void RenderWidgetHostViewAndroid::Destroy() {
  RemoveLayers();
  SetContentViewCore(NULL);

  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;

  delete this;
}

void RenderWidgetHostViewAndroid::SetTooltipText(
    const base::string16& tooltip_text) {
  // Tooltips don't makes sense on Android.
}

void RenderWidgetHostViewAndroid::SelectionChanged(const base::string16& text,
                                                   size_t offset,
                                                   const gfx::Range& range) {
  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);

  if (text.empty() || range.is_empty() || !content_view_core_)
    return;
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  std::string utf8_selection = base::UTF16ToUTF8(text.substr(pos, n));

  content_view_core_->OnSelectionChanged(utf8_selection);
}

void RenderWidgetHostViewAndroid::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (content_view_core_) {
    content_view_core_->OnSelectionBoundsChanged(params);
  }
}

void RenderWidgetHostViewAndroid::ScrollOffsetChanged() {
}

void RenderWidgetHostViewAndroid::SetBackgroundOpaque(bool opaque) {
  RenderWidgetHostViewBase::SetBackgroundOpaque(opaque);
  host_->SetBackgroundOpaque(opaque);
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config bitmap_config) {
  if (!IsReadbackConfigSupported(bitmap_config)) {
    callback.Run(false, SkBitmap());
    return;
  }
  base::TimeTicks start_time = base::TimeTicks::Now();
  if (!using_synchronous_compositor_ && !IsSurfaceAvailableForCopy()) {
    callback.Run(false, SkBitmap());
    return;
  }
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  float device_scale_factor = display.device_scale_factor();
  gfx::Size dst_size_in_pixel =
      ConvertRectToPixel(device_scale_factor, gfx::Rect(dst_size)).size();
  gfx::Rect src_subrect_in_pixel =
      ConvertRectToPixel(device_scale_factor, src_subrect);

  if (using_synchronous_compositor_) {
    SynchronousCopyContents(src_subrect_in_pixel, dst_size_in_pixel, callback,
                            bitmap_config);
    UMA_HISTOGRAM_TIMES("Compositing.CopyFromSurfaceTimeSynchronous",
                        base::TimeTicks::Now() - start_time);
    return;
  }

  scoped_ptr<cc::CopyOutputRequest> request;
  scoped_refptr<cc::Layer> readback_layer;
  DCHECK(content_view_core_);
  DCHECK(content_view_core_->GetWindowAndroid());
  ui::WindowAndroidCompositor* compositor =
      content_view_core_->GetWindowAndroid()->GetCompositor();
  DCHECK(compositor);
  DCHECK(frame_provider_);
  scoped_refptr<cc::DelegatedRendererLayer> delegated_layer =
      cc::DelegatedRendererLayer::Create(frame_provider_);
  delegated_layer->SetBounds(content_size_in_layer_);
  delegated_layer->SetHideLayerAndSubtree(true);
  delegated_layer->SetIsDrawable(true);
  delegated_layer->SetContentsOpaque(true);
  compositor->AttachLayerForReadback(delegated_layer);

  readback_layer = delegated_layer;
  request = cc::CopyOutputRequest::CreateRequest(
      base::Bind(&RenderWidgetHostViewAndroid::
                     PrepareTextureCopyOutputResultForDelegatedReadback,
                 dst_size_in_pixel,
                 bitmap_config,
                 start_time,
                 readback_layer,
                 callback));
  request->set_area(src_subrect_in_pixel);
  readback_layer->RequestCopyOfOutput(request.Pass());
}

void RenderWidgetHostViewAndroid::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool RenderWidgetHostViewAndroid::CanCopyToVideoFrame() const {
  return false;
}

void RenderWidgetHostViewAndroid::ShowDisambiguationPopup(
    const gfx::Rect& target_rect, const SkBitmap& zoomed_bitmap) {
  if (!content_view_core_)
    return;

  content_view_core_->ShowDisambiguationPopup(target_rect, zoomed_bitmap);
}

scoped_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAndroid::CreateSyntheticGestureTarget() {
  return scoped_ptr<SyntheticGestureTarget>(new SyntheticGestureTargetAndroid(
      host_, content_view_core_->CreateTouchEventSynthesizer()));
}

void RenderWidgetHostViewAndroid::SendDelegatedFrameAck(
    uint32 output_surface_id) {
  DCHECK(host_);
  cc::CompositorFrameAck ack;
  if (resource_collection_.get())
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(host_->GetRoutingID(),
                                                   output_surface_id,
                                                   host_->GetProcess()->GetID(),
                                                   ack);
}

void RenderWidgetHostViewAndroid::SendReturnedDelegatedResources(
    uint32 output_surface_id) {
  DCHECK(resource_collection_);

  cc::CompositorFrameAck ack;
  resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  DCHECK(!ack.resources.empty());

  RenderWidgetHostImpl::SendReclaimCompositorResources(
      host_->GetRoutingID(),
      output_surface_id,
      host_->GetProcess()->GetID(),
      ack);
}

void RenderWidgetHostViewAndroid::UnusedResourcesAreAvailable() {
  if (ack_callbacks_.size())
    return;
  SendReturnedDelegatedResources(last_output_surface_id_);
}

void RenderWidgetHostViewAndroid::DestroyDelegatedContent() {
  RemoveLayers();
  frame_provider_ = NULL;
  layer_ = NULL;
}

void RenderWidgetHostViewAndroid::SwapDelegatedFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::DelegatedFrameData> frame_data) {
  bool has_content = !texture_size_in_layer_.IsEmpty();

  if (output_surface_id != last_output_surface_id_) {
    // Drop the cc::DelegatedFrameResourceCollection so that we will not return
    // any resources from the old output surface with the new output surface id.
    if (resource_collection_.get()) {
      resource_collection_->SetClient(NULL);
      if (resource_collection_->LoseAllResources())
        SendReturnedDelegatedResources(last_output_surface_id_);
      resource_collection_ = NULL;
    }
    DestroyDelegatedContent();

    last_output_surface_id_ = output_surface_id;
  }

  // DelegatedRendererLayerImpl applies the inverse device_scale_factor of the
  // renderer frame, assuming that the browser compositor will scale
  // it back up to device scale.  But on Android we put our browser layers in
  // physical pixels and set our browser CC device_scale_factor to 1, so this
  // suppresses the transform.  This line may need to be removed when fixing
  // http://crbug.com/384134 or http://crbug.com/310763
  frame_data->device_scale_factor = 1.0f;

  if (!has_content) {
    DestroyDelegatedContent();
  } else {
    if (!resource_collection_.get()) {
      resource_collection_ = new cc::DelegatedFrameResourceCollection;
      resource_collection_->SetClient(this);
    }
    if (!frame_provider_ ||
        texture_size_in_layer_ != frame_provider_->frame_size()) {
      RemoveLayers();
      frame_provider_ = new cc::DelegatedFrameProvider(
          resource_collection_.get(), frame_data.Pass());
      layer_ = cc::DelegatedRendererLayer::Create(frame_provider_);
      AttachLayers();
    } else {
      frame_provider_->SetFrameData(frame_data.Pass());
    }
  }

  if (layer_.get()) {
    layer_->SetIsDrawable(true);
    layer_->SetContentsOpaque(true);
    layer_->SetBounds(content_size_in_layer_);
    layer_->SetNeedsDisplay();
  }

  base::Closure ack_callback =
      base::Bind(&RenderWidgetHostViewAndroid::SendDelegatedFrameAck,
                 weak_ptr_factory_.GetWeakPtr(),
                 output_surface_id);

  ack_callbacks_.push(ack_callback);
  if (host_->is_hidden())
    RunAckCallbacks();
}

void RenderWidgetHostViewAndroid::ComputeContentsSize(
    const cc::CompositorFrameMetadata& frame_metadata) {
  // Calculate the content size.  This should be 0 if the texture_size is 0.
  gfx::Vector2dF offset;
  if (texture_size_in_layer_.GetArea() > 0)
    offset = frame_metadata.location_bar_content_translation;
  offset.set_y(offset.y() + frame_metadata.overdraw_bottom_height);
  offset.Scale(frame_metadata.device_scale_factor);
  content_size_in_layer_ =
      gfx::Size(texture_size_in_layer_.width() - offset.x(),
                texture_size_in_layer_.height() - offset.y());

  overscroll_effect_->UpdateDisplayParameters(
      CreateOverscrollDisplayParameters(frame_metadata));
}

void RenderWidgetHostViewAndroid::InternalSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  if (!frame->delegated_frame_data) {
    LOG(ERROR) << "Non-delegated renderer path no longer supported";
    return;
  }

  if (locks_on_frame_count_ > 0) {
    DCHECK(HasValidFrame());
    RetainFrame(output_surface_id, frame.Pass());
    return;
  }

  if (layer_ && layer_->layer_tree_host()) {
    for (size_t i = 0; i < frame->metadata.latency_info.size(); i++) {
      scoped_ptr<cc::SwapPromise> swap_promise(
          new cc::LatencyInfoSwapPromise(frame->metadata.latency_info[i]));
      layer_->layer_tree_host()->QueueSwapPromise(swap_promise.Pass());
    }
  }

  DCHECK(!frame->delegated_frame_data->render_pass_list.empty());

  cc::RenderPass* root_pass =
      frame->delegated_frame_data->render_pass_list.back();
  texture_size_in_layer_ = root_pass->output_rect.size();
  ComputeContentsSize(frame->metadata);

  SwapDelegatedFrame(output_surface_id, frame->delegated_frame_data.Pass());
  frame_evictor_->SwappedFrame(!host_->is_hidden());

  OnFrameMetadataUpdated(frame->metadata);
}

void RenderWidgetHostViewAndroid::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  InternalSwapCompositorFrame(output_surface_id, frame.Pass());
}

void RenderWidgetHostViewAndroid::RetainFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  DCHECK(locks_on_frame_count_);

  // Store the incoming frame so that it can be swapped when all the locks have
  // been released. If there is already a stored frame, then replace and skip
  // the previous one but make sure we still eventually send the ACK. Holding
  // the ACK also blocks the renderer when its max_frames_pending is reached.
  if (last_frame_info_) {
    base::Closure ack_callback =
        base::Bind(&RenderWidgetHostViewAndroid::SendDelegatedFrameAck,
                   weak_ptr_factory_.GetWeakPtr(),
                   last_frame_info_->output_surface_id);

    ack_callbacks_.push(ack_callback);
  }

  last_frame_info_.reset(new LastFrameInfo(output_surface_id, frame.Pass()));
}

void RenderWidgetHostViewAndroid::SynchronousFrameMetadata(
    const cc::CompositorFrameMetadata& frame_metadata) {
  // This is a subset of OnSwapCompositorFrame() used in the synchronous
  // compositor flow.
  OnFrameMetadataUpdated(frame_metadata);
  ComputeContentsSize(frame_metadata);

  // DevTools ScreenCast support for Android WebView.
  if (DevToolsAgentHost::HasFor(RenderViewHost::From(GetRenderWidgetHost()))) {
    scoped_refptr<DevToolsAgentHost> dtah =
        DevToolsAgentHost::GetOrCreateFor(
            RenderViewHost::From(GetRenderWidgetHost()));
    // Unblock the compositor.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RenderViewDevToolsAgentHost::SynchronousSwapCompositorFrame,
                   static_cast<RenderViewDevToolsAgentHost*>(dtah.get()),
                   frame_metadata));
  }
}

void RenderWidgetHostViewAndroid::SetOverlayVideoMode(bool enabled) {
  if (layer_)
    layer_->SetContentsOpaque(!enabled);
}

void RenderWidgetHostViewAndroid::SynchronousCopyContents(
    const gfx::Rect& src_subrect_in_pixel,
    const gfx::Size& dst_size_in_pixel,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config config) {
  SynchronousCompositor* compositor =
      SynchronousCompositorImpl::FromID(host_->GetProcess()->GetID(),
                                        host_->GetRoutingID());
  if (!compositor) {
    callback.Run(false, SkBitmap());
    return;
  }

  SkBitmap bitmap;
  bitmap.setConfig(config,
                   dst_size_in_pixel.width(),
                   dst_size_in_pixel.height());
  bitmap.allocPixels();
  SkCanvas canvas(bitmap);
  canvas.scale(
      (float)dst_size_in_pixel.width() / (float)src_subrect_in_pixel.width(),
      (float)dst_size_in_pixel.height() / (float)src_subrect_in_pixel.height());
  compositor->DemandDrawSw(&canvas);
  callback.Run(true, bitmap);
}

void RenderWidgetHostViewAndroid::OnFrameMetadataUpdated(
    const cc::CompositorFrameMetadata& frame_metadata) {

  // Disable double tap zoom for pages that have a width=device-width or
  // narrower viewport (indicating that this is a mobile-optimized or responsive
  // web design, so text will be legible without zooming). Also disable
  // double tap and pinch for pages that prevent zooming in or out.
  bool has_mobile_viewport = HasMobileViewport(frame_metadata);
  bool has_fixed_page_scale = HasFixedPageScale(frame_metadata);
  gesture_provider_.SetDoubleTapSupportForPageEnabled(
      !has_fixed_page_scale && !has_mobile_viewport);

  if (!content_view_core_)
    return;
  // All offsets and sizes are in CSS pixels.
  content_view_core_->UpdateFrameInfo(
      frame_metadata.root_scroll_offset,
      frame_metadata.page_scale_factor,
      gfx::Vector2dF(frame_metadata.min_page_scale_factor,
                     frame_metadata.max_page_scale_factor),
      frame_metadata.root_layer_size,
      frame_metadata.viewport_size,
      frame_metadata.location_bar_offset,
      frame_metadata.location_bar_content_translation,
      frame_metadata.overdraw_bottom_height);
#if defined(VIDEO_HOLE)
  if (host_ && host_->IsRenderView()) {
    RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
        RenderViewHost::From(host_));
    rvhi->media_web_contents_observer()->OnFrameInfoUpdated();
  }
#endif  // defined(VIDEO_HOLE)
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceInitialized(int host_id,
                                                                int route_id) {
  accelerated_surface_route_id_ = route_id;
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  NOTREACHED() << "Need --composite-to-mailbox or --enable-delegated-renderer";
}

void RenderWidgetHostViewAndroid::AttachLayers() {
  if (!content_view_core_)
    return;
  if (!layer_.get())
    return;

  content_view_core_->AttachLayer(layer_);
  if (overscroll_effect_enabled_)
    overscroll_effect_->Enable();
  layer_->SetHideLayerAndSubtree(!is_showing_);
}

void RenderWidgetHostViewAndroid::RemoveLayers() {
  if (!content_view_core_)
    return;
  if (!layer_.get())
    return;

  content_view_core_->RemoveLayer(layer_);
  overscroll_effect_->Disable();
}

void RenderWidgetHostViewAndroid::SetNeedsAnimate() {
  content_view_core_->GetWindowAndroid()->SetNeedsAnimate();
}

bool RenderWidgetHostViewAndroid::Animate(base::TimeTicks frame_time) {
  return overscroll_effect_->Animate(frame_time);
}

void RenderWidgetHostViewAndroid::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceSuspend() {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::AcceleratedSurfaceRelease() {
  NOTREACHED();
}

void RenderWidgetHostViewAndroid::EvictDelegatedFrame() {
  if (layer_.get())
    DestroyDelegatedContent();
  frame_evictor_->DiscardedFrame();
}

bool RenderWidgetHostViewAndroid::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  NOTREACHED();
  return false;
}

void RenderWidgetHostViewAndroid::GetScreenInfo(blink::WebScreenInfo* result) {
  // ScreenInfo isn't tied to the widget on Android. Always return the default.
  RenderWidgetHostViewBase::GetDefaultScreenInfo(result);
}

// TODO(jrg): Find out the implications and answer correctly here,
// as we are returning the WebView and not root window bounds.
gfx::Rect RenderWidgetHostViewAndroid::GetBoundsInRootWindow() {
  return GetViewBounds();
}

gfx::GLSurfaceHandle RenderWidgetHostViewAndroid::GetCompositingSurface() {
  gfx::GLSurfaceHandle handle =
      gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_TRANSPORT);
  if (CompositorImpl::IsInitialized()) {
    handle.parent_client_id =
        ImageTransportFactoryAndroid::GetInstance()->GetChannelID();
  }
  return handle;
}

void RenderWidgetHostViewAndroid::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  const bool event_consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  gesture_provider_.OnTouchEventAck(event_consumed);
}

void RenderWidgetHostViewAndroid::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (content_view_core_)
    content_view_core_->OnGestureEventAck(event, ack_result);
}

InputEventAckState RenderWidgetHostViewAndroid::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (content_view_core_ &&
      content_view_core_->FilterInputEvent(input_event))
    return INPUT_EVENT_ACK_STATE_CONSUMED;

  if (!host_)
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  if (input_event.type == blink::WebInputEvent::GestureTapDown ||
      input_event.type == blink::WebInputEvent::TouchStart) {
    GpuDataManagerImpl* gpu_data = GpuDataManagerImpl::GetInstance();
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim && gpu_data && accelerated_surface_route_id_ &&
        gpu_data->IsDriverBugWorkaroundActive(gpu::WAKE_UP_GPU_BEFORE_DRAWING))
      shim->Send(
          new AcceleratedSurfaceMsg_WakeUpGpu(accelerated_surface_route_id_));
  }

  SynchronousCompositorImpl* compositor =
      SynchronousCompositorImpl::FromID(host_->GetProcess()->GetID(),
                                          host_->GetRoutingID());
  if (compositor)
    return compositor->HandleInputEvent(input_event);
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostViewAndroid::OnSetNeedsFlushInput() {
  if (flush_input_requested_ || !content_view_core_)
    return;
  TRACE_EVENT0("input", "RenderWidgetHostViewAndroid::OnSetNeedsFlushInput");
  flush_input_requested_ = true;
  content_view_core_->GetWindowAndroid()->RequestVSyncUpdate();
}

void RenderWidgetHostViewAndroid::CreateBrowserAccessibilityManagerIfNeeded() {
  if (!host_ || host_->accessibility_mode() != AccessibilityModeComplete)
    return;

  if (!GetBrowserAccessibilityManager()) {
    base::android::ScopedJavaLocalRef<jobject> obj;
    if (content_view_core_)
      obj = content_view_core_->GetJavaObject();
    SetBrowserAccessibilityManager(
        new BrowserAccessibilityManagerAndroid(
            obj,
            BrowserAccessibilityManagerAndroid::GetEmptyDocument(),
            host_));
  }
}

bool RenderWidgetHostViewAndroid::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAndroid::UnlockMouse() {
  NOTIMPLEMENTED();
}

// Methods called from the host to the render

void RenderWidgetHostViewAndroid::SendKeyEvent(
    const NativeWebKeyboardEvent& event) {
  if (host_)
    host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewAndroid::SendTouchEvent(
    const blink::WebTouchEvent& event) {
  if (host_)
    host_->ForwardTouchEventWithLatencyInfo(event, CreateLatencyInfo(event));

  // Send a proactive BeginFrame on the next vsync to reduce latency.
  // This is good enough as long as the first touch event has Begin semantics
  // and the actual scroll happens on the next vsync.
  // TODO: Is this actually still needed?
  if (content_view_core_) {
    content_view_core_->GetWindowAndroid()->RequestVSyncUpdate();
  }
}

void RenderWidgetHostViewAndroid::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  if (host_)
    host_->ForwardMouseEvent(event);
}

void RenderWidgetHostViewAndroid::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  if (host_)
    host_->ForwardWheelEvent(event);
}

void RenderWidgetHostViewAndroid::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  // Sending a gesture that may trigger overscroll should resume the effect.
  if (overscroll_effect_enabled_)
   overscroll_effect_->Enable();

  if (host_)
    host_->ForwardGestureEventWithLatencyInfo(event, CreateLatencyInfo(event));
}

void RenderWidgetHostViewAndroid::MoveCaret(const gfx::Point& point) {
  if (host_)
    host_->MoveCaret(point);
}

SkColor RenderWidgetHostViewAndroid::GetCachedBackgroundColor() const {
  return cached_background_color_;
}

void RenderWidgetHostViewAndroid::DidOverscroll(
    const DidOverscrollParams& params) {
  if (!content_view_core_ || !layer_ || !is_showing_)
    return;

  const float device_scale_factor = content_view_core_->GetDpiScale();
  if (overscroll_effect_->OnOverscrolled(
          content_view_core_->GetLayer(),
          base::TimeTicks::Now(),
          gfx::ScaleVector2d(params.accumulated_overscroll,
                             device_scale_factor),
          gfx::ScaleVector2d(params.latest_overscroll_delta,
                             device_scale_factor),
          gfx::ScaleVector2d(params.current_fling_velocity,
                             device_scale_factor))) {
    SetNeedsAnimate();
  }
}

void RenderWidgetHostViewAndroid::DidStopFlinging() {
  if (content_view_core_)
    content_view_core_->DidStopFlinging();
}

void RenderWidgetHostViewAndroid::SetContentViewCore(
    ContentViewCoreImpl* content_view_core) {
  RemoveLayers();
  if (observing_root_window_ && content_view_core_) {
    content_view_core_->GetWindowAndroid()->RemoveObserver(this);
    observing_root_window_ = false;
  }

  bool resize = false;
  if (content_view_core != content_view_core_) {
    ReleaseLocksOnSurface();
    resize = true;
  }

  content_view_core_ = content_view_core;

  if (GetBrowserAccessibilityManager()) {
    base::android::ScopedJavaLocalRef<jobject> obj;
    if (content_view_core_)
      obj = content_view_core_->GetJavaObject();
    GetBrowserAccessibilityManager()->ToBrowserAccessibilityManagerAndroid()->
        SetContentViewCore(obj);
  }

  AttachLayers();
  if (content_view_core_ && !using_synchronous_compositor_) {
    content_view_core_->GetWindowAndroid()->AddObserver(this);
    observing_root_window_ = true;
    if (needs_begin_frame_)
      content_view_core_->GetWindowAndroid()->RequestVSyncUpdate();
  }

  if (resize && content_view_core_)
    WasResized();
}

void RenderWidgetHostViewAndroid::RunAckCallbacks() {
  while (!ack_callbacks_.empty()) {
    ack_callbacks_.front().Run();
    ack_callbacks_.pop();
  }
}

void RenderWidgetHostViewAndroid::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if (gesture_text_selector_.OnGestureEvent(gesture))
    return;

  SendGestureEvent(CreateWebGestureEventFromGestureEventData(gesture));
}

void RenderWidgetHostViewAndroid::OnCompositingDidCommit() {
  RunAckCallbacks();
}

void RenderWidgetHostViewAndroid::OnDetachCompositor() {
  DCHECK(content_view_core_);
  DCHECK(!using_synchronous_compositor_);
  RunAckCallbacks();
}

void RenderWidgetHostViewAndroid::OnVSync(base::TimeTicks frame_time,
                                          base::TimeDelta vsync_period) {
  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::OnVSync");
  if (!host_)
    return;

  if (flush_input_requested_) {
    flush_input_requested_ = false;
    host_->FlushInput();
  }

  TRACE_EVENT0("cc", "RenderWidgetHostViewAndroid::SendBeginFrame");
  base::TimeTicks display_time = frame_time + vsync_period;

  // TODO(brianderson): Use adaptive draw-time estimation.
  base::TimeDelta estimated_browser_composite_time =
      base::TimeDelta::FromMicroseconds(
          (1.0f * base::Time::kMicrosecondsPerSecond) / (3.0f * 60));

  base::TimeTicks deadline = display_time - estimated_browser_composite_time;

  host_->Send(new ViewMsg_BeginFrame(
      host_->GetRoutingID(),
      cc::BeginFrameArgs::Create(frame_time, deadline, vsync_period)));

  if (needs_begin_frame_)
    content_view_core_->GetWindowAndroid()->RequestVSyncUpdate();
}

void RenderWidgetHostViewAndroid::OnAnimate(base::TimeTicks begin_frame_time) {
  if (Animate(begin_frame_time))
    SetNeedsAnimate();
}

void RenderWidgetHostViewAndroid::OnLostResources() {
  ReleaseLocksOnSurface();
  if (layer_.get())
    DestroyDelegatedContent();
  DCHECK(ack_callbacks_.empty());
}

// static
void
RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResultForDelegatedReadback(
    const gfx::Size& dst_size_in_pixel,
    const SkBitmap::Config config,
    const base::TimeTicks& start_time,
    scoped_refptr<cc::Layer> readback_layer,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  readback_layer->RemoveFromParent();
  PrepareTextureCopyOutputResult(
      dst_size_in_pixel, config, start_time, callback, result.Pass());
}

// static
void RenderWidgetHostViewAndroid::PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkBitmap::Config bitmap_config,
    const base::TimeTicks& start_time,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, false, SkBitmap()));

  if (!result->HasTexture() || result->IsEmpty() || result->size().IsEmpty())
    return;

  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  bitmap->setConfig(bitmap_config,
                    dst_size_in_pixel.width(),
                    dst_size_in_pixel.height(),
                    0, kOpaque_SkAlphaType);
  if (!bitmap->allocPixels())
    return;

  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();

  if (!gl_helper)
    return;

  scoped_ptr<SkAutoLockPixels> bitmap_pixels_lock(
      new SkAutoLockPixels(*bitmap));
  uint8* pixels = static_cast<uint8*>(bitmap->getPixels());

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(),
      texture_mailbox.sync_point(),
      result->size(),
      gfx::Rect(result->size()),
      dst_size_in_pixel,
      pixels,
      bitmap_config,
      base::Bind(&CopyFromCompositingSurfaceFinished,
                 callback,
                 base::Passed(&release_callback),
                 base::Passed(&bitmap),
                 start_time,
                 base::Passed(&bitmap_pixels_lock)),
      GLHelper::SCALER_QUALITY_GOOD);
}

bool RenderWidgetHostViewAndroid::IsReadbackConfigSupported(
    SkBitmap::Config bitmap_config) {
  ImageTransportFactoryAndroid* factory =
      ImageTransportFactoryAndroid::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return false;
  return gl_helper->IsReadbackConfigSupported(bitmap_config);
}

SkBitmap::Config RenderWidgetHostViewAndroid::PreferredReadbackFormat() {
  // Define the criteria here. If say the 16 texture readback is
  // supported we should go with that (this degrades quality)
  // or stick back to the default format.
  if (base::android::SysUtils::IsLowEndDevice()) {
    if (IsReadbackConfigSupported(SkBitmap::kRGB_565_Config))
      return SkBitmap::kRGB_565_Config;
  }
  return SkBitmap::kARGB_8888_Config;
}

void RenderWidgetHostViewAndroid::ShowSelectionHandlesAutomatically() {
  if (content_view_core_)
    content_view_core_->ShowSelectionHandlesAutomatically();
}

void RenderWidgetHostViewAndroid::SelectRange(
    float x1, float y1, float x2, float y2) {
  if (content_view_core_)
    static_cast<WebContentsImpl*>(content_view_core_->GetWebContents())->
        SelectRange(gfx::Point(x1, y1), gfx::Point(x2, y2));
}

void RenderWidgetHostViewAndroid::Unselect() {
  if (content_view_core_)
    content_view_core_->GetWebContents()->Unselect();
}

void RenderWidgetHostViewAndroid::LongPress(
    base::TimeTicks time, float x, float y) {
  blink::WebGestureEvent long_press = WebGestureEventBuilder::Build(
      blink::WebInputEvent::GestureLongPress,
      (time - base::TimeTicks()).InSecondsF(), x, y);
  SendGestureEvent(long_press);
}

// static
void RenderWidgetHostViewBase::GetDefaultScreenInfo(
    blink::WebScreenInfo* results) {
  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  results->rect = display.bounds();
  // TODO(husky): Remove any system controls from availableRect.
  results->availableRect = display.work_area();
  results->deviceScaleFactor = display.device_scale_factor();
  results->orientationAngle = display.RotationAsDegree();
  gfx::DeviceDisplayInfo info;
  results->depth = info.GetBitsPerPixel();
  results->depthPerComponent = info.GetBitsPerComponent();
  results->isMonochrome = (results->depthPerComponent == 0);
}

} // namespace content

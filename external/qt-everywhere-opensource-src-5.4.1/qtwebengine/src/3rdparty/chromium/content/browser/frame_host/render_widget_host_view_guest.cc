// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/common/view_messages.h"
#include "content/common/webplugin_geometry.h"
#include "content/public/common/content_switches.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

#if defined(OS_MACOSX)
#import "content/browser/renderer_host/render_widget_host_view_mac_dictionary_helper.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#endif

namespace content {

namespace {

#if defined(USE_AURA)
blink::WebGestureEvent CreateFlingCancelEvent(double time_stamp) {
  blink::WebGestureEvent gesture_event;
  gesture_event.timeStampSeconds = time_stamp;
  gesture_event.type = blink::WebGestureEvent::GestureFlingCancel;
  gesture_event.sourceDevice = blink::WebGestureDeviceTouchscreen;
  return gesture_event;
}
#endif  // defined(USE_AURA)

}  // namespace

RenderWidgetHostViewGuest::RenderWidgetHostViewGuest(
    RenderWidgetHost* widget_host,
    BrowserPluginGuest* guest,
    RenderWidgetHostViewBase* platform_view)
    : RenderWidgetHostViewChildFrame(widget_host),
      // |guest| is NULL during test.
      guest_(guest ? guest->AsWeakPtr() : base::WeakPtr<BrowserPluginGuest>()),
      platform_view_(platform_view) {
#if defined(USE_AURA)
  gesture_recognizer_.reset(ui::GestureRecognizer::Create());
  gesture_recognizer_->AddGestureEventHelper(this);
#endif  // defined(USE_AURA)
}

RenderWidgetHostViewGuest::~RenderWidgetHostViewGuest() {
#if defined(USE_AURA)
  gesture_recognizer_->RemoveGestureEventHelper(this);
#endif  // defined(USE_AURA)
}

void RenderWidgetHostViewGuest::WasShown() {
  // If the WebContents associated with us showed an interstitial page in the
  // beginning, the teardown path might call WasShown() while |host_| is in
  // the process of destruction. Avoid calling WasShown below in this case.
  // TODO(lazyboy): We shouldn't be showing interstitial pages in guests in the
  // first place: http://crbug.com/273089.
  //
  // |guest_| is NULL during test.
  if ((guest_ && guest_->is_in_destruction()) || !host_->is_hidden())
    return;
  host_->WasShown();
}

void RenderWidgetHostViewGuest::WasHidden() {
  // |guest_| is NULL during test.
  if ((guest_ && guest_->is_in_destruction()) || host_->is_hidden())
    return;
  host_->WasHidden();
}

void RenderWidgetHostViewGuest::SetSize(const gfx::Size& size) {
  size_ = size;
  host_->WasResized();
}

void RenderWidgetHostViewGuest::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

#if defined(USE_AURA)
void RenderWidgetHostViewGuest::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  // TODO(fsamuel): Currently we will only take this codepath if the guest has
  // requested touch events. A better solution is to always forward touchpresses
  // to the embedder process to target a BrowserPlugin, and then route all
  // subsequent touch points of that touchdown to the appropriate guest until
  // that touch point is released.
  ScopedVector<ui::TouchEvent> events;
  if (!MakeUITouchEventsFromWebTouchEvents(touch, &events, LOCAL_COORDINATES))
    return;

  ui::EventResult result = (ack_result ==
      INPUT_EVENT_ACK_STATE_CONSUMED) ? ui::ER_HANDLED : ui::ER_UNHANDLED;
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter)  {
    scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
    gestures.reset(gesture_recognizer_->ProcessTouchEventForGesture(
        *(*iter), result, this));
    ProcessGestures(gestures.get());
  }
}
#endif

gfx::Rect RenderWidgetHostViewGuest::GetViewBounds() const {
  if (!guest_)
    return gfx::Rect();

  RenderWidgetHostViewBase* rwhv = GetGuestRenderWidgetHostView();
  gfx::Rect embedder_bounds;
  if (rwhv)
    embedder_bounds = rwhv->GetViewBounds();
  gfx::Rect shifted_rect = guest_->ToGuestRect(embedder_bounds);
  shifted_rect.set_width(size_.width());
  shifted_rect.set_height(size_.height());
  return shifted_rect;
}

void RenderWidgetHostViewGuest::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  platform_view_->RenderProcessGone(status, error_code);
  // Destroy the guest view instance only, so we don't end up calling
  // platform_view_->Destroy().
  DestroyGuestView();
}

void RenderWidgetHostViewGuest::Destroy() {
  // The RenderWidgetHost's destruction led here, so don't call it.
  DestroyGuestView();

  platform_view_->Destroy();
}

gfx::Size RenderWidgetHostViewGuest::GetPhysicalBackingSize() const {
  return RenderWidgetHostViewBase::GetPhysicalBackingSize();
}

base::string16 RenderWidgetHostViewGuest::GetSelectedText() const {
  return platform_view_->GetSelectedText();
}

void RenderWidgetHostViewGuest::SetTooltipText(
    const base::string16& tooltip_text) {
  platform_view_->SetTooltipText(tooltip_text);
}

void RenderWidgetHostViewGuest::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  if (!guest_)
    return;

  FrameMsg_BuffersSwapped_Params guest_params;
  guest_params.size = params.size;
  guest_params.mailbox = params.mailbox;
  guest_params.gpu_route_id = params.route_id;
  guest_params.gpu_host_id = gpu_host_id;
  guest_->SendMessageToEmbedder(
      new BrowserPluginMsg_BuffersSwapped(guest_->instance_id(),
                                          guest_params));
}

void RenderWidgetHostViewGuest::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

void RenderWidgetHostViewGuest::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  if (!guest_)
    return;

  if (!guest_->attached()) {
    // If the guest doesn't have an embedder then there's nothing to give the
    // the frame to.
    return;
  }
  base::SharedMemoryHandle software_frame_handle =
      base::SharedMemory::NULLHandle();
  if (frame->software_frame_data) {
    cc::SoftwareFrameData* frame_data = frame->software_frame_data.get();
    scoped_ptr<cc::SharedBitmap> bitmap =
        HostSharedBitmapManager::current()->GetSharedBitmapFromId(
            frame_data->size, frame_data->bitmap_id);
    if (!bitmap)
      return;

    RenderWidgetHostView* embedder_rwhv =
        guest_->GetEmbedderRenderWidgetHostView();
    base::ProcessHandle embedder_pid =
        embedder_rwhv->GetRenderWidgetHost()->GetProcess()->GetHandle();

    bitmap->memory()->ShareToProcess(embedder_pid, &software_frame_handle);
  }

  FrameMsg_CompositorFrameSwapped_Params guest_params;
  frame->AssignTo(&guest_params.frame);
  guest_params.output_surface_id = output_surface_id;
  guest_params.producing_route_id = host_->GetRoutingID();
  guest_params.producing_host_id = host_->GetProcess()->GetID();
  guest_params.shared_memory_handle = software_frame_handle;

  guest_->SendMessageToEmbedder(
      new BrowserPluginMsg_CompositorFrameSwapped(guest_->instance_id(),
                                                  guest_params));
}

bool RenderWidgetHostViewGuest::OnMessageReceived(const IPC::Message& msg) {
  return platform_view_->OnMessageReceived(msg);
}

void RenderWidgetHostViewGuest::InitAsChild(
    gfx::NativeView parent_view) {
  platform_view_->InitAsChild(parent_view);
}

void RenderWidgetHostViewGuest::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  // This should never get called.
  NOTREACHED();
}

void RenderWidgetHostViewGuest::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  // This should never get called.
  NOTREACHED();
}

gfx::NativeView RenderWidgetHostViewGuest::GetNativeView() const {
  if (!guest_)
    return gfx::NativeView();

  RenderWidgetHostView* rwhv = guest_->GetEmbedderRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeView();
  return rwhv->GetNativeView();
}

gfx::NativeViewId RenderWidgetHostViewGuest::GetNativeViewId() const {
  if (!guest_)
    return static_cast<gfx::NativeViewId>(NULL);

  RenderWidgetHostView* rwhv = guest_->GetEmbedderRenderWidgetHostView();
  if (!rwhv)
    return static_cast<gfx::NativeViewId>(NULL);
  return rwhv->GetNativeViewId();
}

gfx::NativeViewAccessible RenderWidgetHostViewGuest::GetNativeViewAccessible() {
  if (!guest_)
    return gfx::NativeViewAccessible();

  RenderWidgetHostView* rwhv = guest_->GetEmbedderRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeViewAccessible();
  return rwhv->GetNativeViewAccessible();
}

void RenderWidgetHostViewGuest::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
  platform_view_->MovePluginWindows(moves);
}

void RenderWidgetHostViewGuest::UpdateCursor(const WebCursor& cursor) {
  platform_view_->UpdateCursor(cursor);
}

void RenderWidgetHostViewGuest::SetIsLoading(bool is_loading) {
  platform_view_->SetIsLoading(is_loading);
}

void RenderWidgetHostViewGuest::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetGuestRenderWidgetHostView();
  if (!rwhv)
    return;
  // Forward the information to embedding RWHV.
  rwhv->TextInputStateChanged(params);
}

void RenderWidgetHostViewGuest::ImeCancelComposition() {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetGuestRenderWidgetHostView();
  if (!rwhv)
    return;
  // Forward the information to embedding RWHV.
  rwhv->ImeCancelComposition();
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void RenderWidgetHostViewGuest::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetGuestRenderWidgetHostView();
  if (!rwhv)
    return;
  std::vector<gfx::Rect> guest_character_bounds;
  for (size_t i = 0; i < character_bounds.size(); ++i) {
    gfx::Rect guest_rect = guest_->ToGuestRect(character_bounds[i]);
    guest_character_bounds.push_back(guest_rect);
  }
  // Forward the information to embedding RWHV.
  rwhv->ImeCompositionRangeChanged(range, guest_character_bounds);
}
#endif

void RenderWidgetHostViewGuest::SelectionChanged(const base::string16& text,
                                                 size_t offset,
                                                 const gfx::Range& range) {
  platform_view_->SelectionChanged(text, offset, range);
}

void RenderWidgetHostViewGuest::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetGuestRenderWidgetHostView();
  if (!rwhv)
    return;
  ViewHostMsg_SelectionBounds_Params guest_params(params);
  guest_params.anchor_rect = guest_->ToGuestRect(params.anchor_rect);
  guest_params.focus_rect = guest_->ToGuestRect(params.focus_rect);
  rwhv->SelectionBoundsChanged(guest_params);
}

void RenderWidgetHostViewGuest::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config config) {
  CHECK(guest_);
  guest_->CopyFromCompositingSurface(src_subrect, dst_size, callback);
}

void RenderWidgetHostViewGuest::SetBackgroundOpaque(bool opaque) {
  platform_view_->SetBackgroundOpaque(opaque);
}

bool RenderWidgetHostViewGuest::LockMouse() {
  return platform_view_->LockMouse();
}

void RenderWidgetHostViewGuest::UnlockMouse() {
  return platform_view_->UnlockMouse();
}

void RenderWidgetHostViewGuest::GetScreenInfo(blink::WebScreenInfo* results) {
  if (!guest_)
    return;
  RenderWidgetHostViewBase* embedder_view = GetGuestRenderWidgetHostView();
  if (embedder_view)
    embedder_view->GetScreenInfo(results);
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewGuest::SetActive(bool active) {
  platform_view_->SetActive(active);
}

void RenderWidgetHostViewGuest::SetTakesFocusOnlyOnMouseDown(bool flag) {
  platform_view_->SetTakesFocusOnlyOnMouseDown(flag);
}

void RenderWidgetHostViewGuest::SetWindowVisibility(bool visible) {
  platform_view_->SetWindowVisibility(visible);
}

void RenderWidgetHostViewGuest::WindowFrameChanged() {
  platform_view_->WindowFrameChanged();
}

void RenderWidgetHostViewGuest::ShowDefinitionForSelection() {
  if (!guest_)
    return;

  gfx::Point origin;
  gfx::Rect guest_bounds = GetViewBounds();
  RenderWidgetHostView* rwhv = guest_->GetEmbedderRenderWidgetHostView();
  gfx::Rect embedder_bounds;
  if (rwhv)
    embedder_bounds = rwhv->GetViewBounds();

  gfx::Vector2d guest_offset = gfx::Vector2d(
      // Horizontal offset of guest from embedder.
      guest_bounds.x() - embedder_bounds.x(),
      // Vertical offset from guest's top to embedder's bottom edge.
      embedder_bounds.bottom() - guest_bounds.y());

  RenderWidgetHostViewMacDictionaryHelper helper(platform_view_);
  helper.SetTargetView(rwhv);
  helper.set_offset(guest_offset);
  helper.ShowDefinitionForSelection();
}

bool RenderWidgetHostViewGuest::SupportsSpeech() const {
  return platform_view_->SupportsSpeech();
}

void RenderWidgetHostViewGuest::SpeakSelection() {
  platform_view_->SpeakSelection();
}

bool RenderWidgetHostViewGuest::IsSpeaking() const {
  return platform_view_->IsSpeaking();
}

void RenderWidgetHostViewGuest::StopSpeaking() {
  platform_view_->StopSpeaking();
}

bool RenderWidgetHostViewGuest::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  return false;
}

#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
void RenderWidgetHostViewGuest::ShowDisambiguationPopup(
    const gfx::Rect& target_rect,
    const SkBitmap& zoomed_bitmap) {
}

void RenderWidgetHostViewGuest::LockCompositingSurface() {
}

void RenderWidgetHostViewGuest::UnlockCompositingSurface() {
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
void RenderWidgetHostViewGuest::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId RenderWidgetHostViewGuest::GetParentForWindowlessPlugin()
    const {
  return NULL;
}
#endif

void RenderWidgetHostViewGuest::DestroyGuestView() {
  host_->SetView(NULL);
  host_ = NULL;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

bool RenderWidgetHostViewGuest::CanDispatchToConsumer(
    ui::GestureConsumer* consumer) {
  CHECK_EQ(static_cast<RenderWidgetHostViewGuest*>(consumer), this);
  return true;
}

void RenderWidgetHostViewGuest::DispatchGestureEvent(
    ui::GestureEvent* event) {
  ForwardGestureEventToRenderer(event);
}

void RenderWidgetHostViewGuest::DispatchCancelTouchEvent(
    ui::TouchEvent* event) {
  if (!host_)
    return;

  blink::WebTouchEvent cancel_event;
  // TODO(rbyers): This event has no touches in it.  Don't we need to know what
  // touches are currently active in order to cancel them all properly?
  WebTouchEventTraits::ResetType(blink::WebInputEvent::TouchCancel,
                                 event->time_stamp().InSecondsF(),
                                 &cancel_event);

  host_->ForwardTouchEventWithLatencyInfo(cancel_event, *event->latency());
}

bool RenderWidgetHostViewGuest::ForwardGestureEventToRenderer(
    ui::GestureEvent* gesture) {
#if defined(USE_AURA)
  if (!host_)
    return false;

  if ((gesture->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      gesture->type() == ui::ET_GESTURE_PINCH_UPDATE ||
      gesture->type() == ui::ET_GESTURE_PINCH_END) && !pinch_zoom_enabled_) {
    return true;
  }

  blink::WebGestureEvent web_gesture =
      MakeWebGestureEventFromUIEvent(*gesture);
  const gfx::Point& client_point = gesture->location();
  const gfx::Point& screen_point = gesture->location();

  web_gesture.x = client_point.x();
  web_gesture.y = client_point.y();
  web_gesture.globalX = screen_point.x();
  web_gesture.globalY = screen_point.y();

  if (web_gesture.type == blink::WebGestureEvent::Undefined)
    return false;
  if (web_gesture.type == blink::WebGestureEvent::GestureTapDown) {
    host_->ForwardGestureEvent(
        CreateFlingCancelEvent(gesture->time_stamp().InSecondsF()));
  }
  host_->ForwardGestureEvent(web_gesture);
  return true;
#else
  return false;
#endif
}

void RenderWidgetHostViewGuest::ProcessGestures(
    ui::GestureRecognizer::Gestures* gestures) {
  if ((gestures == NULL) || gestures->empty())
    return;
  for (ui::GestureRecognizer::Gestures::iterator g_it = gestures->begin();
      g_it != gestures->end();
      ++g_it) {
    ForwardGestureEventToRenderer(*g_it);
  }
}

SkBitmap::Config RenderWidgetHostViewGuest::PreferredReadbackFormat() {
  return SkBitmap::kARGB_8888_Config;
}

RenderWidgetHostViewBase*
RenderWidgetHostViewGuest::GetGuestRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewBase*>(
      guest_->GetEmbedderRenderWidgetHostView());
}

}  // namespace content

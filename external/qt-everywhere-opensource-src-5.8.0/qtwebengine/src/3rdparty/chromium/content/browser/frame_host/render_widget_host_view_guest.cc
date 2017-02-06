// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_guest.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "gpu/ipc/common/gpu_messages.h"
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

class ScopedInputScaleDisabler {
 public:
  ScopedInputScaleDisabler(RenderWidgetHostImpl* host, float scale_factor)
      : host_(host), scale_factor_(scale_factor) {
    if (IsUseZoomForDSFEnabled())
      host_->input_router()->SetDeviceScaleFactor(1.0f);
  }

  ~ScopedInputScaleDisabler() {
    if (IsUseZoomForDSFEnabled())
      host_->input_router()->SetDeviceScaleFactor(scale_factor_);
  }

 private:
  RenderWidgetHostImpl* host_;
  float scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(ScopedInputScaleDisabler);
};

}  // namespace

RenderWidgetHostViewGuest::RenderWidgetHostViewGuest(
    RenderWidgetHost* widget_host,
    BrowserPluginGuest* guest,
    base::WeakPtr<RenderWidgetHostViewBase> platform_view)
    : RenderWidgetHostViewChildFrame(widget_host),
      // |guest| is NULL during test.
      guest_(guest ? guest->AsWeakPtr() : base::WeakPtr<BrowserPluginGuest>()),
      platform_view_(platform_view) {
  gfx::NativeView view = GetNativeView();
  if (view)
    UpdateScreenInfo(view);
}

RenderWidgetHostViewGuest::~RenderWidgetHostViewGuest() {}

bool RenderWidgetHostViewGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message,
    RenderWidgetHostImpl* embedder) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(RenderWidgetHostViewGuest, message,
                                   embedder)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_HandleInputEvent,
                        OnHandleInputEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewGuest::Show() {
  // If the WebContents associated with us showed an interstitial page in the
  // beginning, the teardown path might call WasShown() while |host_| is in
  // the process of destruction. Avoid calling WasShown below in this case.
  // TODO(lazyboy): We shouldn't be showing interstitial pages in guests in the
  // first place: http://crbug.com/273089.
  //
  // |guest_| is NULL during test.
  if ((guest_ && guest_->is_in_destruction()) || !host_->is_hidden())
    return;
  // Make sure the size of this view matches the size of the WebContentsView.
  // The two sizes may fall out of sync if we switch RenderWidgetHostViews,
  // resize, and then switch page, as is the case with interstitial pages.
  // NOTE: |guest_| is NULL in unit tests.
  if (guest_) {
    SetSize(guest_->web_contents()->GetViewBounds().size());
    // Since we were last shown, our renderer may have had a different surface
    // set (e.g. showing an interstitial), so we resend our current surface to
    // the renderer.
    if (!surface_id_.is_null()) {
      cc::SurfaceSequence sequence = cc::SurfaceSequence(
          id_allocator_->id_namespace(), next_surface_sequence_++);
      GetSurfaceManager()
          ->GetSurfaceForId(surface_id_)
          ->AddDestructionDependency(sequence);
      guest_->SetChildFrameSurface(surface_id_, current_surface_size_,
                                   current_surface_scale_factor_,
                                   sequence);
    }
  }
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewGuest::Hide() {
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

void RenderWidgetHostViewGuest::Focus() {
  // InterstitialPageImpl focuses views directly, so we place focus logic here.
  // InterstitialPages are not WebContents, and so BrowserPluginGuest does not
  // have direct access to the interstitial page's RenderWidgetHost.
  if (guest_)
    guest_->SetFocus(host_, true, blink::WebFocusTypeNone);
}

bool RenderWidgetHostViewGuest::HasFocus() const {
  if (!guest_)
    return false;
  return guest_->focused();
}

#if defined(USE_AURA)
void RenderWidgetHostViewGuest::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  // TODO(tdresser): Since all ProcessAckedTouchEvent() uses is the event id,
  // don't pass the full event object here. https://crbug.com/550581.
  GetOwnerRenderWidgetHostView()->ProcessAckedTouchEvent(touch, ack_result);
}
#endif

void RenderWidgetHostViewGuest::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  if (event.type == blink::WebInputEvent::TouchStart) {
    DCHECK(guest_->GetOwnerRenderWidgetHostView());
    RenderWidgetHostImpl* embedder = static_cast<RenderWidgetHostImpl*>(
        guest_->GetOwnerRenderWidgetHostView()->GetRenderWidgetHost());
    if (!embedder->GetView()->HasFocus())
      embedder->GetView()->Focus();

    // Since we now route GestureEvents directly to the guest renderer, we need
    // a way to make sure that the BrowserPlugin in the embedder gets focused so
    // that keyboard input (which still travels via BrowserPlugin) is routed to
    // the plugin and thus onwards to the guest.
    // TODO(wjmaclean): When we remove BrowserPlugin, delete this code.
    // http://crbug.com/533069
    if (!HasFocus()) {
      // We need to a account for the position of the guest view within the
      // embedder, as well as the fact that the embedder's host will add its
      // offset in screen coordinates before sending the event (with the latter
      // component just serving to confuse the renderer, hence why it should be
      // removed).
      gfx::Vector2d offset = GetViewBounds().origin() -
          GetOwnerRenderWidgetHostView()->GetBoundsInRootWindow().origin();
      blink::WebGestureEvent gesture_tap_event;
      gesture_tap_event.sourceDevice = blink::WebGestureDeviceTouchscreen;
      gesture_tap_event.type = blink::WebGestureEvent::GestureTapDown;
      gesture_tap_event.x = event.touches[0].position.x + offset.x();
      gesture_tap_event.y = event.touches[0].position.y + offset.y();
      gesture_tap_event.globalX = event.touches[0].screenPosition.x;
      gesture_tap_event.globalY = event.touches[0].screenPosition.y;
      GetOwnerRenderWidgetHostView()->ProcessGestureEvent(gesture_tap_event,
                                                          ui::LatencyInfo());
      gesture_tap_event.type = blink::WebGestureEvent::GestureTapCancel;
      GetOwnerRenderWidgetHostView()->ProcessGestureEvent(gesture_tap_event,
                                                          ui::LatencyInfo());
    }
  }

  host_->ForwardTouchEventWithLatencyInfo(event, latency);
}

gfx::Rect RenderWidgetHostViewGuest::GetViewBounds() const {
  if (!guest_)
    return gfx::Rect();

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  gfx::Rect embedder_bounds;
  if (rwhv)
    embedder_bounds = rwhv->GetViewBounds();
  return gfx::Rect(
      guest_->GetScreenCoordinates(embedder_bounds.origin()), size_);
}

gfx::Rect RenderWidgetHostViewGuest::GetBoundsInRootWindow() {
  return GetViewBounds();
}

void RenderWidgetHostViewGuest::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  // The |platform_view_| gets destroyed before we get here if this view
  // is for an InterstitialPage.
  if (platform_view_)
    platform_view_->RenderProcessGone(status, error_code);

  RenderWidgetHostViewChildFrame::RenderProcessGone(status, error_code);
}

void RenderWidgetHostViewGuest::Destroy() {
  RenderWidgetHostViewChildFrame::Destroy();

  if (platform_view_)  // The platform view might have been destroyed already.
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
  if (guest_)
    guest_->SetTooltipText(tooltip_text);
}

void RenderWidgetHostViewGuest::OnSwapCompositorFrame(
    uint32_t output_surface_id,
    cc::CompositorFrame frame) {
  TRACE_EVENT0("content", "RenderWidgetHostViewGuest::OnSwapCompositorFrame");

  last_scroll_offset_ = frame.metadata.root_scroll_offset;

  cc::RenderPass* root_pass =
      frame.delegated_frame_data->render_pass_list.back().get();

  gfx::Size frame_size = root_pass->output_rect.size();
  float scale_factor = frame.metadata.device_scale_factor;

  // Check whether we need to recreate the cc::Surface, which means the child
  // frame renderer has changed its output surface, or size, or scale factor.
  if (output_surface_id != last_output_surface_id_ && surface_factory_) {
    surface_factory_->Destroy(surface_id_);
    surface_factory_.reset();
  }
  if (output_surface_id != last_output_surface_id_ ||
      frame_size != current_surface_size_ ||
      scale_factor != current_surface_scale_factor_ ||
      guest_->has_attached_since_surface_set()) {
    ClearCompositorSurfaceIfNecessary();
    last_output_surface_id_ = output_surface_id;
    current_surface_size_ = frame_size;
    current_surface_scale_factor_ = scale_factor;
  }

  if (!surface_factory_) {
    cc::SurfaceManager* manager = GetSurfaceManager();
    surface_factory_ = base::WrapUnique(new cc::SurfaceFactory(manager, this));
  }

  if (surface_id_.is_null()) {
    surface_id_ = id_allocator_->GenerateId();
    surface_factory_->Create(surface_id_);

    cc::SurfaceSequence sequence = cc::SurfaceSequence(
        id_allocator_->id_namespace(), next_surface_sequence_++);
    // The renderer process will satisfy this dependency when it creates a
    // SurfaceLayer.
    cc::SurfaceManager* manager = GetSurfaceManager();
    manager->GetSurfaceForId(surface_id_)->AddDestructionDependency(sequence);
    guest_->SetChildFrameSurface(surface_id_, frame_size, scale_factor,
                                 sequence);
  }

  cc::SurfaceFactory::DrawCallback ack_callback = base::Bind(
      &RenderWidgetHostViewChildFrame::SurfaceDrawn,
      RenderWidgetHostViewChildFrame::AsWeakPtr(), output_surface_id);
  ack_pending_count_++;
  // If this value grows very large, something is going wrong.
  DCHECK(ack_pending_count_ < 1000);
  surface_factory_->SubmitCompositorFrame(surface_id_, std::move(frame),
                                          ack_callback);

  ProcessFrameSwappedCallbacks();

  // If after detaching we are sent a frame, we should finish processing it, and
  // then we should clear the surface so that we are not holding resources we
  // no longer need.
  if (!guest_ || !guest_->attached())
    ClearCompositorSurfaceIfNecessary();
}

bool RenderWidgetHostViewGuest::OnMessageReceived(const IPC::Message& msg) {
  if (!platform_view_) {
    // In theory, we can get here if there's a delay between Destroy()
    // being called and when our destructor is invoked.
    return false;
  }

  return platform_view_->OnMessageReceived(msg);
}

void RenderWidgetHostViewGuest::InitAsChild(
    gfx::NativeView parent_view) {
  platform_view_->InitAsChild(parent_view);
}

void RenderWidgetHostViewGuest::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& bounds) {
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

  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeView();
  return rwhv->GetNativeView();
}

gfx::NativeViewAccessible RenderWidgetHostViewGuest::GetNativeViewAccessible() {
  if (!guest_)
    return gfx::NativeViewAccessible();

  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeViewAccessible();
  return rwhv->GetNativeViewAccessible();
}

void RenderWidgetHostViewGuest::UpdateCursor(const WebCursor& cursor) {
  // InterstitialPages are not WebContents so we cannot intercept
  // ViewHostMsg_SetCursor for interstitial pages in BrowserPluginGuest.
  // All guest RenderViewHosts have RenderWidgetHostViewGuests however,
  // and so we will always hit this code path.
  if (!guest_)
    return;
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible()) {
    RenderWidgetHostViewBase* rwhvb = GetOwnerRenderWidgetHostView();
    if (rwhvb)
      rwhvb->UpdateCursor(cursor);
  } else {
    guest_->SendMessageToEmbedder(new BrowserPluginMsg_SetCursor(
        guest_->browser_plugin_instance_id(), cursor));
  }
}

void RenderWidgetHostViewGuest::SetIsLoading(bool is_loading) {
  platform_view_->SetIsLoading(is_loading);
}

void RenderWidgetHostViewGuest::TextInputStateChanged(
    const TextInputState& params) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  // Forward the information to embedding RWHV.
  rwhv->TextInputStateChanged(params);
}

void RenderWidgetHostViewGuest::ImeCancelComposition() {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
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

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  std::vector<gfx::Rect> guest_character_bounds;
  for (size_t i = 0; i < character_bounds.size(); ++i) {
    guest_character_bounds.push_back(gfx::Rect(
        guest_->GetScreenCoordinates(character_bounds[i].origin()),
        character_bounds[i].size()));
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

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  ViewHostMsg_SelectionBounds_Params guest_params(params);
  guest_params.anchor_rect.set_origin(
      guest_->GetScreenCoordinates(params.anchor_rect.origin()));
  guest_params.focus_rect.set_origin(
      guest_->GetScreenCoordinates(params.focus_rect.origin()));
  rwhv->SelectionBoundsChanged(guest_params);
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
  RenderWidgetHostViewBase* embedder_view = GetOwnerRenderWidgetHostView();
  if (embedder_view)
    embedder_view->GetScreenInfo(results);
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewGuest::SetActive(bool active) {
  platform_view_->SetActive(active);
}

void RenderWidgetHostViewGuest::ShowDefinitionForSelection() {
  if (!guest_)
    return;

  gfx::Point origin;
  gfx::Rect guest_bounds = GetViewBounds();
  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  gfx::Rect embedder_bounds;
  if (rwhv)
    embedder_bounds = rwhv->GetViewBounds();

  gfx::Vector2d guest_offset = gfx::Vector2d(
      // Horizontal offset of guest from embedder.
      guest_bounds.x() - embedder_bounds.x(),
      // Vertical offset from guest's top to embedder's bottom edge.
      embedder_bounds.bottom() - guest_bounds.y());

  RenderWidgetHostViewMacDictionaryHelper helper(platform_view_.get());
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
#endif  // defined(OS_MACOSX)

void RenderWidgetHostViewGuest::LockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewGuest::UnlockCompositingSurface() {
  NOTIMPLEMENTED();
}

RenderWidgetHostViewBase*
RenderWidgetHostViewGuest::GetOwnerRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewBase*>(
      guest_->GetOwnerRenderWidgetHostView());
}

void RenderWidgetHostViewGuest::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    InputEventAckState ack_result) {
  if (ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS) {
    guest_->ResendEventToEmbedder(event);
  }
}

void RenderWidgetHostViewGuest::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  bool not_consumed = ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
                      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  // GestureScrollBegin/End are always consumed by the guest, so we only
  // forward GestureScrollUpdate.
  if (event.type == blink::WebInputEvent::GestureScrollUpdate && not_consumed)
    guest_->ResendEventToEmbedder(event);
}

void RenderWidgetHostViewGuest::OnHandleInputEvent(
    RenderWidgetHostImpl* embedder,
    int browser_plugin_instance_id,
    const blink::WebInputEvent* event) {
  // WebMouseWheelEvents go into a queue, and may not be forwarded to the
  // renderer until after this method goes out of scope. Therefore we need to
  // explicitly remove the additional device scale factor from the coordinates
  // before allowing the event to be queued.
  if (IsUseZoomForDSFEnabled() &&
      event->type == blink::WebInputEvent::MouseWheel) {
    blink::WebMouseWheelEvent rescaled_event =
        *static_cast<const blink::WebMouseWheelEvent*>(event);
    rescaled_event.x /= current_device_scale_factor();
    rescaled_event.y /= current_device_scale_factor();
    rescaled_event.deltaX /= current_device_scale_factor();
    rescaled_event.deltaY /= current_device_scale_factor();
    rescaled_event.wheelTicksX /= current_device_scale_factor();
    rescaled_event.wheelTicksY /= current_device_scale_factor();
    host_->ForwardWheelEvent(rescaled_event);
    return;
  }

  ScopedInputScaleDisabler disable(host_, current_device_scale_factor());
  if (blink::WebInputEvent::isMouseEventType(event->type)) {
    // The mouse events for BrowserPlugin are modified by all
    // the CSS transforms applied on the <object> and embedder. As a result of
    // this, the coordinates passed on to the guest renderer are potentially
    // incorrect to determine the position of the context menu(they are not the
    // actual X, Y of the window). As a hack, we report the last location of a
    // right mouse up to the BrowserPluginGuest to inform it of the next
    // potential location for context menu (BUG=470087).
    // TODO(ekaramad): Find a better and more fundamental solution. Could the
    // ContextMenuParams be based on global X, Y?
    const blink::WebMouseEvent& mouse_event =
        static_cast<const blink::WebMouseEvent&>(*event);
    // A MouseDown on the ButtonRight could suggest a ContextMenu.
    if (guest_ && mouse_event.type == blink::WebInputEvent::MouseDown &&
        mouse_event.button == blink::WebPointerProperties::ButtonRight)
      guest_->SetContextMenuPosition(
          gfx::Point(mouse_event.globalX - GetViewBounds().x(),
                     mouse_event.globalY - GetViewBounds().y()));
    host_->ForwardMouseEvent(mouse_event);
    return;
  }

  if (event->type == blink::WebInputEvent::MouseWheel) {
    host_->ForwardWheelEvent(
        *static_cast<const blink::WebMouseWheelEvent*>(event));
    return;
  }

  if (blink::WebInputEvent::isKeyboardEventType(event->type)) {
    if (!embedder->GetLastKeyboardEvent())
      return;
    NativeWebKeyboardEvent keyboard_event(*embedder->GetLastKeyboardEvent());
    host_->ForwardKeyboardEvent(keyboard_event);
    return;
  }

  if (blink::WebInputEvent::isTouchEventType(event->type)) {
    if (event->type == blink::WebInputEvent::TouchStart &&
        !embedder->GetView()->HasFocus()) {
      embedder->GetView()->Focus();
    }

    host_->ForwardTouchEventWithLatencyInfo(
        *static_cast<const blink::WebTouchEvent*>(event),
        ui::LatencyInfo());
    return;
  }

  if (blink::WebInputEvent::isGestureEventType(event->type)) {
    const blink::WebGestureEvent& gesture_event =
        *static_cast<const blink::WebGestureEvent*>(event);

    // We don't forward inertial GestureScrollUpdates to the guest anymore
    // since it now receives GestureFlingStart and will have its own fling
    // curve generating GestureScrollUpdate events for it.
    // TODO(wjmaclean): Should we try to avoid creating a fling curve in the
    // embedder renderer in this case? BrowserPlugin can return 'true' for
    // handleInputEvent() on a GestureFlingStart, and we could use this as
    // a signal to let the guest handle the fling, though we'd need to be
    // sure other plugins would behave appropriately (i.e. return 'false').
    if (gesture_event.type == blink::WebInputEvent::GestureScrollUpdate &&
        gesture_event.data.scrollUpdate.inertialPhase ==
            blink::WebGestureEvent::MomentumPhase) {
      return;
    }
    host_->ForwardGestureEvent(gesture_event);
    return;
  }
}

}  // namespace content

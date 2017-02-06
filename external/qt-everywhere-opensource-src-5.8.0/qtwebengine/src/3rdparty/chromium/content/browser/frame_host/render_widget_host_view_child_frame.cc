// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_child_frame.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/browser_plugin_guest_mode.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace content {

RenderWidgetHostViewChildFrame::RenderWidgetHostViewChildFrame(
    RenderWidgetHost* widget_host)
    : host_(RenderWidgetHostImpl::From(widget_host)),
      next_surface_sequence_(1u),
      last_output_surface_id_(0),
      current_surface_scale_factor_(1.f),
      ack_pending_count_(0),
      frame_connector_(nullptr),
      begin_frame_source_(nullptr),
      observing_begin_frame_source_(false),
      parent_surface_id_namespace_(0),
      weak_factory_(this) {
  id_allocator_ = CreateSurfaceIdAllocator();
  RegisterSurfaceNamespaceId();

  host_->SetView(this);
}

RenderWidgetHostViewChildFrame::~RenderWidgetHostViewChildFrame() {
  if (!surface_id_.is_null())
    surface_factory_->Destroy(surface_id_);
}

void RenderWidgetHostViewChildFrame::SetCrossProcessFrameConnector(
    CrossProcessFrameConnector* frame_connector) {
  if (frame_connector_ == frame_connector)
    return;

  if (frame_connector_) {
    if (parent_surface_id_namespace_) {
      GetSurfaceManager()->UnregisterSurfaceNamespaceHierarchy(
          parent_surface_id_namespace_, GetSurfaceIdNamespace());
    }
    // Unregister the client here, as it is not guaranteed in tests that the
    // destructor will be called.
    GetSurfaceManager()->UnregisterSurfaceFactoryClient(
        id_allocator_->id_namespace());

    parent_surface_id_namespace_ = 0;

    // After the RenderWidgetHostViewChildFrame loses the frame_connector, it
    // won't be able to walk up the frame tree anymore. Clean up anything that
    // needs to be done through the CrossProcessFrameConnector before it's gone.

    // Unlocks the mouse if this RenderWidgetHostView holds the lock.
    UnlockMouse();
  }
  frame_connector_ = frame_connector;
  if (frame_connector_) {
    GetSurfaceManager()->RegisterSurfaceFactoryClient(
        id_allocator_->id_namespace(), this);
    RenderWidgetHostViewBase* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();
    if (parent_view) {
      parent_surface_id_namespace_ = parent_view->GetSurfaceIdNamespace();
      DCHECK_NE(parent_surface_id_namespace_, 0u);
      GetSurfaceManager()->RegisterSurfaceNamespaceHierarchy(
          parent_surface_id_namespace_, GetSurfaceIdNamespace());
    }
  }
}

void RenderWidgetHostViewChildFrame::InitAsChild(
    gfx::NativeView parent_view) {
  NOTREACHED();
}

bool RenderWidgetHostViewChildFrame::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewChildFrame, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetNeedsBeginFrames, OnSetNeedsBeginFrames)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

RenderWidgetHost* RenderWidgetHostViewChildFrame::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewChildFrame::SetSize(const gfx::Size& size) {
  host_->WasResized();
}

void RenderWidgetHostViewChildFrame::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());

  if (rect != last_screen_rect_) {
    last_screen_rect_ = rect;
    host_->SendScreenRects();
  }
}

void RenderWidgetHostViewChildFrame::Focus() {
}

bool RenderWidgetHostViewChildFrame::HasFocus() const {
  if (frame_connector_)
    return frame_connector_->HasFocus();
  return false;
}

bool RenderWidgetHostViewChildFrame::IsSurfaceAvailableForCopy() const {
  return surface_factory_ && !surface_id_.is_null();
}

void RenderWidgetHostViewChildFrame::Show() {
  if (!host_->is_hidden())
    return;
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewChildFrame::Hide() {
  if (host_->is_hidden())
    return;
  host_->WasHidden();
}

bool RenderWidgetHostViewChildFrame::IsShowing() {
  return !host_->is_hidden();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetViewBounds() const {
  gfx::Rect rect;
  if (frame_connector_) {
    rect = frame_connector_->ChildFrameRect();

    RenderWidgetHostView* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();

    // The parent_view can be null in tests when using a TestWebContents.
    if (parent_view) {
      // Translate frame_rect by the parent's RenderWidgetHostView offset.
      rect.Offset(parent_view->GetViewBounds().OffsetFromOrigin());
    }
  }
  return rect;
}

gfx::Size RenderWidgetHostViewChildFrame::GetVisibleViewportSize() const {
  // For subframes, the visual viewport corresponds to the main frame size, so
  // this bubbles up to the parent until it hits the main frame's
  // RenderWidgetHostView.
  //
  // Currently this excludes webview guests, since they expect the visual
  // viewport to return the guest's size rather than the page's; one reason why
  // is that Blink ends up using the visual viewport to calculate things like
  // window.innerWidth/innerHeight for main frames, and a guest is considered
  // to be a main frame.  This should be cleaned up eventually.
  bool is_guest = BrowserPluginGuest::IsGuest(RenderViewHostImpl::From(host_));
  if (frame_connector_ && !is_guest) {
    RenderWidgetHostView* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();
    // The parent_view can be null in unit tests when using a TestWebContents.
    if (parent_view)
      return parent_view->GetVisibleViewportSize();
  }
  return GetViewBounds().size();
}

gfx::Vector2dF RenderWidgetHostViewChildFrame::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewChildFrame::GetNativeView() const {
  NOTREACHED();
  return nullptr;
}

gfx::NativeViewAccessible
RenderWidgetHostViewChildFrame::GetNativeViewAccessible() {
  NOTREACHED();
  return nullptr;
}

void RenderWidgetHostViewChildFrame::SetBackgroundColor(SkColor color) {
  RenderWidgetHostViewBase::SetBackgroundColor(color);
  bool opaque = GetBackgroundOpaque();
  host_->SetBackgroundOpaque(opaque);
}

gfx::Size RenderWidgetHostViewChildFrame::GetPhysicalBackingSize() const {
  gfx::Size size;
  if (frame_connector_) {
    size = gfx::ScaleToCeiledSize(frame_connector_->ChildFrameRect().size(),
                                  frame_connector_->device_scale_factor());
  }
  return size;
}

void RenderWidgetHostViewChildFrame::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  // TODO(kenrb): Fix OOPIF Ime.
}

void RenderWidgetHostViewChildFrame::UpdateCursor(const WebCursor& cursor) {
  if (frame_connector_)
    frame_connector_->UpdateCursor(cursor);
}

void RenderWidgetHostViewChildFrame::SetIsLoading(bool is_loading) {
  // It is valid for an inner WebContents's SetIsLoading() to end up here.
  // This is because an inner WebContents's main frame's RenderWidgetHostView
  // is a RenderWidgetHostViewChildFrame. In contrast, when there is no
  // inner/outer WebContents, only subframe's RenderWidgetHostView can be a
  // RenderWidgetHostViewChildFrame which do not get a SetIsLoading() call.
  if (BrowserPluginGuestMode::UseCrossProcessFramesForGuests() &&
      BrowserPluginGuest::IsGuest(
          static_cast<RenderViewHostImpl*>(RenderViewHost::From(host_)))) {
    return;
  }

  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  if (frame_connector_)
    frame_connector_->RenderProcessGone();
  Destroy();
}

void RenderWidgetHostViewChildFrame::Destroy() {
  // SurfaceIdNamespaces registered with RenderWidgetHostInputEventRouter
  // have already been cleared when RenderWidgetHostViewBase notified its
  // observers of our impending destruction.
  if (frame_connector_) {
    frame_connector_->set_view(nullptr);
    SetCrossProcessFrameConnector(nullptr);
  }

  // We notify our observers about shutdown here since we are about to release
  // host_ and do not want any event calls coming from
  // RenderWidgetHostInputEventRouter afterwards.
  NotifyObserversAboutShutdown();

  host_->SetView(nullptr);
  host_ = nullptr;
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewChildFrame::SetTooltipText(
    const base::string16& tooltip_text) {
  frame_connector_->GetRootRenderWidgetHostView()->SetTooltipText(tooltip_text);
}

void RenderWidgetHostViewChildFrame::SelectionChanged(
    const base::string16& text,
    size_t offset,
    const gfx::Range& range) {
}

void RenderWidgetHostViewChildFrame::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
}

void RenderWidgetHostViewChildFrame::LockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewChildFrame::UnlockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewChildFrame::RegisterSurfaceNamespaceId() {
  // If Destroy() has been called before we get here, host_ may be null.
  if (host_ && host_->delegate() && host_->delegate()->GetInputEventRouter()) {
    RenderWidgetHostInputEventRouter* router =
        host_->delegate()->GetInputEventRouter();
    if (!router->is_registered(GetSurfaceIdNamespace()))
      router->AddSurfaceIdNamespaceOwner(GetSurfaceIdNamespace(), this);
  }
}

void RenderWidgetHostViewChildFrame::UnregisterSurfaceNamespaceId() {
  DCHECK(host_);
  if (host_->delegate() && host_->delegate()->GetInputEventRouter()) {
    host_->delegate()->GetInputEventRouter()->RemoveSurfaceIdNamespaceOwner(
        GetSurfaceIdNamespace());
  }
}

void RenderWidgetHostViewChildFrame::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    InputEventAckState ack_result) {
  if (frame_connector_ &&
      (ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
       ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS))
    frame_connector_->BubbleScrollEvent(event);
}

void RenderWidgetHostViewChildFrame::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  bool not_consumed = ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
                      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  // GestureScrollBegin/End are always consumed by the target frame, so we only
  // forward GestureScrollUpdate.
  if (frame_connector_ &&
      event.type == blink::WebInputEvent::GestureScrollUpdate && not_consumed)
    frame_connector_->BubbleScrollEvent(event);
}

void RenderWidgetHostViewChildFrame::SurfaceDrawn(uint32_t output_surface_id,
                                                  cc::SurfaceDrawStatus drawn) {
  cc::CompositorFrameAck ack;
  DCHECK_GT(ack_pending_count_, 0U);
  if (!surface_returned_resources_.empty())
    ack.resources.swap(surface_returned_resources_);
  if (host_) {
    host_->Send(new ViewMsg_SwapCompositorFrameAck(host_->GetRoutingID(),
                                                   output_surface_id, ack));
  }
  ack_pending_count_--;
}

void RenderWidgetHostViewChildFrame::OnSwapCompositorFrame(
    uint32_t output_surface_id,
    cc::CompositorFrame frame) {
  TRACE_EVENT0("content",
               "RenderWidgetHostViewChildFrame::OnSwapCompositorFrame");

  last_scroll_offset_ = frame.metadata.root_scroll_offset;

  if (!frame_connector_)
    return;

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
      scale_factor != current_surface_scale_factor_) {
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
    frame_connector_->SetChildFrameSurface(surface_id_, frame_size,
                                           scale_factor, sequence);
  }

  cc::SurfaceFactory::DrawCallback ack_callback =
      base::Bind(&RenderWidgetHostViewChildFrame::SurfaceDrawn, AsWeakPtr(),
                 output_surface_id);
  ack_pending_count_++;
  // If this value grows very large, something is going wrong.
  DCHECK_LT(ack_pending_count_, 1000U);
  surface_factory_->SubmitCompositorFrame(surface_id_, std::move(frame),
                                          ack_callback);

  ProcessFrameSwappedCallbacks();
}

void RenderWidgetHostViewChildFrame::ProcessFrameSwappedCallbacks() {
  // We only use callbacks once, therefore we make a new list for registration
  // before we start, and discard the old list entries when we are done.
  FrameSwappedCallbackList process_callbacks;
  process_callbacks.swap(frame_swapped_callbacks_);
  for (std::unique_ptr<base::Closure>& callback : process_callbacks)
    callback->Run();
}

void RenderWidgetHostViewChildFrame::GetScreenInfo(
    blink::WebScreenInfo* results) {
  if (!frame_connector_)
    return;
  frame_connector_->GetScreenInfo(results);
}

gfx::Rect RenderWidgetHostViewChildFrame::GetBoundsInRootWindow() {
  gfx::Rect rect;
  if (frame_connector_) {
    RenderWidgetHostViewBase* root_view =
        frame_connector_->GetRootRenderWidgetHostView();

    // The root_view can be null in tests when using a TestWebContents.
    if (root_view)
      rect = root_view->GetBoundsInRootWindow();
  }
  return rect;
}

void RenderWidgetHostViewChildFrame::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
  if (!frame_connector_)
    return;

  frame_connector_->ForwardProcessAckedTouchEvent(touch, ack_result);
}

bool RenderWidgetHostViewChildFrame::LockMouse() {
  if (frame_connector_)
    return frame_connector_->LockMouse();
  return false;
}

void RenderWidgetHostViewChildFrame::UnlockMouse() {
  if (host_->delegate() && host_->delegate()->HasMouseLock(host_) &&
      frame_connector_)
    frame_connector_->UnlockMouse();
}

bool RenderWidgetHostViewChildFrame::IsMouseLocked() {
  if (!host_->delegate())
    return false;

  return host_->delegate()->HasMouseLock(host_);
}

uint32_t RenderWidgetHostViewChildFrame::GetSurfaceIdNamespace() {
  return id_allocator_->id_namespace();
}

void RenderWidgetHostViewChildFrame::ProcessKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewChildFrame::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewChildFrame::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  if (event.deltaX != 0 || event.deltaY != 0)
    host_->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewChildFrame::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  if (event.type == blink::WebInputEvent::TouchStart &&
   frame_connector_ && !frame_connector_->HasFocus()) {
    frame_connector_->FocusRootView();
  }

  host_->ForwardTouchEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewChildFrame::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardGestureEventWithLatencyInfo(event, latency);
}

gfx::Point RenderWidgetHostViewChildFrame::TransformPointToRootCoordSpace(
    const gfx::Point& point) {
  if (!frame_connector_)
    return point;

  return frame_connector_->TransformPointToRootCoordSpace(point, surface_id_);
}

#if defined(OS_MACOSX)
ui::AcceleratedWidgetMac*
RenderWidgetHostViewChildFrame::GetAcceleratedWidgetMac() const {
  return nullptr;
}

void RenderWidgetHostViewChildFrame::SetActive(bool active) {
}

void RenderWidgetHostViewChildFrame::ShowDefinitionForSelection() {
}

bool RenderWidgetHostViewChildFrame::SupportsSpeech() const {
  return false;
}

void RenderWidgetHostViewChildFrame::SpeakSelection() {
}

bool RenderWidgetHostViewChildFrame::IsSpeaking() const {
  return false;
}

void RenderWidgetHostViewChildFrame::StopSpeaking() {
}
#endif  // defined(OS_MACOSX)

void RenderWidgetHostViewChildFrame::RegisterFrameSwappedCallback(
    std::unique_ptr<base::Closure> callback) {
  frame_swapped_callbacks_.push_back(std::move(callback));
}

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  if (!IsSurfaceAvailableForCopy()) {
    // Defer submitting the copy request until after a frame is drawn, at which
    // point we should be guaranteed that the surface is available.
    RegisterFrameSwappedCallback(base::WrapUnique(new base::Closure(base::Bind(
        &RenderWidgetHostViewChildFrame::SubmitSurfaceCopyRequest, AsWeakPtr(),
        src_subrect, output_size, callback, preferred_color_type))));
    return;
  }

  SubmitSurfaceCopyRequest(src_subrect, output_size, callback,
                           preferred_color_type);
}

void RenderWidgetHostViewChildFrame::SubmitSurfaceCopyRequest(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  DCHECK(IsSurfaceAvailableForCopy());

  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(
          base::Bind(&CopyFromCompositingSurfaceHasResult, output_size,
                     preferred_color_type, callback));
  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);

  surface_factory_->RequestCopyOfSurface(surface_id_, std::move(request));
}

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(gfx::Rect(), false);
}

bool RenderWidgetHostViewChildFrame::CanCopyToVideoFrame() const {
  return false;
}

bool RenderWidgetHostViewChildFrame::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

// cc::SurfaceFactoryClient implementation.
void RenderWidgetHostViewChildFrame::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;

  if (!ack_pending_count_ && host_) {
    cc::CompositorFrameAck ack;
    std::copy(resources.begin(), resources.end(),
              std::back_inserter(ack.resources));
    host_->Send(new ViewMsg_ReclaimCompositorResources(
        host_->GetRoutingID(), last_output_surface_id_, ack));
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void RenderWidgetHostViewChildFrame::SetBeginFrameSource(
    cc::BeginFrameSource* source) {
  if (begin_frame_source_ && observing_begin_frame_source_)
    begin_frame_source_->RemoveObserver(this);
  begin_frame_source_ = source;
  if (begin_frame_source_ && observing_begin_frame_source_)
    begin_frame_source_->AddObserver(this);
}

void RenderWidgetHostViewChildFrame::OnBeginFrame(
    const cc::BeginFrameArgs& args) {
  host_->Send(new ViewMsg_BeginFrame(host_->GetRoutingID(), args));
  last_begin_frame_args_ = args;
}

const cc::BeginFrameArgs&
RenderWidgetHostViewChildFrame::LastUsedBeginFrameArgs() const {
  return last_begin_frame_args_;
}

void RenderWidgetHostViewChildFrame::OnBeginFrameSourcePausedChanged(
    bool paused) {
  // Only used on Android WebView.
}

void RenderWidgetHostViewChildFrame::OnSetNeedsBeginFrames(
    bool needs_begin_frames) {
  if (observing_begin_frame_source_ == needs_begin_frames)
    return;

  observing_begin_frame_source_ = needs_begin_frames;
  if (begin_frame_source_) {
    if (observing_begin_frame_source_)
      begin_frame_source_->AddObserver(this);
    else
      begin_frame_source_->RemoveObserver(this);
  }
}

InputEventAckState RenderWidgetHostViewChildFrame::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  if (input_event.type == blink::WebInputEvent::GestureFlingStart) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // Zero-velocity touchpad flings are an Aura-specific signal that the
    // touchpad scroll has ended, and should not be forwarded to the renderer.
    if (gesture_event.sourceDevice == blink::WebGestureDeviceTouchpad &&
        !gesture_event.data.flingStart.velocityX &&
        !gesture_event.data.flingStart.velocityY) {
      // Here we indicate that there was no consumer for this event, as
      // otherwise the fling animation system will try to run an animation
      // and will also expect a notification when the fling ends. Since
      // CrOS just uses the GestureFlingStart with zero-velocity as a means
      // of indicating that touchpad scroll has ended, we don't actually want
      // a fling animation.
      // Note: this event handling is modeled on similar code in
      // TenderWidgetHostViewAura::FilterInputEvent().
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    }
  }

  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

BrowserAccessibilityManager*
RenderWidgetHostViewChildFrame::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate, bool for_root_frame) {
  return BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
}

void RenderWidgetHostViewChildFrame::ClearCompositorSurfaceIfNecessary() {
  if (surface_factory_ && !surface_id_.is_null())
    surface_factory_->Destroy(surface_id_);
  surface_id_ = cc::SurfaceId();
}

bool RenderWidgetHostViewChildFrame::IsChildFrameForTesting() const {
  return true;
}

cc::SurfaceId RenderWidgetHostViewChildFrame::SurfaceIdForTesting() const {
  return surface_id_;
};

}  // namespace content

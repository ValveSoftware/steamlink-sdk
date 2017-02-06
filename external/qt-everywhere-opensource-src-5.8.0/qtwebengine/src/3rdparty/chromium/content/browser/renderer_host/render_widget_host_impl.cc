// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_impl.h"

#include <math.h>

#include <set>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_metadata_util.h"
#include "content/browser/renderer_host/input/input_router_config_helper.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/frame_messages.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/common/input_messages.h"
#include "content/common/resize_params.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/web_preferences.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/snapshot/snapshot.h"

#if defined(OS_MACOSX)
#include "device/power_save_blocker/power_save_blocker.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTextDirection;

namespace content {
namespace {

bool g_check_for_pending_resize_ack = true;

// <process id, routing id>
using RenderWidgetHostID = std::pair<int32_t, int32_t>;
using RoutingIDWidgetMap =
    base::hash_map<RenderWidgetHostID, RenderWidgetHostImpl*>;
base::LazyInstance<RoutingIDWidgetMap> g_routing_id_widget_map =
    LAZY_INSTANCE_INITIALIZER;

// Implements the RenderWidgetHostIterator interface. It keeps a list of
// RenderWidgetHosts, and makes sure it returns a live RenderWidgetHost at each
// iteration (or NULL if there isn't any left).
class RenderWidgetHostIteratorImpl : public RenderWidgetHostIterator {
 public:
  RenderWidgetHostIteratorImpl()
      : current_index_(0) {
  }

  ~RenderWidgetHostIteratorImpl() override {}

  void Add(RenderWidgetHost* host) {
    hosts_.push_back(RenderWidgetHostID(host->GetProcess()->GetID(),
                                        host->GetRoutingID()));
  }

  // RenderWidgetHostIterator:
  RenderWidgetHost* GetNextHost() override {
    RenderWidgetHost* host = NULL;
    while (current_index_ < hosts_.size() && !host) {
      RenderWidgetHostID id = hosts_[current_index_];
      host = RenderWidgetHost::FromID(id.first, id.second);
      ++current_index_;
    }
    return host;
  }

 private:
  std::vector<RenderWidgetHostID> hosts_;
  size_t current_index_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostIteratorImpl);
};

inline blink::WebGestureEvent CreateScrollBeginForWrapping(
    const blink::WebGestureEvent& gesture_event) {
  DCHECK(gesture_event.type == blink::WebInputEvent::GestureScrollUpdate);

  blink::WebGestureEvent wrap_gesture_scroll_begin;
  wrap_gesture_scroll_begin.type = blink::WebInputEvent::GestureScrollBegin;
  wrap_gesture_scroll_begin.timeStampSeconds = gesture_event.timeStampSeconds;
  wrap_gesture_scroll_begin.sourceDevice = gesture_event.sourceDevice;
  wrap_gesture_scroll_begin.data.scrollBegin.deltaXHint = 0;
  wrap_gesture_scroll_begin.data.scrollBegin.deltaYHint = 0;
  wrap_gesture_scroll_begin.resendingPluginId = gesture_event.resendingPluginId;

  return wrap_gesture_scroll_begin;
}

inline blink::WebGestureEvent CreateScrollEndForWrapping(
    const blink::WebGestureEvent& gesture_event) {
  DCHECK(gesture_event.type == blink::WebInputEvent::GestureScrollUpdate);

  blink::WebGestureEvent wrap_gesture_scroll_end;
  wrap_gesture_scroll_end.type = blink::WebInputEvent::GestureScrollEnd;
  wrap_gesture_scroll_end.timeStampSeconds = gesture_event.timeStampSeconds;
  wrap_gesture_scroll_end.sourceDevice = gesture_event.sourceDevice;
  wrap_gesture_scroll_end.resendingPluginId = gesture_event.resendingPluginId;

  return wrap_gesture_scroll_end;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostImpl

RenderWidgetHostImpl::RenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                                           RenderProcessHost* process,
                                           int32_t routing_id,
                                           bool hidden)
    : renderer_initialized_(false),
      destroyed_(false),
      delegate_(delegate),
      owner_delegate_(nullptr),
      process_(process),
      routing_id_(routing_id),
      is_loading_(false),
      is_hidden_(hidden),
      repaint_ack_pending_(false),
      resize_ack_pending_(false),
      auto_resize_enabled_(false),
      waiting_for_screen_rects_ack_(false),
      needs_repainting_on_restore_(false),
      is_unresponsive_(false),
      in_flight_event_count_(0),
      in_get_backing_store_(false),
      ignore_input_events_(false),
      text_direction_updated_(false),
      text_direction_(blink::WebTextDirectionLeftToRight),
      text_direction_canceled_(false),
      suppress_next_char_events_(false),
      pending_mouse_lock_request_(false),
      allow_privileged_mouse_lock_(false),
      has_touch_handler_(false),
      is_in_touchpad_gesture_scroll_(false),
      is_in_touchscreen_gesture_scroll_(false),
      received_paint_after_load_(false),
      next_browser_snapshot_id_(1),
      owned_by_render_frame_host_(false),
      is_focused_(false),
      hung_renderer_delay_(
          base::TimeDelta::FromMilliseconds(kHungRendererDelayMs)),
      hang_monitor_reason_(
          RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN),
      new_content_rendering_delay_(
          base::TimeDelta::FromMilliseconds(kNewContentRenderingDelayMs)),
      weak_factory_(this) {
  CHECK(delegate_);
  CHECK_NE(MSG_ROUTING_NONE, routing_id_);

#if defined(OS_WIN)
  // Update the display color profile cache so that it is likely to be up to
  // date when the renderer process requests the color profile.
  if (gfx::ColorSpace::CachedProfilesNeedUpdate()) {
    BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(&gfx::ColorSpace::UpdateCachedProfilesOnBackgroundThread));
  }
#endif

  std::pair<RoutingIDWidgetMap::iterator, bool> result =
      g_routing_id_widget_map.Get().insert(std::make_pair(
          RenderWidgetHostID(process->GetID(), routing_id_), this));
  CHECK(result.second) << "Inserting a duplicate item!";
  process_->AddRoute(routing_id_, this);

  // If we're initially visible, tell the process host that we're alive.
  // Otherwise we'll notify the process host when we are first shown.
  if (!hidden)
    process_->WidgetRestored();

  latency_tracker_.Initialize(routing_id_, GetProcess()->GetID());

  input_router_.reset(new InputRouterImpl(
      process_, this, this, routing_id_, GetInputRouterConfigForPlatform()));

  touch_emulator_.reset();

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    hang_monitor_timeout_.reset(new TimeoutMonitor(
        base::Bind(&RenderWidgetHostImpl::RendererIsUnresponsive,
                   weak_factory_.GetWeakPtr())));
  }

  new_content_rendering_timeout_.reset(new TimeoutMonitor(
      base::Bind(&RenderWidgetHostImpl::ClearDisplayedGraphics,
                 weak_factory_.GetWeakPtr())));

  delegate_->RenderWidgetCreated(this);
}

RenderWidgetHostImpl::~RenderWidgetHostImpl() {
  if (!destroyed_)
    Destroy(false);
}

// static
RenderWidgetHost* RenderWidgetHost::FromID(
    int32_t process_id,
    int32_t routing_id) {
  return RenderWidgetHostImpl::FromID(process_id, routing_id);
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::FromID(
    int32_t process_id,
    int32_t routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  RoutingIDWidgetMap::iterator it = widgets->find(
      RenderWidgetHostID(process_id, routing_id));
  return it == widgets->end() ? NULL : it->second;
}

// static
std::unique_ptr<RenderWidgetHostIterator>
RenderWidgetHost::GetRenderWidgetHosts() {
  std::unique_ptr<RenderWidgetHostIteratorImpl> hosts(
      new RenderWidgetHostIteratorImpl());
  for (auto& it : g_routing_id_widget_map.Get()) {
    RenderWidgetHost* widget = it.second;

    RenderViewHost* rvh = RenderViewHost::From(widget);
    if (!rvh) {
      hosts->Add(widget);
      continue;
    }

    // For RenderViewHosts, add only active ones.
    if (static_cast<RenderViewHostImpl*>(rvh)->is_active())
      hosts->Add(widget);
  }

  return std::move(hosts);
}

// static
std::unique_ptr<RenderWidgetHostIterator>
RenderWidgetHostImpl::GetAllRenderWidgetHosts() {
  std::unique_ptr<RenderWidgetHostIteratorImpl> hosts(
      new RenderWidgetHostIteratorImpl());
  for (auto& it : g_routing_id_widget_map.Get())
    hosts->Add(it.second);

  return std::move(hosts);
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::From(RenderWidgetHost* rwh) {
  return static_cast<RenderWidgetHostImpl*>(rwh);
}

void RenderWidgetHostImpl::SetView(RenderWidgetHostViewBase* view) {
  if (view)
    view_ = view->GetWeakPtr();
  else
    view_.reset();

  // If the renderer has not yet been initialized, then the surface ID
  // namespace will be sent during initialization.
  if (view_ && renderer_initialized_) {
    Send(new ViewMsg_SetSurfaceIdNamespace(routing_id_,
                                           view_->GetSurfaceIdNamespace()));
  }

  synthetic_gesture_controller_.reset();
}

RenderProcessHost* RenderWidgetHostImpl::GetProcess() const {
  return process_;
}

int RenderWidgetHostImpl::GetRoutingID() const {
  return routing_id_;
}

RenderWidgetHostViewBase* RenderWidgetHostImpl::GetView() const {
  return view_.get();
}

void RenderWidgetHostImpl::ResetSizeAndRepaintPendingFlags() {
  resize_ack_pending_ = false;
  if (repaint_ack_pending_) {
    TRACE_EVENT_ASYNC_END0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
  }
  repaint_ack_pending_ = false;
  if (old_resize_params_)
    old_resize_params_->new_size = gfx::Size();
}

void RenderWidgetHostImpl::SendScreenRects() {
  if (!renderer_initialized_ || waiting_for_screen_rects_ack_)
    return;

  if (is_hidden_) {
    // On GTK, this comes in for backgrounded tabs. Ignore, to match what
    // happens on Win & Mac, and when the view is shown it'll call this again.
    return;
  }

  if (!view_)
    return;

  last_view_screen_rect_ = view_->GetViewBounds();
  last_window_screen_rect_ = view_->GetBoundsInRootWindow();
  Send(new ViewMsg_UpdateScreenRects(
      GetRoutingID(), last_view_screen_rect_, last_window_screen_rect_));
  waiting_for_screen_rects_ack_ = true;
}

void RenderWidgetHostImpl::SuppressNextCharEvents() {
  suppress_next_char_events_ = true;
}

void RenderWidgetHostImpl::FlushInput() {
  input_router_->RequestNotificationWhenFlushed();
  if (synthetic_gesture_controller_)
    synthetic_gesture_controller_->Flush(base::TimeTicks::Now());
}

void RenderWidgetHostImpl::SetNeedsFlush() {
  if (view_)
    view_->OnSetNeedsFlushInput();
}

void RenderWidgetHostImpl::Init() {
  DCHECK(process_->HasConnection());

  renderer_initialized_ = true;

  // If the RWHV has not yet been set, the surface ID namespace will get
  // passed down by the call to SetView().
  if (view_) {
    Send(new ViewMsg_SetSurfaceIdNamespace(routing_id_,
                                           view_->GetSurfaceIdNamespace()));
  }

  SendScreenRects();
  WasResized();

  if (owner_delegate_)
    owner_delegate_->RenderWidgetDidInit();
}

void RenderWidgetHostImpl::InitForFrame() {
  DCHECK(process_->HasConnection());
  renderer_initialized_ = true;
}

void RenderWidgetHostImpl::ShutdownAndDestroyWidget(bool also_delete) {
  RejectMouseLockOrUnlockIfNecessary();

  if (process_->HasConnection()) {
    // Tell the renderer object to close.
    bool rv = Send(new ViewMsg_Close(routing_id_));
    DCHECK(rv);
  }

  Destroy(also_delete);
}

bool RenderWidgetHostImpl::IsLoading() const {
  return is_loading_;
}

bool RenderWidgetHostImpl::OnMessageReceived(const IPC::Message &msg) {
  // Only process messages if the RenderWidget is alive.
  if (!renderer_initialized())
    return false;

  if (owner_delegate_ && owner_delegate_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_HANDLER(FrameHostMsg_HittestData, OnHittestData)
    IPC_MESSAGE_HANDLER(InputHostMsg_QueueSyntheticGesture,
                        OnQueueSyntheticGesture)
    IPC_MESSAGE_HANDLER(InputHostMsg_ImeCancelComposition,
                        OnImeCancelComposition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateScreenRects_ACK,
                        OnUpdateScreenRectsAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetTooltipText, OnSetTooltipText)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_SwapCompositorFrame,
                                OnSwapCompositorFrame(msg))
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputStateChanged,
                        OnTextInputStateChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse, OnLockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse, OnUnlockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowDisambiguationPopup,
                        OnShowDisambiguationPopup)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionChanged, OnSelectionChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionBoundsChanged,
                        OnSelectionBoundsChanged)
    IPC_MESSAGE_HANDLER(InputHostMsg_ImeCompositionRangeChanged,
                        OnImeCompositionRangeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidFirstPaintAfterLoad,
                        OnFirstPaintAfterLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardCompositorProto,
                        OnForwardCompositorProto)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled && input_router_ && input_router_->OnMessageReceived(msg))
    return true;

  if (!handled && view_ && view_->OnMessageReceived(msg))
    return true;

  return handled;
}

bool RenderWidgetHostImpl::Send(IPC::Message* msg) {
  if (IPC_MESSAGE_ID_CLASS(msg->type()) == InputMsgStart)
    return input_router_->SendInput(base::WrapUnique(msg));

  return process_->Send(msg);
}

void RenderWidgetHostImpl::SetIsLoading(bool is_loading) {
  if (owner_delegate_)
    owner_delegate_->RenderWidgetWillSetIsLoading(is_loading);

  is_loading_ = is_loading;
  if (view_)
    view_->SetIsLoading(is_loading);
}

void RenderWidgetHostImpl::WasHidden() {
  if (is_hidden_)
    return;

  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::WasHidden");
  is_hidden_ = true;

  // Don't bother reporting hung state when we aren't active.
  StopHangMonitorTimeout();

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  Send(new ViewMsg_WasHidden(routing_id_));

  // Tell the RenderProcessHost we were hidden.
  process_->WidgetHidden();

  bool is_visible = false;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));
}

void RenderWidgetHostImpl::WasShown(const ui::LatencyInfo& latency_info) {
  if (!is_hidden_)
    return;

  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::WasShown");
  is_hidden_ = false;

  SendScreenRects();

  // When hidden, timeout monitoring for input events is disabled. Restore it
  // now to ensure consistent hang detection.
  if (in_flight_event_count_) {
    RestartHangMonitorTimeout();
    hang_monitor_reason_ =
        RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_IN_FLIGHT_EVENTS;
  }

  // Always repaint on restore.
  bool needs_repainting = true;
  needs_repainting_on_restore_ = false;
  Send(new ViewMsg_WasShown(routing_id_, needs_repainting, latency_info));

  process_->WidgetRestored();

  bool is_visible = true;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(this),
      Details<bool>(&is_visible));

  // It's possible for our size to be out of sync with the renderer. The
  // following is one case that leads to this:
  // 1. WasResized -> Send ViewMsg_Resize to render
  // 2. WasResized -> do nothing as resize_ack_pending_ is true
  // 3. WasHidden
  // 4. OnUpdateRect from (1) processed. Does NOT invoke WasResized as view
  //    is hidden. Now renderer/browser out of sync with what they think size
  //    is.
  // By invoking WasResized the renderer is updated as necessary. WasResized
  // does nothing if the sizes are already in sync.
  //
  // TODO: ideally ViewMsg_WasShown would take a size. This way, the renderer
  // could handle both the restore and resize at once. This isn't that big a
  // deal as RenderWidget::WasShown delays updating, so that the resize from
  // WasResized is usually processed before the renderer is painted.
  WasResized();
}

bool RenderWidgetHostImpl::GetResizeParams(ResizeParams* resize_params) {
  *resize_params = ResizeParams();

  GetWebScreenInfo(&resize_params->screen_info);
  if (delegate_) {
    resize_params->resizer_rect = delegate_->GetRootWindowResizerRect(this);
    resize_params->is_fullscreen_granted =
        delegate_->IsFullscreenForCurrentTab();
    resize_params->display_mode = delegate_->GetDisplayMode(this);
  } else {
    resize_params->is_fullscreen_granted = false;
    resize_params->display_mode = blink::WebDisplayModeBrowser;
  }

  if (view_) {
    resize_params->new_size = view_->GetRequestedRendererSize();
    resize_params->physical_backing_size = view_->GetPhysicalBackingSize();
    resize_params->top_controls_height = view_->GetTopControlsHeight();
    resize_params->top_controls_shrink_blink_size =
        view_->DoTopControlsShrinkBlinkSize();
    resize_params->visible_viewport_size = view_->GetVisibleViewportSize();
  }

  const bool size_changed =
      !old_resize_params_ ||
      old_resize_params_->new_size != resize_params->new_size ||
      (old_resize_params_->physical_backing_size.IsEmpty() &&
       !resize_params->physical_backing_size.IsEmpty());
  bool dirty = size_changed ||
      old_resize_params_->screen_info != resize_params->screen_info ||
      old_resize_params_->physical_backing_size !=
          resize_params->physical_backing_size ||
      old_resize_params_->is_fullscreen_granted !=
          resize_params->is_fullscreen_granted ||
      old_resize_params_->display_mode != resize_params->display_mode ||
      old_resize_params_->top_controls_height !=
          resize_params->top_controls_height ||
      old_resize_params_->top_controls_shrink_blink_size !=
          resize_params->top_controls_shrink_blink_size ||
      old_resize_params_->visible_viewport_size !=
          resize_params->visible_viewport_size;

  // We don't expect to receive an ACK when the requested size or the physical
  // backing size is empty, or when the main viewport size didn't change.
  resize_params->needs_resize_ack =
      g_check_for_pending_resize_ack && !resize_params->new_size.IsEmpty() &&
      !resize_params->physical_backing_size.IsEmpty() && size_changed;

  return dirty;
}

void RenderWidgetHostImpl::SetInitialRenderSizeParams(
    const ResizeParams& resize_params) {
  resize_ack_pending_ = resize_params.needs_resize_ack;

  old_resize_params_ = base::WrapUnique(new ResizeParams(resize_params));
}

void RenderWidgetHostImpl::WasResized() {
  // Skip if the |delegate_| has already been detached because
  // it's web contents is being deleted.
  if (resize_ack_pending_ || !process_->HasConnection() || !view_ ||
      !renderer_initialized_ || auto_resize_enabled_ || !delegate_) {
    return;
  }

  std::unique_ptr<ResizeParams> params(new ResizeParams);
  if (!GetResizeParams(params.get()))
    return;

  bool width_changed =
      !old_resize_params_ ||
      old_resize_params_->new_size.width() != params->new_size.width();
  if (Send(new ViewMsg_Resize(routing_id_, *params))) {
    resize_ack_pending_ = params->needs_resize_ack;
    old_resize_params_.swap(params);
  }

  if (delegate_)
    delegate_->RenderWidgetWasResized(this, width_changed);
}

void RenderWidgetHostImpl::ResizeRectChanged(const gfx::Rect& new_rect) {
  Send(new ViewMsg_ChangeResizeRect(routing_id_, new_rect));
}

void RenderWidgetHostImpl::GotFocus() {
  Focus();
  if (owner_delegate_)
    owner_delegate_->RenderWidgetGotFocus();
  if (delegate_)
    delegate_->RenderWidgetGotFocus(this);
}

void RenderWidgetHostImpl::Focus() {
  is_focused_ = true;

  Send(new InputMsg_SetFocus(routing_id_, true));

  // Also send page-level focus state to other SiteInstances involved in
  // rendering the current FrameTree.
  if (RenderViewHost::From(this) && delegate_)
    delegate_->ReplicatePageFocus(true);
}

void RenderWidgetHostImpl::Blur() {
  is_focused_ = false;

  // If there is a pending mouse lock request, we don't want to reject it at
  // this point. The user can switch focus back to this view and approve the
  // request later.
  if (IsMouseLocked())
    view_->UnlockMouse();

  if (touch_emulator_)
    touch_emulator_->CancelTouch();

  Send(new InputMsg_SetFocus(routing_id_, false));

  // Also send page-level focus state to other SiteInstances involved in
  // rendering the current FrameTree.
  if (RenderViewHost::From(this) && delegate_)
    delegate_->ReplicatePageFocus(false);
}

void RenderWidgetHostImpl::LostCapture() {
  if (touch_emulator_)
    touch_emulator_->CancelTouch();

  Send(new InputMsg_MouseCaptureLost(routing_id_));

  if (delegate_)
    delegate_->LostCapture(this);
}

void RenderWidgetHostImpl::SetActive(bool active) {
  Send(new ViewMsg_SetActive(routing_id_, active));
}

void RenderWidgetHostImpl::LostMouseLock() {
  if (delegate_)
    delegate_->LostMouseLock(this);
}

void RenderWidgetHostImpl::SendMouseLockLost() {
  Send(new ViewMsg_MouseLockLost(routing_id_));
}

void RenderWidgetHostImpl::ViewDestroyed() {
  RejectMouseLockOrUnlockIfNecessary();

  // TODO(evanm): tracking this may no longer be necessary;
  // eliminate this function if so.
  SetView(NULL);
}

void RenderWidgetHostImpl::CopyFromBackingStore(
    const gfx::Rect& src_subrect,
    const gfx::Size& accelerated_dst_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  if (view_) {
    TRACE_EVENT0("browser",
        "RenderWidgetHostImpl::CopyFromBackingStore::FromCompositingSurface");
    gfx::Rect accelerated_copy_rect = src_subrect.IsEmpty() ?
        gfx::Rect(view_->GetViewBounds().size()) : src_subrect;
    view_->CopyFromCompositingSurface(accelerated_copy_rect,
                                      accelerated_dst_size, callback,
                                      preferred_color_type);
    return;
  }

  callback.Run(SkBitmap(), content::READBACK_FAILED);
}

bool RenderWidgetHostImpl::CanCopyFromBackingStore() {
  if (view_)
    return view_->IsSurfaceAvailableForCopy();
  return false;
}

void RenderWidgetHostImpl::LockBackingStore() {
  if (view_)
    view_->LockCompositingSurface();
}

void RenderWidgetHostImpl::UnlockBackingStore() {
  if (view_)
    view_->UnlockCompositingSurface();
}

#if defined(OS_MACOSX)
void RenderWidgetHostImpl::PauseForPendingResizeOrRepaints() {
  TRACE_EVENT0("browser",
      "RenderWidgetHostImpl::PauseForPendingResizeOrRepaints");

  if (!CanPauseForPendingResizeOrRepaints())
    return;

  WaitForSurface();
}

bool RenderWidgetHostImpl::CanPauseForPendingResizeOrRepaints() {
  // Do not pause if the view is hidden.
  if (is_hidden())
    return false;

  // Do not pause if there is not a paint or resize already coming.
  if (!repaint_ack_pending_ && !resize_ack_pending_)
    return false;

  return true;
}

void RenderWidgetHostImpl::WaitForSurface() {
  // How long to (synchronously) wait for the renderer to respond with a
  // new frame when our current frame doesn't exist or is the wrong size.
  // This timeout impacts the "choppiness" of our window resize.
  const int kPaintMsgTimeoutMS = 50;

  if (!view_)
    return;

  // The view_size will be current_size_ for auto-sized views and otherwise the
  // size of the view_. (For auto-sized views, current_size_ is updated during
  // UpdateRect messages.)
  gfx::Size view_size = current_size_;
  if (!auto_resize_enabled_) {
    // Get the desired size from the current view bounds.
    gfx::Rect view_rect = view_->GetViewBounds();
    if (view_rect.IsEmpty())
      return;
    view_size = view_rect.size();
  }

  TRACE_EVENT2("renderer_host",
               "RenderWidgetHostImpl::WaitForSurface",
               "width",
               base::IntToString(view_size.width()),
               "height",
               base::IntToString(view_size.height()));

  // We should not be asked to paint while we are hidden.  If we are hidden,
  // then it means that our consumer failed to call WasShown.
  DCHECK(!is_hidden_) << "WaitForSurface called while hidden!";

  // We should never be called recursively; this can theoretically lead to
  // infinite recursion and almost certainly leads to lower performance.
  DCHECK(!in_get_backing_store_) << "WaitForSurface called recursively!";
  base::AutoReset<bool> auto_reset_in_get_backing_store(
      &in_get_backing_store_, true);

  // We might have a surface that we can use already.
  if (view_->HasAcceleratedSurface(view_size))
    return;

  // Request that the renderer produce a frame of the right size, if it
  // hasn't been requested already.
  if (!repaint_ack_pending_ && !resize_ack_pending_) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    TRACE_EVENT_ASYNC_BEGIN0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
    Send(new ViewMsg_Repaint(routing_id_, view_size));
  }

  // Pump a nested message loop until we time out or get a frame of the right
  // size.
  TimeTicks start_time = TimeTicks::Now();
  TimeDelta time_left = TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);
  TimeTicks timeout_time = start_time + time_left;
  while (1) {
    TRACE_EVENT0("renderer_host", "WaitForSurface::WaitForSingleTaskToRun");
    if (ui::WindowResizeHelperMac::Get()->WaitForSingleTaskToRun(time_left)) {
      // For auto-resized views, current_size_ determines the view_size and it
      // may have changed during the handling of an UpdateRect message.
      if (auto_resize_enabled_)
        view_size = current_size_;
      if (view_->HasAcceleratedSurface(view_size))
        break;
    }
    time_left = timeout_time - TimeTicks::Now();
    if (time_left <= TimeDelta::FromSeconds(0)) {
      TRACE_EVENT0("renderer_host", "WaitForSurface::Timeout");
      break;
    }
  }
}
#endif

bool RenderWidgetHostImpl::ScheduleComposite() {
  if (is_hidden_ || current_size_.IsEmpty() || repaint_ack_pending_ ||
      resize_ack_pending_) {
    return false;
  }

  // Send out a request to the renderer to paint the view if required.
  repaint_start_time_ = TimeTicks::Now();
  repaint_ack_pending_ = true;
  TRACE_EVENT_ASYNC_BEGIN0(
      "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
  Send(new ViewMsg_Repaint(routing_id_, current_size_));
  return true;
}

void RenderWidgetHostImpl::StartHangMonitorTimeout(
    base::TimeDelta delay,
    RenderWidgetHostDelegate::RendererUnresponsiveType hang_monitor_reason) {
  if (!hang_monitor_timeout_)
    return;
  hang_monitor_timeout_->Start(delay);
  hang_monitor_reason_ = hang_monitor_reason;
}

void RenderWidgetHostImpl::RestartHangMonitorTimeout() {
  if (hang_monitor_timeout_)
    hang_monitor_timeout_->Restart(hung_renderer_delay_);
}

void RenderWidgetHostImpl::DisableHangMonitorForTesting() {
  StopHangMonitorTimeout();
  hang_monitor_timeout_.reset();
}

void RenderWidgetHostImpl::StopHangMonitorTimeout() {
  if (hang_monitor_timeout_) {
    hang_monitor_timeout_->Stop();
    hang_monitor_reason_ =
        RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN;
  }
  RendererIsResponsive();
}

void RenderWidgetHostImpl::StartNewContentRenderingTimeout() {
  // It is possible for a compositor frame to arrive before the browser is
  // notified about the page being committed, in which case no timer is
  // necessary.
  if (received_paint_after_load_) {
    received_paint_after_load_ = false;
    return;
  }

  new_content_rendering_timeout_->Start(new_content_rendering_delay_);
}

void RenderWidgetHostImpl::OnFirstPaintAfterLoad() {
  if (new_content_rendering_timeout_->IsRunning()) {
    new_content_rendering_timeout_->Stop();
  } else {
    received_paint_after_load_ = true;
  }

  if (delegate_)
    delegate_->OnFirstPaintAfterLoad(this);
}

void RenderWidgetHostImpl::ForwardMouseEvent(const WebMouseEvent& mouse_event) {
  ForwardMouseEventWithLatencyInfo(mouse_event, ui::LatencyInfo());
  if (owner_delegate_)
    owner_delegate_->RenderWidgetDidForwardMouseEvent(mouse_event);
}

void RenderWidgetHostImpl::ForwardMouseEventWithLatencyInfo(
      const blink::WebMouseEvent& mouse_event,
      const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT2("input", "RenderWidgetHostImpl::ForwardMouseEvent",
               "x", mouse_event.x, "y", mouse_event.y);

  for (size_t i = 0; i < mouse_event_callbacks_.size(); ++i) {
    if (mouse_event_callbacks_[i].Run(mouse_event))
      return;
  }

  if (ShouldDropInputEvents())
    return;

  if (touch_emulator_ && touch_emulator_->HandleMouseEvent(mouse_event))
    return;

  MouseEventWithLatencyInfo mouse_with_latency(mouse_event, ui_latency);
  DispatchInputEventWithLatencyInfo(mouse_event, &mouse_with_latency.latency);
  input_router_->SendMouseEvent(mouse_with_latency);
}

void RenderWidgetHostImpl::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  ForwardWheelEventWithLatencyInfo(wheel_event, ui::LatencyInfo());
}

void RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT2("input", "RenderWidgetHostImpl::ForwardWheelEvent",
               "dx", wheel_event.deltaX, "dy", wheel_event.deltaY);

  if (ShouldDropInputEvents())
    return;

  if (touch_emulator_ && touch_emulator_->HandleMouseWheelEvent(wheel_event))
    return;

  MouseWheelEventWithLatencyInfo wheel_with_latency(wheel_event, ui_latency);
  DispatchInputEventWithLatencyInfo(wheel_event, &wheel_with_latency.latency);
  input_router_->SendWheelEvent(wheel_with_latency);
}

void RenderWidgetHostImpl::ForwardEmulatedGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  ForwardGestureEvent(gesture_event);
}

void RenderWidgetHostImpl::ForwardGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  ForwardGestureEventWithLatencyInfo(gesture_event, ui::LatencyInfo());
}

void RenderWidgetHostImpl::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardGestureEvent");
  // Early out if necessary, prior to performing latency logic.
  if (ShouldDropInputEvents())
    return;

  // TODO(wjmaclean) Remove the code for supporting resending gesture events
  // when WebView transitions to OOPIF and BrowserPlugin is removed.
  // http://crbug.com/533069
  bool* is_in_gesture_scroll =
      gesture_event.sourceDevice ==
              blink::WebGestureDevice::WebGestureDeviceTouchpad
          ? &is_in_touchpad_gesture_scroll_
          : &is_in_touchscreen_gesture_scroll_;
  if (gesture_event.type == blink::WebInputEvent::GestureScrollBegin) {
    DCHECK(!(*is_in_gesture_scroll));
    *is_in_gesture_scroll = true;
  } else if (gesture_event.type == blink::WebInputEvent::GestureScrollEnd ||
             gesture_event.type == blink::WebInputEvent::GestureFlingStart) {
    DCHECK(*is_in_gesture_scroll ||
           (gesture_event.type == blink::WebInputEvent::GestureFlingStart &&
            gesture_event.sourceDevice ==
                blink::WebGestureDevice::WebGestureDeviceTouchpad));
    *is_in_gesture_scroll = false;
  }

  bool scroll_update_needs_wrapping =
      gesture_event.type == blink::WebInputEvent::GestureScrollUpdate &&
      gesture_event.resendingPluginId != -1 && !(*is_in_gesture_scroll);

  if (scroll_update_needs_wrapping) {
    ForwardGestureEventWithLatencyInfo(
        CreateScrollBeginForWrapping(gesture_event), ui::LatencyInfo());
  }

  // Delegate must be non-null, due to |ShouldDropInputEvents()| test.
  if (delegate_->PreHandleGestureEvent(gesture_event))
    return;

  GestureEventWithLatencyInfo gesture_with_latency(gesture_event, ui_latency);
  DispatchInputEventWithLatencyInfo(gesture_event,
                                    &gesture_with_latency.latency);
  input_router_->SendGestureEvent(gesture_with_latency);

  if (scroll_update_needs_wrapping) {
    ForwardGestureEventWithLatencyInfo(
        CreateScrollEndForWrapping(gesture_event), ui::LatencyInfo());
  }
}

void RenderWidgetHostImpl::ForwardEmulatedTouchEvent(
      const blink::WebTouchEvent& touch_event) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardEmulatedTouchEvent");

  TouchEventWithLatencyInfo touch_with_latency(touch_event);
  DispatchInputEventWithLatencyInfo(touch_event, &touch_with_latency.latency);
  input_router_->SendTouchEvent(touch_with_latency);
}

void RenderWidgetHostImpl::ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardTouchEvent");

  // Always forward TouchEvents for touch stream consistency. They will be
  // ignored if appropriate in FilterInputEvent().

  TouchEventWithLatencyInfo touch_with_latency(touch_event, ui_latency);
  if (touch_emulator_ &&
      touch_emulator_->HandleTouchEvent(touch_with_latency.event)) {
    if (view_) {
      view_->ProcessAckedTouchEvent(
          touch_with_latency, INPUT_EVENT_ACK_STATE_CONSUMED);
    }
    return;
  }

  DispatchInputEventWithLatencyInfo(touch_event, &touch_with_latency.latency);
  input_router_->SendTouchEvent(touch_with_latency);
}

void RenderWidgetHostImpl::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardKeyboardEvent");
  if (owner_delegate_ &&
      !owner_delegate_->MayRenderWidgetForwardKeyboardEvent(key_event)) {
    return;
  }

  if (ShouldDropInputEvents())
    return;

  if (!process_->HasConnection())
    return;

  // First, let keypress listeners take a shot at handling the event.  If a
  // listener handles the event, it should not be propagated to the renderer.
  if (KeyPressListenersHandleEvent(key_event)) {
    // Some keypresses that are accepted by the listener might have follow up
    // char events, which should be ignored.
    if (key_event.type == WebKeyboardEvent::RawKeyDown)
      suppress_next_char_events_ = true;
    return;
  }

  // Double check the type to make sure caller hasn't sent us nonsense that
  // will mess up our key queue.
  if (!WebInputEvent::isKeyboardEventType(key_event.type))
    return;

  if (suppress_next_char_events_) {
    // If preceding RawKeyDown event was handled by the browser, then we need
    // suppress all Char events generated by it. Please note that, one
    // RawKeyDown event may generate multiple Char events, so we can't reset
    // |suppress_next_char_events_| until we get a KeyUp or a RawKeyDown.
    if (key_event.type == WebKeyboardEvent::Char)
      return;
    // We get a KeyUp or a RawKeyDown event.
    suppress_next_char_events_ = false;
  }

  bool is_shortcut = false;

  // Only pre-handle the key event if it's not handled by the input method.
  if (delegate_ && !key_event.skip_in_browser) {
    // We need to set |suppress_next_char_events_| to true if
    // PreHandleKeyboardEvent() returns true, but |this| may already be
    // destroyed at that time. So set |suppress_next_char_events_| true here,
    // then revert it afterwards when necessary.
    if (key_event.type == WebKeyboardEvent::RawKeyDown)
      suppress_next_char_events_ = true;

    // Tab switching/closing accelerators aren't sent to the renderer to avoid
    // a hung/malicious renderer from interfering.
    if (delegate_->PreHandleKeyboardEvent(key_event, &is_shortcut))
      return;

    if (key_event.type == WebKeyboardEvent::RawKeyDown)
      suppress_next_char_events_ = false;
  }

  if (touch_emulator_ && touch_emulator_->HandleKeyboardEvent(key_event))
    return;

  NativeWebKeyboardEventWithLatencyInfo key_event_with_latency(key_event);
  key_event_with_latency.event.isBrowserShortcut = is_shortcut;
  DispatchInputEventWithLatencyInfo(key_event, &key_event_with_latency.latency);
  input_router_->SendKeyboardEvent(key_event_with_latency);
}

void RenderWidgetHostImpl::QueueSyntheticGesture(
    std::unique_ptr<SyntheticGesture> synthetic_gesture,
    const base::Callback<void(SyntheticGesture::Result)>& on_complete) {
  if (!synthetic_gesture_controller_ && view_) {
    synthetic_gesture_controller_.reset(
        new SyntheticGestureController(view_->CreateSyntheticGestureTarget()));
  }
  if (synthetic_gesture_controller_) {
    synthetic_gesture_controller_->QueueSyntheticGesture(
        std::move(synthetic_gesture), on_complete);
  }
}

void RenderWidgetHostImpl::SetCursor(const WebCursor& cursor) {
  if (!view_)
    return;
  view_->UpdateCursor(cursor);
}

void RenderWidgetHostImpl::ShowContextMenuAtPoint(const gfx::Point& point) {
  Send(new ViewMsg_ShowContextMenu(
      GetRoutingID(), ui::MENU_SOURCE_MOUSE, point));
}

void RenderWidgetHostImpl::SendCursorVisibilityState(bool is_visible) {
  Send(new InputMsg_CursorVisibilityChange(GetRoutingID(), is_visible));
}

int64_t RenderWidgetHostImpl::GetLatencyComponentId() const {
  return latency_tracker_.latency_component_id();
}

// static
void RenderWidgetHostImpl::DisableResizeAckCheckForTesting() {
  g_check_for_pending_resize_ack = false;
}

void RenderWidgetHostImpl::AddKeyPressEventCallback(
    const KeyPressEventCallback& callback) {
  key_press_event_callbacks_.push_back(callback);
}

void RenderWidgetHostImpl::RemoveKeyPressEventCallback(
    const KeyPressEventCallback& callback) {
  for (size_t i = 0; i < key_press_event_callbacks_.size(); ++i) {
    if (key_press_event_callbacks_[i].Equals(callback)) {
      key_press_event_callbacks_.erase(
          key_press_event_callbacks_.begin() + i);
      return;
    }
  }
}

void RenderWidgetHostImpl::AddMouseEventCallback(
    const MouseEventCallback& callback) {
  mouse_event_callbacks_.push_back(callback);
}

void RenderWidgetHostImpl::RemoveMouseEventCallback(
    const MouseEventCallback& callback) {
  for (size_t i = 0; i < mouse_event_callbacks_.size(); ++i) {
    if (mouse_event_callbacks_[i].Equals(callback)) {
      mouse_event_callbacks_.erase(mouse_event_callbacks_.begin() + i);
      return;
    }
  }
}

void RenderWidgetHostImpl::AddInputEventObserver(
    RenderWidgetHost::InputEventObserver* observer) {
  if (!input_event_observers_.HasObserver(observer))
    input_event_observers_.AddObserver(observer);
}

void RenderWidgetHostImpl::RemoveInputEventObserver(
    RenderWidgetHost::InputEventObserver* observer) {
  input_event_observers_.RemoveObserver(observer);
}

void RenderWidgetHostImpl::GetWebScreenInfo(blink::WebScreenInfo* result) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::GetWebScreenInfo");
  if (view_)
    view_->GetScreenInfo(result);
  else
    RenderWidgetHostViewBase::GetDefaultScreenInfo(result);
  // TODO(sievers): find a way to make this done another way so the method
  // can be const.
  latency_tracker_.set_device_scale_factor(result->deviceScaleFactor);
  if (IsUseZoomForDSFEnabled())
    input_router_->SetDeviceScaleFactor(result->deviceScaleFactor);
}

void RenderWidgetHostImpl::HandleCompositorProto(
    const std::vector<uint8_t>& proto) {
  DCHECK(!proto.empty());
  Send(new ViewMsg_HandleCompositorProto(GetRoutingID(), proto));
}

void RenderWidgetHostImpl::NotifyScreenInfoChanged() {
  if (delegate_)
    delegate_->ScreenInfoChanged();

  // The resize message (which may not happen immediately) will carry with it
  // the screen info as well as the new size (if the screen has changed scale
  // factor).
  WasResized();
}

void RenderWidgetHostImpl::GetSnapshotFromBrowser(
    const GetSnapshotFromBrowserCallback& callback) {
  int id = next_browser_snapshot_id_++;

#if defined(OS_MACOSX)
  // MacOS version of underlying GrabViewSnapshot() blocks while
  // display/GPU are in a power-saving mode, so make sure display
  // does not go to sleep for the duration of reading a snapshot.
  if (pending_browser_snapshots_.empty()) {
    DCHECK(!power_save_blocker_);
    power_save_blocker_.reset(new device::PowerSaveBlocker(
        device::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
        device::PowerSaveBlocker::kReasonOther, "GetSnapshot",
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  }
#endif
  pending_browser_snapshots_.insert(std::make_pair(id, callback));
  Send(new ViewMsg_ForceRedraw(GetRoutingID(), id));
}

const NativeWebKeyboardEvent*
    RenderWidgetHostImpl::GetLastKeyboardEvent() const {
  return input_router_->GetLastKeyboardEvent();
}

void RenderWidgetHostImpl::OnSelectionChanged(const base::string16& text,
                                              uint32_t offset,
                                              const gfx::Range& range) {
  if (view_)
    view_->SelectionChanged(text, static_cast<size_t>(offset), range);
}

void RenderWidgetHostImpl::OnSelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (view_) {
    view_->SelectionBoundsChanged(params);
  }
}

void RenderWidgetHostImpl::OnForwardCompositorProto(
    const std::vector<uint8_t>& proto) {
  if (delegate_)
    delegate_->ForwardCompositorProto(this, proto);
}

void RenderWidgetHostImpl::UpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  Send(new ViewMsg_UpdateVSyncParameters(GetRoutingID(), timebase, interval));
}

void RenderWidgetHostImpl::RendererExited(base::TerminationStatus status,
                                          int exit_code) {
  if (!renderer_initialized_)
    return;

  // Clearing this flag causes us to re-create the renderer when recovering
  // from a crashed renderer.
  renderer_initialized_ = false;

  waiting_for_screen_rects_ack_ = false;

  // Must reset these to ensure that keyboard events work with a new renderer.
  suppress_next_char_events_ = false;

  // Reset some fields in preparation for recovering from a crash.
  ResetSizeAndRepaintPendingFlags();
  current_size_.SetSize(0, 0);
  // After the renderer crashes, the view is destroyed and so the
  // RenderWidgetHost cannot track its visibility anymore. We assume such
  // RenderWidgetHost to be invisible for the sake of internal accounting - be
  // careful about changing this - see http://crbug.com/401859 and
  // http://crbug.com/522795.
  //
  // We need to at least make sure that the RenderProcessHost is notified about
  // the |is_hidden_| change, so that the renderer will have correct visibility
  // set when respawned.
  if (!is_hidden_) {
    process_->WidgetHidden();
    is_hidden_ = true;
  }

  // Reset this to ensure the hung renderer mechanism is working properly.
  in_flight_event_count_ = 0;
  StopHangMonitorTimeout();

  if (view_) {
    view_->RenderProcessGone(status, exit_code);
    view_.reset();  // The View should be deleted by RenderProcessGone.
  }

  // Reconstruct the input router to ensure that it has fresh state for a new
  // renderer. Otherwise it may be stuck waiting for the old renderer to ack an
  // event. (In particular, the above call to view_->RenderProcessGone will
  // destroy the aura window, which may dispatch a synthetic mouse move.)
  input_router_.reset(new InputRouterImpl(
      process_, this, this, routing_id_, GetInputRouterConfigForPlatform()));

  synthetic_gesture_controller_.reset();
}

void RenderWidgetHostImpl::UpdateTextDirection(WebTextDirection direction) {
  text_direction_updated_ = true;
  text_direction_ = direction;
}

void RenderWidgetHostImpl::CancelUpdateTextDirection() {
  if (text_direction_updated_)
    text_direction_canceled_ = true;
}

void RenderWidgetHostImpl::NotifyTextDirection() {
  if (text_direction_updated_) {
    if (!text_direction_canceled_)
      Send(new ViewMsg_SetTextDirection(GetRoutingID(), text_direction_));
    text_direction_updated_ = false;
    text_direction_canceled_ = false;
  }
}

void RenderWidgetHostImpl::ImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  Send(new InputMsg_ImeSetComposition(
            GetRoutingID(), text, underlines, replacement_range,
            selection_start, selection_end));
}

void RenderWidgetHostImpl::ImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection) {
  Send(new InputMsg_ImeConfirmComposition(
        GetRoutingID(), text, replacement_range, keep_selection));
}

void RenderWidgetHostImpl::ImeCancelComposition() {
  Send(new InputMsg_ImeSetComposition(GetRoutingID(), base::string16(),
            std::vector<blink::WebCompositionUnderline>(),
            gfx::Range::InvalidRange(), 0, 0));
}

void RenderWidgetHostImpl::RejectMouseLockOrUnlockIfNecessary() {
  DCHECK(!pending_mouse_lock_request_ || !IsMouseLocked());
  if (pending_mouse_lock_request_) {
    pending_mouse_lock_request_ = false;
    Send(new ViewMsg_LockMouse_ACK(routing_id_, false));
  } else if (IsMouseLocked()) {
    view_->UnlockMouse();
  }
}

bool RenderWidgetHostImpl::IsMouseLocked() const {
  return view_ ? view_->IsMouseLocked() : false;
}

void RenderWidgetHostImpl::SetAutoResize(bool enable,
                                         const gfx::Size& min_size,
                                         const gfx::Size& max_size) {
  auto_resize_enabled_ = enable;
  min_size_for_auto_resize_ = min_size;
  max_size_for_auto_resize_ = max_size;
}

void RenderWidgetHostImpl::Destroy(bool also_delete) {
  DCHECK(!destroyed_);
  destroyed_ = true;

  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED, Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // Tell the view to die.
  // Note that in the process of the view shutting down, it can call a ton
  // of other messages on us.  So if you do any other deinitialization here,
  // do it after this call to view_->Destroy().
  if (view_) {
    view_->Destroy();
    view_.reset();
  }

  process_->RemoveRoute(routing_id_);
  g_routing_id_widget_map.Get().erase(
      RenderWidgetHostID(process_->GetID(), routing_id_));

  if (delegate_)
    delegate_->RenderWidgetDeleted(this);

  if (also_delete) {
    CHECK(!owner_delegate_);
    delete this;
  }
}

void RenderWidgetHostImpl::RendererIsUnresponsive() {
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_HANG,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());
  is_unresponsive_ = true;
  RenderWidgetHostDelegate::RendererUnresponsiveType reason =
      hang_monitor_reason_;
  hang_monitor_reason_ =
      RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_UNKNOWN;

  if (delegate_)
    delegate_->RendererUnresponsive(this, reason);

  // Do not add code after this since the Delegate may delete this
  // RenderWidgetHostImpl in RendererUnresponsive.
}

void RenderWidgetHostImpl::RendererIsResponsive() {
  if (is_unresponsive_) {
    is_unresponsive_ = false;
    if (delegate_)
      delegate_->RendererResponsive(this);
  }
}

void RenderWidgetHostImpl::ClearDisplayedGraphics() {
  NotifyNewContentRenderingTimeoutForTesting();
  if (view_)
    view_->ClearCompositorFrame();
}

void RenderWidgetHostImpl::OnRenderProcessGone(int status, int exit_code) {
  // RenderFrameHost owns a RenderWidgetHost when it needs one, in which case
  // it handles destruction.
  if (!owned_by_render_frame_host_) {
    // TODO(evanm): This synchronously ends up calling "delete this".
    // Is that really what we want in response to this message?  I'm matching
    // previous behavior of the code here.
    Destroy(true);
  } else {
    RendererExited(static_cast<base::TerminationStatus>(status), exit_code);
  }
}

void RenderWidgetHostImpl::OnHittestData(
    const FrameHostMsg_HittestData_Params& params) {
  if (delegate_)
    delegate_->GetInputEventRouter()->OnHittestData(params);
}

void RenderWidgetHostImpl::OnClose() {
  ShutdownAndDestroyWidget(true);
}

void RenderWidgetHostImpl::OnSetTooltipText(
    const base::string16& tooltip_text,
    WebTextDirection text_direction_hint) {
  // First, add directionality marks around tooltip text if necessary.
  // A naive solution would be to simply always wrap the text. However, on
  // windows, Unicode directional embedding characters can't be displayed on
  // systems that lack RTL fonts and are instead displayed as empty squares.
  //
  // To get around this we only wrap the string when we deem it necessary i.e.
  // when the locale direction is different than the tooltip direction hint.
  //
  // Currently, we use element's directionality as the tooltip direction hint.
  // An alternate solution would be to set the overall directionality based on
  // trying to detect the directionality from the tooltip text rather than the
  // element direction.  One could argue that would be a preferable solution
  // but we use the current approach to match Fx & IE's behavior.
  base::string16 wrapped_tooltip_text = tooltip_text;
  if (!tooltip_text.empty()) {
    if (text_direction_hint == blink::WebTextDirectionLeftToRight) {
      // Force the tooltip to have LTR directionality.
      wrapped_tooltip_text =
          base::i18n::GetDisplayStringInLTRDirectionality(wrapped_tooltip_text);
    } else if (text_direction_hint == blink::WebTextDirectionRightToLeft &&
               !base::i18n::IsRTL()) {
      // Force the tooltip to have RTL directionality.
      base::i18n::WrapStringWithRTLFormatting(&wrapped_tooltip_text);
    }
  }
  if (GetView())
    view_->SetTooltipText(wrapped_tooltip_text);
}

void RenderWidgetHostImpl::OnUpdateScreenRectsAck() {
  waiting_for_screen_rects_ack_ = false;
  if (!view_)
    return;

  if (view_->GetViewBounds() == last_view_screen_rect_ &&
      view_->GetBoundsInRootWindow() == last_window_screen_rect_) {
    return;
  }

  SendScreenRects();
}

void RenderWidgetHostImpl::OnRequestMove(const gfx::Rect& pos) {
  if (view_) {
    view_->SetBounds(pos);
    Send(new ViewMsg_Move_ACK(routing_id_));
  }
}

bool RenderWidgetHostImpl::OnSwapCompositorFrame(
    const IPC::Message& message) {
  // This trace event is used in
  // chrome/browser/extensions/api/cast_streaming/performance_test.cc
  TRACE_EVENT0("test_fps,benchmark", "OnSwapCompositorFrame");

  ViewHostMsg_SwapCompositorFrame::Param param;
  if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
    return false;
  cc::CompositorFrame frame(std::move(std::get<1>(param)));
  uint32_t output_surface_id = std::get<0>(param);
  std::vector<IPC::Message> messages_to_deliver_with_frame;
  messages_to_deliver_with_frame.swap(std::get<2>(param));

  if (!ui::LatencyInfo::Verify(frame.metadata.latency_info,
                               "RenderWidgetHostImpl::OnSwapCompositorFrame")) {
    std::vector<ui::LatencyInfo>().swap(frame.metadata.latency_info);
  }

  latency_tracker_.OnSwapCompositorFrame(&frame.metadata.latency_info);

  bool is_mobile_optimized = IsMobileOptimizedFrame(frame.metadata);
  input_router_->NotifySiteIsMobileOptimized(is_mobile_optimized);
  if (touch_emulator_)
    touch_emulator_->SetDoubleTapSupportForPageEnabled(!is_mobile_optimized);

  if (view_) {
    view_->OnSwapCompositorFrame(output_surface_id, std::move(frame));
    view_->DidReceiveRendererFrame();
  } else {
    cc::CompositorFrameAck ack;
    if (frame.gl_frame_data) {
      ack.gl_frame_data = std::move(frame.gl_frame_data);
    } else if (frame.delegated_frame_data) {
      cc::TransferableResource::ReturnResources(
          frame.delegated_frame_data->resource_list, &ack.resources);
    }
    SendSwapCompositorFrameAck(routing_id_, output_surface_id,
                               process_->GetID(), ack);
  }

  RenderProcessHost* rph = GetProcess();
  for (std::vector<IPC::Message>::const_iterator i =
           messages_to_deliver_with_frame.begin();
       i != messages_to_deliver_with_frame.end();
       ++i) {
    rph->OnMessageReceived(*i);
    if (i->dispatch_error())
      rph->OnBadMessageReceived(*i);
  }
  messages_to_deliver_with_frame.clear();

  return true;
}

void RenderWidgetHostImpl::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::OnUpdateRect");
  TimeTicks paint_start = TimeTicks::Now();

  // Update our knowledge of the RenderWidget's size.
  current_size_ = params.view_size;

  bool is_resize_ack =
      ViewHostMsg_UpdateRect_Flags::is_resize_ack(params.flags);

  // resize_ack_pending_ needs to be cleared before we call DidPaintRect, since
  // that will end up reaching GetBackingStore.
  if (is_resize_ack) {
    DCHECK(!g_check_for_pending_resize_ack || resize_ack_pending_);
    resize_ack_pending_ = false;
  }

  bool is_repaint_ack =
      ViewHostMsg_UpdateRect_Flags::is_repaint_ack(params.flags);
  if (is_repaint_ack) {
    DCHECK(repaint_ack_pending_);
    TRACE_EVENT_ASYNC_END0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
    repaint_ack_pending_ = false;
    TimeDelta delta = TimeTicks::Now() - repaint_start_time_;
    UMA_HISTOGRAM_TIMES("MPArch.RWH_RepaintDelta", delta);
  }

  DCHECK(!params.view_size.IsEmpty());

  DidUpdateBackingStore(params, paint_start);

  if (auto_resize_enabled_) {
    bool post_callback = new_auto_size_.IsEmpty();
    new_auto_size_ = params.view_size;
    if (post_callback) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&RenderWidgetHostImpl::DelayedAutoResized,
                                weak_factory_.GetWeakPtr()));
    }
  }

  // Log the time delta for processing a paint message. On platforms that don't
  // support asynchronous painting, this is equivalent to
  // MPArch.RWH_TotalPaintTime.
  TimeDelta delta = TimeTicks::Now() - paint_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_OnMsgUpdateRect", delta);
}

void RenderWidgetHostImpl::DidUpdateBackingStore(
    const ViewHostMsg_UpdateRect_Params& params,
    const TimeTicks& paint_start) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::DidUpdateBackingStore");
  TimeTicks update_start = TimeTicks::Now();

  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // We don't need to update the view if the view is hidden. We must do this
  // early return after the ACK is sent, however, or the renderer will not send
  // us more data.
  if (is_hidden_)
    return;

  // If we got a resize ack, then perhaps we have another resize to send?
  bool is_resize_ack =
      ViewHostMsg_UpdateRect_Flags::is_resize_ack(params.flags);
  if (is_resize_ack)
    WasResized();

  // Log the time delta for processing a paint message.
  TimeTicks now = TimeTicks::Now();
  TimeDelta delta = now - update_start;
  UMA_HISTOGRAM_TIMES("MPArch.RWH_DidUpdateBackingStore", delta);
}

void RenderWidgetHostImpl::OnQueueSyntheticGesture(
    const SyntheticGesturePacket& gesture_packet) {
  // Only allow untrustworthy gestures if explicitly enabled.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableGpuBenchmarking)) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RWH_SYNTHETIC_GESTURE);
    return;
  }

  QueueSyntheticGesture(
        SyntheticGesture::Create(*gesture_packet.gesture_params()),
        base::Bind(&RenderWidgetHostImpl::OnSyntheticGestureCompleted,
                   weak_factory_.GetWeakPtr()));
}

void RenderWidgetHostImpl::OnSetCursor(const WebCursor& cursor) {
  SetCursor(cursor);
}

void RenderWidgetHostImpl::SetTouchEventEmulationEnabled(
    bool enabled, ui::GestureProviderConfigType config_type) {
  if (enabled) {
    if (!touch_emulator_) {
      touch_emulator_.reset(new TouchEmulator(
          this,
          view_.get() ? content::GetScaleFactorForView(view_.get()) : 1.0f));
    }
    touch_emulator_->Enable(config_type);
  } else {
    if (touch_emulator_)
      touch_emulator_->Disable();
  }
}

void RenderWidgetHostImpl::OnTextInputStateChanged(
    const TextInputState& params) {
  if (view_)
    view_->TextInputStateChanged(params);
}

void RenderWidgetHostImpl::OnImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (view_)
    view_->ImeCompositionRangeChanged(range, character_bounds);
}

void RenderWidgetHostImpl::OnImeCancelComposition() {
  if (view_)
    view_->ImeCancelComposition();
}

void RenderWidgetHostImpl::OnLockMouse(bool user_gesture,
                                       bool last_unlocked_by_target,
                                       bool privileged) {
  if (pending_mouse_lock_request_) {
    Send(new ViewMsg_LockMouse_ACK(routing_id_, false));
    return;
  }

  pending_mouse_lock_request_ = true;
  if (delegate_) {
    delegate_->RequestToLockMouse(this, user_gesture, last_unlocked_by_target,
                                  privileged && allow_privileged_mouse_lock_);
    return;
  }

  if (privileged && allow_privileged_mouse_lock_) {
    // Directly approve to lock the mouse.
    GotResponseToLockMouseRequest(true);
  } else {
    // Otherwise, just reject it.
    GotResponseToLockMouseRequest(false);
  }
}

void RenderWidgetHostImpl::OnUnlockMouse() {
  RejectMouseLockOrUnlockIfNecessary();
}

void RenderWidgetHostImpl::OnShowDisambiguationPopup(
    const gfx::Rect& rect_pixels,
    const gfx::Size& size,
    const cc::SharedBitmapId& id) {
  DCHECK(!rect_pixels.IsEmpty());
  DCHECK(!size.IsEmpty());

  std::unique_ptr<cc::SharedBitmap> bitmap =
      HostSharedBitmapManager::current()->GetSharedBitmapFromId(size, id);
  if (!bitmap) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RWH_SHARED_BITMAP);
    return;
  }

  DCHECK(bitmap->pixels());

  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  SkBitmap zoomed_bitmap;
  zoomed_bitmap.installPixels(info, bitmap->pixels(), info.minRowBytes());

  // Note that |rect| is in coordinates of pixels relative to the window origin.
  // Aura-based systems will want to convert this to DIPs.
  if (view_)
    view_->ShowDisambiguationPopup(rect_pixels, zoomed_bitmap);

  // It is assumed that the disambiguation popup will make a copy of the
  // provided zoomed image, so we delete this one.
  zoomed_bitmap.setPixels(0);
  Send(new ViewMsg_ReleaseDisambiguationPopupBitmap(GetRoutingID(), id));
}

void RenderWidgetHostImpl::SetIgnoreInputEvents(bool ignore_input_events) {
  ignore_input_events_ = ignore_input_events;
}

bool RenderWidgetHostImpl::KeyPressListenersHandleEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type != WebKeyboardEvent::RawKeyDown)
    return false;

  for (size_t i = 0; i < key_press_event_callbacks_.size(); i++) {
    size_t original_size = key_press_event_callbacks_.size();
    if (key_press_event_callbacks_[i].Run(event))
      return true;

    // Check whether the callback that just ran removed itself, in which case
    // the iterator needs to be decremented to properly account for the removal.
    size_t current_size = key_press_event_callbacks_.size();
    if (current_size != original_size) {
      DCHECK_EQ(original_size - 1, current_size);
      --i;
    }
  }

  return false;
}

InputEventAckState RenderWidgetHostImpl::FilterInputEvent(
    const blink::WebInputEvent& event, const ui::LatencyInfo& latency_info) {
  // Don't ignore touch cancel events, since they may be sent while input
  // events are being ignored in order to keep the renderer from getting
  // confused about how many touches are active.
  if (ShouldDropInputEvents() && event.type != WebInputEvent::TouchCancel)
    return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;

  if (!process_->HasConnection())
    return INPUT_EVENT_ACK_STATE_UNKNOWN;

  if (delegate_ && (event.type == WebInputEvent::MouseDown ||
                    event.type == WebInputEvent::GestureScrollBegin ||
                    event.type == WebInputEvent::GestureTapDown ||
                    event.type == WebInputEvent::RawKeyDown)) {
    delegate_->OnUserInteraction(this, event.type);
  }

  return view_ ? view_->FilterInputEvent(event)
               : INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostImpl::IncrementInFlightEventCount() {
  increment_in_flight_event_count();
  if (!is_hidden_) {
    StartHangMonitorTimeout(
        hung_renderer_delay_,
        RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_IN_FLIGHT_EVENTS);
  }
}

void RenderWidgetHostImpl::DecrementInFlightEventCount() {
  if (decrement_in_flight_event_count() <= 0) {
    // Cancel pending hung renderer checks since the renderer is responsive.
    StopHangMonitorTimeout();
  } else {
    // The renderer is responsive, but there are in-flight events to wait for.
    if (!is_hidden_) {
      RestartHangMonitorTimeout();
      hang_monitor_reason_ =
          RenderWidgetHostDelegate::RENDERER_UNRESPONSIVE_IN_FLIGHT_EVENTS;
    }
  }
}

void RenderWidgetHostImpl::OnHasTouchEventHandlers(bool has_handlers) {
  has_touch_handler_ = has_handlers;
}

void RenderWidgetHostImpl::DidFlush() {
  if (synthetic_gesture_controller_)
    synthetic_gesture_controller_->OnDidFlushInput();
}

void RenderWidgetHostImpl::DidOverscroll(const DidOverscrollParams& params) {
  if (view_)
    view_->DidOverscroll(params);
}

void RenderWidgetHostImpl::DidStopFlinging() {
  if (view_)
    view_->DidStopFlinging();
}

void RenderWidgetHostImpl::DispatchInputEventWithLatencyInfo(
    const blink::WebInputEvent& event,
    ui::LatencyInfo* latency) {
  latency_tracker_.OnInputEvent(event, latency);
  FOR_EACH_OBSERVER(InputEventObserver, input_event_observers_,
                    OnInputEvent(event));
}

void RenderWidgetHostImpl::OnKeyboardEventAck(
      const NativeWebKeyboardEventWithLatencyInfo& event,
      InputEventAckState ack_result) {
  latency_tracker_.OnInputEventAck(event.event, &event.latency, ack_result);

  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);

  // We only send unprocessed key event upwards if we are not hidden,
  // because the user has moved away from us and no longer expect any effect
  // of this key event.
  if (delegate_ && !processed && !is_hidden() && !event.event.skip_in_browser) {
    delegate_->HandleKeyboardEvent(event.event);

    // WARNING: This RenderWidgetHostImpl can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this RenderWidgetHostImpl).
  }
}

void RenderWidgetHostImpl::OnMouseEventAck(
    const MouseEventWithLatencyInfo& mouse_event,
    InputEventAckState ack_result) {
  latency_tracker_.OnInputEventAck(mouse_event.event, &mouse_event.latency,
                                   ack_result);
}

void RenderWidgetHostImpl::OnWheelEventAck(
    const MouseWheelEventWithLatencyInfo& wheel_event,
    InputEventAckState ack_result) {
  latency_tracker_.OnInputEventAck(wheel_event.event, &wheel_event.latency,
                                   ack_result);

  if (!is_hidden() && view_) {
    if (ack_result != INPUT_EVENT_ACK_STATE_CONSUMED &&
        delegate_ && delegate_->HandleWheelEvent(wheel_event.event)) {
      ack_result = INPUT_EVENT_ACK_STATE_CONSUMED;
    }
    view_->WheelEventAck(wheel_event.event, ack_result);
  }
}

void RenderWidgetHostImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  latency_tracker_.OnInputEventAck(event.event, &event.latency, ack_result);

  if (view_)
    view_->GestureEventAck(event.event, ack_result);
}

void RenderWidgetHostImpl::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  latency_tracker_.OnInputEventAck(event.event, &event.latency, ack_result);

  if (touch_emulator_ &&
      touch_emulator_->HandleTouchEventAck(event.event, ack_result)) {
    return;
  }

  if (view_)
    view_->ProcessAckedTouchEvent(event, ack_result);
}

void RenderWidgetHostImpl::OnUnexpectedEventAck(UnexpectedEventAckType type) {
  if (type == BAD_ACK_MESSAGE) {
    bad_message::ReceivedBadMessage(process_, bad_message::RWH_BAD_ACK_MESSAGE);
  } else if (type == UNEXPECTED_EVENT_TYPE) {
    suppress_next_char_events_ = false;
  }
}

void RenderWidgetHostImpl::OnSyntheticGestureCompleted(
    SyntheticGesture::Result result) {
  Send(new InputMsg_SyntheticGestureCompleted(GetRoutingID()));
}

bool RenderWidgetHostImpl::ShouldDropInputEvents() const {
  return ignore_input_events_ || process_->IgnoreInputEvents() || !delegate_;
}

void RenderWidgetHostImpl::SetBackgroundOpaque(bool opaque) {
  Send(new ViewMsg_SetBackgroundOpaque(GetRoutingID(), opaque));
}

void RenderWidgetHostImpl::SetEditCommandsForNextKeyEvent(
    const std::vector<EditCommand>& commands) {
  Send(new InputMsg_SetEditCommandsForNextKeyEvent(GetRoutingID(), commands));
}

void RenderWidgetHostImpl::ExecuteEditCommand(const std::string& command,
                                              const std::string& value) {
  Send(new InputMsg_ExecuteEditCommand(GetRoutingID(), command, value));
}

void RenderWidgetHostImpl::ScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  Send(new InputMsg_ScrollFocusedEditableNodeIntoRect(GetRoutingID(), rect));
}

void RenderWidgetHostImpl::MoveCaret(const gfx::Point& point) {
  Send(new InputMsg_MoveCaret(GetRoutingID(), point));
}

bool RenderWidgetHostImpl::GotResponseToLockMouseRequest(bool allowed) {
  if (!allowed) {
    RejectMouseLockOrUnlockIfNecessary();
    return false;
  }

  if (!pending_mouse_lock_request_) {
    // This is possible, e.g., the plugin sends us an unlock request before
    // the user allows to lock to mouse.
    return false;
  }

  pending_mouse_lock_request_ = false;
  if (!view_ || !view_->HasFocus()|| !view_->LockMouse()) {
    Send(new ViewMsg_LockMouse_ACK(routing_id_, false));
    return false;
  }

  Send(new ViewMsg_LockMouse_ACK(routing_id_, true));
  return true;
}

// static
void RenderWidgetHostImpl::SendSwapCompositorFrameAck(
    int32_t route_id,
    uint32_t output_surface_id,
    int renderer_host_id,
    const cc::CompositorFrameAck& ack) {
  RenderProcessHost* host = RenderProcessHost::FromID(renderer_host_id);
  if (!host)
    return;
  host->Send(new ViewMsg_SwapCompositorFrameAck(
      route_id, output_surface_id, ack));
}

// static
void RenderWidgetHostImpl::SendReclaimCompositorResources(
    int32_t route_id,
    uint32_t output_surface_id,
    int renderer_host_id,
    const cc::CompositorFrameAck& ack) {
  RenderProcessHost* host = RenderProcessHost::FromID(renderer_host_id);
  if (!host)
    return;
  host->Send(
      new ViewMsg_ReclaimCompositorResources(route_id, output_surface_id, ack));
}

void RenderWidgetHostImpl::DelayedAutoResized() {
  gfx::Size new_size = new_auto_size_;
  // Clear the new_auto_size_ since the empty value is used as a flag to
  // indicate that no callback is in progress (i.e. without this line
  // DelayedAutoResized will not get called again).
  new_auto_size_.SetSize(0, 0);
  if (!auto_resize_enabled_)
    return;

  if (delegate_)
    delegate_->ResizeDueToAutoResize(this, new_size);
}

void RenderWidgetHostImpl::DetachDelegate() {
  delegate_ = NULL;
}

void RenderWidgetHostImpl::FrameSwapped(const ui::LatencyInfo& latency_info) {
  ui::LatencyInfo::LatencyComponent window_snapshot_component;
  if (latency_info.FindLatency(ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT,
                               GetLatencyComponentId(),
                               &window_snapshot_component)) {
    int sequence_number = static_cast<int>(
        window_snapshot_component.sequence_number);
#if defined(OS_MACOSX)
    // On Mac, when using CoreAnmation, there is a delay between when content
    // is drawn to the screen, and when the snapshot will actually pick up
    // that content. Insert a manual delay of 1/6th of a second (to simulate
    // 10 frames at 60 fps) before actually taking the snapshot.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RenderWidgetHostImpl::WindowSnapshotReachedScreen,
                   weak_factory_.GetWeakPtr(), sequence_number),
        base::TimeDelta::FromSecondsD(1. / 6));
#else
    WindowSnapshotReachedScreen(sequence_number);
#endif
  }

  latency_tracker_.OnFrameSwapped(latency_info);
}

void RenderWidgetHostImpl::DidReceiveRendererFrame() {
  view_->DidReceiveRendererFrame();
}

void RenderWidgetHostImpl::WindowSnapshotReachedScreen(int snapshot_id) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  gfx::Rect view_bounds = GetView()->GetViewBounds();
  gfx::Rect snapshot_bounds(view_bounds.size());

  std::vector<unsigned char> png;
  if (ui::GrabViewSnapshot(
      GetView()->GetNativeView(), &png, snapshot_bounds)) {
    OnSnapshotDataReceived(snapshot_id, &png.front(), png.size());
    return;
  }

  ui::GrabViewSnapshotAsync(
      GetView()->GetNativeView(),
      snapshot_bounds,
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&RenderWidgetHostImpl::OnSnapshotDataReceivedAsync,
                 weak_factory_.GetWeakPtr(),
                 snapshot_id));
}

void RenderWidgetHostImpl::OnSnapshotDataReceived(int snapshot_id,
                                                  const unsigned char* data,
                                                  size_t size) {
  // Any pending snapshots with a lower ID than the one received are considered
  // to be implicitly complete, and returned the same snapshot data.
  PendingSnapshotMap::iterator it = pending_browser_snapshots_.begin();
  while (it != pending_browser_snapshots_.end()) {
      if (it->first <= snapshot_id) {
        it->second.Run(data, size);
        pending_browser_snapshots_.erase(it++);
      } else {
        ++it;
      }
  }
#if defined(OS_MACOSX)
  if (pending_browser_snapshots_.empty())
    power_save_blocker_.reset();
#endif
}

void RenderWidgetHostImpl::OnSnapshotDataReceivedAsync(
    int snapshot_id,
    scoped_refptr<base::RefCountedBytes> png_data) {
  if (png_data.get())
    OnSnapshotDataReceived(snapshot_id, png_data->front(), png_data->size());
  else
    OnSnapshotDataReceived(snapshot_id, NULL, 0);
}

// static
void RenderWidgetHostImpl::CompositorFrameDrawn(
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (size_t i = 0; i < latency_info.size(); i++) {
    std::set<RenderWidgetHostImpl*> rwhi_set;
    for (const auto& lc : latency_info[i].latency_components()) {
      if (lc.first.first == ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT ||
          lc.first.first == ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT ||
          lc.first.first == ui::TAB_SHOW_COMPONENT) {
        // Matches with GetLatencyComponentId
        int routing_id = lc.first.second & 0xffffffff;
        int process_id = (lc.first.second >> 32) & 0xffffffff;
        RenderWidgetHost* rwh =
            RenderWidgetHost::FromID(process_id, routing_id);
        if (!rwh) {
          continue;
        }
        RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
        if (rwhi_set.insert(rwhi).second)
          rwhi->FrameSwapped(latency_info[i]);
      }
    }
  }
}

BrowserAccessibilityManager*
    RenderWidgetHostImpl::GetRootBrowserAccessibilityManager() {
  return delegate_ ? delegate_->GetRootBrowserAccessibilityManager() : NULL;
}

BrowserAccessibilityManager*
    RenderWidgetHostImpl::GetOrCreateRootBrowserAccessibilityManager() {
  return delegate_ ?
      delegate_->GetOrCreateRootBrowserAccessibilityManager() : NULL;
}

}  // namespace content

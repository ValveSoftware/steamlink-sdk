// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_impl.h"

#include <math.h>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/dip_util.h"
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
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/accessibility_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/vector2d_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "webkit/common/webpreferences.h"

#if defined(OS_WIN)
#include "content/common/plugin_constants_win.h"
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

// How long to (synchronously) wait for the renderer to respond with a
// PaintRect message, when our backing-store is invalid, before giving up and
// returning a null or incorrectly sized backing-store from GetBackingStore.
// This timeout impacts the "choppiness" of our window resize perf.
const int kPaintMsgTimeoutMS = 50;

typedef std::pair<int32, int32> RenderWidgetHostID;
typedef base::hash_map<RenderWidgetHostID, RenderWidgetHostImpl*>
    RoutingIDWidgetMap;
base::LazyInstance<RoutingIDWidgetMap> g_routing_id_widget_map =
    LAZY_INSTANCE_INITIALIZER;

int GetInputRouterViewFlagsFromCompositorFrameMetadata(
    const cc::CompositorFrameMetadata metadata) {
  int view_flags = InputRouter::VIEW_FLAGS_NONE;

  if (metadata.min_page_scale_factor == metadata.max_page_scale_factor)
    view_flags |= InputRouter::FIXED_PAGE_SCALE;

  const float window_width_dip =
      std::ceil(metadata.page_scale_factor * metadata.viewport_size.width());
  const float content_width_css = metadata.root_layer_size.width();
  if (content_width_css <= window_width_dip)
    view_flags |= InputRouter::MOBILE_VIEWPORT;

  return view_flags;
}

// Implements the RenderWidgetHostIterator interface. It keeps a list of
// RenderWidgetHosts, and makes sure it returns a live RenderWidgetHost at each
// iteration (or NULL if there isn't any left).
class RenderWidgetHostIteratorImpl : public RenderWidgetHostIterator {
 public:
  RenderWidgetHostIteratorImpl()
      : current_index_(0) {
  }

  virtual ~RenderWidgetHostIteratorImpl() {
  }

  void Add(RenderWidgetHost* host) {
    hosts_.push_back(RenderWidgetHostID(host->GetProcess()->GetID(),
                                        host->GetRoutingID()));
  }

  // RenderWidgetHostIterator:
  virtual RenderWidgetHost* GetNextHost() OVERRIDE {
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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostImpl

RenderWidgetHostImpl::RenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                                           RenderProcessHost* process,
                                           int routing_id,
                                           bool hidden)
    : view_(NULL),
      renderer_initialized_(false),
      hung_renderer_delay_ms_(kHungRendererDelayMs),
      delegate_(delegate),
      process_(process),
      routing_id_(routing_id),
      surface_id_(0),
      is_loading_(false),
      is_hidden_(hidden),
      is_fullscreen_(false),
      repaint_ack_pending_(false),
      resize_ack_pending_(false),
      screen_info_out_of_date_(false),
      overdraw_bottom_height_(0.f),
      should_auto_resize_(false),
      waiting_for_screen_rects_ack_(false),
      accessibility_mode_(AccessibilityModeOff),
      needs_repainting_on_restore_(false),
      is_unresponsive_(false),
      in_flight_event_count_(0),
      in_get_backing_store_(false),
      ignore_input_events_(false),
      input_method_active_(false),
      text_direction_updated_(false),
      text_direction_(blink::WebTextDirectionLeftToRight),
      text_direction_canceled_(false),
      suppress_next_char_events_(false),
      pending_mouse_lock_request_(false),
      allow_privileged_mouse_lock_(false),
      has_touch_handler_(false),
      weak_factory_(this),
      last_input_number_(static_cast<int64>(GetProcess()->GetID()) << 32) {
  CHECK(delegate_);
  if (routing_id_ == MSG_ROUTING_NONE) {
    routing_id_ = process_->GetNextRoutingID();
    surface_id_ = GpuSurfaceTracker::Get()->AddSurfaceForRenderer(
        process_->GetID(),
        routing_id_);
  } else {
    // TODO(piman): This is a O(N) lookup, where we could forward the
    // information from the RenderWidgetHelper. The problem is that doing so
    // currently leaks outside of content all the way to chrome classes, and
    // would be a layering violation. Since we don't expect more than a few
    // hundreds of RWH, this seems acceptable. Revisit if performance become a
    // problem, for example by tracking in the RenderWidgetHelper the routing id
    // (and surface id) that have been created, but whose RWH haven't yet.
    surface_id_ = GpuSurfaceTracker::Get()->LookupSurfaceForRenderer(
        process_->GetID(),
        routing_id_);
    DCHECK(surface_id_);
  }

  std::pair<RoutingIDWidgetMap::iterator, bool> result =
      g_routing_id_widget_map.Get().insert(std::make_pair(
          RenderWidgetHostID(process->GetID(), routing_id_), this));
  CHECK(result.second) << "Inserting a duplicate item!";
  process_->AddRoute(routing_id_, this);

  // If we're initially visible, tell the process host that we're alive.
  // Otherwise we'll notify the process host when we are first shown.
  if (!hidden)
    process_->WidgetRestored();

  accessibility_mode_ =
      BrowserAccessibilityStateImpl::GetInstance()->accessibility_mode();

  input_router_.reset(new InputRouterImpl(
      process_, this, this, routing_id_, GetInputRouterConfigForPlatform()));

  touch_emulator_.reset();

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      IsRenderView() ? RenderViewHost::From(this) : NULL);
  if (BrowserPluginGuest::IsGuest(rvh) ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    hang_monitor_timeout_.reset(new TimeoutMonitor(
        base::Bind(&RenderWidgetHostImpl::RendererIsUnresponsive,
                   weak_factory_.GetWeakPtr())));
  }
}

RenderWidgetHostImpl::~RenderWidgetHostImpl() {
  SetView(NULL);

  GpuSurfaceTracker::Get()->RemoveSurface(surface_id_);
  surface_id_ = 0;

  process_->RemoveRoute(routing_id_);
  g_routing_id_widget_map.Get().erase(
      RenderWidgetHostID(process_->GetID(), routing_id_));

  if (delegate_)
    delegate_->RenderWidgetDeleted(this);
}

// static
RenderWidgetHost* RenderWidgetHost::FromID(
    int32 process_id,
    int32 routing_id) {
  return RenderWidgetHostImpl::FromID(process_id, routing_id);
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::FromID(
    int32 process_id,
    int32 routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  RoutingIDWidgetMap::iterator it = widgets->find(
      RenderWidgetHostID(process_id, routing_id));
  return it == widgets->end() ? NULL : it->second;
}

// static
scoped_ptr<RenderWidgetHostIterator> RenderWidgetHost::GetRenderWidgetHosts() {
  RenderWidgetHostIteratorImpl* hosts = new RenderWidgetHostIteratorImpl();
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  for (RoutingIDWidgetMap::const_iterator it = widgets->begin();
       it != widgets->end();
       ++it) {
    RenderWidgetHost* widget = it->second;

    if (!widget->IsRenderView()) {
      hosts->Add(widget);
      continue;
    }

    // Add only active RenderViewHosts.
    RenderViewHost* rvh = RenderViewHost::From(widget);
    if (RenderViewHostImpl::IsRVHStateActive(
            static_cast<RenderViewHostImpl*>(rvh)->rvh_state()))
      hosts->Add(widget);
  }

  return scoped_ptr<RenderWidgetHostIterator>(hosts);
}

// static
scoped_ptr<RenderWidgetHostIterator>
RenderWidgetHostImpl::GetAllRenderWidgetHosts() {
  RenderWidgetHostIteratorImpl* hosts = new RenderWidgetHostIteratorImpl();
  RoutingIDWidgetMap* widgets = g_routing_id_widget_map.Pointer();
  for (RoutingIDWidgetMap::const_iterator it = widgets->begin();
       it != widgets->end();
       ++it) {
    hosts->Add(it->second);
  }

  return scoped_ptr<RenderWidgetHostIterator>(hosts);
}

// static
RenderWidgetHostImpl* RenderWidgetHostImpl::From(RenderWidgetHost* rwh) {
  return rwh->AsRenderWidgetHostImpl();
}

void RenderWidgetHostImpl::SetView(RenderWidgetHostViewBase* view) {
  view_ = view;

  GpuSurfaceTracker::Get()->SetSurfaceHandle(
      surface_id_, GetCompositingSurface());

  synthetic_gesture_controller_.reset();
}

RenderProcessHost* RenderWidgetHostImpl::GetProcess() const {
  return process_;
}

int RenderWidgetHostImpl::GetRoutingID() const {
  return routing_id_;
}

RenderWidgetHostView* RenderWidgetHostImpl::GetView() const {
  return view_;
}

RenderWidgetHostImpl* RenderWidgetHostImpl::AsRenderWidgetHostImpl() {
  return this;
}

gfx::NativeViewId RenderWidgetHostImpl::GetNativeViewId() const {
  if (view_)
    return view_->GetNativeViewId();
  return 0;
}

gfx::GLSurfaceHandle RenderWidgetHostImpl::GetCompositingSurface() {
  if (view_)
    return view_->GetCompositingSurface();
  return gfx::GLSurfaceHandle();
}

void RenderWidgetHostImpl::ResetSizeAndRepaintPendingFlags() {
  resize_ack_pending_ = false;
  if (repaint_ack_pending_) {
    TRACE_EVENT_ASYNC_END0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
  }
  repaint_ack_pending_ = false;
  last_requested_size_.SetSize(0, 0);
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
  if (delegate_)
    delegate_->DidSendScreenRects(this);
  waiting_for_screen_rects_ack_ = true;
}

void RenderWidgetHostImpl::SuppressNextCharEvents() {
  suppress_next_char_events_ = true;
}

void RenderWidgetHostImpl::FlushInput() {
  input_router_->Flush();
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

  GpuSurfaceTracker::Get()->SetSurfaceHandle(
      surface_id_, GetCompositingSurface());

  // Send the ack along with the information on placement.
  Send(new ViewMsg_CreatingNew_ACK(routing_id_));
  GetProcess()->ResumeRequestsForView(routing_id_);

  WasResized();
}

void RenderWidgetHostImpl::Shutdown() {
  RejectMouseLockOrUnlockIfNecessary();

  if (process_->HasConnection()) {
    // Tell the renderer object to close.
    bool rv = Send(new ViewMsg_Close(routing_id_));
    DCHECK(rv);
  }

  Destroy();
}

bool RenderWidgetHostImpl::IsLoading() const {
  return is_loading_;
}

bool RenderWidgetHostImpl::IsRenderView() const {
  return false;
}

bool RenderWidgetHostImpl::OnMessageReceived(const IPC::Message &msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostImpl, msg)
    IPC_MESSAGE_HANDLER(InputHostMsg_QueueSyntheticGesture,
                        OnQueueSyntheticGesture)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateScreenRects_ACK,
                        OnUpdateScreenRectsAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetTooltipText, OnSetTooltipText)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_SwapCompositorFrame,
                                OnSwapCompositorFrame(msg))
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStopFlinging, OnFlingingStopped)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Blur, OnBlur)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetTouchEventEmulationEnabled,
                        OnSetTouchEventEmulationEnabled)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputStateChanged,
                        OnTextInputStateChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeCancelComposition,
                        OnImeCancelComposition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse, OnLockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse, OnUnlockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowDisambiguationPopup,
                        OnShowDisambiguationPopup)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionChanged, OnSelectionChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionBoundsChanged,
                        OnSelectionBoundsChanged)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WindowlessPluginDummyWindowCreated,
                        OnWindowlessPluginDummyWindowCreated)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WindowlessPluginDummyWindowDestroyed,
                        OnWindowlessPluginDummyWindowDestroyed)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CompositorSurfaceBuffersSwapped,
                        OnCompositorSurfaceBuffersSwapped)
#endif
#if defined(OS_MACOSX) || defined(USE_AURA)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ImeCompositionRangeChanged,
                        OnImeCompositionRangeChanged)
#endif
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
    return input_router_->SendInput(make_scoped_ptr(msg));

  return process_->Send(msg);
}

void RenderWidgetHostImpl::WasHidden() {
  if (is_hidden_)
    return;

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

void RenderWidgetHostImpl::WasShown() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;

  SendScreenRects();

  // Always repaint on restore.
  bool needs_repainting = true;
  needs_repainting_on_restore_ = false;
  Send(new ViewMsg_WasShown(routing_id_, needs_repainting));

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

void RenderWidgetHostImpl::WasResized() {
  // Skip if the |delegate_| has already been detached because
  // it's web contents is being deleted.
  if (resize_ack_pending_ || !process_->HasConnection() || !view_ ||
      !renderer_initialized_ || should_auto_resize_ || !delegate_) {
    return;
  }

  gfx::Size new_size(view_->GetRequestedRendererSize());

  gfx::Size old_physical_backing_size = physical_backing_size_;
  physical_backing_size_ = view_->GetPhysicalBackingSize();
  bool was_fullscreen = is_fullscreen_;
  is_fullscreen_ = IsFullscreen();
  float old_overdraw_bottom_height = overdraw_bottom_height_;
  overdraw_bottom_height_ = view_->GetOverdrawBottomHeight();
  gfx::Size old_visible_viewport_size = visible_viewport_size_;
  visible_viewport_size_ = view_->GetVisibleViewportSize();

  bool size_changed = new_size != last_requested_size_;
  bool side_payload_changed =
      screen_info_out_of_date_ ||
      old_physical_backing_size != physical_backing_size_ ||
      was_fullscreen != is_fullscreen_ ||
      old_overdraw_bottom_height != overdraw_bottom_height_ ||
      old_visible_viewport_size != visible_viewport_size_;

  if (!size_changed && !side_payload_changed)
    return;

  if (!screen_info_) {
    screen_info_.reset(new blink::WebScreenInfo);
    GetWebScreenInfo(screen_info_.get());
  }

  // We don't expect to receive an ACK when the requested size or the physical
  // backing size is empty, or when the main viewport size didn't change.
  if (!new_size.IsEmpty() && !physical_backing_size_.IsEmpty() && size_changed)
    resize_ack_pending_ = g_check_for_pending_resize_ack;

  ViewMsg_Resize_Params params;
  params.screen_info = *screen_info_;
  params.new_size = new_size;
  params.physical_backing_size = physical_backing_size_;
  params.overdraw_bottom_height = overdraw_bottom_height_;
  params.visible_viewport_size = visible_viewport_size_;
  params.resizer_rect = GetRootWindowResizerRect();
  params.is_fullscreen = is_fullscreen_;
  if (!Send(new ViewMsg_Resize(routing_id_, params))) {
    resize_ack_pending_ = false;
  } else {
    last_requested_size_ = new_size;
  }
}

void RenderWidgetHostImpl::ResizeRectChanged(const gfx::Rect& new_rect) {
  Send(new ViewMsg_ChangeResizeRect(routing_id_, new_rect));
}

void RenderWidgetHostImpl::GotFocus() {
  Focus();
}

void RenderWidgetHostImpl::Focus() {
  Send(new InputMsg_SetFocus(routing_id_, true));
}

void RenderWidgetHostImpl::Blur() {
  // If there is a pending mouse lock request, we don't want to reject it at
  // this point. The user can switch focus back to this view and approve the
  // request later.
  if (IsMouseLocked())
    view_->UnlockMouse();

  if (touch_emulator_)
    touch_emulator_->CancelTouch();

  Send(new InputMsg_SetFocus(routing_id_, false));
}

void RenderWidgetHostImpl::LostCapture() {
  if (touch_emulator_)
    touch_emulator_->CancelTouch();

  Send(new InputMsg_MouseCaptureLost(routing_id_));
}

void RenderWidgetHostImpl::SetActive(bool active) {
  Send(new ViewMsg_SetActive(routing_id_, active));
}

void RenderWidgetHostImpl::LostMouseLock() {
  Send(new ViewMsg_MouseLockLost(routing_id_));
}

void RenderWidgetHostImpl::ViewDestroyed() {
  RejectMouseLockOrUnlockIfNecessary();

  // TODO(evanm): tracking this may no longer be necessary;
  // eliminate this function if so.
  SetView(NULL);
}

void RenderWidgetHostImpl::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  if (!view_)
    return;
  view_->SetIsLoading(is_loading);
}

void RenderWidgetHostImpl::CopyFromBackingStore(
    const gfx::Rect& src_subrect,
    const gfx::Size& accelerated_dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback,
    const SkBitmap::Config& bitmap_config) {
  if (view_) {
    TRACE_EVENT0("browser",
        "RenderWidgetHostImpl::CopyFromBackingStore::FromCompositingSurface");
    gfx::Rect accelerated_copy_rect = src_subrect.IsEmpty() ?
        gfx::Rect(view_->GetViewBounds().size()) : src_subrect;
    view_->CopyFromCompositingSurface(
        accelerated_copy_rect, accelerated_dst_size, callback, bitmap_config);
    return;
  }

  callback.Run(false, SkBitmap());
}

bool RenderWidgetHostImpl::CanCopyFromBackingStore() {
  if (view_)
    return view_->IsSurfaceAvailableForCopy();
  return false;
}

#if defined(OS_ANDROID)
void RenderWidgetHostImpl::LockBackingStore() {
  if (view_)
    view_->LockCompositingSurface();
}

void RenderWidgetHostImpl::UnlockBackingStore() {
  if (view_)
    view_->UnlockCompositingSurface();
}
#endif

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
  TRACE_EVENT0("browser", "RenderWidgetHostImpl::WaitForSurface");

  if (!view_)
    return;

  // The view_size will be current_size_ for auto-sized views and otherwise the
  // size of the view_. (For auto-sized views, current_size_ is updated during
  // UpdateRect messages.)
  gfx::Size view_size = current_size_;
  if (!should_auto_resize_) {
    // Get the desired size from the current view bounds.
    gfx::Rect view_rect = view_->GetViewBounds();
    if (view_rect.IsEmpty())
      return;
    view_size = view_rect.size();
  }

  TRACE_EVENT2("renderer_host",
               "RenderWidgetHostImpl::WaitForBackingStore",
               "width",
               base::IntToString(view_size.width()),
               "height",
               base::IntToString(view_size.height()));

  // We should not be asked to paint while we are hidden.  If we are hidden,
  // then it means that our consumer failed to call WasShown. If we're not
  // force creating the backing store, it's OK since we can feel free to give
  // out our cached one if we have it.
  DCHECK(!is_hidden_) << "WaitForSurface called while hidden!";

  // We should never be called recursively; this can theoretically lead to
  // infinite recursion and almost certainly leads to lower performance.
  DCHECK(!in_get_backing_store_) << "WaitForSurface called recursively!";
  base::AutoReset<bool> auto_reset_in_get_backing_store(
      &in_get_backing_store_, true);

  // We might have a surface that we can use!
  if (view_->HasAcceleratedSurface(view_size))
    return;

  // We do not have a suitable backing store in the cache, so send out a
  // request to the renderer to paint the view if required.
  if (!repaint_ack_pending_ && !resize_ack_pending_) {
    repaint_start_time_ = TimeTicks::Now();
    repaint_ack_pending_ = true;
    TRACE_EVENT_ASYNC_BEGIN0(
        "renderer_host", "RenderWidgetHostImpl::repaint_ack_pending_", this);
    Send(new ViewMsg_Repaint(routing_id_, view_size));
  }

  TimeDelta max_delay = TimeDelta::FromMilliseconds(kPaintMsgTimeoutMS);
  TimeTicks end_time = TimeTicks::Now() + max_delay;
  do {
    TRACE_EVENT0("renderer_host", "WaitForSurface::WaitForUpdate");

    // When we have asked the RenderWidget to resize, and we are still waiting
    // on a response, block for a little while to see if we can't get a response
    // before returning the old (incorrectly sized) backing store.
    IPC::Message msg;
    if (process_->WaitForBackingStoreMsg(routing_id_, max_delay, &msg)) {
      OnMessageReceived(msg);

      // For auto-resized views, current_size_ determines the view_size and it
      // may have changed during the handling of an UpdateRect message.
      if (should_auto_resize_)
        view_size = current_size_;

      // Break now if we got a backing store or accelerated surface of the
      // correct size.
      if (view_->HasAcceleratedSurface(view_size))
        return;
    } else {
      TRACE_EVENT0("renderer_host", "WaitForSurface::Timeout");
      break;
    }

    // Loop if we still have time left and haven't gotten a properly sized
    // BackingStore yet. This is necessary to support the GPU path which
    // typically has multiple frames pipelined -- we may need to skip one or two
    // BackingStore messages to get to the latest.
    max_delay = end_time - TimeTicks::Now();
  } while (max_delay > TimeDelta::FromSeconds(0));
}

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

void RenderWidgetHostImpl::StartHangMonitorTimeout(base::TimeDelta delay) {
  if (hang_monitor_timeout_)
    hang_monitor_timeout_->Start(delay);
}

void RenderWidgetHostImpl::RestartHangMonitorTimeout() {
  if (hang_monitor_timeout_)
    hang_monitor_timeout_->Restart(
        base::TimeDelta::FromMilliseconds(hung_renderer_delay_ms_));
}

void RenderWidgetHostImpl::StopHangMonitorTimeout() {
  if (hang_monitor_timeout_)
    hang_monitor_timeout_->Stop();
  RendererIsResponsive();
}

void RenderWidgetHostImpl::EnableFullAccessibilityMode() {
  AddAccessibilityMode(AccessibilityModeComplete);
}

bool RenderWidgetHostImpl::IsFullAccessibilityModeForTesting() {
  return accessibility_mode() == AccessibilityModeComplete;
}

void RenderWidgetHostImpl::EnableTreeOnlyAccessibilityMode() {
  AddAccessibilityMode(AccessibilityModeTreeOnly);
}

bool RenderWidgetHostImpl::IsTreeOnlyAccessibilityModeForTesting() {
  return accessibility_mode() == AccessibilityModeTreeOnly;
}

void RenderWidgetHostImpl::ForwardMouseEvent(const WebMouseEvent& mouse_event) {
  ForwardMouseEventWithLatencyInfo(mouse_event, ui::LatencyInfo());
}

void RenderWidgetHostImpl::ForwardMouseEventWithLatencyInfo(
      const blink::WebMouseEvent& mouse_event,
      const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT2("input", "RenderWidgetHostImpl::ForwardMouseEvent",
               "x", mouse_event.x, "y", mouse_event.y);

  ui::LatencyInfo latency_info =
      CreateRWHLatencyInfoIfNotExist(&ui_latency, mouse_event.type);

  for (size_t i = 0; i < mouse_event_callbacks_.size(); ++i) {
    if (mouse_event_callbacks_[i].Run(mouse_event))
      return;
  }

  if (IgnoreInputEvents())
    return;

  if (touch_emulator_ && touch_emulator_->HandleMouseEvent(mouse_event))
    return;

  input_router_->SendMouseEvent(MouseEventWithLatencyInfo(mouse_event,
                                                          latency_info));
}

void RenderWidgetHostImpl::OnPointerEventActivate() {
}

void RenderWidgetHostImpl::ForwardWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  ForwardWheelEventWithLatencyInfo(wheel_event, ui::LatencyInfo());
}

void RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardWheelEvent");

  ui::LatencyInfo latency_info =
      CreateRWHLatencyInfoIfNotExist(&ui_latency, wheel_event.type);

  if (IgnoreInputEvents())
    return;

  if (touch_emulator_ && touch_emulator_->HandleMouseWheelEvent(wheel_event))
    return;

  input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(wheel_event,
                                                               latency_info));
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
  if (IgnoreInputEvents())
    return;

  if (delegate_->PreHandleGestureEvent(gesture_event))
    return;

  ui::LatencyInfo latency_info =
      CreateRWHLatencyInfoIfNotExist(&ui_latency, gesture_event.type);

  if (gesture_event.type == blink::WebInputEvent::GestureScrollUpdate) {
    latency_info.AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_RWH_COMPONENT,
        GetLatencyComponentId(),
        ++last_input_number_);

    // Make a copy of the INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT with a
    // different name INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
    // So we can track the latency specifically for scroll update events.
    ui::LatencyInfo::LatencyComponent original_component;
    if (latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                 0,
                                 &original_component)) {
      latency_info.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          GetLatencyComponentId(),
          original_component.sequence_number,
          original_component.event_time,
          original_component.event_count);
    }
  }

  GestureEventWithLatencyInfo gesture_with_latency(gesture_event, latency_info);
  input_router_->SendGestureEvent(gesture_with_latency);
}

void RenderWidgetHostImpl::ForwardTouchEvent(
      const blink::WebTouchEvent& touch_event) {
  ForwardTouchEventWithLatencyInfo(touch_event, ui::LatencyInfo());
}

void RenderWidgetHostImpl::ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& ui_latency) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardTouchEvent");

  // Always forward TouchEvents for touch stream consistency. They will be
  // ignored if appropriate in FilterInputEvent().

  ui::LatencyInfo latency_info =
      CreateRWHLatencyInfoIfNotExist(&ui_latency, touch_event.type);
  TouchEventWithLatencyInfo touch_with_latency(touch_event, latency_info);
  input_router_->SendTouchEvent(touch_with_latency);
}

void RenderWidgetHostImpl::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  TRACE_EVENT0("input", "RenderWidgetHostImpl::ForwardKeyboardEvent");
  if (IgnoreInputEvents())
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

  if (key_event.type == WebKeyboardEvent::Char &&
      (key_event.windowsKeyCode == ui::VKEY_RETURN ||
       key_event.windowsKeyCode == ui::VKEY_SPACE)) {
    OnUserGesture();
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

  input_router_->SendKeyboardEvent(
      key_event,
      CreateRWHLatencyInfoIfNotExist(NULL, key_event.type),
      is_shortcut);
}

void RenderWidgetHostImpl::QueueSyntheticGesture(
    scoped_ptr<SyntheticGesture> synthetic_gesture,
    const base::Callback<void(SyntheticGesture::Result)>& on_complete) {
  if (!synthetic_gesture_controller_ && view_) {
    synthetic_gesture_controller_.reset(
        new SyntheticGestureController(
            view_->CreateSyntheticGestureTarget().Pass()));
  }
  if (synthetic_gesture_controller_) {
    synthetic_gesture_controller_->QueueSyntheticGesture(
        synthetic_gesture.Pass(), on_complete);
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

int64 RenderWidgetHostImpl::GetLatencyComponentId() {
  return GetRoutingID() | (static_cast<int64>(GetProcess()->GetID()) << 32);
}

// static
void RenderWidgetHostImpl::DisableResizeAckCheckForTesting() {
  g_check_for_pending_resize_ack = false;
}

ui::LatencyInfo RenderWidgetHostImpl::CreateRWHLatencyInfoIfNotExist(
    const ui::LatencyInfo* original, WebInputEvent::Type type) {
  ui::LatencyInfo info;
  if (original)
    info = *original;
  // In Aura, gesture event will already carry its original touch event's
  // INPUT_EVENT_LATENCY_RWH_COMPONENT.
  if (!info.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                        GetLatencyComponentId(),
                        NULL)) {
    info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                          GetLatencyComponentId(),
                          ++last_input_number_);
    info.TraceEventType(WebInputEventTraits::GetName(type));
  }
  return info;
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

void RenderWidgetHostImpl::GetWebScreenInfo(blink::WebScreenInfo* result) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::GetWebScreenInfo");
  if (view_)
    view_->GetScreenInfo(result);
  else
    RenderWidgetHostViewBase::GetDefaultScreenInfo(result);
  screen_info_out_of_date_ = false;
}

const NativeWebKeyboardEvent*
    RenderWidgetHostImpl::GetLastKeyboardEvent() const {
  return input_router_->GetLastKeyboardEvent();
}

void RenderWidgetHostImpl::NotifyScreenInfoChanged() {
  // The resize message (which may not happen immediately) will carry with it
  // the screen info as well as the new size (if the screen has changed scale
  // factor).
  InvalidateScreenInfo();
  WasResized();
}

void RenderWidgetHostImpl::InvalidateScreenInfo() {
  screen_info_out_of_date_ = true;
  screen_info_.reset();
}

void RenderWidgetHostImpl::OnSelectionChanged(const base::string16& text,
                                              size_t offset,
                                              const gfx::Range& range) {
  if (view_)
    view_->SelectionChanged(text, offset, range);
}

void RenderWidgetHostImpl::OnSelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (view_) {
    view_->SelectionBoundsChanged(params);
  }
}

void RenderWidgetHostImpl::UpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  Send(new ViewMsg_UpdateVSyncParameters(GetRoutingID(), timebase, interval));
}

void RenderWidgetHostImpl::RendererExited(base::TerminationStatus status,
                                          int exit_code) {
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
  // RenderWidgetHost to be visible for the sake of internal accounting - be
  // careful about changing this - see http://crbug.com/401859.
  //
  // We need to at least make sure that the RenderProcessHost is notified about
  // the |is_hidden_| change, so that the renderer will have correct visibility
  // set when respawned.
  if (!is_hidden_) {
    process_->WidgetRestored();
    is_hidden_ = false;
  }

  // Reset this to ensure the hung renderer mechanism is working properly.
  in_flight_event_count_ = 0;

  if (view_) {
    GpuSurfaceTracker::Get()->SetSurfaceHandle(surface_id_,
                                               gfx::GLSurfaceHandle());
    view_->RenderProcessGone(status, exit_code);
    view_ = NULL;  // The View should be deleted by RenderProcessGone.
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

void RenderWidgetHostImpl::SetInputMethodActive(bool activate) {
  input_method_active_ = activate;
  Send(new ViewMsg_SetInputMethodActive(GetRoutingID(), activate));
}

void RenderWidgetHostImpl::CandidateWindowShown() {
  Send(new ViewMsg_CandidateWindowShown(GetRoutingID()));
}

void RenderWidgetHostImpl::CandidateWindowUpdated() {
  Send(new ViewMsg_CandidateWindowUpdated(GetRoutingID()));
}

void RenderWidgetHostImpl::CandidateWindowHidden() {
  Send(new ViewMsg_CandidateWindowHidden(GetRoutingID()));
}

void RenderWidgetHostImpl::ImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  Send(new ViewMsg_ImeSetComposition(
            GetRoutingID(), text, underlines, selection_start, selection_end));
}

void RenderWidgetHostImpl::ImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection) {
  Send(new ViewMsg_ImeConfirmComposition(
        GetRoutingID(), text, replacement_range, keep_selection));
}

void RenderWidgetHostImpl::ImeCancelComposition() {
  Send(new ViewMsg_ImeSetComposition(GetRoutingID(), base::string16(),
            std::vector<blink::WebCompositionUnderline>(), 0, 0));
}

gfx::Rect RenderWidgetHostImpl::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

void RenderWidgetHostImpl::RequestToLockMouse(bool user_gesture,
                                              bool last_unlocked_by_target) {
  // Directly reject to lock the mouse. Subclass can override this method to
  // decide whether to allow mouse lock or not.
  GotResponseToLockMouseRequest(false);
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

bool RenderWidgetHostImpl::IsFullscreen() const {
  return false;
}

void RenderWidgetHostImpl::SetShouldAutoResize(bool enable) {
  should_auto_resize_ = enable;
}

void RenderWidgetHostImpl::Destroy() {
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());

  // Tell the view to die.
  // Note that in the process of the view shutting down, it can call a ton
  // of other messages on us.  So if you do any other deinitialization here,
  // do it after this call to view_->Destroy().
  if (view_)
    view_->Destroy();

  delete this;
}

void RenderWidgetHostImpl::RendererIsUnresponsive() {
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_HANG,
      Source<RenderWidgetHost>(this),
      NotificationService::NoDetails());
  is_unresponsive_ = true;
  NotifyRendererUnresponsive();
}

void RenderWidgetHostImpl::RendererIsResponsive() {
  if (is_unresponsive_) {
    is_unresponsive_ = false;
    NotifyRendererResponsive();
  }
}

void RenderWidgetHostImpl::OnRenderViewReady() {
  SendScreenRects();
  WasResized();
}

void RenderWidgetHostImpl::OnRenderProcessGone(int status, int exit_code) {
  // TODO(evanm): This synchronously ends up calling "delete this".
  // Is that really what we want in response to this message?  I'm matching
  // previous behavior of the code here.
  Destroy();
}

void RenderWidgetHostImpl::OnClose() {
  Shutdown();
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

#if defined(OS_MACOSX)
void RenderWidgetHostImpl::OnCompositorSurfaceBuffersSwapped(
      const ViewHostMsg_CompositorSurfaceBuffersSwapped_Params& params) {
  // This trace event is used in
  // chrome/browser/extensions/api/cast_streaming/performance_test.cc
  TRACE_EVENT0("renderer_host",
               "RenderWidgetHostImpl::OnCompositorSurfaceBuffersSwapped");
  // This trace event is used in
  // chrome/browser/extensions/api/cast_streaming/performance_test.cc
  UNSHIPPED_TRACE_EVENT0("test_fps",
                         TRACE_DISABLED_BY_DEFAULT("OnSwapCompositorFrame"));
  if (!ui::LatencyInfo::Verify(params.latency_info,
                               "ViewHostMsg_CompositorSurfaceBuffersSwapped"))
    return;
  if (!view_) {
    AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
    ack_params.sync_point = 0;
    RenderWidgetHostImpl::AcknowledgeBufferPresent(params.route_id,
                                                   params.gpu_process_host_id,
                                                   ack_params);
    return;
  }
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params gpu_params;
  gpu_params.surface_id = params.surface_id;
  gpu_params.surface_handle = params.surface_handle;
  gpu_params.route_id = params.route_id;
  gpu_params.size = params.size;
  gpu_params.scale_factor = params.scale_factor;
  gpu_params.latency_info = params.latency_info;
  for (size_t i = 0; i < gpu_params.latency_info.size(); i++)
    AddLatencyInfoComponentIds(&gpu_params.latency_info[i]);
  view_->AcceleratedSurfaceBuffersSwapped(gpu_params,
                                          params.gpu_process_host_id);
  view_->DidReceiveRendererFrame();
}
#endif  // OS_MACOSX

bool RenderWidgetHostImpl::OnSwapCompositorFrame(
    const IPC::Message& message) {
  // This trace event is used in
  // chrome/browser/extensions/api/cast_streaming/performance_test.cc
  UNSHIPPED_TRACE_EVENT0("test_fps",
                         TRACE_DISABLED_BY_DEFAULT("OnSwapCompositorFrame"));
  ViewHostMsg_SwapCompositorFrame::Param param;
  if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
    return false;
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  uint32 output_surface_id = param.a;
  param.b.AssignTo(frame.get());

  for (size_t i = 0; i < frame->metadata.latency_info.size(); i++)
    AddLatencyInfoComponentIds(&frame->metadata.latency_info[i]);

  input_router_->OnViewUpdated(
      GetInputRouterViewFlagsFromCompositorFrameMetadata(frame->metadata));

  if (view_) {
    view_->OnSwapCompositorFrame(output_surface_id, frame.Pass());
    view_->DidReceiveRendererFrame();
  } else {
    cc::CompositorFrameAck ack;
    if (frame->gl_frame_data) {
      ack.gl_frame_data = frame->gl_frame_data.Pass();
      ack.gl_frame_data->sync_point = 0;
    } else if (frame->delegated_frame_data) {
      cc::TransferableResource::ReturnResources(
          frame->delegated_frame_data->resource_list,
          &ack.resources);
    } else if (frame->software_frame_data) {
      ack.last_software_frame_id = frame->software_frame_data->id;
    }
    SendSwapCompositorFrameAck(routing_id_, output_surface_id,
                               process_->GetID(), ack);
  }
  return true;
}

void RenderWidgetHostImpl::OnFlingingStopped() {
  if (view_)
    view_->DidStopFlinging();
}

void RenderWidgetHostImpl::OnUpdateRect(
    const ViewHostMsg_UpdateRect_Params& params) {
  TRACE_EVENT0("renderer_host", "RenderWidgetHostImpl::OnUpdateRect");
  TimeTicks paint_start = TimeTicks::Now();

  // Update our knowledge of the RenderWidget's size.
  current_size_ = params.view_size;
  // Update our knowledge of the RenderWidget's scroll offset.
  last_scroll_offset_ = params.scroll_offset;

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

  if (should_auto_resize_) {
    bool post_callback = new_auto_size_.IsEmpty();
    new_auto_size_ = params.view_size;
    if (post_callback) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&RenderWidgetHostImpl::DelayedAutoResized,
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

  // Move the plugins if the view hasn't already been destroyed.  Plugin moves
  // will not be re-issued, so must move them now, regardless of whether we
  // paint or not.  MovePluginWindows attempts to move the plugin windows and
  // in the process could dispatch other window messages which could cause the
  // view to be destroyed.
  if (view_)
    view_->MovePluginWindows(params.plugin_window_moves);

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
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableGpuBenchmarking)) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_RWH7"));
    GetProcess()->ReceivedBadMessage();
    return;
  }

  QueueSyntheticGesture(
        SyntheticGesture::Create(*gesture_packet.gesture_params()),
        base::Bind(&RenderWidgetHostImpl::OnSyntheticGestureCompleted,
                   weak_factory_.GetWeakPtr()));
}

void RenderWidgetHostImpl::OnFocus() {
  // Only RenderViewHost can deal with that message.
  RecordAction(base::UserMetricsAction("BadMessageTerminate_RWH4"));
  GetProcess()->ReceivedBadMessage();
}

void RenderWidgetHostImpl::OnBlur() {
  // Only RenderViewHost can deal with that message.
  RecordAction(base::UserMetricsAction("BadMessageTerminate_RWH5"));
  GetProcess()->ReceivedBadMessage();
}

void RenderWidgetHostImpl::OnSetCursor(const WebCursor& cursor) {
  SetCursor(cursor);
}

void RenderWidgetHostImpl::OnSetTouchEventEmulationEnabled(
    bool enabled, bool allow_pinch) {
  if (delegate_)
    delegate_->OnTouchEmulationEnabled(enabled);

  if (enabled) {
    if (!touch_emulator_)
      touch_emulator_.reset(new TouchEmulator(this));
    touch_emulator_->Enable(allow_pinch);
  } else {
    if (touch_emulator_)
      touch_emulator_->Disable();
  }
}

void RenderWidgetHostImpl::OnTextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (view_)
    view_->TextInputStateChanged(params);
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void RenderWidgetHostImpl::OnImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (view_)
    view_->ImeCompositionRangeChanged(range, character_bounds);
}
#endif

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
  } else if (IsMouseLocked()) {
    Send(new ViewMsg_LockMouse_ACK(routing_id_, true));
    return;
  }

  pending_mouse_lock_request_ = true;
  if (privileged && allow_privileged_mouse_lock_) {
    // Directly approve to lock the mouse.
    GotResponseToLockMouseRequest(true);
  } else {
    RequestToLockMouse(user_gesture, last_unlocked_by_target);
  }
}

void RenderWidgetHostImpl::OnUnlockMouse() {
  RejectMouseLockOrUnlockIfNecessary();
}

void RenderWidgetHostImpl::OnShowDisambiguationPopup(
    const gfx::Rect& rect,
    const gfx::Size& size,
    const cc::SharedBitmapId& id) {
  DCHECK(!rect.IsEmpty());
  DCHECK(!size.IsEmpty());

  scoped_ptr<cc::SharedBitmap> bitmap =
      HostSharedBitmapManager::current()->GetSharedBitmapFromId(size, id);
  if (!bitmap) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_RWH6"));
    GetProcess()->ReceivedBadMessage();
    return;
  }

  DCHECK(bitmap->pixels());

  SkBitmap zoomed_bitmap;
  zoomed_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
      size.width(), size.height());
  zoomed_bitmap.setPixels(bitmap->pixels());

#if defined(OS_ANDROID)
  if (view_)
    view_->ShowDisambiguationPopup(rect, zoomed_bitmap);
#else
  NOTIMPLEMENTED();
#endif

  zoomed_bitmap.setPixels(0);
  Send(new ViewMsg_ReleaseDisambiguationPopupBitmap(GetRoutingID(), id));
}

#if defined(OS_WIN)
void RenderWidgetHostImpl::OnWindowlessPluginDummyWindowCreated(
    gfx::NativeViewId dummy_activation_window) {
  HWND hwnd = reinterpret_cast<HWND>(dummy_activation_window);

  // This may happen as a result of a race condition when the plugin is going
  // away.
  wchar_t window_title[MAX_PATH + 1] = {0};
  if (!IsWindow(hwnd) ||
      !GetWindowText(hwnd, window_title, arraysize(window_title)) ||
      lstrcmpiW(window_title, kDummyActivationWindowName) != 0) {
    return;
  }

#if defined(USE_AURA)
  SetParent(hwnd,
            reinterpret_cast<HWND>(view_->GetParentForWindowlessPlugin()));
#else
  SetParent(hwnd, reinterpret_cast<HWND>(GetNativeViewId()));
#endif
  dummy_windows_for_activation_.push_back(hwnd);
}

void RenderWidgetHostImpl::OnWindowlessPluginDummyWindowDestroyed(
    gfx::NativeViewId dummy_activation_window) {
  HWND hwnd = reinterpret_cast<HWND>(dummy_activation_window);
  std::list<HWND>::iterator i = dummy_windows_for_activation_.begin();
  for (; i != dummy_windows_for_activation_.end(); ++i) {
    if ((*i) == hwnd) {
      dummy_windows_for_activation_.erase(i);
      return;
    }
  }
  NOTREACHED() << "Unknown dummy window";
}
#endif

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
  if (IgnoreInputEvents() && event.type != WebInputEvent::TouchCancel)
    return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;

  if (!process_->HasConnection())
    return INPUT_EVENT_ACK_STATE_UNKNOWN;

  if (event.type == WebInputEvent::MouseDown)
    OnUserGesture();

  return view_ ? view_->FilterInputEvent(event)
               : INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostImpl::IncrementInFlightEventCount() {
  StartHangMonitorTimeout(
      TimeDelta::FromMilliseconds(hung_renderer_delay_ms_));
  increment_in_flight_event_count();
}

void RenderWidgetHostImpl::DecrementInFlightEventCount() {
  DCHECK_GE(in_flight_event_count_, 0);
  // Cancel pending hung renderer checks since the renderer is responsive.
  if (decrement_in_flight_event_count() <= 0)
    StopHangMonitorTimeout();
}

void RenderWidgetHostImpl::OnHasTouchEventHandlers(bool has_handlers) {
  has_touch_handler_ = has_handlers;
}

void RenderWidgetHostImpl::DidFlush() {
  if (synthetic_gesture_controller_)
    synthetic_gesture_controller_->OnDidFlushInput();
  if (view_)
    view_->OnDidFlushInput();
}

void RenderWidgetHostImpl::DidOverscroll(const DidOverscrollParams& params) {
  if (view_)
    view_->DidOverscroll(params);
}

void RenderWidgetHostImpl::OnKeyboardEventAck(
      const NativeWebKeyboardEvent& event,
      InputEventAckState ack_result) {
#if defined(OS_MACOSX)
  if (!is_hidden() && view_ && view_->PostProcessEventForPluginIme(event))
    return;
#endif

  // We only send unprocessed key event upwards if we are not hidden,
  // because the user has moved away from us and no longer expect any effect
  // of this key event.
  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  if (delegate_ && !processed && !is_hidden() && !event.skip_in_browser) {
    delegate_->HandleKeyboardEvent(event);

    // WARNING: This RenderWidgetHostImpl can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this RenderWidgetHostImpl).
  }
}

void RenderWidgetHostImpl::OnWheelEventAck(
    const MouseWheelEventWithLatencyInfo& wheel_event,
    InputEventAckState ack_result) {
  if (!wheel_event.latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, NULL)) {
    // MouseWheelEvent latency ends when it is acked but does not cause any
    // rendering scheduled.
    ui::LatencyInfo latency = wheel_event.latency;
    latency.AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT, 0, 0);
  }

  if (!is_hidden() && view_) {
    if (ack_result != INPUT_EVENT_ACK_STATE_CONSUMED &&
        delegate_->HandleWheelEvent(wheel_event.event)) {
      ack_result = INPUT_EVENT_ACK_STATE_CONSUMED;
    }
    view_->WheelEventAck(wheel_event.event, ack_result);
  }
}

void RenderWidgetHostImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  if (!event.latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, NULL)) {
    // GestureEvent latency ends when it is acked but does not cause any
    // rendering scheduled.
    ui::LatencyInfo latency = event.latency;
    latency.AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT, 0 ,0);
  }

  if (ack_result != INPUT_EVENT_ACK_STATE_CONSUMED) {
    if (delegate_->HandleGestureEvent(event.event))
      ack_result = INPUT_EVENT_ACK_STATE_CONSUMED;
  }

  if (view_)
    view_->GestureEventAck(event.event, ack_result);
}

void RenderWidgetHostImpl::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  TouchEventWithLatencyInfo touch_event = event;
  touch_event.latency.AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_ACKED_TOUCH_COMPONENT, 0, 0);
  // TouchEvent latency ends at ack if it didn't cause any rendering.
  if (!touch_event.latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT, 0, NULL)) {
    touch_event.latency.AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT, 0, 0);
  }
  ComputeTouchLatency(touch_event.latency);

  if (touch_emulator_ && touch_emulator_->HandleTouchEventAck(ack_result))
    return;

  if (view_)
    view_->ProcessAckedTouchEvent(touch_event, ack_result);
}

void RenderWidgetHostImpl::OnUnexpectedEventAck(UnexpectedEventAckType type) {
  if (type == BAD_ACK_MESSAGE) {
    RecordAction(base::UserMetricsAction("BadMessageTerminate_RWH2"));
    process_->ReceivedBadMessage();
  } else if (type == UNEXPECTED_EVENT_TYPE) {
    suppress_next_char_events_ = false;
  }
}

void RenderWidgetHostImpl::OnSyntheticGestureCompleted(
    SyntheticGesture::Result result) {
  Send(new InputMsg_SyntheticGestureCompleted(GetRoutingID()));
}

const gfx::Vector2d& RenderWidgetHostImpl::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

bool RenderWidgetHostImpl::IgnoreInputEvents() const {
  return ignore_input_events_ || process_->IgnoreInputEvents();
}

bool RenderWidgetHostImpl::ShouldForwardTouchEvent() const {
  return input_router_->ShouldForwardTouchEvent();
}

void RenderWidgetHostImpl::StartUserGesture() {
  OnUserGesture();
}

void RenderWidgetHostImpl::Stop() {
  Send(new ViewMsg_Stop(GetRoutingID()));
}

void RenderWidgetHostImpl::SetBackgroundOpaque(bool opaque) {
  Send(new ViewMsg_SetBackgroundOpaque(GetRoutingID(), opaque));
}

void RenderWidgetHostImpl::SetEditCommandsForNextKeyEvent(
    const std::vector<EditCommand>& commands) {
  Send(new InputMsg_SetEditCommandsForNextKeyEvent(GetRoutingID(), commands));
}

void RenderWidgetHostImpl::AddAccessibilityMode(AccessibilityMode mode) {
  SetAccessibilityMode(
      content::AddAccessibilityModeTo(accessibility_mode_, mode));
}

void RenderWidgetHostImpl::RemoveAccessibilityMode(AccessibilityMode mode) {
  SetAccessibilityMode(
      content::RemoveAccessibilityModeFrom(accessibility_mode_, mode));
}

void RenderWidgetHostImpl::ResetAccessibilityMode() {
  SetAccessibilityMode(
      BrowserAccessibilityStateImpl::GetInstance()->accessibility_mode());
}

void RenderWidgetHostImpl::SetAccessibilityMode(AccessibilityMode mode) {
  accessibility_mode_ = mode;
  Send(new ViewMsg_SetAccessibilityMode(GetRoutingID(), mode));
}

void RenderWidgetHostImpl::AccessibilitySetFocus(int object_id) {
  Send(new AccessibilityMsg_SetFocus(GetRoutingID(), object_id));
  view_->OnAccessibilitySetFocus(object_id);
}

void RenderWidgetHostImpl::AccessibilityDoDefaultAction(int object_id) {
  Send(new AccessibilityMsg_DoDefaultAction(GetRoutingID(), object_id));
}

void RenderWidgetHostImpl::AccessibilityShowMenu(int object_id) {
  view_->AccessibilityShowMenu(object_id);
}

void RenderWidgetHostImpl::AccessibilityScrollToMakeVisible(
    int acc_obj_id, gfx::Rect subfocus) {
  Send(new AccessibilityMsg_ScrollToMakeVisible(
      GetRoutingID(), acc_obj_id, subfocus));
}

void RenderWidgetHostImpl::AccessibilityScrollToPoint(
    int acc_obj_id, gfx::Point point) {
  Send(new AccessibilityMsg_ScrollToPoint(
      GetRoutingID(), acc_obj_id, point));
}

void RenderWidgetHostImpl::AccessibilitySetTextSelection(
    int object_id, int start_offset, int end_offset) {
  Send(new AccessibilityMsg_SetTextSelection(
      GetRoutingID(), object_id, start_offset, end_offset));
}

bool RenderWidgetHostImpl::AccessibilityViewHasFocus() const {
  return view_->HasFocus();
}

gfx::Rect RenderWidgetHostImpl::AccessibilityGetViewBounds() const {
  return view_->GetViewBounds();
}

gfx::Point RenderWidgetHostImpl::AccessibilityOriginInScreen(
    const gfx::Rect& bounds) const {
  return view_->AccessibilityOriginInScreen(bounds);
}

void RenderWidgetHostImpl::AccessibilityHitTest(const gfx::Point& point) {
  Send(new AccessibilityMsg_HitTest(GetRoutingID(), point));
}

void RenderWidgetHostImpl::AccessibilityFatalError() {
  Send(new AccessibilityMsg_FatalError(GetRoutingID()));
  view_->SetBrowserAccessibilityManager(NULL);
}

#if defined(OS_WIN)
void RenderWidgetHostImpl::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
  if (view_)
    view_->SetParentNativeViewAccessible(accessible_parent);
}

gfx::NativeViewAccessible
RenderWidgetHostImpl::GetParentNativeViewAccessible() const {
  return delegate_->GetParentNativeViewAccessible();
}
#endif

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
  } else {
    if (!pending_mouse_lock_request_) {
      // This is possible, e.g., the plugin sends us an unlock request before
      // the user allows to lock to mouse.
      return false;
    }

    pending_mouse_lock_request_ = false;
    if (!view_ || !view_->HasFocus()|| !view_->LockMouse()) {
      Send(new ViewMsg_LockMouse_ACK(routing_id_, false));
      return false;
    } else {
      Send(new ViewMsg_LockMouse_ACK(routing_id_, true));
      return true;
    }
  }
}

// static
void RenderWidgetHostImpl::AcknowledgeBufferPresent(
    int32 route_id, int gpu_host_id,
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(gpu_host_id);
  if (ui_shim) {
    ui_shim->Send(new AcceleratedSurfaceMsg_BufferPresented(route_id,
                                                            params));
  }
}

// static
void RenderWidgetHostImpl::SendSwapCompositorFrameAck(
    int32 route_id,
    uint32 output_surface_id,
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
    int32 route_id,
    uint32 output_surface_id,
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
  if (!should_auto_resize_)
    return;

  OnRenderAutoResized(new_size);
}

void RenderWidgetHostImpl::DetachDelegate() {
  delegate_ = NULL;
}

void RenderWidgetHostImpl::ComputeTouchLatency(
    const ui::LatencyInfo& latency_info) {
  ui::LatencyInfo::LatencyComponent ui_component;
  ui::LatencyInfo::LatencyComponent rwh_component;
  ui::LatencyInfo::LatencyComponent acked_component;

  if (!latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_UI_COMPONENT,
                                0,
                                &ui_component) ||
      !latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                GetLatencyComponentId(),
                                &rwh_component))
    return;

  DCHECK(ui_component.event_count == 1);
  DCHECK(rwh_component.event_count == 1);

  base::TimeDelta ui_delta =
      rwh_component.event_time - ui_component.event_time;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Event.Latency.Browser.TouchUI",
      ui_delta.InMicroseconds(),
      1,
      20000,
      100);

  if (latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_ACKED_TOUCH_COMPONENT,
                               0,
                               &acked_component)) {
    DCHECK(acked_component.event_count == 1);
    base::TimeDelta acked_delta =
        acked_component.event_time - rwh_component.event_time;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Latency.Browser.TouchAcked",
        acked_delta.InMicroseconds(),
        1,
        1000000,
        100);
  }
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
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RenderWidgetHostImpl::WindowSnapshotReachedScreen,
                   weak_factory_.GetWeakPtr(),
                   sequence_number),
        base::TimeDelta::FromSecondsD(1. / 6));
#else
    WindowSnapshotReachedScreen(sequence_number);
#endif
  }

  ui::LatencyInfo::LatencyComponent rwh_component;
  ui::LatencyInfo::LatencyComponent swap_component;
  if (!latency_info.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                GetLatencyComponentId(),
                                &rwh_component) ||
      !latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT,
          0, &swap_component)) {
    return;
  }

  ui::LatencyInfo::LatencyComponent original_component;
  if (latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          GetLatencyComponentId(),
          &original_component)) {
    // This UMA metric tracks the time from when the original touch event is
    // created (averaged if there are multiple) to when the scroll gesture
    // results in final frame swap.
    base::TimeDelta delta =
        swap_component.event_time - original_component.event_time;
    for (size_t i = 0; i < original_component.event_count; i++) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.TouchToScrollUpdateSwap",
          delta.InMicroseconds(),
          1,
          1000000,
          100);
    }
  }
}

void RenderWidgetHostImpl::DidReceiveRendererFrame() {
  view_->DidReceiveRendererFrame();
}

void RenderWidgetHostImpl::WindowSnapshotAsyncCallback(
    int routing_id,
    int snapshot_id,
    gfx::Size snapshot_size,
    scoped_refptr<base::RefCountedBytes> png_data) {
  if (!png_data) {
    std::vector<unsigned char> png_vector;
    Send(new ViewMsg_WindowSnapshotCompleted(
        routing_id, snapshot_id, gfx::Size(), png_vector));
    return;
  }

  Send(new ViewMsg_WindowSnapshotCompleted(
      routing_id, snapshot_id, snapshot_size, png_data->data()));
}

void RenderWidgetHostImpl::WindowSnapshotReachedScreen(int snapshot_id) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  std::vector<unsigned char> png;

  // This feature is behind the kEnableGpuBenchmarking command line switch
  // because it poses security concerns and should only be used for testing.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(cc::switches::kEnableGpuBenchmarking)) {
    Send(new ViewMsg_WindowSnapshotCompleted(
        GetRoutingID(), snapshot_id, gfx::Size(), png));
    return;
  }

  gfx::Rect view_bounds = GetView()->GetViewBounds();
  gfx::Rect snapshot_bounds(view_bounds.size());
  gfx::Size snapshot_size = snapshot_bounds.size();

  if (ui::GrabViewSnapshot(
          GetView()->GetNativeView(), &png, snapshot_bounds)) {
    Send(new ViewMsg_WindowSnapshotCompleted(
        GetRoutingID(), snapshot_id, snapshot_size, png));
    return;
  }

  ui::GrabViewSnapshotAsync(
      GetView()->GetNativeView(),
      snapshot_bounds,
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&RenderWidgetHostImpl::WindowSnapshotAsyncCallback,
                 weak_factory_.GetWeakPtr(),
                 GetRoutingID(),
                 snapshot_id,
                 snapshot_size));
}

// static
void RenderWidgetHostImpl::CompositorFrameDrawn(
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (size_t i = 0; i < latency_info.size(); i++) {
    std::set<RenderWidgetHostImpl*> rwhi_set;
    for (ui::LatencyInfo::LatencyMap::const_iterator b =
             latency_info[i].latency_components.begin();
         b != latency_info[i].latency_components.end();
         ++b) {
      if (b->first.first == ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT ||
          b->first.first == ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT) {
        // Matches with GetLatencyComponentId
        int routing_id = b->first.second & 0xffffffff;
        int process_id = (b->first.second >> 32) & 0xffffffff;
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

void RenderWidgetHostImpl::AddLatencyInfoComponentIds(
    ui::LatencyInfo* latency_info) {
  ui::LatencyInfo::LatencyMap new_components;
  ui::LatencyInfo::LatencyMap::iterator lc =
      latency_info->latency_components.begin();
  while (lc != latency_info->latency_components.end()) {
    ui::LatencyComponentType component_type = lc->first.first;
    if (component_type == ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT) {
      // Generate a new component entry with the correct component ID
      ui::LatencyInfo::LatencyMap::key_type key =
          std::make_pair(component_type, GetLatencyComponentId());
      new_components[key] = lc->second;

      // Remove the old entry
      latency_info->latency_components.erase(lc++);
    } else {
      ++lc;
    }
  }

  // Add newly generated components into the latency info
  for (lc = new_components.begin(); lc != new_components.end(); ++lc) {
    latency_info->latency_components[lc->first] = lc->second;
  }
}

SkBitmap::Config RenderWidgetHostImpl::PreferredReadbackFormat() {
  if (view_)
    return view_->PreferredReadbackFormat();
  return SkBitmap::kARGB_8888_Config;
}

}  // namespace content

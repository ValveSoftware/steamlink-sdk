// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "build/build_config.h"
#include "cc/output/output_surface.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/scheduler/renderer/render_widget_scheduling_state.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/common/content_switches_internal.h"
#include "content/common/input/synthetic_gesture_packet.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/gpu/queue_message_swap_promise.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_owner_delegate.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "ipc/ipc_sync_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPopupMenuInfo.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif  // defined(OS_POSIX)

#if defined(MOJO_SHELL_CLIENT)
#include "content/public/common/mojo_shell_connection.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#endif

using blink::WebCompositionUnderline;
using blink::WebCursorInfo;
using blink::WebDeviceEmulationParams;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebNavigationPolicy;
using blink::WebNode;
using blink::WebPagePopup;
using blink::WebPoint;
using blink::WebPopupType;
using blink::WebRange;
using blink::WebRect;
using blink::WebScreenInfo;
using blink::WebSize;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebVector;
using blink::WebWidget;

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

namespace {

typedef std::map<std::string, ui::TextInputMode> TextInputModeMap;

class WebWidgetLockTarget : public content::MouseLockDispatcher::LockTarget {
 public:
  explicit WebWidgetLockTarget(blink::WebWidget* webwidget)
      : webwidget_(webwidget) {}

  void OnLockMouseACK(bool succeeded) override {
    if (succeeded)
      webwidget_->didAcquirePointerLock();
    else
      webwidget_->didNotAcquirePointerLock();
  }

  void OnMouseLockLost() override { webwidget_->didLosePointerLock(); }

  bool HandleMouseLockedInputEvent(const blink::WebMouseEvent& event) override {
    // The WebWidget handles mouse lock in Blink's handleInputEvent().
    return false;
  }

 private:
  blink::WebWidget* webwidget_;
};

class TextInputModeMapSingleton {
 public:
  static TextInputModeMapSingleton* GetInstance() {
    return base::Singleton<TextInputModeMapSingleton>::get();
  }
  TextInputModeMapSingleton() {
    map_["verbatim"] = ui::TEXT_INPUT_MODE_VERBATIM;
    map_["latin"] = ui::TEXT_INPUT_MODE_LATIN;
    map_["latin-name"] = ui::TEXT_INPUT_MODE_LATIN_NAME;
    map_["latin-prose"] = ui::TEXT_INPUT_MODE_LATIN_PROSE;
    map_["full-width-latin"] = ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN;
    map_["kana"] = ui::TEXT_INPUT_MODE_KANA;
    map_["katakana"] = ui::TEXT_INPUT_MODE_KATAKANA;
    map_["numeric"] = ui::TEXT_INPUT_MODE_NUMERIC;
    map_["tel"] = ui::TEXT_INPUT_MODE_TEL;
    map_["email"] = ui::TEXT_INPUT_MODE_EMAIL;
    map_["url"] = ui::TEXT_INPUT_MODE_URL;
  }
  const TextInputModeMap& map() const { return map_; }
 private:
  TextInputModeMap map_;

  friend struct base::DefaultSingletonTraits<TextInputModeMapSingleton>;

  DISALLOW_COPY_AND_ASSIGN(TextInputModeMapSingleton);
};

ui::TextInputMode ConvertInputMode(const blink::WebString& input_mode) {
  static TextInputModeMapSingleton* singleton =
      TextInputModeMapSingleton::GetInstance();
  TextInputModeMap::const_iterator it =
      singleton->map().find(input_mode.utf8());
  if (it == singleton->map().end())
    return ui::TEXT_INPUT_MODE_DEFAULT;
  return it->second;
}

bool IsDateTimeInput(ui::TextInputType type) {
  return type == ui::TEXT_INPUT_TYPE_DATE ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL ||
         type == ui::TEXT_INPUT_TYPE_MONTH ||
         type == ui::TEXT_INPUT_TYPE_TIME || type == ui::TEXT_INPUT_TYPE_WEEK;
}

content::RenderWidgetInputHandlerDelegate* GetRenderWidgetInputHandlerDelegate(
    content::RenderWidget* widget) {
#if defined(MOJO_SHELL_CLIENT)
  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  if (content::MojoShellConnection::GetForProcess() &&
      cmdline.HasSwitch(switches::kUseMusInRenderer)) {
    return content::RenderWidgetMusConnection::GetOrCreate(
        widget->routing_id());
  }
#endif
  // If we don't have a connection to the Mojo shell, then we want to route IPCs
  // back to the browser process rather than Mus so we use the |widget| as the
  // RenderWidgetInputHandlerDelegate.
  return widget;
}

}  // namespace

namespace content {

// RenderWidget ---------------------------------------------------------------

RenderWidget::RenderWidget(CompositorDependencies* compositor_deps,
                           blink::WebPopupType popup_type,
                           const blink::WebScreenInfo& screen_info,
                           bool swapped_out,
                           bool hidden,
                           bool never_visible)
    : routing_id_(MSG_ROUTING_NONE),
      compositor_deps_(compositor_deps),
      webwidget_(nullptr),
      owner_delegate_(nullptr),
      opener_id_(MSG_ROUTING_NONE),
      next_paint_flags_(0),
      auto_resize_mode_(false),
      need_update_rect_for_auto_resize_(false),
      did_show_(false),
      is_hidden_(hidden),
      compositor_never_visible_(never_visible),
      is_fullscreen_granted_(false),
      display_mode_(blink::WebDisplayModeUndefined),
      ime_event_guard_(nullptr),
      closing_(false),
      host_closing_(false),
      is_swapped_out_(swapped_out),
      for_oopif_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      text_input_flags_(0),
      can_compose_inline_(true),
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      screen_info_(screen_info),
      device_scale_factor_(screen_info_.deviceScaleFactor),
#if defined(OS_ANDROID)
      text_field_is_dirty_(false),
#endif
      popup_origin_scale_for_emulation_(0.f),
      frame_swap_message_queue_(new FrameSwapMessageQueue()),
      resizing_mode_selector_(new ResizingModeSelector()),
      has_host_context_menu_location_(false),
      has_focus_(false) {
  if (!swapped_out)
    RenderProcess::current()->AddRefProcess();
  DCHECK(RenderThread::Get());
  device_color_profile_.push_back('0');
#if defined(OS_ANDROID)
  text_input_info_history_.push_back(blink::WebTextInputInfo());
#endif

  // In tests there may not be a RenderThreadImpl.
  if (RenderThreadImpl::current()) {
    render_widget_scheduling_state_ = RenderThreadImpl::current()
                                          ->GetRendererScheduler()
                                          ->NewRenderWidgetSchedulingState();
    render_widget_scheduling_state_->SetHidden(is_hidden_);
  }
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";

  // If we are swapped out, we have released already.
  if (!is_swapped_out_ && RenderProcess::current())
    RenderProcess::current()->ReleaseProcess();
}

// static
RenderWidget* RenderWidget::Create(int32_t opener_id,
                                   CompositorDependencies* compositor_deps,
                                   blink::WebPopupType popup_type,
                                   const blink::WebScreenInfo& screen_info) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget(new RenderWidget(
      compositor_deps, popup_type, screen_info, false, false, false));
  if (widget->Init(opener_id)) {  // adds reference on success.
    return widget.get();
  }
  return NULL;
}

// static
RenderWidget* RenderWidget::CreateForFrame(
    int routing_id,
    bool hidden,
    const blink::WebScreenInfo& screen_info,
    CompositorDependencies* compositor_deps,
    blink::WebLocalFrame* frame) {
  CHECK_NE(routing_id, MSG_ROUTING_NONE);
  // TODO(avi): Before RenderViewImpl has-a RenderWidget, the browser passes the
  // same routing ID for both the view routing ID and the main frame widget
  // routing ID. https://crbug.com/545684
  RenderViewImpl* view = RenderViewImpl::FromRoutingID(routing_id);
  if (view) {
    view->AttachWebFrameWidget(
        RenderWidget::CreateWebFrameWidget(view->GetWidget(), frame));
    return view->GetWidget();
  }
  scoped_refptr<RenderWidget> widget(
      new RenderWidget(compositor_deps, blink::WebPopupTypeNone, screen_info,
                       false, hidden, false));
  widget->SetRoutingID(routing_id);
  widget->for_oopif_ = true;
  // DoInit increments the reference count on |widget|, keeping it alive after
  // this function returns.
  if (widget->DoInit(MSG_ROUTING_NONE,
                     RenderWidget::CreateWebFrameWidget(widget.get(), frame),
                     nullptr)) {
    return widget.get();
  }
  return nullptr;
}

// static
blink::WebFrameWidget* RenderWidget::CreateWebFrameWidget(
    RenderWidget* render_widget,
    blink::WebLocalFrame* frame) {
  if (!frame->parent()) {
    // TODO(dcheng): The main frame widget currently has a special case.
    // Eliminate this once WebView is no longer a WebWidget.
    return blink::WebFrameWidget::create(render_widget, frame->view(), frame);
  }
  return blink::WebFrameWidget::create(render_widget, frame);
}

// static
blink::WebWidget* RenderWidget::CreateWebWidget(RenderWidget* render_widget) {
  switch (render_widget->popup_type_) {
    case blink::WebPopupTypeNone:  // Nothing to create.
      break;
    case blink::WebPopupTypePage:
      return WebPagePopup::create(render_widget);
    default:
      NOTREACHED();
  }
  return NULL;
}

void RenderWidget::CloseForFrame() {
  OnClose();
}

void RenderWidget::SetRoutingID(int32_t routing_id) {
  routing_id_ = routing_id;
  input_handler_.reset(new RenderWidgetInputHandler(
      GetRenderWidgetInputHandlerDelegate(this), this));
}

bool RenderWidget::Init(int32_t opener_id) {
  bool success = DoInit(
      opener_id, RenderWidget::CreateWebWidget(this),
      new ViewHostMsg_CreateWidget(opener_id, popup_type_, &routing_id_));
  if (success) {
    SetRoutingID(routing_id_);
    return true;
  }
  return false;
}

bool RenderWidget::DoInit(int32_t opener_id,
                          WebWidget* web_widget,
                          IPC::SyncMessage* create_widget_message) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = web_widget;
  webwidget_mouse_lock_target_.reset(new WebWidgetLockTarget(webwidget_));
  mouse_lock_dispatcher_.reset(new RenderWidgetMouseLockDispatcher(this));

  bool result = true;
  if (create_widget_message)
    result = RenderThread::Get()->Send(create_widget_message);

  if (result) {
    RenderThread::Get()->AddRoute(routing_id_, this);
    // Take a reference on behalf of the RenderThread.  This will be balanced
    // when we receive ViewMsg_Close.
    AddRef();
    if (RenderThreadImpl::current()) {
      RenderThreadImpl::current()->WidgetCreated();
      if (is_hidden_)
        RenderThreadImpl::current()->WidgetHidden();
    }

    return true;
  } else {
    // The above Send can fail when the tab is closing.
    return false;
  }
}

void RenderWidget::SetSwappedOut(bool is_swapped_out) {
  // We should only toggle between states.
  DCHECK(is_swapped_out_ != is_swapped_out);
  is_swapped_out_ = is_swapped_out;

  // If we are swapping out, we will call ReleaseProcess, allowing the process
  // to exit if all of its RenderViews are swapped out.  We wait until the
  // WasSwappedOut call to do this, to allow the unload handler to finish.
  // If we are swapping in, we call AddRefProcess to prevent the process from
  // exiting.
  if (!is_swapped_out_)
    RenderProcess::current()->AddRefProcess();
}

void RenderWidget::WasSwappedOut() {
  // If we have been swapped out and no one else is using this process,
  // it's safe to exit now.
  CHECK(is_swapped_out_);
  RenderProcess::current()->ReleaseProcess();
}

void RenderWidget::SetPopupOriginAdjustmentsForEmulation(
    RenderWidgetScreenMetricsEmulator* emulator) {
  popup_origin_scale_for_emulation_ = emulator->scale();
  popup_view_origin_for_emulation_ = emulator->applied_widget_rect().origin();
  popup_screen_origin_for_emulation_ = gfx::Point(
      emulator->original_screen_rect().origin().x() + emulator->offset().x(),
      emulator->original_screen_rect().origin().y() + emulator->offset().y());
  screen_info_ = emulator->original_screen_info();
  device_scale_factor_ = screen_info_.deviceScaleFactor;
}

gfx::Rect RenderWidget::AdjustValidationMessageAnchor(const gfx::Rect& anchor) {
  if (screen_metrics_emulator_)
    return screen_metrics_emulator_->AdjustValidationMessageAnchor(anchor);
  return anchor;
}

#if defined(USE_EXTERNAL_POPUP_MENU)
void RenderWidget::SetExternalPopupOriginAdjustmentsForEmulation(
    ExternalPopupMenu* popup,
    RenderWidgetScreenMetricsEmulator* emulator) {
  popup->SetOriginScaleAndOffsetForEmulation(
      emulator->scale(), emulator->offset());
}
#endif

void RenderWidget::OnShowHostContextMenu(ContextMenuParams* params) {
  if (screen_metrics_emulator_)
    screen_metrics_emulator_->OnShowContextMenu(params);
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
  if (mouse_lock_dispatcher_ &&
      mouse_lock_dispatcher_->OnMessageReceived(message))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(InputMsg_CursorVisibilityChange,
                        OnCursorVisibilityChange)
    IPC_MESSAGE_HANDLER(InputMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(InputMsg_ImeConfirmComposition, OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(InputMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(InputMsg_SyntheticGestureCompleted,
                        OnSyntheticGestureCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableDeviceEmulation,
                        OnEnableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableDeviceEmulation,
                        OnDisableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_ChangeResizeRect, OnChangeResizeRect)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScreenRects, OnUpdateScreenRects)
    IPC_MESSAGE_HANDLER(ViewMsg_SetSurfaceIdNamespace, OnSetSurfaceIdNamespace)
    IPC_MESSAGE_HANDLER(ViewMsg_WaitForNextFrameForTests,
                        OnWaitNextFrameForTests)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(InputMsg_ImeEventAck, OnImeEventAck)
    IPC_MESSAGE_HANDLER(InputMsg_RequestTextInputStateUpdate,
                        OnRequestTextInputStateUpdate)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowImeIfNeeded, OnShowImeIfNeeded)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_HandleCompositorProto, OnHandleCompositorProto)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidget::Send(IPC::Message* message) {
  // Don't send any messages after the browser has told us to close, and filter
  // most outgoing messages while swapped out.
  if ((is_swapped_out_ &&
       !SwappedOutMessages::CanSendWhileSwappedOut(message)) ||
      closing_) {
    delete message;
    return false;
  }

  // If given a messsage without a routing ID, then assign our routing ID.
  if (message->routing_id() == MSG_ROUTING_NONE)
    message->set_routing_id(routing_id_);

  return RenderThread::Get()->Send(message);
}

void RenderWidget::SetWindowRectSynchronously(
    const gfx::Rect& new_window_rect) {
  ResizeParams params;
  params.screen_info = screen_info_;
  params.new_size = new_window_rect.size();
  params.physical_backing_size =
      gfx::ScaleToCeiledSize(new_window_rect.size(), device_scale_factor_);
  params.visible_viewport_size = new_window_rect.size();
  params.resizer_rect = gfx::Rect();
  params.is_fullscreen_granted = is_fullscreen_granted_;
  params.display_mode = display_mode_;
  params.needs_resize_ack = false;
  Resize(params);

  view_screen_rect_ = new_window_rect;
  window_screen_rect_ = new_window_rect;
  if (!did_show_)
    initial_rect_ = new_window_rect;
}

void RenderWidget::OnClose() {
  DCHECK(content::RenderThread::Get());
  if (closing_)
    return;
  NotifyOnClose();
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    RenderThread::Get()->RemoveRoute(routing_id_);
    SetHidden(false);
    if (RenderThreadImpl::current())
      RenderThreadImpl::current()->WidgetDestroyed();
  }

  if (for_oopif_) {
    // Widgets for frames may be created and closed at any time while the frame
    // is alive. However, the closing process must happen synchronously. Frame
    // widget and frames hold pointers to each other. If Close() is deferred to
    // the message loop like in the non-frame widget case, WebWidget::close()
    // can end up accessing members of an already-deleted frame.
    Close();
  } else {
    // If there is a Send call on the stack, then it could be dangerous to close
    // now.  Post a task that only gets invoked when there are no nested message
    // loops.
    base::ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
        FROM_HERE, base::Bind(&RenderWidget::Close, this));
  }

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

void RenderWidget::OnResize(const ResizeParams& params) {
  if (resizing_mode_selector_->ShouldAbortOnResize(this, params))
    return;

  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnResize(params);
    return;
  }

  Resize(params);
}

void RenderWidget::OnEnableDeviceEmulation(
   const blink::WebDeviceEmulationParams& params) {
  if (!screen_metrics_emulator_) {
    ResizeParams resize_params;
    resize_params.screen_info = screen_info_;
    resize_params.new_size = size_;
    resize_params.physical_backing_size = physical_backing_size_;
    resize_params.visible_viewport_size = visible_viewport_size_;
    resize_params.resizer_rect = resizer_rect_;
    resize_params.is_fullscreen_granted = is_fullscreen_granted_;
    resize_params.display_mode = display_mode_;
    screen_metrics_emulator_.reset(new RenderWidgetScreenMetricsEmulator(
        this, params, resize_params, view_screen_rect_, window_screen_rect_));
    screen_metrics_emulator_->Apply();
  } else {
    screen_metrics_emulator_->ChangeEmulationParams(params);
  }
}

void RenderWidget::OnDisableDeviceEmulation() {
  screen_metrics_emulator_.reset();
}

void RenderWidget::OnChangeResizeRect(const gfx::Rect& resizer_rect) {
  if (resizer_rect_ == resizer_rect)
    return;
  resizer_rect_ = resizer_rect;
  if (webwidget_)
    webwidget_->didChangeWindowResizerRect();
}

void RenderWidget::OnWasHidden() {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    WasHidden());
}

void RenderWidget::OnWasShown(bool needs_repainting,
                              const ui::LatencyInfo& latency_info) {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasShown");
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // See OnWasHidden
  SetHidden(false);
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    WasShown());

  if (!needs_repainting)
    return;

  // Generate a full repaint.
  if (compositor_) {
    ui::LatencyInfo swap_latency_info(latency_info);
    std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor(
        compositor_->CreateLatencyInfoSwapPromiseMonitor(&swap_latency_info));
    compositor_->SetNeedsForcedRedraw();
  }
  ScheduleComposite();
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
  if (!pending_window_rect_count_)
    view_screen_rect_ = pending_window_rect_;
}

GURL RenderWidget::GetURLForGraphicsContext3D() {
  return GURL();
}

void RenderWidget::OnHandleInputEvent(const blink::WebInputEvent* input_event,
                                      const ui::LatencyInfo& latency_info,
                                      InputEventDispatchType dispatch_type) {
  if (!input_event)
    return;
  input_handler_->HandleInputEvent(*input_event, latency_info, dispatch_type);
}

void RenderWidget::OnCursorVisibilityChange(bool is_visible) {
  if (webwidget_)
    webwidget_->setCursorVisibilityState(is_visible);
}

void RenderWidget::OnMouseCaptureLost() {
  if (webwidget_)
    webwidget_->mouseCaptureLost();
}

void RenderWidget::OnSetFocus(bool enable) {
  has_focus_ = enable;

  if (webwidget_)
    webwidget_->setFocus(enable);

  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    RenderWidgetSetFocus(enable));
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetCompositorDelegate

void RenderWidget::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  webwidget_->applyViewportDeltas(inner_delta, outer_delta,
                                  elastic_overscroll_delta, page_scale,
                                  top_controls_delta);
}

void RenderWidget::BeginMainFrame(double frame_time_sec) {
  webwidget_->beginFrame(frame_time_sec);
}

std::unique_ptr<cc::OutputSurface> RenderWidget::CreateOutputSurface(
    bool fallback) {
  DCHECK(webwidget_);
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::OutputSurface.
  DCHECK(!compositor_never_visible_);
  return RenderThreadImpl::current()->CreateCompositorOutputSurface(
      fallback, routing_id_, frame_swap_message_queue_,
      GetURLForGraphicsContext3D());
}

std::unique_ptr<cc::BeginFrameSource>
RenderWidget::CreateExternalBeginFrameSource() {
  return compositor_deps_->CreateExternalBeginFrameSource(routing_id_);
}

void RenderWidget::DidCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "RenderWidget::DidCommitAndDrawCompositorFrame");

  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    DidCommitAndDrawCompositorFrame());

  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();
}

void RenderWidget::DidCommitCompositorFrame() {
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    DidCommitCompositorFrame());
  FOR_EACH_OBSERVER(RenderFrameProxy, render_frame_proxies_,
                    DidCommitCompositorFrame());
#if defined(VIDEO_HOLE)
  FOR_EACH_OBSERVER(RenderFrameImpl, video_hole_frames_,
                    DidCommitCompositorFrame());
#endif  // defined(VIDEO_HOLE)
  input_handler_->FlushPendingInputEventAck();
}

void RenderWidget::DidCompletePageScaleAnimation() {}

void RenderWidget::DidCompleteSwapBuffers() {
  TRACE_EVENT0("renderer", "RenderWidget::DidCompleteSwapBuffers");

  // Notify subclasses threaded composited rendering was flushed to the screen.
  DidFlushPaint();

  if (!next_paint_flags_ && !need_update_rect_for_auto_resize_) {
    return;
  }

  ViewHostMsg_UpdateRect_Params params;
  params.view_size = size_;
  params.flags = next_paint_flags_;

  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;
}

void RenderWidget::ForwardCompositorProto(const std::vector<uint8_t>& proto) {
  Send(new ViewHostMsg_ForwardCompositorProto(routing_id_, proto));
}

bool RenderWidget::IsClosing() const {
  return host_closing_;
}

void RenderWidget::OnSwapBuffersAborted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersAborted");
  // Schedule another frame so the compositor learns about it.
  ScheduleComposite();
}

void RenderWidget::OnSwapBuffersComplete() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersComplete");

  // Notify subclasses that composited rendering was flushed to the screen.
  DidFlushPaint();
}

void RenderWidget::OnSwapBuffersPosted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersPosted");
}

void RenderWidget::RequestScheduleAnimation() {
  scheduleAnimation();
}

void RenderWidget::UpdateVisualState() {
  webwidget_->updateAllLifecyclePhases();
}

void RenderWidget::WillBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  // The UpdateTextInputState can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_NON_IME);
  UpdateSelectionBounds();

  FOR_EACH_OBSERVER(RenderFrameProxy, render_frame_proxies_,
                    WillBeginCompositorFrame());
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetInputHandlerDelegate

void RenderWidget::FocusChangeComplete() {
  if (owner_delegate_)
    owner_delegate_->RenderWidgetFocusChangeComplete();
}

bool RenderWidget::HasTouchEventHandlersAt(const gfx::Point& point) const {
  if (owner_delegate_)
    return owner_delegate_->DoesRenderWidgetHaveTouchEventHandlersAt(point);

  return true;
}

void RenderWidget::ObserveGestureEventAndResult(
    const blink::WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    bool event_processed) {
  if (!compositor_deps_->IsElasticOverscrollEnabled())
    return;

  cc::InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = event_processed;
  scroll_result.did_overscroll_root = !unused_delta.IsZero();
  scroll_result.unused_scroll_delta = unused_delta;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager) {
    input_handler_manager->ObserveGestureEventAndResultOnMainThread(
        routing_id_, gesture_event, scroll_result);
  }
}

void RenderWidget::OnDidHandleKeyEvent() {
  if (owner_delegate_)
    owner_delegate_->RenderWidgetDidHandleKeyEvent();
}

void RenderWidget::OnDidOverscroll(const DidOverscrollParams& params) {
  Send(new InputHostMsg_DidOverscroll(routing_id_, params));
}

void RenderWidget::OnInputEventAck(
    std::unique_ptr<InputEventAck> input_event_ack) {
  Send(new InputHostMsg_HandleInputEvent_ACK(routing_id_, *input_event_ack));
}

void RenderWidget::NotifyInputEventHandled(
    blink::WebInputEvent::Type handled_type,
    InputEventAckState ack_result) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager) {
    input_handler_manager->NotifyInputEventHandledOnMainThread(
        routing_id_, handled_type, ack_result);
  }
}

void RenderWidget::SetInputHandler(RenderWidgetInputHandler* input_handler) {
  // Nothing to do here. RenderWidget created the |input_handler| and will take
  // ownership of it. We just verify here that we don't already have an input
  // handler.
  DCHECK(!input_handler_);
}

void RenderWidget::UpdateTextInputState(ShowIme show_ime,
                                        ChangeSource change_source) {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateTextInputState");
  if (ime_event_guard_) {
    // show_ime should still be effective even if it was set inside the IME
    // event guard.
    if (show_ime == ShowIme::IF_NEEDED) {
      ime_event_guard_->set_show_ime(true);
    }
    return;
  }

  ui::TextInputType new_type = GetTextInputType();
  if (IsDateTimeInput(new_type))
    return;  // Not considered as a text input field in WebKit/Chromium.

  blink::WebTextInputInfo new_info;
  if (webwidget_)
    new_info = webwidget_->textInputInfo();
  const ui::TextInputMode new_mode = ConvertInputMode(new_info.inputMode);

  bool new_can_compose_inline = CanComposeInline();

  // Only sends text input params if they are changed or if the ime should be
  // shown.
  if (show_ime == ShowIme::IF_NEEDED ||
      (IsUsingImeThread() && change_source == ChangeSource::FROM_IME) ||
      (text_input_type_ != new_type || text_input_mode_ != new_mode ||
       text_input_info_ != new_info ||
       can_compose_inline_ != new_can_compose_inline)
#if defined(OS_ANDROID)
      || text_field_is_dirty_
#endif
      ) {
    TextInputState params;
    params.type = new_type;
    params.mode = new_mode;
    params.flags = new_info.flags;
    params.value = new_info.value.utf8();
    params.selection_start = new_info.selectionStart;
    params.selection_end = new_info.selectionEnd;
    params.composition_start = new_info.compositionStart;
    params.composition_end = new_info.compositionEnd;
    params.can_compose_inline = new_can_compose_inline;
    params.show_ime_if_needed = (show_ime == ShowIme::IF_NEEDED);
#if defined(USE_AURA)
    params.is_non_ime_change = true;
#endif
#if defined(OS_ANDROID)
    params.is_non_ime_change =
        (change_source == ChangeSource::FROM_NON_IME) || text_field_is_dirty_;
    if (params.is_non_ime_change)
      OnImeEventSentForAck(new_info);
    text_field_is_dirty_ = false;
#endif
    Send(new ViewHostMsg_TextInputStateChanged(routing_id(), params));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    text_input_mode_ = new_mode;
    can_compose_inline_ = new_can_compose_inline;
    text_input_flags_ = new_info.flags;
  }
}

bool RenderWidget::WillHandleGestureEvent(const blink::WebGestureEvent& event) {
  if (owner_delegate_)
    return owner_delegate_->RenderWidgetWillHandleGestureEvent(event);

  return false;
}

bool RenderWidget::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    RenderWidgetWillHandleMouseEvent());

  if (owner_delegate_)
    return owner_delegate_->RenderWidgetWillHandleMouseEvent(event);

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetScreenMetricsDelegate

void RenderWidget::Redraw() {
  set_next_paint_is_resize_ack();
  if (compositor_)
    compositor_->SetNeedsRedrawRect(gfx::Rect(size_));
}

void RenderWidget::ResizeWebWidget() {
  webwidget_->resize(GetSizeForWebWidget());
}

gfx::Size RenderWidget::GetSizeForWebWidget() const {
  if (IsUseZoomForDSFEnabled())
    return gfx::ScaleToCeiledSize(size_, GetOriginalDeviceScaleFactor());

  return size_;
}

void RenderWidget::Resize(const ResizeParams& params) {
  bool orientation_changed =
      screen_info_.orientationAngle != params.screen_info.orientationAngle ||
      screen_info_.orientationType != params.screen_info.orientationType;

  screen_info_ = params.screen_info;
  SetDeviceScaleFactor(screen_info_.deviceScaleFactor);

  if (resizing_mode_selector_->NeverUsesSynchronousResize()) {
    // A resize ack shouldn't be requested if we have not ACK'd the previous
    // one.
    DCHECK(!params.needs_resize_ack || !next_paint_is_resize_ack());
  }

  // Ignore this during shutdown.
  if (!webwidget_)
    return;

  if (compositor_)
    compositor_->setViewportSize(params.physical_backing_size);

  visible_viewport_size_ = params.visible_viewport_size;
  resizer_rect_ = params.resizer_rect;

  // NOTE: We may have entered fullscreen mode without changing our size.
  bool fullscreen_change =
      is_fullscreen_granted_ != params.is_fullscreen_granted;
  is_fullscreen_granted_ = params.is_fullscreen_granted;
  display_mode_ = params.display_mode;

  size_ = params.new_size;
  physical_backing_size_ = params.physical_backing_size;

  ResizeWebWidget();

  WebSize visual_viewport_size;

  if (IsUseZoomForDSFEnabled()) {
    visual_viewport_size = gfx::ScaleToCeiledSize(
        params.visible_viewport_size,
        GetOriginalDeviceScaleFactor());
  } else {
    visual_viewport_size = visible_viewport_size_;
  }

  webwidget()->resizeVisualViewport(visual_viewport_size);

  // When resizing, we want to wait to paint before ACK'ing the resize.  This
  // ensures that we only resize as fast as we can paint.  We only need to
  // send an ACK if we are resized to a non-empty rect.
  if (params.new_size.IsEmpty() || params.physical_backing_size.IsEmpty()) {
    // In this case there is no paint/composite and therefore no
    // ViewHostMsg_UpdateRect to send the resize ack with. We'd need to send the
    // ack through a fake ViewHostMsg_UpdateRect or a different message.
    DCHECK(!params.needs_resize_ack);
  }

  // Send the Resize_ACK flag once we paint again if requested.
  if (params.needs_resize_ack)
    set_next_paint_is_resize_ack();

  if (fullscreen_change)
    DidToggleFullscreen();

  if (orientation_changed)
    OnOrientationChange();

  // If a resize ack is requested and it isn't set-up, then no more resizes will
  // come in and in general things will go wrong.
  DCHECK(!params.needs_resize_ack || next_paint_is_resize_ack());
}

void RenderWidget::SetScreenMetricsEmulationParameters(
    bool enabled,
    const blink::WebDeviceEmulationParams& params) {
  // This is only supported in RenderView.
  NOTREACHED();
}

void RenderWidget::SetScreenRects(const gfx::Rect& view_screen_rect,
                                  const gfx::Rect& window_screen_rect) {
  view_screen_rect_ = view_screen_rect;
  window_screen_rect_ = window_screen_rect;
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::didAutoResize(const WebSize& new_size) {
  WebRect new_size_in_window(0, 0, new_size.width, new_size.height);
  convertViewportToWindow(&new_size_in_window);
  if (size_.width() != new_size_in_window.width ||
      size_.height() != new_size_in_window.height) {
    size_ = gfx::Size(new_size_in_window.width, new_size_in_window.height);

    if (resizing_mode_selector_->is_synchronous_mode()) {
      gfx::Rect new_pos(rootWindowRect().x,
                        rootWindowRect().y,
                        size_.width(),
                        size_.height());
      view_screen_rect_ = new_pos;
      window_screen_rect_ = new_pos;
    }

    AutoResizeCompositor();

    if (!resizing_mode_selector_->is_synchronous_mode())
      need_update_rect_for_auto_resize_ = true;
  }
}

void RenderWidget::AutoResizeCompositor()  {
  physical_backing_size_ = gfx::ScaleToCeiledSize(size_, device_scale_factor_);
  if (compositor_)
    compositor_->setViewportSize(physical_backing_size_);
}

void RenderWidget::initializeLayerTreeView() {
  DCHECK(!host_closing_);

  compositor_ = RenderWidgetCompositor::Create(this, device_scale_factor_,
                                               compositor_deps_);
  compositor_->setViewportSize(physical_backing_size_);
  OnDeviceScaleFactorChanged();
  // For background pages and certain tests, we don't want to trigger
  // OutputSurface creation.
  if (compositor_never_visible_ || !RenderThreadImpl::current())
    compositor_->SetNeverVisible();

  StartCompositor();
}

void RenderWidget::WillCloseLayerTreeView() {
  if (host_closing_)
    return;

  // Prevent new compositors or output surfaces from being created.
  host_closing_ = true;

  // Always send this notification to prevent new layer tree views from
  // being created, even if one hasn't been created yet.
  if (webwidget_)
    webwidget_->willCloseLayerTreeView();
}

blink::WebLayerTreeView* RenderWidget::layerTreeView() {
  return compositor_.get();
}

void RenderWidget::didMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {
  if (layout_type == blink::WebMeaningfulLayout::VisuallyNonEmpty) {
    QueueMessage(new ViewHostMsg_DidFirstVisuallyNonEmptyPaint(routing_id_),
                 MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
  }

  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_,
                    DidMeaningfulLayout(layout_type));
}

void RenderWidget::ScheduleComposite() {
  if (compositor_ &&
      compositor_deps_->GetCompositorImplThreadTaskRunner().get()) {
    compositor_->setNeedsAnimate();
  }
}

void RenderWidget::ScheduleCompositeWithForcedRedraw() {
  if (compositor_) {
    // Regardless of whether threaded compositing is enabled, always
    // use this mechanism to force the compositor to redraw. However,
    // the invalidation code path below is still needed for the
    // non-threaded case.
    compositor_->SetNeedsForcedRedraw();
  }
  ScheduleComposite();
}

// static
std::unique_ptr<cc::SwapPromise> RenderWidget::QueueMessageImpl(
    IPC::Message* msg,
    MessageDeliveryPolicy policy,
    FrameSwapMessageQueue* frame_swap_message_queue,
    scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
    int source_frame_number) {
  bool first_message_for_frame = false;
  frame_swap_message_queue->QueueMessageForFrame(policy, source_frame_number,
                                                 base::WrapUnique(msg),
                                                 &first_message_for_frame);
  if (first_message_for_frame) {
    std::unique_ptr<cc::SwapPromise> promise(new QueueMessageSwapPromise(
        sync_message_filter, frame_swap_message_queue, source_frame_number));
    return promise;
  }
  return nullptr;
}

void RenderWidget::QueueMessage(IPC::Message* msg,
                                MessageDeliveryPolicy policy) {
  // RenderThreadImpl::current() is NULL in some tests.
  if (!compositor_ || !RenderThreadImpl::current()) {
    Send(msg);
    return;
  }

  std::unique_ptr<cc::SwapPromise> swap_promise =
      QueueMessageImpl(msg, policy, frame_swap_message_queue_.get(),
                       RenderThreadImpl::current()->sync_message_filter(),
                       compositor_->GetSourceFrameNumber());

  if (swap_promise) {
    compositor_->QueueSwapPromise(std::move(swap_promise));
    // Request a commit. This might either A) request a commit ahead of time
    // or B) request a commit which is not needed because there are not
    // pending updates. If B) then the commit will be skipped and the swap
    // promises will be broken (see EarlyOut_NoUpdates). To achieve that we
    // call SetNeedsUpdateLayers instead of SetNeedsCommit so that
    // can_cancel_commit is not unset.
    compositor_->SetNeedsUpdateLayers();
  }
}

void RenderWidget::didChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor;
  InitializeCursorFromWebCursorInfo(&cursor, cursor_info);
  // Only send a SetCursor message if we need to make a change.
  if (!current_cursor_.IsEqual(cursor)) {
    current_cursor_ = cursor;
    Send(new ViewHostMsg_SetCursor(routing_id_, cursor));
  }
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a blocked popup or as a new tab).
//
void RenderWidget::show(WebNavigationPolicy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(routing_id_ != MSG_ROUTING_NONE);
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (did_show_)
    return;

  did_show_ = true;
  // NOTE: initial_rect_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowWidget(opener_id_, routing_id_, initial_rect_));
  SetPendingWindowRect(initial_rect_);
}

void RenderWidget::didFocus() {
}

void RenderWidget::DoDeferredClose() {
  WillCloseLayerTreeView();
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::NotifyOnClose() {
  FOR_EACH_OBSERVER(RenderFrameImpl, render_frames_, WidgetWillClose());
}

void RenderWidget::closeWidgetSoon() {
  DCHECK(content::RenderThread::Get());
  if (is_swapped_out_) {
    // This widget is currently swapped out, and the active widget is in a
    // different process.  Have the browser route the close request to the
    // active widget instead, so that the correct unload handlers are run.
    Send(new ViewHostMsg_RouteCloseEvent(routing_id_));
    return;
  }

  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.

  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RendwerWidgetHost to close now, the window
  // could be closed before the JS finishes executing.  So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close message can be sent.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::DoDeferredClose, this));
}

void RenderWidget::QueueSyntheticGesture(
    std::unique_ptr<SyntheticGestureParams> gesture_params,
    const SyntheticGestureCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  pending_synthetic_gesture_callbacks_.push(callback);

  SyntheticGesturePacket gesture_packet;
  gesture_packet.set_gesture_params(std::move(gesture_params));

  Send(new InputHostMsg_QueueSyntheticGesture(routing_id_, gesture_packet));
}

void RenderWidget::Close() {
  screen_metrics_emulator_.reset();
  WillCloseLayerTreeView();
  compositor_.reset();
  if (webwidget_) {
    webwidget_->close();
    webwidget_ = nullptr;
  }
}

WebRect RenderWidget::windowRect() {
  if (pending_window_rect_count_)
    return pending_window_rect_;

  return view_screen_rect_;
}

void RenderWidget::setToolTipText(const blink::WebString& text,
                                  WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, text, hint));
}

void RenderWidget::setWindowRect(const WebRect& rect_in_screen) {
  WebRect window_rect = rect_in_screen;
  if (popup_origin_scale_for_emulation_) {
    float scale = popup_origin_scale_for_emulation_;
    window_rect.x = popup_screen_origin_for_emulation_.x() +
        (window_rect.x - popup_view_origin_for_emulation_.x()) * scale;
    window_rect.y = popup_screen_origin_for_emulation_.y() +
        (window_rect.y - popup_view_origin_for_emulation_.y()) * scale;
  }

  if (!resizing_mode_selector_->is_synchronous_mode()) {
    if (did_show_) {
      Send(new ViewHostMsg_RequestMove(routing_id_, window_rect));
      SetPendingWindowRect(window_rect);
    } else {
      initial_rect_ = window_rect;
    }
  } else {
    SetWindowRectSynchronously(window_rect);
  }
}

void RenderWidget::SetPendingWindowRect(const WebRect& rect) {
  pending_window_rect_ = rect;
  pending_window_rect_count_++;
}

WebRect RenderWidget::rootWindowRect() {
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    return pending_window_rect_;
  }

  return window_screen_rect_;
}

WebRect RenderWidget::windowResizerRect() {
  return resizer_rect_;
}

void RenderWidget::OnImeSetComposition(
    const base::string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
    const gfx::Range& replacement_range,
    int selection_start, int selection_end) {
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  if (!webwidget_->setComposition(
      text, WebVector<WebCompositionUnderline>(underlines),
      selection_start, selection_end)) {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    Send(new InputHostMsg_ImeCancelComposition(routing_id()));
  }
  UpdateCompositionInfo(true);
}

void RenderWidget::OnImeConfirmComposition(const base::string16& text,
                                           const gfx::Range& replacement_range,
                                           bool keep_selection) {
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  input_handler_->set_handling_input_event(true);
  if (text.length())
    webwidget_->confirmComposition(text);
  else if (keep_selection)
    webwidget_->confirmComposition(WebWidget::KeepSelection);
  else
    webwidget_->confirmComposition(WebWidget::DoNotKeepSelection);
  input_handler_->set_handling_input_event(false);
  UpdateCompositionInfo(true);
}

void RenderWidget::OnDeviceScaleFactorChanged() {
  if (!compositor_)
    return;
  if (IsUseZoomForDSFEnabled())
    compositor_->SetPaintedDeviceScaleFactor(GetOriginalDeviceScaleFactor());
  else
    compositor_->setDeviceScaleFactor(device_scale_factor_);
}

void RenderWidget::OnRepaint(gfx::Size size_to_paint) {
  // During shutdown we can just ignore this message.
  if (!webwidget_)
    return;

  // Even if the browser provides an empty damage rect, it's still expecting to
  // receive a repaint ack so just damage the entire widget bounds.
  if (size_to_paint.IsEmpty()) {
    size_to_paint = size_;
  }

  set_next_paint_is_repaint_ack();
  if (compositor_)
    compositor_->SetNeedsRedrawRect(gfx::Rect(size_to_paint));
}

void RenderWidget::OnSyntheticGestureCompleted() {
  DCHECK(!pending_synthetic_gesture_callbacks_.empty());

  pending_synthetic_gesture_callbacks_.front().Run();
  pending_synthetic_gesture_callbacks_.pop();
}

void RenderWidget::OnSetTextDirection(WebTextDirection direction) {
  if (!webwidget_)
    return;
  webwidget_->setTextDirection(direction);
}

void RenderWidget::OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                                       const gfx::Rect& window_screen_rect) {
  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnUpdateScreenRects(view_screen_rect,
                                                  window_screen_rect);
  } else {
    SetScreenRects(view_screen_rect, window_screen_rect);
  }
  Send(new ViewHostMsg_UpdateScreenRects_ACK(routing_id()));
}

void RenderWidget::OnUpdateWindowScreenRect(
    const gfx::Rect& window_screen_rect) {
  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnUpdateWindowScreenRect(window_screen_rect);
  } else {
    window_screen_rect_ = window_screen_rect;
  }
}

void RenderWidget::OnSetSurfaceIdNamespace(uint32_t surface_id_namespace) {
  if (compositor_)
    compositor_->SetSurfaceIdNamespace(surface_id_namespace);
}

void RenderWidget::OnHandleCompositorProto(const std::vector<uint8_t>& proto) {
  if (compositor_)
    compositor_->OnHandleCompositorProto(proto);
}

void RenderWidget::showImeIfNeeded() {
  OnShowImeIfNeeded();
}

ui::TextInputType RenderWidget::GetTextInputType() {
  if (webwidget_)
    return WebKitToUiTextInputType(webwidget_->textInputType());
  return ui::TEXT_INPUT_TYPE_NONE;
}

void RenderWidget::UpdateCompositionInfo(bool should_update_range) {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateCompositionInfo");
  gfx::Range range = gfx::Range();
  if (should_update_range) {
    GetCompositionRange(&range);
  } else {
    range = composition_range_;
  }
  std::vector<gfx::Rect> character_bounds;
  GetCompositionCharacterBounds(&character_bounds);

  if (!ShouldUpdateCompositionInfo(range, character_bounds))
    return;
  composition_character_bounds_ = character_bounds;
  composition_range_ = range;
  Send(new InputHostMsg_ImeCompositionRangeChanged(
      routing_id(), composition_range_, composition_character_bounds_));
}

void RenderWidget::convertViewportToWindow(blink::WebRect* rect) {
  if (IsUseZoomForDSFEnabled()) {
    float reverse = 1 / GetOriginalDeviceScaleFactor();
    // TODO(oshima): We may need to allow pixel precision here as the the
    // anchor element can be placed at half pixel.
    gfx::Rect window_rect =
        gfx::ScaleToEnclosedRect(gfx::Rect(*rect), reverse);
    rect->x = window_rect.x();
    rect->y = window_rect.y();
    rect->width = window_rect.width();
    rect->height = window_rect.height();
  }
}

void RenderWidget::convertWindowToViewport(blink::WebFloatRect* rect) {
  if (IsUseZoomForDSFEnabled()) {
    rect->x *= GetOriginalDeviceScaleFactor();
    rect->y *= GetOriginalDeviceScaleFactor();
    rect->width *= GetOriginalDeviceScaleFactor();
    rect->height *= GetOriginalDeviceScaleFactor();
  }
}

void RenderWidget::OnShowImeIfNeeded() {
#if defined(OS_ANDROID) || defined(USE_AURA)
  UpdateTextInputState(ShowIme::IF_NEEDED, ChangeSource::FROM_NON_IME);
#endif

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  FocusChangeComplete();
#endif
}

#if defined(OS_ANDROID)
void RenderWidget::OnImeEventSentForAck(const blink::WebTextInputInfo& info) {
  text_input_info_history_.push_back(info);
}

void RenderWidget::OnImeEventAck() {
  DCHECK_GE(text_input_info_history_.size(), 1u);
  text_input_info_history_.pop_front();
}

void RenderWidget::OnRequestTextInputStateUpdate() {
  DCHECK(!ime_event_guard_);
  UpdateSelectionBounds();
  UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_IME);
}
#endif

bool RenderWidget::ShouldHandleImeEvent() {
#if defined(OS_ANDROID)
  if (!webwidget_)
    return false;
  if (IsUsingImeThread())
    return true;

  // We cannot handle IME events if there is any chance that the event we are
  // receiving here from the browser is based on the state that is different
  // from our current one as indicated by |text_input_info_|.
  // The states the browser might be in are:
  // text_input_info_history_[0] - current state ack'd by browser
  // text_input_info_history_[1...N] - pending state changes
  for (size_t i = 0u; i < text_input_info_history_.size() - 1u; ++i) {
    if (text_input_info_history_[i] != text_input_info_)
      return false;
  }
  return true;
#else
  return !!webwidget_;
#endif
}

void RenderWidget::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  device_scale_factor_ = device_scale_factor;

  OnDeviceScaleFactorChanged();

  ScheduleComposite();
}

bool RenderWidget::SetDeviceColorProfile(
    const std::vector<char>& color_profile) {
  if (device_color_profile_ == color_profile)
    return false;

  device_color_profile_ = color_profile;

  if (owner_delegate_)
    owner_delegate_->RenderWidgetDidSetColorProfile(color_profile);

  return true;
}

void RenderWidget::OnOrientationChange() {
}

void RenderWidget::DidFlushPaint() {
  if (owner_delegate_)
    owner_delegate_->RenderWidgetDidFlushPaint();
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it and ensure
  // throttled acks are released in case frame production ceases.
  is_hidden_ = hidden;
  input_handler_->FlushPendingInputEventAck();

  if (is_hidden_)
    RenderThreadImpl::current()->WidgetHidden();
  else
    RenderThreadImpl::current()->WidgetRestored();

  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHidden(hidden);
}

void RenderWidget::DidToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_granted_) {
    webwidget_->didEnterFullScreen();
  } else {
    webwidget_->didExitFullScreen();
  }
}

bool RenderWidget::next_paint_is_resize_ack() const {
  return ViewHostMsg_UpdateRect_Flags::is_resize_ack(next_paint_flags_);
}

void RenderWidget::set_next_paint_is_resize_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK;
}

void RenderWidget::set_next_paint_is_repaint_ack() {
  next_paint_flags_ |= ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK;
}

bool RenderWidget::IsUsingImeThread() {
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(features::kImeThread);
#else
  return false;
#endif
}

void RenderWidget::OnImeEventGuardStart(ImeEventGuard* guard) {
  if (!ime_event_guard_)
    ime_event_guard_ = guard;
}

void RenderWidget::OnImeEventGuardFinish(ImeEventGuard* guard) {
  if (ime_event_guard_ != guard) {
#if defined(OS_ANDROID)
    // In case a from-IME event (e.g. touch) ends up in not-from-IME event
    // (e.g. long press gesture), we want to treat it as not-from-IME event
    // so that ReplicaInputConnection can make changes to its Editable model.
    // Therefore, we want to mark this text state update as 'from IME' only
    // when all the nested events are all originating from IME.
    ime_event_guard_->set_from_ime(
        ime_event_guard_->from_ime() && guard->from_ime());
#endif
    return;
  }
  ime_event_guard_ = nullptr;

  // While handling an ime event, text input state and selection bounds updates
  // are ignored. These must explicitly be updated once finished handling the
  // ime event.
  UpdateSelectionBounds();
#if defined(OS_ANDROID)
  UpdateTextInputState(
      guard->show_ime() ? ShowIme::IF_NEEDED : ShowIme::HIDE_IME,
      guard->from_ime() ? ChangeSource::FROM_IME : ChangeSource::FROM_NON_IME);
#endif
}

void RenderWidget::GetSelectionBounds(gfx::Rect* focus, gfx::Rect* anchor) {
  WebRect focus_webrect;
  WebRect anchor_webrect;
  webwidget_->selectionBounds(focus_webrect, anchor_webrect);
  convertViewportToWindow(&focus_webrect);
  convertViewportToWindow(&anchor_webrect);
  *focus = focus_webrect;
  *anchor = anchor_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateSelectionBounds");
  if (!webwidget_)
    return;
  if (ime_event_guard_)
    return;

#if defined(USE_AURA)
  // TODO(mohsen): For now, always send explicit selection IPC notifications for
  // Aura beucause composited selection updates are not working for webview tags
  // which regresses IME inside webview. Remove this when composited selection
  // updates are fixed for webviews. See, http://crbug.com/510568.
  bool send_ipc = true;
#else
  // With composited selection updates, the selection bounds will be reported
  // directly by the compositor, in which case explicit IPC selection
  // notifications should be suppressed.
  bool send_ipc =
      !blink::WebRuntimeFeatures::isCompositedSelectionUpdateEnabled();
#endif
  if (send_ipc) {
    ViewHostMsg_SelectionBounds_Params params;
    GetSelectionBounds(&params.anchor_rect, &params.focus_rect);
    if (selection_anchor_rect_ != params.anchor_rect ||
        selection_focus_rect_ != params.focus_rect) {
      selection_anchor_rect_ = params.anchor_rect;
      selection_focus_rect_ = params.focus_rect;
      webwidget_->selectionTextDirection(params.focus_dir, params.anchor_dir);
      params.is_anchor_first = webwidget_->isSelectionAnchorFirst();
      Send(new ViewHostMsg_SelectionBoundsChanged(routing_id_, params));
    }
  }

  UpdateCompositionInfo(false);
}

void RenderWidget::SetDeviceColorProfileForTesting(
    const std::vector<char>& color_profile) {
  SetDeviceColorProfile(color_profile);
}

void RenderWidget::ResetDeviceColorProfileForTesting() {
  std::vector<char> color_profile;
  color_profile.push_back('0');
  SetDeviceColorProfile(color_profile);
}

// Check blink::WebTextInputType and ui::TextInputType is kept in sync.
STATIC_ASSERT_ENUM(blink::WebTextInputTypeNone, ui::TEXT_INPUT_TYPE_NONE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeText, ui::TEXT_INPUT_TYPE_TEXT);
STATIC_ASSERT_ENUM(blink::WebTextInputTypePassword,
                   ui::TEXT_INPUT_TYPE_PASSWORD);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeSearch, ui::TEXT_INPUT_TYPE_SEARCH);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeEmail, ui::TEXT_INPUT_TYPE_EMAIL);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeNumber, ui::TEXT_INPUT_TYPE_NUMBER);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeTelephone,
                   ui::TEXT_INPUT_TYPE_TELEPHONE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeURL, ui::TEXT_INPUT_TYPE_URL);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDate, ui::TEXT_INPUT_TYPE_DATE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDateTime,
                   ui::TEXT_INPUT_TYPE_DATE_TIME);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDateTimeLocal,
                   ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeMonth, ui::TEXT_INPUT_TYPE_MONTH);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeTime, ui::TEXT_INPUT_TYPE_TIME);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeWeek, ui::TEXT_INPUT_TYPE_WEEK);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeTextArea,
                   ui::TEXT_INPUT_TYPE_TEXT_AREA);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeContentEditable,
                   ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE);
STATIC_ASSERT_ENUM(blink::WebTextInputTypeDateTimeField,
                   ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD);

ui::TextInputType RenderWidget::WebKitToUiTextInputType(
    blink::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX)) <<
    "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

void RenderWidget::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();
}

void RenderWidget::GetCompositionRange(gfx::Range* range) {
  size_t location, length;
  if (webwidget_->compositionRange(&location, &length)) {
    range->set_start(location);
    range->set_end(location + length);
  } else if (webwidget_->caretOrSelectionRange(&location, &length)) {
    range->set_start(location);
    range->set_end(location + length);
  } else {
    *range = gfx::Range::InvalidRange();
  }
}

bool RenderWidget::ShouldUpdateCompositionInfo(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& bounds) {
  if (composition_range_ != range)
    return true;
  if (bounds.size() != composition_character_bounds_.size())
    return true;
  for (size_t i = 0; i < bounds.size(); ++i) {
    if (bounds[i] != composition_character_bounds_[i])
      return true;
  }
  return false;
}

bool RenderWidget::CanComposeInline() {
  return true;
}

WebScreenInfo RenderWidget::screenInfo() {
  return screen_info_;
}

void RenderWidget::resetInputMethod() {
  ImeEventGuard guard(this);
  // If the last text input type is not None, then we should finish any
  // ongoing composition regardless of the new text input type.
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    if (webwidget_->confirmComposition())
      Send(new InputHostMsg_ImeCancelComposition(routing_id()));
  }

  UpdateCompositionInfo(true);
}

#if defined(OS_ANDROID)
void RenderWidget::showUnhandledTapUIIfNeeded(
    const WebPoint& tapped_position,
    const WebNode& tapped_node,
    bool page_changed) {
  DCHECK(input_handler_->handling_input_event());
  bool should_trigger = !page_changed && tapped_node.isTextNode() &&
                        !tapped_node.isContentEditable() &&
                        !tapped_node.isInsideFocusableElementOrARIAWidget();
  if (should_trigger) {
    Send(new ViewHostMsg_ShowUnhandledTapUIIfNeeded(routing_id_,
        tapped_position.x, tapped_position.y));
  }
}
#endif

void RenderWidget::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
#if defined(OS_ANDROID) || defined(USE_AURA)
  if (event_cancelled)
    return;
  if (event.type == WebInputEvent::GestureTap) {
    UpdateTextInputState(ShowIme::IF_NEEDED, ChangeSource::FROM_NON_IME);
  } else if (event.type == WebInputEvent::GestureLongPress) {
    DCHECK(webwidget_);
    if (webwidget_->textInputInfo().value.isEmpty())
      UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_NON_IME);
    else
      UpdateTextInputState(ShowIme::IF_NEEDED, ChangeSource::FROM_NON_IME);
  }
#endif
}

void RenderWidget::didOverscroll(
    const blink::WebFloatSize& overscrollDelta,
    const blink::WebFloatSize& accumulatedOverscroll,
    const blink::WebFloatPoint& position,
    const blink::WebFloatSize& velocity) {
#if defined(OS_MACOSX)
  // On OSX the user can disable the elastic overscroll effect. If that's the
  // case, don't forward the overscroll notification.
  DCHECK(compositor_deps());
  if (!compositor_deps()->IsElasticOverscrollEnabled())
    return;
#endif
  input_handler_->DidOverscrollFromBlink(overscrollDelta, accumulatedOverscroll,
                                         position, velocity);
}

void RenderWidget::StartCompositor() {
  if (!is_hidden())
    compositor_->setVisible(true);
}

RenderWidgetCompositor* RenderWidget::compositor() const {
  return compositor_.get();
}

void RenderWidget::SetHandlingInputEventForTesting(bool handling_input_event) {
  input_handler_->set_handling_input_event(handling_input_event);
}

bool RenderWidget::SendAckForMouseMoveFromDebugger() {
  return input_handler_->SendAckForMouseMoveFromDebugger();
}

void RenderWidget::IgnoreAckForMouseMoveFromDebugger() {
  input_handler_->IgnoreAckForMouseMoveFromDebugger();
}

void RenderWidget::hasTouchEventHandlers(bool has_handlers) {
  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHasTouchHandler(has_handlers);
  Send(new ViewHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

// Check blink::WebTouchAction and content::TouchAction is kept in sync.
STATIC_ASSERT_ENUM(blink::WebTouchActionNone, TOUCH_ACTION_NONE);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanLeft, TOUCH_ACTION_PAN_LEFT);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanRight, TOUCH_ACTION_PAN_RIGHT);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanX, TOUCH_ACTION_PAN_X);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanUp, TOUCH_ACTION_PAN_UP);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanDown, TOUCH_ACTION_PAN_DOWN);
STATIC_ASSERT_ENUM(blink::WebTouchActionPanY, TOUCH_ACTION_PAN_Y);
STATIC_ASSERT_ENUM(blink::WebTouchActionPan, TOUCH_ACTION_PAN);
STATIC_ASSERT_ENUM(blink::WebTouchActionPinchZoom, TOUCH_ACTION_PINCH_ZOOM);
STATIC_ASSERT_ENUM(blink::WebTouchActionManipulation,
                   TOUCH_ACTION_MANIPULATION);
STATIC_ASSERT_ENUM(blink::WebTouchActionDoubleTapZoom,
                   TOUCH_ACTION_DOUBLE_TAP_ZOOM);
STATIC_ASSERT_ENUM(blink::WebTouchActionAuto, TOUCH_ACTION_AUTO);

void RenderWidget::setTouchAction(
    blink::WebTouchAction web_touch_action) {

  // Ignore setTouchAction calls that result from synthetic touch events (eg.
  // when blink is emulating touch with mouse).
  if (input_handler_->handling_event_type() != WebInputEvent::TouchStart)
    return;

   content::TouchAction content_touch_action =
       static_cast<content::TouchAction>(web_touch_action);
  Send(new InputHostMsg_SetTouchAction(routing_id_, content_touch_action));
}

void RenderWidget::didUpdateTextOfFocusedElementByNonUserInput() {
#if defined(OS_ANDROID)
  if (!IsUsingImeThread())
    text_field_is_dirty_ = true;
#endif
}

void RenderWidget::RegisterRenderFrameProxy(RenderFrameProxy* proxy) {
  render_frame_proxies_.AddObserver(proxy);
}

void RenderWidget::UnregisterRenderFrameProxy(RenderFrameProxy* proxy) {
  render_frame_proxies_.RemoveObserver(proxy);
}

void RenderWidget::RegisterRenderFrame(RenderFrameImpl* frame) {
  render_frames_.AddObserver(frame);
}

void RenderWidget::UnregisterRenderFrame(RenderFrameImpl* frame) {
  render_frames_.RemoveObserver(frame);
}

#if defined(VIDEO_HOLE)
void RenderWidget::RegisterVideoHoleFrame(RenderFrameImpl* frame) {
  video_hole_frames_.AddObserver(frame);
}

void RenderWidget::UnregisterVideoHoleFrame(RenderFrameImpl* frame) {
  video_hole_frames_.RemoveObserver(frame);
}
#endif  // defined(VIDEO_HOLE)

void RenderWidget::OnWaitNextFrameForTests(int routing_id) {
  QueueMessage(new ViewHostMsg_WaitForNextFrameForTests_ACK(routing_id),
               MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
}

float RenderWidget::GetOriginalDeviceScaleFactor() const {
  return
      screen_metrics_emulator_ ?
      screen_metrics_emulator_->original_screen_info().deviceScaleFactor :
      device_scale_factor_;
}

bool RenderWidget::requestPointerLock() {
  return mouse_lock_dispatcher_->LockMouse(webwidget_mouse_lock_target_.get());
}

void RenderWidget::requestPointerUnlock() {
  mouse_lock_dispatcher_->UnlockMouse(webwidget_mouse_lock_target_.get());
}

bool RenderWidget::isPointerLocked() {
  return mouse_lock_dispatcher_->IsMouseLockedTo(
      webwidget_mouse_lock_target_.get());
}

}  // namespace content

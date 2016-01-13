// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/debug/trace_event_synthetic_delay.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/output_surface.h"
#include "cc/trees/layer_tree_host.h"
#include "content/child/npapi/webplugin.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/input/synthetic_gesture_packet.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "content/renderer/gpu/compositor_software_output_device.h"
#include "content/renderer/gpu/delegated_compositor_output_surface.h"
#include "content/renderer/gpu/mailbox_output_surface.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "ipc/ipc_sync_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPopupMenu.h"
#include "third_party/WebKit/public/web/WebPopupMenuInfo.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#include "base/android/sys_utils.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif  // defined(OS_POSIX)

#include "third_party/WebKit/public/web/WebWidget.h"

using blink::WebCompositionUnderline;
using blink::WebCursorInfo;
using blink::WebDeviceEmulationParams;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebNavigationPolicy;
using blink::WebPagePopup;
using blink::WebPopupMenu;
using blink::WebPopupMenuInfo;
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

namespace {

typedef std::map<std::string, ui::TextInputMode> TextInputModeMap;

class TextInputModeMapSingleton {
 public:
  static TextInputModeMapSingleton* GetInstance() {
    return Singleton<TextInputModeMapSingleton>::get();
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

  friend struct DefaultSingletonTraits<TextInputModeMapSingleton>;

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

// TODO(brianderson): Replace the hard-coded threshold with a fraction of
// the BeginMainFrame interval.
// 4166us will allow 1/4 of a 60Hz interval or 1/2 of a 120Hz interval to
// be spent in input hanlders before input starts getting throttled.
const int kInputHandlingTimeThrottlingThresholdMicroseconds = 4166;

}  // namespace

namespace content {

// RenderWidget::ScreenMetricsEmulator ----------------------------------------

class RenderWidget::ScreenMetricsEmulator {
 public:
  ScreenMetricsEmulator(
      RenderWidget* widget,
      const WebDeviceEmulationParams& params);
  virtual ~ScreenMetricsEmulator();

  // Scale and offset used to convert between host coordinates
  // and webwidget coordinates.
  float scale() { return scale_; }
  gfx::Point offset() { return offset_; }
  gfx::Rect applied_widget_rect() const { return applied_widget_rect_; }
  gfx::Rect original_screen_rect() const { return original_view_screen_rect_; }
  const WebScreenInfo& original_screen_info() { return original_screen_info_; }

  void ChangeEmulationParams(
      const WebDeviceEmulationParams& params);

  // The following methods alter handlers' behavior for messages related to
  // widget size and position.
  void OnResizeMessage(const ViewMsg_Resize_Params& params);
  void OnUpdateScreenRectsMessage(const gfx::Rect& view_screen_rect,
                                  const gfx::Rect& window_screen_rect);
  void OnShowContextMenu(ContextMenuParams* params);

 private:
  void Reapply();
  void Apply(float overdraw_bottom_height,
             gfx::Rect resizer_rect,
             bool is_fullscreen);

  RenderWidget* widget_;

  // Parameters as passed by RenderWidget::EnableScreenMetricsEmulation.
  WebDeviceEmulationParams params_;

  // The computed scale and offset used to fit widget into browser window.
  float scale_;
  gfx::Point offset_;

  // Widget rect as passed to webwidget.
  gfx::Rect applied_widget_rect_;

  // Original values to restore back after emulation ends.
  gfx::Size original_size_;
  gfx::Size original_physical_backing_size_;
  gfx::Size original_visible_viewport_size_;
  blink::WebScreenInfo original_screen_info_;
  gfx::Rect original_view_screen_rect_;
  gfx::Rect original_window_screen_rect_;
};

RenderWidget::ScreenMetricsEmulator::ScreenMetricsEmulator(
    RenderWidget* widget,
    const WebDeviceEmulationParams& params)
    : widget_(widget),
      params_(params),
      scale_(1.f) {
  original_size_ = widget_->size_;
  original_physical_backing_size_ = widget_->physical_backing_size_;
  original_visible_viewport_size_ = widget_->visible_viewport_size_;
  original_screen_info_ = widget_->screen_info_;
  original_view_screen_rect_ = widget_->view_screen_rect_;
  original_window_screen_rect_ = widget_->window_screen_rect_;
  Apply(widget_->overdraw_bottom_height_, widget_->resizer_rect_,
      widget_->is_fullscreen_);
}

RenderWidget::ScreenMetricsEmulator::~ScreenMetricsEmulator() {
  widget_->screen_info_ = original_screen_info_;

  widget_->SetDeviceScaleFactor(original_screen_info_.deviceScaleFactor);
  widget_->SetScreenMetricsEmulationParameters(0.f, gfx::Point(), 1.f);
  widget_->view_screen_rect_ = original_view_screen_rect_;
  widget_->window_screen_rect_ = original_window_screen_rect_;
  widget_->Resize(original_size_, original_physical_backing_size_,
      widget_->overdraw_bottom_height_, original_visible_viewport_size_,
      widget_->resizer_rect_, widget_->is_fullscreen_, NO_RESIZE_ACK);
}

void RenderWidget::ScreenMetricsEmulator::ChangeEmulationParams(
    const WebDeviceEmulationParams& params) {
  params_ = params;
  Reapply();
}

void RenderWidget::ScreenMetricsEmulator::Reapply() {
  Apply(widget_->overdraw_bottom_height_, widget_->resizer_rect_,
      widget_->is_fullscreen_);
}

void RenderWidget::ScreenMetricsEmulator::Apply(
    float overdraw_bottom_height,
    gfx::Rect resizer_rect,
    bool is_fullscreen) {
  applied_widget_rect_.set_size(gfx::Size(params_.viewSize));
  if (!applied_widget_rect_.width())
    applied_widget_rect_.set_width(original_size_.width());
  if (!applied_widget_rect_.height())
    applied_widget_rect_.set_height(original_size_.height());

  if (params_.fitToView && !original_size_.IsEmpty()) {
    int original_width = std::max(original_size_.width(), 1);
    int original_height = std::max(original_size_.height(), 1);
    float width_ratio =
        static_cast<float>(applied_widget_rect_.width()) / original_width;
    float height_ratio =
        static_cast<float>(applied_widget_rect_.height()) / original_height;
    float ratio = std::max(1.0f, std::max(width_ratio, height_ratio));
    scale_ = 1.f / ratio;

    // Center emulated view inside available view space.
    offset_.set_x(
        (original_size_.width() - scale_ * applied_widget_rect_.width()) / 2);
    offset_.set_y(
        (original_size_.height() - scale_ * applied_widget_rect_.height()) / 2);
  } else {
    scale_ = params_.scale;
    offset_.SetPoint(params_.offset.x, params_.offset.y);
  }

  if (params_.screenPosition == WebDeviceEmulationParams::Desktop) {
    applied_widget_rect_.set_origin(original_view_screen_rect_.origin());
    widget_->screen_info_.rect = original_screen_info_.rect;
    widget_->screen_info_.availableRect = original_screen_info_.availableRect;
    widget_->window_screen_rect_ = original_window_screen_rect_;
  } else {
    applied_widget_rect_.set_origin(gfx::Point(0, 0));
    widget_->screen_info_.rect = applied_widget_rect_;
    widget_->screen_info_.availableRect = applied_widget_rect_;
    widget_->window_screen_rect_ = applied_widget_rect_;
  }

  float applied_device_scale_factor = params_.deviceScaleFactor ?
      params_.deviceScaleFactor : original_screen_info_.deviceScaleFactor;
  widget_->screen_info_.deviceScaleFactor = applied_device_scale_factor;

  // Pass three emulation parameters to the blink side:
  // - we keep the real device scale factor in compositor to produce sharp image
  //   even when emulating different scale factor;
  // - in order to fit into view, WebView applies offset and scale to the
  //   root layer.
  widget_->SetScreenMetricsEmulationParameters(
      original_screen_info_.deviceScaleFactor, offset_, scale_);

  widget_->SetDeviceScaleFactor(applied_device_scale_factor);
  widget_->view_screen_rect_ = applied_widget_rect_;

  gfx::Size physical_backing_size = gfx::ToCeiledSize(gfx::ScaleSize(
      original_size_, original_screen_info_.deviceScaleFactor));
  widget_->Resize(applied_widget_rect_.size(), physical_backing_size,
      overdraw_bottom_height, applied_widget_rect_.size(), resizer_rect,
      is_fullscreen, NO_RESIZE_ACK);
}

void RenderWidget::ScreenMetricsEmulator::OnResizeMessage(
    const ViewMsg_Resize_Params& params) {
  bool need_ack = params.new_size != original_size_ &&
      !params.new_size.IsEmpty() && !params.physical_backing_size.IsEmpty();
  original_size_ = params.new_size;
  original_physical_backing_size_ = params.physical_backing_size;
  original_screen_info_ = params.screen_info;
  original_visible_viewport_size_ = params.visible_viewport_size;
  Apply(params.overdraw_bottom_height, params.resizer_rect,
      params.is_fullscreen);

  if (need_ack) {
    widget_->set_next_paint_is_resize_ack();
    if (widget_->compositor_)
      widget_->compositor_->SetNeedsRedrawRect(gfx::Rect(widget_->size_));
  }
}

void RenderWidget::ScreenMetricsEmulator::OnUpdateScreenRectsMessage(
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect) {
  original_view_screen_rect_ = view_screen_rect;
  original_window_screen_rect_ = window_screen_rect;
  if (params_.screenPosition == WebDeviceEmulationParams::Desktop)
    Reapply();
}

void RenderWidget::ScreenMetricsEmulator::OnShowContextMenu(
    ContextMenuParams* params) {
  params->x *= scale_;
  params->x += offset_.x();
  params->y *= scale_;
  params->y += offset_.y();
}

// RenderWidget ---------------------------------------------------------------

RenderWidget::RenderWidget(blink::WebPopupType popup_type,
                           const blink::WebScreenInfo& screen_info,
                           bool swapped_out,
                           bool hidden,
                           bool never_visible)
    : routing_id_(MSG_ROUTING_NONE),
      surface_id_(0),
      webwidget_(NULL),
      opener_id_(MSG_ROUTING_NONE),
      init_complete_(false),
      overdraw_bottom_height_(0.f),
      next_paint_flags_(0),
      auto_resize_mode_(false),
      need_update_rect_for_auto_resize_(false),
      did_show_(false),
      is_hidden_(hidden),
      never_visible_(never_visible),
      is_fullscreen_(false),
      has_focus_(false),
      handling_input_event_(false),
      handling_ime_event_(false),
      handling_event_type_(WebInputEvent::Undefined),
      ignore_ack_for_mouse_move_from_debugger_(false),
      closing_(false),
      is_swapped_out_(swapped_out),
      input_method_is_active_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      text_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      can_compose_inline_(true),
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      suppress_next_char_events_(false),
      screen_info_(screen_info),
      device_scale_factor_(screen_info_.deviceScaleFactor),
      is_threaded_compositing_enabled_(false),
      current_event_latency_info_(NULL),
      next_output_surface_id_(0),
#if defined(OS_ANDROID)
      text_field_is_dirty_(false),
      outstanding_ime_acks_(0),
      body_background_color_(SK_ColorWHITE),
#endif
      popup_origin_scale_for_emulation_(0.f),
      resizing_mode_selector_(new ResizingModeSelector()),
      context_menu_source_type_(ui::MENU_SOURCE_MOUSE),
      has_host_context_menu_location_(false) {
  if (!swapped_out)
    RenderProcess::current()->AddRefProcess();
  DCHECK(RenderThread::Get());
  is_threaded_compositing_enabled_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableThreadedCompositing);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";

  // If we are swapped out, we have released already.
  if (!is_swapped_out_ && RenderProcess::current())
    RenderProcess::current()->ReleaseProcess();
}

// static
RenderWidget* RenderWidget::Create(int32 opener_id,
                                   blink::WebPopupType popup_type,
                                   const blink::WebScreenInfo& screen_info) {
  DCHECK(opener_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderWidget> widget(
      new RenderWidget(popup_type, screen_info, false, false, false));
  if (widget->Init(opener_id)) {  // adds reference on success.
    return widget.get();
  }
  return NULL;
}

// static
WebWidget* RenderWidget::CreateWebWidget(RenderWidget* render_widget) {
  switch (render_widget->popup_type_) {
    case blink::WebPopupTypeNone:  // Nothing to create.
      break;
    case blink::WebPopupTypeSelect:
    case blink::WebPopupTypeSuggestion:
      return WebPopupMenu::create(render_widget);
    case blink::WebPopupTypePage:
      return WebPagePopup::create(render_widget);
    default:
      NOTREACHED();
  }
  return NULL;
}

bool RenderWidget::Init(int32 opener_id) {
  return DoInit(opener_id,
                RenderWidget::CreateWebWidget(this),
                new ViewHostMsg_CreateWidget(opener_id, popup_type_,
                                             &routing_id_, &surface_id_));
}

bool RenderWidget::DoInit(int32 opener_id,
                          WebWidget* web_widget,
                          IPC::SyncMessage* create_widget_message) {
  DCHECK(!webwidget_);

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = web_widget;

  bool result = RenderThread::Get()->Send(create_widget_message);
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

// This is used to complete pending inits and non-pending inits.
void RenderWidget::CompleteInit() {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  init_complete_ = true;

  if (compositor_)
    StartCompositor();

  Send(new ViewHostMsg_RenderViewReady(routing_id_));
}

void RenderWidget::SetSwappedOut(bool is_swapped_out) {
  // We should only toggle between states.
  DCHECK(is_swapped_out_ != is_swapped_out);
  is_swapped_out_ = is_swapped_out;

  // If we are swapping out, we will call ReleaseProcess, allowing the process
  // to exit if all of its RenderViews are swapped out.  We wait until the
  // WasSwappedOut call to do this, to avoid showing the sad tab.
  // If we are swapping in, we call AddRefProcess to prevent the process from
  // exiting.
  if (!is_swapped_out)
    RenderProcess::current()->AddRefProcess();
}

void RenderWidget::EnableScreenMetricsEmulation(
    const WebDeviceEmulationParams& params) {
  if (!screen_metrics_emulator_)
    screen_metrics_emulator_.reset(new ScreenMetricsEmulator(this, params));
  else
    screen_metrics_emulator_->ChangeEmulationParams(params);
}

void RenderWidget::DisableScreenMetricsEmulation() {
  screen_metrics_emulator_.reset();
}

void RenderWidget::SetPopupOriginAdjustmentsForEmulation(
    ScreenMetricsEmulator* emulator) {
  popup_origin_scale_for_emulation_ = emulator->scale();
  popup_view_origin_for_emulation_ = emulator->applied_widget_rect().origin();
  popup_screen_origin_for_emulation_ = gfx::Point(
      emulator->original_screen_rect().origin().x() + emulator->offset().x(),
      emulator->original_screen_rect().origin().y() + emulator->offset().y());
  screen_info_ = emulator->original_screen_info();
  device_scale_factor_ = screen_info_.deviceScaleFactor;
}

void RenderWidget::SetScreenMetricsEmulationParameters(
    float device_scale_factor,
    const gfx::Point& root_layer_offset,
    float root_layer_scale) {
  // This is only supported in RenderView.
  NOTREACHED();
}

#if defined(OS_MACOSX) || defined(OS_ANDROID)
void RenderWidget::SetExternalPopupOriginAdjustmentsForEmulation(
    ExternalPopupMenu* popup, ScreenMetricsEmulator* emulator) {
  popup->SetOriginScaleAndOffsetForEmulation(
      emulator->scale(), emulator->offset());
}
#endif

void RenderWidget::OnShowHostContextMenu(ContextMenuParams* params) {
  if (screen_metrics_emulator_)
    screen_metrics_emulator_->OnShowContextMenu(params);
}

void RenderWidget::ScheduleCompositeWithForcedRedraw() {
  if (compositor_) {
    // Regardless of whether threaded compositing is enabled, always
    // use this mechanism to force the compositor to redraw. However,
    // the invalidation code path below is still needed for the
    // non-threaded case.
    compositor_->SetNeedsForcedRedraw();
  }
  scheduleComposite();
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(InputMsg_CursorVisibilityChange,
                        OnCursorVisibilityChange)
    IPC_MESSAGE_HANDLER(InputMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(InputMsg_SyntheticGestureCompleted,
                        OnSyntheticGestureCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_CreatingNew_ACK, OnCreatingNewAck)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_ChangeResizeRect, OnChangeResizeRect)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(ViewMsg_WasSwappedOut, OnWasSwappedOut)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInputMethodActive, OnSetInputMethodActive)
    IPC_MESSAGE_HANDLER(ViewMsg_CandidateWindowShown, OnCandidateWindowShown)
    IPC_MESSAGE_HANDLER(ViewMsg_CandidateWindowUpdated,
                        OnCandidateWindowUpdated)
    IPC_MESSAGE_HANDLER(ViewMsg_CandidateWindowHidden, OnCandidateWindowHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeConfirmComposition, OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScreenRects, OnUpdateScreenRects)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowImeIfNeeded, OnShowImeIfNeeded)
    IPC_MESSAGE_HANDLER(ViewMsg_ImeEventAck, OnImeEventAck)
#endif
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

void RenderWidget::Resize(const gfx::Size& new_size,
                          const gfx::Size& physical_backing_size,
                          float overdraw_bottom_height,
                          const gfx::Size& visible_viewport_size,
                          const gfx::Rect& resizer_rect,
                          bool is_fullscreen,
                          ResizeAck resize_ack) {
  if (resizing_mode_selector_->NeverUsesSynchronousResize()) {
    // A resize ack shouldn't be requested if we have not ACK'd the previous
    // one.
    DCHECK(resize_ack != SEND_RESIZE_ACK || !next_paint_is_resize_ack());
    DCHECK(resize_ack == SEND_RESIZE_ACK || resize_ack == NO_RESIZE_ACK);
  }

  // Ignore this during shutdown.
  if (!webwidget_)
    return;

  if (compositor_) {
    compositor_->setViewportSize(new_size, physical_backing_size);
    compositor_->SetOverdrawBottomHeight(overdraw_bottom_height);
  }

  physical_backing_size_ = physical_backing_size;
  overdraw_bottom_height_ = overdraw_bottom_height;
  visible_viewport_size_ = visible_viewport_size;
  resizer_rect_ = resizer_rect;

  // NOTE: We may have entered fullscreen mode without changing our size.
  bool fullscreen_change = is_fullscreen_ != is_fullscreen;
  if (fullscreen_change)
    WillToggleFullscreen();
  is_fullscreen_ = is_fullscreen;

  if (size_ != new_size) {
    size_ = new_size;

    // When resizing, we want to wait to paint before ACK'ing the resize.  This
    // ensures that we only resize as fast as we can paint.  We only need to
    // send an ACK if we are resized to a non-empty rect.
    webwidget_->resize(new_size);
  } else if (!resizing_mode_selector_->is_synchronous_mode()) {
    resize_ack = NO_RESIZE_ACK;
  }

  webwidget()->resizePinchViewport(gfx::Size(
      visible_viewport_size.width(),
      visible_viewport_size.height()));

  if (new_size.IsEmpty() || physical_backing_size.IsEmpty()) {
    // For empty size or empty physical_backing_size, there is no next paint
    // (along with which to send the ack) until they are set to non-empty.
    resize_ack = NO_RESIZE_ACK;
  }

  // Send the Resize_ACK flag once we paint again if requested.
  if (resize_ack == SEND_RESIZE_ACK)
    set_next_paint_is_resize_ack();

  if (fullscreen_change)
    DidToggleFullscreen();

  // If a resize ack is requested and it isn't set-up, then no more resizes will
  // come in and in general things will go wrong.
  DCHECK(resize_ack != SEND_RESIZE_ACK || next_paint_is_resize_ack());
}

void RenderWidget::ResizeSynchronously(const gfx::Rect& new_position) {
  Resize(new_position.size(), new_position.size(), overdraw_bottom_height_,
         visible_viewport_size_, gfx::Rect(), is_fullscreen_, NO_RESIZE_ACK);
  view_screen_rect_ = new_position;
  window_screen_rect_ = new_position;
  if (!did_show_)
    initial_pos_ = new_position;
}

void RenderWidget::OnClose() {
  if (closing_)
    return;
  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    if (RenderThreadImpl::current())
      RenderThreadImpl::current()->WidgetDestroyed();
    RenderThread::Get()->RemoveRoute(routing_id_);
    SetHidden(false);
  }

  // If there is a Send call on the stack, then it could be dangerous to close
  // now.  Post a task that only gets invoked when there are no nested message
  // loops.
  base::MessageLoop::current()->PostNonNestableTask(
      FROM_HERE, base::Bind(&RenderWidget::Close, this));

  // Balances the AddRef taken when we called AddRoute.
  Release();
}

// Got a response from the browser after the renderer decided to create a new
// view.
void RenderWidget::OnCreatingNewAck() {
  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  CompleteInit();
}

void RenderWidget::OnResize(const ViewMsg_Resize_Params& params) {
  if (resizing_mode_selector_->ShouldAbortOnResize(this, params))
    return;

  if (screen_metrics_emulator_) {
    screen_metrics_emulator_->OnResizeMessage(params);
    return;
  }

  bool orientation_changed =
      screen_info_.orientationAngle != params.screen_info.orientationAngle;

  screen_info_ = params.screen_info;
  SetDeviceScaleFactor(screen_info_.deviceScaleFactor);
  Resize(params.new_size, params.physical_backing_size,
         params.overdraw_bottom_height, params.visible_viewport_size,
         params.resizer_rect, params.is_fullscreen, SEND_RESIZE_ACK);

  if (orientation_changed)
    OnOrientationChange();
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

void RenderWidget::OnWasShown(bool needs_repainting) {
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
  if (compositor_)
    compositor_->SetNeedsForcedRedraw();
  scheduleComposite();
}

void RenderWidget::OnWasSwappedOut() {
  // If we have been swapped out and no one else is using this process,
  // it's safe to exit now.  If we get swapped back in, we will call
  // AddRefProcess in SetSwappedOut.
  if (is_swapped_out_)
    RenderProcess::current()->ReleaseProcess();
}

void RenderWidget::OnRequestMoveAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
}

GURL RenderWidget::GetURLForGraphicsContext3D() {
  return GURL();
}

scoped_ptr<cc::OutputSurface> RenderWidget::CreateOutputSurface(bool fallback) {
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::OutputSurface.
  DCHECK(!never_visible_);

#if defined(OS_ANDROID)
  if (SynchronousCompositorFactory* factory =
      SynchronousCompositorFactory::GetInstance()) {
    return factory->CreateOutputSurface(routing_id());
  }
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool use_software = fallback;
  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    use_software = true;

  scoped_refptr<ContextProviderCommandBuffer> context_provider;
  if (!use_software) {
    context_provider = ContextProviderCommandBuffer::Create(
        CreateGraphicsContext3D(), "RenderCompositor");
    if (!context_provider.get()) {
      // Cause the compositor to wait and try again.
      return scoped_ptr<cc::OutputSurface>();
    }
  }

  uint32 output_surface_id = next_output_surface_id_++;
  if (command_line.HasSwitch(switches::kEnableDelegatedRenderer)) {
    DCHECK(is_threaded_compositing_enabled_);
    return scoped_ptr<cc::OutputSurface>(
        new DelegatedCompositorOutputSurface(
            routing_id(),
            output_surface_id,
            context_provider));
  }
  if (!context_provider.get()) {
    scoped_ptr<cc::SoftwareOutputDevice> software_device(
        new CompositorSoftwareOutputDevice());

    return scoped_ptr<cc::OutputSurface>(new CompositorOutputSurface(
        routing_id(),
        output_surface_id,
        NULL,
        software_device.Pass(),
        true));
  }

  if (command_line.HasSwitch(cc::switches::kCompositeToMailbox)) {
    // Composite-to-mailbox is currently used for layout tests in order to cause
    // them to draw inside in the renderer to do the readback there. This should
    // no longer be the case when crbug.com/311404 is fixed.
    DCHECK(is_threaded_compositing_enabled_ ||
           RenderThreadImpl::current()->layout_test_mode());
    cc::ResourceFormat format = cc::RGBA_8888;
#if defined(OS_ANDROID)
    if (base::android::SysUtils::IsLowEndDevice())
      format = cc::RGB_565;
#endif
    return scoped_ptr<cc::OutputSurface>(
        new MailboxOutputSurface(
            routing_id(),
            output_surface_id,
            context_provider,
            scoped_ptr<cc::SoftwareOutputDevice>(),
            format));
  }
  bool use_swap_compositor_frame_message = false;
  return scoped_ptr<cc::OutputSurface>(
      new CompositorOutputSurface(
          routing_id(),
          output_surface_id,
          context_provider,
          scoped_ptr<cc::SoftwareOutputDevice>(),
          use_swap_compositor_frame_message));
}

void RenderWidget::OnSwapBuffersAborted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersAborted");
  // Schedule another frame so the compositor learns about it.
  scheduleComposite();
}

void RenderWidget::OnSwapBuffersPosted() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersPosted");
}

void RenderWidget::OnSwapBuffersComplete() {
  TRACE_EVENT0("renderer", "RenderWidget::OnSwapBuffersComplete");

  // Notify subclasses that composited rendering was flushed to the screen.
  DidFlushPaint();
}

void RenderWidget::OnHandleInputEvent(const blink::WebInputEvent* input_event,
                                      const ui::LatencyInfo& latency_info,
                                      bool is_keyboard_shortcut) {
  base::AutoReset<bool> handling_input_event_resetter(
      &handling_input_event_, true);
  if (!input_event)
    return;
  base::AutoReset<WebInputEvent::Type> handling_event_type_resetter(
      &handling_event_type_, input_event->type);

  base::AutoReset<const ui::LatencyInfo*> resetter(&current_event_latency_info_,
                                                   &latency_info);

  base::TimeTicks start_time;
  if (base::TimeTicks::IsHighResNowFastAndReliable())
    start_time = base::TimeTicks::HighResNow();

  const char* const event_name =
      WebInputEventTraits::GetName(input_event->type);
  TRACE_EVENT1("renderer", "RenderWidget::OnHandleInputEvent",
               "event", event_name);
  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("blink.HandleInputEvent");
  TRACE_EVENT_FLOW_STEP0(
      "input",
      "LatencyInfo.Flow",
      TRACE_ID_DONT_MANGLE(latency_info.trace_id),
      "HanldeInputEventMain");

  scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  ui::LatencyInfo swap_latency_info(latency_info);
  if (compositor_) {
    latency_info_swap_promise_monitor =
        compositor_->CreateLatencyInfoSwapPromiseMonitor(&swap_latency_info)
            .Pass();
  }

  if (base::TimeTicks::IsHighResNowFastAndReliable()) {
    // If we don't have a high res timer, these metrics won't be accurate enough
    // to be worth collecting. Note that this does introduce some sampling bias.

    base::TimeDelta now = base::TimeDelta::FromInternalValue(
        base::TimeTicks::HighResNow().ToInternalValue());

    int64 delta =
        static_cast<int64>((now.InSecondsF() - input_event->timeStampSeconds) *
                           base::Time::kMicrosecondsPerSecond);

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.AggregatedLatency.Renderer2", delta, 1, 10000000, 100);
    base::HistogramBase* counter_for_type = base::Histogram::FactoryGet(
        base::StringPrintf("Event.Latency.Renderer2.%s", event_name),
        1,
        10000000,
        100,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    counter_for_type->Add(delta);
  }

  bool prevent_default = false;
  if (WebInputEvent::isMouseEventType(input_event->type)) {
    const WebMouseEvent& mouse_event =
        *static_cast<const WebMouseEvent*>(input_event);
    TRACE_EVENT2("renderer", "HandleMouseMove",
                 "x", mouse_event.x, "y", mouse_event.y);
    context_menu_source_type_ = ui::MENU_SOURCE_MOUSE;
    prevent_default = WillHandleMouseEvent(mouse_event);
  }

  if (WebInputEvent::isKeyboardEventType(input_event->type)) {
    context_menu_source_type_ = ui::MENU_SOURCE_KEYBOARD;
#if defined(OS_ANDROID)
    // The DPAD_CENTER key on Android has a dual semantic: (1) in the general
    // case it should behave like a select key (i.e. causing a click if a button
    // is focused). However, if a text field is focused (2), its intended
    // behavior is to just show the IME and don't propagate the key.
    // A typical use case is a web form: the DPAD_CENTER should bring up the IME
    // when clicked on an input text field and cause the form submit if clicked
    // when the submit button is focused, but not vice-versa.
    // The UI layer takes care of translating DPAD_CENTER into a RETURN key,
    // but at this point we have to swallow the event for the scenario (2).
    const WebKeyboardEvent& key_event =
        *static_cast<const WebKeyboardEvent*>(input_event);
    if (key_event.nativeKeyCode == AKEYCODE_DPAD_CENTER &&
        GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      OnShowImeIfNeeded();
      prevent_default = true;
    }
#endif
  }

  if (WebInputEvent::isGestureEventType(input_event->type)) {
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(input_event);
    context_menu_source_type_ = ui::MENU_SOURCE_TOUCH;
    prevent_default = prevent_default || WillHandleGestureEvent(gesture_event);
  }

  bool processed = prevent_default;
  if (input_event->type != WebInputEvent::Char || !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (!processed && webwidget_)
      processed = webwidget_->handleInputEvent(*input_event);
  }

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
  if (!processed && is_keyboard_shortcut)
    suppress_next_char_events_ = true;

  InputEventAckState ack_result = processed ?
      INPUT_EVENT_ACK_STATE_CONSUMED : INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  if (!processed &&  input_event->type == WebInputEvent::TouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(input_event);
    // Hit-test for all the pressed touch points. If there is a touch-handler
    // for any of the touch points, then the renderer should continue to receive
    // touch events.
    ack_result = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    for (size_t i = 0; i < touch_event.touchesLength; ++i) {
      if (touch_event.touches[i].state == WebTouchPoint::StatePressed &&
          HasTouchEventHandlersAt(
              gfx::ToFlooredPoint(touch_event.touches[i].position))) {
        ack_result = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
        break;
      }
    }
  }

  // Unconsumed touchmove acks should never be throttled as they're required to
  // dispatch compositor-handled scroll gestures.
  bool event_type_can_be_rate_limited =
      input_event->type == WebInputEvent::MouseMove ||
      input_event->type == WebInputEvent::MouseWheel ||
      (input_event->type == WebInputEvent::TouchMove &&
       ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);

  bool frame_pending = compositor_ && compositor_->BeginMainFrameRequested();

  // If we don't have a fast and accurate HighResNow, we assume the input
  // handlers are heavy and rate limit them.
  bool rate_limiting_wanted = true;
  if (base::TimeTicks::IsHighResNowFastAndReliable()) {
      base::TimeTicks end_time = base::TimeTicks::HighResNow();
      total_input_handling_time_this_frame_ += (end_time - start_time);
      rate_limiting_wanted =
          total_input_handling_time_this_frame_.InMicroseconds() >
          kInputHandlingTimeThrottlingThresholdMicroseconds;
  }

  TRACE_EVENT_SYNTHETIC_DELAY_END("blink.HandleInputEvent");

  // Note that we can't use handling_event_type_ here since it will be overriden
  // by reentrant calls for events after the paused one.
  bool no_ack = ignore_ack_for_mouse_move_from_debugger_ &&
      input_event->type == WebInputEvent::MouseMove;
  if (!WebInputEventTraits::IgnoresAckDisposition(*input_event) && !no_ack) {
    InputHostMsg_HandleInputEvent_ACK_Params ack;
    ack.type = input_event->type;
    ack.state = ack_result;
    ack.latency = swap_latency_info;
    scoped_ptr<IPC::Message> response(
        new InputHostMsg_HandleInputEvent_ACK(routing_id_, ack));
    if (rate_limiting_wanted && event_type_can_be_rate_limited &&
        frame_pending && !is_hidden_) {
      // We want to rate limit the input events in this case, so we'll wait for
      // painting to finish before ACKing this message.
      TRACE_EVENT_INSTANT0("renderer",
        "RenderWidget::OnHandleInputEvent ack throttled",
        TRACE_EVENT_SCOPE_THREAD);
      if (pending_input_event_ack_) {
        // As two different kinds of events could cause us to postpone an ack
        // we send it now, if we have one pending. The Browser should never
        // send us the same kind of event we are delaying the ack for.
        Send(pending_input_event_ack_.release());
      }
      pending_input_event_ack_ = response.Pass();
      if (compositor_)
        compositor_->NotifyInputThrottledUntilCommit();
    } else {
      Send(response.release());
    }
  }
  if (input_event->type == WebInputEvent::MouseMove)
    ignore_ack_for_mouse_move_from_debugger_ = false;

#if defined(OS_ANDROID)
  // Allow the IME to be shown when the focus changes as a consequence
  // of a processed touch end event.
  if (input_event->type == WebInputEvent::TouchEnd && processed)
    UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_NON_IME);
#elif defined(USE_AURA)
  // Show the virtual keyboard if enabled and a user gesture triggers a focus
  // change.
  if (processed && (input_event->type == WebInputEvent::TouchEnd ||
      input_event->type == WebInputEvent::MouseUp))
    UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_IME);
#endif

  if (!prevent_default) {
    if (WebInputEvent::isKeyboardEventType(input_event->type))
      DidHandleKeyEvent();
    if (WebInputEvent::isMouseEventType(input_event->type))
      DidHandleMouseEvent(*(static_cast<const WebMouseEvent*>(input_event)));
    if (WebInputEvent::isTouchEventType(input_event->type))
      DidHandleTouchEvent(*(static_cast<const WebTouchEvent*>(input_event)));
  }
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
}

void RenderWidget::ClearFocus() {
  // We may have got the focus from the browser before this gets processed, in
  // which case we do not want to unfocus ourself.
  if (!has_focus_ && webwidget_)
    webwidget_->setFocus(false);
}

void RenderWidget::FlushPendingInputEventAck() {
  if (pending_input_event_ack_)
    Send(pending_input_event_ack_.release());
  total_input_handling_time_this_frame_ = base::TimeDelta();
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::didAutoResize(const WebSize& new_size) {
  if (size_.width() != new_size.width || size_.height() != new_size.height) {
    size_ = new_size;

    if (resizing_mode_selector_->is_synchronous_mode()) {
      WebRect new_pos(rootWindowRect().x,
                      rootWindowRect().y,
                      new_size.width,
                      new_size.height);
      view_screen_rect_ = new_pos;
      window_screen_rect_ = new_pos;
    }

    AutoResizeCompositor();

    if (!resizing_mode_selector_->is_synchronous_mode())
      need_update_rect_for_auto_resize_ = true;
  }
}

void RenderWidget::AutoResizeCompositor()  {
  physical_backing_size_ = gfx::ToCeiledSize(gfx::ScaleSize(size_,
      device_scale_factor_));
  if (compositor_)
    compositor_->setViewportSize(size_, physical_backing_size_);
}

void RenderWidget::initializeLayerTreeView() {
  compositor_ = RenderWidgetCompositor::Create(
      this, is_threaded_compositing_enabled_);
  compositor_->setViewportSize(size_, physical_backing_size_);
  if (init_complete_)
    StartCompositor();
}

blink::WebLayerTreeView* RenderWidget::layerTreeView() {
  return compositor_.get();
}

void RenderWidget::suppressCompositorScheduling(bool enable) {
  if (compositor_)
    compositor_->SetSuppressScheduleComposite(enable);
}

void RenderWidget::willBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  DCHECK(RenderThreadImpl::current()->compositor_message_loop_proxy().get());

  // The following two can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState(NO_SHOW_IME, FROM_NON_IME);
  UpdateSelectionBounds();
}

void RenderWidget::didBecomeReadyForAdditionalInput() {
  TRACE_EVENT0("renderer", "RenderWidget::didBecomeReadyForAdditionalInput");
  FlushPendingInputEventAck();
}

void RenderWidget::DidCommitCompositorFrame() {
  FOR_EACH_OBSERVER(RenderFrameProxy, render_frame_proxies_,
                    DidCommitCompositorFrame());
#if defined(VIDEO_HOLE)
  FOR_EACH_OBSERVER(RenderFrameImpl, video_hole_frames_,
                    DidCommitCompositorFrame());
#endif  // defined(VIDEO_HOLE)
}

void RenderWidget::didCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "RenderWidget::didCommitAndDrawCompositorFrame");
  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();
}

void RenderWidget::didCompleteSwapBuffers() {
  TRACE_EVENT0("renderer", "RenderWidget::didCompleteSwapBuffers");

  // Notify subclasses threaded composited rendering was flushed to the screen.
  DidFlushPaint();

  if (!next_paint_flags_ &&
      !need_update_rect_for_auto_resize_ &&
      !plugin_window_moves_.size()) {
    return;
  }

  ViewHostMsg_UpdateRect_Params params;
  params.view_size = size_;
  params.plugin_window_moves.swap(plugin_window_moves_);
  params.flags = next_paint_flags_;
  params.scroll_offset = GetScrollOffset();
  params.scale_factor = device_scale_factor_;

  Send(new ViewHostMsg_UpdateRect(routing_id_, params));
  next_paint_flags_ = 0;
  need_update_rect_for_auto_resize_ = false;
}

void RenderWidget::scheduleComposite() {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // render_thread may be NULL in tests.
  if (render_thread && render_thread->compositor_message_loop_proxy().get() &&
      compositor_) {
    compositor_->setNeedsAnimate();
  }
}

void RenderWidget::didChangeCursor(const WebCursorInfo& cursor_info) {
  // TODO(darin): Eliminate this temporary.
  WebCursor cursor;
  InitializeCursorFromWebKitCursorInfo(&cursor, cursor_info);
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
  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowWidget(opener_id_, routing_id_, initial_pos_));
  SetPendingWindowRect(initial_pos_);
}

void RenderWidget::didFocus() {
}

void RenderWidget::didBlur() {
}

void RenderWidget::DoDeferredClose() {
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::closeWidgetSoon() {
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
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&RenderWidget::DoDeferredClose, this));
}

void RenderWidget::QueueSyntheticGesture(
    scoped_ptr<SyntheticGestureParams> gesture_params,
    const SyntheticGestureCompletionCallback& callback) {
  DCHECK(!callback.is_null());

  pending_synthetic_gesture_callbacks_.push(callback);

  SyntheticGesturePacket gesture_packet;
  gesture_packet.set_gesture_params(gesture_params.Pass());

  Send(new InputHostMsg_QueueSyntheticGesture(routing_id_, gesture_packet));
}

void RenderWidget::Close() {
  screen_metrics_emulator_.reset();
  if (webwidget_) {
    webwidget_->willCloseLayerTreeView();
    compositor_.reset();
    webwidget_->close();
    webwidget_ = NULL;
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

void RenderWidget::setWindowRect(const WebRect& rect) {
  WebRect pos = rect;
  if (popup_origin_scale_for_emulation_) {
    float scale = popup_origin_scale_for_emulation_;
    pos.x = popup_screen_origin_for_emulation_.x() +
        (pos.x - popup_view_origin_for_emulation_.x()) * scale;
    pos.y = popup_screen_origin_for_emulation_.y() +
        (pos.y - popup_view_origin_for_emulation_.y()) * scale;
  }

  if (!resizing_mode_selector_->is_synchronous_mode()) {
    if (did_show_) {
      Send(new ViewHostMsg_RequestMove(routing_id_, pos));
      SetPendingWindowRect(pos);
    } else {
      initial_pos_ = pos;
    }
  } else {
    ResizeSynchronously(pos);
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

void RenderWidget::OnSetInputMethodActive(bool is_active) {
  // To prevent this renderer process from sending unnecessary IPC messages to
  // a browser process, we permit the renderer process to send IPC messages
  // only during the input method attached to the browser process is active.
  input_method_is_active_ = is_active;
}

void RenderWidget::OnCandidateWindowShown() {
  webwidget_->didShowCandidateWindow();
}

void RenderWidget::OnCandidateWindowUpdated() {
  webwidget_->didUpdateCandidateWindow();
}

void RenderWidget::OnCandidateWindowHidden() {
  webwidget_->didHideCandidateWindow();
}

void RenderWidget::OnImeSetComposition(
    const base::string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
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
    Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
  }
#if defined(OS_MACOSX) || defined(USE_AURA)
  UpdateCompositionInfo(true);
#endif
}

void RenderWidget::OnImeConfirmComposition(const base::string16& text,
                                           const gfx::Range& replacement_range,
                                           bool keep_selection) {
  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  handling_input_event_ = true;
  if (text.length())
    webwidget_->confirmComposition(text);
  else if (keep_selection)
    webwidget_->confirmComposition(WebWidget::KeepSelection);
  else
    webwidget_->confirmComposition(WebWidget::DoNotKeepSelection);
  handling_input_event_ = false;
#if defined(OS_MACOSX) || defined(USE_AURA)
  UpdateCompositionInfo(true);
#endif
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
    screen_metrics_emulator_->OnUpdateScreenRectsMessage(
        view_screen_rect, window_screen_rect);
  } else {
    view_screen_rect_ = view_screen_rect;
    window_screen_rect_ = window_screen_rect;
  }
  Send(new ViewHostMsg_UpdateScreenRects_ACK(routing_id()));
}

#if defined(OS_ANDROID)
void RenderWidget::OnShowImeIfNeeded() {
  UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_NON_IME);
}

void RenderWidget::IncrementOutstandingImeEventAcks() {
  ++outstanding_ime_acks_;
}

void RenderWidget::OnImeEventAck() {
  --outstanding_ime_acks_;
  DCHECK(outstanding_ime_acks_ >= 0);
}
#endif

bool RenderWidget::ShouldHandleImeEvent() {
#if defined(OS_ANDROID)
  return !!webwidget_ && outstanding_ime_acks_ == 0;
#else
  return !!webwidget_;
#endif
}

bool RenderWidget::SendAckForMouseMoveFromDebugger() {
  if (handling_event_type_ == WebInputEvent::MouseMove) {
    // If we pause multiple times during a single mouse move event, we should
    // only send ACK once.
    if (!ignore_ack_for_mouse_move_from_debugger_) {
      InputHostMsg_HandleInputEvent_ACK_Params ack;
      ack.type = handling_event_type_;
      ack.state = INPUT_EVENT_ACK_STATE_CONSUMED;
      Send(new InputHostMsg_HandleInputEvent_ACK(routing_id_, ack));
    }
    return true;
  }
  return false;
}

void RenderWidget::IgnoreAckForMouseMoveFromDebugger() {
  ignore_ack_for_mouse_move_from_debugger_ = true;
}

void RenderWidget::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  device_scale_factor_ = device_scale_factor;
  scheduleComposite();
}

bool RenderWidget::SetDeviceColorProfile(
    const std::vector<char>& color_profile) {
  if (device_color_profile_ == color_profile)
    return false;

  device_color_profile_ = color_profile;
  return true;
}

void RenderWidget::OnOrientationChange() {
}

gfx::Vector2d RenderWidget::GetScrollOffset() {
  // Bare RenderWidgets don't support scroll offset.
  return gfx::Vector2d();
}

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it.
  is_hidden_ = hidden;
  if (is_hidden_)
    RenderThreadImpl::current()->WidgetHidden();
  else
    RenderThreadImpl::current()->WidgetRestored();
}

void RenderWidget::WillToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
    webwidget_->willExitFullScreen();
  } else {
    webwidget_->willEnterFullScreen();
  }
}

void RenderWidget::DidToggleFullscreen() {
  if (!webwidget_)
    return;

  if (is_fullscreen_) {
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

static bool IsDateTimeInput(ui::TextInputType type) {
  return type == ui::TEXT_INPUT_TYPE_DATE ||
      type == ui::TEXT_INPUT_TYPE_DATE_TIME ||
      type == ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL ||
      type == ui::TEXT_INPUT_TYPE_MONTH ||
      type == ui::TEXT_INPUT_TYPE_TIME ||
      type == ui::TEXT_INPUT_TYPE_WEEK;
}


void RenderWidget::StartHandlingImeEvent() {
  DCHECK(!handling_ime_event_);
  handling_ime_event_ = true;
}

void RenderWidget::FinishHandlingImeEvent() {
  DCHECK(handling_ime_event_);
  handling_ime_event_ = false;
  // While handling an ime event, text input state and selection bounds updates
  // are ignored. These must explicitly be updated once finished handling the
  // ime event.
  UpdateSelectionBounds();
#if defined(OS_ANDROID)
  UpdateTextInputState(NO_SHOW_IME, FROM_IME);
#endif
}

void RenderWidget::UpdateTextInputState(ShowIme show_ime,
                                        ChangeSource change_source) {
  if (handling_ime_event_)
    return;
  if (show_ime == NO_SHOW_IME && !input_method_is_active_)
    return;
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
  if (show_ime == SHOW_IME_IF_NEEDED ||
      (text_input_type_ != new_type ||
       text_input_mode_ != new_mode ||
       text_input_info_ != new_info ||
       can_compose_inline_ != new_can_compose_inline)
#if defined(OS_ANDROID)
      || text_field_is_dirty_
#endif
      ) {
    ViewHostMsg_TextInputState_Params p;
    p.type = new_type;
    p.mode = new_mode;
    p.value = new_info.value.utf8();
    p.selection_start = new_info.selectionStart;
    p.selection_end = new_info.selectionEnd;
    p.composition_start = new_info.compositionStart;
    p.composition_end = new_info.compositionEnd;
    p.can_compose_inline = new_can_compose_inline;
    p.show_ime_if_needed = (show_ime == SHOW_IME_IF_NEEDED);
#if defined(USE_AURA)
    p.is_non_ime_change = true;
#endif
#if defined(OS_ANDROID)
    p.is_non_ime_change = (change_source == FROM_NON_IME) ||
                         text_field_is_dirty_;
    if (p.is_non_ime_change)
      IncrementOutstandingImeEventAcks();
    text_field_is_dirty_ = false;
#endif
    Send(new ViewHostMsg_TextInputStateChanged(routing_id(), p));

    text_input_info_ = new_info;
    text_input_type_ = new_type;
    text_input_mode_ = new_mode;
    can_compose_inline_ = new_can_compose_inline;
  }
}

void RenderWidget::GetSelectionBounds(gfx::Rect* focus, gfx::Rect* anchor) {
  WebRect focus_webrect;
  WebRect anchor_webrect;
  webwidget_->selectionBounds(focus_webrect, anchor_webrect);
  *focus = focus_webrect;
  *anchor = anchor_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  if (!webwidget_)
    return;
  if (handling_ime_event_)
    return;

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
#if defined(OS_MACOSX) || defined(USE_AURA)
  UpdateCompositionInfo(false);
#endif
}

// Check blink::WebTextInputType and ui::TextInputType is kept in sync.
COMPILE_ASSERT(int(blink::WebTextInputTypeNone) == \
               int(ui::TEXT_INPUT_TYPE_NONE), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeText) == \
               int(ui::TEXT_INPUT_TYPE_TEXT), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypePassword) == \
               int(ui::TEXT_INPUT_TYPE_PASSWORD), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeSearch) == \
               int(ui::TEXT_INPUT_TYPE_SEARCH), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeEmail) == \
               int(ui::TEXT_INPUT_TYPE_EMAIL), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeNumber) == \
               int(ui::TEXT_INPUT_TYPE_NUMBER), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeTelephone) == \
               int(ui::TEXT_INPUT_TYPE_TELEPHONE), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeURL) == \
               int(ui::TEXT_INPUT_TYPE_URL), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeDate) == \
               int(ui::TEXT_INPUT_TYPE_DATE), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTime) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTimeLocal) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeMonth) == \
               int(ui::TEXT_INPUT_TYPE_MONTH), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeTime) == \
               int(ui::TEXT_INPUT_TYPE_TIME), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeWeek) == \
               int(ui::TEXT_INPUT_TYPE_WEEK), mismatching_enum);
COMPILE_ASSERT(int(blink::WebTextInputTypeTextArea) == \
               int(ui::TEXT_INPUT_TYPE_TEXT_AREA), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeContentEditable) == \
               int(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE), mismatching_enums);
COMPILE_ASSERT(int(blink::WebTextInputTypeDateTimeField) == \
               int(ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD), mismatching_enums);

ui::TextInputType RenderWidget::WebKitToUiTextInputType(
    blink::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX)) <<
    "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

ui::TextInputType RenderWidget::GetTextInputType() {
  if (webwidget_)
    return WebKitToUiTextInputType(webwidget_->textInputInfo().type);
  return ui::TEXT_INPUT_TYPE_NONE;
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void RenderWidget::UpdateCompositionInfo(bool should_update_range) {
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
  Send(new ViewHostMsg_ImeCompositionRangeChanged(
      routing_id(), composition_range_, composition_character_bounds_));
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
#endif

#if defined(OS_ANDROID)
void RenderWidget::DidChangeBodyBackgroundColor(SkColor bg_color) {
  // If not initialized, default to white. Note that 0 is different from black
  // as black still has alpha 0xFF.
  if (!bg_color)
    bg_color = SK_ColorWHITE;

  if (bg_color != body_background_color_) {
    body_background_color_ = bg_color;
    Send(new ViewHostMsg_DidChangeBodyBackgroundColor(routing_id(), bg_color));
  }
}
#endif

bool RenderWidget::CanComposeInline() {
  return true;
}

WebScreenInfo RenderWidget::screenInfo() {
  return screen_info_;
}

float RenderWidget::deviceScaleFactor() {
  return device_scale_factor_;
}

void RenderWidget::resetInputMethod() {
  if (!input_method_is_active_)
    return;

  ImeEventGuard guard(this);
  // If the last text input type is not None, then we should finish any
  // ongoing composition regardless of the new text input type.
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    if (webwidget_->confirmComposition())
      Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
  }

#if defined(OS_MACOSX) || defined(USE_AURA)
  UpdateCompositionInfo(true);
#endif
}

void RenderWidget::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
#if defined(OS_ANDROID) || defined(USE_AURA)
  if (event_cancelled)
    return;
  if (event.type == WebInputEvent::GestureTap ||
      event.type == WebInputEvent::GestureLongPress) {
    UpdateTextInputState(SHOW_IME_IF_NEEDED, FROM_NON_IME);
  }
#endif
}

void RenderWidget::StartCompositor() {
  // For widgets that are never visible, we don't need the compositor to run
  // at all.
  if (never_visible_)
    return;
  compositor_->setSurfaceReady();
}

void RenderWidget::SchedulePluginMove(const WebPluginGeometry& move) {
  size_t i = 0;
  for (; i < plugin_window_moves_.size(); ++i) {
    if (plugin_window_moves_[i].window == move.window) {
      if (move.rects_valid) {
        plugin_window_moves_[i] = move;
      } else {
        plugin_window_moves_[i].visible = move.visible;
      }
      break;
    }
  }

  if (i == plugin_window_moves_.size())
    plugin_window_moves_.push_back(move);
}

void RenderWidget::CleanupWindowInPluginMoves(gfx::PluginWindowHandle window) {
  for (WebPluginGeometryVector::iterator i = plugin_window_moves_.begin();
       i != plugin_window_moves_.end(); ++i) {
    if (i->window == window) {
      plugin_window_moves_.erase(i);
      break;
    }
  }
}


RenderWidgetCompositor* RenderWidget::compositor() const {
  return compositor_.get();
}

bool RenderWidget::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  return false;
}

bool RenderWidget::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  return false;
}

void RenderWidget::hasTouchEventHandlers(bool has_handlers) {
  Send(new ViewHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

void RenderWidget::setTouchAction(
    blink::WebTouchAction web_touch_action) {

  // Ignore setTouchAction calls that result from synthetic touch events (eg.
  // when blink is emulating touch with mouse).
  if (handling_event_type_ != WebInputEvent::TouchStart)
    return;

   // Verify the same values are used by the types so we can cast between them.
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_AUTO) ==
                      blink::WebTouchActionAuto,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_NONE) ==
                      blink::WebTouchActionNone,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_PAN_X) ==
                      blink::WebTouchActionPanX,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(static_cast<blink::WebTouchAction>(TOUCH_ACTION_PAN_Y) ==
                      blink::WebTouchActionPanY,
                  enum_values_must_match_for_touch_action);
   COMPILE_ASSERT(
       static_cast<blink::WebTouchAction>(TOUCH_ACTION_PINCH_ZOOM) ==
           blink::WebTouchActionPinchZoom,
       enum_values_must_match_for_touch_action);

   content::TouchAction content_touch_action =
       static_cast<content::TouchAction>(web_touch_action);
  Send(new InputHostMsg_SetTouchAction(routing_id_, content_touch_action));
}

void RenderWidget::didUpdateTextOfFocusedElementByNonUserInput() {
#if defined(OS_ANDROID)
  text_field_is_dirty_ = true;
#endif
}

bool RenderWidget::HasTouchEventHandlersAt(const gfx::Point& point) const {
  return true;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
RenderWidget::CreateGraphicsContext3D() {
  if (!webwidget_)
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuCompositing))
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  if (!RenderThreadImpl::current())
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();
  CauseForGpuLaunch cause =
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;
  scoped_refptr<GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync(cause));
  if (!gpu_channel_host)
    return scoped_ptr<WebGraphicsContext3DCommandBufferImpl>();

  // Explicitly disable antialiasing for the compositor. As of the time of
  // this writing, the only platform that supported antialiasing for the
  // compositor was Mac OS X, because the on-screen OpenGL context creation
  // code paths on Windows and Linux didn't yet have multisampling support.
  // Mac OS X essentially always behaves as though it's rendering offscreen.
  // Multisampling has a heavy cost especially on devices with relatively low
  // fill rate like most notebooks, and the Mac implementation would need to
  // be optimized to resolve directly into the IOSurface shared between the
  // GPU and browser processes. For these reasons and to avoid platform
  // disparities we explicitly disable antialiasing.
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;
  attributes.depth = false;
  attributes.stencil = false;
  bool lose_context_when_out_of_memory = true;
  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
#if defined(OS_ANDROID)
  // If we raster too fast we become upload bound, and pending
  // uploads consume memory. For maximum upload throughput, we would
  // want to allow for upload_throughput * pipeline_time of pending
  // uploads, after which we are just wasting memory. Since we don't
  // know our upload throughput yet, this just caps our memory usage.
  size_t divider = 1;
  if (base::android::SysUtils::IsLowEndDevice())
    divider = 6;
  // For reference Nexus10 can upload 1MB in about 2.5ms.
  const double max_mb_uploaded_per_ms = 2.0 / (5 * divider);
  // Deadline to draw a frame to achieve 60 frames per second.
  const size_t kMillisecondsPerFrame = 16;
  // Assuming a two frame deep pipeline between the CPU and the GPU.
  size_t max_transfer_buffer_usage_mb =
      static_cast<size_t>(2 * kMillisecondsPerFrame * max_mb_uploaded_per_ms);
  static const size_t kBytesPerMegabyte = 1024 * 1024;
  // We keep the MappedMemoryReclaimLimit the same as the upload limit
  // to avoid unnecessarily stalling the compositor thread.
  limits.mapped_memory_reclaim_limit =
      max_transfer_buffer_usage_mb * kBytesPerMegabyte;
#endif

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(surface_id(),
                                                GetURLForGraphicsContext3D(),
                                                gpu_channel_host.get(),
                                                attributes,
                                                lose_context_when_out_of_memory,
                                                limits,
                                                NULL));
  return context.Pass();
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

}  // namespace content

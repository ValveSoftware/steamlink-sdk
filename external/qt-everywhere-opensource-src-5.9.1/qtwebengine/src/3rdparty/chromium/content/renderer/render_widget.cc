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
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/copy_output_request.h"
#include "cc/scheduler/begin_frame_source.h"
#include "content/common/content_switches_internal.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/drag_messages.h"
#include "content/common/input/synthetic_gesture_packet.h"
#include "content/common/input_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"
#include "content/renderer/drop_data_builder.h"
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
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_sync_message.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebDragOperation.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/render_widget_scheduling_state.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebInputMethodController.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPopupMenuInfo.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/clipboard/clipboard.h"
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
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif  // defined(OS_POSIX)

#if defined(USE_AURA)
#include "content/public/common/service_manager_connection.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#endif

#if defined(OS_MACOSX)
#include "content/renderer/text_input_client_observer.h"
#endif

using blink::WebCompositionUnderline;
using blink::WebCursorInfo;
using blink::WebDeviceEmulationParams;
using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebDragData;
using blink::WebFrameWidget;
using blink::WebGestureEvent;
using blink::WebImage;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebInputMethodController;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebNavigationPolicy;
using blink::WebNode;
using blink::WebPagePopup;
using blink::WebPoint;
using blink::WebPopupType;
using blink::WebRange;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebVector;
using blink::WebWidget;

namespace content {

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

bool IsDateTimeInput(ui::TextInputType type) {
  return type == ui::TEXT_INPUT_TYPE_DATE ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME ||
         type == ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL ||
         type == ui::TEXT_INPUT_TYPE_MONTH ||
         type == ui::TEXT_INPUT_TYPE_TIME || type == ui::TEXT_INPUT_TYPE_WEEK;
}

content::RenderWidgetInputHandlerDelegate* GetRenderWidgetInputHandlerDelegate(
    content::RenderWidget* widget) {
#if defined(USE_AURA) && !defined(TOOLKIT_QT)
  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  if (content::ServiceManagerConnection::GetForProcess() &&
      cmdline.HasSwitch(switches::kUseMusInRenderer)) {
    return content::RenderWidgetMusConnection::GetOrCreate(
        widget->routing_id());
  }
#endif
  // If we don't have a connection to the Service Manager, then we want to route
  // IPCs back to the browser process rather than Mus so we use the |widget| as
  // the RenderWidgetInputHandlerDelegate.
  return widget;
}

WebDragData DropMetaDataToWebDragData(
    const std::vector<DropData::Metadata>& drop_meta_data) {
  std::vector<WebDragData::Item> item_list;
  for (const auto& meta_data_item : drop_meta_data) {
    if (meta_data_item.kind == DropData::Kind::STRING) {
      WebDragData::Item item;
      item.storageType = WebDragData::Item::StorageTypeString;
      item.stringType = meta_data_item.mime_type;
      // Have to pass a dummy URL here instead of an empty URL because the
      // DropData received by browser_plugins goes through a round trip:
      // DropData::MetaData --> WebDragData-->DropData. In the end, DropData
      // will contain an empty URL (which means no URL is dragged) if the URL in
      // WebDragData is empty.
      if (base::EqualsASCII(meta_data_item.mime_type,
                            ui::Clipboard::kMimeTypeURIList)) {
        item.stringData = WebString::fromUTF8("about:dragdrop-placeholder");
      }
      item_list.push_back(item);
      continue;
    }

    // TODO(hush): crbug.com/584789. Blink needs to support creating a file with
    // just the mimetype. This is needed to drag files to WebView on Android
    // platform.
    if ((meta_data_item.kind == DropData::Kind::FILENAME) &&
        !meta_data_item.filename.empty()) {
      WebDragData::Item item;
      item.storageType = WebDragData::Item::StorageTypeFilename;
      item.filenameData = meta_data_item.filename.AsUTF16Unsafe();
      item_list.push_back(item);
      continue;
    }

    if (meta_data_item.kind == DropData::Kind::FILESYSTEMFILE) {
      WebDragData::Item item;
      item.storageType = WebDragData::Item::StorageTypeFileSystemFile;
      item.fileSystemURL = meta_data_item.file_system_url;
      item_list.push_back(item);
      continue;
    }
  }

  WebDragData result;
  result.initialize();
  result.setItems(item_list);
  return result;
}

WebDragData DropDataToWebDragData(const DropData& drop_data) {
  std::vector<WebDragData::Item> item_list;

  // These fields are currently unused when dragging into WebKit.
  DCHECK(drop_data.download_metadata.empty());
  DCHECK(drop_data.file_contents.empty());
  DCHECK(drop_data.file_description_filename.empty());

  if (!drop_data.text.is_null()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeText);
    item.stringData = drop_data.text.string();
    item_list.push_back(item);
  }

  if (!drop_data.url.is_empty()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeURIList);
    item.stringData = WebString::fromUTF8(drop_data.url.spec());
    item.title = drop_data.url_title;
    item_list.push_back(item);
  }

  if (!drop_data.html.is_null()) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = WebString::fromUTF8(ui::Clipboard::kMimeTypeHTML);
    item.stringData = drop_data.html.string();
    item.baseURL = drop_data.html_base_url;
    item_list.push_back(item);
  }

  for (std::vector<ui::FileInfo>::const_iterator it =
           drop_data.filenames.begin();
       it != drop_data.filenames.end();
       ++it) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeFilename;
    item.filenameData = it->path.AsUTF16Unsafe();
    item.displayNameData = it->display_name.AsUTF16Unsafe();
    item_list.push_back(item);
  }

  for (std::vector<DropData::FileSystemFileInfo>::const_iterator it =
           drop_data.file_system_files.begin();
       it != drop_data.file_system_files.end();
       ++it) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeFileSystemFile;
    item.fileSystemURL = it->url;
    item.fileSystemFileSize = it->size;
    item.fileSystemId = blink::WebString::fromASCII(it->filesystem_id);
    item_list.push_back(item);
  }

  for (std::map<base::string16, base::string16>::const_iterator it =
           drop_data.custom_data.begin();
       it != drop_data.custom_data.end();
       ++it) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeString;
    item.stringType = it->first;
    item.stringData = it->second;
    item_list.push_back(item);
  }

  WebDragData result;
  result.initialize();
  result.setItems(item_list);
  result.setFilesystemId(drop_data.filesystem_id);
  return result;
}

content::RenderWidget::CreateRenderWidgetFunction g_create_render_widget =
    nullptr;

content::RenderWidget::RenderWidgetInitializedCallback
    g_render_widget_initialized = nullptr;

ui::TextInputType ConvertWebTextInputType(blink::WebTextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX))
      << "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<ui::TextInputType>(type);
}

ui::TextInputMode ConvertWebTextInputMode(blink::WebTextInputMode mode) {
  // Check the mode is in the range representable by ui::TextInputMode.
  DCHECK_LE(mode, static_cast<int>(ui::TEXT_INPUT_MODE_MAX))
      << "blink::WebTextInputMode and ui::TextInputMode not synchronized";
  return static_cast<ui::TextInputMode>(mode);
}

}  // namespace

// RenderWidget ---------------------------------------------------------------

RenderWidget::RenderWidget(int32_t widget_routing_id,
                           CompositorDependencies* compositor_deps,
                           blink::WebPopupType popup_type,
                           const ScreenInfo& screen_info,
                           bool swapped_out,
                           bool hidden,
                           bool never_visible)
    : routing_id_(widget_routing_id),
      compositor_deps_(compositor_deps),
      webwidget_internal_(nullptr),
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
      composition_range_(gfx::Range::InvalidRange()),
      popup_type_(popup_type),
      pending_window_rect_count_(0),
      screen_info_(screen_info),
      device_scale_factor_(screen_info_.device_scale_factor),
#if defined(OS_ANDROID)
      text_field_is_dirty_(false),
#endif
      monitor_composition_info_(false),
      popup_origin_scale_for_emulation_(0.f),
      frame_swap_message_queue_(new FrameSwapMessageQueue()),
      resizing_mode_selector_(new ResizingModeSelector()),
      has_host_context_menu_location_(false),
      has_focus_(false),
#if defined(OS_MACOSX)
      text_input_client_observer_(new TextInputClientObserver(this)),
#endif
      focused_pepper_plugin_(nullptr),
      current_content_source_id_(0) {
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
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
  DCHECK(!webwidget_internal_) << "Leaking our WebWidget!";

  // If we are swapped out, we have released already.
  if (!is_swapped_out_ && RenderProcess::current())
    RenderProcess::current()->ReleaseProcess();
}

// static
void RenderWidget::InstallCreateHook(
    CreateRenderWidgetFunction create_render_widget,
    RenderWidgetInitializedCallback render_widget_initialized) {
  CHECK(!g_create_render_widget && !g_render_widget_initialized);
  g_create_render_widget = create_render_widget;
  g_render_widget_initialized = render_widget_initialized;
}

// static
RenderWidget* RenderWidget::Create(int32_t opener_id,
                                   CompositorDependencies* compositor_deps,
                                   blink::WebPopupType popup_type,
                                   const ScreenInfo& screen_info) {
  DCHECK(opener_id != MSG_ROUTING_NONE);

  // Do a synchronous IPC to obtain a routing ID.
  int32_t routing_id = MSG_ROUTING_NONE;
  if (!RenderThreadImpl::current_render_message_filter()->CreateNewWidget(
          opener_id, popup_type, &routing_id)) {
    return nullptr;
  }

  scoped_refptr<RenderWidget> widget(
      new RenderWidget(routing_id, compositor_deps, popup_type, screen_info,
                       false, false, false));
  widget->Init(opener_id, RenderWidget::CreateWebWidget(widget.get()));
  DCHECK(!widget->HasOneRef());  // RenderWidget::Init() adds a reference.
  return widget.get();
}

// static
RenderWidget* RenderWidget::CreateForFrame(
    int widget_routing_id,
    bool hidden,
    const ScreenInfo& screen_info,
    CompositorDependencies* compositor_deps,
    blink::WebLocalFrame* frame) {
  CHECK_NE(widget_routing_id, MSG_ROUTING_NONE);
  // TODO(avi): Before RenderViewImpl has-a RenderWidget, the browser passes the
  // same routing ID for both the view routing ID and the main frame widget
  // routing ID. https://crbug.com/545684
  RenderViewImpl* view = RenderViewImpl::FromRoutingID(widget_routing_id);
  if (view) {
    view->AttachWebFrameWidget(
        RenderWidget::CreateWebFrameWidget(view->GetWidget(), frame));
    return view->GetWidget();
  }
  scoped_refptr<RenderWidget> widget(
      g_create_render_widget
          ? g_create_render_widget(widget_routing_id, compositor_deps,
                                   blink::WebPopupTypeNone, screen_info, false,
                                   hidden, false)
          : new RenderWidget(widget_routing_id, compositor_deps,
                             blink::WebPopupTypeNone, screen_info, false,
                             hidden, false));
  widget->for_oopif_ = true;
  // Init increments the reference count on |widget|, keeping it alive after
  // this function returns.
  widget->Init(MSG_ROUTING_NONE,
               RenderWidget::CreateWebFrameWidget(widget.get(), frame));

  if (g_render_widget_initialized)
    g_render_widget_initialized(widget.get());
  return widget.get();
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

void RenderWidget::Init(int32_t opener_id, WebWidget* web_widget) {
  DCHECK(!webwidget_internal_);
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);

  input_handler_.reset(new RenderWidgetInputHandler(
      GetRenderWidgetInputHandlerDelegate(this), this));

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_internal_ = web_widget;
  webwidget_mouse_lock_target_.reset(
      new WebWidgetLockTarget(webwidget_internal_));
  mouse_lock_dispatcher_.reset(new RenderWidgetMouseLockDispatcher(this));

  RenderThread::Get()->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_Close.
  AddRef();
  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->WidgetCreated();
    if (is_hidden_)
      RenderThreadImpl::current()->WidgetHidden();
  }
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
  device_scale_factor_ = screen_info_.device_scale_factor;
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
#if defined(OS_MACOSX)
  if (IPC_MESSAGE_CLASS(message) == TextInputClientMsgStart)
    return text_input_client_observer_->OnMessageReceived(message);
#endif
  if (mouse_lock_dispatcher_ &&
      mouse_lock_dispatcher_->OnMessageReceived(message))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(InputMsg_CursorVisibilityChange,
                        OnCursorVisibilityChange)
    IPC_MESSAGE_HANDLER(InputMsg_ImeSetComposition, OnImeSetComposition)
    IPC_MESSAGE_HANDLER(InputMsg_ImeCommitText, OnImeCommitText)
    IPC_MESSAGE_HANDLER(InputMsg_ImeFinishComposingText,
                        OnImeFinishComposingText)
    IPC_MESSAGE_HANDLER(InputMsg_MouseCaptureLost, OnMouseCaptureLost)
    IPC_MESSAGE_HANDLER(InputMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(InputMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(InputMsg_SyntheticGestureCompleted,
                        OnSyntheticGestureCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewMsg_Resize, OnResize)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableDeviceEmulation,
                        OnEnableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableDeviceEmulation,
                        OnDisableDeviceEmulation)
    IPC_MESSAGE_HANDLER(ViewMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(ViewMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(ViewMsg_Repaint, OnRepaint)
    IPC_MESSAGE_HANDLER(ViewMsg_SetTextDirection, OnSetTextDirection)
    IPC_MESSAGE_HANDLER(ViewMsg_Move_ACK, OnRequestMoveAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScreenRects, OnUpdateScreenRects)
    IPC_MESSAGE_HANDLER(ViewMsg_WaitForNextFrameForTests,
                        OnWaitNextFrameForTests)
    IPC_MESSAGE_HANDLER(InputMsg_RequestCompositionUpdate,
                        OnRequestCompositionUpdate)
    IPC_MESSAGE_HANDLER(ViewMsg_HandleCompositorProto, OnHandleCompositorProto)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(DragMsg_SourceEnded, OnDragSourceEnded)
    IPC_MESSAGE_HANDLER(DragMsg_SourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(InputMsg_ImeEventAck, OnImeEventAck)
    IPC_MESSAGE_HANDLER(InputMsg_RequestTextInputStateUpdate,
                        OnRequestTextInputStateUpdate)
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

void RenderWidget::SendOrCrash(IPC::Message* message) {
  bool result = Send(message);
  CHECK(closing_ || result) << "Failed to send message";
}

void RenderWidget::SetWindowRectSynchronously(
    const gfx::Rect& new_window_rect) {
  ResizeParams params;
  params.screen_info = screen_info_;
  params.new_size = new_window_rect.size();
  params.physical_backing_size =
      gfx::ScaleToCeiledSize(new_window_rect.size(), device_scale_factor_);
  params.visible_viewport_size = new_window_rect.size();
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

void RenderWidget::OnWasHidden() {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");
  // Go into a mode where we stop generating paint and scrolling events.
  SetHidden(true);
  for (auto& observer : render_frames_)
    observer.WasHidden();
}

void RenderWidget::OnWasShown(bool needs_repainting,
                              const ui::LatencyInfo& latency_info) {
  TRACE_EVENT0("renderer", "RenderWidget::OnWasShown");
  // During shutdown we can just ignore this message.
  if (!GetWebWidget())
    return;

  // See OnWasHidden
  SetHidden(false);
  for (auto& observer : render_frames_)
    observer.WasShown();

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
  if (GetWebWidget())
    GetWebWidget()->setCursorVisibilityState(is_visible);
}

void RenderWidget::OnMouseCaptureLost() {
  if (GetWebWidget())
    GetWebWidget()->mouseCaptureLost();
}

void RenderWidget::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderWidget::OnSetFocus(bool enable) {
  has_focus_ = enable;

  if (GetWebWidget())
    GetWebWidget()->setFocus(enable);

  for (auto& observer : render_frames_)
    observer.RenderWidgetSetFocus(enable);
}

void RenderWidget::SetNeedsMainFrame() {
  RenderWidgetCompositor* rwc = compositor();
  if (!rwc)
    return;
  rwc->setNeedsBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetCompositorDelegate

void RenderWidget::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  GetWebWidget()->applyViewportDeltas(inner_delta, outer_delta,
                                      elastic_overscroll_delta, page_scale,
                                      top_controls_delta);
}

void RenderWidget::BeginMainFrame(double frame_time_sec) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // render_thread may be NULL in tests.
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager)
    input_handler_manager->ProcessRafAlignedInputOnMainThread(routing_id_);

  GetWebWidget()->beginFrame(frame_time_sec);
}

std::unique_ptr<cc::CompositorFrameSink>
RenderWidget::CreateCompositorFrameSink(bool fallback) {
  DCHECK(GetWebWidget());
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::CompositorFrameSink.
  DCHECK(!compositor_never_visible_);
  return RenderThreadImpl::current()->CreateCompositorFrameSink(
      fallback, routing_id_, frame_swap_message_queue_,
      GetURLForGraphicsContext3D());
}

void RenderWidget::DidCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "RenderWidget::DidCommitAndDrawCompositorFrame");

  for (auto& observer : render_frames_)
    observer.DidCommitAndDrawCompositorFrame();

  // Notify subclasses that we initiated the paint operation.
  DidInitiatePaint();
}

void RenderWidget::DidCommitCompositorFrame() {
  for (auto& observer : render_frames_)
    observer.DidCommitCompositorFrame();
  for (auto& observer : render_frame_proxies_)
    observer.DidCommitCompositorFrame();
}

void RenderWidget::DidCompletePageScaleAnimation() {}

void RenderWidget::DidReceiveCompositorFrameAck() {
  TRACE_EVENT0("renderer", "RenderWidget::DidReceiveCompositorFrameAck");

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

void RenderWidget::RequestScheduleAnimation() {
  scheduleAnimation();
}

void RenderWidget::UpdateVisualState() {
  GetWebWidget()->updateAllLifecyclePhases();
}

void RenderWidget::WillBeginCompositorFrame() {
  TRACE_EVENT0("gpu", "RenderWidget::willBeginCompositorFrame");

  // The UpdateTextInputState can result in further layout and possibly
  // enable GPU acceleration so they need to be called before any painting
  // is done.
  UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_NON_IME);
  UpdateSelectionBounds();

  for (auto& observer : render_frame_proxies_)
    observer.WillBeginCompositorFrame();
}

std::unique_ptr<cc::SwapPromise> RenderWidget::RequestCopyOfOutputForLayoutTest(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  return RenderThreadImpl::current()->RequestCopyOfOutputForLayoutTest(
      routing_id_, std::move(request));
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
  ClearEditCommands();
}

void RenderWidget::SetEditCommandForNextKeyEvent(const std::string& name,
                                                 const std::string& value) {
  ClearEditCommands();
  edit_commands_.emplace_back(name, value);
}

void RenderWidget::ClearEditCommands() {
  edit_commands_.clear();
}

void RenderWidget::OnDidOverscroll(const ui::DidOverscrollParams& params) {
  Send(new InputHostMsg_DidOverscroll(routing_id_, params));
}

void RenderWidget::OnInputEventAck(
    std::unique_ptr<InputEventAck> input_event_ack) {
  SendOrCrash(
      new InputHostMsg_HandleInputEvent_ACK(routing_id_, *input_event_ack));
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
  if (GetWebWidget())
    new_info = GetWebWidget()->textInputInfo();
  const ui::TextInputMode new_mode =
      ConvertWebTextInputMode(new_info.inputMode);

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
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);

  return false;
}

bool RenderWidget::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  for (auto& observer : render_frames_)
    observer.RenderWidgetWillHandleMouseEvent();

  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);

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
  GetWebWidget()->resize(GetSizeForWebWidget());
}

gfx::Size RenderWidget::GetSizeForWebWidget() const {
  if (IsUseZoomForDSFEnabled())
    return gfx::ScaleToCeiledSize(size_, GetOriginalDeviceScaleFactor());

  return size_;
}

void RenderWidget::Resize(const ResizeParams& params) {
  bool orientation_changed =
      screen_info_.orientation_angle != params.screen_info.orientation_angle ||
      screen_info_.orientation_type != params.screen_info.orientation_type;

  screen_info_ = params.screen_info;

  if (device_scale_factor_ != screen_info_.device_scale_factor) {
    device_scale_factor_ = screen_info_.device_scale_factor;
    OnDeviceScaleFactorChanged();
    ScheduleComposite();
  }

  if (resizing_mode_selector_->NeverUsesSynchronousResize()) {
    // A resize ack shouldn't be requested if we have not ACK'd the previous
    // one.
    DCHECK(!params.needs_resize_ack || !next_paint_is_resize_ack());
  }

  // Ignore this during shutdown.
  if (!GetWebWidget())
    return;

  if (compositor_) {
    compositor_->setViewportSize(params.physical_backing_size);
    compositor_->setBottomControlsHeight(params.bottom_controls_height);
    compositor_->SetDeviceColorSpace(screen_info_.icc_profile.GetColorSpace());
  }

  visible_viewport_size_ = params.visible_viewport_size;

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

  GetWebWidget()->resizeVisualViewport(visual_viewport_size);

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
  compositor_->SetDeviceColorSpace(screen_info_.icc_profile.GetColorSpace());
  compositor_->SetContentSourceId(current_content_source_id_);
  // For background pages and certain tests, we don't want to trigger
  // CompositorFrameSink creation.
  if (compositor_never_visible_ || !RenderThreadImpl::current())
    compositor_->SetNeverVisible();

  StartCompositor();
  DCHECK_NE(MSG_ROUTING_NONE, routing_id_);
  compositor_->SetFrameSinkId(
      cc::FrameSinkId(RenderThread::Get()->GetClientId(), routing_id_));
}

void RenderWidget::WillCloseLayerTreeView() {
  if (host_closing_)
    return;

  // Prevent new compositors or output surfaces from being created.
  host_closing_ = true;

  // Always send this notification to prevent new layer tree views from
  // being created, even if one hasn't been created yet.
  if (blink::WebWidget* widget = GetWebWidget())
    widget->willCloseLayerTreeView();
}

blink::WebLayerTreeView* RenderWidget::layerTreeView() {
  return compositor_.get();
}

void RenderWidget::didMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {
  if (layout_type == blink::WebMeaningfulLayout::VisuallyNonEmpty) {
    QueueMessage(new ViewHostMsg_DidFirstVisuallyNonEmptyPaint(routing_id_),
                 MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
  }

  for (auto& observer : render_frames_)
    observer.DidMeaningfulLayout(layout_type);
}

void RenderWidget::ScheduleComposite() {
  if (compositor_ &&
      compositor_deps_->GetCompositorImplThreadTaskRunner().get()) {
    compositor_->setNeedsCompositorUpdate();
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

void RenderWidget::DoDeferredClose() {
  WillCloseLayerTreeView();
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidget::NotifyOnClose() {
  for (auto& observer : render_frames_)
    observer.WidgetWillClose();
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
  if (webwidget_internal_) {
    webwidget_internal_->close();
    webwidget_internal_ = nullptr;
  }
}

void RenderWidget::ScreenRectToEmulatedIfNeeded(WebRect* window_rect) const {
  DCHECK(window_rect);
  float scale = popup_origin_scale_for_emulation_;
  if (!scale)
    return;
  window_rect->x =
      popup_view_origin_for_emulation_.x() +
      (window_rect->x - popup_screen_origin_for_emulation_.x()) / scale;
  window_rect->y =
      popup_view_origin_for_emulation_.y() +
      (window_rect->y - popup_screen_origin_for_emulation_.y()) / scale;
}

void RenderWidget::EmulatedToScreenRectIfNeeded(WebRect* window_rect) const {
  DCHECK(window_rect);
  float scale = popup_origin_scale_for_emulation_;
  if (!scale)
    return;
  window_rect->x =
      popup_screen_origin_for_emulation_.x() +
      (window_rect->x - popup_view_origin_for_emulation_.x()) * scale;
  window_rect->y =
      popup_screen_origin_for_emulation_.y() +
      (window_rect->y - popup_view_origin_for_emulation_.y()) * scale;
}

WebRect RenderWidget::windowRect() {
  WebRect rect;
  if (pending_window_rect_count_) {
    // NOTE(mbelshe): If there is a pending_window_rect_, then getting
    // the RootWindowRect is probably going to return wrong results since the
    // browser may not have processed the Move yet.  There isn't really anything
    // good to do in this case, and it shouldn't happen - since this size is
    // only really needed for windowToScreen, which is only used for Popups.
    rect = pending_window_rect_;
  } else {
    rect = window_screen_rect_;
  }

  ScreenRectToEmulatedIfNeeded(&rect);
  return rect;
}

WebRect RenderWidget::viewRect() {
  WebRect rect = view_screen_rect_;
  ScreenRectToEmulatedIfNeeded(&rect);
  return rect;
}

void RenderWidget::setToolTipText(const blink::WebString& text,
                                  WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, text, hint));
}

void RenderWidget::setWindowRect(const WebRect& rect_in_screen) {
  WebRect window_rect = rect_in_screen;
  EmulatedToScreenRectIfNeeded(&window_rect);

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

  // Popups don't get size updates back from the browser so just store the set
  // values.
  if (popup_type_ != blink::WebPopupTypeNone) {
      window_screen_rect_ = rect;
      view_screen_rect_ = rect;
  }
}

void RenderWidget::OnImeSetComposition(
    const base::string16& text,
    const std::vector<WebCompositionUnderline>& underlines,
    const gfx::Range& replacement_range,
    int selection_start, int selection_end) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeSetComposition(
        text, underlines, selection_start, selection_end);
    return;
  }
#endif
  if (replacement_range.IsValid()) {
    GetWebWidget()->applyReplacementRange(
        WebRange(replacement_range.start(), replacement_range.length()));
  }

  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  blink::WebInputMethodController* controller = GetInputMethodController();
  DCHECK(controller);
  if (!controller ||
      !controller->setComposition(
          text, WebVector<WebCompositionUnderline>(underlines), selection_start,
          selection_end)) {
    // If we failed to set the composition text, then we need to let the browser
    // process to cancel the input method's ongoing composition session, to make
    // sure we are in a consistent state.
    Send(new InputHostMsg_ImeCancelComposition(routing_id()));
  }
  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::OnImeCommitText(const base::string16& text,
                                   const gfx::Range& replacement_range,
                                   int relative_cursor_pos) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeCommitText(
        text, replacement_range, relative_cursor_pos);
    return;
  }
#endif
  if (replacement_range.IsValid()) {
    GetWebWidget()->applyReplacementRange(
        WebRange(replacement_range.start(), replacement_range.length()));
  }

  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  input_handler_->set_handling_input_event(true);
  if (auto* controller = GetInputMethodController())
    controller->commitText(text, relative_cursor_pos);
  input_handler_->set_handling_input_event(false);
  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::OnImeFinishComposingText(bool keep_selection) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeFinishComposingText(
        keep_selection);
    return;
  }
#endif

  if (!ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(this);
  input_handler_->set_handling_input_event(true);
  if (auto* controller = GetInputMethodController()) {
    controller->finishComposingText(
        keep_selection ? WebInputMethodController::KeepSelection
                       : WebInputMethodController::DoNotKeepSelection);
  }
  input_handler_->set_handling_input_event(false);
  UpdateCompositionInfo(false /* not an immediate request */);
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
  if (!GetWebWidget())
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
  if (!GetWebWidget())
    return;
  GetWebWidget()->setTextDirection(direction);
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
  if (screen_metrics_emulator_)
    screen_metrics_emulator_->OnUpdateWindowScreenRect(window_screen_rect);
  else
    window_screen_rect_ = window_screen_rect;
}

void RenderWidget::OnHandleCompositorProto(const std::vector<uint8_t>& proto) {
  if (compositor_)
    compositor_->OnHandleCompositorProto(proto);
}

void RenderWidget::OnDragTargetDragEnter(
    const std::vector<DropData::Metadata>& drop_meta_data,
    const gfx::Point& client_point,
    const gfx::Point& screen_point,
    WebDragOperationsMask ops,
    int key_modifiers) {
  if (!GetWebWidget())
    return;

  DCHECK(GetWebWidget()->isWebFrameWidget());
  WebDragOperation operation = static_cast<WebFrameWidget*>(GetWebWidget())->
      dragTargetDragEnter(DropMetaDataToWebDragData(drop_meta_data),
                          client_point, screen_point, ops, key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id(), operation));
}

void RenderWidget::OnDragTargetDragOver(const gfx::Point& client_point,
                                        const gfx::Point& screen_point,
                                        WebDragOperationsMask ops,
                                        int key_modifiers) {
  if (!GetWebWidget())
    return;

  DCHECK(GetWebWidget()->isWebFrameWidget());
  WebDragOperation operation =
      static_cast<WebFrameWidget*>(GetWebWidget())->dragTargetDragOver(
          ConvertWindowPointToViewport(client_point),
          screen_point, ops, key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id(), operation));
}

void RenderWidget::OnDragTargetDragLeave() {
  if (!GetWebWidget())
    return;
  DCHECK(GetWebWidget()->isWebFrameWidget());
  static_cast<WebFrameWidget*>(GetWebWidget())->dragTargetDragLeave();
}

void RenderWidget::OnDragTargetDrop(const DropData& drop_data,
                                    const gfx::Point& client_point,
                                    const gfx::Point& screen_point,
                                    int key_modifiers) {
  if (!GetWebWidget())
    return;

  DCHECK(GetWebWidget()->isWebFrameWidget());
  static_cast<WebFrameWidget*>(GetWebWidget())->dragTargetDrop(
      DropDataToWebDragData(drop_data),
      ConvertWindowPointToViewport(client_point),
      screen_point, key_modifiers);
}

void RenderWidget::OnDragSourceEnded(const gfx::Point& client_point,
                                     const gfx::Point& screen_point,
                                     WebDragOperation op) {
  if (!GetWebWidget())
    return;

  static_cast<WebFrameWidget*>(GetWebWidget())->dragSourceEndedAt(
      ConvertWindowPointToViewport(client_point), screen_point, op);
}

void RenderWidget::OnDragSourceSystemDragEnded() {
  if (!GetWebWidget())
    return;

  static_cast<WebFrameWidget*>(GetWebWidget())->dragSourceSystemDragEnded();
}

void RenderWidget::showImeIfNeeded() {
#if defined(OS_ANDROID) || defined(USE_AURA)
  UpdateTextInputState(ShowIme::IF_NEEDED, ChangeSource::FROM_NON_IME);
#endif

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  FocusChangeComplete();
#endif
}

ui::TextInputType RenderWidget::GetTextInputType() {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->text_input_type();
#endif
  if (GetWebWidget())
    return ConvertWebTextInputType(GetWebWidget()->textInputType());
  return ui::TEXT_INPUT_TYPE_NONE;
}

void RenderWidget::UpdateCompositionInfo(bool immediate_request) {
  if (!monitor_composition_info_ && !immediate_request)
    return;  // Do not calculate composition info if not requested.

  TRACE_EVENT0("renderer", "RenderWidget::UpdateCompositionInfo");
  gfx::Range range;
  std::vector<gfx::Rect> character_bounds;

  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    // Composition information is only available on editable node.
    range = gfx::Range::InvalidRange();
  } else {
    GetCompositionRange(&range);
    GetCompositionCharacterBounds(&character_bounds);
  }

  if (!immediate_request &&
      !ShouldUpdateCompositionInfo(range, character_bounds)) {
    return;
  }
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

void RenderWidget::OnRequestCompositionUpdate(bool immediate_request,
                                              bool monitor_request) {
  monitor_composition_info_ = monitor_request;
  if (!immediate_request)
    return;
  UpdateCompositionInfo(true /* immediate request */);
}

bool RenderWidget::ShouldHandleImeEvent() {
#if defined(OS_ANDROID)
  if (!GetWebWidget())
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
  return !!GetWebWidget();
#endif
}

void RenderWidget::OnSetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  device_scale_factor_ = device_scale_factor;

  OnDeviceScaleFactorChanged();
  ScheduleComposite();

  physical_backing_size_ = gfx::ScaleToCeiledSize(size_, device_scale_factor_);
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

void RenderWidget::SetHidden(bool hidden) {
  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it and ensure
  // throttled acks are released in case frame production ceases.
  is_hidden_ = hidden;

  if (is_hidden_)
    RenderThreadImpl::current()->WidgetHidden();
  else
    RenderThreadImpl::current()->WidgetRestored();

  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHidden(hidden);
}

void RenderWidget::DidToggleFullscreen() {
  if (!GetWebWidget())
    return;

  if (is_fullscreen_granted_) {
    GetWebWidget()->didEnterFullscreen();
  } else {
    GetWebWidget()->didExitFullscreen();
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
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    blink::WebRect caret(focused_pepper_plugin_->GetCaretBounds());
    convertViewportToWindow(&caret);
    *focus = caret;
    *anchor = caret;
    return;
  }
#endif
  WebRect focus_webrect;
  WebRect anchor_webrect;
  GetWebWidget()->selectionBounds(focus_webrect, anchor_webrect);
  convertViewportToWindow(&focus_webrect);
  convertViewportToWindow(&anchor_webrect);
  *focus = focus_webrect;
  *anchor = anchor_webrect;
}

void RenderWidget::UpdateSelectionBounds() {
  TRACE_EVENT0("renderer", "RenderWidget::UpdateSelectionBounds");
  if (!GetWebWidget())
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
      GetWebWidget()->selectionTextDirection(params.focus_dir,
                                             params.anchor_dir);
      params.is_anchor_first = GetWebWidget()->isSelectionAnchorFirst();
      Send(new ViewHostMsg_SelectionBoundsChanged(routing_id_, params));
    }
  }

  UpdateCompositionInfo(false /* not an immediate request */);
}

void RenderWidget::SetDeviceColorProfileForTesting(
    const std::vector<char>& color_profile) {
  SetDeviceColorProfile(color_profile);
}

void RenderWidget::DidAutoResize(const gfx::Size& new_size) {
  WebRect new_size_in_window(0, 0, new_size.width(), new_size.height());
  convertViewportToWindow(&new_size_in_window);
  if (size_.width() != new_size_in_window.width ||
      size_.height() != new_size_in_window.height) {
    size_ = gfx::Size(new_size_in_window.width, new_size_in_window.height);

    if (resizing_mode_selector_->is_synchronous_mode()) {
      gfx::Rect new_pos(windowRect().x, windowRect().y,
                        size_.width(), size_.height());
      view_screen_rect_ = new_pos;
      window_screen_rect_ = new_pos;
    }

    AutoResizeCompositor();

    if (!resizing_mode_selector_->is_synchronous_mode())
      need_update_rect_for_auto_resize_ = true;
  }
}

void RenderWidget::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();

#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return;
#endif

  if (!GetWebWidget())
    return;
  blink::WebVector<blink::WebRect> bounds_from_blink;
  if (!GetWebWidget()->getCompositionCharacterBounds(bounds_from_blink))
    return;

  for (size_t i = 0; i < bounds_from_blink.size(); ++i) {
    convertViewportToWindow(&bounds_from_blink[i]);
    bounds->push_back(bounds_from_blink[i]);
  }
}

void RenderWidget::GetCompositionRange(gfx::Range* range) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return;
#endif
  WebRange web_range = GetWebWidget()->compositionRange();
  if (web_range.isNull()) {
    *range = gfx::Range::InvalidRange();
    return;
  }
  range->set_start(web_range.startOffset());
  range->set_end(web_range.endOffset());
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
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->IsPluginAcceptingCompositionEvents();
#endif
  return true;
}

blink::WebScreenInfo RenderWidget::screenInfo() {
  blink::WebScreenInfo web_screen_info;
  web_screen_info.deviceScaleFactor = screen_info_.device_scale_factor;
  web_screen_info.depth = screen_info_.depth;
  web_screen_info.depthPerComponent = screen_info_.depth_per_component;
  web_screen_info.isMonochrome = screen_info_.is_monochrome;
  web_screen_info.rect = blink::WebRect(screen_info_.rect);
  web_screen_info.availableRect = blink::WebRect(screen_info_.available_rect);
  switch (screen_info_.orientation_type) {
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY:
      web_screen_info.orientationType =
          blink::WebScreenOrientationPortraitPrimary;
      break;
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY:
      web_screen_info.orientationType =
          blink::WebScreenOrientationPortraitSecondary;
      break;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY:
      web_screen_info.orientationType =
          blink::WebScreenOrientationLandscapePrimary;
      break;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY:
      web_screen_info.orientationType =
          blink::WebScreenOrientationLandscapeSecondary;
      break;
    default:
      web_screen_info.orientationType =
          blink::WebScreenOrientationUndefined;
      break;
  }
  web_screen_info.orientationAngle = screen_info_.orientation_angle;
  return web_screen_info;
}

void RenderWidget::resetInputMethod() {
  ImeEventGuard guard(this);
  // If the last text input type is not None, then we should finish any
  // ongoing composition regardless of the new text input type.
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_NONE) {
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    blink::WebInputMethodController* controller = GetInputMethodController();
    if (controller &&
        controller->finishComposingText(
            WebInputMethodController::DoNotKeepSelection))
      Send(new InputHostMsg_ImeCancelComposition(routing_id()));
  }

  UpdateCompositionInfo(false /* not an immediate request */);
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
    DCHECK(GetWebWidget());
    if (GetWebWidget()->textInputInfo().value.isEmpty())
      UpdateTextInputState(ShowIme::HIDE_IME, ChangeSource::FROM_NON_IME);
    else
      UpdateTextInputState(ShowIme::IF_NEEDED, ChangeSource::FROM_NON_IME);
  }
// TODO(ananta): Piggyback off existing IPCs to communicate this information,
// crbug/420130.
#if defined(OS_WIN)
  if (event.type != blink::WebGestureEvent::GestureTap)
    return;

  // TODO(estade): hit test the event against focused node to make sure
  // the tap actually hit the focused node.
  blink::WebTextInputType text_input_type = GetWebWidget()->textInputType();

  Send(new ViewHostMsg_FocusedNodeTouched(
      routing_id_, text_input_type != blink::WebTextInputTypeNone));
#endif
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

void RenderWidget::hasTouchEventHandlers(bool has_handlers) {
  if (render_widget_scheduling_state_)
    render_widget_scheduling_state_->SetHasTouchHandler(has_handlers);
  Send(new ViewHostMsg_HasTouchEventHandlers(routing_id_, has_handlers));
}

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

void RenderWidget::OnWaitNextFrameForTests(int routing_id) {
  QueueMessage(new ViewHostMsg_WaitForNextFrameForTests_ACK(routing_id),
               MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
}

float RenderWidget::GetOriginalDeviceScaleFactor() const {
  return
      screen_metrics_emulator_ ?
      screen_metrics_emulator_->original_screen_info().device_scale_factor :
      device_scale_factor_;
}

gfx::Point RenderWidget::ConvertWindowPointToViewport(
    const gfx::Point& point) {
  blink::WebFloatRect point_in_viewport(point.x(), point.y(), 0, 0);
  convertWindowToViewport(&point_in_viewport);
  return gfx::Point(point_in_viewport.x, point_in_viewport.y);
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

void RenderWidget::startDragging(blink::WebReferrerPolicy policy,
                                 const WebDragData& data,
                                 WebDragOperationsMask mask,
                                 const WebImage& image,
                                 const WebPoint& webImageOffset) {
  blink::WebRect offset_in_window(webImageOffset.x, webImageOffset.y, 0, 0);
  convertViewportToWindow(&offset_in_window);
  DropData drop_data(DropDataBuilder::Build(data));
  drop_data.referrer_policy = policy;
  gfx::Vector2d imageOffset(offset_in_window.x, offset_in_window.y);
  Send(new DragHostMsg_StartDragging(routing_id(), drop_data, mask,
                                     image.getSkBitmap(), imageOffset,
                                     possible_drag_event_info_));
}

uint32_t RenderWidget::GetContentSourceId() {
  return current_content_source_id_;
}

void RenderWidget::IncrementContentSourceId() {
  if (compositor_)
    compositor_->SetContentSourceId(++current_content_source_id_);
}

blink::WebWidget* RenderWidget::GetWebWidget() const {
  return webwidget_internal_;
}

blink::WebInputMethodController* RenderWidget::GetInputMethodController()
    const {
  if (!GetWebWidget()->isWebFrameWidget()) {
    // TODO(ekaramad): We should not get here in response to IME IPC or updates
    // when the RenderWidget is swapped out. We should top sending IPCs from the
    // browser side (https://crbug.com/669219).
    // If there is no WebFrameWidget, then there will be no
    // InputMethodControllers for a WebLocalFrame.
    return nullptr;
  }
  return static_cast<blink::WebFrameWidget*>(GetWebWidget())
      ->getActiveWebInputMethodController();
}

}  // namespace content

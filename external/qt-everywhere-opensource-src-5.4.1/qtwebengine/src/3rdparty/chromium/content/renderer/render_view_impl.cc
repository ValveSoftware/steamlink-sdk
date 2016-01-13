// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <algorithm>
#include <cmath>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/trace_event.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/child/child_thread.h"
#include "content/child/npapi/webplugin_delegate_impl.h"
#include "content/child/request_extra_data.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/database_messages.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/input_messages.h"
#include "content/common/pepper_messages.h"
#include "content/common/socket_stream_handle_data.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/web_preferences.h"
#include "content/renderer/accessibility/renderer_accessibility.h"
#include "content/renderer/accessibility/renderer_accessibility_complete.h"
#include "content/renderer/accessibility/renderer_accessibility_focus_only.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/disambiguation_popup_helper.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/history_controller.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/idle_user_detector.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/memory_benchmarking_extension.h"
#include "content/renderer/mhtml_generator.h"
#include "content/renderer/push_messaging_dispatcher.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl_params.h"
#include "content/renderer/render_view_mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "content/renderer/speech_recognition_dispatcher.h"
#include "content/renderer/stats_collection_controller.h"
#include "content/renderer/stats_collection_observer.h"
#include "content/renderer/text_input_client_observer.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/web_ui_extension.h"
#include "content/renderer/web_ui_extension_data.h"
#include "content/renderer/web_ui_mojo.h"
#include "content/renderer/websharedworker_proxy.h"
#include "media/audio/audio_output_device.h"
#include "media/base/filter_collection.h"
#include "media/base/media_switches.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebSocketStreamHandle.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebColorName.h"
#include "third_party/WebKit/public/web/WebColorSuggestion.h"
#include "third_party/WebKit/public/web/WebDOMEvent.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFileChooserParams.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebGlyphCache.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebPageSerializer.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginDocument.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSearchableFormData.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "third_party/WebKit/public/web/default/WebRenderTheme.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "v8/include/v8.h"
#include "webkit/child/weburlresponse_extradata_impl.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "content/renderer/android/address_detector.h"
#include "content/renderer/android/content_detector.h"
#include "content/renderer/android/email_detector.h"
#include "content/renderer/android/phone_number_detector.h"
#include "net/android/network_library.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "ui/gfx/rect_f.h"

#elif defined(OS_WIN)
// TODO(port): these files are currently Windows only because they concern:
//   * theming
#include "ui/native_theme/native_theme_win.h"
#elif defined(USE_X11)
#include "ui/native_theme/native_theme.h"
#elif defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/renderer/npapi/webplugin_delegate_proxy.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/rtc_peer_connection_handler.h"
#endif

using blink::WebAXObject;
using blink::WebApplicationCacheHost;
using blink::WebApplicationCacheHostClient;
using blink::WebCString;
using blink::WebColor;
using blink::WebColorName;
using blink::WebConsoleMessage;
using blink::WebData;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebDragData;
using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebElement;
using blink::WebExternalPopupMenu;
using blink::WebExternalPopupMenuClient;
using blink::WebFileChooserCompletion;
using blink::WebFindOptions;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebGestureEvent;
using blink::WebHistoryItem;
using blink::WebHTTPBody;
using blink::WebIconURL;
using blink::WebImage;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMediaPlayerAction;
using blink::WebMouseEvent;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPageSerializer;
using blink::WebPageSerializerClient;
using blink::WebPeerConnection00Handler;
using blink::WebPeerConnection00HandlerClient;
using blink::WebPeerConnectionHandler;
using blink::WebPeerConnectionHandlerClient;
using blink::WebPluginAction;
using blink::WebPluginContainer;
using blink::WebPluginDocument;
using blink::WebPoint;
using blink::WebPopupMenuInfo;
using blink::WebRange;
using blink::WebRect;
using blink::WebReferrerPolicy;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSerializedScriptValue;
using blink::WebSettings;
using blink::WebSize;
using blink::WebSocketStreamHandle;
using blink::WebStorageNamespace;
using blink::WebStorageQuotaCallbacks;
using blink::WebStorageQuotaError;
using blink::WebStorageQuotaType;
using blink::WebString;
using blink::WebTextAffinity;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebUserGestureIndicator;
using blink::WebVector;
using blink::WebView;
using blink::WebWidget;
using blink::WebWindowFeatures;
using base::Time;
using base::TimeDelta;
using webkit_glue::WebURLResponseExtraDataImpl;

#if defined(OS_ANDROID)
using blink::WebContentDetectionResult;
using blink::WebFloatPoint;
using blink::WebFloatRect;
using blink::WebHitTestResult;
#endif

namespace content {

//-----------------------------------------------------------------------------

typedef std::map<blink::WebView*, RenderViewImpl*> ViewMap;
static base::LazyInstance<ViewMap> g_view_map = LAZY_INSTANCE_INITIALIZER;
typedef std::map<int32, RenderViewImpl*> RoutingIDViewMap;
static base::LazyInstance<RoutingIDViewMap> g_routing_id_view_map =
    LAZY_INSTANCE_INITIALIZER;

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
const int kDelaySecondsForContentStateSyncHidden = 5;
const int kDelaySecondsForContentStateSync = 1;

#if defined(OS_ANDROID)
// Delay between tapping in content and launching the associated android intent.
// Used to allow users see what has been recognized as content.
const size_t kContentIntentDelayMilliseconds = 700;
#endif

static RenderViewImpl* (*g_create_render_view_impl)(RenderViewImplParams*) =
    NULL;

// static
bool RenderViewImpl::IsReload(const FrameMsg_Navigate_Params& params) {
  return
      params.navigation_type == FrameMsg_Navigate_Type::RELOAD ||
      params.navigation_type == FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE ||
      params.navigation_type ==
          FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
}

// static
Referrer RenderViewImpl::GetReferrerFromRequest(
    WebFrame* frame,
    const WebURLRequest& request) {
  return Referrer(GURL(request.httpHeaderField(WebString::fromUTF8("Referer"))),
                  request.referrerPolicy());
}

// static
WindowOpenDisposition RenderViewImpl::NavigationPolicyToDisposition(
    WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyIgnore:
      return IGNORE_ACTION;
    case blink::WebNavigationPolicyDownload:
      return SAVE_TO_DISK;
    case blink::WebNavigationPolicyCurrentTab:
      return CURRENT_TAB;
    case blink::WebNavigationPolicyNewBackgroundTab:
      return NEW_BACKGROUND_TAB;
    case blink::WebNavigationPolicyNewForegroundTab:
      return NEW_FOREGROUND_TAB;
    case blink::WebNavigationPolicyNewWindow:
      return NEW_WINDOW;
    case blink::WebNavigationPolicyNewPopup:
      return NEW_POPUP;
  default:
    NOTREACHED() << "Unexpected WebNavigationPolicy";
    return IGNORE_ACTION;
  }
}

// Returns true if the device scale is high enough that losing subpixel
// antialiasing won't have a noticeable effect on text quality.
static bool DeviceScaleEnsuresTextQuality(float device_scale_factor) {
#if defined(OS_ANDROID)
  // On Android, we never have subpixel antialiasing.
  return true;
#else
  return device_scale_factor > 1.5f;
#endif

}

static bool ShouldUseFixedPositionCompositing(float device_scale_factor) {
  // Compositing for fixed-position elements is dependent on
  // device_scale_factor if no flag is set. http://crbug.com/172738
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableCompositingForFixedPosition))
    return false;

  if (command_line.HasSwitch(switches::kEnableCompositingForFixedPosition))
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseAcceleratedCompositingForOverflowScroll(
    float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableAcceleratedOverflowScroll))
    return false;

  if (command_line.HasSwitch(switches::kEnableAcceleratedOverflowScroll))
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseCompositedScrollingForFrames(
    float device_scale_factor) {
  if (RenderThreadImpl::current() &&
      !RenderThreadImpl::current()->is_lcd_text_enabled())
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseTransitionCompositing(float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableCompositingForTransition))
    return false;

  if (command_line.HasSwitch(switches::kEnableCompositingForTransition))
    return true;

  // TODO(ajuma): Re-enable this by default for high-DPI once the problem
  // of excessive layer promotion caused by overlap has been addressed.
  // http://crbug.com/178119.
  return false;
}

static bool ShouldUseAcceleratedFixedRootBackground(float device_scale_factor) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableAcceleratedFixedRootBackground))
    return false;

  if (command_line.HasSwitch(switches::kEnableAcceleratedFixedRootBackground))
    return true;

  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

static bool ShouldUseExpandedHeuristicsForGpuRasterization() {
  return base::FieldTrialList::FindFullName(
             "GpuRasterizationExpandedContentWhitelist") == "Enabled";
}

static FaviconURL::IconType ToFaviconType(blink::WebIconURL::Type type) {
  switch (type) {
    case blink::WebIconURL::TypeFavicon:
      return FaviconURL::FAVICON;
    case blink::WebIconURL::TypeTouch:
      return FaviconURL::TOUCH_ICON;
    case blink::WebIconURL::TypeTouchPrecomposed:
      return FaviconURL::TOUCH_PRECOMPOSED_ICON;
    case blink::WebIconURL::TypeInvalid:
      return FaviconURL::INVALID_ICON;
  }
  return FaviconURL::INVALID_ICON;
}

static void ConvertToFaviconSizes(
    const blink::WebVector<blink::WebSize>& web_sizes,
    std::vector<gfx::Size>* sizes) {
  DCHECK(sizes->empty());
  sizes->reserve(web_sizes.size());
  for (size_t i = 0; i < web_sizes.size(); ++i)
    sizes->push_back(gfx::Size(web_sizes[i]));
}

///////////////////////////////////////////////////////////////////////////////

struct RenderViewImpl::PendingFileChooser {
  PendingFileChooser(const FileChooserParams& p, WebFileChooserCompletion* c)
      : params(p),
        completion(c) {
  }
  FileChooserParams params;
  WebFileChooserCompletion* completion;  // MAY BE NULL to skip callback.
};

namespace {

class WebWidgetLockTarget : public MouseLockDispatcher::LockTarget {
 public:
  explicit WebWidgetLockTarget(blink::WebWidget* webwidget)
      : webwidget_(webwidget) {}

  virtual void OnLockMouseACK(bool succeeded) OVERRIDE {
    if (succeeded)
      webwidget_->didAcquirePointerLock();
    else
      webwidget_->didNotAcquirePointerLock();
  }

  virtual void OnMouseLockLost() OVERRIDE {
    webwidget_->didLosePointerLock();
  }

  virtual bool HandleMouseLockedInputEvent(
      const blink::WebMouseEvent &event) OVERRIDE {
    // The WebWidget handles mouse lock in WebKit's handleInputEvent().
    return false;
  }

 private:
  blink::WebWidget* webwidget_;
};

bool TouchEnabled() {
// Based on the definition of chrome::kEnableTouchIcon.
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
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

  // TODO(dcheng): Do we need to distinguish between null and empty URLs? Is it
  // meaningful to write an empty URL to the clipboard?
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

}  // namespace

RenderViewImpl::RenderViewImpl(RenderViewImplParams* params)
    : RenderWidget(blink::WebPopupTypeNone,
                   params->screen_info,
                   params->swapped_out,
                   params->hidden,
                   params->never_visible),
      webkit_preferences_(params->webkit_prefs),
      send_content_state_immediately_(false),
      enabled_bindings_(0),
      send_preferred_size_changes_(false),
      navigation_gesture_(NavigationGestureUnknown),
      opened_by_user_gesture_(true),
      opener_suppressed_(false),
      suppress_dialogs_until_swap_out_(false),
      page_id_(-1),
      last_page_id_sent_to_browser_(-1),
      next_page_id_(params->next_page_id),
      history_list_offset_(-1),
      history_list_length_(0),
      frames_in_progress_(0),
      target_url_status_(TARGET_NONE),
      uses_temporary_zoom_level_(false),
#if defined(OS_ANDROID)
      top_controls_constraints_(cc::BOTH),
#endif
      has_scrolled_focused_editable_node_into_rect_(false),
      push_messaging_dispatcher_(NULL),
      speech_recognition_dispatcher_(NULL),
      media_stream_dispatcher_(NULL),
      browser_plugin_manager_(NULL),
      devtools_agent_(NULL),
      accessibility_mode_(AccessibilityModeOff),
      renderer_accessibility_(NULL),
      mouse_lock_dispatcher_(NULL),
#if defined(OS_ANDROID)
      expected_content_intent_id_(0),
#endif
#if defined(OS_WIN)
      focused_plugin_id_(-1),
#endif
#if defined(ENABLE_PLUGINS)
      plugin_find_handler_(NULL),
      focused_pepper_plugin_(NULL),
      pepper_last_mouse_event_target_(NULL),
#endif
      enumeration_completion_id_(0),
      session_storage_namespace_id_(params->session_storage_namespace_id),
      next_snapshot_id_(0) {
}

void RenderViewImpl::Initialize(RenderViewImplParams* params) {
  routing_id_ = params->routing_id;
  surface_id_ = params->surface_id;
  if (params->opener_id != MSG_ROUTING_NONE && params->is_renderer_created)
    opener_id_ = params->opener_id;

  // Ensure we start with a valid next_page_id_ from the browser.
  DCHECK_GE(next_page_id_, 0);

  main_render_frame_.reset(RenderFrameImpl::Create(
      this, params->main_frame_routing_id));
  // The main frame WebLocalFrame object is closed by
  // RenderFrameImpl::frameDetached().
  WebLocalFrame* web_frame = WebLocalFrame::create(main_render_frame_.get());
  main_render_frame_->SetWebFrame(web_frame);

  if (params->proxy_routing_id != MSG_ROUTING_NONE) {
    CHECK(params->swapped_out);
    RenderFrameProxy* proxy =
        RenderFrameProxy::CreateFrameProxy(params->proxy_routing_id,
                                           params->main_frame_routing_id);
    main_render_frame_->set_render_frame_proxy(proxy);
  }

  webwidget_ = WebView::create(this);
  webwidget_mouse_lock_target_.reset(new WebWidgetLockTarget(webwidget_));

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kStatsCollectionController))
    stats_collection_observer_.reset(new StatsCollectionObserver(this));

#if defined(OS_ANDROID)
  const std::string region_code =
      command_line.HasSwitch(switches::kNetworkCountryIso)
          ? command_line.GetSwitchValueASCII(switches::kNetworkCountryIso)
          : net::android::GetTelephonyNetworkOperator();
  content_detectors_.push_back(linked_ptr<ContentDetector>(
      new AddressDetector()));
  content_detectors_.push_back(linked_ptr<ContentDetector>(
      new PhoneNumberDetector(region_code)));
  content_detectors_.push_back(linked_ptr<ContentDetector>(
      new EmailDetector()));
#endif

  RenderThread::Get()->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_ClosePage.
  AddRef();
  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->WidgetCreated();
    if (is_hidden_)
      RenderThreadImpl::current()->WidgetHidden();
  }

  // If this is a popup, we must wait for the CreatingNew_ACK message before
  // completing initialization.  Otherwise, we can finish it now.
  if (opener_id_ == MSG_ROUTING_NONE) {
    did_show_ = true;
    CompleteInit();
  }

  g_view_map.Get().insert(std::make_pair(webview(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(routing_id_, this));
  webview()->setDeviceScaleFactor(device_scale_factor_);
  webview()->settings()->setAcceleratedCompositingForFixedPositionEnabled(
      ShouldUseFixedPositionCompositing(device_scale_factor_));
  webview()->settings()->setAcceleratedCompositingForOverflowScrollEnabled(
      ShouldUseAcceleratedCompositingForOverflowScroll(device_scale_factor_));
  webview()->settings()->setAcceleratedCompositingForTransitionEnabled(
      ShouldUseTransitionCompositing(device_scale_factor_));
  webview()->settings()->setAcceleratedCompositingForFixedRootBackgroundEnabled(
      ShouldUseAcceleratedFixedRootBackground(device_scale_factor_));
  webview()->settings()->setCompositedScrollingForFramesEnabled(
      ShouldUseCompositedScrollingForFrames(device_scale_factor_));
  webview()->settings()->setUseExpandedHeuristicsForGpuRasterization(
      ShouldUseExpandedHeuristicsForGpuRasterization());

  ApplyWebPreferences(webkit_preferences_, webview());

  webview()->settings()->setAllowConnectingInsecureWebSocket(
      command_line.HasSwitch(switches::kAllowInsecureWebSocketFromHttpsOrigin));

  webview()->setMainFrame(main_render_frame_->GetWebFrame());
  main_render_frame_->Initialize();

  if (switches::IsTouchDragDropEnabled())
    webview()->settings()->setTouchDragDropEnabled(true);

  if (switches::IsTouchEditingEnabled())
    webview()->settings()->setTouchEditingEnabled(true);

  if (!params->frame_name.empty())
    webview()->mainFrame()->setName(params->frame_name);

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_created_with_opener)
    webview()->setOpenedByDOM();

  OnSetRendererPrefs(params->renderer_prefs);

#if defined(ENABLE_WEBRTC)
  if (!media_stream_dispatcher_)
    media_stream_dispatcher_ = new MediaStreamDispatcher(this);
#endif

  new MHTMLGenerator(this);
#if defined(OS_MACOSX)
  new TextInputClientObserver(this);
#endif  // defined(OS_MACOSX)

  // The next group of objects all implement RenderViewObserver, so are deleted
  // along with the RenderView automatically.
  devtools_agent_ = new DevToolsAgent(this);
  if (RenderWidgetCompositor* rwc = compositor()) {
    webview()->devToolsAgent()->setLayerTreeId(rwc->GetLayerTreeId());
  }
  mouse_lock_dispatcher_ = new RenderViewMouseLockDispatcher(this);

  history_controller_.reset(new HistoryController(this));

  // Create renderer_accessibility_ if needed.
  OnSetAccessibilityMode(params->accessibility_mode);

  new IdleUserDetector(this);

  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BINDINGS_POLICY_DOM_AUTOMATION;
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    enabled_bindings_ |= BINDINGS_POLICY_STATS_COLLECTION;

  ProcessViewLayoutFlags(command_line);

  GetContentClient()->renderer()->RenderViewCreated(this);

  // If we have an opener_id but we weren't created by a renderer, then
  // it's the browser asking us to set our opener to another RenderView.
  if (params->opener_id != MSG_ROUTING_NONE && !params->is_renderer_created) {
    RenderViewImpl* opener_view = FromRoutingID(params->opener_id);
    if (opener_view)
      webview()->mainFrame()->setOpener(opener_view->webview()->mainFrame());
  }

  // If we are initially swapped out, navigate to kSwappedOutURL.
  // This ensures we are in a unique origin that others cannot script.
  if (is_swapped_out_)
    NavigateToSwappedOutURL(webview()->mainFrame());
}

RenderViewImpl::~RenderViewImpl() {
  for (BitmapMap::iterator it = disambiguation_bitmaps_.begin();
       it != disambiguation_bitmaps_.end();
       ++it)
    delete it->second;
  history_page_ids_.clear();

  base::debug::TraceLog::GetInstance()->RemoveProcessLabel(routing_id_);

  // If file chooser is still waiting for answer, dispatch empty answer.
  while (!file_chooser_completions_.empty()) {
    if (file_chooser_completions_.front()->completion) {
      file_chooser_completions_.front()->completion->didChooseFile(
          WebVector<WebString>());
    }
    file_chooser_completions_.pop_front();
  }

#if defined(OS_ANDROID)
  // The date/time picker client is both a scoped_ptr member of this class and
  // a RenderViewObserver. Reset it to prevent double deletion.
  date_time_picker_client_.reset();
#endif

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap or RoutingIDViewMap.
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
  RoutingIDViewMap* routing_id_views = g_routing_id_view_map.Pointer();
  for (RoutingIDViewMap::iterator it = routing_id_views->begin();
       it != routing_id_views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, RenderViewGone());
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, OnDestruct());
}

/*static*/
RenderViewImpl* RenderViewImpl::FromWebView(WebView* webview) {
  ViewMap* views = g_view_map.Pointer();
  ViewMap::iterator it = views->find(webview);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromWebView(blink::WebView* webview) {
  return RenderViewImpl::FromWebView(webview);
}

/*static*/
RenderViewImpl* RenderViewImpl::FromRoutingID(int32 routing_id) {
  RoutingIDViewMap* views = g_routing_id_view_map.Pointer();
  RoutingIDViewMap::iterator it = views->find(routing_id);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromRoutingID(int routing_id) {
  return RenderViewImpl::FromRoutingID(routing_id);
}

/* static */
size_t RenderViewImpl::GetRenderViewCount() {
  return g_view_map.Get().size();
}

/*static*/
void RenderView::ForEach(RenderViewVisitor* visitor) {
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

/*static*/
RenderViewImpl* RenderViewImpl::Create(
    int32 opener_id,
    bool window_was_created_with_opener,
    const RendererPreferences& renderer_prefs,
    const WebPreferences& webkit_prefs,
    int32 routing_id,
    int32 main_frame_routing_id,
    int32 surface_id,
    int64 session_storage_namespace_id,
    const base::string16& frame_name,
    bool is_renderer_created,
    bool swapped_out,
    int32 proxy_routing_id,
    bool hidden,
    bool never_visible,
    int32 next_page_id,
    const blink::WebScreenInfo& screen_info,
    AccessibilityMode accessibility_mode) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  RenderViewImplParams params(opener_id,
                              window_was_created_with_opener,
                              renderer_prefs,
                              webkit_prefs,
                              routing_id,
                              main_frame_routing_id,
                              surface_id,
                              session_storage_namespace_id,
                              frame_name,
                              is_renderer_created,
                              swapped_out,
                              proxy_routing_id,
                              hidden,
                              never_visible,
                              next_page_id,
                              screen_info,
                              accessibility_mode);
  RenderViewImpl* render_view = NULL;
  if (g_create_render_view_impl)
    render_view = g_create_render_view_impl(&params);
  else
    render_view = new RenderViewImpl(&params);

  render_view->Initialize(&params);
  return render_view;
}

// static
void RenderViewImpl::InstallCreateHook(
    RenderViewImpl* (*create_render_view_impl)(RenderViewImplParams*)) {
  CHECK(!g_create_render_view_impl);
  g_create_render_view_impl = create_render_view_impl;
}

void RenderViewImpl::AddObserver(RenderViewObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderViewImpl::RemoveObserver(RenderViewObserver* observer) {
  observer->RenderViewGone();
  observers_.RemoveObserver(observer);
}

blink::WebView* RenderViewImpl::webview() const {
  return static_cast<blink::WebView*>(webwidget());
}

#if defined(ENABLE_PLUGINS)
void RenderViewImpl::PepperInstanceCreated(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.insert(instance);
}

void RenderViewImpl::PepperInstanceDeleted(
    PepperPluginInstanceImpl* instance) {
  active_pepper_instances_.erase(instance);

  if (pepper_last_mouse_event_target_ == instance)
    pepper_last_mouse_event_target_ = NULL;
  if (focused_pepper_plugin_ == instance)
    PepperFocusChanged(instance, false);
}

void RenderViewImpl::PepperFocusChanged(PepperPluginInstanceImpl* instance,
                                        bool focused) {
  if (focused)
    focused_pepper_plugin_ = instance;
  else if (focused_pepper_plugin_ == instance)
    focused_pepper_plugin_ = NULL;

  UpdateTextInputState(NO_SHOW_IME, FROM_NON_IME);
  UpdateSelectionBounds();
}

void RenderViewImpl::RegisterPluginDelegate(WebPluginDelegateProxy* delegate) {
  plugin_delegates_.insert(delegate);
  // If the renderer is visible, set initial visibility and focus state.
  if (!is_hidden()) {
#if defined(OS_MACOSX)
    delegate->SetContainerVisibility(true);
    if (webview() && webview()->isActive())
      delegate->SetWindowFocus(true);
#endif
  }
  // Plugins start assuming the content has focus (so that they work in
  // environments where RenderView isn't hosting them), so we always have to
  // set the initial state. See webplugin_delegate_impl.h for details.
  delegate->SetContentAreaFocus(has_focus());
}

void RenderViewImpl::UnregisterPluginDelegate(
    WebPluginDelegateProxy* delegate) {
  plugin_delegates_.erase(delegate);
}

#if defined(OS_WIN)
void RenderViewImpl::PluginFocusChanged(bool focused, int plugin_id) {
  if (focused)
    focused_plugin_id_ = plugin_id;
  else
    focused_plugin_id_ = -1;
}
#endif

#if defined(OS_MACOSX)
void RenderViewImpl::PluginFocusChanged(bool focused, int plugin_id) {
  Send(new ViewHostMsg_PluginFocusChanged(routing_id(), focused, plugin_id));
}

void RenderViewImpl::StartPluginIme() {
  IPC::Message* msg = new ViewHostMsg_StartPluginIme(routing_id());
  // This message can be sent during event-handling, and needs to be delivered
  // within that context.
  msg->set_unblock(true);
  Send(msg);
}
#endif  // defined(OS_MACOSX)

#endif  // ENABLE_PLUGINS

void RenderViewImpl::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  if (webview())
    webview()->transferActiveWheelFlingAnimation(params);
}

bool RenderViewImpl::HasIMETextFocus() {
  return GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
}

bool RenderViewImpl::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (main_frame)
    GetContentClient()->SetActiveURL(main_frame->document().url());

  ObserverListBase<RenderViewObserver>::Iterator it(observers_);
  RenderViewObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    if (observer->OnMessageReceived(message))
      return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewImpl, message)
    IPC_MESSAGE_HANDLER(InputMsg_ExecuteEditCommand, OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(InputMsg_MoveCaret, OnMoveCaret)
    IPC_MESSAGE_HANDLER(InputMsg_ScrollFocusedEditableNodeIntoRect,
                        OnScrollFocusedEditableNodeIntoRect)
    IPC_MESSAGE_HANDLER(InputMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(ViewMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyImageAt, OnCopyImageAt)
    IPC_MESSAGE_HANDLER(ViewMsg_SaveImageAt, OnSaveImageAt)
    IPC_MESSAGE_HANDLER(ViewMsg_Find, OnFind)
    IPC_MESSAGE_HANDLER(ViewMsg_StopFinding, OnStopFinding)
    IPC_MESSAGE_HANDLER(ViewMsg_Zoom, OnZoom)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForLoadingURL,
                        OnSetZoomLevelForLoadingURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForView,
                        OnSetZoomLevelForView)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageEncoding, OnSetPageEncoding)
    IPC_MESSAGE_HANDLER(ViewMsg_ResetPageEncodingToDefault,
                        OnResetPageEncodingToDefault)
    IPC_MESSAGE_HANDLER(ViewMsg_PostMessageEvent, OnPostMessageEvent)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(DragMsg_SourceEnded, OnDragSourceEnded)
    IPC_MESSAGE_HANDLER(DragMsg_SourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowBindings, OnAllowBindings)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_EnumerateDirectoryResponse,
                        OnEnumerateDirectoryResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_RunFileChooserResponse, OnFileChooserResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_SuppressDialogsUntilSwapOut,
                        OnSuppressDialogsUntilSwapOut)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_ThemeChanged, OnThemeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearFocusedElement, OnClearFocusedElement)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBackgroundOpaque, OnSetBackgroundOpaque)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableAutoResize, OnEnableAutoResize)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableAutoResize, OnDisableAutoResize)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableScrollbarsForSmallWindows,
                        OnDisableScrollbarsForSmallWindows)
    IPC_MESSAGE_HANDLER(ViewMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlayerActionAt, OnMediaPlayerActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginActionAt, OnPluginActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_SetActive, OnSetActive)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                        OnGetAllSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(
        ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
        OnGetSerializedHtmlDataForCurrentPageWithLocalLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_ShowContextMenu, OnShowContextMenu)
    // TODO(viettrungluu): Move to a separate message filter.
    IPC_MESSAGE_HANDLER(ViewMsg_SetHistoryLengthAndPrune,
                        OnSetHistoryLengthAndPrune)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableViewSourceMode, OnEnableViewSourceMode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAccessibilityMode, OnSetAccessibilityMode)
    IPC_MESSAGE_HANDLER(ViewMsg_DisownOpener, OnDisownOpener)
    IPC_MESSAGE_HANDLER(ViewMsg_ReleaseDisambiguationPopupBitmap,
                        OnReleaseDisambiguationPopupBitmap)
    IPC_MESSAGE_HANDLER(ViewMsg_WindowSnapshotCompleted,
                        OnWindowSnapshotCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectWordAroundCaret, OnSelectWordAroundCaret)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(InputMsg_ActivateNearestFindResult,
                        OnActivateNearestFindResult)
    IPC_MESSAGE_HANDLER(ViewMsg_FindMatchRects, OnFindMatchRects)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectPopupMenuItems, OnSelectPopupMenuItems)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTopControlsState,
                        OnUpdateTopControlsState)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtractSmartClipData, OnExtractSmartClipData)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginImeCompositionCompleted,
                        OnPluginImeCompositionCompleted)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectPopupMenuItem, OnSelectPopupMenuItem)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInLiveResize, OnSetInLiveResize)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWindowVisibility, OnSetWindowVisibility)
    IPC_MESSAGE_HANDLER(ViewMsg_WindowFrameChanged, OnWindowFrameChanged)
#endif
    // Adding a new message? Add platform independent ones first, then put the
    // platform specific ones at the end.

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderViewImpl::OnSelectWordAroundCaret() {
  if (!webview())
    return;

  handling_input_event_ = true;
  webview()->focusedFrame()->selectWordAroundCaret();
  handling_input_event_ = false;
}

bool RenderViewImpl::IsBackForwardToStaleEntry(
    const FrameMsg_Navigate_Params& params,
    bool is_reload) {
  // Make sure this isn't a back/forward to an entry we have already cropped
  // or replaced from our history, before the browser knew about it.  If so,
  // a new navigation has committed in the mean time, and we can ignore this.
  bool is_back_forward = !is_reload && params.page_state.IsValid();

  // Note: if the history_list_length_ is 0 for a back/forward, we must be
  // restoring from a previous session.  We'll update our state in OnNavigate.
  if (!is_back_forward || history_list_length_ <= 0)
    return false;

  DCHECK_EQ(static_cast<int>(history_page_ids_.size()), history_list_length_);

  // Check for whether the forward history has been cropped due to a recent
  // navigation the browser didn't know about.
  if (params.pending_history_list_offset >= history_list_length_)
    return true;

  // Check for whether this entry has been replaced with a new one.
  int expected_page_id =
      history_page_ids_[params.pending_history_list_offset];
  if (expected_page_id > 0 && params.page_id != expected_page_id) {
    if (params.page_id < expected_page_id)
      return true;

    // Otherwise we've removed an earlier entry and should have shifted all
    // entries left.  For now, it's ok to lazily update the list.
    // TODO(creis): Notify all live renderers when we remove entries from
    // the front of the list, so that we don't hit this case.
    history_page_ids_[params.pending_history_list_offset] = params.page_id;
  }

  return false;
}

// Stop loading the current page.
void RenderViewImpl::OnStop() {
  if (webview())
    webview()->mainFrame()->stopLoading();
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, OnStop());
  main_render_frame_->OnStop();
}

void RenderViewImpl::OnCopyImageAt(int x, int y) {
  webview()->copyImageAt(WebPoint(x, y));
}

void RenderViewImpl::OnSaveImageAt(int x, int y) {
  webview()->saveImageAt(WebPoint(x, y));
}

void RenderViewImpl::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING) {
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_,
                                         pending_target_url_));
  }

  target_url_status_ = TARGET_NONE;
}

void RenderViewImpl::OnExecuteEditCommand(const std::string& name,
    const std::string& value) {
  if (!webview() || !webview()->focusedFrame())
    return;

  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8(name), WebString::fromUTF8(value));
}

void RenderViewImpl::OnMoveCaret(const gfx::Point& point) {
  if (!webview())
    return;

  Send(new ViewHostMsg_MoveCaret_ACK(routing_id_));

  webview()->focusedFrame()->moveCaretSelection(point);
}

void RenderViewImpl::OnScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  if (has_scrolled_focused_editable_node_into_rect_ &&
      rect == rect_for_scrolled_focused_editable_node_) {
    return;
  }

  blink::WebElement element = GetFocusedElement();
  if (!element.isNull() && IsEditableNode(element)) {
    rect_for_scrolled_focused_editable_node_ = rect;
    has_scrolled_focused_editable_node_into_rect_ = true;
    webview()->scrollFocusedNodeIntoRect(rect);
  }
}

void RenderViewImpl::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderViewImpl::OnSetHistoryLengthAndPrune(int history_length,
                                                int32 minimum_page_id) {
  DCHECK_GE(history_length, 0);
  DCHECK(history_list_offset_ == history_list_length_ - 1);
  DCHECK_GE(minimum_page_id, -1);

  // Generate the new list.
  std::vector<int32> new_history_page_ids(history_length, -1);
  for (size_t i = 0; i < history_page_ids_.size(); ++i) {
    if (minimum_page_id >= 0 && history_page_ids_[i] < minimum_page_id)
      continue;
    new_history_page_ids.push_back(history_page_ids_[i]);
  }
  new_history_page_ids.swap(history_page_ids_);

  // Update indexes.
  history_list_length_ = history_page_ids_.size();
  history_list_offset_ = history_list_length_ - 1;
}


void RenderViewImpl::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->setInitialFocus(reverse);
}

#if defined(OS_MACOSX)
void RenderViewImpl::OnSetInLiveResize(bool in_live_resize) {
  if (!webview())
    return;
  if (in_live_resize)
    webview()->willStartLiveResize();
  else
    webview()->willEndLiveResize();
}
#endif

///////////////////////////////////////////////////////////////////////////////

// Sends the current history state to the browser so it will be saved before we
// navigate to a new page.
void RenderViewImpl::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;
  SendUpdateState(history_controller_->GetCurrentEntry());
}

void RenderViewImpl::SendUpdateState(HistoryEntry* entry) {
  if (!entry)
    return;

  // Don't send state updates for kSwappedOutURL.
  if (entry->root().urlString() == WebString::fromUTF8(kSwappedOutURL))
    return;

  Send(new ViewHostMsg_UpdateState(
      routing_id_, page_id_, HistoryEntryToPageState(entry)));
}

bool RenderViewImpl::SendAndRunNestedMessageLoop(IPC::SyncMessage* message) {
  // Before WebKit asks us to show an alert (etc.), it takes care of doing the
  // equivalent of WebView::willEnterModalLoop.  In the case of showModalDialog
  // it is particularly important that we do not call willEnterModalLoop as
  // that would defer resource loads for the dialog itself.
  if (RenderThreadImpl::current())  // Will be NULL during unit tests.
    RenderThreadImpl::current()->DoNotNotifyWebKitOfModalLoop();

  message->EnableMessagePumping();  // Runs a nested message loop.
  return Send(message);
}

void RenderViewImpl::GetWindowSnapshot(const WindowSnapshotCallback& callback) {
  int id = next_snapshot_id_++;
  pending_snapshots_.insert(std::make_pair(id, callback));
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT,
                                0,
                                id);
  scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  if (RenderWidgetCompositor* rwc = compositor()) {
    latency_info_swap_promise_monitor =
        rwc->CreateLatencyInfoSwapPromiseMonitor(&latency_info).Pass();
  }
  ScheduleCompositeWithForcedRedraw();
}

void RenderViewImpl::OnWindowSnapshotCompleted(const int snapshot_id,
    const gfx::Size& size, const std::vector<unsigned char>& png) {

  // Any pending snapshots with a lower ID than the one received are considered
  // to be implicitly complete, and returned the same snapshot data.
  PendingSnapshotMap::iterator it = pending_snapshots_.begin();
  while(it != pending_snapshots_.end()) {
      if (it->first <= snapshot_id) {
        it->second.Run(size, png);
        pending_snapshots_.erase(it++);
      } else {
        ++it;
      }
  }
}

// blink::WebViewClient ------------------------------------------------------

WebView* RenderViewImpl::createView(WebLocalFrame* creator,
                                    const WebURLRequest& request,
                                    const WebWindowFeatures& features,
                                    const WebString& frame_name,
                                    WebNavigationPolicy policy,
                                    bool suppress_opener) {
  ViewHostMsg_CreateWindow_Params params;
  params.opener_id = routing_id_;
  params.user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  if (GetContentClient()->renderer()->AllowPopup())
    params.user_gesture = true;
  params.window_container_type = WindowFeaturesToContainerType(features);
  params.session_storage_namespace_id = session_storage_namespace_id_;
  if (frame_name != "_blank")
    params.frame_name = frame_name;
  params.opener_render_frame_id =
      RenderFrameImpl::FromWebFrame(creator)->GetRoutingID();
  params.opener_url = creator->document().url();
  params.opener_top_level_frame_url = creator->top()->document().url();
  GURL security_url(creator->document().securityOrigin().toString());
  if (!security_url.is_valid())
    security_url = GURL();
  params.opener_security_origin = security_url;
  params.opener_suppressed = suppress_opener;
  params.disposition = NavigationPolicyToDisposition(policy);
  if (!request.isNull()) {
    params.target_url = request.url();
    params.referrer = GetReferrerFromRequest(creator, request);
  }
  params.features = features;

  for (size_t i = 0; i < features.additionalFeatures.size(); ++i)
    params.additional_features.push_back(features.additionalFeatures[i]);

  int32 routing_id = MSG_ROUTING_NONE;
  int32 main_frame_routing_id = MSG_ROUTING_NONE;
  int32 surface_id = 0;
  int64 cloned_session_storage_namespace_id;

  RenderThread::Get()->Send(
      new ViewHostMsg_CreateWindow(params,
                                   &routing_id,
                                   &main_frame_routing_id,
                                   &surface_id,
                                   &cloned_session_storage_namespace_id));
  if (routing_id == MSG_ROUTING_NONE)
    return NULL;

  WebUserGestureIndicator::consumeUserGesture();

  // While this view may be a background extension page, it can spawn a visible
  // render view. So we just assume that the new one is not another background
  // page instead of passing on our own value.
  // TODO(vangelis): Can we tell if the new view will be a background page?
  bool never_visible = false;

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  RenderViewImpl* view = RenderViewImpl::Create(
      routing_id_,
      true,  // window_was_created_with_opener
      renderer_preferences_,
      webkit_preferences_,
      routing_id,
      main_frame_routing_id,
      surface_id,
      cloned_session_storage_namespace_id,
      base::string16(),  // WebCore will take care of setting the correct name.
      true,              // is_renderer_created
      false,             // swapped_out
      MSG_ROUTING_NONE,  // proxy_routing_id
      params.disposition == NEW_BACKGROUND_TAB,  // hidden
      never_visible,
      1,  // next_page_id
      screen_info_,
      accessibility_mode_);
  view->opened_by_user_gesture_ = params.user_gesture;

  // Record whether the creator frame is trying to suppress the opener field.
  view->opener_suppressed_ = params.opener_suppressed;

  return view->webview();
}

WebWidget* RenderViewImpl::createPopupMenu(blink::WebPopupType popup_type) {
  RenderWidget* widget =
      RenderWidget::Create(routing_id_, popup_type, screen_info_);
  if (!widget)
    return NULL;
  if (screen_metrics_emulator_) {
    widget->SetPopupOriginAdjustmentsForEmulation(
        screen_metrics_emulator_.get());
  }
  return widget->webwidget();
}

WebExternalPopupMenu* RenderViewImpl::createExternalPopupMenu(
    const WebPopupMenuInfo& popup_menu_info,
    WebExternalPopupMenuClient* popup_menu_client) {
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // An IPC message is sent to the browser to build and display the actual
  // popup.  The user could have time to click a different select by the time
  // the popup is shown.  In that case external_popup_menu_ is non NULL.
  // By returning NULL in that case, we instruct WebKit to cancel that new
  // popup.  So from the user perspective, only the first one will show, and
  // will have to close the first one before another one can be shown.
  if (external_popup_menu_)
    return NULL;
  external_popup_menu_.reset(
      new ExternalPopupMenu(this, popup_menu_info, popup_menu_client));
  if (screen_metrics_emulator_) {
    SetExternalPopupOriginAdjustmentsForEmulation(
        external_popup_menu_.get(), screen_metrics_emulator_.get());
  }
  return external_popup_menu_.get();
#else
  return NULL;
#endif
}

WebStorageNamespace* RenderViewImpl::createSessionStorageNamespace() {
  CHECK(session_storage_namespace_id_ != kInvalidSessionStorageNamespaceId);
  return new WebStorageNamespaceImpl(session_storage_namespace_id_);
}

void RenderViewImpl::printPage(WebLocalFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    PrintPage(frame, handling_input_event_));
}

bool RenderViewImpl::enumerateChosenDirectory(
    const WebString& path,
    WebFileChooserCompletion* chooser_completion) {
  int id = enumeration_completion_id_++;
  enumeration_completions_[id] = chooser_completion;
  return Send(new ViewHostMsg_EnumerateDirectory(
      routing_id_,
      id,
      base::FilePath::FromUTF16Unsafe(path)));
}

void RenderViewImpl::FrameDidStartLoading(WebFrame* frame) {
  DCHECK_GE(frames_in_progress_, 0);
  if (frames_in_progress_ == 0)
    FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidStartLoading());
  frames_in_progress_++;
}

void RenderViewImpl::FrameDidStopLoading(WebFrame* frame) {
  // TODO(japhet): This should be a DCHECK, but the pdf plugin sometimes
  // calls DidStopLoading() without a matching DidStartLoading().
  if (frames_in_progress_ == 0)
    return;
  frames_in_progress_--;
  if (frames_in_progress_ == 0) {
    DidStopLoadingIcons();
    FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidStopLoading());
  }
}

void RenderViewImpl::didCancelCompositionOnSelectionChange() {
  Send(new ViewHostMsg_ImeCancelComposition(routing_id()));
}

bool RenderViewImpl::handleCurrentKeyboardEvent() {
  if (edit_commands_.empty())
    return false;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame)
    return false;

  EditCommands::iterator it = edit_commands_.begin();
  EditCommands::iterator end = edit_commands_.end();

  bool did_execute_command = false;
  for (; it != end; ++it) {
    // In gtk and cocoa, it's possible to bind multiple edit commands to one
    // key (but it's the exception). Once one edit command is not executed, it
    // seems safest to not execute the rest.
    if (!frame->executeCommand(WebString::fromUTF8(it->name),
                               WebString::fromUTF8(it->value),
                               GetFocusedElement()))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

bool RenderViewImpl::runFileChooser(
    const blink::WebFileChooserParams& params,
    WebFileChooserCompletion* chooser_completion) {
  // Do not open the file dialog in a hidden RenderView.
  if (is_hidden())
    return false;
  FileChooserParams ipc_params;
  if (params.directory)
    ipc_params.mode = FileChooserParams::UploadFolder;
  else if (params.multiSelect)
    ipc_params.mode = FileChooserParams::OpenMultiple;
  else if (params.saveAs)
    ipc_params.mode = FileChooserParams::Save;
  else
    ipc_params.mode = FileChooserParams::Open;
  ipc_params.title = params.title;
  ipc_params.default_file_name =
      base::FilePath::FromUTF16Unsafe(params.initialValue);
  ipc_params.accept_types.reserve(params.acceptTypes.size());
  for (size_t i = 0; i < params.acceptTypes.size(); ++i)
    ipc_params.accept_types.push_back(params.acceptTypes[i]);
#if defined(OS_ANDROID)
  ipc_params.capture = params.useMediaCapture;
#endif

  return ScheduleFileChooser(ipc_params, chooser_completion);
}

void RenderViewImpl::showValidationMessage(
    const blink::WebRect& anchor_in_root_view,
    const blink::WebString& main_text,
    const blink::WebString& sub_text,
    blink::WebTextDirection hint) {
  base::string16 wrapped_main_text = main_text;
  base::string16 wrapped_sub_text = sub_text;
  if (hint == blink::WebTextDirectionLeftToRight) {
    wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(wrapped_main_text);
    if (!wrapped_sub_text.empty()) {
      wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(wrapped_sub_text);
    }
  } else if (hint == blink::WebTextDirectionRightToLeft
             && !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(&wrapped_main_text);
    if (!wrapped_sub_text.empty()) {
      base::i18n::WrapStringWithRTLFormatting(&wrapped_sub_text);
    }
  }
  Send(new ViewHostMsg_ShowValidationMessage(
    routing_id(), anchor_in_root_view, wrapped_main_text, wrapped_sub_text));
}

void RenderViewImpl::hideValidationMessage() {
  Send(new ViewHostMsg_HideValidationMessage(routing_id()));
}

void RenderViewImpl::moveValidationMessage(
    const blink::WebRect& anchor_in_root_view) {
  Send(new ViewHostMsg_MoveValidationMessage(routing_id(),
                                             anchor_in_root_view));
}

void RenderViewImpl::setStatusText(const WebString& text) {
}

void RenderViewImpl::UpdateTargetURL(const GURL& url,
                                     const GURL& fallback_url) {
  GURL latest_url = url.is_empty() ? fallback_url : url;
  if (latest_url == target_url_)
    return;

  // Tell the browser to display a destination link.
  if (target_url_status_ == TARGET_INFLIGHT ||
      target_url_status_ == TARGET_PENDING) {
    // If we have a request in-flight, save the URL to be sent when we
    // receive an ACK to the in-flight request. We can happily overwrite
    // any existing pending sends.
    pending_target_url_ = latest_url;
    target_url_status_ = TARGET_PENDING;
  } else {
    // URLs larger than |MaxURLChars()| cannot be sent through IPC -
    // see |ParamTraits<GURL>|.
    if (latest_url.possibly_invalid_spec().size() > GetMaxURLChars())
      latest_url = GURL();
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_, latest_url));
    target_url_ = latest_url;
    target_url_status_ = TARGET_INFLIGHT;
  }
}

gfx::RectF RenderViewImpl::ClientRectToPhysicalWindowRect(
    const gfx::RectF& rect) const {
  gfx::RectF window_rect = rect;
  window_rect.Scale(device_scale_factor_ * webview()->pageScaleFactor());
  return window_rect;
}

void RenderViewImpl::StartNavStateSyncTimerIfNecessary() {
  // No need to update state if no page has committed yet.
  if (page_id_ == -1)
    return;

  int delay;
  if (send_content_state_immediately_)
    delay = 0;
  else if (is_hidden())
    delay = kDelaySecondsForContentStateSyncHidden;
  else
    delay = kDelaySecondsForContentStateSync;

  if (nav_state_sync_timer_.IsRunning()) {
    // The timer is already running. If the delay of the timer maches the amount
    // we want to delay by, then return. Otherwise stop the timer so that it
    // gets started with the right delay.
    if (nav_state_sync_timer_.GetCurrentDelay().InSeconds() == delay)
      return;
    nav_state_sync_timer_.Stop();
  }

  nav_state_sync_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(delay), this,
                              &RenderViewImpl::SyncNavigationState);
}

void RenderViewImpl::setMouseOverURL(const WebURL& url) {
  mouse_over_url_ = GURL(url);
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void RenderViewImpl::setKeyboardFocusURL(const WebURL& url) {
  focus_url_ = GURL(url);
  UpdateTargetURL(focus_url_, mouse_over_url_);
}

void RenderViewImpl::startDragging(WebLocalFrame* frame,
                                   const WebDragData& data,
                                   WebDragOperationsMask mask,
                                   const WebImage& image,
                                   const WebPoint& webImageOffset) {
  DropData drop_data(DropDataBuilder::Build(data));
  drop_data.referrer_policy = frame->document().referrerPolicy();
  gfx::Vector2d imageOffset(webImageOffset.x, webImageOffset.y);
  Send(new DragHostMsg_StartDragging(routing_id_,
                                     drop_data,
                                     mask,
                                     image.getSkBitmap(),
                                     imageOffset,
                                     possible_drag_event_info_));
}

bool RenderViewImpl::acceptsLoadDrops() {
  return renderer_preferences_.can_accept_load_drops;
}

void RenderViewImpl::focusNext() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, false));
}

void RenderViewImpl::focusPrevious() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, true));
}

void RenderViewImpl::focusedNodeChanged(const WebNode& node) {
  has_scrolled_focused_editable_node_into_rect_ = false;

  Send(new ViewHostMsg_FocusedNodeChanged(routing_id_, IsEditableNode(node)));

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FocusedNodeChanged(node));
}

void RenderViewImpl::didUpdateLayout() {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidUpdateLayout());

  // We don't always want to set up a timer, only if we've been put in that
  // mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  if (check_preferred_size_timer_.IsRunning())
    return;
  check_preferred_size_timer_.Start(FROM_HERE,
                                    TimeDelta::FromMilliseconds(0), this,
                                    &RenderViewImpl::CheckPreferredSize);
}

void RenderViewImpl::navigateBackForwardSoon(int offset) {
  Send(new ViewHostMsg_GoToEntryAtOffset(routing_id_, offset));
}

int RenderViewImpl::historyBackListCount() {
  return history_list_offset_ < 0 ? 0 : history_list_offset_;
}

int RenderViewImpl::historyForwardListCount() {
  return history_list_length_ - historyBackListCount() - 1;
}

void RenderViewImpl::postAccessibilityEvent(
    const WebAXObject& obj, blink::WebAXEvent event) {
  if (renderer_accessibility_) {
    renderer_accessibility_->HandleWebAccessibilityEvent(obj, event);
  }
}

void RenderViewImpl::didUpdateInspectorSetting(const WebString& key,
                                           const WebString& value) {
  Send(new ViewHostMsg_UpdateInspectorSetting(routing_id_,
                                              key.utf8(),
                                              value.utf8()));
}

// blink::WebWidgetClient ----------------------------------------------------

void RenderViewImpl::didFocus() {
  // TODO(jcivelli): when https://bugs.webkit.org/show_bug.cgi?id=33389 is fixed
  //                 we won't have to test for user gesture anymore and we can
  //                 move that code back to render_widget.cc
  if (WebUserGestureIndicator::isProcessingUserGesture() &&
      !RenderThreadImpl::current()->layout_test_mode()) {
    Send(new ViewHostMsg_Focus(routing_id_));
  }
}

void RenderViewImpl::didBlur() {
  // TODO(jcivelli): see TODO above in didFocus().
  if (WebUserGestureIndicator::isProcessingUserGesture() &&
      !RenderThreadImpl::current()->layout_test_mode()) {
    Send(new ViewHostMsg_Blur(routing_id_));
  }
}

// We are supposed to get a single call to Show for a newly created RenderView
// that was created via RenderViewImpl::CreateWebView.  So, we wait until this
// point to dispatch the ShowView message.
//
// This method provides us with the information about how to display the newly
// created RenderView (i.e., as a blocked popup or as a new tab).
//
void RenderViewImpl::show(WebNavigationPolicy policy) {
  if (did_show_) {
    // When supports_multiple_windows is disabled, popups are reusing
    // the same view. In some scenarios, this makes WebKit to call show() twice.
    if (webkit_preferences_.supports_multiple_windows)
      NOTREACHED() << "received extraneous Show call";
    return;
  }
  did_show_ = true;

  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  // Force new windows to a popup if they were not opened with a user gesture.
  if (!opened_by_user_gesture_) {
    // We exempt background tabs for compat with older versions of Chrome.
    // TODO(darin): This seems bogus.  These should have a user gesture, so
    // we probably don't need this check.
    if (policy != blink::WebNavigationPolicyNewBackgroundTab)
      policy = blink::WebNavigationPolicyNewPopup;
  }

  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if disposition is not NEW_POPUP, or the
  // browser process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowView(opener_id_, routing_id_,
      NavigationPolicyToDisposition(policy), initial_pos_,
      opened_by_user_gesture_));
  SetPendingWindowRect(initial_pos_);
}

void RenderViewImpl::runModal() {
  DCHECK(did_show_) << "should already have shown the view";

  // Don't allow further dialogs if we are waiting to swap out, since the
  // PageGroupLoadDeferrer in our stack prevents it.
  if (suppress_dialogs_until_swap_out_)
    return;

  // We must keep WebKit's shared timer running in this case in order to allow
  // showModalDialog to function properly.
  //
  // TODO(darin): WebKit should really be smarter about suppressing events and
  // timers so that we do not need to manage the shared timer in such a heavy
  // handed manner.
  //
  if (RenderThreadImpl::current())  // Will be NULL during unit tests.
    RenderThreadImpl::current()->DoNotSuspendWebKitSharedTimer();

  SendAndRunNestedMessageLoop(new ViewHostMsg_RunModal(
      routing_id_, opener_id_));
}

bool RenderViewImpl::enterFullScreen() {
  Send(new ViewHostMsg_ToggleFullscreen(routing_id_, true));
  return true;
}

void RenderViewImpl::exitFullScreen() {
  Send(new ViewHostMsg_ToggleFullscreen(routing_id_, false));
}

bool RenderViewImpl::requestPointerLock() {
  return mouse_lock_dispatcher_->LockMouse(webwidget_mouse_lock_target_.get());
}

void RenderViewImpl::requestPointerUnlock() {
  mouse_lock_dispatcher_->UnlockMouse(webwidget_mouse_lock_target_.get());
}

bool RenderViewImpl::isPointerLocked() {
  return mouse_lock_dispatcher_->IsMouseLockedTo(
      webwidget_mouse_lock_target_.get());
}

void RenderViewImpl::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
  RenderWidget::didHandleGestureEvent(event, event_cancelled);

  if (event.type != blink::WebGestureEvent::GestureTap)
    return;

  blink::WebTextInputType text_input_type =
      GetWebView()->textInputInfo().type;

  Send(new ViewHostMsg_FocusedNodeTouched(
      routing_id(), text_input_type != blink::WebTextInputTypeNone));
}

void RenderViewImpl::initializeLayerTreeView() {
  RenderWidget::initializeLayerTreeView();
  RenderWidgetCompositor* rwc = compositor();
  if (!rwc)
    return;
  if (webview() && webview()->devToolsAgent())
    webview()->devToolsAgent()->setLayerTreeId(rwc->GetLayerTreeId());

#if !defined(OS_MACOSX)  // many events are unhandled - http://crbug.com/138003
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // render_thread may be NULL in tests.
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager) {
    input_handler_manager->AddInputHandler(
        routing_id_, rwc->GetInputHandler(), AsWeakPtr());
  }
#endif
}

// blink::WebFrameClient -----------------------------------------------------

void RenderViewImpl::Repaint(const gfx::Size& size) {
  OnRepaint(size);
}

void RenderViewImpl::SetEditCommandForNextKeyEvent(const std::string& name,
                                                   const std::string& value) {
  EditCommands edit_commands;
  edit_commands.push_back(EditCommand(name, value));
  OnSetEditCommandsForNextKeyEvent(edit_commands);
}

void RenderViewImpl::ClearEditCommands() {
  edit_commands_.clear();
}

SSLStatus RenderViewImpl::GetSSLStatusOfFrame(blink::WebFrame* frame) const {
  std::string security_info;
  if (frame && frame->dataSource())
    security_info = frame->dataSource()->response().securityInfo();

  SSLStatus ssl_status;
  DeserializeSecurityInfo(security_info,
                          &ssl_status.cert_id,
                          &ssl_status.cert_status,
                          &ssl_status.security_bits,
                          &ssl_status.connection_status,
                          &ssl_status.signed_certificate_timestamp_ids);
  return ssl_status;
}

const std::string& RenderViewImpl::GetAcceptLanguages() const {
  return renderer_preferences_.accept_languages;
}

void RenderViewImpl::didCreateDataSource(WebLocalFrame* frame,
                                         WebDataSource* ds) {
  bool content_initiated = !pending_navigation_params_.get();

  // Make sure any previous redirect URLs end up in our new data source.
  if (pending_navigation_params_.get()) {
    for (std::vector<GURL>::const_iterator i =
             pending_navigation_params_->redirects.begin();
         i != pending_navigation_params_->redirects.end(); ++i) {
      ds->appendRedirect(*i);
    }
  }

  DocumentState* document_state = DocumentState::FromDataSource(ds);
  if (!document_state) {
    document_state = new DocumentState;
    ds->setExtraData(document_state);
    if (!content_initiated)
      PopulateDocumentStateFromPending(document_state);
  }

  // Carry over the user agent override flag, if it exists.
  if (content_initiated && webview() && webview()->mainFrame() &&
      webview()->mainFrame()->dataSource()) {
    DocumentState* old_document_state =
        DocumentState::FromDataSource(webview()->mainFrame()->dataSource());
    if (old_document_state) {
      InternalDocumentStateData* internal_data =
          InternalDocumentStateData::FromDocumentState(document_state);
      InternalDocumentStateData* old_internal_data =
          InternalDocumentStateData::FromDocumentState(old_document_state);
      internal_data->set_is_overriding_user_agent(
          old_internal_data->is_overriding_user_agent());
    }
  }

  // The rest of RenderView assumes that a WebDataSource will always have a
  // non-null NavigationState.
  if (content_initiated) {
    document_state->set_navigation_state(
        NavigationState::CreateContentInitiated());
  } else {
    document_state->set_navigation_state(CreateNavigationStateFromPending());
    pending_navigation_params_.reset();
  }

  // DocumentState::referred_by_prefetcher_ is true if we are
  // navigating from a page that used prefetching using a link on that
  // page.  We are early enough in the request process here that we
  // can still see the DocumentState of the previous page and set
  // this value appropriately.
  // TODO(gavinp): catch the important case of navigation in a new
  // renderer process.
  if (webview()) {
    if (WebFrame* old_frame = webview()->mainFrame()) {
      const WebURLRequest& original_request = ds->originalRequest();
      const GURL referrer(
          original_request.httpHeaderField(WebString::fromUTF8("Referer")));
      if (!referrer.is_empty() &&
          DocumentState::FromDataSource(
              old_frame->dataSource())->was_prefetcher()) {
        for (; old_frame; old_frame = old_frame->traverseNext(false)) {
          WebDataSource* old_frame_ds = old_frame->dataSource();
          if (old_frame_ds && referrer == GURL(old_frame_ds->request().url())) {
            document_state->set_was_referred_by_prefetcher(true);
            break;
          }
        }
      }
    }
  }

  if (content_initiated) {
    const WebURLRequest& request = ds->request();
    switch (request.cachePolicy()) {
      case WebURLRequest::UseProtocolCachePolicy:  // normal load.
        document_state->set_load_type(DocumentState::LINK_LOAD_NORMAL);
        break;
      case WebURLRequest::ReloadIgnoringCacheData:  // reload.
      case WebURLRequest::ReloadBypassingCache:  // end-to-end reload.
        document_state->set_load_type(DocumentState::LINK_LOAD_RELOAD);
        break;
      case WebURLRequest::ReturnCacheDataElseLoad:  // allow stale data.
        document_state->set_load_type(
            DocumentState::LINK_LOAD_CACHE_STALE_OK);
        break;
      case WebURLRequest::ReturnCacheDataDontLoad:  // Don't re-post.
        document_state->set_load_type(DocumentState::LINK_LOAD_CACHE_ONLY);
        break;
      default:
        NOTREACHED();
    }
  }

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidCreateDataSource(frame, ds));
}

void RenderViewImpl::PopulateDocumentStateFromPending(
    DocumentState* document_state) {
  const FrameMsg_Navigate_Params& params = *pending_navigation_params_.get();
  document_state->set_request_time(params.request_time);

  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);

  if (!params.url.SchemeIs(url::kJavaScriptScheme) &&
      params.navigation_type == FrameMsg_Navigate_Type::RESTORE) {
    // We're doing a load of a page that was restored from the last session. By
    // default this prefers the cache over loading (LOAD_PREFERRING_CACHE) which
    // can result in stale data for pages that are set to expire. We explicitly
    // override that by setting the policy here so that as necessary we load
    // from the network.
    //
    // TODO(davidben): Remove this in favor of passing a cache policy to the
    // loadHistoryItem call in OnNavigate. That requires not overloading
    // UseProtocolCachePolicy to mean both "normal load" and "determine cache
    // policy based on load type, etc".
    internal_data->set_cache_policy_override(
        WebURLRequest::UseProtocolCachePolicy);
  }

  if (IsReload(params))
    document_state->set_load_type(DocumentState::RELOAD);
  else if (params.page_state.IsValid())
    document_state->set_load_type(DocumentState::HISTORY_LOAD);
  else
    document_state->set_load_type(DocumentState::NORMAL_LOAD);

  internal_data->set_is_overriding_user_agent(params.is_overriding_user_agent);
  internal_data->set_must_reset_scroll_and_scale_state(
      params.navigation_type ==
          FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL);
  document_state->set_can_load_local_resources(params.can_load_local_resources);
}

NavigationState* RenderViewImpl::CreateNavigationStateFromPending() {
  const FrameMsg_Navigate_Params& params = *pending_navigation_params_.get();
  NavigationState* navigation_state = NULL;

  // A navigation resulting from loading a javascript URL should not be treated
  // as a browser initiated event.  Instead, we want it to look as if the page
  // initiated any load resulting from JS execution.
  if (!params.url.SchemeIs(url::kJavaScriptScheme)) {
    navigation_state = NavigationState::CreateBrowserInitiated(
        params.page_id,
        params.pending_history_list_offset,
        params.should_clear_history_list,
        params.transition);
    navigation_state->set_should_replace_current_entry(
        params.should_replace_current_entry);
    navigation_state->set_transferred_request_child_id(
        params.transferred_request_child_id);
    navigation_state->set_transferred_request_request_id(
        params.transferred_request_request_id);
    navigation_state->set_allow_download(params.allow_download);
    navigation_state->set_extra_headers(params.extra_headers);
  } else {
    navigation_state = NavigationState::CreateContentInitiated();
  }
  return navigation_state;
}

void RenderViewImpl::ProcessViewLayoutFlags(const CommandLine& command_line) {
  bool enable_viewport =
      command_line.HasSwitch(switches::kEnableViewport) ||
      command_line.HasSwitch(switches::kEnableViewportMeta);

  // If viewport tag is enabled, then the WebKit side will take care
  // of setting the fixed layout size and page scale limits.
  if (enable_viewport)
    return;

  // When navigating to a new page, reset the page scale factor to be 1.0.
  webview()->setInitialPageScaleOverride(1.f);

  float maxPageScaleFactor =
      command_line.HasSwitch(switches::kEnablePinch) ? 4.f : 1.f ;
  webview()->setPageScaleFactorLimits(1, maxPageScaleFactor);
}

void RenderViewImpl::didClearWindowObject(WebLocalFrame* frame) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidClearWindowObject(frame));

  if (enabled_bindings_& BINDINGS_POLICY_WEB_UI)
    WebUIExtension::Install(frame);

  if (enabled_bindings_ & BINDINGS_POLICY_STATS_COLLECTION)
    StatsCollectionController::Install(frame);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kEnableSkiaBenchmarking))
    SkiaBenchmarking::Install(frame);

  if (command_line.HasSwitch(switches::kEnableMemoryBenchmarking))
    MemoryBenchmarkingExtension::Install(frame);
}

void RenderViewImpl::didChangeIcon(WebLocalFrame* frame,
                                   WebIconURL::Type icon_type) {
  if (frame->parent())
    return;

  if (!TouchEnabled() && icon_type != WebIconURL::TypeFavicon)
    return;

  WebVector<WebIconURL> icon_urls = frame->iconURLs(icon_type);
  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    std::vector<gfx::Size> sizes;
    ConvertToFaviconSizes(icon_urls[i].sizes(), &sizes);
    urls.push_back(FaviconURL(
        icon_urls[i].iconURL(), ToFaviconType(icon_urls[i].iconType()), sizes));
  }
  SendUpdateFaviconURL(urls);
}

void RenderViewImpl::didUpdateCurrentHistoryItem(WebLocalFrame* frame) {
  StartNavStateSyncTimerIfNecessary();
}

void RenderViewImpl::CheckPreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  gfx::Size size = webview()->contentsPreferredMinimumSize();

  // In the presence of zoom, these sizes are still reported as if unzoomed,
  // so we need to adjust.
  double zoom_factor = ZoomLevelToZoomFactor(webview()->zoomLevel());
  size.set_width(static_cast<int>(size.width() * zoom_factor));
  size.set_height(static_cast<int>(size.height() * zoom_factor));

  if (size == preferred_size_)
    return;

  preferred_size_ = size;
  Send(new ViewHostMsg_DidContentsPreferredSizeChange(routing_id_,
                                                      preferred_size_));
}

BrowserPluginManager* RenderViewImpl::GetBrowserPluginManager() {
  if (!browser_plugin_manager_.get())
    browser_plugin_manager_ = BrowserPluginManager::Create(this);
  return browser_plugin_manager_.get();
}

void RenderViewImpl::UpdateScrollState(WebFrame* frame) {
  Send(new ViewHostMsg_DidChangeScrollOffset(routing_id_));
}

void RenderViewImpl::didChangeScrollOffset(WebLocalFrame* frame) {
  StartNavStateSyncTimerIfNecessary();

  if (webview()->mainFrame() == frame)
    UpdateScrollState(frame);

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidChangeScrollOffset(frame));
}

void RenderViewImpl::SendFindReply(int request_id,
                                   int match_count,
                                   int ordinal,
                                   const WebRect& selection_rect,
                                   bool final_status_update) {
  Send(new ViewHostMsg_Find_Reply(routing_id_,
                                  request_id,
                                  match_count,
                                  selection_rect,
                                  ordinal,
                                  final_status_update));
}

blink::WebString RenderViewImpl::acceptLanguages() {
  return WebString::fromUTF8(renderer_preferences_.accept_languages);
}

// blink::WebPageSerializerClient implementation ------------------------------

void RenderViewImpl::didSerializeDataForFrame(
    const WebURL& frame_url,
    const WebCString& data,
    WebPageSerializerClient::PageSerializationStatus status) {
  Send(new ViewHostMsg_SendSerializedHtmlData(
    routing_id(),
    frame_url,
    data.data(),
    static_cast<int32>(status)));
}

// RenderView implementation ---------------------------------------------------

bool RenderViewImpl::Send(IPC::Message* message) {
  return RenderWidget::Send(message);
}

RenderFrame* RenderViewImpl::GetMainRenderFrame() {
  return main_render_frame_.get();
}

int RenderViewImpl::GetRoutingID() const {
  return routing_id_;
}

int RenderViewImpl::GetPageId() const {
  return page_id_;
}

gfx::Size RenderViewImpl::GetSize() const {
  return size();
}

WebPreferences& RenderViewImpl::GetWebkitPreferences() {
  return webkit_preferences_;
}

void RenderViewImpl::SetWebkitPreferences(const WebPreferences& preferences) {
  OnUpdateWebPreferences(preferences);
}

blink::WebView* RenderViewImpl::GetWebView() {
  return webview();
}

blink::WebElement RenderViewImpl::GetFocusedElement() const {
  if (!webview())
    return WebElement();
  WebFrame* focused_frame = webview()->focusedFrame();
  if (focused_frame) {
    WebDocument doc = focused_frame->document();
    if (!doc.isNull())
      return doc.focusedElement();
  }

  return WebElement();
}

bool RenderViewImpl::IsEditableNode(const WebNode& node) const {
  if (node.isNull())
    return false;

  if (node.isContentEditable())
    return true;

  if (node.isElementNode()) {
    const WebElement& element = node.toConst<WebElement>();
    if (element.isTextFormControlElement())
      return true;

    // Also return true if it has an ARIA role of 'textbox'.
    for (unsigned i = 0; i < element.attributeCount(); ++i) {
      if (LowerCaseEqualsASCII(element.attributeLocalName(i), "role")) {
        if (LowerCaseEqualsASCII(element.attributeValue(i), "textbox"))
          return true;
        break;
      }
    }
  }

  return false;
}

bool RenderViewImpl::ShouldDisplayScrollbars(int width, int height) const {
  return (!send_preferred_size_changes_ ||
          (disable_scrollbars_size_limit_.width() <= width ||
           disable_scrollbars_size_limit_.height() <= height));
}

int RenderViewImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

bool RenderViewImpl::GetContentStateImmediately() const {
  return send_content_state_immediately_;
}

blink::WebPageVisibilityState RenderViewImpl::GetVisibilityState() const {
  return visibilityState();
}

void RenderViewImpl::DidStartLoading() {
  main_render_frame_->didStartLoading(true);
}

void RenderViewImpl::DidStopLoading() {
  main_render_frame_->didStopLoading();
}

void RenderViewImpl::SyncNavigationState() {
  if (!webview())
    return;
  SendUpdateState(history_controller_->GetCurrentEntry());
}

blink::WebPlugin* RenderViewImpl::GetWebPluginForFind() {
  if (!webview())
    return NULL;

  WebFrame* main_frame = webview()->mainFrame();
  if (main_frame->document().isPluginDocument())
    return webview()->mainFrame()->document().to<WebPluginDocument>().plugin();

#if defined(ENABLE_PLUGINS)
  if (plugin_find_handler_)
    return plugin_find_handler_->container()->plugin();
#endif

  return NULL;
}

void RenderViewImpl::OnFind(int request_id,
                            const base::string16& search_text,
                            const WebFindOptions& options) {
  WebFrame* main_frame = webview()->mainFrame();
  blink::WebPlugin* plugin = GetWebPluginForFind();
  // Check if the plugin still exists in the document.
  if (plugin) {
    if (options.findNext) {
      // Just navigate back/forward.
      plugin->selectFindResult(options.forward);
    } else {
      if (!plugin->startFind(
          search_text, options.matchCase, request_id)) {
        // Send "no results".
        SendFindReply(request_id, 0, 0, gfx::Rect(), true);
      }
    }
    return;
  }

  WebFrame* frame_after_main = main_frame->traverseNext(true);
  WebFrame* focused_frame = webview()->focusedFrame();
  WebFrame* search_frame = focused_frame;  // start searching focused frame.

  bool multi_frame = (frame_after_main != main_frame);

  // If we have multiple frames, we don't want to wrap the search within the
  // frame, so we check here if we only have main_frame in the chain.
  bool wrap_within_frame = !multi_frame;

  WebRect selection_rect;
  bool result = false;

  // If something is selected when we start searching it means we cannot just
  // increment the current match ordinal; we need to re-generate it.
  WebRange current_selection = focused_frame->selectionRange();

  do {
    result = search_frame->find(
        request_id, search_text, options, wrap_within_frame, &selection_rect);

    if (!result) {
      // don't leave text selected as you move to the next frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"),
                                   GetFocusedElement());

      // Find the next frame, but skip the invisible ones.
      do {
        // What is the next frame to search? (we might be going backwards). Note
        // that we specify wrap=true so that search_frame never becomes NULL.
        search_frame = options.forward ?
            search_frame->traverseNext(true) :
            search_frame->traversePrevious(true);
      } while (!search_frame->hasVisibleContent() &&
               search_frame != focused_frame);

      // Make sure selection doesn't affect the search operation in new frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"),
                                   GetFocusedElement());

      // If we have multiple frames and we have wrapped back around to the
      // focused frame, we need to search it once more allowing wrap within
      // the frame, otherwise it will report 'no match' if the focused frame has
      // reported matches, but no frames after the focused_frame contain a
      // match for the search word(s).
      if (multi_frame && search_frame == focused_frame) {
        result = search_frame->find(
            request_id, search_text, options, true,  // Force wrapping.
            &selection_rect);
      }
    }

    webview()->setFocusedFrame(search_frame);
  } while (!result && search_frame != focused_frame);

  if (options.findNext && current_selection.isNull()) {
    // Force the main_frame to report the actual count.
    main_frame->increaseMatchCount(0, request_id);
  } else {
    // If nothing is found, set result to "0 of 0", otherwise, set it to
    // "-1 of 1" to indicate that we found at least one item, but we don't know
    // yet what is active.
    int ordinal = result ? -1 : 0;  // -1 here means, we might know more later.
    int match_count = result ? 1 : 0;  // 1 here means possibly more coming.

    // If we find no matches then this will be our last status update.
    // Otherwise the scoping effort will send more results.
    bool final_status_update = !result;

    SendFindReply(request_id, match_count, ordinal, selection_rect,
                  final_status_update);

    // Scoping effort begins, starting with the mainframe.
    search_frame = main_frame;

    main_frame->resetMatchCount();

    do {
      // Cancel all old scoping requests before starting a new one.
      search_frame->cancelPendingScopingEffort();

      // We don't start another scoping effort unless at least one match has
      // been found.
      if (result) {
        // Start new scoping request. If the scoping function determines that it
        // needs to scope, it will defer until later.
        search_frame->scopeStringMatches(request_id,
                                         search_text,
                                         options,
                                         true);  // reset the tickmarks
      }

      // Iterate to the next frame. The frame will not necessarily scope, for
      // example if it is not visible.
      search_frame = search_frame->traverseNext(true);
    } while (search_frame != main_frame);
  }
}

void RenderViewImpl::OnStopFinding(StopFindAction action) {
  WebView* view = webview();
  if (!view)
    return;

  blink::WebPlugin* plugin = GetWebPluginForFind();
  if (plugin) {
    plugin->stopFind();
    return;
  }

  bool clear_selection = action == STOP_FIND_ACTION_CLEAR_SELECTION;
  if (clear_selection) {
    view->focusedFrame()->executeCommand(WebString::fromUTF8("Unselect"),
                                         GetFocusedElement());
  }

  WebFrame* frame = view->mainFrame();
  while (frame) {
    frame->stopFinding(clear_selection);
    frame = frame->traverseNext(false);
  }

  if (action == STOP_FIND_ACTION_ACTIVATE_SELECTION) {
    WebFrame* focused_frame = view->focusedFrame();
    if (focused_frame) {
      WebDocument doc = focused_frame->document();
      if (!doc.isNull()) {
        WebElement element = doc.focusedElement();
        if (!element.isNull())
          element.simulateClick();
      }
    }
  }
}

#if defined(OS_ANDROID)
void RenderViewImpl::OnActivateNearestFindResult(int request_id,
                                                 float x, float y) {
  if (!webview())
      return;

  WebFrame* main_frame = webview()->mainFrame();
  WebRect selection_rect;
  int ordinal = main_frame->selectNearestFindMatch(WebFloatPoint(x, y),
                                                   &selection_rect);
  if (ordinal == -1) {
    // Something went wrong, so send a no-op reply (force the main_frame to
    // report the current match count) in case the host is waiting for a
    // response due to rate-limiting).
    main_frame->increaseMatchCount(0, request_id);
    return;
  }

  SendFindReply(request_id,
                -1 /* number_of_matches */,
                ordinal,
                selection_rect,
                true /* final_update */);
}

void RenderViewImpl::OnFindMatchRects(int current_version) {
  if (!webview())
      return;

  WebFrame* main_frame = webview()->mainFrame();
  std::vector<gfx::RectF> match_rects;

  int rects_version = main_frame->findMatchMarkersVersion();
  if (current_version != rects_version) {
    WebVector<WebFloatRect> web_match_rects;
    main_frame->findMatchRects(web_match_rects);
    match_rects.reserve(web_match_rects.size());
    for (size_t i = 0; i < web_match_rects.size(); ++i)
      match_rects.push_back(gfx::RectF(web_match_rects[i]));
  }

  gfx::RectF active_rect = main_frame->activeFindMatchRect();
  Send(new ViewHostMsg_FindMatchRects_Reply(routing_id_,
                                               rects_version,
                                               match_rects,
                                               active_rect));
}
#endif

void RenderViewImpl::OnZoom(PageZoom zoom) {
  if (!webview())  // Not sure if this can happen, but no harm in being safe.
    return;

  webview()->hidePopups();

  double old_zoom_level = webview()->zoomLevel();
  double zoom_level;
  if (zoom == PAGE_ZOOM_RESET) {
    zoom_level = 0;
  } else if (static_cast<int>(old_zoom_level) == old_zoom_level) {
    // Previous zoom level is a whole number, so just increment/decrement.
    zoom_level = old_zoom_level + zoom;
  } else {
    // Either the user hit the zoom factor limit and thus the zoom level is now
    // not a whole number, or a plugin changed it to a custom value.  We want
    // to go to the next whole number so that the user can always get back to
    // 100% with the keyboard/menu.
    if ((old_zoom_level > 1 && zoom > 0) ||
        (old_zoom_level < 1 && zoom < 0)) {
      zoom_level = static_cast<int>(old_zoom_level + zoom);
    } else {
      // We're going towards 100%, so first go to the next whole number.
      zoom_level = static_cast<int>(old_zoom_level);
    }
  }
  webview()->setZoomLevel(zoom_level);
  zoomLevelChanged();
}

void RenderViewImpl::OnSetZoomLevelForLoadingURL(const GURL& url,
                                                 double zoom_level) {
#if !defined(OS_ANDROID)
  // On Android, page zoom isn't used, and in case of WebView, text zoom is used
  // for legacy WebView text scaling emulation. Thus, the code that resets
  // the zoom level from this map will be effectively resetting text zoom level.
  host_zoom_levels_[url] = zoom_level;
#endif
}

void RenderViewImpl::OnSetZoomLevelForView(bool uses_temporary_zoom_level,
                                           double level) {
  uses_temporary_zoom_level_ = uses_temporary_zoom_level;

  webview()->hidePopups();
  webview()->setZoomLevel(level);
}

void RenderViewImpl::OnSetPageEncoding(const std::string& encoding_name) {
  webview()->setPageEncoding(WebString::fromUTF8(encoding_name));
}

void RenderViewImpl::OnResetPageEncodingToDefault() {
  WebString no_encoding;
  webview()->setPageEncoding(no_encoding);
}

void RenderViewImpl::OnPostMessageEvent(
    const ViewMsg_PostMessage_Params& params) {
  // TODO(nasko): Support sending to subframes.
  WebFrame* frame = webview()->mainFrame();

  // Find the source frame if it exists.
  WebFrame* source_frame = NULL;
  if (params.source_routing_id != MSG_ROUTING_NONE) {
    RenderViewImpl* source_view = FromRoutingID(params.source_routing_id);
    if (source_view)
      source_frame = source_view->webview()->mainFrame();
  }

  // If the message contained MessagePorts, create the corresponding endpoints.
  DCHECK_EQ(params.message_port_ids.size(), params.new_routing_ids.size());
  blink::WebMessagePortChannelArray channels(params.message_port_ids.size());
  for (size_t i = 0;
       i < params.message_port_ids.size() && i < params.new_routing_ids.size();
       ++i) {
    channels[i] =
        new WebMessagePortChannelImpl(params.new_routing_ids[i],
                                      params.message_port_ids[i],
                                      base::MessageLoopProxy::current().get());
  }

  // Create an event with the message.  The final parameter to initMessageEvent
  // is the last event ID, which is not used with postMessage.
  WebDOMEvent event = frame->document().createEvent("MessageEvent");
  WebDOMMessageEvent msg_event = event.to<WebDOMMessageEvent>();
  msg_event.initMessageEvent("message",
                             // |canBubble| and |cancellable| are always false
                             false, false,
                             WebSerializedScriptValue::fromString(params.data),
                             params.source_origin, source_frame, "", channels);

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  WebSecurityOrigin target_origin;
  if (!params.target_origin.empty()) {
    target_origin =
        WebSecurityOrigin::createFromString(WebString(params.target_origin));
  }
  frame->dispatchMessageEventWithOriginCheck(target_origin, msg_event);
}

void RenderViewImpl::OnAllowBindings(int enabled_bindings_flags) {
  if ((enabled_bindings_flags & BINDINGS_POLICY_WEB_UI) &&
      !(enabled_bindings_ & BINDINGS_POLICY_WEB_UI)) {
    // WebUIExtensionData deletes itself when we're destroyed.
    new WebUIExtensionData(this);
    // WebUIMojo deletes itself when we're destroyed.
    new WebUIMojo(this);
  }

  enabled_bindings_ |= enabled_bindings_flags;

  // Keep track of the total bindings accumulated in this process.
  RenderProcess::current()->AddBindings(enabled_bindings_flags);
}

void RenderViewImpl::OnDragTargetDragEnter(const DropData& drop_data,
                                           const gfx::Point& client_point,
                                           const gfx::Point& screen_point,
                                           WebDragOperationsMask ops,
                                           int key_modifiers) {
  WebDragOperation operation = webview()->dragTargetDragEnter(
      DropDataToWebDragData(drop_data),
      client_point,
      screen_point,
      ops,
      key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderViewImpl::OnDragTargetDragOver(const gfx::Point& client_point,
                                          const gfx::Point& screen_point,
                                          WebDragOperationsMask ops,
                                          int key_modifiers) {
  WebDragOperation operation = webview()->dragTargetDragOver(
      client_point,
      screen_point,
      ops,
      key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderViewImpl::OnDragTargetDragLeave() {
  webview()->dragTargetDragLeave();
}

void RenderViewImpl::OnDragTargetDrop(const gfx::Point& client_point,
                                      const gfx::Point& screen_point,
                                      int key_modifiers) {
  webview()->dragTargetDrop(client_point, screen_point, key_modifiers);

  Send(new DragHostMsg_TargetDrop_ACK(routing_id_));
}

void RenderViewImpl::OnDragSourceEnded(const gfx::Point& client_point,
                                       const gfx::Point& screen_point,
                                       WebDragOperation op) {
  webview()->dragSourceEndedAt(client_point, screen_point, op);
}

void RenderViewImpl::OnDragSourceSystemDragEnded() {
  webview()->dragSourceSystemDragEnded();
}

void RenderViewImpl::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  ApplyWebPreferences(webkit_preferences_, webview());
}

void RenderViewImpl::OnEnumerateDirectoryResponse(
    int id,
    const std::vector<base::FilePath>& paths) {
  if (!enumeration_completions_[id])
    return;

  WebVector<WebString> ws_file_names(paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    ws_file_names[i] = paths[i].AsUTF16Unsafe();

  enumeration_completions_[id]->didChooseFile(ws_file_names);
  enumeration_completions_.erase(id);
}

void RenderViewImpl::OnFileChooserResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  // This could happen if we navigated to a different page before the user
  // closed the chooser.
  if (file_chooser_completions_.empty())
    return;

  // Convert Chrome's SelectedFileInfo list to WebKit's.
  WebVector<WebFileChooserCompletion::SelectedFileInfo> selected_files(
      files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    WebFileChooserCompletion::SelectedFileInfo selected_file;
    selected_file.path = files[i].local_path.AsUTF16Unsafe();
    selected_file.displayName =
        base::FilePath(files[i].display_name).AsUTF16Unsafe();
    selected_files[i] = selected_file;
  }

  if (file_chooser_completions_.front()->completion)
    file_chooser_completions_.front()->completion->didChooseFile(
        selected_files);
  file_chooser_completions_.pop_front();

  // If there are more pending file chooser requests, schedule one now.
  if (!file_chooser_completions_.empty()) {
    Send(new ViewHostMsg_RunFileChooser(routing_id_,
        file_chooser_completions_.front()->params));
  }
}

void RenderViewImpl::OnEnableAutoResize(const gfx::Size& min_size,
                                        const gfx::Size& max_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;
  auto_resize_mode_ = true;
  webview()->enableAutoResizeMode(min_size, max_size);
}

void RenderViewImpl::OnDisableAutoResize(const gfx::Size& new_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;
  auto_resize_mode_ = false;
  webview()->disableAutoResizeMode();

  if (!new_size.IsEmpty()) {
    Resize(new_size,
           physical_backing_size_,
           overdraw_bottom_height_,
           visible_viewport_size_,
           resizer_rect_,
           is_fullscreen_,
           NO_RESIZE_ACK);
  }
}

void RenderViewImpl::OnEnablePreferredSizeChangedMode() {
  if (send_preferred_size_changes_)
    return;
  send_preferred_size_changes_ = true;

  // Start off with an initial preferred size notification (in case
  // |didUpdateLayout| was already called).
  didUpdateLayout();
}

void RenderViewImpl::OnDisableScrollbarsForSmallWindows(
    const gfx::Size& disable_scrollbar_size_limit) {
  disable_scrollbars_size_limit_ = disable_scrollbar_size_limit;
}

void RenderViewImpl::OnSetRendererPrefs(
    const RendererPreferences& renderer_prefs) {
  double old_zoom_level = renderer_preferences_.default_zoom_level;
  std::string old_accept_languages = renderer_preferences_.accept_languages;

  renderer_preferences_ = renderer_prefs;
  UpdateFontRenderingFromRendererPrefs();

#if defined(USE_DEFAULT_RENDER_THEME)
  if (renderer_prefs.use_custom_colors) {
    WebColorName name = blink::WebColorWebkitFocusRingColor;
    blink::setNamedColors(&name, &renderer_prefs.focus_ring_color, 1);
    blink::setCaretBlinkInterval(renderer_prefs.caret_blink_interval);

    if (webview()) {
      webview()->setSelectionColors(
          renderer_prefs.active_selection_bg_color,
          renderer_prefs.active_selection_fg_color,
          renderer_prefs.inactive_selection_bg_color,
          renderer_prefs.inactive_selection_fg_color);
      webview()->themeChanged();
    }
  }
#endif  // defined(USE_DEFAULT_RENDER_THEME)

  if (RenderThreadImpl::current())  // Will be NULL during unit tests.
    RenderThreadImpl::current()->SetFlingCurveParameters(
        renderer_prefs.touchpad_fling_profile,
        renderer_prefs.touchscreen_fling_profile);

  // If the zoom level for this page matches the old zoom default, and this
  // is not a plugin, update the zoom level to match the new default.
  if (webview() && !webview()->mainFrame()->document().isPluginDocument() &&
      !ZoomValuesEqual(old_zoom_level,
                       renderer_preferences_.default_zoom_level) &&
      ZoomValuesEqual(webview()->zoomLevel(), old_zoom_level)) {
    webview()->setZoomLevel(renderer_preferences_.default_zoom_level);
    zoomLevelChanged();
  }

  if (webview() &&
      old_accept_languages != renderer_preferences_.accept_languages) {
    webview()->acceptLanguagesChanged();
  }
}

void RenderViewImpl::OnMediaPlayerActionAt(const gfx::Point& location,
                                           const WebMediaPlayerAction& action) {
  if (webview())
    webview()->performMediaPlayerAction(action, location);
}

void RenderViewImpl::OnOrientationChange() {
  // TODO(mlamouri): consumers of that event should be using DisplayObserver.
  FOR_EACH_OBSERVER(RenderViewObserver,
                    observers_,
                    OrientationChangeEvent());

  webview()->mainFrame()->sendOrientationChangeEvent();
}

void RenderViewImpl::OnPluginActionAt(const gfx::Point& location,
                                      const WebPluginAction& action) {
  if (webview())
    webview()->performPluginAction(action, location);
}

void RenderViewImpl::OnGetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  // Prepare list to storage all savable resource links.
  std::vector<GURL> resources_list;
  std::vector<GURL> referrer_urls_list;
  std::vector<blink::WebReferrerPolicy> referrer_policies_list;
  std::vector<GURL> frames_list;
  SavableResourcesResult result(&resources_list,
                                &referrer_urls_list,
                                &referrer_policies_list,
                                &frames_list);

  // webkit/ doesn't know about Referrer.
  if (!GetAllSavableResourceLinksForCurrentPage(
          webview(),
          page_url,
          &result,
          const_cast<const char**>(GetSavableSchemes()))) {
    // If something is wrong when collecting all savable resource links,
    // send empty list to embedder(browser) to tell it failed.
    referrer_urls_list.clear();
    referrer_policies_list.clear();
    resources_list.clear();
    frames_list.clear();
  }

  std::vector<Referrer> referrers_list;
  CHECK_EQ(referrer_urls_list.size(), referrer_policies_list.size());
  for (unsigned i = 0; i < referrer_urls_list.size(); ++i) {
    referrers_list.push_back(
        Referrer(referrer_urls_list[i], referrer_policies_list[i]));
  }

  // Send result of all savable resource links to embedder.
  Send(new ViewHostMsg_SendCurrentPageAllSavableResourceLinks(routing_id(),
                                                              resources_list,
                                                              referrers_list,
                                                              frames_list));
}

void RenderViewImpl::OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<GURL>& links,
    const std::vector<base::FilePath>& local_paths,
    const base::FilePath& local_directory_name) {

  // Convert std::vector of GURLs to WebVector<WebURL>
  WebVector<WebURL> weburl_links(links);

  // Convert std::vector of base::FilePath to WebVector<WebString>
  WebVector<WebString> webstring_paths(local_paths.size());
  for (size_t i = 0; i < local_paths.size(); i++)
    webstring_paths[i] = local_paths[i].AsUTF16Unsafe();

  WebPageSerializer::serialize(webview()->mainFrame()->toWebLocalFrame(),
                               true,
                               this,
                               weburl_links,
                               webstring_paths,
                               local_directory_name.AsUTF16Unsafe());
}

void RenderViewImpl::OnSuppressDialogsUntilSwapOut() {
  // Don't show any more dialogs until we finish OnSwapOut.
  suppress_dialogs_until_swap_out_ = true;
}

void RenderViewImpl::NavigateToSwappedOutURL(blink::WebFrame* frame) {
  // We use loadRequest instead of loadHTMLString because the former commits
  // synchronously.  Otherwise a new navigation can interrupt the navigation
  // to kSwappedOutURL. If that happens to be to the page we had been
  // showing, then WebKit will never send a commit and we'll be left spinning.
  // TODO(creis): Until we move this to RenderFrame, we may call this from a
  // swapped out RenderFrame while our own is_swapped_out_ is false.
  RenderFrameImpl* rf = RenderFrameImpl::FromWebFrame(frame);
  CHECK(is_swapped_out_ || rf->is_swapped_out());
  GURL swappedOutURL(kSwappedOutURL);
  WebURLRequest request(swappedOutURL);
  frame->loadRequest(request);
}

void RenderViewImpl::OnClosePage() {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, ClosePage());
  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  webview()->mainFrame()->dispatchUnloadEvent();

  Send(new ViewHostMsg_ClosePage_ACK(routing_id_));
}

void RenderViewImpl::OnThemeChanged() {
#if defined(USE_AURA)
  // Aura doesn't care if we switch themes.
#elif defined(OS_WIN)
  ui::NativeThemeWin::instance()->CloseHandles();
  if (webview())
    webview()->themeChanged();
#else  // defined(OS_WIN)
  // TODO(port): we don't support theming on non-Windows platforms yet
  NOTIMPLEMENTED();
#endif
}

void RenderViewImpl::OnMoveOrResizeStarted() {
  if (webview())
    webview()->hidePopups();
}

void RenderViewImpl::OnResize(const ViewMsg_Resize_Params& params) {
  if (webview()) {
    webview()->hidePopups();
    if (send_preferred_size_changes_) {
      webview()->mainFrame()->setCanHaveScrollbars(
          ShouldDisplayScrollbars(params.new_size.width(),
                                  params.new_size.height()));
    }
    UpdateScrollState(webview()->mainFrame());
  }

  gfx::Size old_visible_viewport_size = visible_viewport_size_;

  RenderWidget::OnResize(params);

  if (old_visible_viewport_size != visible_viewport_size_)
    has_scrolled_focused_editable_node_into_rect_ = false;
}

void RenderViewImpl::DidInitiatePaint() {
#if defined(ENABLE_PLUGINS)
  // Notify all instances that we painted.  The same caveats apply as for
  // ViewFlushedPaint regarding instances closing themselves, so we take
  // similar precautions.
  PepperPluginSet plugins = active_pepper_instances_;
  for (PepperPluginSet::iterator i = plugins.begin(); i != plugins.end(); ++i) {
    if (active_pepper_instances_.find(*i) != active_pepper_instances_.end())
      (*i)->ViewInitiatedPaint();
  }
#endif
}

void RenderViewImpl::DidFlushPaint() {
#if defined(ENABLE_PLUGINS)
  // Notify all instances that we flushed. This will call into the plugin, and
  // we it may ask to close itself as a result. This will, in turn, modify our
  // set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  PepperPluginSet plugins = active_pepper_instances_;
  for (PepperPluginSet::iterator i = plugins.begin(); i != plugins.end(); ++i) {
    // The copy above makes sure our iterator is never invalid if some plugins
    // are destroyed. But some plugin may decide to close all of its views in
    // response to a paint in one of them, so we need to make sure each one is
    // still "current" before using it.
    //
    // It's possible that a plugin was destroyed, but another one was created
    // with the same address. In this case, we'll call ViewFlushedPaint on that
    // new plugin. But that's OK for this particular case since we're just
    // notifying all of our instances that the view flushed, and the new one is
    // one of our instances.
    //
    // What about the case where a new one is created in a callback at a new
    // address and we don't issue the callback? We're still OK since this
    // callback is used for flush callbacks and we could not have possibly
    // started a new paint for the new plugin while processing a previous paint
    // for an existing one.
    if (active_pepper_instances_.find(*i) != active_pepper_instances_.end())
      (*i)->ViewFlushedPaint();
  }
#endif

  // If the RenderWidget is closing down then early-exit, otherwise we'll crash.
  // See crbug.com/112921.
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();

  // If we have a provisional frame we are between the start and commit stages
  // of loading and we don't want to save stats.
  if (!main_frame->provisionalDataSource()) {
    WebDataSource* ds = main_frame->dataSource();
    DocumentState* document_state = DocumentState::FromDataSource(ds);
    InternalDocumentStateData* data =
        InternalDocumentStateData::FromDocumentState(document_state);
    if (data->did_first_visually_non_empty_layout() &&
        !data->did_first_visually_non_empty_paint()) {
      data->set_did_first_visually_non_empty_paint(true);
      Send(new ViewHostMsg_DidFirstVisuallyNonEmptyPaint(routing_id_));
    }

    // TODO(jar): The following code should all be inside a method, probably in
    // NavigatorState.
    Time now = Time::Now();
    if (document_state->first_paint_time().is_null()) {
      document_state->set_first_paint_time(now);
    }
    if (document_state->first_paint_after_load_time().is_null() &&
        !document_state->finish_load_time().is_null()) {
      document_state->set_first_paint_after_load_time(now);
    }
  }
}

gfx::Vector2d RenderViewImpl::GetScrollOffset() {
  WebSize scroll_offset = webview()->mainFrame()->scrollOffset();
  return gfx::Vector2d(scroll_offset.width, scroll_offset.height);
}

void RenderViewImpl::OnClearFocusedElement() {
  if (webview())
    webview()->clearFocusedElement();
}

void RenderViewImpl::OnSetBackgroundOpaque(bool opaque) {
  if (webview())
    webview()->setIsTransparent(!opaque);
  if (compositor_)
    compositor_->setHasTransparentBackground(!opaque);
}

void RenderViewImpl::OnSetAccessibilityMode(AccessibilityMode new_mode) {
  if (accessibility_mode_ == new_mode)
    return;
  accessibility_mode_ = new_mode;
  if (renderer_accessibility_) {
    delete renderer_accessibility_;
    renderer_accessibility_ = NULL;
  }
  if (accessibility_mode_ == AccessibilityModeOff)
    return;

  if (accessibility_mode_ & AccessibilityModeFlagFullTree)
    renderer_accessibility_ = new RendererAccessibilityComplete(this);
#if !defined(OS_ANDROID)
  else
    renderer_accessibility_ = new RendererAccessibilityFocusOnly(this);
#endif
}

void RenderViewImpl::OnSetActive(bool active) {
  if (webview())
    webview()->setIsActive(active);

#if defined(ENABLE_PLUGINS) && defined(OS_MACOSX)
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetWindowFocus(active);
  }
#endif
}

#if defined(OS_MACOSX)
void RenderViewImpl::OnSetWindowVisibility(bool visible) {
#if defined(ENABLE_PLUGINS)
  // Inform plugins that their container has changed visibility.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(visible);
  }
#endif
}

void RenderViewImpl::OnWindowFrameChanged(const gfx::Rect& window_frame,
                                          const gfx::Rect& view_frame) {
#if defined(ENABLE_PLUGINS)
  // Inform plugins that their window's frame has changed.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->WindowFrameChanged(window_frame, view_frame);
  }
#endif
}

void RenderViewImpl::OnPluginImeCompositionCompleted(const base::string16& text,
                                                     int plugin_id) {
  // WebPluginDelegateProxy is responsible for figuring out if this event
  // applies to it or not, so inform all the delegates.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->ImeCompositionCompleted(text, plugin_id);
  }
}
#endif  // OS_MACOSX

void RenderViewImpl::OnClose() {
  if (closing_)
    RenderThread::Get()->Send(new ViewHostMsg_Close_ACK(routing_id_));
  RenderWidget::OnClose();
}

void RenderViewImpl::Close() {
  // We need to grab a pointer to the doomed WebView before we destroy it.
  WebView* doomed = webview();
  RenderWidget::Close();
  g_view_map.Get().erase(doomed);
  g_routing_id_view_map.Get().erase(routing_id_);
  RenderThread::Get()->Send(new ViewHostMsg_Close_ACK(routing_id_));
}

void RenderViewImpl::DidHandleKeyEvent() {
  ClearEditCommands();
}

bool RenderViewImpl::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);

#if defined(ENABLE_PLUGINS)
  // This method is called for every mouse event that the render view receives.
  // And then the mouse event is forwarded to WebKit, which dispatches it to the
  // event target. Potentially a Pepper plugin will receive the event.
  // In order to tell whether a plugin gets the last mouse event and which it
  // is, we set |pepper_last_mouse_event_target_| to NULL here. If a plugin gets
  // the event, it will notify us via DidReceiveMouseEvent() and set itself as
  // |pepper_last_mouse_event_target_|.
  pepper_last_mouse_event_target_ = NULL;
#endif

  // If the mouse is locked, only the current owner of the mouse lock can
  // process mouse events.
  return mouse_lock_dispatcher_->WillHandleMouseEvent(event);
}

bool RenderViewImpl::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);
  return false;
}

void RenderViewImpl::DidHandleMouseEvent(const WebMouseEvent& event) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidHandleMouseEvent(event));
}

void RenderViewImpl::DidHandleTouchEvent(const WebTouchEvent& event) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidHandleTouchEvent(event));
}

bool RenderViewImpl::HasTouchEventHandlersAt(const gfx::Point& point) const {
  if (!webview())
    return false;
  return webview()->hasTouchEventHandlersAt(point);
}

void RenderViewImpl::OnWasHidden() {
  RenderWidget::OnWasHidden();

#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  RenderThreadImpl::current()->video_capture_impl_manager()->
      SuspendDevices(true);
  if (speech_recognition_dispatcher_)
    speech_recognition_dispatcher_->AbortAllRecognitions();
#endif

  if (webview())
    webview()->setVisibilityState(visibilityState(), false);

#if defined(ENABLE_PLUGINS)
  for (PepperPluginSet::iterator i = active_pepper_instances_.begin();
       i != active_pepper_instances_.end(); ++i)
    (*i)->PageVisibilityChanged(false);

#if defined(OS_MACOSX)
  // Inform NPAPI plugins that their container is no longer visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(false);
  }
#endif  // OS_MACOSX
#endif // ENABLE_PLUGINS
}

void RenderViewImpl::OnWasShown(bool needs_repainting) {
  RenderWidget::OnWasShown(needs_repainting);

#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  RenderThreadImpl::current()->video_capture_impl_manager()->
      SuspendDevices(false);
#endif

  if (webview())
    webview()->setVisibilityState(visibilityState(), false);

#if defined(ENABLE_PLUGINS)
  for (PepperPluginSet::iterator i = active_pepper_instances_.begin();
       i != active_pepper_instances_.end(); ++i)
    (*i)->PageVisibilityChanged(true);

#if defined(OS_MACOSX)
  // Inform NPAPI plugins that their container is now visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(true);
  }
#endif  // OS_MACOSX
#endif  // ENABLE_PLUGINS
}

GURL RenderViewImpl::GetURLForGraphicsContext3D() {
  DCHECK(webview());
  if (webview()->mainFrame())
    return GURL(webview()->mainFrame()->document().url());
  else
    return GURL("chrome://gpu/RenderViewImpl::CreateGraphicsContext3D");
}

void RenderViewImpl::OnSetFocus(bool enable) {
  RenderWidget::OnSetFocus(enable);

#if defined(ENABLE_PLUGINS)
  if (webview() && webview()->isActive()) {
    // Notify all NPAPI plugins.
    std::set<WebPluginDelegateProxy*>::iterator plugin_it;
    for (plugin_it = plugin_delegates_.begin();
         plugin_it != plugin_delegates_.end(); ++plugin_it) {
#if defined(OS_MACOSX)
      // RenderWidget's call to setFocus can cause the underlying webview's
      // activation state to change just like a call to setIsActive.
      if (enable)
        (*plugin_it)->SetWindowFocus(true);
#endif
      (*plugin_it)->SetContentAreaFocus(enable);
    }
  }
  // Notify all Pepper plugins.
  for (PepperPluginSet::iterator i = active_pepper_instances_.begin();
       i != active_pepper_instances_.end(); ++i)
    (*i)->SetContentAreaFocus(enable);
#endif
  // Notify all BrowserPlugins of the RenderView's focus state.
  if (browser_plugin_manager_.get())
    browser_plugin_manager_->UpdateFocusState();
}

void RenderViewImpl::OnImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeSetComposition(
        text, underlines, selection_start, selection_end);
    return;
  }

#if defined(OS_WIN)
  // When a plug-in has focus, we create platform-specific IME data used by
  // our IME emulator and send it directly to the focused plug-in, i.e. we
  // bypass WebKit. (WebPluginDelegate dispatches this IME data only when its
  // instance ID is the same one as the specified ID.)
  if (focused_plugin_id_ >= 0) {
    std::vector<int> clauses;
    std::vector<int> target;
    for (size_t i = 0; i < underlines.size(); ++i) {
      clauses.push_back(underlines[i].startOffset);
      clauses.push_back(underlines[i].endOffset);
      if (underlines[i].thick) {
        target.clear();
        target.push_back(underlines[i].startOffset);
        target.push_back(underlines[i].endOffset);
      }
    }
    std::set<WebPluginDelegateProxy*>::iterator it;
    for (it = plugin_delegates_.begin(); it != plugin_delegates_.end(); ++it) {
      (*it)->ImeCompositionUpdated(text, clauses, target, selection_end,
                                   focused_plugin_id_);
    }
    return;
  }
#endif  // OS_WIN
#endif  // ENABLE_PLUGINS
  RenderWidget::OnImeSetComposition(text,
                                    underlines,
                                    selection_start,
                                    selection_end);
}

void RenderViewImpl::OnImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    focused_pepper_plugin_->render_frame()->OnImeConfirmComposition(
        text, replacement_range, keep_selection);
    return;
  }
#if defined(OS_WIN)
  // Same as OnImeSetComposition(), we send the text from IMEs directly to
  // plug-ins. When we send IME text directly to plug-ins, we should not send
  // it to WebKit to prevent WebKit from controlling IMEs.
  // TODO(thakis): Honor |replacement_range| for plugins?
  if (focused_plugin_id_ >= 0) {
    std::set<WebPluginDelegateProxy*>::iterator it;
    for (it = plugin_delegates_.begin();
          it != plugin_delegates_.end(); ++it) {
      (*it)->ImeCompositionCompleted(text, focused_plugin_id_);
    }
    return;
  }
#endif  // OS_WIN
#endif  // ENABLE_PLUGINS
  if (replacement_range.IsValid() && webview()) {
    // Select the text in |replacement_range|, it will then be replaced by
    // text added by the call to RenderWidget::OnImeConfirmComposition().
    if (WebLocalFrame* frame = webview()->focusedFrame()->toWebLocalFrame()) {
      WebRange webrange = WebRange::fromDocumentRange(
          frame, replacement_range.start(), replacement_range.length());
      if (!webrange.isNull())
        frame->selectRange(webrange);
    }
  }
  RenderWidget::OnImeConfirmComposition(text,
                                        replacement_range,
                                        keep_selection);
}

void RenderViewImpl::SetDeviceScaleFactor(float device_scale_factor) {
  RenderWidget::SetDeviceScaleFactor(device_scale_factor);
  if (webview()) {
    webview()->setDeviceScaleFactor(device_scale_factor);
    webview()->settings()->setAcceleratedCompositingForFixedPositionEnabled(
        ShouldUseFixedPositionCompositing(device_scale_factor_));
    webview()->settings()->setAcceleratedCompositingForOverflowScrollEnabled(
        ShouldUseAcceleratedCompositingForOverflowScroll(device_scale_factor_));
    webview()->settings()->setAcceleratedCompositingForTransitionEnabled(
        ShouldUseTransitionCompositing(device_scale_factor_));
    webview()->settings()->
        setAcceleratedCompositingForFixedRootBackgroundEnabled(
            ShouldUseAcceleratedFixedRootBackground(device_scale_factor_));
    webview()->settings()->setCompositedScrollingForFramesEnabled(
        ShouldUseCompositedScrollingForFrames(device_scale_factor_));
  }
  if (auto_resize_mode_)
    AutoResizeCompositor();

  if (browser_plugin_manager_.get())
    browser_plugin_manager_->UpdateDeviceScaleFactor(device_scale_factor_);
}

bool RenderViewImpl::SetDeviceColorProfile(
    const std::vector<char>& profile) {
  bool changed = RenderWidget::SetDeviceColorProfile(profile);
  if (changed && webview()) {
    // TODO(noel): notify the webview() of the color profile change so it
    // can update and repaint all color profiled page elements.
  }
  return changed;
}

ui::TextInputType RenderViewImpl::GetTextInputType() {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->text_input_type();
#endif
  return RenderWidget::GetTextInputType();
}

void RenderViewImpl::GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    gfx::Rect caret = focused_pepper_plugin_->GetCaretBounds();
    *start = caret;
    *end = caret;
    return;
  }
#endif
  RenderWidget::GetSelectionBounds(start, end);
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void RenderViewImpl::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds) {
  DCHECK(bounds);
  bounds->clear();

#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    return;
  }
#endif

  if (!webview())
    return;
  size_t start_offset = 0;
  size_t character_count = 0;
  if (!webview()->compositionRange(&start_offset, &character_count))
    return;
  if (character_count == 0)
    return;

  blink::WebFrame* frame = webview()->focusedFrame();
  if (!frame)
    return;

  bounds->reserve(character_count);
  blink::WebRect webrect;
  for (size_t i = 0; i < character_count; ++i) {
    if (!frame->firstRectForCharacterRange(start_offset + i, 1, webrect)) {
      DLOG(ERROR) << "Could not retrieve character rectangle at " << i;
      bounds->clear();
      return;
    }
    bounds->push_back(webrect);
  }
}

void RenderViewImpl::GetCompositionRange(gfx::Range* range) {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_) {
    return;
  }
#endif
  RenderWidget::GetCompositionRange(range);
}
#endif

bool RenderViewImpl::CanComposeInline() {
#if defined(ENABLE_PLUGINS)
  if (focused_pepper_plugin_)
    return focused_pepper_plugin_->IsPluginAcceptingCompositionEvents();
#endif
  return true;
}

void RenderViewImpl::InstrumentWillBeginFrame(int frame_id) {
  if (!webview())
    return;
  if (!webview()->devToolsAgent())
    return;
  webview()->devToolsAgent()->didBeginFrame(frame_id);
}

void RenderViewImpl::InstrumentDidBeginFrame() {
  if (!webview())
    return;
  if (!webview()->devToolsAgent())
    return;
  // TODO(jamesr/caseq): Decide if this needs to be renamed.
  webview()->devToolsAgent()->didComposite();
}

void RenderViewImpl::InstrumentDidCancelFrame() {
  if (!webview())
    return;
  if (!webview()->devToolsAgent())
    return;
  webview()->devToolsAgent()->didCancelFrame();
}

void RenderViewImpl::InstrumentWillComposite() {
  if (!webview())
    return;
  if (!webview()->devToolsAgent())
    return;
  webview()->devToolsAgent()->willComposite();
}

void RenderViewImpl::SetScreenMetricsEmulationParameters(
    float device_scale_factor,
    const gfx::Point& root_layer_offset,
    float root_layer_scale) {
  if (webview() && compositor()) {
    webview()->setCompositorDeviceScaleFactorOverride(device_scale_factor);
    webview()->setRootLayerTransform(
        blink::WebSize(root_layer_offset.x(), root_layer_offset.y()),
        root_layer_scale);
  }
}

bool RenderViewImpl::ScheduleFileChooser(
    const FileChooserParams& params,
    WebFileChooserCompletion* completion) {
  static const size_t kMaximumPendingFileChooseRequests = 4;
  if (file_chooser_completions_.size() > kMaximumPendingFileChooseRequests) {
    // This sanity check prevents too many file choose requests from getting
    // queued which could DoS the user. Getting these is most likely a
    // programming error (there are many ways to DoS the user so it's not
    // considered a "real" security check), either in JS requesting many file
    // choosers to pop up, or in a plugin.
    //
    // TODO(brettw) we might possibly want to require a user gesture to open
    // a file picker, which will address this issue in a better way.
    return false;
  }

  file_chooser_completions_.push_back(linked_ptr<PendingFileChooser>(
      new PendingFileChooser(params, completion)));
  if (file_chooser_completions_.size() == 1) {
    // Actually show the browse dialog when this is the first request.
    Send(new ViewHostMsg_RunFileChooser(routing_id_, params));
  }
  return true;
}

blink::WebSpeechRecognizer* RenderViewImpl::speechRecognizer() {
  if (!speech_recognition_dispatcher_)
    speech_recognition_dispatcher_ = new SpeechRecognitionDispatcher(this);
  return speech_recognition_dispatcher_;
}

void RenderViewImpl::zoomLimitsChanged(double minimum_level,
                                       double maximum_level) {
  int minimum_percent = static_cast<int>(
      ZoomLevelToZoomFactor(minimum_level) * 100);
  int maximum_percent = static_cast<int>(
      ZoomLevelToZoomFactor(maximum_level) * 100);

  Send(new ViewHostMsg_UpdateZoomLimits(
      routing_id_, minimum_percent, maximum_percent));
}

void RenderViewImpl::zoomLevelChanged() {
  double zoom_level = webview()->zoomLevel();

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, ZoomLevelChanged());

  // Do not send empty URLs to the browser when we are just setting the default
  // zoom level (from RendererPreferences) before the first navigation.
  if (!webview()->mainFrame()->document().url().isEmpty()) {
    // Tell the browser which url got zoomed so it can update the menu and the
    // saved values if necessary
    Send(new ViewHostMsg_DidZoomURL(
        routing_id_, zoom_level,
        GURL(webview()->mainFrame()->document().url())));
  }
}

double RenderViewImpl::zoomLevelToZoomFactor(double zoom_level) const {
  return ZoomLevelToZoomFactor(zoom_level);
}

double RenderViewImpl::zoomFactorToZoomLevel(double factor) const {
  return ZoomFactorToZoomLevel(factor);
}

void RenderViewImpl::registerProtocolHandler(const WebString& scheme,
                                             const WebURL& base_url,
                                             const WebURL& url,
                                             const WebString& title) {
  bool user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  GURL base(base_url);
  GURL absolute_url = base.Resolve(base::UTF16ToUTF8(url.string()));
  if (base.GetOrigin() != absolute_url.GetOrigin()) {
    return;
  }
  Send(new ViewHostMsg_RegisterProtocolHandler(routing_id_,
                                               base::UTF16ToUTF8(scheme),
                                               absolute_url,
                                               title,
                                               user_gesture));
}

blink::WebPageVisibilityState RenderViewImpl::visibilityState() const {
  blink::WebPageVisibilityState current_state = is_hidden() ?
      blink::WebPageVisibilityStateHidden :
      blink::WebPageVisibilityStateVisible;
  blink::WebPageVisibilityState override_state = current_state;
  // TODO(jam): move this method to WebFrameClient.
  if (GetContentClient()->renderer()->
          ShouldOverridePageVisibilityState(main_render_frame_.get(),
                                            &override_state))
    return override_state;
  return current_state;
}

blink::WebPushClient* RenderViewImpl::webPushClient() {
  if (!push_messaging_dispatcher_)
    push_messaging_dispatcher_ = new PushMessagingDispatcher(this);
  return push_messaging_dispatcher_;
}

void RenderViewImpl::draggableRegionsChanged() {
  FOR_EACH_OBSERVER(
      RenderViewObserver,
      observers_,
      DraggableRegionsChanged(webview()->mainFrame()));
}

#if defined(OS_ANDROID)
WebContentDetectionResult RenderViewImpl::detectContentAround(
    const WebHitTestResult& touch_hit) {
  DCHECK(!touch_hit.isNull());
  DCHECK(!touch_hit.node().isNull());
  DCHECK(touch_hit.node().isTextNode());

  // Process the position with all the registered content detectors until
  // a match is found. Priority is provided by their relative order.
  for (ContentDetectorList::const_iterator it = content_detectors_.begin();
      it != content_detectors_.end(); ++it) {
    ContentDetector::Result content = (*it)->FindTappedContent(touch_hit);
    if (content.valid) {
      return WebContentDetectionResult(content.content_boundaries,
          base::UTF8ToUTF16(content.text), content.intent_url);
    }
  }
  return WebContentDetectionResult();
}

void RenderViewImpl::scheduleContentIntent(const WebURL& intent) {
  // Introduce a short delay so that the user can notice the content.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&RenderViewImpl::LaunchAndroidContentIntent,
                 AsWeakPtr(),
                 intent,
                 expected_content_intent_id_),
      base::TimeDelta::FromMilliseconds(kContentIntentDelayMilliseconds));
}

void RenderViewImpl::cancelScheduledContentIntents() {
  ++expected_content_intent_id_;
}

void RenderViewImpl::LaunchAndroidContentIntent(const GURL& intent,
                                                size_t request_id) {
  if (request_id != expected_content_intent_id_)
      return;

  // Remove the content highlighting if any.
  scheduleComposite();

  if (!intent.is_empty())
    Send(new ViewHostMsg_StartContentIntent(routing_id_, intent));
}

bool RenderViewImpl::openDateTimeChooser(
    const blink::WebDateTimeChooserParams& params,
    blink::WebDateTimeChooserCompletion* completion) {
  // JavaScript may try to open a date time chooser while one is already open.
  if (date_time_picker_client_)
    return false;
  date_time_picker_client_.reset(
      new RendererDateTimePicker(this, params, completion));
  return date_time_picker_client_->Open();
}

void RenderViewImpl::DismissDateTimeDialog() {
  DCHECK(date_time_picker_client_);
  date_time_picker_client_.reset(NULL);
}

#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX)
void RenderViewImpl::OnSelectPopupMenuItem(int selected_index) {
  if (external_popup_menu_ == NULL)
    return;
  external_popup_menu_->DidSelectItem(selected_index);
  external_popup_menu_.reset();
}
#endif

#if defined(OS_ANDROID)
void RenderViewImpl::OnSelectPopupMenuItems(
    bool canceled,
    const std::vector<int>& selected_indices) {
  // It is possible to receive more than one of these calls if the user presses
  // a select faster than it takes for the show-select-popup IPC message to make
  // it to the browser UI thread.  Ignore the extra-messages.
  // TODO(jcivelli): http:/b/5793321 Implement a better fix, as detailed in bug.
  if (!external_popup_menu_)
    return;

  external_popup_menu_->DidSelectItems(canceled, selected_indices);
  external_popup_menu_.reset();
}
#endif

#if defined(OS_MACOSX) || defined(OS_ANDROID)
void RenderViewImpl::DidHideExternalPopupMenu() {
  // We need to clear external_popup_menu_ as soon as ExternalPopupMenu::close
  // is called. Otherwise, createExternalPopupMenu() for new popup will fail.
  external_popup_menu_.reset();
}
#endif

void RenderViewImpl::OnShowContextMenu(
    ui::MenuSourceType source_type, const gfx::Point& location) {
  context_menu_source_type_ = source_type;
  has_host_context_menu_location_ = true;
  host_context_menu_location_ = location;
  if (webview())
    webview()->showContextMenu();
  has_host_context_menu_location_ = false;
}

void RenderViewImpl::OnEnableViewSourceMode() {
  if (!webview())
    return;
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;
  main_frame->enableViewSourceMode(true);
}

void RenderViewImpl::OnDisownOpener() {
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (main_frame && main_frame->opener())
    main_frame->setOpener(NULL);
}

#if defined(OS_ANDROID)
bool RenderViewImpl::didTapMultipleTargets(
    const blink::WebGestureEvent& event,
    const WebVector<WebRect>& target_rects) {
  // Never show a disambiguation popup when accessibility is enabled,
  // as this interferes with "touch exploration".
  bool matchesAccessibilityModeComplete =
      (accessibility_mode_ & AccessibilityModeComplete) ==
      AccessibilityModeComplete;
  if (matchesAccessibilityModeComplete)
    return false;

  gfx::Rect finger_rect(
      event.x - event.data.tap.width / 2, event.y - event.data.tap.height / 2,
      event.data.tap.width, event.data.tap.height);
  gfx::Rect zoom_rect;
  float new_total_scale =
      DisambiguationPopupHelper::ComputeZoomAreaAndScaleFactor(
          finger_rect, target_rects, GetSize(),
          gfx::Rect(webview()->mainFrame()->visibleContentRect()).size(),
          device_scale_factor_ * webview()->pageScaleFactor(), &zoom_rect);
  if (!new_total_scale)
    return false;

  bool handled = false;
  switch (renderer_preferences_.tap_multiple_targets_strategy) {
    case TAP_MULTIPLE_TARGETS_STRATEGY_ZOOM:
      handled = webview()->zoomToMultipleTargetsRect(zoom_rect);
      break;
    case TAP_MULTIPLE_TARGETS_STRATEGY_POPUP: {
      gfx::Size canvas_size =
          gfx::ToCeiledSize(gfx::ScaleSize(zoom_rect.size(), new_total_scale));
      cc::SharedBitmapManager* manager =
          RenderThreadImpl::current()->shared_bitmap_manager();
      scoped_ptr<cc::SharedBitmap> shared_bitmap =
          manager->AllocateSharedBitmap(canvas_size);
      {
        SkBitmap bitmap;
        SkImageInfo info = SkImageInfo::MakeN32Premul(canvas_size.width(),
                                                      canvas_size.height());
        bitmap.installPixels(info, shared_bitmap->pixels(), info.minRowBytes());
        SkCanvas canvas(bitmap);

        // TODO(trchen): Cleanup the device scale factor mess.
        // device scale will be applied in WebKit
        // --> zoom_rect doesn't include device scale,
        //     but WebKit will still draw on zoom_rect * device_scale_factor_
        canvas.scale(new_total_scale / device_scale_factor_,
                     new_total_scale / device_scale_factor_);
        canvas.translate(-zoom_rect.x() * device_scale_factor_,
                         -zoom_rect.y() * device_scale_factor_);

        DCHECK(webwidget_->isAcceleratedCompositingActive());
        // TODO(aelias): The disambiguation popup should be composited so we
        // don't have to call this method.
        webwidget_->paintCompositedDeprecated(&canvas, zoom_rect);
      }

      gfx::Rect physical_window_zoom_rect = gfx::ToEnclosingRect(
          ClientRectToPhysicalWindowRect(gfx::RectF(zoom_rect)));
      Send(new ViewHostMsg_ShowDisambiguationPopup(routing_id_,
                                                   physical_window_zoom_rect,
                                                   canvas_size,
                                                   shared_bitmap->id()));
      cc::SharedBitmapId id = shared_bitmap->id();
      disambiguation_bitmaps_[id] = shared_bitmap.release();
      handled = true;
      break;
    }
    case TAP_MULTIPLE_TARGETS_STRATEGY_NONE:
      // No-op.
      break;
  }

  return handled;
}
#endif

unsigned RenderViewImpl::GetLocalSessionHistoryLengthForTesting() const {
  return history_list_length_;
}

void RenderViewImpl::SetFocusAndActivateForTesting(bool enable) {
  if (enable) {
    if (has_focus())
      return;
    OnSetActive(true);
    OnSetFocus(true);
  } else {
    if (!has_focus())
      return;
    OnSetFocus(false);
    OnSetActive(false);
  }
}

void RenderViewImpl::SetDeviceScaleFactorForTesting(float factor) {
  ViewMsg_Resize_Params params;
  params.screen_info = screen_info_;
  params.screen_info.deviceScaleFactor = factor;
  params.new_size = size();
  params.physical_backing_size =
      gfx::ToCeiledSize(gfx::ScaleSize(size(), factor));
  params.overdraw_bottom_height = 0.f;
  params.resizer_rect = WebRect();
  params.is_fullscreen = is_fullscreen();
  OnResize(params);
}

void RenderViewImpl::SetScreenOrientationForTesting(
    const blink::WebScreenOrientationType& orientation) {
  ViewMsg_Resize_Params params;
  params.screen_info = screen_info_;
  params.screen_info.orientationType = orientation;
  // FIXME(ostap): This relationship between orientationType and
  // orientationAngle is temporary. The test should be able to specify
  // the angle in addition to the orientation type.
  switch (orientation) {
    case blink::WebScreenOrientationLandscapePrimary:
      params.screen_info.orientationAngle = 90;
      break;
    case blink::WebScreenOrientationLandscapeSecondary:
      params.screen_info.orientationAngle = -90;
      break;
    case blink::WebScreenOrientationPortraitSecondary:
      params.screen_info.orientationAngle = 180;
      break;
    default:
      params.screen_info.orientationAngle = 0;
  }
  params.new_size = size();
  params.physical_backing_size = gfx::ToCeiledSize(
      gfx::ScaleSize(size(), params.screen_info.deviceScaleFactor));
  params.overdraw_bottom_height = 0.f;
  params.resizer_rect = WebRect();
  params.is_fullscreen = is_fullscreen();
  OnResize(params);
}

void RenderViewImpl::SetDeviceColorProfileForTesting(
    const std::vector<char>& color_profile) {
  SetDeviceColorProfile(color_profile);
}

void RenderViewImpl::ForceResizeForTesting(const gfx::Size& new_size) {
  gfx::Rect new_position(rootWindowRect().x,
                         rootWindowRect().y,
                         new_size.width(),
                         new_size.height());
  ResizeSynchronously(new_position);
}

void RenderViewImpl::UseSynchronousResizeModeForTesting(bool enable) {
  resizing_mode_selector_->set_is_synchronous_mode(enable);
}

void RenderViewImpl::EnableAutoResizeForTesting(const gfx::Size& min_size,
                                                const gfx::Size& max_size) {
  OnEnableAutoResize(min_size, max_size);
}

void RenderViewImpl::DisableAutoResizeForTesting(const gfx::Size& new_size) {
  OnDisableAutoResize(new_size);
}

void RenderViewImpl::OnReleaseDisambiguationPopupBitmap(
    const cc::SharedBitmapId& id) {
  BitmapMap::iterator it = disambiguation_bitmaps_.find(id);
  DCHECK(it != disambiguation_bitmaps_.end());
  delete it->second;
  disambiguation_bitmaps_.erase(it);
}

void RenderViewImpl::DidCommitCompositorFrame() {
  RenderWidget::DidCommitCompositorFrame();
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidCommitCompositorFrame());
}

void RenderViewImpl::SendUpdateFaviconURL(const std::vector<FaviconURL>& urls) {
  if (!urls.empty())
    Send(new ViewHostMsg_UpdateFaviconURL(routing_id_, urls));
}

void RenderViewImpl::DidStopLoadingIcons() {
  int icon_types = WebIconURL::TypeFavicon;
  if (TouchEnabled())
    icon_types |= WebIconURL::TypeTouchPrecomposed | WebIconURL::TypeTouch;

  WebVector<WebIconURL> icon_urls =
      webview()->mainFrame()->iconURLs(icon_types);

  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    WebURL url = icon_urls[i].iconURL();
    std::vector<gfx::Size> sizes;
    ConvertToFaviconSizes(icon_urls[i].sizes(), &sizes);
    if (!url.isEmpty())
      urls.push_back(
          FaviconURL(url, ToFaviconType(icon_urls[i].iconType()), sizes));
  }
  SendUpdateFaviconURL(urls);
}

}  // namespace content

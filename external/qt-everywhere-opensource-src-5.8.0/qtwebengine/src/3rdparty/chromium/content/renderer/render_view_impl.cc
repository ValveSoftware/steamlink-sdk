// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/child/request_extra_data.h"
#include "content/child/v8_value_converter_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/history_controller.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/idle_user_detector.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/navigation_state_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/speech_recognition_dispatcher.h"
#include "content/renderer/text_input_client_observer.h"
#include "content/renderer/web_ui_extension_data.h"
#include "content/renderer/websharedworker_proxy.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_switches.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebConnectionType.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebColorSuggestion.h"
#include "third_party/WebKit/public/web/WebDOMEvent.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFileChooserParams.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameContentDumper.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebPageImportanceSignals.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSearchableFormData.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "third_party/WebKit/public/web/default/WebRenderTheme.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "content/renderer/android/address_detector.h"
#include "content/renderer/android/content_detector.h"
#include "content/renderer/android/disambiguation_popup_helper.h"
#include "content/renderer/android/email_detector.h"
#include "content/renderer/android/phone_number_detector.h"
#include "ui/gfx/geometry/rect_f.h"

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
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#endif

using blink::WebAXObject;
using blink::WebApplicationCacheHost;
using blink::WebApplicationCacheHostClient;
using blink::WebColor;
using blink::WebConsoleMessage;
using blink::WebData;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebDragData;
using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebElement;
using blink::WebFileChooserCompletion;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebFrameContentDumper;
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
using blink::WebPeerConnection00Handler;
using blink::WebPeerConnection00HandlerClient;
using blink::WebPeerConnectionHandler;
using blink::WebPeerConnectionHandlerClient;
using blink::WebPluginAction;
using blink::WebPoint;
using blink::WebRange;
using blink::WebRect;
using blink::WebReferrerPolicy;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSettings;
using blink::WebSize;
using blink::WebStorageNamespace;
using blink::WebStorageQuotaCallbacks;
using blink::WebStorageQuotaError;
using blink::WebStorageQuotaType;
using blink::WebString;
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
using blink::WebRuntimeFeatures;
using base::Time;
using base::TimeDelta;

#if defined(OS_ANDROID)
using blink::WebContentDetectionResult;
using blink::WebHitTestResult;
#endif

namespace content {

//-----------------------------------------------------------------------------

typedef std::map<blink::WebView*, RenderViewImpl*> ViewMap;
static base::LazyInstance<ViewMap> g_view_map = LAZY_INSTANCE_INITIALIZER;
typedef std::map<int32_t, RenderViewImpl*> RoutingIDViewMap;
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

static RenderViewImpl* (*g_create_render_view_impl)(
    CompositorDependencies* compositor_deps,
    const ViewMsg_New_Params&) = nullptr;

// static
Referrer RenderViewImpl::GetReferrerFromRequest(
    WebFrame* frame,
    const WebURLRequest& request) {
  return Referrer(
      blink::WebStringToGURL(request.httpHeaderField(
          WebString::fromUTF8("Referer"))),
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
  // 1.5 is a common touchscreen tablet device scale factor. For such
  // devices main thread antialiasing is a heavy burden.
  return device_scale_factor >= 1.5f;
#endif
}

static bool PreferCompositingToLCDText(CompositorDependencies* compositor_deps,
                                       float device_scale_factor) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisablePreferCompositingToLCDText))
    return false;
  if (command_line.HasSwitch(switches::kEnablePreferCompositingToLCDText))
    return true;
  if (!compositor_deps->IsLcdTextEnabled())
    return true;
  return DeviceScaleEnsuresTextQuality(device_scale_factor);
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

namespace {

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

typedef void (*SetFontFamilyWrapper)(blink::WebSettings*,
                                     const base::string16&,
                                     UScriptCode);

void SetStandardFontFamilyWrapper(WebSettings* settings,
                                  const base::string16& font,
                                  UScriptCode script) {
  settings->setStandardFontFamily(font, script);
}

void SetFixedFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->setFixedFontFamily(font, script);
}

void SetSerifFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->setSerifFontFamily(font, script);
}

void SetSansSerifFontFamilyWrapper(WebSettings* settings,
                                   const base::string16& font,
                                   UScriptCode script) {
  settings->setSansSerifFontFamily(font, script);
}

void SetCursiveFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->setCursiveFontFamily(font, script);
}

void SetFantasyFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->setFantasyFontFamily(font, script);
}

void SetPictographFontFamilyWrapper(WebSettings* settings,
                                    const base::string16& font,
                                    UScriptCode script) {
  settings->setPictographFontFamily(font, script);
}

// If |scriptCode| is a member of a family of "similar" script codes, returns
// the script code in that family that is used by WebKit for font selection
// purposes.  For example, USCRIPT_KATAKANA_OR_HIRAGANA and USCRIPT_JAPANESE are
// considered equivalent for the purposes of font selection.  WebKit uses the
// script code USCRIPT_KATAKANA_OR_HIRAGANA.  So, if |scriptCode| is
// USCRIPT_JAPANESE, the function returns USCRIPT_KATAKANA_OR_HIRAGANA.  WebKit
// uses different scripts than the ones in Chrome pref names because the version
// of ICU included on certain ports does not have some of the newer scripts.  If
// |scriptCode| is not a member of such a family, returns |scriptCode|.
UScriptCode GetScriptForWebSettings(UScriptCode scriptCode) {
  switch (scriptCode) {
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA:
    case USCRIPT_JAPANESE:
      return USCRIPT_KATAKANA_OR_HIRAGANA;
    case USCRIPT_KOREAN:
      return USCRIPT_HANGUL;
    default:
      return scriptCode;
  }
}

void ApplyFontsFromMap(const ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       WebSettings* settings) {
  for (ScriptFontFamilyMap::const_iterator it = map.begin(); it != map.end();
       ++it) {
    int32_t script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it->first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
      UScriptCode code = static_cast<UScriptCode>(script);
      (*setter)(settings, it->second, GetScriptForWebSettings(code));
    }
  }
}

void ApplyBlinkSettings(const base::CommandLine& command_line,
                        WebSettings* settings) {
  if (!command_line.HasSwitch(switches::kBlinkSettings))
    return;

  std::vector<std::string> blink_settings = base::SplitString(
      command_line.GetSwitchValueASCII(switches::kBlinkSettings),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& setting : blink_settings) {
    size_t pos = setting.find('=');
    settings->setFromStrings(
        blink::WebString::fromLatin1(setting.substr(0, pos)),
        blink::WebString::fromLatin1(
            pos == std::string::npos ? "" : setting.substr(pos + 1)));
  }
}

WebSettings::V8CacheStrategiesForCacheStorage
GetV8CacheStrategiesForCacheStorage() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string v8_cache_strategies = command_line.GetSwitchValueASCII(
      switches::kV8CacheStrategiesForCacheStorage);
  if (v8_cache_strategies.empty()) {
    v8_cache_strategies =
        base::FieldTrialList::FindFullName("V8CacheStrategiesForCacheStorage");
  }

  if (base::StartsWith(v8_cache_strategies, "none",
                       base::CompareCase::SENSITIVE)) {
    return WebSettings::V8CacheStrategiesForCacheStorage::None;
  } else if (base::StartsWith(v8_cache_strategies, "normal",
                              base::CompareCase::SENSITIVE)) {
    return WebSettings::V8CacheStrategiesForCacheStorage::Normal;
  } else if (base::StartsWith(v8_cache_strategies, "aggressive",
                              base::CompareCase::SENSITIVE)) {
    return WebSettings::V8CacheStrategiesForCacheStorage::Aggressive;
  } else {
    return WebSettings::V8CacheStrategiesForCacheStorage::Default;
  }
}

}  // namespace

RenderViewImpl::RenderViewImpl(CompositorDependencies* compositor_deps,
                               const ViewMsg_New_Params& params)
    : RenderWidget(compositor_deps,
                   blink::WebPopupTypeNone,
                   params.initial_size.screen_info,
                   params.swapped_out,
                   params.hidden,
                   params.never_visible),
      webkit_preferences_(params.web_preferences),
      send_content_state_immediately_(false),
      enabled_bindings_(0),
      send_preferred_size_changes_(false),
      navigation_gesture_(NavigationGestureUnknown),
      opened_by_user_gesture_(true),
      page_id_(-1),
      next_page_id_(params.next_page_id),
      history_list_offset_(-1),
      history_list_length_(0),
      frames_in_progress_(0),
      target_url_status_(TARGET_NONE),
      uses_temporary_zoom_level_(false),
#if defined(OS_ANDROID)
      top_controls_constraints_(TOP_CONTROLS_STATE_BOTH),
#endif
      top_controls_shrink_blink_size_(false),
      top_controls_height_(0.f),
      webview_(nullptr),
      has_scrolled_focused_editable_node_into_rect_(false),
      page_zoom_level_(params.page_zoom_level),
      main_render_frame_(nullptr),
      frame_widget_(nullptr),
      speech_recognition_dispatcher_(NULL),
#if defined(OS_ANDROID)
      expected_content_intent_id_(0),
#endif
      enumeration_completion_id_(0),
      session_storage_namespace_id_(params.session_storage_namespace_id),
      has_added_input_handler_(false) {
  GetWidget()->set_owner_delegate(this);
}

void RenderViewImpl::Initialize(const ViewMsg_New_Params& params,
                                bool was_created_by_renderer) {
  SetRoutingID(params.view_id);

  int opener_view_routing_id;
  WebFrame* opener_frame = RenderFrameImpl::ResolveOpener(
      params.opener_frame_route_id, &opener_view_routing_id);
  if (opener_view_routing_id != MSG_ROUTING_NONE && was_created_by_renderer)
    opener_id_ = opener_view_routing_id;

  display_mode_ = params.initial_size.display_mode;

  // Ensure we start with a valid next_page_id_ from the browser.
  DCHECK_GE(next_page_id_, 0);

  webview_ =
      WebView::create(this, is_hidden() ? blink::WebPageVisibilityStateHidden
                                        : blink::WebPageVisibilityStateVisible);
  RenderWidget::DoInit(MSG_ROUTING_NONE, webview_->widget(), nullptr);

  g_view_map.Get().insert(std::make_pair(webview(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(GetRoutingID(), this));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kStatsCollectionController))
    stats_collection_observer_.reset(new StatsCollectionObserver(this));

  // Debug cases of https://crbug.com/575245.
  base::debug::SetCrashKeyValue("rvinit_view_id",
                                base::IntToString(GetRoutingID()));
  base::debug::SetCrashKeyValue("rvinit_proxy_id",
                                base::IntToString(params.proxy_routing_id));
  base::debug::SetCrashKeyValue(
      "rvinit_main_frame_id", base::IntToString(params.main_frame_routing_id));

  webview()->setDisplayMode(display_mode_);
  webview()->settings()->setPreferCompositingToLCDTextEnabled(
      PreferCompositingToLCDText(compositor_deps_, device_scale_factor_));
  webview()->settings()->setThreadedScrollingEnabled(
      !command_line.HasSwitch(switches::kDisableThreadedScrolling));
  webview()->settings()->setRootLayerScrolls(
      command_line.HasSwitch(switches::kRootLayerScrolls));
  webview()->setShowFPSCounter(
      command_line.HasSwitch(cc::switches::kShowFPSCounter));
  webview()->setDeviceColorProfile(params.image_decode_color_profile);

  ApplyWebPreferencesInternal(webkit_preferences_, webview(), compositor_deps_);

  if (switches::IsTouchDragDropEnabled())
    webview()->settings()->setTouchDragDropEnabled(true);

  WebSettings::SelectionStrategyType selection_strategy =
      WebSettings::SelectionStrategyType::Character;
  const std::string selection_strategy_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTouchTextSelectionStrategy);
  if (selection_strategy_str == "direction")
    selection_strategy = WebSettings::SelectionStrategyType::Direction;
  webview()->settings()->setSelectionStrategy(selection_strategy);

  std::string passiveListenersDefault =
      command_line.GetSwitchValueASCII(switches::kPassiveListenersDefault);
  if (!passiveListenersDefault.empty()) {
    WebSettings::PassiveEventListenerDefault passiveDefault =
        WebSettings::PassiveEventListenerDefault::False;
    if (passiveListenersDefault == "documentonlytrue")
      passiveDefault = WebSettings::PassiveEventListenerDefault::DocumentTrue;
    else if (passiveListenersDefault == "true")
      passiveDefault = WebSettings::PassiveEventListenerDefault::True;
    else if (passiveListenersDefault == "forcealltrue")
      passiveDefault = WebSettings::PassiveEventListenerDefault::ForceAllTrue;
    webview()->settings()->setPassiveEventListenerDefault(passiveDefault);
  }

  ApplyBlinkSettings(command_line, webview()->settings());

  if (params.main_frame_routing_id != MSG_ROUTING_NONE) {
    main_render_frame_ = RenderFrameImpl::CreateMainFrame(
        this, params.main_frame_routing_id, params.main_frame_widget_routing_id,
        params.hidden, screen_info(), compositor_deps_, opener_frame);
  }

  if (params.proxy_routing_id != MSG_ROUTING_NONE) {
    CHECK(params.swapped_out);
    RenderFrameProxy::CreateFrameProxy(
        params.proxy_routing_id, GetRoutingID(), params.opener_frame_route_id,
        MSG_ROUTING_NONE, params.replicated_frame_state);
  }

  if (main_render_frame_)
    main_render_frame_->Initialize();

#if defined(OS_ANDROID)
  content_detectors_.push_back(base::WrapUnique(new AddressDetector()));
  const std::string& contry_iso =
      params.renderer_preferences.network_contry_iso;
  if (!contry_iso.empty()) {
    content_detectors_.push_back(
        base::WrapUnique(new PhoneNumberDetector(contry_iso)));
  }
  content_detectors_.push_back(base::WrapUnique(new EmailDetector()));
#endif

  // If this is a popup, we must wait for the CreatingNew_ACK message before
  // completing initialization.  Otherwise, we can finish it now.
  if (opener_id_ == MSG_ROUTING_NONE)
    did_show_ = true;

  // Set the main frame's name.  Only needs to be done for WebLocalFrames,
  // since the remote case was handled as part of SetReplicatedState on the
  // proxy above.
  if (!params.replicated_frame_state.name.empty() &&
      webview()->mainFrame()->isWebLocalFrame()) {
    webview()->mainFrame()->setName(
        blink::WebString::fromUTF8(params.replicated_frame_state.name));
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params.window_was_created_with_opener)
    webview()->setOpenedByDOM();

  UpdateWebViewWithDeviceScaleFactor();
  OnSetRendererPrefs(params.renderer_preferences);

  if (!params.enable_auto_resize) {
    OnResize(params.initial_size);
  } else {
    OnEnableAutoResize(params.min_size, params.max_size);
  }

#if defined(OS_MACOSX)
  new TextInputClientObserver(this);
#endif  // defined(OS_MACOSX)

  // We don't use HistoryController in OOPIF-enabled modes.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    history_controller_.reset(new HistoryController(this));

  new IdleUserDetector(this);

  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BINDINGS_POLICY_DOM_AUTOMATION;
  if (command_line.HasSwitch(switches::kStatsCollectionController))
    enabled_bindings_ |= BINDINGS_POLICY_STATS_COLLECTION;

  GetContentClient()->renderer()->RenderViewCreated(this);

  // Ensure that sandbox flags are inherited from an opener in a different
  // process.  In that case, the browser process will set any inherited sandbox
  // flags in |replicated_frame_state|, so apply them here.
  if (!was_created_by_renderer && webview()->mainFrame()->isWebLocalFrame()) {
    webview()->mainFrame()->toWebLocalFrame()->forceSandboxFlags(
        params.replicated_frame_state.sandbox_flags);
  }

  page_zoom_level_ = params.page_zoom_level;
}

RenderViewImpl::~RenderViewImpl() {
  DCHECK(!frame_widget_);

  for (BitmapMap::iterator it = disambiguation_bitmaps_.begin();
       it != disambiguation_bitmaps_.end();
       ++it)
    delete it->second;

#if defined(OS_ANDROID)
  // The date/time picker client is both a std::unique_ptr member of this class
  // and a RenderViewObserver. Reset it to prevent double deletion.
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
RenderViewImpl* RenderViewImpl::FromRoutingID(int32_t routing_id) {
  RoutingIDViewMap* views = g_routing_id_view_map.Pointer();
  RoutingIDViewMap::iterator it = views->find(routing_id);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromRoutingID(int routing_id) {
  return RenderViewImpl::FromRoutingID(routing_id);
}

/* static */
size_t RenderView::GetRenderViewCount() {
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
void RenderView::ApplyWebPreferences(const WebPreferences& prefs,
                                     WebView* web_view) {
  WebSettings* settings = web_view->settings();
  ApplyFontsFromMap(prefs.standard_font_family_map,
                    SetStandardFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fixed_font_family_map,
                    SetFixedFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.serif_font_family_map,
                    SetSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.sans_serif_font_family_map,
                    SetSansSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.cursive_font_family_map,
                    SetCursiveFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fantasy_font_family_map,
                    SetFantasyFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.pictograph_font_family_map,
                    SetPictographFontFamilyWrapper, settings);
  settings->setDefaultFontSize(prefs.default_font_size);
  settings->setDefaultFixedFontSize(prefs.default_fixed_font_size);
  settings->setMinimumFontSize(prefs.minimum_font_size);
  settings->setMinimumLogicalFontSize(prefs.minimum_logical_font_size);
  settings->setDefaultTextEncodingName(
      base::ASCIIToUTF16(prefs.default_encoding));
  settings->setJavaScriptEnabled(prefs.javascript_enabled);
  settings->setWebSecurityEnabled(prefs.web_security_enabled);
  settings->setJavaScriptCanOpenWindowsAutomatically(
      prefs.javascript_can_open_windows_automatically);
  settings->setLoadsImagesAutomatically(prefs.loads_images_automatically);
  settings->setImagesEnabled(prefs.images_enabled);
  settings->setPluginsEnabled(prefs.plugins_enabled);
  settings->setDOMPasteAllowed(prefs.dom_paste_enabled);
  settings->setUsesEncodingDetector(prefs.uses_universal_detector);
  settings->setTextAreasAreResizable(prefs.text_areas_are_resizable);
  settings->setAllowScriptsToCloseWindows(prefs.allow_scripts_to_close_windows);
  settings->setDownloadableBinaryFontsEnabled(prefs.remote_fonts_enabled);
  settings->setJavaScriptCanAccessClipboard(
      prefs.javascript_can_access_clipboard);
  WebRuntimeFeatures::enableXSLT(prefs.xslt_enabled);
  settings->setXSSAuditorEnabled(prefs.xss_auditor_enabled);
  settings->setDNSPrefetchingEnabled(prefs.dns_prefetching_enabled);
  settings->setDataSaverEnabled(prefs.data_saver_enabled);
  settings->setLocalStorageEnabled(prefs.local_storage_enabled);
  settings->setSyncXHRInDocumentsEnabled(prefs.sync_xhr_in_documents_enabled);
  WebRuntimeFeatures::enableDatabase(prefs.databases_enabled);
  settings->setOfflineWebApplicationCacheEnabled(
      prefs.application_cache_enabled);
  settings->setCaretBrowsingEnabled(prefs.caret_browsing_enabled);
  settings->setHyperlinkAuditingEnabled(prefs.hyperlink_auditing_enabled);
  settings->setCookieEnabled(prefs.cookie_enabled);
  settings->setNavigateOnDragDrop(prefs.navigate_on_drag_drop);

  // By default, allow_universal_access_from_file_urls is set to false and thus
  // we mitigate attacks from local HTML files by not granting file:// URLs
  // universal access. Only test shell will enable this.
  settings->setAllowUniversalAccessFromFileURLs(
      prefs.allow_universal_access_from_file_urls);
  settings->setAllowFileAccessFromFileURLs(
      prefs.allow_file_access_from_file_urls);

  // Enable experimental WebGL support if requested on command line
  // and support is compiled in.
  settings->setExperimentalWebGLEnabled(prefs.experimental_webgl_enabled);

  // Enable WebGL errors to the JS console if requested.
  settings->setWebGLErrorsToConsoleEnabled(
      prefs.webgl_errors_to_console_enabled);

  // Uses the mock theme engine for scrollbars.
  settings->setMockScrollbarsEnabled(prefs.mock_scrollbars_enabled);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  settings->setAccelerated2dCanvasEnabled(prefs.accelerated_2d_canvas_enabled);

  settings->setMinimumAccelerated2dCanvasSize(
      prefs.minimum_accelerated_2d_canvas_size);

  // Disable antialiasing for 2d canvas if requested on the command line.
  settings->setAntialiased2dCanvasEnabled(
      !prefs.antialiased_2d_canvas_disabled);
  WebRuntimeFeatures::forceDisable2dCanvasCopyOnWrite(
      prefs.disable_2d_canvas_copy_on_write);

  // Enabled antialiasing of clips for 2d canvas if requested on the command
  // line.
  settings->setAntialiasedClips2dCanvasEnabled(
      prefs.antialiased_clips_2d_canvas_enabled);

  // Set MSAA sample count for 2d canvas if requested on the command line (or
  // default value if not).
  settings->setAccelerated2dCanvasMSAASampleCount(
      prefs.accelerated_2d_canvas_msaa_sample_count);

  settings->setUnifiedTextCheckerEnabled(prefs.unified_textchecker_enabled);

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view->setTabsToLinks(prefs.tabs_to_links);

  settings->setAllowDisplayOfInsecureContent(
      prefs.allow_displaying_insecure_content);
  settings->setAllowRunningOfInsecureContent(
      prefs.allow_running_insecure_content);
  settings->setDisableReadingFromCanvas(prefs.disable_reading_from_canvas);
  settings->setStrictMixedContentChecking(prefs.strict_mixed_content_checking);

  settings->setStrictlyBlockBlockableMixedContent(
      prefs.strictly_block_blockable_mixed_content);

  settings->setStrictMixedContentCheckingForPlugin(
      prefs.block_mixed_plugin_content);

  settings->setStrictPowerfulFeatureRestrictions(
      prefs.strict_powerful_feature_restrictions);
  settings->setAllowGeolocationOnInsecureOrigins(
      prefs.allow_geolocation_on_insecure_origins);
  settings->setPasswordEchoEnabled(prefs.password_echo_enabled);
  settings->setShouldPrintBackgrounds(prefs.should_print_backgrounds);
  settings->setShouldClearDocumentBackground(
      prefs.should_clear_document_background);
  settings->setEnableScrollAnimator(prefs.enable_scroll_animator);

  WebRuntimeFeatures::enableTouch(prefs.touch_enabled);
  settings->setMaxTouchPoints(prefs.pointer_events_max_touch_points);
  settings->setAvailablePointerTypes(prefs.available_pointer_types);
  settings->setPrimaryPointerType(
      static_cast<blink::PointerType>(prefs.primary_pointer_type));
  settings->setAvailableHoverTypes(prefs.available_hover_types);
  settings->setPrimaryHoverType(
      static_cast<blink::HoverType>(prefs.primary_hover_type));
  settings->setDeviceSupportsTouch(prefs.device_supports_touch);
  settings->setDeviceSupportsMouse(prefs.device_supports_mouse);
  settings->setEnableTouchAdjustment(prefs.touch_adjustment_enabled);

  WebRuntimeFeatures::enableImageColorProfiles(
      prefs.image_color_profiles_enabled);
  settings->setShouldRespectImageOrientation(
      prefs.should_respect_image_orientation);

  settings->setEditingBehavior(
      static_cast<WebSettings::EditingBehavior>(prefs.editing_behavior));

  settings->setSupportsMultipleWindows(prefs.supports_multiple_windows);

  settings->setInertVisualViewport(prefs.inert_visual_viewport);

  settings->setMainFrameClipsContent(!prefs.record_whole_document);

  settings->setSmartInsertDeleteEnabled(prefs.smart_insert_delete_enabled);

  settings->setSpatialNavigationEnabled(prefs.spatial_navigation_enabled);

  settings->setSelectionIncludesAltImageText(true);

  settings->setV8CacheOptions(
      static_cast<WebSettings::V8CacheOptions>(prefs.v8_cache_options));

  settings->setV8CacheStrategiesForCacheStorage(
      GetV8CacheStrategiesForCacheStorage());

  settings->setImageAnimationPolicy(
      static_cast<WebSettings::ImageAnimationPolicy>(prefs.animation_policy));

  settings->setPresentationRequiresUserGesture(
      prefs.user_gesture_required_for_presentation);

  settings->setTextTrackMarginPercentage(prefs.text_track_margin_percentage);

  // Needs to happen before setIgnoreVIewportTagScaleLimits below.
  web_view->setDefaultPageScaleLimits(
      prefs.default_minimum_page_scale_factor,
      prefs.default_maximum_page_scale_factor);

#if defined(OS_ANDROID)
  settings->setAllowCustomScrollbarInMainFrame(false);
  settings->setTextAutosizingEnabled(prefs.text_autosizing_enabled);
  settings->setAccessibilityFontScaleFactor(prefs.font_scale_factor);
  settings->setDeviceScaleAdjustment(prefs.device_scale_adjustment);
  settings->setFullscreenSupported(prefs.fullscreen_supported);
  web_view->setIgnoreViewportTagScaleLimits(prefs.force_enable_zoom);
  settings->setAutoZoomFocusedNodeToLegibleScale(true);
  settings->setDoubleTapToZoomEnabled(prefs.double_tap_to_zoom_enabled);
  settings->setMediaControlsOverlayPlayButtonEnabled(true);
  settings->setMediaPlaybackRequiresUserGesture(
      prefs.user_gesture_required_for_media_playback);
  settings->setDefaultVideoPosterURL(
        base::ASCIIToUTF16(prefs.default_video_poster_url.spec()));
  settings->setSupportDeprecatedTargetDensityDPI(
      prefs.support_deprecated_target_density_dpi);
  settings->setUseLegacyBackgroundSizeShorthandBehavior(
      prefs.use_legacy_background_size_shorthand_behavior);
  settings->setWideViewportQuirkEnabled(prefs.wide_viewport_quirk);
  settings->setUseWideViewport(prefs.use_wide_viewport);
  settings->setForceZeroLayoutHeight(prefs.force_zero_layout_height);
  settings->setViewportMetaLayoutSizeQuirk(
      prefs.viewport_meta_layout_size_quirk);
  settings->setViewportMetaMergeContentQuirk(
      prefs.viewport_meta_merge_content_quirk);
  settings->setViewportMetaNonUserScalableQuirk(
      prefs.viewport_meta_non_user_scalable_quirk);
  settings->setViewportMetaZeroValuesQuirk(
      prefs.viewport_meta_zero_values_quirk);
  settings->setClobberUserAgentInitialScaleQuirk(
      prefs.clobber_user_agent_initial_scale_quirk);
  settings->setIgnoreMainFrameOverflowHiddenQuirk(
      prefs.ignore_main_frame_overflow_hidden_quirk);
  settings->setReportScreenSizeInPhysicalPixelsQuirk(
      prefs.report_screen_size_in_physical_pixels_quirk);
  settings->setShouldReuseGlobalForUnownedMainFrame(
      prefs.resue_global_for_unowned_main_frame);
  settings->setProgressBarCompletion(
      static_cast<WebSettings::ProgressBarCompletion>(
          prefs.progress_bar_completion));
  settings->setPreferHiddenVolumeControls(true);
  WebRuntimeFeatures::enableAutoplayMutedVideos(
      prefs.autoplay_muted_videos_enabled);
#elif defined(TOOLKIT_QT)
  settings->setFullscreenSupported(prefs.fullscreen_supported);
#endif

  settings->setAutoplayExperimentMode(
      blink::WebString::fromUTF8(prefs.autoplay_experiment_mode));

  settings->setViewportEnabled(prefs.viewport_enabled);
  settings->setViewportMetaEnabled(prefs.viewport_meta_enabled);
  settings->setShrinksViewportContentToFit(
        prefs.shrinks_viewport_contents_to_fit);
  settings->setViewportStyle(
      static_cast<blink::WebViewportStyle>(prefs.viewport_style));

  settings->setLoadWithOverviewMode(prefs.initialize_at_minimum_page_scale);
  settings->setMainFrameResizesAreOrientationChanges(
      prefs.main_frame_resizes_are_orientation_changes);

  settings->setPinchOverlayScrollbarThickness(
      prefs.pinch_overlay_scrollbar_thickness);
  settings->setUseSolidColorScrollbars(prefs.use_solid_color_scrollbars);

  settings->setShowContextMenuOnMouseUp(prefs.context_menu_on_mouse_up);
  settings->setAlwaysShowContextMenuOnTouch(
      prefs.always_show_context_menu_on_touch);

#if defined(OS_MACOSX)
  settings->setDoubleTapToZoomEnabled(true);
  web_view->setMaximumLegibleScale(prefs.default_maximum_page_scale_factor);
#endif
}

/*static*/
RenderViewImpl* RenderViewImpl::Create(CompositorDependencies* compositor_deps,
                                       const ViewMsg_New_Params& params,
                                       bool was_created_by_renderer) {
  DCHECK(params.view_id != MSG_ROUTING_NONE);
  RenderViewImpl* render_view = NULL;
  if (g_create_render_view_impl)
    render_view = g_create_render_view_impl(compositor_deps, params);
  else
    render_view = new RenderViewImpl(compositor_deps, params);

  render_view->Initialize(params, was_created_by_renderer);
  return render_view;
}

// static
void RenderViewImpl::InstallCreateHook(RenderViewImpl* (
    *create_render_view_impl)(CompositorDependencies* compositor_deps,
                              const ViewMsg_New_Params&)) {
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
  return webview_;
}

#if defined(ENABLE_PLUGINS)

#if defined(OS_MACOSX)
void RenderViewImpl::OnGetRenderedText() {
  if (!webview())
    return;

  if (!webview()->mainFrame()->isWebLocalFrame())
    return;

  // Get rendered text from WebLocalFrame.
  // TODO: Currently IPC truncates any data that has a
  // size > kMaximumMessageSize. May be split the text into smaller chunks and
  // send back using multiple IPC. See http://crbug.com/393444.
  static const size_t kMaximumMessageSize = 8 * 1024 * 1024;
  // TODO(dglazkov): Using this API is wrong. It's not OOPIF-compatible and
  // sends text in the wrong order. See http://crbug.com/584798.
  // TODO(dglazkov): WebFrameContentDumper should only be used for
  // testing purposes. See http://crbug.com/585164.
  std::string text =
      WebFrameContentDumper::dumpWebViewAsText(webview(), kMaximumMessageSize)
          .utf8();

  Send(new ViewMsg_GetRenderedTextCompleted(GetRoutingID(), text));
}
#endif  // defined(OS_MACOSX)

#endif  // ENABLE_PLUGINS

void RenderViewImpl::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  if (webview())
    webview()->transferActiveWheelFlingAnimation(params);
}

// RenderWidgetInputHandlerDelegate -----------------------------------------

void RenderViewImpl::RenderWidgetFocusChangeComplete() {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FocusChangeComplete());
}

bool RenderViewImpl::DoesRenderWidgetHaveTouchEventHandlersAt(
    const gfx::Point& point) const {
  if (!webview())
    return false;
  return webview()->hasTouchEventHandlersAt(point);
}

void RenderViewImpl::RenderWidgetDidHandleKeyEvent() {
  ClearEditCommands();
}

bool RenderViewImpl::RenderWidgetWillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);
  return false;
}

bool RenderViewImpl::RenderWidgetWillHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  possible_drag_event_info_.event_source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  possible_drag_event_info_.event_location =
      gfx::Point(event.globalX, event.globalY);

  // If the mouse is locked, only the current owner of the mouse lock can
  // process mouse events.
  return mouse_lock_dispatcher_->WillHandleMouseEvent(event);
}

// IPC::Listener implementation ----------------------------------------------

bool RenderViewImpl::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (main_frame && main_frame->isWebLocalFrame())
    GetContentClient()->SetActiveURL(main_frame->document().url());

  // Input IPC messages must not be processed if the RenderView is in
  // swapped out state.
  if (is_swapped_out_ && IPC_MESSAGE_ID_CLASS(message.type()) == InputMsgStart)
    return false;

  base::ObserverListBase<RenderViewObserver>::Iterator it(&observers_);
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
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageScale, OnSetPageScale)
    IPC_MESSAGE_HANDLER(ViewMsg_Zoom, OnZoom)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForLoadingURL,
                        OnSetZoomLevelForLoadingURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageEncoding, OnSetPageEncoding)
    IPC_MESSAGE_HANDLER(ViewMsg_ResetPageEncodingToDefault,
                        OnResetPageEncodingToDefault)
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
    IPC_MESSAGE_HANDLER(ViewMsg_ShowContextMenu, OnShowContextMenu)
    // TODO(viettrungluu): Move to a separate message filter.
    IPC_MESSAGE_HANDLER(ViewMsg_SetHistoryOffsetAndLength,
                        OnSetHistoryOffsetAndLength)
    IPC_MESSAGE_HANDLER(ViewMsg_ReleaseDisambiguationPopupBitmap,
                        OnReleaseDisambiguationPopupBitmap)
    IPC_MESSAGE_HANDLER(ViewMsg_ForceRedraw, OnForceRedraw)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectWordAroundCaret, OnSelectWordAroundCaret)

    // Page messages.
    IPC_MESSAGE_HANDLER(PageMsg_UpdateWindowScreenRect,
                        OnUpdateWindowScreenRect)
    IPC_MESSAGE_HANDLER(PageMsg_SetZoomLevel, OnSetZoomLevel)
    IPC_MESSAGE_HANDLER(PageMsg_WasHidden, OnPageWasHidden)
    IPC_MESSAGE_HANDLER(PageMsg_WasShown, OnPageWasShown)

#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTopControlsState,
                        OnUpdateTopControlsState)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtractSmartClipData, OnExtractSmartClipData)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_GetRenderedText,
                        OnGetRenderedText)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
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

  input_handler_->set_handling_input_event(true);
  webview()->focusedFrame()->selectWordAroundCaret();
  input_handler_->set_handling_input_event(false);
}

void RenderViewImpl::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING)
    Send(new ViewHostMsg_UpdateTargetURL(GetRoutingID(), pending_target_url_));

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

  Send(new InputHostMsg_MoveCaret_ACK(GetRoutingID()));
  webview()->focusedFrame()->moveCaretSelection(
      ConvertWindowPointToViewport(point));
}

void RenderViewImpl::OnScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  if (has_scrolled_focused_editable_node_into_rect_ &&
      rect == rect_for_scrolled_focused_editable_node_) {
    GetWidget()->FocusChangeComplete();
    return;
  }

  if (!webview()->scrollFocusedEditableElementIntoRect(rect))
    return;

  rect_for_scrolled_focused_editable_node_ = rect;
  has_scrolled_focused_editable_node_into_rect_ = true;
  if (!compositor()->hasPendingPageScaleAnimation())
    GetWidget()->FocusChangeComplete();
}

void RenderViewImpl::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderViewImpl::OnSetHistoryOffsetAndLength(int history_offset,
                                                 int history_length) {
  DCHECK_GE(history_offset, -1);
  DCHECK_GE(history_length, 0);

  history_list_offset_ = history_offset;
  history_list_length_ = history_length;
}

void RenderViewImpl::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->setInitialFocus(reverse);
}

void RenderViewImpl::OnUpdateWindowScreenRect(gfx::Rect window_screen_rect) {
  RenderWidget::OnUpdateWindowScreenRect(window_screen_rect);
}

///////////////////////////////////////////////////////////////////////////////

void RenderViewImpl::SendUpdateState() {
  // We don't use this path in OOPIF-enabled modes.
  DCHECK(!SiteIsolationPolicy::UseSubframeNavigationEntries());

  HistoryEntry* entry = history_controller_->GetCurrentEntry();
  if (!entry)
    return;

  Send(new ViewHostMsg_UpdateState(GetRoutingID(), page_id_,
                                   HistoryEntryToPageState(entry)));
}

void RenderViewImpl::SendFrameStateUpdates() {
  // We only use this path in OOPIF-enabled modes.
  DCHECK(SiteIsolationPolicy::UseSubframeNavigationEntries());

  // Tell each frame with pending state to send its UpdateState message.
  for (int render_frame_routing_id : frames_with_pending_state_) {
    RenderFrameImpl* frame =
        RenderFrameImpl::FromRoutingID(render_frame_routing_id);
    if (frame)
      frame->SendUpdateState();
  }
  frames_with_pending_state_.clear();
}

void RenderViewImpl::ApplyWebPreferencesInternal(
    const WebPreferences& prefs,
    blink::WebView* web_view,
    CompositorDependencies* compositor_deps) {
  ApplyWebPreferences(prefs, web_view);
}

void RenderViewImpl::OnForceRedraw(int id) {
  ui::LatencyInfo latency_info;
  if (id) {
    latency_info.AddLatencyNumber(ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT,
                                  0,
                                  id);
  }
  std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  if (RenderWidgetCompositor* rwc = compositor()) {
    latency_info_swap_promise_monitor =
        rwc->CreateLatencyInfoSwapPromiseMonitor(&latency_info);
  }
  ScheduleCompositeWithForcedRedraw();
}

// blink::WebViewClient ------------------------------------------------------

WebView* RenderViewImpl::createView(WebLocalFrame* creator,
                                    const WebURLRequest& request,
                                    const WebWindowFeatures& features,
                                    const WebString& frame_name,
                                    WebNavigationPolicy policy,
                                    bool suppress_opener) {
  ViewHostMsg_CreateWindow_Params params;
  params.opener_id = GetRoutingID();
  params.user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  if (GetContentClient()->renderer()->AllowPopup())
    params.user_gesture = true;
  params.window_container_type = WindowFeaturesToContainerType(features);
  params.session_storage_namespace_id = session_storage_namespace_id_;
  if (frame_name != "_blank")
    params.frame_name = base::UTF16ToUTF8(base::StringPiece16(frame_name));
  params.opener_render_frame_id =
      RenderFrameImpl::FromWebFrame(creator)->GetRoutingID();
  params.opener_url = creator->document().url();

  // The browser process uses the top frame's URL for a content settings check
  // to determine whether the popup is allowed.  If the top frame is remote,
  // its URL is not available, so use its replicated origin instead.
  //
  // TODO(alexmos): This works fine for regular origins but may break path
  // matching for file URLs with OOP subframes that open popups.  This should
  // be fixed by either moving this lookup to the browser process or removing
  // path-based matching for file URLs from content settings.  See
  // https://crbug.com/466297.
  if (creator->top()->isWebLocalFrame()) {
    params.opener_top_level_frame_url = creator->top()->document().url();
  } else {
    params.opener_top_level_frame_url =
        blink::WebStringToGURL(creator->top()->getSecurityOrigin().toString());
  }

  GURL security_url(blink::WebStringToGURL(
      creator->document().getSecurityOrigin().toString()));
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

  ViewHostMsg_CreateWindow_Reply reply;
  RenderThread::Get()->Send(new ViewHostMsg_CreateWindow(params, &reply));
  if (reply.route_id == MSG_ROUTING_NONE)
    return NULL;

  WebUserGestureIndicator::consumeUserGesture();

  // While this view may be a background extension page, it can spawn a visible
  // render view. So we just assume that the new one is not another background
  // page instead of passing on our own value.
  // TODO(vangelis): Can we tell if the new view will be a background page?
  bool never_visible = false;

  ResizeParams initial_size = ResizeParams();
  initial_size.screen_info = screen_info_;

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  ViewMsg_New_Params view_params;

  RenderFrameImpl* creator_frame = RenderFrameImpl::FromWebFrame(creator);
  view_params.opener_frame_route_id = creator_frame->GetRoutingID();
  DCHECK_EQ(GetRoutingID(), creator_frame->render_view()->GetRoutingID());

  view_params.window_was_created_with_opener = true;
  view_params.renderer_preferences = renderer_preferences_;
  view_params.web_preferences = webkit_preferences_;
  view_params.view_id = reply.route_id;
  view_params.main_frame_routing_id = reply.main_frame_route_id;
  view_params.main_frame_widget_routing_id = reply.main_frame_widget_route_id;
  view_params.session_storage_namespace_id =
      reply.cloned_session_storage_namespace_id;
  view_params.swapped_out = false;
  // WebCore will take care of setting the correct name.
  view_params.replicated_frame_state = FrameReplicationState();
  view_params.hidden = (params.disposition == NEW_BACKGROUND_TAB);
  view_params.never_visible = never_visible;
  view_params.next_page_id = 1;
  view_params.initial_size = initial_size;
  view_params.enable_auto_resize = false;
  view_params.min_size = gfx::Size();
  view_params.max_size = gfx::Size();
  view_params.page_zoom_level = page_zoom_level_;

  RenderViewImpl* view =
      RenderViewImpl::Create(compositor_deps_, view_params, true);
  view->opened_by_user_gesture_ = params.user_gesture;

  return view->webview();
}

WebWidget* RenderViewImpl::createPopupMenu(blink::WebPopupType popup_type) {
  RenderWidget* widget = RenderWidget::Create(GetRoutingID(), compositor_deps_,
                                              popup_type, screen_info_);
  if (!widget)
    return NULL;
  if (screen_metrics_emulator_) {
    widget->SetPopupOriginAdjustmentsForEmulation(
        screen_metrics_emulator_.get());
  }
  return widget->webwidget();
}

WebStorageNamespace* RenderViewImpl::createSessionStorageNamespace() {
  CHECK(session_storage_namespace_id_ != kInvalidSessionStorageNamespaceId);
  return new WebStorageNamespaceImpl(session_storage_namespace_id_);
}

void RenderViewImpl::printPage(WebLocalFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    PrintPage(frame, input_handler().handling_input_event()));
}

bool RenderViewImpl::enumerateChosenDirectory(
    const WebString& path,
    WebFileChooserCompletion* chooser_completion) {
  int id = enumeration_completion_id_++;
  enumeration_completions_[id] = chooser_completion;
  return Send(new ViewHostMsg_EnumerateDirectory(
      GetRoutingID(), id, blink::WebStringToFilePath(path)));
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

void RenderViewImpl::AttachWebFrameWidget(blink::WebFrameWidget* frame_widget) {
  // The previous WebFrameWidget must already be detached by CloseForFrame().
  DCHECK(!frame_widget_);
  frame_widget_ = frame_widget;
}

void RenderViewImpl::SetZoomLevel(double zoom_level) {
  // If we change the zoom level for the view, make sure any subsequent subframe
  // loads reflect the current zoom level.
  page_zoom_level_ = zoom_level;

  webview()->setZoomLevel(zoom_level);
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, OnZoomLevelChanged());
}

void RenderViewImpl::didCancelCompositionOnSelectionChange() {
  Send(new InputHostMsg_ImeCancelComposition(GetRoutingID()));
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
                               WebString::fromUTF8(it->value)))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

void RenderViewImpl::SetValidationMessageDirection(
    base::string16* wrapped_main_text,
    blink::WebTextDirection main_text_hint,
    base::string16* wrapped_sub_text,
    blink::WebTextDirection sub_text_hint) {
  if (main_text_hint == blink::WebTextDirectionLeftToRight) {
    *wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_main_text);
  } else if (main_text_hint == blink::WebTextDirectionRightToLeft &&
             !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(wrapped_main_text);
  }

  if (!wrapped_sub_text->empty()) {
    if (sub_text_hint == blink::WebTextDirectionLeftToRight) {
      *wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_sub_text);
    } else if (sub_text_hint == blink::WebTextDirectionRightToLeft) {
      base::i18n::WrapStringWithRTLFormatting(wrapped_sub_text);
    }
  }
}

void RenderViewImpl::showValidationMessage(
    const blink::WebRect& anchor_in_viewport,
    const blink::WebString& main_text,
    blink::WebTextDirection main_text_hint,
    const blink::WebString& sub_text,
    blink::WebTextDirection sub_text_hint) {
  base::string16 wrapped_main_text = main_text;
  base::string16 wrapped_sub_text = sub_text;

  SetValidationMessageDirection(
      &wrapped_main_text, main_text_hint, &wrapped_sub_text, sub_text_hint);

  Send(new ViewHostMsg_ShowValidationMessage(
      GetRoutingID(), AdjustValidationMessageAnchor(anchor_in_viewport),
      wrapped_main_text, wrapped_sub_text));
}

void RenderViewImpl::hideValidationMessage() {
  Send(new ViewHostMsg_HideValidationMessage(GetRoutingID()));
}

void RenderViewImpl::moveValidationMessage(
    const blink::WebRect& anchor_in_viewport) {
  Send(new ViewHostMsg_MoveValidationMessage(
      GetRoutingID(), AdjustValidationMessageAnchor(anchor_in_viewport)));
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
    // URLs larger than |kMaxURLChars| cannot be sent through IPC -
    // see |ParamTraits<GURL>|.
    if (latest_url.possibly_invalid_spec().size() > url::kMaxURLChars)
      latest_url = GURL();
    Send(new ViewHostMsg_UpdateTargetURL(GetRoutingID(), latest_url));
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

void RenderViewImpl::StartNavStateSyncTimerIfNecessary(RenderFrameImpl* frame) {
  // In OOPIF modes, keep track of which frames have pending updates.
  if (SiteIsolationPolicy::UseSubframeNavigationEntries())
    frames_with_pending_state_.insert(frame->GetRoutingID());

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

  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // In OOPIF modes, tell each frame with pending state to inform the browser.
    nav_state_sync_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(delay), this,
                                &RenderViewImpl::SendFrameStateUpdates);
  } else {
    // By default, send an UpdateState for the current history item.
    nav_state_sync_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(delay), this,
                                &RenderViewImpl::SendUpdateState);
  }
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
  blink::WebRect offset_in_window(webImageOffset.x, webImageOffset.y, 0, 0);
  ConvertViewportToWindowViaWidget(&offset_in_window);
  DropData drop_data(DropDataBuilder::Build(data));
  drop_data.referrer_policy = frame->document().referrerPolicy();
  gfx::Vector2d imageOffset(offset_in_window.x, offset_in_window.y);
  Send(new DragHostMsg_StartDragging(GetRoutingID(), drop_data, mask,
                                     image.getSkBitmap(), imageOffset,
                                     possible_drag_event_info_));
}

bool RenderViewImpl::acceptsLoadDrops() {
  return renderer_preferences_.can_accept_load_drops;
}

void RenderViewImpl::focusNext() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), false));
}

void RenderViewImpl::focusPrevious() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), true));
}

// TODO(esprehn): Blink only ever passes Elements, this should take WebElement.
void RenderViewImpl::focusedNodeChanged(const WebNode& fromNode,
                                        const WebNode& toNode) {
  has_scrolled_focused_editable_node_into_rect_ = false;

  gfx::Rect node_bounds;
  bool is_editable = false;
  if (!toNode.isNull() && toNode.isElementNode()) {
    WebElement element = const_cast<WebNode&>(toNode).to<WebElement>();
    blink::WebRect rect = element.boundsInViewport();
    ConvertViewportToWindowViaWidget(&rect);
    node_bounds = gfx::Rect(rect);
    is_editable = element.isEditable();
  }
  Send(new ViewHostMsg_FocusedNodeChanged(GetRoutingID(), is_editable,
                                          node_bounds));

  // TODO(estade): remove.
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FocusedNodeChanged(toNode));

  RenderFrameImpl* previous_frame = nullptr;
  if (!fromNode.isNull())
    previous_frame = RenderFrameImpl::FromWebFrame(fromNode.document().frame());
  RenderFrameImpl* new_frame = nullptr;
  if (!toNode.isNull())
    new_frame = RenderFrameImpl::FromWebFrame(toNode.document().frame());

  if (previous_frame && previous_frame != new_frame)
    previous_frame->FocusedNodeChanged(WebNode());
  if (new_frame)
    new_frame->FocusedNodeChanged(toNode);

  // TODO(dmazzoni): remove once there's a separate a11y tree per frame.
  if (main_render_frame_)
    main_render_frame_->FocusedNodeChangedForAccessibility(toNode);
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
  Send(new ViewHostMsg_GoToEntryAtOffset(GetRoutingID(), offset));
}

int RenderViewImpl::historyBackListCount() {
  return history_list_offset_ < 0 ? 0 : history_list_offset_;
}

int RenderViewImpl::historyForwardListCount() {
  return history_list_length_ - historyBackListCount() - 1;
}

// blink::WebWidgetClient ----------------------------------------------------

void RenderViewImpl::didFocus() {
  // TODO(jcivelli): when https://bugs.webkit.org/show_bug.cgi?id=33389 is fixed
  //                 we won't have to test for user gesture anymore and we can
  //                 move that code back to render_widget.cc
  if (WebUserGestureIndicator::isProcessingUserGesture() &&
      !RenderThreadImpl::current()->layout_test_mode()) {
    Send(new ViewHostMsg_Focus(GetRoutingID()));
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

  // NOTE: initial_rect_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if disposition is not NEW_POPUP, or the
  // browser process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowView(opener_id_, GetRoutingID(),
                                NavigationPolicyToDisposition(policy),
                                initial_rect_, opened_by_user_gesture_));
  SetPendingWindowRect(initial_rect_);
}

void RenderViewImpl::onMouseDown(const WebNode& mouse_down_node) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, OnMouseDown(mouse_down_node));
}

void RenderViewImpl::didHandleGestureEvent(
    const WebGestureEvent& event,
    bool event_cancelled) {
  RenderWidget::didHandleGestureEvent(event, event_cancelled);

  if (!event_cancelled) {
    FOR_EACH_OBSERVER(
        RenderViewObserver, observers_, DidHandleGestureEvent(event));
  }

  // TODO(ananta): Piggyback off existing IPCs to communicate this information,
  // crbug/420130.
#if defined(OS_WIN)
  if (event.type != blink::WebGestureEvent::GestureTap)
    return;

  // TODO(estade): hit test the event against focused node to make sure
  // the tap actually hit the focused node.
  blink::WebTextInputType text_input_type = GetWebView()->textInputType();

  Send(new ViewHostMsg_FocusedNodeTouched(
      GetRoutingID(), text_input_type != blink::WebTextInputTypeNone));
#endif
}

void RenderViewImpl::initializeLayerTreeView() {
  RenderWidget::initializeLayerTreeView();
  RenderWidgetCompositor* rwc = compositor();
  if (!rwc)
    return;

  bool use_threaded_event_handling = true;
#if defined(OS_MACOSX)
  // Disable threaded event handling if content is not handling the elastic
  // overscroll effect. This includes the cases where the elastic overscroll
  // effect is being handled by Blink (because of command line flags) and older
  // operating system versions which do not have an elastic overscroll effect
  // (SnowLeopard, which has Aqua scrollbars which need synchronous updates).
  use_threaded_event_handling = compositor_deps_->IsElasticOverscrollEnabled();
#endif
  if (!use_threaded_event_handling)
    return;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // render_thread may be NULL in tests.
  InputHandlerManager* input_handler_manager =
      render_thread ? render_thread->input_handler_manager() : NULL;
  if (input_handler_manager) {
    input_handler_manager->AddInputHandler(
        GetRoutingID(), rwc->GetInputHandler(), AsWeakPtr(),
        webkit_preferences_.enable_scroll_animator);
    has_added_input_handler_ = true;
  }
}

bool RenderViewImpl::allowsBrokenNullLayerTreeView() const {
  return RenderWidget::allowsBrokenNullLayerTreeView();
}

void RenderViewImpl::closeWidgetSoon() {
  RenderWidget::closeWidgetSoon();
}

void RenderViewImpl::convertViewportToWindow(blink::WebRect* rect) {
  RenderWidget::convertViewportToWindow(rect);
}

void RenderViewImpl::convertWindowToViewport(blink::WebFloatRect* rect) {
  RenderWidget::convertWindowToViewport(rect);
}

void RenderViewImpl::didAutoResize(const blink::WebSize& newSize) {
  RenderWidget::didAutoResize(newSize);
}

void RenderViewImpl::didChangeCursor(const blink::WebCursorInfo& info) {
  RenderWidget::didChangeCursor(info);
}

void RenderViewImpl::didInvalidateRect(const blink::WebRect& rect) {
  RenderWidget::didInvalidateRect(rect);
}

void RenderViewImpl::didMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  RenderWidget::didMeaningfulLayout(layout_type);
}

void RenderViewImpl::didOverscroll(
    const blink::WebFloatSize& overscrollDelta,
    const blink::WebFloatSize& accumulatedOverscroll,
    const blink::WebFloatPoint& positionInViewport,
    const blink::WebFloatSize& velocityInViewport) {
  RenderWidget::didOverscroll(overscrollDelta, accumulatedOverscroll,
                              positionInViewport, velocityInViewport);
}

void RenderViewImpl::didUpdateTextOfFocusedElementByNonUserInput() {
  RenderWidget::didUpdateTextOfFocusedElementByNonUserInput();
}

void RenderViewImpl::hasTouchEventHandlers(bool has_handlers) {
  RenderWidget::hasTouchEventHandlers(has_handlers);
}

blink::WebLayerTreeView* RenderViewImpl::layerTreeView() {
  return RenderWidget::layerTreeView();
}

void RenderViewImpl::resetInputMethod() {
  RenderWidget::resetInputMethod();
}

blink::WebRect RenderViewImpl::rootWindowRect() {
  return RenderWidget::rootWindowRect();
}

void RenderViewImpl::scheduleAnimation() {
  RenderWidget::scheduleAnimation();
}

blink::WebScreenInfo RenderViewImpl::screenInfo() {
  return RenderWidget::screenInfo();
}

void RenderViewImpl::setToolTipText(const blink::WebString& text,
                                    blink::WebTextDirection hint) {
  RenderWidget::setToolTipText(text, hint);
}

void RenderViewImpl::setTouchAction(blink::WebTouchAction touchAction) {
  RenderWidget::setTouchAction(touchAction);
}

void RenderViewImpl::setWindowRect(const blink::WebRect& rect) {
  RenderWidget::setWindowRect(rect);
}

void RenderViewImpl::showImeIfNeeded() {
  RenderWidget::showImeIfNeeded();
}

void RenderViewImpl::showUnhandledTapUIIfNeeded(
    const blink::WebPoint& tappedPosition,
    const blink::WebNode& tappedNode,
    bool pageChanged) {
  RenderWidget::showUnhandledTapUIIfNeeded(tappedPosition, tappedNode,
                                           pageChanged);
}

blink::WebRect RenderViewImpl::windowRect() {
  return RenderWidget::windowRect();
}

blink::WebRect RenderViewImpl::windowResizerRect() {
  return RenderWidget::windowResizerRect();
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

  SSLStatus result;
  CHECK(DeserializeSecurityInfo(security_info, &result));
  return result;
}

const std::string& RenderViewImpl::GetAcceptLanguages() const {
  return renderer_preferences_.accept_languages;
}

void RenderViewImpl::ConvertViewportToWindowViaWidget(blink::WebRect* rect) {
  convertViewportToWindow(rect);
}

gfx::RectF RenderViewImpl::ElementBoundsInWindow(
    const blink::WebElement& element) {
  blink::WebRect bounding_box_in_window = element.boundsInViewport();
  ConvertViewportToWindowViaWidget(&bounding_box_in_window);
  return gfx::RectF(bounding_box_in_window);
}

float RenderViewImpl::GetDeviceScaleFactorForTest() const {
  return device_scale_factor_;
}

bool RenderViewImpl::HasAddedInputHandler() const {
  return has_added_input_handler_;
}

gfx::Point RenderViewImpl::ConvertWindowPointToViewport(
    const gfx::Point& point) {
  blink::WebFloatRect point_in_viewport(point.x(), point.y(), 0, 0);
  convertWindowToViewport(&point_in_viewport);
  return gfx::Point(point_in_viewport.x, point_in_viewport.y);
}

void RenderViewImpl::didChangeIcon(WebLocalFrame* frame,
                                   WebIconURL::Type icon_type) {
  if (frame->parent())
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

void RenderViewImpl::CheckPreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  gfx::Size size = webview()->contentsPreferredMinimumSize();
  if (size == preferred_size_)
    return;

  preferred_size_ = size;
  Send(new ViewHostMsg_DidContentsPreferredSizeChange(GetRoutingID(),
                                                      preferred_size_));
}

blink::WebString RenderViewImpl::acceptLanguages() {
  return WebString::fromUTF8(renderer_preferences_.accept_languages);
}

// RenderView implementation ---------------------------------------------------

bool RenderViewImpl::Send(IPC::Message* message) {
  return RenderWidget::Send(message);
}

RenderWidget* RenderViewImpl::GetWidget() const {
  return const_cast<RenderWidget*>(static_cast<const RenderWidget*>(this));
}

RenderFrameImpl* RenderViewImpl::GetMainRenderFrame() {
  return main_render_frame_;
}

int RenderViewImpl::GetRoutingID() const {
  return routing_id();
}

gfx::Size RenderViewImpl::GetSize() const {
  return size();
}

float RenderViewImpl::GetDeviceScaleFactor() const {
  return device_scale_factor_;
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

blink::WebFrameWidget* RenderViewImpl::GetWebFrameWidget() {
  return frame_widget_;
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

void RenderViewImpl::DidStartLoading() {
  main_render_frame_->didStartLoading(true);
}

void RenderViewImpl::DidStopLoading() {
  main_render_frame_->didStopLoading();
}

void RenderViewImpl::OnSetPageScale(float page_scale_factor) {
  if (!webview())
    return;
  webview()->setPageScaleFactor(page_scale_factor);
}

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
  SetZoomLevel(zoom_level);
  zoomLevelChanged();
}

void RenderViewImpl::OnSetZoomLevelForLoadingURL(const GURL& url,
                                                 double zoom_level) {
  // TODO(wjmaclean): We should see if this restriction is really necessary,
  // since it isn't enforced in other parts of the page zoom system (e.g.
  // when a users changes the zoom of a currently displayed page). Android
  // has no UI for this, so in theory the following code would normally just use
  // the default zoom anyways.
#if !defined(OS_ANDROID)
  // On Android, page zoom isn't used, and in case of WebView, text zoom is used
  // for legacy WebView text scaling emulation. Thus, the code that resets
  // the zoom level from this map will be effectively resetting text zoom level.
  host_zoom_levels_[url] = zoom_level;
#endif
}

void RenderViewImpl::OnSetZoomLevel(
    PageMsg_SetZoomLevel_Command command,
    double zoom_level) {
  switch (command) {
    case PageMsg_SetZoomLevel_Command::CLEAR_TEMPORARY:
      uses_temporary_zoom_level_ = false;
      break;
    case PageMsg_SetZoomLevel_Command::SET_TEMPORARY:
      uses_temporary_zoom_level_ = true;
      break;
    case PageMsg_SetZoomLevel_Command::USE_CURRENT_TEMPORARY_MODE:
      // Don't override a temporary zoom level without an explicit SET.
      if (uses_temporary_zoom_level_)
        return;
      break;
    default:
      NOTIMPLEMENTED();
  }
  webview()->hidePopups();
  SetZoomLevel(zoom_level);
}

void RenderViewImpl::OnSetPageEncoding(const std::string& encoding_name) {
  webview()->setPageEncoding(WebString::fromUTF8(encoding_name));
}

void RenderViewImpl::OnResetPageEncodingToDefault() {
  WebString no_encoding;
  webview()->setPageEncoding(no_encoding);
}

void RenderViewImpl::OnAllowBindings(int enabled_bindings_flags) {
  if ((enabled_bindings_flags & BINDINGS_POLICY_WEB_UI) &&
      !(enabled_bindings_ & BINDINGS_POLICY_WEB_UI)) {
    // WebUIExtensionData deletes itself when we're destroyed.
    new WebUIExtensionData(this);

  }

  enabled_bindings_ |= enabled_bindings_flags;

  // Keep track of the total bindings accumulated in this process.
  RenderProcess::current()->AddBindings(enabled_bindings_flags);

  if (main_render_frame_)
    main_render_frame_->MaybeEnableMojoBindings();
}

void RenderViewImpl::OnDragTargetDragEnter(
    const std::vector<DropData::Metadata>& drop_meta_data,
    const gfx::Point& client_point,
    const gfx::Point& screen_point,
    WebDragOperationsMask ops,
    int key_modifiers) {
  WebDragOperation operation = webview()->dragTargetDragEnter(
      DropMetaDataToWebDragData(drop_meta_data), client_point, screen_point,
      ops, key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(GetRoutingID(), operation));
}

void RenderViewImpl::OnDragTargetDragOver(const gfx::Point& client_point,
                                          const gfx::Point& screen_point,
                                          WebDragOperationsMask ops,
                                          int key_modifiers) {
  WebDragOperation operation = webview()->dragTargetDragOver(
      ConvertWindowPointToViewport(client_point),
      screen_point,
      ops,
      key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(GetRoutingID(), operation));
}

void RenderViewImpl::OnDragTargetDragLeave() {
  webview()->dragTargetDragLeave();
}

void RenderViewImpl::OnDragTargetDrop(const DropData& drop_data,
                                      const gfx::Point& client_point,
                                      const gfx::Point& screen_point,
                                      int key_modifiers) {
  webview()->dragTargetDrop(DropDataToWebDragData(drop_data), client_point,
                            screen_point, key_modifiers);
}

void RenderViewImpl::OnDragSourceEnded(const gfx::Point& client_point,
                                       const gfx::Point& screen_point,
                                       WebDragOperation op) {
  webview()->dragSourceEndedAt(
      ConvertWindowPointToViewport(client_point), screen_point, op);
}

void RenderViewImpl::OnDragSourceSystemDragEnded() {
  webview()->dragSourceSystemDragEnded();
}

void RenderViewImpl::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  ApplyWebPreferencesInternal(webkit_preferences_, webview(), compositor_deps_);
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

void RenderViewImpl::OnEnableAutoResize(const gfx::Size& min_size,
                                        const gfx::Size& max_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;

  auto_resize_mode_ = true;
  if (IsUseZoomForDSFEnabled()) {
    webview()->enableAutoResizeMode(
        gfx::ScaleToCeiledSize(min_size, device_scale_factor_),
        gfx::ScaleToCeiledSize(max_size, device_scale_factor_));
  } else {
    webview()->enableAutoResizeMode(min_size, max_size);
  }
}

void RenderViewImpl::OnDisableAutoResize(const gfx::Size& new_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;
  auto_resize_mode_ = false;
  webview()->disableAutoResizeMode();

  if (!new_size.IsEmpty()) {
    ResizeParams resize_params;
    resize_params.new_size = new_size;
    resize_params.physical_backing_size = physical_backing_size_;
    resize_params.top_controls_shrink_blink_size =
        top_controls_shrink_blink_size_;
    resize_params.top_controls_height = top_controls_height_;
    resize_params.visible_viewport_size = visible_viewport_size_;
    resize_params.resizer_rect = resizer_rect_;
    resize_params.is_fullscreen_granted = is_fullscreen_granted();
    resize_params.display_mode = display_mode_;
    resize_params.needs_resize_ack = false;
    Resize(resize_params);
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
  std::string old_accept_languages = renderer_preferences_.accept_languages;

  renderer_preferences_ = renderer_prefs;

  UpdateFontRenderingFromRendererPrefs();

#if defined(USE_DEFAULT_RENDER_THEME)
  if (renderer_prefs.use_custom_colors) {
    blink::setFocusRingColor(renderer_prefs.focus_ring_color);
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
  if (webview() && webview()->mainFrame()->isWebLocalFrame())
    webview()->mainFrame()->toWebLocalFrame()->sendOrientationChangeEvent();
}

void RenderViewImpl::OnPluginActionAt(const gfx::Point& location,
                                      const WebPluginAction& action) {
  if (webview())
    webview()->performPluginAction(action, location);
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

  Send(new ViewHostMsg_ClosePage_ACK(GetRoutingID()));
}

void RenderViewImpl::OnClose() {
  if (closing_)
    RenderThread::Get()->Send(new ViewHostMsg_Close_ACK(GetRoutingID()));
  RenderWidget::OnClose();
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

void RenderViewImpl::ResizeWebWidget() {
    webview()->resizeWithTopControls(GetSizeForWebWidget(),
                                     top_controls_height_,
                                     top_controls_shrink_blink_size_);
}

void RenderViewImpl::OnResize(const ResizeParams& params) {
  TRACE_EVENT0("renderer", "RenderViewImpl::OnResize");
  if (webview()) {
    webview()->hidePopups();
    if (send_preferred_size_changes_ &&
        webview()->mainFrame()->isWebLocalFrame()) {
      webview()->mainFrame()->setCanHaveScrollbars(
          ShouldDisplayScrollbars(params.new_size.width(),
                                  params.new_size.height()));
    }
    if (display_mode_ != params.display_mode) {
      display_mode_ = params.display_mode;
      webview()->setDisplayMode(display_mode_);
    }
  }

  gfx::Size old_visible_viewport_size = visible_viewport_size_;

  top_controls_shrink_blink_size_ = params.top_controls_shrink_blink_size;
  top_controls_height_ = params.top_controls_height;

  RenderWidget::OnResize(params);

  if (old_visible_viewport_size != visible_viewport_size_)
    has_scrolled_focused_editable_node_into_rect_ = false;
}

void RenderViewImpl::RenderWidgetDidFlushPaint() {
  // If the RenderWidget is closing down then early-exit, otherwise we'll crash.
  // See crbug.com/112921.
  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  for (WebFrame* frame = main_frame; frame;
       frame = frame->traverseNext(false)) {
    // TODO(nasko): This is a hack for the case in which the top-level
    // frame is being rendered in another process. It will not
    // behave correctly for out of process iframes.
    if (frame->isWebLocalFrame()) {
      main_frame = frame;
      break;
    }
  }

  // There's nothing to do if there are no local frames in this RenderView's
  // frame tree. This can happen if DidFlushPaint is called after the
  // RenderView's local main frame is swapped to a remote frame. See
  // http://crbug.com/513552.
  if (main_frame->isWebRemoteFrame())
    return;

  // If we have a provisional frame we are between the start and commit stages
  // of loading and we don't want to save stats.
  if (!main_frame->provisionalDataSource()) {
    WebDataSource* ds = main_frame->dataSource();
    if (!ds)
      return;

    DocumentState* document_state = DocumentState::FromDataSource(ds);

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

void RenderViewImpl::OnClearFocusedElement() {
  if (webview())
    webview()->clearFocusedElement();
}

void RenderViewImpl::OnSetBackgroundOpaque(bool opaque) {
  if (frame_widget_)
    frame_widget_->setIsTransparent(!opaque);
  if (compositor_)
    compositor_->setHasTransparentBackground(!opaque);
}

void RenderViewImpl::OnSetActive(bool active) {
  if (webview())
    webview()->setIsActive(active);
}

void RenderViewImpl::CloseForFrame() {
  DCHECK(frame_widget_);
  frame_widget_->close();
  frame_widget_ = nullptr;
}

void RenderViewImpl::Close() {
  // We need to grab a pointer to the doomed WebView before we destroy it.
  WebView* doomed = webview_;
  RenderWidget::Close();
  webview_ = nullptr;
  g_view_map.Get().erase(doomed);
  g_routing_id_view_map.Get().erase(GetRoutingID());
  RenderThread::Get()->Send(new ViewHostMsg_Close_ACK(GetRoutingID()));
}

void RenderViewImpl::OnPageWasHidden() {
#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  RenderThreadImpl::current()->video_capture_impl_manager()->
      SuspendDevices(true);
  if (speech_recognition_dispatcher_)
    speech_recognition_dispatcher_->AbortAllRecognitions();
#endif

  if (webview()) {
    // TODO(lfg): It's not correct to defer the page visibility to the main
    // frame. Currently, this is done because the main frame may override the
    // visibility of the page when prerendering. In order to fix this,
    // prerendering must be made aware of OOPIFs. https://crbug.com/440544
    blink::WebPageVisibilityState visibilityState =
        GetMainRenderFrame() ? GetMainRenderFrame()->visibilityState()
                             : blink::WebPageVisibilityStateHidden;
    webview()->setVisibilityState(visibilityState, false);
  }
}

void RenderViewImpl::OnPageWasShown() {
#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  RenderThreadImpl::current()->video_capture_impl_manager()->
      SuspendDevices(false);
#endif

  if (webview()) {
    blink::WebPageVisibilityState visibilityState =
        GetMainRenderFrame() ? GetMainRenderFrame()->visibilityState()
                             : blink::WebPageVisibilityStateVisible;
    webview()->setVisibilityState(visibilityState, false);
  }
}

GURL RenderViewImpl::GetURLForGraphicsContext3D() {
  DCHECK(webview());
  if (webview()->mainFrame()->isWebLocalFrame())
    return GURL(webview()->mainFrame()->document().url());
  else
    return GURL("chrome://gpu/RenderViewImpl::CreateGraphicsContext3D");
}

void RenderViewImpl::OnSetFocus(bool enable) {
  // This message must always be received when the main frame is a
  // WebLocalFrame.
  CHECK(webview()->mainFrame()->isWebLocalFrame());
  SetFocus(enable);
}

void RenderViewImpl::SetFocus(bool enable) {
  RenderWidget::OnSetFocus(enable);

  // Notify all BrowserPlugins of the RenderView's focus state.
  if (BrowserPluginManager::Get())
    BrowserPluginManager::Get()->UpdateFocusState();
}

void RenderViewImpl::OnImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin = GetFocusedPepperPlugin();
  if (focused_pepper_plugin) {
    focused_pepper_plugin->render_frame()->OnImeSetComposition(
        text, underlines, selection_start, selection_end);
    return;
  }
#endif  // ENABLE_PLUGINS
  if (replacement_range.IsValid() && webview()) {
    // Select the text in |replacement_range|, it will then be replaced by
    // text added by the call to RenderWidget::OnImeSetComposition().
    if (WebLocalFrame* frame = webview()->focusedFrame()->toWebLocalFrame()) {
      WebRange webrange = WebRange::fromDocumentRange(
          frame, replacement_range.start(), replacement_range.length());
      if (!webrange.isNull())
        frame->selectRange(webrange);
    }
  }
  RenderWidget::OnImeSetComposition(text,
                                    underlines,
                                    replacement_range,
                                    selection_start,
                                    selection_end);
}

void RenderViewImpl::OnImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection) {
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin = GetFocusedPepperPlugin();
  if (focused_pepper_plugin) {
    focused_pepper_plugin->render_frame()->OnImeConfirmComposition(
        text, replacement_range, keep_selection);
    return;
  }
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

void RenderViewImpl::RenderWidgetDidSetColorProfile(
    const std::vector<char>& profile) {
  if (webview()) {
    bool was_reset = (profile.size() == 1 && profile[0] == '0');

    if (was_reset) {
      webview()->resetDeviceColorProfileForTesting();
    } else {
      WebVector<char> colorProfile = profile;
      webview()->setDeviceColorProfile(colorProfile);
    }
  }
}

ui::TextInputType RenderViewImpl::GetTextInputType() {
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin = GetFocusedPepperPlugin();
  if (focused_pepper_plugin)
    return focused_pepper_plugin->text_input_type();
#endif
  return RenderWidget::GetTextInputType();
}

void RenderViewImpl::GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) {
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin = GetFocusedPepperPlugin();
  if (focused_pepper_plugin) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    blink::WebRect caret(focused_pepper_plugin->GetCaretBounds());
    ConvertViewportToWindowViaWidget(&caret);
    *start = caret;
    *end = caret;
    return;
  }
#endif
  RenderWidget::GetSelectionBounds(start, end);
}

void RenderViewImpl::GetCompositionCharacterBounds(
    std::vector<gfx::Rect>* bounds_in_window) {
  DCHECK(bounds_in_window);
  bounds_in_window->clear();

#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin = GetFocusedPepperPlugin();
  if (focused_pepper_plugin)
    return;
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

  bounds_in_window->reserve(character_count);
  blink::WebRect webrect;
  for (size_t i = 0; i < character_count; ++i) {
    if (!frame->firstRectForCharacterRange(start_offset + i, 1, webrect)) {
      DLOG(ERROR) << "Could not retrieve character rectangle at " << i;
      bounds_in_window->clear();
      return;
    }
    ConvertViewportToWindowViaWidget(&webrect);
    bounds_in_window->push_back(webrect);
  }
}

void RenderViewImpl::GetCompositionRange(gfx::Range* range) {
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin = GetFocusedPepperPlugin();
  if (focused_pepper_plugin)
    return;
#endif
  RenderWidget::GetCompositionRange(range);
}

bool RenderViewImpl::CanComposeInline() {
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_pepper_plugin = GetFocusedPepperPlugin();
  if (focused_pepper_plugin)
    return focused_pepper_plugin->IsPluginAcceptingCompositionEvents();
#endif
  return true;
}

void RenderViewImpl::DidCompletePageScaleAnimation() {
  GetWidget()->FocusChangeComplete();
}

void RenderViewImpl::OnDeviceScaleFactorChanged() {
  RenderWidget::OnDeviceScaleFactorChanged();
  UpdateWebViewWithDeviceScaleFactor();
  if (auto_resize_mode_)
    AutoResizeCompositor();
}

void RenderViewImpl::SetScreenMetricsEmulationParameters(
    bool enabled,
    const blink::WebDeviceEmulationParams& params) {
  if (webview() && compositor()) {
    if (enabled)
      webview()->enableDeviceEmulation(params);
    else
      webview()->disableDeviceEmulation();
  }
}

blink::WebSpeechRecognizer* RenderViewImpl::speechRecognizer() {
  if (!speech_recognition_dispatcher_)
    speech_recognition_dispatcher_ = new SpeechRecognitionDispatcher(this);
  return speech_recognition_dispatcher_;
}

void RenderViewImpl::zoomLimitsChanged(double minimum_level,
                                       double maximum_level) {
  // Round the double to avoid returning incorrect minimum/maximum zoom
  // percentages.
  int minimum_percent = round(
      ZoomLevelToZoomFactor(minimum_level) * 100);
  int maximum_percent = round(
      ZoomLevelToZoomFactor(maximum_level) * 100);

  Send(new ViewHostMsg_UpdateZoomLimits(GetRoutingID(), minimum_percent,
                                        maximum_percent));
}

void RenderViewImpl::zoomLevelChanged() {
  double zoom_level = webview()->zoomLevel();

  // Do not send empty URLs to the browser when we are just setting the default
  // zoom level (from RendererPreferences) before the first navigation.
  if (!webview()->mainFrame()->document().url().isEmpty()) {
    // Tell the browser which url got zoomed so it can update the menu and the
    // saved values if necessary
    Send(new ViewHostMsg_DidZoomURL(
        GetRoutingID(), zoom_level,
        GURL(webview()->mainFrame()->document().url())));
  }
}

void RenderViewImpl::pageScaleFactorChanged() {
  if (!webview())
    return;

  Send(new ViewHostMsg_PageScaleFactorChanged(GetRoutingID(),
                                              webview()->pageScaleFactor()));
}

double RenderViewImpl::zoomLevelToZoomFactor(double zoom_level) const {
  return ZoomLevelToZoomFactor(zoom_level);
}

double RenderViewImpl::zoomFactorToZoomLevel(double factor) const {
  return ZoomFactorToZoomLevel(factor);
}

void RenderViewImpl::draggableRegionsChanged() {
  FOR_EACH_OBSERVER(
      RenderViewObserver,
      observers_,
      DraggableRegionsChanged(webview()->mainFrame()));
}

void RenderViewImpl::pageImportanceSignalsChanged() {
  if (!webview() || !main_render_frame_)
    return;

  const auto& web_signals = webview()->pageImportanceSignals();

  PageImportanceSignals signals;
  signals.had_form_interaction = web_signals->hadFormInteraction();

  main_render_frame_->Send(new FrameHostMsg_UpdatePageImportanceSignals(
      main_render_frame_->GetRoutingID(), signals));
}

#if defined(OS_ANDROID)
WebContentDetectionResult RenderViewImpl::detectContentAround(
    const WebHitTestResult& touch_hit) {
  DCHECK(touch_hit.node().isTextNode());

  // Process the position with all the registered content detectors until
  // a match is found. Priority is provided by their relative order.
  for (const auto& detector : content_detectors_) {
    ContentDetector::Result content = detector->FindTappedContent(touch_hit);
    if (content.valid) {
      return WebContentDetectionResult(content.content_boundaries,
          base::UTF8ToUTF16(content.text), content.intent_url);
    }
  }
  return WebContentDetectionResult();
}

void RenderViewImpl::scheduleContentIntent(const WebURL& intent,
                                           bool is_main_frame) {
  // Introduce a short delay so that the user can notice the content.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&RenderViewImpl::LaunchAndroidContentIntent, AsWeakPtr(),
                 intent, expected_content_intent_id_, is_main_frame),
      base::TimeDelta::FromMilliseconds(kContentIntentDelayMilliseconds));
}

void RenderViewImpl::cancelScheduledContentIntents() {
  ++expected_content_intent_id_;
}

void RenderViewImpl::LaunchAndroidContentIntent(const GURL& intent,
                                                size_t request_id,
                                                bool is_main_frame) {
  if (request_id != expected_content_intent_id_)
    return;

  // Remove the content highlighting if any.
  ScheduleComposite();

  if (!intent.is_empty()) {
    Send(new ViewHostMsg_StartContentIntent(GetRoutingID(), intent,
                                            is_main_frame));
  }
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

void RenderViewImpl::OnShowContextMenu(
    ui::MenuSourceType source_type, const gfx::Point& location) {
  input_handler_->set_context_menu_source_type(source_type);
  has_host_context_menu_location_ = true;
  host_context_menu_location_ = location;
  if (webview())
    webview()->showContextMenu();
  has_host_context_menu_location_ = false;
}

#if defined(OS_ANDROID)
bool RenderViewImpl::didTapMultipleTargets(
    const WebSize& inner_viewport_offset,
    const WebRect& touch_rect,
    const WebVector<WebRect>& target_rects) {
  // Never show a disambiguation popup when accessibility is enabled,
  // as this interferes with "touch exploration".
  AccessibilityMode accessibility_mode =
      GetMainRenderFrame()->accessibility_mode();
  bool matches_accessibility_mode_complete =
      (accessibility_mode & AccessibilityModeComplete) ==
          AccessibilityModeComplete;
  if (matches_accessibility_mode_complete)
    return false;

  // The touch_rect, target_rects and zoom_rect are in the outer viewport
  // reference frame.
  gfx::Rect zoom_rect;
  float new_total_scale =
      DisambiguationPopupHelper::ComputeZoomAreaAndScaleFactor(
          touch_rect, target_rects, GetSize(),
          gfx::Rect(webview()->mainFrame()->visibleContentRect()).size(),
          device_scale_factor_ * webview()->pageScaleFactor(), &zoom_rect);
  if (!new_total_scale || zoom_rect.IsEmpty())
    return false;

  bool handled = false;
  switch (renderer_preferences_.tap_multiple_targets_strategy) {
    case TAP_MULTIPLE_TARGETS_STRATEGY_ZOOM:
      handled = webview()->zoomToMultipleTargetsRect(zoom_rect);
      break;
    case TAP_MULTIPLE_TARGETS_STRATEGY_POPUP: {
      gfx::Size canvas_size =
          gfx::ScaleToCeiledSize(zoom_rect.size(), new_total_scale);
      cc::SharedBitmapManager* manager =
          RenderThreadImpl::current()->shared_bitmap_manager();
      std::unique_ptr<cc::SharedBitmap> shared_bitmap =
          manager->AllocateSharedBitmap(canvas_size);
      CHECK(!!shared_bitmap);
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
        webwidget_->paintIgnoringCompositing(&canvas, zoom_rect);
      }

      gfx::Rect zoom_rect_in_screen =
          zoom_rect - gfx::Vector2d(inner_viewport_offset.width,
                                    inner_viewport_offset.height);

      gfx::Rect physical_window_zoom_rect = gfx::ToEnclosingRect(
          ClientRectToPhysicalWindowRect(gfx::RectF(zoom_rect_in_screen)));

      Send(new ViewHostMsg_ShowDisambiguationPopup(
          GetRoutingID(), physical_window_zoom_rect, canvas_size,
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
#endif  // defined(OS_ANDROID)

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
  ResizeParams params;
  params.screen_info = screen_info_;
  params.screen_info.deviceScaleFactor = factor;
  params.new_size = size();
  params.visible_viewport_size = visible_viewport_size_;
  params.physical_backing_size = gfx::ScaleToCeiledSize(size(), factor);
  params.top_controls_shrink_blink_size = false;
  params.top_controls_height = 0.f;
  params.resizer_rect = WebRect();
  params.is_fullscreen_granted = is_fullscreen_granted();
  params.display_mode = display_mode_;
  OnResize(params);
}

void RenderViewImpl::ForceResizeForTesting(const gfx::Size& new_size) {
  gfx::Rect new_window_rect(rootWindowRect().x,
                            rootWindowRect().y,
                            new_size.width(),
                            new_size.height());
  SetWindowRectSynchronously(new_window_rect);
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
    Send(new ViewHostMsg_UpdateFaviconURL(GetRoutingID(), urls));
}

void RenderViewImpl::DidStopLoadingIcons() {
  int icon_types = WebIconURL::TypeFavicon | WebIconURL::TypeTouchPrecomposed |
      WebIconURL::TypeTouch;

  // Favicons matter only for the top-level frame. If it is a WebRemoteFrame,
  // just return early.
  if (webview()->mainFrame()->isWebRemoteFrame())
    return;

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

void RenderViewImpl::UpdateWebViewWithDeviceScaleFactor() {
  if (!webview())
    return;
  if (IsUseZoomForDSFEnabled()) {
    webview()->setZoomFactorForDeviceScaleFactor(device_scale_factor_);
  } else {
    webview()->setDeviceScaleFactor(device_scale_factor_);
  }
  webview()->settings()->setPreferCompositingToLCDTextEnabled(
      PreferCompositingToLCDText(compositor_deps_, device_scale_factor_));
}

#if defined(ENABLE_PLUGINS)
PepperPluginInstanceImpl* RenderViewImpl::GetFocusedPepperPlugin() {
  blink::WebFrame* frame = GetWebView()->mainFrame();

  while (frame) {
    if (frame->isWebLocalFrame()) {
      RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(frame);
      if (render_frame->focused_pepper_plugin())
        return render_frame->focused_pepper_plugin();
    }
    frame = frame->traverseNext(false);
  }

  return nullptr;
}
#endif

}  // namespace content

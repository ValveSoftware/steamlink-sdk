// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_plugin_instance_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/base/latency_info_swap_promise.h"
#include "cc/layers/texture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/content_constants_internal.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/pepper/common.h"
#include "content/renderer/pepper/content_decryptor_delegate.h"
#include "content/renderer/pepper/event_conversion.h"
#include "content/renderer/pepper/fullscreen_container.h"
#include "content/renderer/pepper/gfx_conversion.h"
#include "content/renderer/pepper/host_dispatcher_wrapper.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/message_channel.h"
#include "content/renderer/pepper/npapi_glue.h"
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_compositor_host.h"
#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"
#include "content/renderer/pepper/pepper_graphics_2d_host.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/pepper/pepper_url_loader_host.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/plugin_object.h"
#include "content/renderer/pepper/ppb_buffer_impl.h"
#include "content/renderer/pepper/ppb_graphics_3d_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/pepper/url_request_info_util.h"
#include "content/renderer/pepper/url_response_info_util.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/sad_plugin.h"
#include "media/base/audio_hardware_config.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_text_input_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/c/private/ppb_find_private.h"
#include "ppapi/c/private/ppp_find_private.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/proxy/uma_private_resource.h"
#include "ppapi/proxy/url_loader_resource.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_gamepad_shared.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/ppb_url_util_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/ppp_instance_combined.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "printing/metafile.h"
#include "printing/metafile_skia_wrapper.h"
#include "printing/units.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebPrintScalingOption.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "v8/include/v8.h"

#if defined(OS_CHROMEOS)
#include "ui/events/keycodes/keyboard_codes_posix.h"
#endif

#if defined(OS_MACOSX)
#include "printing/metafile_impl.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "base/metrics/histogram.h"
#include "base/win/windows_version.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/gdi_util.h"
#endif

using base::StringPrintf;
using ppapi::InputEventData;
using ppapi::PpapiGlobals;
using ppapi::PPB_InputEvent_Shared;
using ppapi::PPB_View_Shared;
using ppapi::PPP_Instance_Combined;
using ppapi::Resource;
using ppapi::ScopedPPResource;
using ppapi::ScopedPPVar;
using ppapi::StringVar;
using ppapi::TrackedCallback;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Gamepad_API;
using ppapi::thunk::PPB_Graphics2D_API;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::Var;
using ppapi::ArrayBufferVar;
using ppapi::ViewData;
using blink::WebBindings;
using blink::WebCanvas;
using blink::WebCursorInfo;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPrintParams;
using blink::WebPrintScalingOption;
using blink::WebScopedUserGesture;
using blink::WebString;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLLoaderClient;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebUserGestureIndicator;
using blink::WebUserGestureToken;
using blink::WebView;

namespace content {

#if defined(OS_WIN)
// Exported by pdf.dll
typedef bool (*RenderPDFPageToDCProc)(const unsigned char* pdf_buffer,
                                      int buffer_size,
                                      int page_number,
                                      HDC dc,
                                      int dpi_x,
                                      int dpi_y,
                                      int bounds_origin_x,
                                      int bounds_origin_y,
                                      int bounds_width,
                                      int bounds_height,
                                      bool fit_to_bounds,
                                      bool stretch_to_bounds,
                                      bool keep_aspect_ratio,
                                      bool center_in_bounds,
                                      bool autorotate);

void DrawEmptyRectangle(HDC dc) {
  // TODO(sanjeevr): This is a temporary hack. If we output a JPEG
  // to the EMF, the EnumEnhMetaFile call fails in the browser
  // process. The failure also happens if we output nothing here.
  // We need to investigate the reason for this failure and fix it.
  // In the meantime this temporary hack of drawing an empty
  // rectangle in the DC gets us by.
  Rectangle(dc, 0, 0, 0, 0);
}
#endif  // defined(OS_WIN)

namespace {

// Check PP_TextInput_Type and ui::TextInputType are kept in sync.
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_NONE) == int(PP_TEXTINPUT_TYPE_NONE),
               mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_TEXT) == int(PP_TEXTINPUT_TYPE_TEXT),
               mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_PASSWORD) ==
                   int(PP_TEXTINPUT_TYPE_PASSWORD),
               mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_SEARCH) == int(PP_TEXTINPUT_TYPE_SEARCH),
               mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_EMAIL) == int(PP_TEXTINPUT_TYPE_EMAIL),
               mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_NUMBER) == int(PP_TEXTINPUT_TYPE_NUMBER),
               mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_TELEPHONE) ==
                   int(PP_TEXTINPUT_TYPE_TELEPHONE),
               mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_URL) == int(PP_TEXTINPUT_TYPE_URL),
               mismatching_enums);

// The default text input type is to regard the plugin always accept text input.
// This is for allowing users to use input methods even on completely-IME-
// unaware plugins (e.g., PPAPI Flash or PDF plugin for M16).
// Plugins need to explicitly opt out the text input mode if they know
// that they don't accept texts.
const ui::TextInputType kPluginDefaultTextInputType = ui::TEXT_INPUT_TYPE_TEXT;

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, np_name)       \
  COMPILE_ASSERT(static_cast<int>(WebCursorInfo::webkit_name) == \
                     static_cast<int>(np_name),                  \
                 mismatching_enums)

#define COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(webkit_name, pp_name)     \
  COMPILE_ASSERT(static_cast<int>(webkit_name) == static_cast<int>(pp_name), \
                 mismatching_enums)

// <embed>/<object> attributes.
const char kWidth[] = "width";
const char kHeight[] = "height";
const char kBorder[] = "border";  // According to w3c, deprecated.
const char kStyle[] = "style";

COMPILE_ASSERT_MATCHING_ENUM(TypePointer, PP_MOUSECURSOR_TYPE_POINTER);
COMPILE_ASSERT_MATCHING_ENUM(TypeCross, PP_MOUSECURSOR_TYPE_CROSS);
COMPILE_ASSERT_MATCHING_ENUM(TypeHand, PP_MOUSECURSOR_TYPE_HAND);
COMPILE_ASSERT_MATCHING_ENUM(TypeIBeam, PP_MOUSECURSOR_TYPE_IBEAM);
COMPILE_ASSERT_MATCHING_ENUM(TypeWait, PP_MOUSECURSOR_TYPE_WAIT);
COMPILE_ASSERT_MATCHING_ENUM(TypeHelp, PP_MOUSECURSOR_TYPE_HELP);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastResize, PP_MOUSECURSOR_TYPE_EASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthResize, PP_MOUSECURSOR_TYPE_NORTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastResize,
                             PP_MOUSECURSOR_TYPE_NORTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestResize,
                             PP_MOUSECURSOR_TYPE_NORTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthResize, PP_MOUSECURSOR_TYPE_SOUTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastResize,
                             PP_MOUSECURSOR_TYPE_SOUTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestResize,
                             PP_MOUSECURSOR_TYPE_SOUTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestResize, PP_MOUSECURSOR_TYPE_WESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthSouthResize,
                             PP_MOUSECURSOR_TYPE_NORTHSOUTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastWestResize,
                             PP_MOUSECURSOR_TYPE_EASTWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastSouthWestResize,
                             PP_MOUSECURSOR_TYPE_NORTHEASTSOUTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestSouthEastResize,
                             PP_MOUSECURSOR_TYPE_NORTHWESTSOUTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeColumnResize,
                             PP_MOUSECURSOR_TYPE_COLUMNRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeRowResize, PP_MOUSECURSOR_TYPE_ROWRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeMiddlePanning,
                             PP_MOUSECURSOR_TYPE_MIDDLEPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastPanning, PP_MOUSECURSOR_TYPE_EASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthPanning,
                             PP_MOUSECURSOR_TYPE_NORTHPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastPanning,
                             PP_MOUSECURSOR_TYPE_NORTHEASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestPanning,
                             PP_MOUSECURSOR_TYPE_NORTHWESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthPanning,
                             PP_MOUSECURSOR_TYPE_SOUTHPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastPanning,
                             PP_MOUSECURSOR_TYPE_SOUTHEASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestPanning,
                             PP_MOUSECURSOR_TYPE_SOUTHWESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestPanning, PP_MOUSECURSOR_TYPE_WESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeMove, PP_MOUSECURSOR_TYPE_MOVE);
COMPILE_ASSERT_MATCHING_ENUM(TypeVerticalText,
                             PP_MOUSECURSOR_TYPE_VERTICALTEXT);
COMPILE_ASSERT_MATCHING_ENUM(TypeCell, PP_MOUSECURSOR_TYPE_CELL);
COMPILE_ASSERT_MATCHING_ENUM(TypeContextMenu, PP_MOUSECURSOR_TYPE_CONTEXTMENU);
COMPILE_ASSERT_MATCHING_ENUM(TypeAlias, PP_MOUSECURSOR_TYPE_ALIAS);
COMPILE_ASSERT_MATCHING_ENUM(TypeProgress, PP_MOUSECURSOR_TYPE_PROGRESS);
COMPILE_ASSERT_MATCHING_ENUM(TypeNoDrop, PP_MOUSECURSOR_TYPE_NODROP);
COMPILE_ASSERT_MATCHING_ENUM(TypeCopy, PP_MOUSECURSOR_TYPE_COPY);
COMPILE_ASSERT_MATCHING_ENUM(TypeNone, PP_MOUSECURSOR_TYPE_NONE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNotAllowed, PP_MOUSECURSOR_TYPE_NOTALLOWED);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomIn, PP_MOUSECURSOR_TYPE_ZOOMIN);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomOut, PP_MOUSECURSOR_TYPE_ZOOMOUT);
COMPILE_ASSERT_MATCHING_ENUM(TypeGrab, PP_MOUSECURSOR_TYPE_GRAB);
COMPILE_ASSERT_MATCHING_ENUM(TypeGrabbing, PP_MOUSECURSOR_TYPE_GRABBING);
// Do not assert WebCursorInfo::TypeCustom == PP_CURSORTYPE_CUSTOM;
// PP_CURSORTYPE_CUSTOM is pinned to allow new cursor types.

COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(blink::WebPrintScalingOptionNone,
                                           PP_PRINTSCALINGOPTION_NONE);
COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(
    blink::WebPrintScalingOptionFitToPrintableArea,
    PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA);
COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(
    blink::WebPrintScalingOptionSourceSize,
    PP_PRINTSCALINGOPTION_SOURCE_SIZE);

// Sets |*security_origin| to be the WebKit security origin associated with the
// document containing the given plugin instance. On success, returns true. If
// the instance is invalid, returns false and |*security_origin| will be
// unchanged.
bool SecurityOriginForInstance(PP_Instance instance_id,
                               blink::WebSecurityOrigin* security_origin) {
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  WebElement plugin_element = instance->container()->element();
  *security_origin = plugin_element.document().securityOrigin();
  return true;
}

// Convert the given vector to an array of C-strings. The strings in the
// returned vector are only guaranteed valid so long as the vector of strings
// is not modified.
scoped_ptr<const char* []> StringVectorToArgArray(
    const std::vector<std::string>& vector) {
  scoped_ptr<const char * []> array(new const char* [vector.size()]);
  for (size_t i = 0; i < vector.size(); ++i)
    array[i] = vector[i].c_str();
  return array.Pass();
}

// Returns true if this is a "system reserved" key which should not be sent to
// a plugin. Some poorly behaving plugins (like Flash) incorrectly report that
// they handle all keys sent to them. This can prevent keystrokes from working
// for things like screen brightness and volume control.
bool IsReservedSystemInputEvent(const blink::WebInputEvent& event) {
#if defined(OS_CHROMEOS)
  if (event.type != WebInputEvent::KeyDown &&
      event.type != WebInputEvent::KeyUp)
    return false;
  const blink::WebKeyboardEvent& key_event =
      static_cast<const blink::WebKeyboardEvent&>(event);
  switch (key_event.windowsKeyCode) {
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_DOWN:
    case ui::VKEY_KBD_BRIGHTNESS_UP:
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_VOLUME_UP:
      return true;
    default:
      return false;
  }
#endif  // defined(OS_CHROMEOS)
  return false;
}

class PluginInstanceLockTarget : public MouseLockDispatcher::LockTarget {
 public:
  PluginInstanceLockTarget(PepperPluginInstanceImpl* plugin)
      : plugin_(plugin) {}

  virtual void OnLockMouseACK(bool succeeded) OVERRIDE {
    plugin_->OnLockMouseACK(succeeded);
  }

  virtual void OnMouseLockLost() OVERRIDE { plugin_->OnMouseLockLost(); }

  virtual bool HandleMouseLockedInputEvent(const blink::WebMouseEvent& event)
      OVERRIDE {
    plugin_->HandleMouseLockedInputEvent(event);
    return true;
  }

 private:
  PepperPluginInstanceImpl* plugin_;
};

void InitLatencyInfo(ui::LatencyInfo* new_latency,
                     const ui::LatencyInfo* old_latency,
                     blink::WebInputEvent::Type type,
                     int64 input_sequence) {
  new_latency->AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_BEGIN_PLUGIN_COMPONENT,
      0,
      input_sequence);
  new_latency->TraceEventType(WebInputEventTraits::GetName(type));
  if (old_latency) {
    new_latency->CopyLatencyFrom(*old_latency,
                                 ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
    new_latency->CopyLatencyFrom(*old_latency,
                                 ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  }
}

}  // namespace

// static
PepperPluginInstanceImpl* PepperPluginInstanceImpl::Create(
    RenderFrameImpl* render_frame,
    PluginModule* module,
    WebPluginContainer* container,
    const GURL& plugin_url) {
  base::Callback<const void*(const char*)> get_plugin_interface_func =
      base::Bind(&PluginModule::GetPluginInterface, module);
  PPP_Instance_Combined* ppp_instance_combined =
      PPP_Instance_Combined::Create(get_plugin_interface_func);
  if (!ppp_instance_combined)
    return NULL;
  return new PepperPluginInstanceImpl(
      render_frame, module, ppp_instance_combined, container, plugin_url);
}

PepperPluginInstanceImpl::ExternalDocumentLoader::ExternalDocumentLoader()
    : finished_loading_(false) {}

PepperPluginInstanceImpl::ExternalDocumentLoader::~ExternalDocumentLoader() {}

void PepperPluginInstanceImpl::ExternalDocumentLoader::ReplayReceivedData(
    WebURLLoaderClient* document_loader) {
  for (std::list<std::string>::iterator it = data_.begin(); it != data_.end();
       ++it) {
    document_loader->didReceiveData(
        NULL, it->c_str(), it->length(), 0 /* encoded_data_length */);
  }
  if (finished_loading_) {
    document_loader->didFinishLoading(
        NULL,
        0 /* finish_time */,
        blink::WebURLLoaderClient::kUnknownEncodedDataLength);
  }
  if (error_.get()) {
    document_loader->didFail(NULL, *error_);
  }
}

void PepperPluginInstanceImpl::ExternalDocumentLoader::didReceiveData(
    WebURLLoader* loader,
    const char* data,
    int data_length,
    int encoded_data_length) {
  data_.push_back(std::string(data, data_length));
}

void PepperPluginInstanceImpl::ExternalDocumentLoader::didFinishLoading(
    WebURLLoader* loader,
    double finish_time,
    int64_t total_encoded_data_length) {
  DCHECK(!finished_loading_);
  finished_loading_ = true;
}

void PepperPluginInstanceImpl::ExternalDocumentLoader::didFail(
    WebURLLoader* loader,
    const WebURLError& error) {
  DCHECK(!error_.get());
  error_.reset(new WebURLError(error));
}

PepperPluginInstanceImpl::GamepadImpl::GamepadImpl()
    : Resource(ppapi::Resource::Untracked()) {}

PepperPluginInstanceImpl::GamepadImpl::~GamepadImpl() {}

PPB_Gamepad_API* PepperPluginInstanceImpl::GamepadImpl::AsPPB_Gamepad_API() {
  return this;
}

void PepperPluginInstanceImpl::GamepadImpl::Sample(
    PP_Instance instance,
    PP_GamepadsSampleData* data) {
  blink::WebGamepads webkit_data;
  RenderThreadImpl::current()->SampleGamepads(&webkit_data);
  ConvertWebKitGamepadData(bit_cast<ppapi::WebKitGamepads>(webkit_data), data);
}

PepperPluginInstanceImpl::PepperPluginInstanceImpl(
    RenderFrameImpl* render_frame,
    PluginModule* module,
    ppapi::PPP_Instance_Combined* instance_interface,
    WebPluginContainer* container,
    const GURL& plugin_url)
    : RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      module_(module),
      instance_interface_(instance_interface),
      pp_instance_(0),
      container_(container),
      layer_bound_to_fullscreen_(false),
      layer_is_hardware_(false),
      plugin_url_(plugin_url),
      full_frame_(false),
      sent_initial_did_change_view_(false),
      bound_graphics_2d_platform_(NULL),
      bound_compositor_(NULL),
      has_webkit_focus_(false),
      has_content_area_focus_(false),
      find_identifier_(-1),
      plugin_find_interface_(NULL),
      plugin_input_event_interface_(NULL),
      plugin_mouse_lock_interface_(NULL),
      plugin_pdf_interface_(NULL),
      plugin_private_interface_(NULL),
      plugin_selection_interface_(NULL),
      plugin_textinput_interface_(NULL),
      plugin_zoom_interface_(NULL),
      checked_for_plugin_input_event_interface_(false),
      checked_for_plugin_pdf_interface_(false),
      gamepad_impl_(new GamepadImpl()),
      uma_private_impl_(NULL),
      plugin_print_interface_(NULL),
      plugin_graphics_3d_interface_(NULL),
      always_on_top_(false),
      fullscreen_container_(NULL),
      flash_fullscreen_(false),
      desired_fullscreen_state_(false),
      sad_plugin_(NULL),
      input_event_mask_(0),
      filtered_input_event_mask_(0),
      text_input_type_(kPluginDefaultTextInputType),
      text_input_caret_(0, 0, 0, 0),
      text_input_caret_bounds_(0, 0, 0, 0),
      text_input_caret_set_(false),
      selection_caret_(0),
      selection_anchor_(0),
      pending_user_gesture_(0.0),
      document_loader_(NULL),
      external_document_load_(false),
      npp_(new NPP_t),
      isolate_(v8::Isolate::GetCurrent()),
      is_deleted_(false),
      last_input_number_(0),
      is_tracking_latency_(false),
      view_change_weak_ptr_factory_(this),
      weak_factory_(this) {
  pp_instance_ = HostGlobals::Get()->AddInstance(this);

  memset(&current_print_settings_, 0, sizeof(current_print_settings_));
  module_->InstanceCreated(this);

  if (render_frame) {  // NULL in tests
    render_frame->render_view()->PepperInstanceCreated(this);
    // Bind a callback now so that we can inform the RenderViewImpl when we are
    // destroyed. This works around a temporary problem stemming from work to
    // move parts of RenderViewImpl in to RenderFrameImpl (see
    // crbug.com/245126). If destruction happens in this order:
    //  1) RenderFrameImpl
    //  2) PepperPluginInstanceImpl
    //  3) RenderViewImpl
    // Then after 1), the PepperPluginInstanceImpl doesn't have any way to talk
    // to the RenderViewImpl. But when the instance is destroyed, it still
    // needs to inform the RenderViewImpl that it has gone away, otherwise
    // between (2) and (3), the RenderViewImpl will still have the dead
    // instance in its active set, and so might make calls on the deleted
    // instance. See crbug.com/343576 for more information. Once the plugin
    // calls move entirely from RenderViewImpl in to RenderFrameImpl, this
    // can be a little bit simplified by instead making a direct call on
    // RenderFrameImpl in the destructor (but only if render_frame_ is valid).
    instance_deleted_callback_ =
        base::Bind(&RenderViewImpl::PepperInstanceDeleted,
                   render_frame->render_view()->AsWeakPtr(),
                   base::Unretained(this));
    view_data_.is_page_visible = !render_frame_->GetRenderWidget()->is_hidden();

    // Set the initial focus.
    SetContentAreaFocus(render_frame_->GetRenderWidget()->has_focus());

    if (!module_->IsProxied()) {
      PepperBrowserConnection* browser_connection =
          PepperBrowserConnection::Get(render_frame_);
      browser_connection->DidCreateInProcessInstance(
          pp_instance(),
          render_frame_->GetRoutingID(),
          container_->element().document().url(),
          GetPluginURL());
    }
  }

  RendererPpapiHostImpl* host_impl = module_->renderer_ppapi_host();
  resource_creation_ = host_impl->CreateInProcessResourceCreationAPI(this);

  if (GetContentClient()->renderer() &&  // NULL in unit tests.
      GetContentClient()->renderer()->IsExternalPepperPlugin(module->name()))
    external_document_load_ = true;
}

PepperPluginInstanceImpl::~PepperPluginInstanceImpl() {
  DCHECK(!fullscreen_container_);

  // Free all the plugin objects. This will automatically clear the back-
  // pointer from the NPObject so WebKit can't call into the plugin any more.
  //
  // Swap out the set so we can delete from it (the objects will try to
  // unregister themselves inside the delete call).
  PluginObjectSet plugin_object_copy;
  live_plugin_objects_.swap(plugin_object_copy);
  for (PluginObjectSet::iterator i = plugin_object_copy.begin();
       i != plugin_object_copy.end();
       ++i)
    delete *i;

  if (TrackedCallback::IsPending(lock_mouse_callback_))
    lock_mouse_callback_->Abort();

  if (!instance_deleted_callback_.is_null())
    instance_deleted_callback_.Run();

  if (!module_->IsProxied() && render_frame_) {
    PepperBrowserConnection* browser_connection =
        PepperBrowserConnection::Get(render_frame_);
    browser_connection->DidDeleteInProcessInstance(pp_instance());
  }

  UnSetAndDeleteLockTargetAdapter();
  module_->InstanceDeleted(this);
  // If we switched from the NaCl plugin module, notify it too.
  if (original_module_.get())
    original_module_->InstanceDeleted(this);

  // This should be last since some of the above "instance deleted" calls will
  // want to look up in the global map to get info off of our object.
  HostGlobals::Get()->InstanceDeleted(pp_instance_);
}

// NOTE: Any of these methods that calls into the plugin needs to take into
// account that the plugin may use Var to remove the <embed> from the DOM, which
// will make the PepperWebPluginImpl drop its reference, usually the last one.
// If a method needs to access a member of the instance after the call has
// returned, then it needs to keep its own reference on the stack.

void PepperPluginInstanceImpl::Delete() {
  is_deleted_ = true;

  if (render_frame_ &&
      render_frame_->render_view()->plugin_find_handler() == this) {
    render_frame_->render_view()->set_plugin_find_handler(NULL);
  }

  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  // Force the MessageChannel to release its "passthrough object" which should
  // release our last reference to the "InstanceObject" and will probably
  // destroy it. We want to do this prior to calling DidDestroy in case the
  // destructor of the instance object tries to use the instance.
  message_channel_->SetPassthroughObject(NULL);
  // If this is a NaCl plugin instance, shut down the NaCl plugin by calling
  // its DidDestroy. Don't call DidDestroy on the untrusted plugin instance,
  // since there is little that it can do at this point.
  if (original_instance_interface_)
    original_instance_interface_->DidDestroy(pp_instance());
  else
    instance_interface_->DidDestroy(pp_instance());
  // Ensure we don't attempt to call functions on the destroyed instance.
  original_instance_interface_.reset();
  instance_interface_.reset();

  if (fullscreen_container_) {
    fullscreen_container_->Destroy();
    fullscreen_container_ = NULL;
  }

  // Force-unbind any Graphics. In the case of Graphics2D, if the plugin
  // leaks the graphics 2D, it may actually get cleaned up after our
  // destruction, so we need its pointers to be up-to-date.
  BindGraphics(pp_instance(), 0);
  container_ = NULL;
}

bool PepperPluginInstanceImpl::is_deleted() const { return is_deleted_; }

void PepperPluginInstanceImpl::Paint(WebCanvas* canvas,
                                     const gfx::Rect& plugin_rect,
                                     const gfx::Rect& paint_rect) {
  TRACE_EVENT0("ppapi", "PluginInstance::Paint");
  if (module()->is_crashed()) {
    // Crashed plugin painting.
    if (!sad_plugin_)  // Lazily initialize bitmap.
      sad_plugin_ = GetContentClient()->renderer()->GetSadPluginBitmap();
    if (sad_plugin_)
      PaintSadPlugin(canvas, plugin_rect, *sad_plugin_);
    return;
  }

  if (bound_graphics_2d_platform_)
    bound_graphics_2d_platform_->Paint(canvas, plugin_rect, paint_rect);
}

void PepperPluginInstanceImpl::InvalidateRect(const gfx::Rect& rect) {
  if (fullscreen_container_) {
    if (rect.IsEmpty())
      fullscreen_container_->Invalidate();
    else
      fullscreen_container_->InvalidateRect(rect);
  } else {
    if (!container_ || view_data_.rect.size.width == 0 ||
        view_data_.rect.size.height == 0)
      return;  // Nothing to do.
    if (rect.IsEmpty())
      container_->invalidate();
    else
      container_->invalidateRect(rect);
  }

  cc::Layer* layer =
      texture_layer_ ? texture_layer_.get() : compositor_layer_.get();
  if (layer) {
    if (rect.IsEmpty()) {
      layer->SetNeedsDisplay();
    } else {
      layer->SetNeedsDisplayRect(rect);
    }
  }
}

void PepperPluginInstanceImpl::ScrollRect(int dx,
                                          int dy,
                                          const gfx::Rect& rect) {
  cc::Layer* layer =
      texture_layer_ ? texture_layer_.get() : compositor_layer_.get();
  if (layer) {
    InvalidateRect(rect);
  } else if (fullscreen_container_) {
    fullscreen_container_->ScrollRect(dx, dy, rect);
  } else {
    if (full_frame_ && !IsViewAccelerated()) {
      container_->scrollRect(dx, dy, rect);
    } else {
      // Can't do optimized scrolling since there could be other elements on top
      // of us or the view renders via the accelerated compositor which is
      // incompatible with the move and backfill scrolling model.
      InvalidateRect(rect);
    }
  }
}

void PepperPluginInstanceImpl::CommitBackingTexture() {
  if (!texture_layer_.get())
    return;
  gpu::Mailbox mailbox;
  uint32 sync_point = 0;
  bound_graphics_3d_->GetBackingMailbox(&mailbox, &sync_point);
  DCHECK(!mailbox.IsZero());
  DCHECK_NE(sync_point, 0u);
  texture_layer_->SetTextureMailboxWithoutReleaseCallback(
      cc::TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point));
  texture_layer_->SetNeedsDisplay();
}

void PepperPluginInstanceImpl::InstanceCrashed() {
  // Force free all resources and vars.
  HostGlobals::Get()->InstanceCrashed(pp_instance());

  // Free any associated graphics.
  SetFullscreen(false);
  FlashSetFullscreen(false, false);
  // Unbind current 2D or 3D graphics context.
  BindGraphics(pp_instance(), 0);
  InvalidateRect(gfx::Rect());

  if (content_decryptor_delegate_) {
    content_decryptor_delegate_->InstanceCrashed();
    content_decryptor_delegate_.reset();
  }

  if (render_frame_)
    render_frame_->PluginCrashed(module_->path(), module_->GetPeerProcessId());
  UnSetAndDeleteLockTargetAdapter();
}

static void SetGPUHistogram(const ppapi::Preferences& prefs,
                            const std::vector<std::string>& arg_names,
                            const std::vector<std::string>& arg_values) {
// Calculate a histogram to let us determine how likely people are to try to
// run Stage3D content on machines that have it blacklisted.
#if defined(OS_WIN)
  bool needs_gpu = false;
  bool is_xp = base::win::GetVersion() <= base::win::VERSION_XP;

  for (size_t i = 0; i < arg_names.size(); i++) {
    if (arg_names[i] == "wmode") {
      // In theory content other than Flash could have a "wmode" argument,
      // but that's pretty unlikely.
      if (arg_values[i] == "direct" || arg_values[i] == "gpu")
        needs_gpu = true;
      break;
    }
  }
  // 0 : No 3D content and GPU is blacklisted
  // 1 : No 3D content and GPU is not blacklisted
  // 2 : 3D content but GPU is blacklisted
  // 3 : 3D content and GPU is not blacklisted
  // 4 : No 3D content and GPU is blacklisted on XP
  // 5 : No 3D content and GPU is not blacklisted on XP
  // 6 : 3D content but GPU is blacklisted on XP
  // 7 : 3D content and GPU is not blacklisted on XP
  UMA_HISTOGRAM_ENUMERATION(
      "Flash.UsesGPU", is_xp * 4 + needs_gpu * 2 + prefs.is_webgl_supported, 8);
#endif
}

bool PepperPluginInstanceImpl::Initialize(
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    bool full_frame) {
  if (!render_frame_)
    return false;
  message_channel_.reset(new MessageChannel(this));

  full_frame_ = full_frame;

  UpdateTouchEventRequest();
  container_->setWantsWheelEvents(IsAcceptingWheelEvents());

  SetGPUHistogram(
      ppapi::Preferences(render_frame_->render_view()->webkit_preferences()),
      arg_names,
      arg_values);

  argn_ = arg_names;
  argv_ = arg_values;
  scoped_ptr<const char * []> argn_array(StringVectorToArgArray(argn_));
  scoped_ptr<const char * []> argv_array(StringVectorToArgArray(argv_));
  bool success = PP_ToBool(instance_interface_->DidCreate(
      pp_instance(), argn_.size(), argn_array.get(), argv_array.get()));
  // If this is a plugin that hosts external plugins, we should delay messages
  // so that the child plugin that's created later will receive all the
  // messages. (E.g., NaCl trusted plugin starting a child NaCl app.)
  //
  // A host for external plugins will call ResetAsProxied later, at which point
  // we can Start() the message_channel_.
  if (success && (!module_->renderer_ppapi_host()->IsExternalPluginHost()))
    message_channel_->Start();
  return success;
}

bool PepperPluginInstanceImpl::HandleDocumentLoad(
    const blink::WebURLResponse& response) {
  DCHECK(!document_loader_);
  if (external_document_load_) {
    // The external proxy isn't available, so save the response and record
    // document load notifications for later replay.
    external_document_response_ = response;
    external_document_loader_.reset(new ExternalDocumentLoader());
    document_loader_ = external_document_loader_.get();
    return true;
  }

  if (module()->is_crashed()) {
    // Don't create a resource for a crashed plugin.
    container()->element().document().frame()->stopLoading();
    return false;
  }

  DCHECK(!document_loader_);

  // Create a loader resource host for this load. Note that we have to set
  // the document_loader before issuing the in-process
  // PPP_Instance.HandleDocumentLoad call below, since this may reentrantly
  // call into the instance and expect it to be valid.
  RendererPpapiHostImpl* host_impl = module_->renderer_ppapi_host();
  PepperURLLoaderHost* loader_host =
      new PepperURLLoaderHost(host_impl, true, pp_instance(), 0);
  // TODO(teravest): Remove set_document_loader() from instance and clean up
  // this relationship.
  set_document_loader(loader_host);
  loader_host->didReceiveResponse(NULL, response);

  // This host will be pending until the resource object attaches to it.
  //
  // PpapiHost now owns the pointer to loader_host, so we don't have to worry
  // about managing it.
  int pending_host_id = host_impl->GetPpapiHost()->AddPendingResourceHost(
      scoped_ptr<ppapi::host::ResourceHost>(loader_host));
  DCHECK(pending_host_id);

  DataFromWebURLResponse(
      host_impl,
      pp_instance(),
      response,
      base::Bind(&PepperPluginInstanceImpl::DidDataFromWebURLResponse,
                 weak_factory_.GetWeakPtr(),
                 response,
                 pending_host_id));

  // If the load was not abandoned, document_loader_ will now be set. It's
  // possible that the load was canceled by now and document_loader_ was
  // already nulled out.
  return true;
}

bool PepperPluginInstanceImpl::SendCompositionEventToPlugin(
    PP_InputEvent_Type type,
    const base::string16& text) {
  std::vector<blink::WebCompositionUnderline> empty;
  return SendCompositionEventWithUnderlineInformationToPlugin(
      type,
      text,
      empty,
      static_cast<int>(text.size()),
      static_cast<int>(text.size()));
}

bool
PepperPluginInstanceImpl::SendCompositionEventWithUnderlineInformationToPlugin(
    PP_InputEvent_Type type,
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);

  if (!LoadInputEventInterface())
    return false;

  PP_InputEvent_Class event_class = PP_INPUTEVENT_CLASS_IME;
  if (!(filtered_input_event_mask_ & event_class) &&
      !(input_event_mask_ & event_class))
    return false;

  ppapi::InputEventData event;
  event.event_type = type;
  event.event_time_stamp =
      ppapi::TimeTicksToPPTimeTicks(base::TimeTicks::Now());

  // Convert UTF16 text to UTF8 with offset conversion.
  std::vector<size_t> utf16_offsets;
  utf16_offsets.push_back(selection_start);
  utf16_offsets.push_back(selection_end);
  for (size_t i = 0; i < underlines.size(); ++i) {
    utf16_offsets.push_back(underlines[i].startOffset);
    utf16_offsets.push_back(underlines[i].endOffset);
  }
  std::vector<size_t> utf8_offsets(utf16_offsets);
  event.character_text = base::UTF16ToUTF8AndAdjustOffsets(text, &utf8_offsets);

  // Set the converted selection range.
  event.composition_selection_start =
      (utf8_offsets[0] == std::string::npos ? event.character_text.size()
                                            : utf8_offsets[0]);
  event.composition_selection_end =
      (utf8_offsets[1] == std::string::npos ? event.character_text.size()
                                            : utf8_offsets[1]);

  // Set the converted segmentation points.
  // Be sure to add 0 and size(), and remove duplication or errors.
  std::set<size_t> offset_set(utf8_offsets.begin() + 2, utf8_offsets.end());
  offset_set.insert(0);
  offset_set.insert(event.character_text.size());
  offset_set.erase(std::string::npos);
  event.composition_segment_offsets.assign(offset_set.begin(),
                                           offset_set.end());

  // Set the composition target.
  for (size_t i = 0; i < underlines.size(); ++i) {
    if (underlines[i].thick) {
      std::vector<uint32_t>::iterator it =
          std::find(event.composition_segment_offsets.begin(),
                    event.composition_segment_offsets.end(),
                    utf8_offsets[2 * i + 2]);
      if (it != event.composition_segment_offsets.end()) {
        event.composition_target_segment =
            it - event.composition_segment_offsets.begin();
        break;
      }
    }
  }

  // Send the event.
  bool handled = false;
  if (filtered_input_event_mask_ & event_class)
    event.is_filtered = true;
  else
    handled = true;  // Unfiltered events are assumed to be handled.
  scoped_refptr<PPB_InputEvent_Shared> event_resource(
      new PPB_InputEvent_Shared(ppapi::OBJECT_IS_IMPL, pp_instance(), event));
  handled |= PP_ToBool(plugin_input_event_interface_->HandleInputEvent(
      pp_instance(), event_resource->pp_resource()));
  return handled;
}

void PepperPluginInstanceImpl::RequestInputEventsHelper(
    uint32_t event_classes) {
  if (event_classes & PP_INPUTEVENT_CLASS_TOUCH)
    UpdateTouchEventRequest();
  if (event_classes & PP_INPUTEVENT_CLASS_WHEEL)
    container_->setWantsWheelEvents(IsAcceptingWheelEvents());
}

bool PepperPluginInstanceImpl::HandleCompositionStart(
    const base::string16& text) {
  return SendCompositionEventToPlugin(PP_INPUTEVENT_TYPE_IME_COMPOSITION_START,
                                      text);
}

bool PepperPluginInstanceImpl::HandleCompositionUpdate(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  return SendCompositionEventWithUnderlineInformationToPlugin(
      PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE,
      text,
      underlines,
      selection_start,
      selection_end);
}

bool PepperPluginInstanceImpl::HandleCompositionEnd(
    const base::string16& text) {
  return SendCompositionEventToPlugin(PP_INPUTEVENT_TYPE_IME_COMPOSITION_END,
                                      text);
}

bool PepperPluginInstanceImpl::HandleTextInput(const base::string16& text) {
  return SendCompositionEventToPlugin(PP_INPUTEVENT_TYPE_IME_TEXT, text);
}

void PepperPluginInstanceImpl::GetSurroundingText(base::string16* text,
                                                  gfx::Range* range) const {
  std::vector<size_t> offsets;
  offsets.push_back(selection_anchor_);
  offsets.push_back(selection_caret_);
  *text = base::UTF8ToUTF16AndAdjustOffsets(surrounding_text_, &offsets);
  range->set_start(offsets[0] == base::string16::npos ? text->size()
                                                      : offsets[0]);
  range->set_end(offsets[1] == base::string16::npos ? text->size()
                                                    : offsets[1]);
}

bool PepperPluginInstanceImpl::IsPluginAcceptingCompositionEvents() const {
  return (filtered_input_event_mask_ & PP_INPUTEVENT_CLASS_IME) ||
         (input_event_mask_ & PP_INPUTEVENT_CLASS_IME);
}

gfx::Rect PepperPluginInstanceImpl::GetCaretBounds() const {
  if (!text_input_caret_set_) {
    // If it is never set by the plugin, use the bottom left corner.
    return gfx::Rect(view_data_.rect.point.x,
                     view_data_.rect.point.y + view_data_.rect.size.height,
                     0,
                     0);
  }

  // TODO(kinaba) Take CSS transformation into accont.
  // TODO(kinaba) Take bounding_box into account. On some platforms, an
  // "exclude rectangle" where candidate window must avoid the region can be
  // passed to IME. Currently, we pass only the caret rectangle because
  // it is the only information supported uniformly in Chromium.
  gfx::Rect caret(text_input_caret_);
  caret.Offset(view_data_.rect.point.x, view_data_.rect.point.y);
  return caret;
}

bool PepperPluginInstanceImpl::HandleInputEvent(
    const blink::WebInputEvent& event,
    WebCursorInfo* cursor_info) {
  TRACE_EVENT0("ppapi", "PepperPluginInstanceImpl::HandleInputEvent");

  if (!render_frame_)
    return false;
  if (WebInputEvent::isMouseEventType(event.type)) {
    render_frame_->PepperDidReceiveMouseEvent(this);
  }

  // Don't dispatch input events to crashed plugins.
  if (module()->is_crashed())
    return false;

  // Don't send reserved system key events to plugins.
  if (IsReservedSystemInputEvent(event))
    return false;

  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);

  bool rv = false;
  if (LoadInputEventInterface()) {
    PP_InputEvent_Class event_class = ClassifyInputEvent(event.type);
    if (!event_class)
      return false;

    if ((filtered_input_event_mask_ & event_class) ||
        (input_event_mask_ & event_class)) {
      // Actually send the event.
      std::vector<ppapi::InputEventData> events;
      CreateInputEventData(event, &events);

      // Allow the user gesture to be pending after the plugin handles the
      // event. This allows out-of-process plugins to respond to the user
      // gesture after processing has finished here.
      if (WebUserGestureIndicator::isProcessingUserGesture()) {
        pending_user_gesture_ =
            ppapi::EventTimeToPPTimeTicks(event.timeStampSeconds);
        pending_user_gesture_token_ =
            WebUserGestureIndicator::currentUserGestureToken();
        pending_user_gesture_token_.setOutOfProcess();
      }

      const ui::LatencyInfo* current_event_latency_info = NULL;
      if (render_frame_->GetRenderWidget()) {
        current_event_latency_info =
            render_frame_->GetRenderWidget()->current_event_latency_info();
      }

      // Each input event may generate more than one PP_InputEvent.
      for (size_t i = 0; i < events.size(); i++) {
        if (is_tracking_latency_) {
          InitLatencyInfo(&events[i].latency_info,
                          current_event_latency_info,
                          event.type,
                          last_input_number_++);
        }
        if (filtered_input_event_mask_ & event_class)
          events[i].is_filtered = true;
        else
          rv = true;  // Unfiltered events are assumed to be handled.
        scoped_refptr<PPB_InputEvent_Shared> event_resource(
            new PPB_InputEvent_Shared(
                ppapi::OBJECT_IS_IMPL, pp_instance(), events[i]));

        rv |= PP_ToBool(plugin_input_event_interface_->HandleInputEvent(
            pp_instance(), event_resource->pp_resource()));
      }
    }
  }

  if (cursor_)
    *cursor_info = *cursor_;
  return rv;
}

void PepperPluginInstanceImpl::HandleMessage(ScopedPPVar message) {
  TRACE_EVENT0("ppapi", "PepperPluginInstanceImpl::HandleMessage");
  ppapi::proxy::HostDispatcher* dispatcher =
      ppapi::proxy::HostDispatcher::GetForInstance(pp_instance());
  if (!dispatcher || (message.get().type == PP_VARTYPE_OBJECT)) {
    // The dispatcher should always be valid, and MessageChannel should never
    // send an 'object' var over PPP_Messaging.
    NOTREACHED();
    return;
  }
  dispatcher->Send(new PpapiMsg_PPPMessaging_HandleMessage(
      ppapi::API_ID_PPP_MESSAGING,
      pp_instance(),
      ppapi::proxy::SerializedVarSendInputShmem(dispatcher, message.get(),
                                                pp_instance())));
}

bool PepperPluginInstanceImpl::HandleBlockingMessage(ScopedPPVar message,
                                                     ScopedPPVar* result) {
  TRACE_EVENT0("ppapi", "PepperPluginInstanceImpl::HandleBlockingMessage");
  ppapi::proxy::HostDispatcher* dispatcher =
      ppapi::proxy::HostDispatcher::GetForInstance(pp_instance());
  if (!dispatcher || (message.get().type == PP_VARTYPE_OBJECT)) {
    // The dispatcher should always be valid, and MessageChannel should never
    // send an 'object' var over PPP_Messaging.
    NOTREACHED();
    return false;
  }
  ppapi::proxy::ReceiveSerializedVarReturnValue msg_reply;
  bool was_handled = false;
  dispatcher->Send(new PpapiMsg_PPPMessageHandler_HandleBlockingMessage(
      ppapi::API_ID_PPP_MESSAGING,
      pp_instance(),
      ppapi::proxy::SerializedVarSendInputShmem(dispatcher, message.get(),
                                                pp_instance()),
      &msg_reply,
      &was_handled));
  *result = ScopedPPVar(ScopedPPVar::PassRef(), msg_reply.Return(dispatcher));
  TRACE_EVENT0("ppapi",
               "PepperPluginInstanceImpl::HandleBlockingMessage return.");
  return was_handled;
}

PP_Var PepperPluginInstanceImpl::GetInstanceObject() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);

  // If the plugin supports the private instance interface, try to retrieve its
  // instance object.
  if (LoadPrivateInterface())
    return plugin_private_interface_->GetInstanceObject(pp_instance());
  return PP_MakeUndefined();
}

void PepperPluginInstanceImpl::ViewChanged(
    const gfx::Rect& position,
    const gfx::Rect& clip,
    const std::vector<gfx::Rect>& cut_outs_rects) {
  // WebKit can give weird (x,y) positions for empty clip rects (since the
  // position technically doesn't matter). But we want to make these
  // consistent since this is given to the plugin, so force everything to 0
  // in the "everything is clipped" case.
  gfx::Rect new_clip;
  if (!clip.IsEmpty())
    new_clip = clip;

  cut_outs_rects_ = cut_outs_rects;

  view_data_.rect = PP_FromGfxRect(position);
  view_data_.clip_rect = PP_FromGfxRect(clip);
  view_data_.device_scale = container_->deviceScaleFactor();
  view_data_.css_scale =
      container_->pageZoomFactor() * container_->pageScaleFactor();

  gfx::Size scroll_offset =
      container_->element().document().frame()->scrollOffset();
  view_data_.scroll_offset = PP_MakePoint(scroll_offset.width(),
                                          scroll_offset.height());

  if (desired_fullscreen_state_ || view_data_.is_fullscreen) {
    WebElement element = container_->element();
    WebDocument document = element.document();
    bool is_fullscreen_element = (element == document.fullScreenElement());
    if (!view_data_.is_fullscreen && desired_fullscreen_state_ &&
        render_frame()->GetRenderWidget()->is_fullscreen() &&
        is_fullscreen_element) {
      // Entered fullscreen. Only possible via SetFullscreen().
      view_data_.is_fullscreen = true;
    } else if (view_data_.is_fullscreen && !is_fullscreen_element) {
      // Exited fullscreen. Possible via SetFullscreen() or F11/link,
      // so desired_fullscreen_state might be out-of-date.
      desired_fullscreen_state_ = false;
      view_data_.is_fullscreen = false;

      // This operation will cause the plugin to re-layout which will send more
      // DidChangeView updates. Schedule an asynchronous update and suppress
      // notifications until that completes to avoid sending intermediate sizes
      // to the plugins.
      ScheduleAsyncDidChangeView();

      // Reset the size attributes that we hacked to fill in the screen and
      // retrigger ViewChanged. Make sure we don't forward duplicates of
      // this view to the plugin.
      ResetSizeAttributesAfterFullscreen();
      return;
    }
  }

  UpdateFlashFullscreenState(fullscreen_container_ != NULL);

  SendDidChangeView();
}

void PepperPluginInstanceImpl::SetWebKitFocus(bool has_focus) {
  if (has_webkit_focus_ == has_focus)
    return;

  bool old_plugin_focus = PluginHasFocus();
  has_webkit_focus_ = has_focus;
  if (PluginHasFocus() != old_plugin_focus)
    SendFocusChangeNotification();
}

void PepperPluginInstanceImpl::SetContentAreaFocus(bool has_focus) {
  if (has_content_area_focus_ == has_focus)
    return;

  bool old_plugin_focus = PluginHasFocus();
  has_content_area_focus_ = has_focus;
  if (PluginHasFocus() != old_plugin_focus)
    SendFocusChangeNotification();
}

void PepperPluginInstanceImpl::PageVisibilityChanged(bool is_visible) {
  if (is_visible == view_data_.is_page_visible)
    return;  // Nothing to do.
  view_data_.is_page_visible = is_visible;

  // If the initial DidChangeView notification hasn't been sent to the plugin,
  // let it pass the visibility state for us, instead of sending a notification
  // immediately. It is possible that PepperPluginInstanceImpl::ViewChanged()
  // hasn't been called for the first time. In that case, most of the fields in
  // |view_data_| haven't been properly initialized.
  if (sent_initial_did_change_view_)
    SendDidChangeView();
}

void PepperPluginInstanceImpl::ViewInitiatedPaint() {
  if (bound_graphics_2d_platform_)
    bound_graphics_2d_platform_->ViewInitiatedPaint();
  else if (bound_graphics_3d_.get())
    bound_graphics_3d_->ViewInitiatedPaint();
  else if (bound_compositor_)
    bound_compositor_->ViewInitiatedPaint();
}

void PepperPluginInstanceImpl::ViewFlushedPaint() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (bound_graphics_2d_platform_)
    bound_graphics_2d_platform_->ViewFlushedPaint();
  else if (bound_graphics_3d_.get())
    bound_graphics_3d_->ViewFlushedPaint();
  else if (bound_compositor_)
    bound_compositor_->ViewFlushedPaint();
}

void PepperPluginInstanceImpl::SetSelectedText(
    const base::string16& selected_text) {
  selected_text_ = selected_text;
}

void PepperPluginInstanceImpl::SetLinkUnderCursor(const std::string& url) {
  link_under_cursor_ = base::UTF8ToUTF16(url);
}

void PepperPluginInstanceImpl::SetTextInputType(ui::TextInputType type) {
  text_input_type_ = type;
  render_frame_->PepperTextInputTypeChanged(this);
}

void PepperPluginInstanceImpl::PostMessageToJavaScript(PP_Var message) {
  message_channel_->PostMessageToJavaScript(message);
}

int32_t PepperPluginInstanceImpl::RegisterMessageHandler(
    PP_Instance instance,
    void* user_data,
    const PPP_MessageHandler_0_1* handler,
    PP_Resource message_loop) {
  // Not supported in-process.
  NOTIMPLEMENTED();
  return PP_ERROR_FAILED;
}

void PepperPluginInstanceImpl::UnregisterMessageHandler(PP_Instance instance) {
  // Not supported in-process.
  NOTIMPLEMENTED();
}

base::string16 PepperPluginInstanceImpl::GetSelectedText(bool html) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!LoadSelectionInterface())
    return selected_text_;

  PP_Var rv = plugin_selection_interface_->GetSelectedText(pp_instance(),
                                                           PP_FromBool(html));
  StringVar* string = StringVar::FromPPVar(rv);
  base::string16 selection;
  if (string)
    selection = base::UTF8ToUTF16(string->value());
  // Release the ref the plugin transfered to us.
  HostGlobals::Get()->GetVarTracker()->ReleaseVar(rv);
  return selection;
}

base::string16 PepperPluginInstanceImpl::GetLinkAtPosition(
    const gfx::Point& point) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!LoadPdfInterface()) {
    // TODO(koz): Change the containing function to GetLinkUnderCursor(). We can
    // return |link_under_cursor_| here because this is only ever called with
    // the current mouse coordinates.
    return link_under_cursor_;
  }

  PP_Point p;
  p.x = point.x();
  p.y = point.y();
  PP_Var rv = plugin_pdf_interface_->GetLinkAtPosition(pp_instance(), p);
  // If the plugin returns undefined for this function it has switched to
  // providing us with the link under the cursor eagerly.
  if (rv.type == PP_VARTYPE_UNDEFINED)
    return link_under_cursor_;
  StringVar* string = StringVar::FromPPVar(rv);
  base::string16 link;
  if (string)
    link = base::UTF8ToUTF16(string->value());
  // Release the ref the plugin transfered to us.
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(rv);
  return link;
}

void PepperPluginInstanceImpl::RequestSurroundingText(
    size_t desired_number_of_characters) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!LoadTextInputInterface())
    return;
  plugin_textinput_interface_->RequestSurroundingText(
      pp_instance(), desired_number_of_characters);
}

void PepperPluginInstanceImpl::Zoom(double factor, bool text_only) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!LoadZoomInterface())
    return;
  plugin_zoom_interface_->Zoom(pp_instance(), factor, PP_FromBool(text_only));
}

bool PepperPluginInstanceImpl::StartFind(const base::string16& search_text,
                                         bool case_sensitive,
                                         int identifier) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!LoadFindInterface())
    return false;
  find_identifier_ = identifier;
  return PP_ToBool(
      plugin_find_interface_->StartFind(pp_instance(),
                                        base::UTF16ToUTF8(search_text).c_str(),
                                        PP_FromBool(case_sensitive)));
}

void PepperPluginInstanceImpl::SelectFindResult(bool forward) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (LoadFindInterface())
    plugin_find_interface_->SelectFindResult(pp_instance(),
                                             PP_FromBool(forward));
}

void PepperPluginInstanceImpl::StopFind() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!LoadFindInterface())
    return;
  find_identifier_ = -1;
  plugin_find_interface_->StopFind(pp_instance());
}

bool PepperPluginInstanceImpl::LoadFindInterface() {
  if (!module_->permissions().HasPermission(ppapi::PERMISSION_PRIVATE))
    return false;
  if (!plugin_find_interface_) {
    plugin_find_interface_ = static_cast<const PPP_Find_Private*>(
        module_->GetPluginInterface(PPP_FIND_PRIVATE_INTERFACE));
  }

  return !!plugin_find_interface_;
}

bool PepperPluginInstanceImpl::LoadInputEventInterface() {
  if (!checked_for_plugin_input_event_interface_) {
    checked_for_plugin_input_event_interface_ = true;
    plugin_input_event_interface_ = static_cast<const PPP_InputEvent*>(
        module_->GetPluginInterface(PPP_INPUT_EVENT_INTERFACE));
  }
  return !!plugin_input_event_interface_;
}

bool PepperPluginInstanceImpl::LoadMouseLockInterface() {
  if (!plugin_mouse_lock_interface_) {
    plugin_mouse_lock_interface_ = static_cast<const PPP_MouseLock*>(
        module_->GetPluginInterface(PPP_MOUSELOCK_INTERFACE));
  }

  return !!plugin_mouse_lock_interface_;
}

bool PepperPluginInstanceImpl::LoadPdfInterface() {
  if (!checked_for_plugin_pdf_interface_) {
    checked_for_plugin_pdf_interface_ = true;
    plugin_pdf_interface_ = static_cast<const PPP_Pdf_1*>(
        module_->GetPluginInterface(PPP_PDF_INTERFACE_1));
  }

  return !!plugin_pdf_interface_;
}

bool PepperPluginInstanceImpl::LoadPrintInterface() {
  // Only check for the interface if the plugin has dev permission.
  if (!module_->permissions().HasPermission(ppapi::PERMISSION_DEV))
    return false;
  if (!plugin_print_interface_) {
    plugin_print_interface_ = static_cast<const PPP_Printing_Dev*>(
        module_->GetPluginInterface(PPP_PRINTING_DEV_INTERFACE));
  }
  return !!plugin_print_interface_;
}

bool PepperPluginInstanceImpl::LoadPrivateInterface() {
  // If this is a NaCl app, we want to talk to the trusted NaCl plugin to
  // call GetInstanceObject. This is necessary to ensure that the properties
  // the trusted plugin exposes (readyState and lastError) work properly. Note
  // that untrusted NaCl apps are not allowed to provide PPP_InstancePrivate,
  // so it's correct to never look up PPP_InstancePrivate for them.
  //
  // If this is *not* a NaCl plugin, original_module_ will never be set; we talk
  // to the "real" module.
  scoped_refptr<PluginModule> module =
      original_module_ ? original_module_ : module_;
  // Only check for the interface if the plugin has private permission.
  if (!module->permissions().HasPermission(ppapi::PERMISSION_PRIVATE))
    return false;
  if (!plugin_private_interface_) {
    plugin_private_interface_ = static_cast<const PPP_Instance_Private*>(
        module->GetPluginInterface(PPP_INSTANCE_PRIVATE_INTERFACE));
  }

  return !!plugin_private_interface_;
}

bool PepperPluginInstanceImpl::LoadSelectionInterface() {
  if (!plugin_selection_interface_) {
    plugin_selection_interface_ = static_cast<const PPP_Selection_Dev*>(
        module_->GetPluginInterface(PPP_SELECTION_DEV_INTERFACE));
  }
  return !!plugin_selection_interface_;
}

bool PepperPluginInstanceImpl::LoadTextInputInterface() {
  if (!plugin_textinput_interface_) {
    plugin_textinput_interface_ = static_cast<const PPP_TextInput_Dev*>(
        module_->GetPluginInterface(PPP_TEXTINPUT_DEV_INTERFACE));
  }

  return !!plugin_textinput_interface_;
}

bool PepperPluginInstanceImpl::LoadZoomInterface() {
  if (!plugin_zoom_interface_) {
    plugin_zoom_interface_ = static_cast<const PPP_Zoom_Dev*>(
        module_->GetPluginInterface(PPP_ZOOM_DEV_INTERFACE));
  }

  return !!plugin_zoom_interface_;
}

void PepperPluginInstanceImpl::UpdateLayerTransform() {
  if (!bound_graphics_2d_platform_ || !texture_layer_) {
    // Currently the transform is only applied for Graphics2D.
    return;
  }
  // Set the UV coordinates of the texture based on the size of the Graphics2D
  // context. By default a texture gets scaled to the size of the layer. But
  // if the size of the Graphics2D context doesn't match the size of the plugin
  // then it will be incorrectly stretched. This also affects how the plugin
  // is painted when it is being resized. If the Graphics2D contents are
  // stretched when a plugin is resized while waiting for a new frame from the
  // plugin to be rendered, then flickering behavior occurs as in
  // crbug.com/353453.
  gfx::SizeF graphics_2d_size_in_dip =
      gfx::ScaleSize(bound_graphics_2d_platform_->Size(),
                     bound_graphics_2d_platform_->GetScale());
  gfx::Size plugin_size_in_dip(view_data_.rect.size.width,
                               view_data_.rect.size.height);

  texture_layer_->SetUV(
      gfx::PointF(0.0f, 0.0f),
      gfx::PointF(
          plugin_size_in_dip.width() / graphics_2d_size_in_dip.width(),
          plugin_size_in_dip.height() / graphics_2d_size_in_dip.height()));
}

bool PepperPluginInstanceImpl::PluginHasFocus() const {
  return flash_fullscreen_ || (has_webkit_focus_ && has_content_area_focus_);
}

void PepperPluginInstanceImpl::SendFocusChangeNotification() {
  // Keep a reference on the stack. RenderViewImpl::PepperFocusChanged may
  // remove the <embed> from the DOM, which will make the PepperWebPluginImpl
  // drop its reference, usually the last one. This is similar to possible
  // plugin behavior described at the NOTE above Delete().
  scoped_refptr<PepperPluginInstanceImpl> ref(this);

  if (!render_frame_)
    return;

  bool has_focus = PluginHasFocus();
  render_frame_->render_view()->PepperFocusChanged(this, has_focus);

  // instance_interface_ may have been cleared in Delete() if the
  // PepperWebPluginImpl is destroyed.
  if (instance_interface_)
    instance_interface_->DidChangeFocus(pp_instance(), PP_FromBool(has_focus));
}

void PepperPluginInstanceImpl::UpdateTouchEventRequest() {
  bool raw_touch = (filtered_input_event_mask_ & PP_INPUTEVENT_CLASS_TOUCH) ||
                   (input_event_mask_ & PP_INPUTEVENT_CLASS_TOUCH);
  container_->requestTouchEventType(
      raw_touch
          ? blink::WebPluginContainer::TouchEventRequestTypeRaw
          : blink::WebPluginContainer::TouchEventRequestTypeSynthesizedMouse);
}

bool PepperPluginInstanceImpl::IsAcceptingWheelEvents() const {
  return (filtered_input_event_mask_ & PP_INPUTEVENT_CLASS_WHEEL) ||
         (input_event_mask_ & PP_INPUTEVENT_CLASS_WHEEL);
}

void PepperPluginInstanceImpl::ScheduleAsyncDidChangeView() {
  if (view_change_weak_ptr_factory_.HasWeakPtrs())
    return;  // Already scheduled.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PepperPluginInstanceImpl::SendAsyncDidChangeView,
                 view_change_weak_ptr_factory_.GetWeakPtr()));
}

void PepperPluginInstanceImpl::SendAsyncDidChangeView() {
  // The bound callback that owns the weak pointer is still valid until after
  // this function returns. SendDidChangeView checks HasWeakPtrs, so we need to
  // invalidate them here.
  // NOTE: If we ever want to have more than one pending callback, it should
  // use a different factory, or we should have a different strategy here.
  view_change_weak_ptr_factory_.InvalidateWeakPtrs();
  SendDidChangeView();
}

void PepperPluginInstanceImpl::SendDidChangeView() {
  // Don't send DidChangeView to crashed plugins.
  if (module()->is_crashed())
    return;

  if (view_change_weak_ptr_factory_.HasWeakPtrs() ||
      (sent_initial_did_change_view_ &&
       last_sent_view_data_.Equals(view_data_)))
    return;  // Nothing to update.

  sent_initial_did_change_view_ = true;
  last_sent_view_data_ = view_data_;
  ScopedPPResource resource(
      ScopedPPResource::PassRef(),
      (new PPB_View_Shared(ppapi::OBJECT_IS_IMPL, pp_instance(), view_data_))
          ->GetReference());

  UpdateLayerTransform();

  // It's possible that Delete() has been called but the renderer hasn't
  // released its reference to this object yet.
  if (instance_interface_) {
    instance_interface_->DidChangeView(
        pp_instance(), resource, &view_data_.rect, &view_data_.clip_rect);
  }
}

void PepperPluginInstanceImpl::ReportGeometry() {
  // If this call was delayed, we may have transitioned back to fullscreen in
  // the mean time, so only report the geometry if we are actually in normal
  // mode.
  if (container_ && !fullscreen_container_ && !flash_fullscreen_)
    container_->reportGeometry();
}

bool PepperPluginInstanceImpl::GetPreferredPrintOutputFormat(
    PP_PrintOutputFormat_Dev* format) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!LoadPrintInterface())
    return false;
  uint32_t supported_formats =
      plugin_print_interface_->QuerySupportedFormats(pp_instance());
  if (supported_formats & PP_PRINTOUTPUTFORMAT_PDF) {
    *format = PP_PRINTOUTPUTFORMAT_PDF;
    return true;
  }
  return false;
}

bool PepperPluginInstanceImpl::SupportsPrintInterface() {
  PP_PrintOutputFormat_Dev format;
  return GetPreferredPrintOutputFormat(&format);
}

bool PepperPluginInstanceImpl::IsPrintScalingDisabled() {
  DCHECK(plugin_print_interface_);
  if (!plugin_print_interface_)
    return false;
  return plugin_print_interface_->IsScalingDisabled(pp_instance()) == PP_TRUE;
}

int PepperPluginInstanceImpl::PrintBegin(const WebPrintParams& print_params) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  PP_PrintOutputFormat_Dev format;
  if (!GetPreferredPrintOutputFormat(&format)) {
    // PrintBegin should not have been called since SupportsPrintInterface
    // would have returned false;
    NOTREACHED();
    return 0;
  }
  int num_pages = 0;
  PP_PrintSettings_Dev print_settings;
  print_settings.printable_area = PP_FromGfxRect(print_params.printableArea);
  print_settings.content_area = PP_FromGfxRect(print_params.printContentArea);
  print_settings.paper_size = PP_FromGfxSize(print_params.paperSize);
  print_settings.dpi = print_params.printerDPI;
  print_settings.orientation = PP_PRINTORIENTATION_NORMAL;
  print_settings.grayscale = PP_FALSE;
  print_settings.print_scaling_option =
      static_cast<PP_PrintScalingOption_Dev>(print_params.printScalingOption);
  print_settings.format = format;
  num_pages = plugin_print_interface_->Begin(pp_instance(), &print_settings);
  if (!num_pages)
    return 0;
  current_print_settings_ = print_settings;
  canvas_.clear();
  ranges_.clear();
  return num_pages;
}

bool PepperPluginInstanceImpl::PrintPage(int page_number,
                                         blink::WebCanvas* canvas) {
#if defined(ENABLE_FULL_PRINTING)
  DCHECK(plugin_print_interface_);
  PP_PrintPageNumberRange_Dev page_range;
  page_range.first_page_number = page_range.last_page_number = page_number;
  // The canvas only has a metafile on it for print preview.
  bool save_for_later =
      (printing::MetafileSkiaWrapper::GetMetafileFromCanvas(*canvas) != NULL);
#if defined(OS_MACOSX) || \
    (defined(OS_WIN) && !defined(WIN_PDF_METAFILE_FOR_PRINTING))
  save_for_later = save_for_later && skia::IsPreviewMetafile(*canvas);
#endif
  if (save_for_later) {
    ranges_.push_back(page_range);
    canvas_ = skia::SharePtr(canvas);
    return true;
  } else {
    return PrintPageHelper(&page_range, 1, canvas);
  }
#else  // defined(ENABLED_PRINTING)
  return false;
#endif
}

bool PepperPluginInstanceImpl::PrintPageHelper(
    PP_PrintPageNumberRange_Dev* page_ranges,
    int num_ranges,
    blink::WebCanvas* canvas) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  DCHECK(plugin_print_interface_);
  if (!plugin_print_interface_)
    return false;
  PP_Resource print_output = plugin_print_interface_->PrintPages(
      pp_instance(), page_ranges, num_ranges);
  if (!print_output)
    return false;

  bool ret = false;

  if (current_print_settings_.format == PP_PRINTOUTPUTFORMAT_PDF)
    ret = PrintPDFOutput(print_output, canvas);

  // Now we need to release the print output resource.
  PluginModule::GetCore()->ReleaseResource(print_output);

  return ret;
}

void PepperPluginInstanceImpl::PrintEnd() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  if (!ranges_.empty())
    PrintPageHelper(&(ranges_.front()), ranges_.size(), canvas_.get());
  canvas_.clear();
  ranges_.clear();

  DCHECK(plugin_print_interface_);
  if (plugin_print_interface_)
    plugin_print_interface_->End(pp_instance());

  memset(&current_print_settings_, 0, sizeof(current_print_settings_));
#if defined(OS_MACOSX)
  last_printed_page_ = NULL;
#endif  // defined(OS_MACOSX)
}

bool PepperPluginInstanceImpl::CanRotateView() {
  if (!LoadPdfInterface())
    return false;

  return true;
}

void PepperPluginInstanceImpl::RotateView(WebPlugin::RotationType type) {
  if (!LoadPdfInterface())
    return;
  PP_PrivatePageTransformType transform_type =
      type == WebPlugin::RotationType90Clockwise
          ? PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CW
          : PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CCW;
  plugin_pdf_interface_->Transform(pp_instance(), transform_type);
  // NOTE: plugin instance may have been deleted.
}

bool PepperPluginInstanceImpl::FlashIsFullscreenOrPending() {
  return fullscreen_container_ != NULL;
}

bool PepperPluginInstanceImpl::IsFullscreenOrPending() {
  return desired_fullscreen_state_;
}

bool PepperPluginInstanceImpl::SetFullscreen(bool fullscreen) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);

  // Check whether we are trying to switch to the state we're already going
  // to (i.e. if we're already switching to fullscreen but the fullscreen
  // container isn't ready yet, don't do anything more).
  if (fullscreen == IsFullscreenOrPending())
    return false;

  if (!render_frame_)
    return false;
  if (fullscreen && !render_frame_->render_view()
                         ->renderer_preferences()
                         .plugin_fullscreen_allowed)
    return false;

  // Check whether we are trying to switch while the state is in transition.
  // The 2nd request gets dropped while messing up the internal state, so
  // disallow this.
  if (view_data_.is_fullscreen != desired_fullscreen_state_)
    return false;

  if (fullscreen && !IsProcessingUserGesture())
    return false;

  VLOG(1) << "Setting fullscreen to " << (fullscreen ? "on" : "off");
  desired_fullscreen_state_ = fullscreen;

  if (fullscreen) {
    // Create the user gesture in case we're processing one that's pending.
    WebScopedUserGesture user_gesture(CurrentUserGestureToken());
    // WebKit does not resize the plugin to fill the screen in fullscreen mode,
    // so we will tweak plugin's attributes to support the expected behavior.
    KeepSizeAttributesBeforeFullscreen();
    SetSizeAttributesForFullscreen();
    container_->element().requestFullScreen();
  } else {
    container_->element().document().cancelFullScreen();
  }
  return true;
}

void PepperPluginInstanceImpl::UpdateFlashFullscreenState(
    bool flash_fullscreen) {
  bool is_mouselock_pending = TrackedCallback::IsPending(lock_mouse_callback_);

  if (flash_fullscreen == flash_fullscreen_) {
    // Manually clear callback when fullscreen fails with mouselock pending.
    if (!flash_fullscreen && is_mouselock_pending)
      lock_mouse_callback_->Run(PP_ERROR_FAILED);
    return;
  }

  UpdateLayer(false);

  bool old_plugin_focus = PluginHasFocus();
  flash_fullscreen_ = flash_fullscreen;
  if (is_mouselock_pending && !IsMouseLocked()) {
    if (!IsProcessingUserGesture() &&
        !module_->permissions().HasPermission(
            ppapi::PERMISSION_BYPASS_USER_GESTURE)) {
      lock_mouse_callback_->Run(PP_ERROR_NO_USER_GESTURE);
    } else {
      // Open a user gesture here so the Webkit user gesture checks will succeed
      // for out-of-process plugins.
      WebScopedUserGesture user_gesture(CurrentUserGestureToken());
      if (!LockMouse())
        lock_mouse_callback_->Run(PP_ERROR_FAILED);
    }
  }

  if (PluginHasFocus() != old_plugin_focus)
    SendFocusChangeNotification();
}

bool PepperPluginInstanceImpl::IsViewAccelerated() {
  if (!container_)
    return false;

  WebDocument document = container_->element().document();
  WebLocalFrame* frame = document.frame();
  if (!frame)
    return false;
  WebView* view = frame->view();
  if (!view)
    return false;

  return view->isAcceleratedCompositingActive();
}

bool PepperPluginInstanceImpl::PrintPDFOutput(PP_Resource print_output,
                                              blink::WebCanvas* canvas) {
#if defined(ENABLE_FULL_PRINTING)
  ppapi::thunk::EnterResourceNoLock<PPB_Buffer_API> enter(print_output, true);
  if (enter.failed())
    return false;

  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || !mapper.size()) {
    NOTREACHED();
    return false;
  }
#if defined(OS_WIN)
  // For Windows, we need the PDF DLL to render the output PDF to a DC.
  HMODULE pdf_module = GetModuleHandle(L"pdf.dll");
  if (!pdf_module)
    return false;
  RenderPDFPageToDCProc render_proc = reinterpret_cast<RenderPDFPageToDCProc>(
      GetProcAddress(pdf_module, "RenderPDFPageToDC"));
  if (!render_proc)
    return false;
#endif  // defined(OS_WIN)

  bool ret = false;
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // On Linux we just set the final bits in the native metafile
  // (NativeMetafile and PreviewMetafile must have compatible formats,
  // i.e. both PDF for this to work).
  printing::Metafile* metafile =
      printing::MetafileSkiaWrapper::GetMetafileFromCanvas(*canvas);
  DCHECK(metafile != NULL);
  if (metafile)
    ret = metafile->InitFromData(mapper.data(), mapper.size());
#elif defined(OS_WIN)
  printing::Metafile* metafile =
      printing::MetafileSkiaWrapper::GetMetafileFromCanvas(*canvas);
  if (metafile) {
    // We only have a metafile when doing print preview, so we just want to
    // pass the PDF off to preview.
    ret = metafile->InitFromData(mapper.data(), mapper.size());
  } else {
    // On Windows, we now need to render the PDF to the DC that backs the
    // supplied canvas.
    HDC dc = skia::BeginPlatformPaint(canvas);
    DrawEmptyRectangle(dc);
    gfx::Size size_in_pixels;
    size_in_pixels.set_width(
        printing::ConvertUnit(current_print_settings_.printable_area.size.width,
                              static_cast<int>(printing::kPointsPerInch),
                              current_print_settings_.dpi));
    size_in_pixels.set_height(printing::ConvertUnit(
        current_print_settings_.printable_area.size.height,
        static_cast<int>(printing::kPointsPerInch),
        current_print_settings_.dpi));
    // We need to scale down DC to fit an entire page into DC available area.
    // First, we'll try to use default scaling based on the 72dpi that is
    // used in webkit for printing.
    // If default scaling is not enough to fit the entire PDF without
    // Current metafile is based on screen DC and have current screen size.
    // Writing outside of those boundaries will result in the cut-off output.
    // On metafiles (this is the case here), scaling down will still record
    // original coordinates and we'll be able to print in full resolution.
    // Before playback we'll need to counter the scaling up that will happen
    // in the browser (printed_document_win.cc).
    double dynamic_scale = gfx::CalculatePageScale(
        dc, size_in_pixels.width(), size_in_pixels.height());
    double page_scale = static_cast<double>(printing::kPointsPerInch) /
                        static_cast<double>(current_print_settings_.dpi);

    if (dynamic_scale < page_scale) {
      page_scale = dynamic_scale;
      printing::MetafileSkiaWrapper::SetCustomScaleOnCanvas(*canvas,
                                                            page_scale);
    }

    gfx::ScaleDC(dc, page_scale);

    ret = render_proc(static_cast<unsigned char*>(mapper.data()),
                      mapper.size(),
                      0,
                      dc,
                      current_print_settings_.dpi,
                      current_print_settings_.dpi,
                      0,
                      0,
                      size_in_pixels.width(),
                      size_in_pixels.height(),
                      true,
                      false,
                      true,
                      true,
                      true);
    skia::EndPlatformPaint(canvas);
  }
#endif  // defined(OS_WIN)

  return ret;
#else  // defined(ENABLE_FULL_PRINTING)
  return false;
#endif
}

void PepperPluginInstanceImpl::UpdateLayer(bool device_changed) {
  if (!container_)
    return;

  gpu::Mailbox mailbox;
  uint32 sync_point = 0;
  if (bound_graphics_3d_.get()) {
    bound_graphics_3d_->GetBackingMailbox(&mailbox, &sync_point);
    DCHECK_EQ(mailbox.IsZero(), sync_point == 0);
  }
  bool want_3d_layer = !mailbox.IsZero();
  bool want_2d_layer = !!bound_graphics_2d_platform_;
  bool want_texture_layer = want_3d_layer || want_2d_layer;
  bool want_compositor_layer = !!bound_compositor_;

  if (!device_changed &&
      (want_texture_layer == !!texture_layer_.get()) &&
      (want_3d_layer == layer_is_hardware_) &&
      (want_compositor_layer == !!compositor_layer_) &&
      layer_bound_to_fullscreen_ == !!fullscreen_container_) {
    UpdateLayerTransform();
    return;
  }

  if (texture_layer_ || compositor_layer_) {
    if (!layer_bound_to_fullscreen_)
      container_->setWebLayer(NULL);
    else if (fullscreen_container_)
      fullscreen_container_->SetLayer(NULL);
    web_layer_.reset();
    texture_layer_ = NULL;
    compositor_layer_ = NULL;
  }

  if (want_texture_layer) {
    bool opaque = false;
    if (want_3d_layer) {
      DCHECK(bound_graphics_3d_.get());
      texture_layer_ = cc::TextureLayer::CreateForMailbox(NULL);
      opaque = bound_graphics_3d_->IsOpaque();
      texture_layer_->SetTextureMailboxWithoutReleaseCallback(
          cc::TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point));
    } else {
      DCHECK(bound_graphics_2d_platform_);
      texture_layer_ = cc::TextureLayer::CreateForMailbox(this);
      bound_graphics_2d_platform_->AttachedToNewLayer();
      opaque = bound_graphics_2d_platform_->IsAlwaysOpaque();
      texture_layer_->SetFlipped(false);
    }

    // Ignore transparency in fullscreen, since that's what Flash always
    // wants to do, and that lets it not recreate a context if
    // wmode=transparent was specified.
    opaque = opaque || fullscreen_container_;
    texture_layer_->SetContentsOpaque(opaque);
    web_layer_.reset(new WebLayerImpl(texture_layer_));
  } else if (want_compositor_layer) {
    compositor_layer_ = bound_compositor_->layer();
    web_layer_.reset(new WebLayerImpl(compositor_layer_));
  }

  if (web_layer_) {
    if (fullscreen_container_) {
      fullscreen_container_->SetLayer(web_layer_.get());
    } else {
      container_->setWebLayer(web_layer_.get());
    }
  }

  layer_bound_to_fullscreen_ = !!fullscreen_container_;
  layer_is_hardware_ = want_3d_layer;
  UpdateLayerTransform();
}

bool PepperPluginInstanceImpl::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox,
    scoped_ptr<cc::SingleReleaseCallback>* release_callback,
    bool use_shared_memory) {
  if (!bound_graphics_2d_platform_)
    return false;
  return bound_graphics_2d_platform_->PrepareTextureMailbox(mailbox,
                                                            release_callback);
}

void PepperPluginInstanceImpl::OnDestruct() { render_frame_ = NULL; }

void PepperPluginInstanceImpl::AddLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  if (render_frame_ && render_frame_->GetRenderWidget()) {
    RenderWidgetCompositor* compositor =
        render_frame_->GetRenderWidget()->compositor();
    if (compositor) {
      for (size_t i = 0; i < latency_info.size(); i++) {
        scoped_ptr<cc::SwapPromise> swap_promise(
            new cc::LatencyInfoSwapPromise(latency_info[i]));
        compositor->QueueSwapPromise(swap_promise.Pass());
      }
    }
  }
}

void PepperPluginInstanceImpl::AddPluginObject(PluginObject* plugin_object) {
  DCHECK(live_plugin_objects_.find(plugin_object) ==
         live_plugin_objects_.end());
  live_plugin_objects_.insert(plugin_object);
}

void PepperPluginInstanceImpl::RemovePluginObject(PluginObject* plugin_object) {
  // Don't actually verify that the object is in the set since during module
  // deletion we'll be in the process of freeing them.
  live_plugin_objects_.erase(plugin_object);
}

bool PepperPluginInstanceImpl::IsProcessingUserGesture() {
  PP_TimeTicks now = ppapi::TimeTicksToPPTimeTicks(base::TimeTicks::Now());
  // Give a lot of slack so tests won't be flaky.
  const PP_TimeTicks kUserGestureDurationInSeconds = 10.0;
  return pending_user_gesture_token_.hasGestures() &&
         (now - pending_user_gesture_ < kUserGestureDurationInSeconds);
}

WebUserGestureToken PepperPluginInstanceImpl::CurrentUserGestureToken() {
  if (!IsProcessingUserGesture())
    pending_user_gesture_token_ = WebUserGestureToken();
  return pending_user_gesture_token_;
}

void PepperPluginInstanceImpl::OnLockMouseACK(bool succeeded) {
  if (TrackedCallback::IsPending(lock_mouse_callback_))
    lock_mouse_callback_->Run(succeeded ? PP_OK : PP_ERROR_FAILED);
}

void PepperPluginInstanceImpl::OnMouseLockLost() {
  if (LoadMouseLockInterface())
    plugin_mouse_lock_interface_->MouseLockLost(pp_instance());
}

void PepperPluginInstanceImpl::HandleMouseLockedInputEvent(
    const blink::WebMouseEvent& event) {
  // |cursor_info| is ignored since it is hidden when the mouse is locked.
  blink::WebCursorInfo cursor_info;
  HandleInputEvent(event, &cursor_info);
}

void PepperPluginInstanceImpl::SimulateInputEvent(
    const InputEventData& input_event) {
  WebView* web_view = container()->element().document().frame()->view();
  if (!web_view) {
    NOTREACHED();
    return;
  }

  bool handled = SimulateIMEEvent(input_event);
  if (handled)
    return;

  std::vector<linked_ptr<WebInputEvent> > events =
      CreateSimulatedWebInputEvents(
          input_event,
          view_data_.rect.point.x + view_data_.rect.size.width / 2,
          view_data_.rect.point.y + view_data_.rect.size.height / 2);
  for (std::vector<linked_ptr<WebInputEvent> >::iterator it = events.begin();
       it != events.end();
       ++it) {
    web_view->handleInputEvent(*it->get());
  }
}

bool PepperPluginInstanceImpl::SimulateIMEEvent(
    const InputEventData& input_event) {
  switch (input_event.event_type) {
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
      SimulateImeSetCompositionEvent(input_event);
      break;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
      DCHECK(input_event.character_text.empty());
      SimulateImeSetCompositionEvent(input_event);
      break;
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      if (!render_frame_)
        return false;
      render_frame_->SimulateImeConfirmComposition(
          base::UTF8ToUTF16(input_event.character_text), gfx::Range());
      break;
    default:
      return false;
  }
  return true;
}

void PepperPluginInstanceImpl::SimulateImeSetCompositionEvent(
    const InputEventData& input_event) {
  if (!render_frame_)
    return;

  std::vector<size_t> offsets;
  offsets.push_back(input_event.composition_selection_start);
  offsets.push_back(input_event.composition_selection_end);
  offsets.insert(offsets.end(),
                 input_event.composition_segment_offsets.begin(),
                 input_event.composition_segment_offsets.end());

  base::string16 utf16_text =
      base::UTF8ToUTF16AndAdjustOffsets(input_event.character_text, &offsets);

  std::vector<blink::WebCompositionUnderline> underlines;
  for (size_t i = 2; i + 1 < offsets.size(); ++i) {
    blink::WebCompositionUnderline underline;
    underline.startOffset = offsets[i];
    underline.endOffset = offsets[i + 1];
    if (input_event.composition_target_segment == static_cast<int32_t>(i - 2))
      underline.thick = true;
    underlines.push_back(underline);
  }

  render_frame_->SimulateImeSetComposition(
      utf16_text, underlines, offsets[0], offsets[1]);
}

ContentDecryptorDelegate*
PepperPluginInstanceImpl::GetContentDecryptorDelegate() {
  if (content_decryptor_delegate_)
    return content_decryptor_delegate_.get();

  const PPP_ContentDecryptor_Private* plugin_decryption_interface =
      static_cast<const PPP_ContentDecryptor_Private*>(
          module_->GetPluginInterface(PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE));
  if (!plugin_decryption_interface)
    return NULL;

  content_decryptor_delegate_.reset(
      new ContentDecryptorDelegate(pp_instance_, plugin_decryption_interface));
  return content_decryptor_delegate_.get();
}

PP_Bool PepperPluginInstanceImpl::BindGraphics(PP_Instance instance,
                                               PP_Resource device) {
  TRACE_EVENT0("ppapi", "PepperPluginInstanceImpl::BindGraphics");
  // The Graphics3D instance can't be destroyed until we call
  // UpdateLayer().
  scoped_refptr<ppapi::Resource> old_graphics = bound_graphics_3d_.get();
  if (bound_graphics_3d_.get()) {
    bound_graphics_3d_->BindToInstance(false);
    bound_graphics_3d_ = NULL;
  }
  if (bound_graphics_2d_platform_) {
    bound_graphics_2d_platform_->BindToInstance(NULL);
    bound_graphics_2d_platform_ = NULL;
  }
  if (bound_compositor_) {
    bound_compositor_->BindToInstance(NULL);
    bound_compositor_ = NULL;
  }

  // Special-case clearing the current device.
  if (!device) {
    UpdateLayer(true);
    InvalidateRect(gfx::Rect());
    return PP_TRUE;
  }

  // Refuse to bind if in transition to fullscreen with PPB_FlashFullscreen or
  // to/from fullscreen with PPB_Fullscreen.
  if ((fullscreen_container_ && !flash_fullscreen_) ||
      desired_fullscreen_state_ != view_data_.is_fullscreen)
    return PP_FALSE;

  const ppapi::host::PpapiHost* ppapi_host =
      RendererPpapiHost::GetForPPInstance(instance)->GetPpapiHost();
  ppapi::host::ResourceHost* host = ppapi_host->GetResourceHost(device);
  PepperGraphics2DHost* graphics_2d = NULL;
  PepperCompositorHost* compositor = NULL;
  if (host) {
    if (host->IsGraphics2DHost()) {
      graphics_2d = static_cast<PepperGraphics2DHost*>(host);
    } else if (host->IsCompositorHost()) {
      compositor = static_cast<PepperCompositorHost*>(host);
    } else {
      DLOG(ERROR) <<
          "Resource is not PepperCompositorHost or PepperGraphics2DHost.";
    }
  }

  EnterResourceNoLock<PPB_Graphics3D_API> enter_3d(device, false);
  PPB_Graphics3D_Impl* graphics_3d =
      enter_3d.succeeded()
          ? static_cast<PPB_Graphics3D_Impl*>(enter_3d.object())
          : NULL;

  if (compositor) {
    if (compositor->BindToInstance(this)) {
      bound_compositor_ = compositor;
      UpdateLayer(true);
      return PP_TRUE;
    }
  } else if (graphics_2d) {
    if (graphics_2d->BindToInstance(this)) {
      bound_graphics_2d_platform_ = graphics_2d;
      UpdateLayer(true);
      return PP_TRUE;
    }
  } else if (graphics_3d) {
    // Make sure graphics can only be bound to the instance it is
    // associated with.
    if (graphics_3d->pp_instance() == pp_instance() &&
        graphics_3d->BindToInstance(true)) {
      bound_graphics_3d_ = graphics_3d;
      UpdateLayer(true);
      return PP_TRUE;
    }
  }

  // The instance cannot be bound or the device is not a valid resource type.
  return PP_FALSE;
}

PP_Bool PepperPluginInstanceImpl::IsFullFrame(PP_Instance instance) {
  return PP_FromBool(full_frame());
}

const ViewData* PepperPluginInstanceImpl::GetViewData(PP_Instance instance) {
  return &view_data_;
}

PP_Bool PepperPluginInstanceImpl::FlashIsFullscreen(PP_Instance instance) {
  return PP_FromBool(flash_fullscreen_);
}

PP_Var PepperPluginInstanceImpl::GetWindowObject(PP_Instance instance) {
  if (!container_)
    return PP_MakeUndefined();

  WebLocalFrame* frame = container_->element().document().frame();
  if (!frame)
    return PP_MakeUndefined();

  return NPObjectToPPVar(this, frame->windowObject());
}

PP_Var PepperPluginInstanceImpl::GetOwnerElementObject(PP_Instance instance) {
  if (!container_)
    return PP_MakeUndefined();
  return NPObjectToPPVar(this, container_->scriptableObjectForElement());
}

PP_Var PepperPluginInstanceImpl::ExecuteScript(PP_Instance instance,
                                               PP_Var script,
                                               PP_Var* exception) {
  // Executing the script may remove the plugin from the DOM, so we need to keep
  // a reference to ourselves so that we can still process the result after the
  // WebBindings::evaluate() below.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);
  TryCatch try_catch(exception);
  if (try_catch.has_exception())
    return PP_MakeUndefined();

  // Convert the script into an inconvenient NPString object.
  StringVar* script_string = StringVar::FromPPVar(script);
  if (!script_string) {
    try_catch.SetException("Script param to ExecuteScript must be a string.");
    return PP_MakeUndefined();
  }
  NPString np_script;
  np_script.UTF8Characters = script_string->value().c_str();
  np_script.UTF8Length = script_string->value().length();

  // Get the current frame to pass to the evaluate function.
  WebLocalFrame* frame = container_->element().document().frame();
  if (!frame) {
    try_catch.SetException("No frame to execute script in.");
    return PP_MakeUndefined();
  }

  NPVariant result;
  bool ok = false;
  if (IsProcessingUserGesture()) {
    blink::WebScopedUserGesture user_gesture(CurrentUserGestureToken());
    ok =
        WebBindings::evaluate(NULL, frame->windowObject(), &np_script, &result);
  } else {
    ok =
        WebBindings::evaluate(NULL, frame->windowObject(), &np_script, &result);
  }
  if (!ok) {
    // TryCatch doesn't catch the exceptions properly. Since this is only for
    // a trusted API, just set to a general exception message.
    try_catch.SetException("Exception caught");
    WebBindings::releaseVariantValue(&result);
    return PP_MakeUndefined();
  }

  PP_Var ret = NPVariantToPPVar(this, &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

uint32_t PepperPluginInstanceImpl::GetAudioHardwareOutputSampleRate(
    PP_Instance instance) {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputSampleRate();
}

uint32_t PepperPluginInstanceImpl::GetAudioHardwareOutputBufferSize(
    PP_Instance instance) {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread->GetAudioHardwareConfig()->GetOutputBufferSize();
}

PP_Var PepperPluginInstanceImpl::GetDefaultCharSet(PP_Instance instance) {
  if (!render_frame_)
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(
      render_frame_->render_view()->webkit_preferences().default_encoding);
}

// These PPB_ContentDecryptor_Private calls are responses to
// PPP_ContentDecryptor_Private calls made on |content_decryptor_delegate_|.
// Therefore, |content_decryptor_delegate_| must have been initialized when
// the following methods are called.
void PepperPluginInstanceImpl::PromiseResolved(PP_Instance instance,
                                               uint32 promise_id) {
  content_decryptor_delegate_->OnPromiseResolved(promise_id);
}

void PepperPluginInstanceImpl::PromiseResolvedWithSession(
    PP_Instance instance,
    uint32 promise_id,
    PP_Var web_session_id_var) {
  content_decryptor_delegate_->OnPromiseResolvedWithSession(promise_id,
                                                            web_session_id_var);
}

void PepperPluginInstanceImpl::PromiseRejected(
    PP_Instance instance,
    uint32 promise_id,
    PP_CdmExceptionCode exception_code,
    uint32 system_code,
    PP_Var error_description_var) {
  content_decryptor_delegate_->OnPromiseRejected(
      promise_id, exception_code, system_code, error_description_var);
}

void PepperPluginInstanceImpl::SessionMessage(PP_Instance instance,
                                              PP_Var web_session_id_var,
                                              PP_Var message_var,
                                              PP_Var destination_url_var) {
  content_decryptor_delegate_->OnSessionMessage(
      web_session_id_var, message_var, destination_url_var);
}

void PepperPluginInstanceImpl::SessionReady(PP_Instance instance,
                                            PP_Var web_session_id_var) {
  content_decryptor_delegate_->OnSessionReady(web_session_id_var);
}

void PepperPluginInstanceImpl::SessionClosed(PP_Instance instance,
                                             PP_Var web_session_id_var) {
  content_decryptor_delegate_->OnSessionClosed(web_session_id_var);
}

void PepperPluginInstanceImpl::SessionError(PP_Instance instance,
                                            PP_Var web_session_id_var,
                                            PP_CdmExceptionCode exception_code,
                                            uint32 system_code,
                                            PP_Var error_description_var) {
  content_decryptor_delegate_->OnSessionError(
      web_session_id_var, exception_code, system_code, error_description_var);
}

void PepperPluginInstanceImpl::DeliverBlock(
    PP_Instance instance,
    PP_Resource decrypted_block,
    const PP_DecryptedBlockInfo* block_info) {
  content_decryptor_delegate_->DeliverBlock(decrypted_block, block_info);
}

void PepperPluginInstanceImpl::DecoderInitializeDone(
    PP_Instance instance,
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id,
    PP_Bool success) {
  content_decryptor_delegate_->DecoderInitializeDone(
      decoder_type, request_id, success);
}

void PepperPluginInstanceImpl::DecoderDeinitializeDone(
    PP_Instance instance,
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  content_decryptor_delegate_->DecoderDeinitializeDone(decoder_type,
                                                       request_id);
}

void PepperPluginInstanceImpl::DecoderResetDone(
    PP_Instance instance,
    PP_DecryptorStreamType decoder_type,
    uint32_t request_id) {
  content_decryptor_delegate_->DecoderResetDone(decoder_type, request_id);
}

void PepperPluginInstanceImpl::DeliverFrame(
    PP_Instance instance,
    PP_Resource decrypted_frame,
    const PP_DecryptedFrameInfo* frame_info) {
  content_decryptor_delegate_->DeliverFrame(decrypted_frame, frame_info);
}

void PepperPluginInstanceImpl::DeliverSamples(
    PP_Instance instance,
    PP_Resource audio_frames,
    const PP_DecryptedSampleInfo* sample_info) {
  content_decryptor_delegate_->DeliverSamples(audio_frames, sample_info);
}

void PepperPluginInstanceImpl::SetPluginToHandleFindRequests(
    PP_Instance instance) {
  if (!LoadFindInterface())
    return;
  bool is_main_frame =
      render_frame_ &&
      render_frame_->GetRenderView()->GetMainRenderFrame() == render_frame_;
  if (!is_main_frame)
    return;
  render_frame_->render_view()->set_plugin_find_handler(this);
}

void PepperPluginInstanceImpl::NumberOfFindResultsChanged(
    PP_Instance instance,
    int32_t total,
    PP_Bool final_result) {
  DCHECK_NE(find_identifier_, -1);
  if (render_frame_) {
    render_frame_->reportFindInPageMatchCount(
        find_identifier_, total, PP_ToBool(final_result));
  }
}

void PepperPluginInstanceImpl::SelectedFindResultChanged(PP_Instance instance,
                                                         int32_t index) {
  DCHECK_NE(find_identifier_, -1);
  if (render_frame_) {
    render_frame_->reportFindInPageSelection(
        find_identifier_, index + 1, blink::WebRect());
  }
}

void PepperPluginInstanceImpl::SetTickmarks(PP_Instance instance,
                                            const PP_Rect* tickmarks,
                                            uint32_t count) {
  if (!render_frame_ || !render_frame_->GetWebFrame())
    return;

  blink::WebVector<blink::WebRect> tickmarks_converted(
      static_cast<size_t>(count));
  for (uint32 i = 0; i < count; ++i) {
    tickmarks_converted[i] = blink::WebRect(tickmarks[i].point.x,
                                            tickmarks[i].point.y,
                                            tickmarks[i].size.width,
                                            tickmarks[i].size.height);
    ;
  }
  blink::WebFrame* frame = render_frame_->GetWebFrame();
  frame->setTickmarks(tickmarks_converted);
}

PP_Bool PepperPluginInstanceImpl::IsFullscreen(PP_Instance instance) {
  return PP_FromBool(view_data_.is_fullscreen);
}

PP_Bool PepperPluginInstanceImpl::SetFullscreen(PP_Instance instance,
                                                PP_Bool fullscreen) {
  return PP_FromBool(SetFullscreen(PP_ToBool(fullscreen)));
}

PP_Bool PepperPluginInstanceImpl::GetScreenSize(PP_Instance instance,
                                                PP_Size* size) {
  blink::WebScreenInfo info = render_frame()->GetRenderWidget()->screenInfo();
  *size = PP_MakeSize(info.rect.width, info.rect.height);
  return PP_TRUE;
}

ppapi::Resource* PepperPluginInstanceImpl::GetSingletonResource(
    PP_Instance instance,
    ppapi::SingletonResourceID id) {
  // Flash APIs and some others aren't implemented in-process.
  switch (id) {
    case ppapi::BROKER_SINGLETON_ID:
    case ppapi::BROWSER_FONT_SINGLETON_ID:
    case ppapi::FILE_MAPPING_SINGLETON_ID:
    case ppapi::FLASH_CLIPBOARD_SINGLETON_ID:
    case ppapi::FLASH_FILE_SINGLETON_ID:
    case ppapi::FLASH_FULLSCREEN_SINGLETON_ID:
    case ppapi::FLASH_SINGLETON_ID:
    case ppapi::ISOLATED_FILESYSTEM_SINGLETON_ID:
    case ppapi::NETWORK_PROXY_SINGLETON_ID:
    case ppapi::PDF_SINGLETON_ID:
    case ppapi::TRUETYPE_FONT_SINGLETON_ID:
      NOTIMPLEMENTED();
      return NULL;
    case ppapi::GAMEPAD_SINGLETON_ID:
      return gamepad_impl_.get();
    case ppapi::UMA_SINGLETON_ID: {
      if (!uma_private_impl_) {
        RendererPpapiHostImpl* host_impl = module_->renderer_ppapi_host();
        if (host_impl->in_process_router()) {
          uma_private_impl_ = new ppapi::proxy::UMAPrivateResource(
              host_impl->in_process_router()->GetPluginConnection(instance),
              instance);
        }
      }
      return uma_private_impl_.get();
    }
  }

  NOTREACHED();
  return NULL;
}

int32_t PepperPluginInstanceImpl::RequestInputEvents(PP_Instance instance,
                                                     uint32_t event_classes) {
  input_event_mask_ |= event_classes;
  filtered_input_event_mask_ &= ~(event_classes);
  RequestInputEventsHelper(event_classes);
  return ValidateRequestInputEvents(false, event_classes);
}

int32_t PepperPluginInstanceImpl::RequestFilteringInputEvents(
    PP_Instance instance,
    uint32_t event_classes) {
  filtered_input_event_mask_ |= event_classes;
  input_event_mask_ &= ~(event_classes);
  RequestInputEventsHelper(event_classes);
  return ValidateRequestInputEvents(true, event_classes);
}

void PepperPluginInstanceImpl::ClearInputEventRequest(PP_Instance instance,
                                                      uint32_t event_classes) {
  input_event_mask_ &= ~(event_classes);
  filtered_input_event_mask_ &= ~(event_classes);
  RequestInputEventsHelper(event_classes);
}

void PepperPluginInstanceImpl::StartTrackingLatency(PP_Instance instance) {
  if (module_->permissions().HasPermission(ppapi::PERMISSION_PRIVATE))
    is_tracking_latency_ = true;
}

void PepperPluginInstanceImpl::ZoomChanged(PP_Instance instance,
                                           double factor) {
  // We only want to tell the page to change its zoom if the whole page is the
  // plugin.  If we're in an iframe, then don't do anything.
  if (!IsFullPagePlugin())
    return;
  container()->zoomLevelChanged(content::ZoomFactorToZoomLevel(factor));
}

void PepperPluginInstanceImpl::ZoomLimitsChanged(PP_Instance instance,
                                                 double minimum_factor,
                                                 double maximum_factor) {
  if (!render_frame_)
    return;
  if (minimum_factor > maximum_factor) {
    NOTREACHED();
    return;
  }
  double minimum_level = ZoomFactorToZoomLevel(minimum_factor);
  double maximum_level = ZoomFactorToZoomLevel(maximum_factor);
  render_frame_->render_view()->webview()->zoomLimitsChanged(minimum_level,
                                                             maximum_level);
}

void PepperPluginInstanceImpl::PostMessage(PP_Instance instance,
                                           PP_Var message) {
  PostMessageToJavaScript(message);
}

PP_Bool PepperPluginInstanceImpl::SetCursor(PP_Instance instance,
                                            PP_MouseCursor_Type type,
                                            PP_Resource image,
                                            const PP_Point* hot_spot) {
  if (!ValidateSetCursorParams(type, image, hot_spot))
    return PP_FALSE;

  if (type != PP_MOUSECURSOR_TYPE_CUSTOM) {
    DoSetCursor(new WebCursorInfo(static_cast<WebCursorInfo::Type>(type)));
    return PP_TRUE;
  }

  EnterResourceNoLock<PPB_ImageData_API> enter(image, true);
  if (enter.failed())
    return PP_FALSE;
  PPB_ImageData_Impl* image_data =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  ImageDataAutoMapper auto_mapper(image_data);
  if (!auto_mapper.is_valid())
    return PP_FALSE;

  scoped_ptr<WebCursorInfo> custom_cursor(
      new WebCursorInfo(WebCursorInfo::TypeCustom));
  custom_cursor->hotSpot.x = hot_spot->x;
  custom_cursor->hotSpot.y = hot_spot->y;

  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  // Make a deep copy, so that the cursor remains valid even after the original
  // image data gets freed.
  if (!bitmap->copyTo(&custom_cursor->customImage.getSkBitmap())) {
    return PP_FALSE;
  }

  DoSetCursor(custom_cursor.release());
  return PP_TRUE;
}

int32_t PepperPluginInstanceImpl::LockMouse(
    PP_Instance instance,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(lock_mouse_callback_))
    return PP_ERROR_INPROGRESS;

  if (IsMouseLocked())
    return PP_OK;

  if (!CanAccessMainFrame())
    return PP_ERROR_NOACCESS;

  if (!IsProcessingUserGesture())
    return PP_ERROR_NO_USER_GESTURE;

  // Attempt mouselock only if Flash isn't waiting on fullscreen, otherwise
  // we wait and call LockMouse() in UpdateFlashFullscreenState().
  if (!FlashIsFullscreenOrPending() || flash_fullscreen_) {
    // Open a user gesture here so the Webkit user gesture checks will succeed
    // for out-of-process plugins.
    WebScopedUserGesture user_gesture(CurrentUserGestureToken());
    if (!LockMouse())
      return PP_ERROR_FAILED;
  }

  // Either mouselock succeeded or a Flash fullscreen is pending.
  lock_mouse_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

void PepperPluginInstanceImpl::UnlockMouse(PP_Instance instance) {
  GetMouseLockDispatcher()->UnlockMouse(GetOrCreateLockTargetAdapter());
}

void PepperPluginInstanceImpl::SetTextInputType(PP_Instance instance,
                                                PP_TextInput_Type type) {
  if (!render_frame_)
    return;
  int itype = type;
  if (itype < 0 || itype > ui::TEXT_INPUT_TYPE_URL)
    itype = ui::TEXT_INPUT_TYPE_NONE;
  SetTextInputType(static_cast<ui::TextInputType>(itype));
}

void PepperPluginInstanceImpl::UpdateCaretPosition(
    PP_Instance instance,
    const PP_Rect& caret,
    const PP_Rect& bounding_box) {
  if (!render_frame_)
    return;
  text_input_caret_ = PP_ToGfxRect(caret);
  text_input_caret_bounds_ = PP_ToGfxRect(bounding_box);
  text_input_caret_set_ = true;
  render_frame_->PepperCaretPositionChanged(this);
}

void PepperPluginInstanceImpl::CancelCompositionText(PP_Instance instance) {
  if (render_frame_)
    render_frame_->PepperCancelComposition(this);
}

void PepperPluginInstanceImpl::SelectionChanged(PP_Instance instance) {
  // TODO(kinaba): currently the browser always calls RequestSurroundingText.
  // It can be optimized so that it won't call it back until the information
  // is really needed.

  // Avoid calling in nested context or else this will reenter the plugin. This
  // uses a weak pointer rather than exploiting the fact that this class is
  // refcounted because we don't actually want this operation to affect the
  // lifetime of the instance.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PepperPluginInstanceImpl::RequestSurroundingText,
                 weak_factory_.GetWeakPtr(),
                 static_cast<size_t>(kExtraCharsForTextInput)));
}

void PepperPluginInstanceImpl::UpdateSurroundingText(PP_Instance instance,
                                                     const char* text,
                                                     uint32_t caret,
                                                     uint32_t anchor) {
  if (!render_frame_)
    return;
  surrounding_text_ = text;
  selection_caret_ = caret;
  selection_anchor_ = anchor;
  render_frame_->PepperSelectionChanged(this);
}

PP_Var PepperPluginInstanceImpl::ResolveRelativeToDocument(
    PP_Instance instance,
    PP_Var relative,
    PP_URLComponents_Dev* components) {
  StringVar* relative_string = StringVar::FromPPVar(relative);
  if (!relative_string)
    return PP_MakeNull();

  WebElement plugin_element = container()->element();
  GURL document_url = plugin_element.document().baseURL();
  return ppapi::PPB_URLUtil_Shared::GenerateURLReturn(
      document_url.Resolve(relative_string->value()), components);
}

PP_Bool PepperPluginInstanceImpl::DocumentCanRequest(PP_Instance instance,
                                                     PP_Var url) {
  StringVar* url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return PP_FALSE;

  blink::WebSecurityOrigin security_origin;
  if (!SecurityOriginForInstance(instance, &security_origin))
    return PP_FALSE;

  GURL gurl(url_string->value());
  if (!gurl.is_valid())
    return PP_FALSE;

  return BoolToPPBool(security_origin.canRequest(gurl));
}

PP_Bool PepperPluginInstanceImpl::DocumentCanAccessDocument(
    PP_Instance instance,
    PP_Instance target) {
  blink::WebSecurityOrigin our_origin;
  if (!SecurityOriginForInstance(instance, &our_origin))
    return PP_FALSE;

  blink::WebSecurityOrigin target_origin;
  if (!SecurityOriginForInstance(instance, &target_origin))
    return PP_FALSE;

  return BoolToPPBool(our_origin.canAccess(target_origin));
}

PP_Var PepperPluginInstanceImpl::GetDocumentURL(
    PP_Instance instance,
    PP_URLComponents_Dev* components) {
  blink::WebDocument document = container()->element().document();
  return ppapi::PPB_URLUtil_Shared::GenerateURLReturn(document.url(),
                                                      components);
}

PP_Var PepperPluginInstanceImpl::GetPluginInstanceURL(
    PP_Instance instance,
    PP_URLComponents_Dev* components) {
  return ppapi::PPB_URLUtil_Shared::GenerateURLReturn(plugin_url_, components);
}

PP_Var PepperPluginInstanceImpl::GetPluginReferrerURL(
    PP_Instance instance,
    PP_URLComponents_Dev* components) {
  blink::WebDocument document = container()->element().document();
  if (!full_frame_)
    return ppapi::PPB_URLUtil_Shared::GenerateURLReturn(document.url(),
                                                        components);
  WebLocalFrame* frame = document.frame();
  if (!frame)
    return PP_MakeUndefined();
  const WebURLRequest& request = frame->dataSource()->originalRequest();
  WebString referer = request.httpHeaderField("Referer");
  if (referer.isEmpty())
    return PP_MakeUndefined();
  return ppapi::PPB_URLUtil_Shared::GenerateURLReturn(GURL(referer),
                                                      components);
}

PP_ExternalPluginResult PepperPluginInstanceImpl::ResetAsProxied(
    scoped_refptr<PluginModule> module) {
  // Save the original module and switch over to the new one now that this
  // plugin is using the IPC-based proxy.
  original_module_ = module_;
  module_ = module;

  // For NaCl instances, remember the NaCl plugin instance interface, so we
  // can shut it down by calling its DidDestroy in our Delete() method.
  original_instance_interface_.reset(instance_interface_.release());

  base::Callback<const void*(const char*)> get_plugin_interface_func =
      base::Bind(&PluginModule::GetPluginInterface, module_.get());
  PPP_Instance_Combined* ppp_instance_combined =
      PPP_Instance_Combined::Create(get_plugin_interface_func);
  if (!ppp_instance_combined) {
    // The proxy must support at least one usable PPP_Instance interface.
    // While this could be a failure to implement the interface in the NaCl
    // module, it is more likely that the NaCl process has crashed. Either
    // way, report that module initialization failed.
    return PP_EXTERNAL_PLUGIN_ERROR_MODULE;
  }

  instance_interface_.reset(ppp_instance_combined);
  // Clear all PPP interfaces we may have cached.
  plugin_find_interface_ = NULL;
  plugin_input_event_interface_ = NULL;
  checked_for_plugin_input_event_interface_ = false;
  plugin_mouse_lock_interface_ = NULL;
  plugin_pdf_interface_ = NULL;
  checked_for_plugin_pdf_interface_ = false;
  plugin_private_interface_ = NULL;
  plugin_selection_interface_ = NULL;
  plugin_textinput_interface_ = NULL;
  plugin_zoom_interface_ = NULL;

  // Re-send the DidCreate event via the proxy.
  scoped_ptr<const char * []> argn_array(StringVectorToArgArray(argn_));
  scoped_ptr<const char * []> argv_array(StringVectorToArgArray(argv_));
  if (!instance_interface_->DidCreate(
          pp_instance(), argn_.size(), argn_array.get(), argv_array.get()))
    return PP_EXTERNAL_PLUGIN_ERROR_INSTANCE;
  message_channel_->Start();

  // Clear sent_initial_did_change_view_ and cancel any pending DidChangeView
  // event. This way, SendDidChangeView will send the "current" view
  // immediately (before other events like HandleDocumentLoad).
  sent_initial_did_change_view_ = false;
  view_change_weak_ptr_factory_.InvalidateWeakPtrs();
  SendDidChangeView();

  DCHECK(external_document_load_);
  external_document_load_ = false;
  if (!external_document_response_.isNull()) {
    document_loader_ = NULL;
    // Pass the response to the new proxy.
    HandleDocumentLoad(external_document_response_);
    external_document_response_ = blink::WebURLResponse();
    // Replay any document load events we've received to the real loader.
    external_document_loader_->ReplayReceivedData(document_loader_);
    external_document_loader_.reset(NULL);
  }

  return PP_EXTERNAL_PLUGIN_OK;
}

bool PepperPluginInstanceImpl::IsValidInstanceOf(PluginModule* module) {
  DCHECK(module);
  return module == module_.get() || module == original_module_.get();
}

NPP PepperPluginInstanceImpl::instanceNPP() { return npp_.get(); }

PepperPluginInstance* PepperPluginInstance::Get(PP_Instance instance_id) {
  return HostGlobals::Get()->GetInstance(instance_id);
}

RenderView* PepperPluginInstanceImpl::GetRenderView() {
  return render_frame_ ? render_frame_->render_view() : NULL;
}

blink::WebPluginContainer* PepperPluginInstanceImpl::GetContainer() {
  return container_;
}

v8::Isolate* PepperPluginInstanceImpl::GetIsolate() const { return isolate_; }

ppapi::VarTracker* PepperPluginInstanceImpl::GetVarTracker() {
  return HostGlobals::Get()->GetVarTracker();
}

const GURL& PepperPluginInstanceImpl::GetPluginURL() { return plugin_url_; }

base::FilePath PepperPluginInstanceImpl::GetModulePath() {
  return module_->path();
}

PP_Resource PepperPluginInstanceImpl::CreateImage(gfx::ImageSkia* source_image,
                                                  float scale) {
  gfx::ImageSkiaRep image_skia_rep = source_image->GetRepresentation(scale);

  if (image_skia_rep.is_null() || image_skia_rep.scale() != scale)
    return 0;

  scoped_refptr<PPB_ImageData_Impl> image_data(
      new PPB_ImageData_Impl(pp_instance(), PPB_ImageData_Impl::PLATFORM));
  if (!image_data->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
                        image_skia_rep.pixel_width(),
                        image_skia_rep.pixel_height(),
                        false)) {
    return 0;
  }

  ImageDataAutoMapper mapper(image_data.get());
  if (!mapper.is_valid())
    return 0;

  skia::PlatformCanvas* canvas = image_data->GetPlatformCanvas();
  // Note: Do not SkBitmap::copyTo the canvas bitmap directly because it will
  // ignore the allocated pixels in shared memory and re-allocate a new buffer.
  canvas->writePixels(image_skia_rep.sk_bitmap(), 0, 0);

  return image_data->GetReference();
}

PP_ExternalPluginResult PepperPluginInstanceImpl::SwitchToOutOfProcessProxy(
    const base::FilePath& file_path,
    ppapi::PpapiPermissions permissions,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessId plugin_pid,
    int plugin_child_id) {
  // Create a new module for each instance of the external plugin that is using
  // the IPC based out-of-process proxy. We can't use the existing module,
  // because it is configured for the in-process plugin, and we must keep it
  // that way to allow the page to create other instances.
  scoped_refptr<PluginModule> external_plugin_module(
      module_->CreateModuleForExternalPluginInstance());

  RendererPpapiHostImpl* renderer_ppapi_host =
      external_plugin_module->CreateOutOfProcessModule(render_frame_,
                                                       file_path,
                                                       permissions,
                                                       channel_handle,
                                                       plugin_pid,
                                                       plugin_child_id,
                                                       true);
  if (!renderer_ppapi_host) {
    DLOG(ERROR) << "CreateExternalPluginModule() failed";
    return PP_EXTERNAL_PLUGIN_ERROR_MODULE;
  }

  // Finally, switch the instance to the proxy.
  return external_plugin_module->InitAsProxiedExternalPlugin(this);
}

void PepperPluginInstanceImpl::SetAlwaysOnTop(bool on_top) {
  always_on_top_ = on_top;
}

void PepperPluginInstanceImpl::DoSetCursor(WebCursorInfo* cursor) {
  cursor_.reset(cursor);
  if (fullscreen_container_) {
    fullscreen_container_->DidChangeCursor(*cursor);
  } else if (render_frame_) {
    render_frame_->PepperDidChangeCursor(this, *cursor);
  }
}

bool PepperPluginInstanceImpl::IsFullPagePlugin() {
  WebLocalFrame* frame = container()->element().document().frame();
  return frame->view()->mainFrame()->document().isPluginDocument();
}

bool PepperPluginInstanceImpl::FlashSetFullscreen(bool fullscreen,
                                                  bool delay_report) {
  TRACE_EVENT0("ppapi", "PepperPluginInstanceImpl::FlashSetFullscreen");
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PepperPluginInstanceImpl> ref(this);

  // We check whether we are trying to switch to the state we're already going
  // to (i.e. if we're already switching to fullscreen but the fullscreen
  // container isn't ready yet, don't do anything more).
  if (fullscreen == FlashIsFullscreenOrPending())
    return true;

  if (!render_frame_)
    return false;
  if (fullscreen && !render_frame_->render_view()
                         ->renderer_preferences()
                         .plugin_fullscreen_allowed)
    return false;

  // Unbind current 2D or 3D graphics context.
  VLOG(1) << "Setting fullscreen to " << (fullscreen ? "on" : "off");
  if (fullscreen) {
    DCHECK(!fullscreen_container_);
    fullscreen_container_ =
        render_frame_->CreatePepperFullscreenContainer(this);
    UpdateLayer(false);
  } else {
    DCHECK(fullscreen_container_);
    fullscreen_container_->Destroy();
    fullscreen_container_ = NULL;
    UpdateFlashFullscreenState(false);
    if (!delay_report) {
      ReportGeometry();
    } else {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&PepperPluginInstanceImpl::ReportGeometry, this));
    }
  }

  return true;
}

bool PepperPluginInstanceImpl::IsRectTopmost(const gfx::Rect& rect) {
  if (flash_fullscreen_)
    return true;

  return container_->isRectTopmost(rect);
}

int32_t PepperPluginInstanceImpl::Navigate(
    const ppapi::URLRequestInfoData& request,
    const char* target,
    bool from_user_action) {
  if (!container_)
    return PP_ERROR_FAILED;

  WebDocument document = container_->element().document();
  WebLocalFrame* frame = document.frame();
  if (!frame)
    return PP_ERROR_FAILED;

  ppapi::URLRequestInfoData completed_request = request;

  WebURLRequest web_request;
  if (!CreateWebURLRequest(
          pp_instance_, &completed_request, frame, &web_request)) {
    return PP_ERROR_FAILED;
  }
  web_request.setFirstPartyForCookies(document.firstPartyForCookies());
  web_request.setHasUserGesture(from_user_action);

  GURL gurl(web_request.url());
  if (gurl.SchemeIs("javascript")) {
    // In imitation of the NPAPI implementation, only |target_frame == frame| is
    // allowed for security reasons.
    WebFrame* target_frame =
        frame->view()->findFrameByName(WebString::fromUTF8(target), frame);
    if (target_frame != frame)
      return PP_ERROR_NOACCESS;

    // TODO(viettrungluu): NPAPI sends the result back to the plugin -- do we
    // need that?
    WebString result = container_->executeScriptURL(gurl, from_user_action);
    return result.isNull() ? PP_ERROR_FAILED : PP_OK;
  }

  // Only GETs and POSTs are supported.
  if (web_request.httpMethod() != "GET" && web_request.httpMethod() != "POST")
    return PP_ERROR_BADARGUMENT;

  WebString target_str = WebString::fromUTF8(target);
  container_->loadFrameRequest(web_request, target_str, false, NULL);
  return PP_OK;
}

int PepperPluginInstanceImpl::MakePendingFileRefRendererHost(
    const base::FilePath& path) {
  RendererPpapiHostImpl* host_impl = module_->renderer_ppapi_host();
  PepperFileRefRendererHost* file_ref_host(
      new PepperFileRefRendererHost(host_impl, pp_instance(), 0, path));
  return host_impl->GetPpapiHost()->AddPendingResourceHost(
      scoped_ptr<ppapi::host::ResourceHost>(file_ref_host));
}

void PepperPluginInstanceImpl::SetEmbedProperty(PP_Var key, PP_Var value) {
  message_channel_->SetReadOnlyProperty(key, value);
}

bool PepperPluginInstanceImpl::CanAccessMainFrame() const {
  if (!container_)
    return false;
  blink::WebDocument containing_document = container_->element().document();

  if (!containing_document.frame() || !containing_document.frame()->view() ||
      !containing_document.frame()->view()->mainFrame()) {
    return false;
  }
  blink::WebDocument main_document =
      containing_document.frame()->view()->mainFrame()->document();

  return containing_document.securityOrigin().canAccess(
      main_document.securityOrigin());
}

void PepperPluginInstanceImpl::KeepSizeAttributesBeforeFullscreen() {
  WebElement element = container_->element();
  width_before_fullscreen_ = element.getAttribute(WebString::fromUTF8(kWidth));
  height_before_fullscreen_ =
      element.getAttribute(WebString::fromUTF8(kHeight));
  border_before_fullscreen_ =
      element.getAttribute(WebString::fromUTF8(kBorder));
  style_before_fullscreen_ = element.getAttribute(WebString::fromUTF8(kStyle));
}

void PepperPluginInstanceImpl::SetSizeAttributesForFullscreen() {
  if (!render_frame_)
    return;
  blink::WebScreenInfo info = render_frame_->GetRenderWidget()->screenInfo();
  screen_size_for_fullscreen_ = gfx::Size(info.rect.width, info.rect.height);
  std::string width = StringPrintf("%d", screen_size_for_fullscreen_.width());
  std::string height = StringPrintf("%d", screen_size_for_fullscreen_.height());

  WebElement element = container_->element();
  element.setAttribute(WebString::fromUTF8(kWidth), WebString::fromUTF8(width));
  element.setAttribute(WebString::fromUTF8(kHeight),
                       WebString::fromUTF8(height));
  element.setAttribute(WebString::fromUTF8(kBorder), WebString::fromUTF8("0"));

  // There should be no style settings that matter in fullscreen mode,
  // so just replace them instead of appending.
  // NOTE: "position: fixed" and "display: block" reset the plugin and
  // using %% settings might not work without them (e.g. if the plugin is a
  // child of a container element).
  std::string style;
  style += StringPrintf("width: %s !important; ", width.c_str());
  style += StringPrintf("height: %s !important; ", height.c_str());
  style += "margin: 0 !important; padding: 0 !important; border: 0 !important";
  container_->element().setAttribute(kStyle, WebString::fromUTF8(style));
}

void PepperPluginInstanceImpl::ResetSizeAttributesAfterFullscreen() {
  screen_size_for_fullscreen_ = gfx::Size();
  WebElement element = container_->element();
  element.setAttribute(WebString::fromUTF8(kWidth), width_before_fullscreen_);
  element.setAttribute(WebString::fromUTF8(kHeight), height_before_fullscreen_);
  element.setAttribute(WebString::fromUTF8(kBorder), border_before_fullscreen_);
  element.setAttribute(WebString::fromUTF8(kStyle), style_before_fullscreen_);
}

bool PepperPluginInstanceImpl::IsMouseLocked() {
  return GetMouseLockDispatcher()->IsMouseLockedTo(
      GetOrCreateLockTargetAdapter());
}

bool PepperPluginInstanceImpl::LockMouse() {
  return GetMouseLockDispatcher()->LockMouse(GetOrCreateLockTargetAdapter());
}

MouseLockDispatcher::LockTarget*
PepperPluginInstanceImpl::GetOrCreateLockTargetAdapter() {
  if (!lock_target_.get()) {
    lock_target_.reset(new PluginInstanceLockTarget(this));
  }
  return lock_target_.get();
}

MouseLockDispatcher* PepperPluginInstanceImpl::GetMouseLockDispatcher() {
  if (flash_fullscreen_) {
    RenderWidgetFullscreenPepper* container =
        static_cast<RenderWidgetFullscreenPepper*>(fullscreen_container_);
    return container->mouse_lock_dispatcher();
  } else if (render_frame_) {
    return render_frame_->render_view()->mouse_lock_dispatcher();
  }
  return NULL;
}

void PepperPluginInstanceImpl::UnSetAndDeleteLockTargetAdapter() {
  if (lock_target_.get()) {
    GetMouseLockDispatcher()->OnLockTargetDestroyed(lock_target_.get());
    lock_target_.reset();
  }
}

void PepperPluginInstanceImpl::DidDataFromWebURLResponse(
    const blink::WebURLResponse& response,
    int pending_host_id,
    const ppapi::URLResponseInfoData& data) {
  if (is_deleted_)
    return;

  RendererPpapiHostImpl* host_impl = module_->renderer_ppapi_host();

  if (host_impl->in_process_router()) {
    // Running in-process, we can just create the resource and call the
    // PPP_Instance function directly.
    scoped_refptr<ppapi::proxy::URLLoaderResource> loader_resource(
        new ppapi::proxy::URLLoaderResource(
            host_impl->in_process_router()->GetPluginConnection(pp_instance()),
            pp_instance(),
            pending_host_id,
            data));

    PP_Resource loader_pp_resource = loader_resource->GetReference();
    if (!instance_interface_->HandleDocumentLoad(pp_instance(),
                                                 loader_pp_resource))
      loader_resource->Close();
    // We don't pass a ref into the plugin, if it wants one, it will have taken
    // an additional one.
    ppapi::PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(
        loader_pp_resource);
  } else {
    // Running out-of-process. Initiate an IPC call to notify the plugin
    // process.
    ppapi::proxy::HostDispatcher* dispatcher =
        ppapi::proxy::HostDispatcher::GetForInstance(pp_instance());
    dispatcher->Send(new PpapiMsg_PPPInstance_HandleDocumentLoad(
        ppapi::API_ID_PPP_INSTANCE, pp_instance(), pending_host_id, data));
  }
}

}  // namespace content

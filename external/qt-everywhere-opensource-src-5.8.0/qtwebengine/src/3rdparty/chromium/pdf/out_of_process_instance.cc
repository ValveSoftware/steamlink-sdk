// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/out_of_process_instance.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>  // for min/max()
#define _USE_MATH_DEFINES  // for M_PI
#include <cmath>      // for log() and pow()
#include <math.h>
#include <list>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/content_restriction.h"
#include "net/base/escape.h"
#include "pdf/pdf.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/dev/text_input_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/private/var_private.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace chrome_pdf {

const char kChromePrint[] = "chrome://print/";
const char kChromeExtension[] =
    "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai";

// Constants used in handling postMessage() messages.
const char kType[] = "type";
// Viewport message arguments. (Page -> Plugin).
const char kJSViewportType[] = "viewport";
const char kJSXOffset[] = "xOffset";
const char kJSYOffset[] = "yOffset";
const char kJSZoom[] = "zoom";
// Stop scrolling message (Page -> Plugin)
const char kJSStopScrollingType[] = "stopScrolling";
// Document dimension arguments (Plugin -> Page).
const char kJSDocumentDimensionsType[] = "documentDimensions";
const char kJSDocumentWidth[] = "width";
const char kJSDocumentHeight[] = "height";
const char kJSPageDimensions[] = "pageDimensions";
const char kJSPageX[] = "x";
const char kJSPageY[] = "y";
const char kJSPageWidth[] = "width";
const char kJSPageHeight[] = "height";
// Document load progress arguments (Plugin -> Page)
const char kJSLoadProgressType[] = "loadProgress";
const char kJSProgressPercentage[] = "progress";
// Metadata
const char kJSMetadataType[] = "metadata";
const char kJSBookmarks[] = "bookmarks";
const char kJSTitle[] = "title";
// Get password arguments (Plugin -> Page)
const char kJSGetPasswordType[] = "getPassword";
// Get password complete arguments (Page -> Plugin)
const char kJSGetPasswordCompleteType[] = "getPasswordComplete";
const char kJSPassword[] = "password";
// Print (Page -> Plugin)
const char kJSPrintType[] = "print";
// Save (Page -> Plugin)
const char kJSSaveType[] = "save";
// Go to page (Plugin -> Page)
const char kJSGoToPageType[] = "goToPage";
const char kJSPageNumber[] = "page";
// Reset print preview mode (Page -> Plugin)
const char kJSResetPrintPreviewModeType[] = "resetPrintPreviewMode";
const char kJSPrintPreviewUrl[] = "url";
const char kJSPrintPreviewGrayscale[] = "grayscale";
const char kJSPrintPreviewPageCount[] = "pageCount";
// Load preview page (Page -> Plugin)
const char kJSLoadPreviewPageType[] = "loadPreviewPage";
const char kJSPreviewPageUrl[] = "url";
const char kJSPreviewPageIndex[] = "index";
// Set scroll position (Plugin -> Page)
const char kJSSetScrollPositionType[] = "setScrollPosition";
const char kJSPositionX[] = "x";
const char kJSPositionY[] = "y";
// Cancel the stream URL request (Plugin -> Page)
const char kJSCancelStreamUrlType[] = "cancelStreamUrl";
// Navigate to the given URL (Plugin -> Page)
const char kJSNavigateType[] = "navigate";
const char kJSNavigateUrl[] = "url";
const char kJSNavigateNewTab[] = "newTab";
// Open the email editor with the given parameters (Plugin -> Page)
const char kJSEmailType[] = "email";
const char kJSEmailTo[] = "to";
const char kJSEmailCc[] = "cc";
const char kJSEmailBcc[] = "bcc";
const char kJSEmailSubject[] = "subject";
const char kJSEmailBody[] = "body";
// Rotation (Page -> Plugin)
const char kJSRotateClockwiseType[] = "rotateClockwise";
const char kJSRotateCounterclockwiseType[] = "rotateCounterclockwise";
// Select all text in the document (Page -> Plugin)
const char kJSSelectAllType[] = "selectAll";
// Get the selected text in the document (Page -> Plugin)
const char kJSGetSelectedTextType[] = "getSelectedText";
// Reply with selected text (Plugin -> Page)
const char kJSGetSelectedTextReplyType[] = "getSelectedTextReply";
const char kJSSelectedText[] = "selectedText";

// Get the named destination with the given name (Page -> Plugin)
const char KJSGetNamedDestinationType[] = "getNamedDestination";
const char KJSGetNamedDestination[] = "namedDestination";
// Reply with the page number of the named destination (Plugin -> Page)
const char kJSGetNamedDestinationReplyType[] = "getNamedDestinationReply";
const char kJSNamedDestinationPageNumber[] = "pageNumber";

// Selecting text in document (Plugin -> Page)
const char kJSSetIsSelectingType[] = "setIsSelecting";
const char kJSIsSelecting[] = "isSelecting";

// Notify when a form field is focused (Plugin -> Page)
const char kJSFieldFocusType[] = "formFocusChange";
const char kJSFieldFocus[] = "focused";

const int kFindResultCooldownMs = 100;

// A delay to wait between each accessibility page to keep the system
// responsive.
const int kAccessibilityPageDelayMs = 100;

const double kMinZoom = 0.01;

namespace {

static const char kPPPPdfInterface[] = PPP_PDF_INTERFACE_1;

// Used for UMA. Do not delete entries, and keep in sync with histograms.xml.
enum PDFFeatures {
  LOADED_DOCUMENT = 0,
  HAS_TITLE = 1,
  HAS_BOOKMARKS = 2,
  FEATURES_COUNT
};

PP_Var GetLinkAtPosition(PP_Instance instance, PP_Point point) {
  pp::Var var;
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    var = static_cast<OutOfProcessInstance*>(object)->GetLinkAtPosition(
        pp::Point(point));
  }
  return var.Detach();
}

void Transform(PP_Instance instance, PP_PrivatePageTransformType type) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    OutOfProcessInstance* obj_instance =
        static_cast<OutOfProcessInstance*>(object);
    switch (type) {
      case PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CW:
        obj_instance->RotateClockwise();
        break;
      case PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CCW:
        obj_instance->RotateCounterclockwise();
        break;
    }
  }
}

PP_Bool GetPrintPresetOptionsFromDocument(
    PP_Instance instance,
    PP_PdfPrintPresetOptions_Dev* options) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    OutOfProcessInstance* obj_instance =
        static_cast<OutOfProcessInstance*>(object);
    obj_instance->GetPrintPresetOptionsFromDocument(options);
  }
  return PP_TRUE;
}

void EnableAccessibility(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    OutOfProcessInstance* obj_instance =
        static_cast<OutOfProcessInstance*>(object);
    return obj_instance->EnableAccessibility();
  }
}

const PPP_Pdf ppp_private = {
  &GetLinkAtPosition,
  &Transform,
  &GetPrintPresetOptionsFromDocument,
  &EnableAccessibility,
};

int ExtractPrintPreviewPageIndex(const std::string& src_url) {
  // Sample |src_url| format: chrome://print/id/page_index/print.pdf
  std::vector<std::string> url_substr = base::SplitString(
      src_url.substr(strlen(kChromePrint)), "/",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3)
    return -1;

  if (url_substr[2] != "print.pdf")
    return -1;

  int page_index = 0;
  if (!base::StringToInt(url_substr[1], &page_index))
    return -1;
  return page_index;
}

bool IsPrintPreviewUrl(const std::string& url) {
  return url.substr(0, strlen(kChromePrint)) == kChromePrint;
}

void ScalePoint(float scale, pp::Point* point) {
  point->set_x(static_cast<int>(point->x() * scale));
  point->set_y(static_cast<int>(point->y() * scale));
}

void ScaleRect(float scale, pp::Rect* rect) {
  int left = static_cast<int>(floorf(rect->x() * scale));
  int top = static_cast<int>(floorf(rect->y() * scale));
  int right = static_cast<int>(ceilf((rect->x() + rect->width()) * scale));
  int bottom = static_cast<int>(ceilf((rect->y() + rect->height()) * scale));
  rect->SetRect(left, top, right - left, bottom - top);
}

// TODO(raymes): Remove this dependency on VarPrivate/InstancePrivate. It's
// needed right now to do a synchronous call to JavaScript, but we could easily
// replace this with a custom PPB_PDF function.
pp::Var ModalDialog(const pp::Instance* instance,
                    const std::string& type,
                    const std::string& message,
                    const std::string& default_answer) {
  const PPB_Instance_Private* interface =
      reinterpret_cast<const PPB_Instance_Private*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_INSTANCE_PRIVATE_INTERFACE));
  pp::VarPrivate window(pp::PASS_REF,
      interface->GetWindowObject(instance->pp_instance()));
  if (default_answer.empty())
    return window.Call(type, message);
  else
    return window.Call(type, message, default_answer);
}

}  // namespace

OutOfProcessInstance::OutOfProcessInstance(PP_Instance instance)
    : pp::Instance(instance),
      pp::Find_Private(this),
      pp::Printing_Dev(this),
      cursor_(PP_CURSORTYPE_POINTER),
      zoom_(1.0),
      device_scale_(1.0),
      full_(false),
      paint_manager_(this, this, true),
      first_paint_(true),
      document_load_state_(LOAD_STATE_LOADING),
      preview_document_load_state_(LOAD_STATE_COMPLETE),
      uma_(this),
      told_browser_about_unsupported_feature_(false),
      print_preview_page_count_(0),
      last_progress_sent_(0),
      recently_sent_find_update_(false),
      received_viewport_message_(false),
      did_call_start_loading_(false),
      stop_scrolling_(false),
      background_color_(0),
      top_toolbar_height_(0),
      accessibility_state_(ACCESSIBILITY_STATE_OFF) {
  loader_factory_.Initialize(this);
  timer_factory_.Initialize(this);
  form_factory_.Initialize(this);
  print_callback_factory_.Initialize(this);
  engine_.reset(PDFEngine::Create(this));
  pp::Module::Get()->AddPluginInterface(kPPPPdfInterface, &ppp_private);
  AddPerInstanceObject(kPPPPdfInterface, this);

  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_TOUCH);
}

OutOfProcessInstance::~OutOfProcessInstance() {
  RemovePerInstanceObject(kPPPPdfInterface, this);
  // Explicitly reset the PDFEngine during destruction as it may call back into
  // this object.
  engine_.reset();
}

bool OutOfProcessInstance::Init(uint32_t argc,
                                const char* argn[],
                                const char* argv[]) {
  // Check if the PDF is being loaded in the PDF chrome extension. We only allow
  // the plugin to be loaded in the extension and print preview to avoid
  // exposing sensitive APIs directly to external websites.
  pp::Var document_url_var = pp::URLUtil_Dev::Get()->GetDocumentURL(this);
  if (!document_url_var.is_string())
    return false;
  std::string document_url = document_url_var.AsString();
  std::string extension_url = std::string(kChromeExtension);
  std::string print_preview_url = std::string(kChromePrint);
  if (!base::StringPiece(document_url).starts_with(kChromeExtension) &&
      !base::StringPiece(document_url).starts_with(kChromePrint)) {
    return false;
  }

  // Check if the plugin is full frame. This is passed in from JS.
  for (uint32_t i = 0; i < argc; ++i) {
    if (strcmp(argn[i], "full-frame") == 0) {
      full_ = true;
      break;
    }
  }

  // Only allow the plugin to handle find requests if it is full frame.
  if (full_)
    SetPluginToHandleFindRequests();

  text_input_.reset(new pp::TextInput_Dev(this));

  const char* stream_url = nullptr;
  const char* original_url = nullptr;
  const char* headers = nullptr;
  for (uint32_t i = 0; i < argc; ++i) {
    bool success = true;
    if (strcmp(argn[i], "src") == 0)
      original_url = argv[i];
    else if (strcmp(argn[i], "stream-url") == 0)
      stream_url = argv[i];
    else if (strcmp(argn[i], "headers") == 0)
      headers = argv[i];
    else if (strcmp(argn[i], "background-color") == 0)
      success = base::HexStringToUInt(argv[i], &background_color_);
    else if (strcmp(argn[i], "top-toolbar-height") == 0)
      success = base::StringToInt(argv[i], &top_toolbar_height_);

    if (!success)
      return false;
  }

  if (!original_url)
    return false;

  if (!stream_url)
    stream_url = original_url;

  // If we're in print preview mode we don't need to load the document yet.
  // A |kJSResetPrintPreviewModeType| message will be sent to the plugin letting
  // it know the url to load. By not loading here we avoid loading the same
  // document twice.
  if (IsPrintPreviewUrl(original_url))
    return true;

  LoadUrl(stream_url);
  url_ = original_url;
  return engine_->New(original_url, headers);
}

void OutOfProcessInstance::HandleMessage(const pp::Var& message) {
  pp::VarDictionary dict(message);
  if (!dict.Get(kType).is_string()) {
    NOTREACHED();
    return;
  }

  std::string type = dict.Get(kType).AsString();

  if (type == kJSViewportType &&
      dict.Get(pp::Var(kJSXOffset)).is_number() &&
      dict.Get(pp::Var(kJSYOffset)).is_number() &&
      dict.Get(pp::Var(kJSZoom)).is_number()) {
    received_viewport_message_ = true;
    stop_scrolling_ = false;
    double zoom = dict.Get(pp::Var(kJSZoom)).AsDouble();
    pp::FloatPoint scroll_offset(dict.Get(pp::Var(kJSXOffset)).AsDouble(),
                                 dict.Get(pp::Var(kJSYOffset)).AsDouble());

    // Bound the input parameters.
    zoom = std::max(kMinZoom, zoom);
    SetZoom(zoom);
    scroll_offset = BoundScrollOffsetToDocument(scroll_offset);
    engine_->ScrolledToXPosition(scroll_offset.x() * device_scale_);
    engine_->ScrolledToYPosition(scroll_offset.y() * device_scale_);
  } else if (type == kJSGetPasswordCompleteType &&
             dict.Get(pp::Var(kJSPassword)).is_string()) {
    if (password_callback_) {
      pp::CompletionCallbackWithOutput<pp::Var> callback = *password_callback_;
      password_callback_.reset();
      *callback.output() = dict.Get(pp::Var(kJSPassword)).pp_var();
      callback.Run(PP_OK);
    } else {
      NOTREACHED();
    }
  } else if (type == kJSPrintType) {
    Print();
  } else if (type == kJSSaveType) {
    pp::PDF::SaveAs(this);
  } else if (type == kJSRotateClockwiseType) {
    RotateClockwise();
  } else if (type == kJSRotateCounterclockwiseType) {
    RotateCounterclockwise();
  } else if (type == kJSSelectAllType) {
    engine_->SelectAll();
  } else if (type == kJSResetPrintPreviewModeType &&
             dict.Get(pp::Var(kJSPrintPreviewUrl)).is_string() &&
             dict.Get(pp::Var(kJSPrintPreviewGrayscale)).is_bool() &&
             dict.Get(pp::Var(kJSPrintPreviewPageCount)).is_int()) {
    url_ = dict.Get(pp::Var(kJSPrintPreviewUrl)).AsString();
    preview_pages_info_ = std::queue<PreviewPageInfo>();
    preview_document_load_state_ = LOAD_STATE_COMPLETE;
    document_load_state_ = LOAD_STATE_LOADING;
    LoadUrl(url_);
    preview_engine_.reset();
    engine_.reset(PDFEngine::Create(this));
    engine_->SetGrayscale(dict.Get(pp::Var(kJSPrintPreviewGrayscale)).AsBool());
    engine_->New(url_.c_str(), nullptr /* empty header */);

    print_preview_page_count_ =
        std::max(dict.Get(pp::Var(kJSPrintPreviewPageCount)).AsInt(), 0);

    paint_manager_.InvalidateRect(pp::Rect(pp::Point(), plugin_size_));
  } else if (type == kJSLoadPreviewPageType &&
             dict.Get(pp::Var(kJSPreviewPageUrl)).is_string() &&
             dict.Get(pp::Var(kJSPreviewPageIndex)).is_int()) {
    ProcessPreviewPageInfo(dict.Get(pp::Var(kJSPreviewPageUrl)).AsString(),
                           dict.Get(pp::Var(kJSPreviewPageIndex)).AsInt());
  } else if (type == kJSStopScrollingType) {
    stop_scrolling_ = true;
  } else if (type == kJSGetSelectedTextType) {
    std::string selected_text = engine_->GetSelectedText();
    // Always return unix newlines to JS.
    base::ReplaceChars(selected_text, "\r", std::string(), &selected_text);
    pp::VarDictionary reply;
    reply.Set(pp::Var(kType), pp::Var(kJSGetSelectedTextReplyType));
    reply.Set(pp::Var(kJSSelectedText), selected_text);
    PostMessage(reply);
  } else if (type == KJSGetNamedDestinationType &&
             dict.Get(pp::Var(KJSGetNamedDestination)).is_string()) {
    int page_number = engine_->GetNamedDestinationPage(
        dict.Get(pp::Var(KJSGetNamedDestination)).AsString());
    pp::VarDictionary reply;
    reply.Set(pp::Var(kType), pp::Var(kJSGetNamedDestinationReplyType));
    if (page_number >= 0)
      reply.Set(pp::Var(kJSNamedDestinationPageNumber), page_number);
    PostMessage(reply);
  } else {
    NOTREACHED();
  }
}

bool OutOfProcessInstance::HandleInputEvent(
    const pp::InputEvent& event) {
  // To simplify things, convert the event into device coordinates if it is
  // a mouse event.
  pp::InputEvent event_device_res(event);
  {
    pp::MouseInputEvent mouse_event(event);
    if (!mouse_event.is_null()) {
      pp::Point point = mouse_event.GetPosition();
      pp::Point movement = mouse_event.GetMovement();
      ScalePoint(device_scale_, &point);
      ScalePoint(device_scale_, &movement);
      mouse_event = pp::MouseInputEvent(
          this,
          event.GetType(),
          event.GetTimeStamp(),
          event.GetModifiers(),
          mouse_event.GetButton(),
          point,
          mouse_event.GetClickCount(),
          movement);
      event_device_res = mouse_event;
    }
  }

  pp::InputEvent offset_event(event_device_res);
  switch (offset_event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE: {
      pp::MouseInputEvent mouse_event(event_device_res);
      pp::MouseInputEvent mouse_event_dip(event);
      pp::Point point = mouse_event.GetPosition();
      point.set_x(point.x() - available_area_.x());
      offset_event = pp::MouseInputEvent(
          this,
          event.GetType(),
          event.GetTimeStamp(),
          event.GetModifiers(),
          mouse_event.GetButton(),
          point,
          mouse_event.GetClickCount(),
          mouse_event.GetMovement());
      break;
    }
    default:
      break;
  }
  if (engine_->HandleEvent(offset_event))
    return true;

  // Middle click is used for scrolling and is handled by the container page.
  pp::MouseInputEvent mouse_event(event_device_res);
  if (!mouse_event.is_null() &&
      mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_MIDDLE) {
    return false;
  }

  // Return true for unhandled clicks so the plugin takes focus.
  return (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN);
}

void OutOfProcessInstance::DidChangeView(const pp::View& view) {
  pp::Rect view_rect(view.GetRect());
  float old_device_scale = device_scale_;
  float device_scale = view.GetDeviceScale();
  pp::Size view_device_size(view_rect.width() * device_scale,
                            view_rect.height() * device_scale);

  if (view_device_size != plugin_size_ || device_scale != device_scale_) {
    device_scale_ = device_scale;
    plugin_dip_size_ = view_rect.size();
    plugin_size_ = view_device_size;

    paint_manager_.SetSize(view_device_size, device_scale_);

    pp::Size new_image_data_size = PaintManager::GetNewContextSize(
        image_data_.size(),
        plugin_size_);
    if (new_image_data_size != image_data_.size()) {
      image_data_ = pp::ImageData(this,
                                  PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                  new_image_data_size,
                                  false);
      first_paint_ = true;
    }

    if (image_data_.is_null()) {
      DCHECK(plugin_size_.IsEmpty());
      return;
    }

    OnGeometryChanged(zoom_, old_device_scale);
  }

  if (!stop_scrolling_) {
    pp::Point scroll_offset(view.GetScrollOffset());
    // Because view messages come from the DOM, the coordinates of the viewport
    // are 0-based (i.e. they do not correspond to the viewport's coordinates in
    // JS), so we need to subtract the toolbar height to convert them into
    // viewport coordinates.
    pp::FloatPoint scroll_offset_float(scroll_offset.x(),
                                       scroll_offset.y() - top_toolbar_height_);
    scroll_offset_float = BoundScrollOffsetToDocument(scroll_offset_float);
    engine_->ScrolledToXPosition(scroll_offset_float.x() * device_scale_);
    engine_->ScrolledToYPosition(scroll_offset_float.y() * device_scale_);
  }
}

void OutOfProcessInstance::GetPrintPresetOptionsFromDocument(
    PP_PdfPrintPresetOptions_Dev* options) {
  options->is_scaling_disabled = PP_FromBool(IsPrintScalingDisabled());
  options->duplex =
      static_cast<PP_PrivateDuplexMode_Dev>(engine_->GetDuplexType());
  options->copies = engine_->GetCopiesToPrint();
  pp::Size uniform_page_size;
  options->is_page_size_uniform =
      PP_FromBool(engine_->GetPageSizeAndUniformity(&uniform_page_size));
  options->uniform_page_size = uniform_page_size;
}

void OutOfProcessInstance::EnableAccessibility() {
  if (accessibility_state_ == ACCESSIBILITY_STATE_LOADED)
    return;

  if (accessibility_state_ == ACCESSIBILITY_STATE_OFF)
    accessibility_state_ = ACCESSIBILITY_STATE_PENDING;

  if (document_load_state_ == LOAD_STATE_COMPLETE)
    LoadAccessibility();
}

void OutOfProcessInstance::LoadAccessibility() {
  accessibility_state_ = ACCESSIBILITY_STATE_LOADED;
  PP_PrivateAccessibilityDocInfo doc_info;
  doc_info.page_count = engine_->GetNumberOfPages();
  doc_info.text_accessible = PP_FromBool(
      engine_->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE));
  doc_info.text_copyable = PP_FromBool(
      engine_->HasPermission(PDFEngine::PERMISSION_COPY));

  pp::PDF::SetAccessibilityDocInfo(GetPluginInstance(), &doc_info);

  // If the document contents isn't accessible, don't send anything more.
  if (!(engine_->HasPermission(PDFEngine::PERMISSION_COPY) ||
        engine_->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE))) {
    return;
  }

  PP_PrivateAccessibilityViewportInfo viewport_info;
  viewport_info.scroll.x = 0;
  viewport_info.scroll.y = -top_toolbar_height_ * device_scale_;
  viewport_info.offset = available_area_.point();
  viewport_info.zoom = zoom_ * device_scale_;
  pp::PDF::SetAccessibilityViewportInfo(GetPluginInstance(), &viewport_info);

  // Schedule loading the first page.
  pp::CompletionCallback callback = timer_factory_.NewCallback(
      &OutOfProcessInstance::SendNextAccessibilityPage);
  pp::Module::Get()->core()->CallOnMainThread(kAccessibilityPageDelayMs,
                                              callback, 0);
}

void OutOfProcessInstance::SendNextAccessibilityPage(int32_t page_index) {
  int page_count = engine_->GetNumberOfPages();
  if (page_index < 0 || page_index >= page_count)
    return;

  int char_count = engine_->GetCharCount(page_index);
  PP_PrivateAccessibilityPageInfo page_info;
  page_info.page_index = page_index;
  page_info.bounds = engine_->GetPageBoundsRect(page_index);
  page_info.char_count = char_count;

  std::vector<PP_PrivateAccessibilityCharInfo> chars(page_info.char_count);
  for (uint32_t i = 0; i < page_info.char_count; ++i) {
    chars[i].unicode_character = engine_->GetCharUnicode(page_index, i);
  }

  std::vector<PP_PrivateAccessibilityTextRunInfo> text_runs;
  int char_index = 0;
  while (char_index < char_count) {
    PP_PrivateAccessibilityTextRunInfo text_run_info;
    pp::FloatRect bounds;
    engine_->GetTextRunInfo(page_index, char_index, &text_run_info.len,
                            &text_run_info.font_size, &bounds);
    DCHECK_LE(char_index + text_run_info.len,
              static_cast<uint32_t>(char_count));
    text_run_info.direction = PP_PRIVATEDIRECTION_LTR;
    text_run_info.bounds = bounds;
    text_runs.push_back(text_run_info);

    // We need to provide enough information to draw a bounding box
    // around any arbitrary text range, but the bounding boxes of characters
    // we get from PDFium don't necessarily "line up". Walk through the
    // characters in each text run and let the width of each character be
    // the difference between the x coordinate of one character and the
    // x coordinate of the next. The rest of the bounds of each character
    // can be computed from the bounds of the text run.
    pp::FloatRect char_bounds = engine_->GetCharBounds(page_index, char_index);
    for (uint32_t i = 0; i < text_run_info.len - 1; i++) {
      DCHECK_LT(char_index + i + 1,
                static_cast<uint32_t>(char_count));
      pp::FloatRect next_char_bounds = engine_->GetCharBounds(
          page_index, char_index + i + 1);
      chars[char_index + i].char_width = next_char_bounds.x() - char_bounds.x();
      char_bounds = next_char_bounds;
    }
    chars[char_index + text_run_info.len - 1].char_width = char_bounds.width();

    char_index += text_run_info.len;
  }

  page_info.text_run_count = text_runs.size();
  pp::PDF::SetAccessibilityPageInfo(GetPluginInstance(), &page_info,
                                    text_runs.data(), chars.data());

  // Schedule loading the next page.
  pp::CompletionCallback callback = timer_factory_.NewCallback(
      &OutOfProcessInstance::SendNextAccessibilityPage);
  pp::Module::Get()->core()->CallOnMainThread(kAccessibilityPageDelayMs,
                                              callback, page_index + 1);
}

pp::Var OutOfProcessInstance::GetLinkAtPosition(
    const pp::Point& point) {
  pp::Point offset_point(point);
  ScalePoint(device_scale_, &offset_point);
  offset_point.set_x(offset_point.x() - available_area_.x());
  return engine_->GetLinkAtPosition(offset_point);
}

uint32_t OutOfProcessInstance::QuerySupportedPrintOutputFormats() {
  return engine_->QuerySupportedPrintOutputFormats();
}

int32_t OutOfProcessInstance::PrintBegin(
    const PP_PrintSettings_Dev& print_settings) {
  // For us num_pages is always equal to the number of pages in the PDF
  // document irrespective of the printable area.
  int32_t ret = engine_->GetNumberOfPages();
  if (!ret)
    return 0;

  uint32_t supported_formats = engine_->QuerySupportedPrintOutputFormats();
  if ((print_settings.format & supported_formats) == 0)
    return 0;

  print_settings_.is_printing = true;
  print_settings_.pepper_print_settings = print_settings;
  engine_->PrintBegin();
  return ret;
}

pp::Resource OutOfProcessInstance::PrintPages(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count) {
  if (!print_settings_.is_printing)
    return pp::Resource();

  print_settings_.print_pages_called_ = true;
  return engine_->PrintPages(page_ranges, page_range_count,
                             print_settings_.pepper_print_settings);
}

void OutOfProcessInstance::PrintEnd() {
  if (print_settings_.print_pages_called_)
    UserMetricsRecordAction("PDF.PrintPage");
  print_settings_.Clear();
  engine_->PrintEnd();
}

bool OutOfProcessInstance::IsPrintScalingDisabled() {
  return !engine_->GetPrintScaling();
}

bool OutOfProcessInstance::StartFind(const std::string& text,
                                     bool case_sensitive) {
  engine_->StartFind(text, case_sensitive);
  return true;
}

void OutOfProcessInstance::SelectFindResult(bool forward) {
  engine_->SelectFindResult(forward);
}

void OutOfProcessInstance::StopFind() {
  engine_->StopFind();
  tickmarks_.clear();
  SetTickmarks(tickmarks_);
}

void OutOfProcessInstance::OnPaint(
    const std::vector<pp::Rect>& paint_rects,
    std::vector<PaintManager::ReadyRect>* ready,
    std::vector<pp::Rect>* pending) {
  if (image_data_.is_null()) {
    DCHECK(plugin_size_.IsEmpty());
    return;
  }
  if (first_paint_) {
    first_paint_ = false;
    pp::Rect rect = pp::Rect(pp::Point(), image_data_.size());
    FillRect(rect, background_color_);
    ready->push_back(PaintManager::ReadyRect(rect, image_data_, true));
  }

  if (!received_viewport_message_)
    return;

  engine_->PrePaint();

  for (const auto& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    pp::Rect rect =
        paint_rect.Intersect(pp::Rect(pp::Point(), plugin_size_));
    if (rect.IsEmpty())
      continue;

    pp::Rect pdf_rect = available_area_.Intersect(rect);
    if (!pdf_rect.IsEmpty()) {
      pdf_rect.Offset(available_area_.x() * -1, 0);

      std::vector<pp::Rect> pdf_ready;
      std::vector<pp::Rect> pdf_pending;
      engine_->Paint(pdf_rect, &image_data_, &pdf_ready, &pdf_pending);
      for (auto& ready_rect : pdf_ready) {
        ready_rect.Offset(available_area_.point());
        ready->push_back(
            PaintManager::ReadyRect(ready_rect, image_data_, false));
      }
      for (auto& pending_rect : pdf_pending) {
        pending_rect.Offset(available_area_.point());
        pending->push_back(pending_rect);
      }
    }

    // Ensure the region above the first page (if any) is filled;
    int32_t first_page_ypos = engine_->GetNumberOfPages() == 0 ?
        0 : engine_->GetPageScreenRect(0).y();
    if (rect.y() < first_page_ypos) {
      pp::Rect region = rect.Intersect(pp::Rect(
          pp::Point(), pp::Size(plugin_size_.width(), first_page_ypos)));
      ready->push_back(PaintManager::ReadyRect(region, image_data_, false));
      FillRect(region, background_color_);
    }

    for (const auto& background_part : background_parts_) {
      pp::Rect intersection = background_part.location.Intersect(rect);
      if (!intersection.IsEmpty()) {
        FillRect(intersection, background_part.color);
        ready->push_back(
            PaintManager::ReadyRect(intersection, image_data_, false));
      }
    }
  }

  engine_->PostPaint();
}

void OutOfProcessInstance::DidOpen(int32_t result) {
  if (result == PP_OK) {
    if (!engine_->HandleDocumentLoad(embed_loader_)) {
      document_load_state_ = LOAD_STATE_LOADING;
      DocumentLoadFailed();
    }
  } else if (result != PP_ERROR_ABORTED) {  // Can happen in tests.
    NOTREACHED();
    DocumentLoadFailed();
  }

  // If it's a progressive load, cancel the stream URL request so that requests
  // can be made on the original URL.
  // TODO(raymes): Make this clearer once the in-process plugin is deleted.
  if (engine_->IsProgressiveLoad()) {
    pp::VarDictionary message;
    message.Set(kType, kJSCancelStreamUrlType);
    PostMessage(message);
  }
}

void OutOfProcessInstance::DidOpenPreview(int32_t result) {
  if (result == PP_OK) {
    preview_client_.reset(new PreviewModeClient(this));
    preview_engine_.reset(PDFEngine::Create(preview_client_.get()));
    preview_engine_->HandleDocumentLoad(embed_preview_loader_);
  } else {
    NOTREACHED();
  }
}

void OutOfProcessInstance::OnClientTimerFired(int32_t id) {
  engine_->OnCallback(id);
}

void OutOfProcessInstance::CalculateBackgroundParts() {
  background_parts_.clear();
  int left_width = available_area_.x();
  int right_start = available_area_.right();
  int right_width = abs(plugin_size_.width() - available_area_.right());
  int bottom = std::min(available_area_.bottom(), plugin_size_.height());

  // Add the left, right, and bottom rectangles.  Note: we assume only
  // horizontal centering.
  BackgroundPart part = {
    pp::Rect(0, 0, left_width, bottom),
    background_color_
  };
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
  part.location = pp::Rect(right_start, 0, right_width, bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
  part.location = pp::Rect(
      0, bottom, plugin_size_.width(), plugin_size_.height() - bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
}

int OutOfProcessInstance::GetDocumentPixelWidth() const {
  return static_cast<int>(ceil(document_size_.width() * zoom_ * device_scale_));
}

int OutOfProcessInstance::GetDocumentPixelHeight() const {
  return static_cast<int>(
      ceil(document_size_.height() * zoom_ * device_scale_));
}

void OutOfProcessInstance::FillRect(const pp::Rect& rect, uint32_t color) {
  DCHECK(!image_data_.is_null() || rect.IsEmpty());
  uint32_t* buffer_start = static_cast<uint32_t*>(image_data_.data());
  int stride = image_data_.stride();
  uint32_t* ptr = buffer_start + rect.y() * stride / 4 + rect.x();
  int height = rect.height();
  int width = rect.width();
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x)
      *(ptr + x) = color;
    ptr += stride /4;
  }
}

void OutOfProcessInstance::DocumentSizeUpdated(const pp::Size& size) {
  document_size_ = size;

  pp::VarDictionary dimensions;
  dimensions.Set(kType, kJSDocumentDimensionsType);
  dimensions.Set(kJSDocumentWidth, pp::Var(document_size_.width()));
  dimensions.Set(kJSDocumentHeight, pp::Var(document_size_.height()));
  pp::VarArray page_dimensions_array;
  int num_pages = engine_->GetNumberOfPages();
  for (int i = 0; i < num_pages; ++i) {
    pp::Rect page_rect = engine_->GetPageRect(i);
    pp::VarDictionary page_dimensions;
    page_dimensions.Set(kJSPageX, pp::Var(page_rect.x()));
    page_dimensions.Set(kJSPageY, pp::Var(page_rect.y()));
    page_dimensions.Set(kJSPageWidth, pp::Var(page_rect.width()));
    page_dimensions.Set(kJSPageHeight, pp::Var(page_rect.height()));
    page_dimensions_array.Set(i, page_dimensions);
  }
  dimensions.Set(kJSPageDimensions, page_dimensions_array);
  PostMessage(dimensions);

  OnGeometryChanged(zoom_, device_scale_);
}

void OutOfProcessInstance::Invalidate(const pp::Rect& rect) {
  pp::Rect offset_rect(rect);
  offset_rect.Offset(available_area_.point());
  paint_manager_.InvalidateRect(offset_rect);
}

void OutOfProcessInstance::Scroll(const pp::Point& point) {
  if (!image_data_.is_null())
    paint_manager_.ScrollRect(available_area_, point);
}

void OutOfProcessInstance::ScrollToX(int x) {
  pp::VarDictionary position;
  position.Set(kType, kJSSetScrollPositionType);
  position.Set(kJSPositionX, pp::Var(x / device_scale_));
  PostMessage(position);
}

void OutOfProcessInstance::ScrollToY(int y) {
  pp::VarDictionary position;
  position.Set(kType, kJSSetScrollPositionType);
  position.Set(kJSPositionY, pp::Var(y / device_scale_));
  PostMessage(position);
}

void OutOfProcessInstance::ScrollToPage(int page) {
  if (engine_->GetNumberOfPages() == 0)
    return;

  pp::VarDictionary message;
  message.Set(kType, kJSGoToPageType);
  message.Set(kJSPageNumber, pp::Var(page));
  PostMessage(message);
}

void OutOfProcessInstance::NavigateTo(const std::string& url,
                                      bool open_in_new_tab) {
  pp::VarDictionary message;
  message.Set(kType, kJSNavigateType);
  message.Set(kJSNavigateUrl, url);
  message.Set(kJSNavigateNewTab, open_in_new_tab);
  PostMessage(message);
}

void OutOfProcessInstance::UpdateCursor(PP_CursorType_Dev cursor) {
  if (cursor == cursor_)
    return;
  cursor_ = cursor;

  const PPB_CursorControl_Dev* cursor_interface =
      reinterpret_cast<const PPB_CursorControl_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CURSOR_CONTROL_DEV_INTERFACE));
  if (!cursor_interface) {
    NOTREACHED();
    return;
  }

  cursor_interface->SetCursor(
      pp_instance(), cursor_, pp::ImageData().pp_resource(), nullptr);
}

void OutOfProcessInstance::UpdateTickMarks(
    const std::vector<pp::Rect>& tickmarks) {
  float inverse_scale = 1.0f / device_scale_;
  std::vector<pp::Rect> scaled_tickmarks = tickmarks;
  for (auto& tickmark : scaled_tickmarks)
    ScaleRect(inverse_scale, &tickmark);
  tickmarks_ = scaled_tickmarks;
}

void OutOfProcessInstance::NotifyNumberOfFindResultsChanged(int total,
                                                            bool final_result) {
  // We don't want to spam the renderer with too many updates to the number of
  // find results. Don't send an update if we sent one too recently. If it's the
  // final update, we always send it though.
  if (final_result) {
    NumberOfFindResultsChanged(total, final_result);
    SetTickmarks(tickmarks_);
    return;
  }

  if (recently_sent_find_update_)
    return;

  NumberOfFindResultsChanged(total, final_result);
  SetTickmarks(tickmarks_);
  recently_sent_find_update_ = true;
  pp::CompletionCallback callback =
      timer_factory_.NewCallback(
          &OutOfProcessInstance::ResetRecentlySentFindUpdate);
  pp::Module::Get()->core()->CallOnMainThread(kFindResultCooldownMs,
                                              callback, 0);
}

void OutOfProcessInstance::NotifySelectedFindResultChanged(
    int current_find_index) {
  DCHECK_GE(current_find_index, 0);
  SelectedFindResultChanged(current_find_index);
}

void OutOfProcessInstance::GetDocumentPassword(
    pp::CompletionCallbackWithOutput<pp::Var> callback) {
  if (password_callback_) {
    NOTREACHED();
    return;
  }

  password_callback_.reset(
      new pp::CompletionCallbackWithOutput<pp::Var>(callback));
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSGetPasswordType));
  PostMessage(message);
}

void OutOfProcessInstance::Alert(const std::string& message) {
  ModalDialog(this, "alert", message, std::string());
}

bool OutOfProcessInstance::Confirm(const std::string& message) {
  pp::Var result = ModalDialog(this, "confirm", message, std::string());
  return result.is_bool() ? result.AsBool() : false;
}

std::string OutOfProcessInstance::Prompt(const std::string& question,
                                         const std::string& default_answer) {
  pp::Var result = ModalDialog(this, "prompt", question, default_answer);
  return result.is_string() ? result.AsString() : std::string();
}

std::string OutOfProcessInstance::GetURL() {
  return url_;
}

void OutOfProcessInstance::Email(const std::string& to,
                                 const std::string& cc,
                                 const std::string& bcc,
                                 const std::string& subject,
                                 const std::string& body) {
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSEmailType));
  message.Set(pp::Var(kJSEmailTo),
              pp::Var(net::EscapeUrlEncodedData(to, false)));
  message.Set(pp::Var(kJSEmailCc),
              pp::Var(net::EscapeUrlEncodedData(cc, false)));
  message.Set(pp::Var(kJSEmailBcc),
              pp::Var(net::EscapeUrlEncodedData(bcc, false)));
  message.Set(pp::Var(kJSEmailSubject),
              pp::Var(net::EscapeUrlEncodedData(subject, false)));
  message.Set(pp::Var(kJSEmailBody),
              pp::Var(net::EscapeUrlEncodedData(body, false)));
  PostMessage(message);
}

void OutOfProcessInstance::Print() {
  if (!engine_->HasPermission(PDFEngine::PERMISSION_PRINT_LOW_QUALITY) &&
      !engine_->HasPermission(PDFEngine::PERMISSION_PRINT_HIGH_QUALITY)) {
    return;
  }

  pp::CompletionCallback callback =
      print_callback_factory_.NewCallback(&OutOfProcessInstance::OnPrint);
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
}

void OutOfProcessInstance::OnPrint(int32_t) {
  pp::PDF::Print(this);
}

void OutOfProcessInstance::SubmitForm(const std::string& url,
                                      const void* data,
                                      int length) {
  pp::URLRequestInfo request(this);
  request.SetURL(url);
  request.SetMethod("POST");
  request.AppendDataToBody(reinterpret_cast<const char*>(data), length);

  pp::CompletionCallback callback =
      form_factory_.NewCallback(&OutOfProcessInstance::FormDidOpen);
  form_loader_ = CreateURLLoaderInternal();
  int rv = form_loader_.Open(request, callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    callback.Run(rv);
}

void OutOfProcessInstance::FormDidOpen(int32_t result) {
  // TODO: inform the user of success/failure.
  if (result != PP_OK) {
    NOTREACHED();
  }
}

std::string OutOfProcessInstance::ShowFileSelectionDialog() {
  // Seems like very low priority to implement, since the pdf has no way to get
  // the file data anyways.  Javascript doesn't let you do this synchronously.
  NOTREACHED();
  return std::string();
}

pp::URLLoader OutOfProcessInstance::CreateURLLoader() {
  if (full_) {
    if (!did_call_start_loading_) {
      did_call_start_loading_ = true;
      pp::PDF::DidStartLoading(this);
    }

    // Disable save and print until the document is fully loaded, since they
    // would generate an incomplete document.  Need to do this each time we
    // call DidStartLoading since that resets the content restrictions.
    pp::PDF::SetContentRestriction(this, CONTENT_RESTRICTION_SAVE |
                                   CONTENT_RESTRICTION_PRINT);
  }

  return CreateURLLoaderInternal();
}

void OutOfProcessInstance::ScheduleCallback(int id, int delay_in_ms) {
  pp::CompletionCallback callback =
      timer_factory_.NewCallback(&OutOfProcessInstance::OnClientTimerFired);
  pp::Module::Get()->core()->CallOnMainThread(delay_in_ms, callback, id);
}

void OutOfProcessInstance::SearchString(const base::char16* string,
                            const base::char16* term,
                            bool case_sensitive,
                            std::vector<SearchStringResult>* results) {
  PP_PrivateFindResult* pp_results;
  int count = 0;
  pp::PDF::SearchString(
      this,
      reinterpret_cast<const unsigned short*>(string),
      reinterpret_cast<const unsigned short*>(term),
      case_sensitive,
      &pp_results,
      &count);

  results->resize(count);
  for (int i = 0; i < count; ++i) {
    (*results)[i].start_index = pp_results[i].start_index;
    (*results)[i].length = pp_results[i].length;
  }

  pp::Memory_Dev memory;
  memory.MemFree(pp_results);
}

void OutOfProcessInstance::DocumentPaintOccurred() {
}

void OutOfProcessInstance::DocumentLoadComplete(int page_count) {
  // Clear focus state for OSK.
  FormTextFieldFocusChange(false);

  DCHECK(document_load_state_ == LOAD_STATE_LOADING);
  document_load_state_ = LOAD_STATE_COMPLETE;
  UserMetricsRecordAction("PDF.LoadSuccess");
  uma_.HistogramEnumeration("PDF.DocumentFeature", LOADED_DOCUMENT,
                            FEATURES_COUNT);

  // Note: If we are in print preview mode the scroll location is retained
  // across document loads so we don't want to scroll again and override it.
  if (IsPrintPreview()) {
    AppendBlankPrintPreviewPages();
    OnGeometryChanged(0, 0);
  }

  pp::VarDictionary metadata_message;
  metadata_message.Set(pp::Var(kType), pp::Var(kJSMetadataType));
  std::string title = engine_->GetMetadata("Title");
  if (!base::TrimWhitespace(base::UTF8ToUTF16(title), base::TRIM_ALL).empty()) {
    metadata_message.Set(pp::Var(kJSTitle), pp::Var(title));
    uma_.HistogramEnumeration("PDF.DocumentFeature", HAS_TITLE, FEATURES_COUNT);
  }

  pp::VarArray bookmarks = engine_->GetBookmarks();
  metadata_message.Set(pp::Var(kJSBookmarks), bookmarks);
  if (bookmarks.GetLength() > 0) {
    uma_.HistogramEnumeration("PDF.DocumentFeature", HAS_BOOKMARKS,
                              FEATURES_COUNT);
  }
  PostMessage(metadata_message);

  pp::VarDictionary progress_message;
  progress_message.Set(pp::Var(kType), pp::Var(kJSLoadProgressType));
  progress_message.Set(pp::Var(kJSProgressPercentage), pp::Var(100));
  PostMessage(progress_message);

  if (!full_)
    return;

  if (did_call_start_loading_) {
    pp::PDF::DidStopLoading(this);
    did_call_start_loading_ = false;
  }

  int content_restrictions =
      CONTENT_RESTRICTION_CUT | CONTENT_RESTRICTION_PASTE;
  if (!engine_->HasPermission(PDFEngine::PERMISSION_COPY))
    content_restrictions |= CONTENT_RESTRICTION_COPY;

  if (!engine_->HasPermission(PDFEngine::PERMISSION_PRINT_LOW_QUALITY) &&
      !engine_->HasPermission(PDFEngine::PERMISSION_PRINT_HIGH_QUALITY)) {
    content_restrictions |= CONTENT_RESTRICTION_PRINT;
  }

  pp::PDF::SetContentRestriction(this, content_restrictions);

  uma_.HistogramCustomCounts("PDF.PageCount", page_count, 1, 1000000, 50);

  if (accessibility_state_ == ACCESSIBILITY_STATE_PENDING)
    LoadAccessibility();
}

void OutOfProcessInstance::RotateClockwise() {
  engine_->RotateClockwise();
}

void OutOfProcessInstance::RotateCounterclockwise() {
  engine_->RotateCounterclockwise();
}

void OutOfProcessInstance::PreviewDocumentLoadComplete() {
  if (preview_document_load_state_ != LOAD_STATE_LOADING ||
      preview_pages_info_.empty()) {
    return;
  }

  preview_document_load_state_ = LOAD_STATE_COMPLETE;

  int dest_page_index = preview_pages_info_.front().second;
  int src_page_index =
      ExtractPrintPreviewPageIndex(preview_pages_info_.front().first);
  if (src_page_index > 0 &&  dest_page_index > -1 && preview_engine_.get())
    engine_->AppendPage(preview_engine_.get(), dest_page_index);

  preview_pages_info_.pop();
  // |print_preview_page_count_| is not updated yet. Do not load any
  // other preview pages till we get this information.
  if (print_preview_page_count_ == 0)
    return;

  if (!preview_pages_info_.empty())
    LoadAvailablePreviewPage();
}

void OutOfProcessInstance::DocumentLoadFailed() {
  DCHECK(document_load_state_ == LOAD_STATE_LOADING);
  UserMetricsRecordAction("PDF.LoadFailure");

  if (did_call_start_loading_) {
    pp::PDF::DidStopLoading(this);
    did_call_start_loading_ = false;
  }

  document_load_state_ = LOAD_STATE_FAILED;
  paint_manager_.InvalidateRect(pp::Rect(pp::Point(), plugin_size_));

  // Send a progress value of -1 to indicate a failure.
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSLoadProgressType));
  message.Set(pp::Var(kJSProgressPercentage), pp::Var(-1));
  PostMessage(message);
}

void OutOfProcessInstance::PreviewDocumentLoadFailed() {
  UserMetricsRecordAction("PDF.PreviewDocumentLoadFailure");
  if (preview_document_load_state_ != LOAD_STATE_LOADING ||
      preview_pages_info_.empty()) {
    return;
  }

  preview_document_load_state_ = LOAD_STATE_FAILED;
  preview_pages_info_.pop();

  if (!preview_pages_info_.empty())
    LoadAvailablePreviewPage();
}

pp::Instance* OutOfProcessInstance::GetPluginInstance() {
  return this;
}

void OutOfProcessInstance::DocumentHasUnsupportedFeature(
    const std::string& feature) {
  std::string metric("PDF_Unsupported_");
  metric += feature;
  if (!unsupported_features_reported_.count(metric)) {
    unsupported_features_reported_.insert(metric);
    UserMetricsRecordAction(metric);
  }

  // Since we use an info bar, only do this for full frame plugins..
  if (!full_)
    return;

  if (told_browser_about_unsupported_feature_)
    return;
  told_browser_about_unsupported_feature_ = true;

  pp::PDF::HasUnsupportedFeature(this);
}

void OutOfProcessInstance::DocumentLoadProgress(uint32_t available,
                                                uint32_t doc_size) {
  double progress = 0.0;
  if (doc_size == 0) {
    // Document size is unknown. Use heuristics.
    // We'll make progress logarithmic from 0 to 100M.
    static const double kFactor = log(100000000.0) / 100.0;
    if (available > 0) {
      progress = log(static_cast<double>(available)) / kFactor;
      if (progress > 100.0)
        progress = 100.0;
    }
  } else {
    progress = 100.0 * static_cast<double>(available) / doc_size;
  }

  // We send 100% load progress in DocumentLoadComplete.
  if (progress >= 100)
    return;

  // Avoid sending too many progress messages over PostMessage.
  if (progress > last_progress_sent_ + 1) {
    last_progress_sent_ = progress;
    pp::VarDictionary message;
    message.Set(pp::Var(kType), pp::Var(kJSLoadProgressType));
    message.Set(pp::Var(kJSProgressPercentage), pp::Var(progress));
    PostMessage(message);
  }
}

void OutOfProcessInstance::FormTextFieldFocusChange(bool in_focus) {
  if (!text_input_.get())
    return;

  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSFieldFocusType));
  message.Set(pp::Var(kJSFieldFocus), pp::Var(in_focus));
  PostMessage(message);

  if (in_focus)
    text_input_->SetTextInputType(PP_TEXTINPUT_TYPE_DEV_TEXT);
  else
    text_input_->SetTextInputType(PP_TEXTINPUT_TYPE_DEV_NONE);
}

void OutOfProcessInstance::ResetRecentlySentFindUpdate(int32_t /* unused */) {
  recently_sent_find_update_ = false;
}

void OutOfProcessInstance::OnGeometryChanged(double old_zoom,
                                             float old_device_scale) {
  if (zoom_ != old_zoom || device_scale_ != old_device_scale)
    engine_->ZoomUpdated(zoom_ * device_scale_);

  available_area_ = pp::Rect(plugin_size_);
  int doc_width = GetDocumentPixelWidth();
  if (doc_width < available_area_.width()) {
    available_area_.Offset((available_area_.width() - doc_width) / 2, 0);
    available_area_.set_width(doc_width);
  }
  int bottom_of_document =
      GetDocumentPixelHeight() + (top_toolbar_height_ * device_scale_);
  if (bottom_of_document < available_area_.height())
    available_area_.set_height(bottom_of_document);

  CalculateBackgroundParts();
  engine_->PageOffsetUpdated(available_area_.point());
  engine_->PluginSizeUpdated(available_area_.size());

  if (document_size_.IsEmpty())
    return;
  paint_manager_.InvalidateRect(pp::Rect(pp::Point(), plugin_size_));
}

void OutOfProcessInstance::LoadUrl(const std::string& url) {
  LoadUrlInternal(url, &embed_loader_, &OutOfProcessInstance::DidOpen);
}

void OutOfProcessInstance::LoadPreviewUrl(const std::string& url) {
  LoadUrlInternal(url, &embed_preview_loader_,
                  &OutOfProcessInstance::DidOpenPreview);
}

void OutOfProcessInstance::LoadUrlInternal(
    const std::string& url,
    pp::URLLoader* loader,
    void (OutOfProcessInstance::* method)(int32_t)) {
  pp::URLRequestInfo request(this);
  request.SetURL(url);
  request.SetMethod("GET");

  *loader = CreateURLLoaderInternal();
  pp::CompletionCallback callback = loader_factory_.NewCallback(method);
  int rv = loader->Open(request, callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    callback.Run(rv);
}

pp::URLLoader OutOfProcessInstance::CreateURLLoaderInternal() {
  pp::URLLoader loader(this);

  const PPB_URLLoaderTrusted* trusted_interface =
      reinterpret_cast<const PPB_URLLoaderTrusted*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_URLLOADERTRUSTED_INTERFACE));
  if (trusted_interface)
    trusted_interface->GrantUniversalAccess(loader.pp_resource());
  return loader;
}

void OutOfProcessInstance::SetZoom(double scale) {
  double old_zoom = zoom_;
  zoom_ = scale;
  OnGeometryChanged(old_zoom, device_scale_);
}

void OutOfProcessInstance::AppendBlankPrintPreviewPages() {
  if (print_preview_page_count_ == 0)
    return;
  engine_->AppendBlankPages(print_preview_page_count_);
  if (!preview_pages_info_.empty())
    LoadAvailablePreviewPage();
}

bool OutOfProcessInstance::IsPrintPreview() {
  return IsPrintPreviewUrl(url_);
}

uint32_t OutOfProcessInstance::GetBackgroundColor() {
  return background_color_;
}

void OutOfProcessInstance::IsSelectingChanged(bool is_selecting) {
  pp::VarDictionary message;
  message.Set(kType, kJSSetIsSelectingType);
  message.Set(kJSIsSelecting, pp::Var(is_selecting));
  PostMessage(message);
}

void OutOfProcessInstance::ProcessPreviewPageInfo(const std::string& url,
                                                  int dst_page_index) {
  if (!IsPrintPreview())
    return;

  int src_page_index = ExtractPrintPreviewPageIndex(url);
  if (src_page_index < 1)
    return;

  preview_pages_info_.push(std::make_pair(url, dst_page_index));
  LoadAvailablePreviewPage();
}

void OutOfProcessInstance::LoadAvailablePreviewPage() {
  if (preview_pages_info_.empty() ||
      document_load_state_ != LOAD_STATE_COMPLETE) {
    return;
  }

  std::string url = preview_pages_info_.front().first;
  int dst_page_index = preview_pages_info_.front().second;
  int src_page_index = ExtractPrintPreviewPageIndex(url);
  if (src_page_index < 1 ||
      dst_page_index >= print_preview_page_count_ ||
      preview_document_load_state_ == LOAD_STATE_LOADING) {
    return;
  }

  preview_document_load_state_ = LOAD_STATE_LOADING;
  LoadPreviewUrl(url);
}

void OutOfProcessInstance::UserMetricsRecordAction(
    const std::string& action) {
  // TODO(raymes): Move this function to PPB_UMA_Private.
  pp::PDF::UserMetricsRecordAction(this, pp::Var(action));
}

pp::FloatPoint OutOfProcessInstance::BoundScrollOffsetToDocument(
    const pp::FloatPoint& scroll_offset) {
  float max_x = document_size_.width() * zoom_ - plugin_dip_size_.width();
  float x = std::max(std::min(scroll_offset.x(), max_x), 0.0f);
  float min_y = -top_toolbar_height_;
  float max_y = document_size_.height() * zoom_ - plugin_dip_size_.height();
  float y = std::max(std::min(scroll_offset.y(), max_y), min_y);
  return pp::FloatPoint(x, y);
}

}  // namespace chrome_pdf

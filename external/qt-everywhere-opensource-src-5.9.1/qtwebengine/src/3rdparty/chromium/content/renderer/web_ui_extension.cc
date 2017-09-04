// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_extension.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "content/common/view_messages.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/web_ui_extension_data.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace content {

namespace {

bool ShouldRespondToRequest(
    blink::WebFrame** frame_ptr,
    RenderView** render_view_ptr) {
  blink::WebFrame* frame = blink::WebLocalFrame::frameForCurrentContext();
  if (!frame || !frame->view())
    return false;

  RenderView* render_view = RenderView::FromWebView(frame->view());
  if (!render_view)
    return false;

  GURL frame_url = frame->document().url();

  bool webui_enabled =
      (render_view->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI) &&
      (frame_url.SchemeIs(kChromeUIScheme) ||
       frame_url.SchemeIs(url::kDataScheme));

  if (!webui_enabled)
    return false;

  *frame_ptr = frame;
  *render_view_ptr = render_view;
  return true;
}

}  // namespace

// Exposes two methods:
//  - chrome.send: Used to send messages to the browser. Requires the message
//      name as the first argument and can have an optional second argument that
//      should be an array.
//  - chrome.getVariableValue: Returns value for the input variable name if such
//      a value was set by the browser. Else will return an empty string.
void WebUIExtension::Install(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate,
                                                          context->Global());
  chrome->Set(gin::StringToSymbol(isolate, "send"),
              gin::CreateFunctionTemplate(
                  isolate, base::Bind(&WebUIExtension::Send))->GetFunction());
  chrome->Set(gin::StringToSymbol(isolate, "getVariableValue"),
              gin::CreateFunctionTemplate(
                  isolate, base::Bind(&WebUIExtension::GetVariableValue))
                  ->GetFunction());
}

// static
void WebUIExtension::Send(gin::Arguments* args) {
  blink::WebFrame* frame;
  RenderView* render_view;
  if (!ShouldRespondToRequest(&frame, &render_view))
    return;

  std::string message;
  if (!args->GetNext(&message)) {
    args->ThrowError();
    return;
  }

  // If they've provided an optional message parameter, convert that into a
  // Value to send to the browser process.
  std::unique_ptr<base::ListValue> content;
  if (args->PeekNext().IsEmpty() || args->PeekNext()->IsUndefined()) {
    content.reset(new base::ListValue());
  } else {
    v8::Local<v8::Object> obj;
    if (!args->GetNext(&obj)) {
      args->ThrowError();
      return;
    }

    std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    content = base::ListValue::From(
        converter->FromV8Value(obj, frame->mainWorldScriptContext()));
    DCHECK(content);
  }

  // Send the message up to the browser.
  render_view->Send(new ViewHostMsg_WebUISend(render_view->GetRoutingID(),
                                              frame->document().url(),
                                              message,
                                              *content));
}

// static
std::string WebUIExtension::GetVariableValue(const std::string& name) {
  blink::WebFrame* frame;
  RenderView* render_view;
  if (!ShouldRespondToRequest(&frame, &render_view))
    return std::string();

  return WebUIExtensionData::Get(render_view)->GetValue(name);
}

}  // namespace content

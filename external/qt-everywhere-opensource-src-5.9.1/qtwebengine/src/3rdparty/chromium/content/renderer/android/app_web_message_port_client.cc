// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/app_web_message_port_client.h"

#include <memory>
#include <utility>

#include "content/common/app_web_message_port_messages.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using blink::WebSerializedScriptValue;
using content::V8ValueConverter;
using std::vector;

namespace content {

AppWebMessagePortClient::AppWebMessagePortClient(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

AppWebMessagePortClient::~AppWebMessagePortClient() {}

bool AppWebMessagePortClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppWebMessagePortClient, message)
    IPC_MESSAGE_HANDLER(AppWebMessagePortMsg_WebToAppMessage, OnWebToAppMessage)
    IPC_MESSAGE_HANDLER(AppWebMessagePortMsg_AppToWebMessage, OnAppToWebMessage)
    IPC_MESSAGE_HANDLER(AppWebMessagePortMsg_ClosePort, OnClosePort)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AppWebMessagePortClient::OnDestruct() {
  delete this;
}

void AppWebMessagePortClient::OnWebToAppMessage(
    int message_port_id,
    const base::string16& message,
    const vector<int>& sent_message_port_ids) {
  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  blink::WebFrame* main_frame =
      render_frame()->GetRenderView()->GetWebView()->mainFrame();
  if (main_frame == nullptr) {
    return;
  }
  v8::Local<v8::Context> context = main_frame->mainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  DCHECK(!context.IsEmpty());
  WebSerializedScriptValue v = WebSerializedScriptValue::fromString(message);
  v8::Local<v8::Value> v8value = v.deserialize();

  std::unique_ptr<V8ValueConverter> converter;
  converter.reset(V8ValueConverter::create());
  converter->SetDateAllowed(true);
  converter->SetRegExpAllowed(true);
  base::ListValue result;
  std::unique_ptr<base::Value> value = converter->FromV8Value(v8value, context);
  if (value) {
    result.Append(std::move(value));
  }

  Send(new AppWebMessagePortHostMsg_ConvertedWebToAppMessage(
      render_frame()->GetRoutingID(), message_port_id, result,
      sent_message_port_ids));
}

void AppWebMessagePortClient::OnAppToWebMessage(
    int message_port_id,
    const base::string16& message,
    const vector<int>& sent_message_port_ids) {
  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  blink::WebFrame* main_frame =
      render_frame()->GetRenderView()->GetWebView()->mainFrame();
  if (main_frame == nullptr) {
    return;
  }
  v8::Local<v8::Context> context = main_frame->mainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  DCHECK(!context.IsEmpty());
  std::unique_ptr<V8ValueConverter> converter;
  converter.reset(V8ValueConverter::create());
  converter->SetDateAllowed(true);
  converter->SetRegExpAllowed(true);
  std::unique_ptr<base::Value> value(new base::StringValue(message));
  v8::Local<v8::Value> result_value =
      converter->ToV8Value(value.get(), context);
  WebSerializedScriptValue serialized_script_value =
      WebSerializedScriptValue::serialize(result_value);
  base::string16 result = serialized_script_value.toString();
  Send(new AppWebMessagePortHostMsg_ConvertedAppToWebMessage(
      render_frame()->GetRoutingID(), message_port_id, result,
      sent_message_port_ids));
}

void AppWebMessagePortClient::OnClosePort(int message_port_id) {
  Send(new AppWebMessagePortHostMsg_ClosePortAck(render_frame()->GetRoutingID(),
                                                 message_port_id));
}
}

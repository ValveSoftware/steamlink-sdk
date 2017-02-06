// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/app_window_custom_bindings.h"

#include "base/command_line.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/script_context.h"
#include "grit/extensions_renderer_resources.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

namespace extensions {

AppWindowCustomBindings::AppWindowCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("GetFrame", base::Bind(&AppWindowCustomBindings::GetFrame,
                                       base::Unretained(this)));

  RouteFunction("GetWindowControlsHtmlTemplate",
      base::Bind(&AppWindowCustomBindings::GetWindowControlsHtmlTemplate,
                 base::Unretained(this)));
}

void AppWindowCustomBindings::GetFrame(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(jeremya): convert this to IDL nocompile to get validation, and turn
  // these argument checks into CHECK().
  if (args.Length() != 2)
    return;

  if (!args[0]->IsInt32() || !args[1]->IsBoolean())
    return;

  int frame_id = args[0]->Int32Value();
  bool notify_browser = args[1]->BooleanValue();

  if (frame_id == MSG_ROUTING_NONE)
    return;

  content::RenderFrame* app_frame =
      content::RenderFrame::FromRoutingID(frame_id);
  if (!app_frame)
    return;

  if (notify_browser) {
    content::RenderThread::Get()->Send(new ExtensionHostMsg_AppWindowReady(
        app_frame->GetRenderView()->GetRoutingID()));
  }

  v8::Local<v8::Value> window =
      app_frame->GetWebFrame()->mainWorldScriptContext()->Global();
  args.GetReturnValue().Set(window);
}

void AppWindowCustomBindings::GetWindowControlsHtmlTemplate(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 0);

  v8::Local<v8::Value> result = v8::String::Empty(args.GetIsolate());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAppWindowControls)) {
    base::StringValue value(
        ResourceBundle::GetSharedInstance()
            .GetRawDataResource(IDR_WINDOW_CONTROLS_TEMPLATE_HTML)
            .as_string());
    std::unique_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    result = converter->ToV8Value(&value, context()->v8_context());
  }
  args.GetReturnValue().Set(result);
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/runtime_custom_bindings.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace extensions {

RuntimeCustomBindings::RuntimeCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetManifest",
      base::Bind(&RuntimeCustomBindings::GetManifest, base::Unretained(this)));
  RouteFunction("GetExtensionViews",
                base::Bind(&RuntimeCustomBindings::GetExtensionViews,
                           base::Unretained(this)));
}

RuntimeCustomBindings::~RuntimeCustomBindings() {
}

void RuntimeCustomBindings::GetManifest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(context()->extension());

  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  args.GetReturnValue().Set(converter->ToV8Value(
      context()->extension()->manifest()->value(), context()->v8_context()));
}

void RuntimeCustomBindings::GetExtensionViews(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsString());

  // |browser_window_id| == extension_misc::kUnknownWindowId means getting
  // all views for the current extension.
  int browser_window_id = args[0]->Int32Value();
  int tab_id = args[1]->Int32Value();

  std::string view_type_string =
      base::ToUpperASCII(*v8::String::Utf8Value(args[2]));
  // |view_type| == VIEW_TYPE_INVALID means getting any type of
  // views.
  ViewType view_type = VIEW_TYPE_INVALID;
  if (view_type_string == kViewTypeBackgroundPage) {
    view_type = VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
  } else if (view_type_string == kViewTypeTabContents) {
    view_type = VIEW_TYPE_TAB_CONTENTS;
  } else if (view_type_string == kViewTypePopup) {
    view_type = VIEW_TYPE_EXTENSION_POPUP;
  } else if (view_type_string == kViewTypeExtensionDialog) {
    view_type = VIEW_TYPE_EXTENSION_DIALOG;
  } else if (view_type_string == kViewTypeAppWindow) {
    view_type = VIEW_TYPE_APP_WINDOW;
  } else if (view_type_string == kViewTypeLauncherPage) {
    view_type = VIEW_TYPE_LAUNCHER_PAGE;
  } else if (view_type_string == kViewTypePanel) {
    view_type = VIEW_TYPE_PANEL;
  } else {
    CHECK_EQ(view_type_string, kViewTypeAll);
  }

  const std::string& extension_id = context()->GetExtensionID();
  if (extension_id.empty())
    return;

  std::vector<content::RenderFrame*> frames =
      ExtensionFrameHelper::GetExtensionFrames(extension_id, browser_window_id,
                                               tab_id, view_type);
  v8::Local<v8::Context> v8_context = args.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Array> v8_views = v8::Array::New(args.GetIsolate());
  int v8_index = 0;
  for (content::RenderFrame* frame : frames) {
    // We filter out iframes here. GetExtensionViews should only return the
    // main views, not any subframes. (Returning subframes can cause broken
    // behavior by treating an app window's iframe as its main frame, and maybe
    // other nastiness).
    blink::WebFrame* web_frame = frame->GetWebFrame();
    if (web_frame->top() != web_frame)
      continue;

    if (!blink::WebFrame::scriptCanAccess(web_frame))
      continue;

    v8::Local<v8::Context> context = web_frame->mainWorldScriptContext();
    if (!context.IsEmpty()) {
      v8::Local<v8::Value> window = context->Global();
      CHECK(!window.IsEmpty());
      v8::Maybe<bool> maybe =
          v8_views->CreateDataProperty(v8_context, v8_index++, window);
      CHECK(maybe.IsJust() && maybe.FromJust());
    }
  }

  args.GetReturnValue().Set(v8_views);
}

}  // namespace extensions

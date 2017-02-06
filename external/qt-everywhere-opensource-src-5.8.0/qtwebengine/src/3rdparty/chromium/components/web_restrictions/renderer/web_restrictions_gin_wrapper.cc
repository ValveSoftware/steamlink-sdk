// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_restrictions/renderer/web_restrictions_gin_wrapper.h"

#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace web_restrictions {

gin::WrapperInfo WebRestrictionsGinWrapper::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void WebRestrictionsGinWrapper::Install(content::RenderFrame* render_frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->mainWorldScriptContext();
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);
  gin::Handle<WebRestrictionsGinWrapper> controller =
      gin::CreateHandle(isolate, new WebRestrictionsGinWrapper(render_frame));
  if (controller.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "webRestriction"), controller.ToV8());
}

WebRestrictionsGinWrapper::WebRestrictionsGinWrapper(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

WebRestrictionsGinWrapper::~WebRestrictionsGinWrapper() {}

bool WebRestrictionsGinWrapper::RequestPermission() {
  if (!render_frame())
    return false;
  render_frame()->GetWebFrame()->reload();
  return true;
}

void WebRestrictionsGinWrapper::OnDestruct() {
  // Do nothing. Overrides version that deletes RenderFrameObserver.
}

gin::ObjectTemplateBuilder WebRestrictionsGinWrapper::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<WebRestrictionsGinWrapper>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("requestPermission",
                 &WebRestrictionsGinWrapper::RequestPermission);
}

}  // namespace web_restrictions

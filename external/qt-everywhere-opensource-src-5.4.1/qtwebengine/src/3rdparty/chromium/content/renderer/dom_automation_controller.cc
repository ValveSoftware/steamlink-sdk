// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_automation_controller.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "content/common/child_process_messages.h"
#include "content/common/frame_messages.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

gin::WrapperInfo DomAutomationController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void DomAutomationController::Install(RenderFrame* render_frame,
                                      blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<DomAutomationController> controller =
      gin::CreateHandle(isolate, new DomAutomationController(render_frame));
  if (controller.IsEmpty())
    return;

  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "domAutomationController"),
              controller.ToV8());
}

DomAutomationController::DomAutomationController(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), automation_id_(MSG_ROUTING_NONE) {}

DomAutomationController::~DomAutomationController() {}

gin::ObjectTemplateBuilder DomAutomationController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<DomAutomationController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("send", &DomAutomationController::SendMsg)
      .SetMethod("setAutomationId", &DomAutomationController::SetAutomationId)
      .SetMethod("sendJSON", &DomAutomationController::SendJSON)
      .SetMethod("sendWithId", &DomAutomationController::SendWithId);
}

void DomAutomationController::OnDestruct() {}

bool DomAutomationController::SendMsg(const gin::Arguments& args) {
  if (!render_frame())
    return false;

  if (automation_id_ == MSG_ROUTING_NONE)
    return false;

  std::string json;
  JSONStringValueSerializer serializer(&json);
  scoped_ptr<base::Value> value;

  // Warning: note that JSON officially requires the root-level object to be
  // an object (e.g. {foo:3}) or an array, while here we're serializing
  // strings, bools, etc. to "JSON".  This only works because (a) the JSON
  // writer is lenient, and (b) on the receiving side we wrap the JSON string
  // in square brackets, converting it to an array, then parsing it and
  // grabbing the 0th element to get the value out.
  if (args.PeekNext()->IsString() || args.PeekNext()->IsBoolean() ||
      args.PeekNext()->IsNumber()) {
    V8ValueConverterImpl conv;
    value.reset(
        conv.FromV8Value(args.PeekNext(), args.isolate()->GetCurrentContext()));
  } else {
    return false;
  }

  if (!serializer.Serialize(*value))
    return false;

  bool succeeded = Send(new FrameHostMsg_DomOperationResponse(
      routing_id(), json, automation_id_));

  automation_id_ = MSG_ROUTING_NONE;
  return succeeded;
}

bool DomAutomationController::SendJSON(const std::string& json) {
  if (!render_frame())
    return false;

  if (automation_id_ == MSG_ROUTING_NONE)
    return false;
  bool result = Send(new FrameHostMsg_DomOperationResponse(
      routing_id(), json, automation_id_));

  automation_id_ = MSG_ROUTING_NONE;
  return result;
}

bool DomAutomationController::SendWithId(int automation_id,
                                         const std::string& str) {
  if (!render_frame())
    return false;
  return Send(
      new FrameHostMsg_DomOperationResponse(routing_id(), str, automation_id));
}

bool DomAutomationController::SetAutomationId(int automation_id) {
  automation_id_ = automation_id;
  return true;
}

}  // namespace content

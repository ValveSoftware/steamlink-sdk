// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/activity_log_converter_strategy.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/script_context.h"

using content::V8ValueConverter;

namespace extensions {

APIActivityLogger::APIActivityLogger(ScriptContext* context,
                                     Dispatcher* dispatcher)
    : ObjectBackedNativeHandler(context), dispatcher_(dispatcher) {
  RouteFunction("LogEvent", base::Bind(&APIActivityLogger::LogEvent,
                                       base::Unretained(this)));
  RouteFunction("LogAPICall", base::Bind(&APIActivityLogger::LogAPICall,
                                         base::Unretained(this)));
}

APIActivityLogger::~APIActivityLogger() {}

// static
void APIActivityLogger::LogAPICall(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  LogInternal(APICALL, args);
}

// static
void APIActivityLogger::LogEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  LogInternal(EVENT, args);
}

// static
void APIActivityLogger::LogInternal(
    const CallType call_type,
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_GT(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsArray());

  if (!dispatcher_->activity_logging_enabled())
    return;

  std::string ext_id = *v8::String::Utf8Value(args[0]);
  ExtensionHostMsg_APIActionOrEvent_Params params;
  params.api_call = *v8::String::Utf8Value(args[1]);
  if (args.Length() == 4)  // Extras are optional.
    params.extra = *v8::String::Utf8Value(args[3]);
  else
    params.extra = "";

  // Get the array of api call arguments.
  v8::Local<v8::Array> arg_array = v8::Local<v8::Array>::Cast(args[2]);
  if (arg_array->Length() > 0) {
    std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    ActivityLogConverterStrategy strategy;
    converter->SetFunctionAllowed(true);
    converter->SetStrategy(&strategy);
    std::unique_ptr<base::ListValue> arg_list(new base::ListValue());
    for (size_t i = 0; i < arg_array->Length(); ++i) {
      arg_list->Set(
          i,
          converter->FromV8Value(arg_array->Get(i),
                                 args.GetIsolate()->GetCurrentContext()));
    }
    params.arguments.Swap(arg_list.get());
  }

  if (call_type == APICALL) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_AddAPIActionToActivityLog(ext_id, params));
  } else if (call_type == EVENT) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_AddEventToActivityLog(ext_id, params));
  }
}

}  // namespace extensions

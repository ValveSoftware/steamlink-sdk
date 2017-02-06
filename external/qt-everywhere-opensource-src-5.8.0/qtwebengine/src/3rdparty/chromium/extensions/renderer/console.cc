// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/console.h"

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/child/worker_thread.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"

namespace extensions {
namespace console {

using namespace v8_helpers;

namespace {

// Writes |message| to stack to show up in minidump, then crashes.
void CheckWithMinidump(const std::string& message) {
  char minidump[1024];
  base::debug::Alias(&minidump);
  base::snprintf(
      minidump, arraysize(minidump), "e::console: %s", message.c_str());
  CHECK(false) << message;
}

typedef void (*LogMethod)(content::RenderFrame* render_frame,
                          const std::string& message);

void BoundLogMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  std::string message;
  for (int i = 0; i < info.Length(); ++i) {
    if (i > 0)
      message += " ";
    message += *v8::String::Utf8Value(info[i]);
  }

  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  if (context.IsEmpty()) {
    LOG(WARNING) << "Could not log \"" << message << "\": no context given";
    return;
  }

  // A worker's ScriptContext neither lives in ScriptContextSet nor it has a
  // RenderFrame associated with it, so early exit in this case.
  // TODO(lazyboy): Fix.
  if (content::WorkerThread::GetCurrentId() > 0)
    return;

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  LogMethod log_method =
      reinterpret_cast<LogMethod>(info.Data().As<v8::External>()->Value());
  (*log_method)(script_context ? script_context->GetRenderFrame() : nullptr,
                message);
}

void BindLogMethod(v8::Isolate* isolate,
                   v8::Local<v8::Object> target,
                   const std::string& name,
                   LogMethod log_method) {
  v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
      isolate,
      &BoundLogMethodCallback,
      v8::External::New(isolate, reinterpret_cast<void*>(log_method)));
  tmpl->RemovePrototype();
  v8::Local<v8::Function> function;
  if (!tmpl->GetFunction(isolate->GetCurrentContext()).ToLocal(&function)) {
    LOG(FATAL) << "Could not create log function \"" << name << "\"";
    return;
  }
  v8::Local<v8::String> v8_name = ToV8StringUnsafe(isolate, name);
  if (!SetProperty(isolate->GetCurrentContext(), target, v8_name, function)) {
    LOG(WARNING) << "Could not bind log method \"" << name << "\"";
  }
  SetProperty(isolate->GetCurrentContext(), target, v8_name,
              tmpl->GetFunction());
}

}  // namespace

void Debug(content::RenderFrame* render_frame, const std::string& message) {
  AddMessage(render_frame, content::CONSOLE_MESSAGE_LEVEL_DEBUG, message);
}

void Log(content::RenderFrame* render_frame, const std::string& message) {
  AddMessage(render_frame, content::CONSOLE_MESSAGE_LEVEL_LOG, message);
}

void Warn(content::RenderFrame* render_frame, const std::string& message) {
  AddMessage(render_frame, content::CONSOLE_MESSAGE_LEVEL_WARNING, message);
}

void Error(content::RenderFrame* render_frame, const std::string& message) {
  AddMessage(render_frame, content::CONSOLE_MESSAGE_LEVEL_ERROR, message);
}

void Fatal(content::RenderFrame* render_frame, const std::string& message) {
  Error(render_frame, message);
  CheckWithMinidump(message);
}

void AddMessage(content::RenderFrame* render_frame,
                content::ConsoleMessageLevel level,
                const std::string& message) {
  if (!render_frame) {
    LOG(WARNING) << "Could not log \"" << message
                 << "\": no render frame found";
  } else {
    render_frame->AddMessageToConsole(level, message);
  }
}

v8::Local<v8::Object> AsV8Object(v8::Isolate* isolate) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Object> console_object = v8::Object::New(isolate);
  BindLogMethod(isolate, console_object, "debug", &Debug);
  BindLogMethod(isolate, console_object, "log", &Log);
  BindLogMethod(isolate, console_object, "warn", &Warn);
  BindLogMethod(isolate, console_object, "error", &Error);
  return handle_scope.Escape(console_object);
}

}  // namespace console
}  // namespace extensions

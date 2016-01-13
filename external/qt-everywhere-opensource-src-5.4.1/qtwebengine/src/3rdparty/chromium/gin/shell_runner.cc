// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/shell_runner.h"

#include "gin/converter.h"
#include "gin/modules/module_registry.h"
#include "gin/per_context_data.h"
#include "gin/public/context_holder.h"
#include "gin/try_catch.h"

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::ObjectTemplate;
using v8::Script;

namespace gin {

ShellRunnerDelegate::ShellRunnerDelegate() {
}

ShellRunnerDelegate::~ShellRunnerDelegate() {
}

v8::Handle<ObjectTemplate> ShellRunnerDelegate::GetGlobalTemplate(
    ShellRunner* runner,
    v8::Isolate* isolate) {
  return v8::Handle<ObjectTemplate>();
}

void ShellRunnerDelegate::DidCreateContext(ShellRunner* runner) {
}

void ShellRunnerDelegate::WillRunScript(ShellRunner* runner) {
}

void ShellRunnerDelegate::DidRunScript(ShellRunner* runner) {
}

void ShellRunnerDelegate::UnhandledException(ShellRunner* runner,
                                               TryCatch& try_catch) {
  CHECK(false) << try_catch.GetStackTrace();
}

ShellRunner::ShellRunner(ShellRunnerDelegate* delegate, Isolate* isolate)
    : delegate_(delegate) {
  v8::Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context =
      Context::New(isolate, NULL, delegate_->GetGlobalTemplate(this, isolate));

  context_holder_.reset(new ContextHolder(isolate));
  context_holder_->SetContext(context);
  PerContextData::From(context)->set_runner(this);

  v8::Context::Scope scope(context);
  delegate_->DidCreateContext(this);
}

ShellRunner::~ShellRunner() {
}

void ShellRunner::Run(const std::string& source,
                      const std::string& resource_name) {
  TryCatch try_catch;
  v8::Isolate* isolate = GetContextHolder()->isolate();
  v8::Handle<Script> script = Script::Compile(
      StringToV8(isolate, source), StringToV8(isolate, resource_name));
  if (try_catch.HasCaught()) {
    delegate_->UnhandledException(this, try_catch);
    return;
  }

  Run(script);
}

v8::Handle<v8::Value> ShellRunner::Call(v8::Handle<v8::Function> function,
                                        v8::Handle<v8::Value> receiver,
                                        int argc,
                                        v8::Handle<v8::Value> argv[]) {
  TryCatch try_catch;
  delegate_->WillRunScript(this);

  v8::Handle<v8::Value> result = function->Call(receiver, argc, argv);

  delegate_->DidRunScript(this);
  if (try_catch.HasCaught())
    delegate_->UnhandledException(this, try_catch);

  return result;
}

ContextHolder* ShellRunner::GetContextHolder() {
  return context_holder_.get();
}

void ShellRunner::Run(v8::Handle<Script> script) {
  TryCatch try_catch;
  delegate_->WillRunScript(this);

  script->Run();

  delegate_->DidRunScript(this);
  if (try_catch.HasCaught()) {
    delegate_->UnhandledException(this, try_catch);
  }
}

}  // namespace gin

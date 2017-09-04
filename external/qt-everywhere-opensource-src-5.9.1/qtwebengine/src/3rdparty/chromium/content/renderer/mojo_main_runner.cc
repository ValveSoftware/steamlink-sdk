// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo_main_runner.h"

#include "content/public/renderer/render_frame.h"
#include "gin/modules/module_registry.h"
#include "gin/per_context_data.h"
#include "gin/public/context_holder.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::ObjectTemplate;
using v8::Script;

namespace content {

MojoMainRunner::MojoMainRunner(blink::WebFrame* frame,
                               gin::ContextHolder* context_holder)
    : frame_(frame),
      context_holder_(context_holder) {
  DCHECK(frame_);
  v8::Isolate::Scope isolate_scope(context_holder->isolate());
  HandleScope handle_scope(context_holder->isolate());
  // Note: this installs the runner globally. If we need to support more than
  // one runner at a time we'll have to revisit this.
  gin::PerContextData::From(context_holder->context())->set_runner(this);
}

MojoMainRunner::~MojoMainRunner() {
}

void MojoMainRunner::Run(const std::string& source,
                         const std::string& resource_name) {
  frame_->executeScript(
      blink::WebScriptSource(blink::WebString::fromUTF8(source)));
}

v8::Local<v8::Value> MojoMainRunner::Call(v8::Local<v8::Function> function,
                                          v8::Local<v8::Value> receiver,
                                          int argc,
                                          v8::Local<v8::Value> argv[]) {
  return frame_->callFunctionEvenIfScriptDisabled(function, receiver, argc,
                                                  argv);
}

gin::ContextHolder* MojoMainRunner::GetContextHolder() {
  return context_holder_;
}

}  // namespace content

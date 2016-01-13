// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_mojo_context_state.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/public/renderer/resource_fetcher.h"
#include "content/renderer/web_ui_runner.h"
#include "gin/converter.h"
#include "gin/modules/module_registry.h"
#include "gin/per_context_data.h"
#include "gin/public/context_holder.h"
#include "gin/try_catch.h"
#include "mojo/bindings/js/core.h"
#include "mojo/bindings/js/handle.h"
#include "mojo/bindings/js/support.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::ObjectTemplate;
using v8::Script;

namespace content {

namespace {

// All modules have this prefixed to them when downloading.
// TODO(sky): move this into some common place.
const char kModulePrefix[] = "chrome://mojo/";

void RunMain(base::WeakPtr<gin::Runner> runner,
             mojo::ScopedMessagePipeHandle* handle,
             v8::Handle<v8::Value> module) {
  v8::Isolate* isolate = runner->GetContextHolder()->isolate();
  v8::Handle<v8::Function> start;
  CHECK(gin::ConvertFromV8(isolate, module, &start));
  v8::Handle<v8::Value> args[] = {
      gin::ConvertToV8(isolate, mojo::Handle(handle->release().value())) };
  runner->Call(start, runner->global(), 1, args);
}

}  // namespace

// WebUIMojo -------------------------------------------------------------------

WebUIMojoContextState::WebUIMojoContextState(blink::WebFrame* frame,
                                             v8::Handle<v8::Context> context)
    : frame_(frame),
      module_added_(false) {
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  gin::ContextHolder* context_holder = context_data->context_holder();
  runner_.reset(new WebUIRunner(frame_, context_holder));
  gin::Runner::Scope scoper(runner_.get());
  gin::ModuleRegistry::From(context)->AddObserver(this);
  runner_->RegisterBuiltinModules();
  gin::ModuleRegistry::InstallGlobals(context->GetIsolate(), context->Global());
  // Warning |frame| may be destroyed.
  // TODO(sky): add test for this.
}

WebUIMojoContextState::~WebUIMojoContextState() {
  gin::Runner::Scope scoper(runner_.get());
  gin::ModuleRegistry::From(
      runner_->GetContextHolder()->context())->RemoveObserver(this);
}

void WebUIMojoContextState::SetHandle(mojo::ScopedMessagePipeHandle handle) {
  gin::ContextHolder* context_holder = runner_->GetContextHolder();
  mojo::ScopedMessagePipeHandle* passed_handle =
      new mojo::ScopedMessagePipeHandle(handle.Pass());
  gin::ModuleRegistry::From(context_holder->context())->LoadModule(
      context_holder->isolate(),
      "main",
      base::Bind(RunMain, runner_->GetWeakPtr(), base::Owned(passed_handle)));
}

void WebUIMojoContextState::FetchModules(const std::vector<std::string>& ids) {
  gin::Runner::Scope scoper(runner_.get());
  gin::ContextHolder* context_holder = runner_->GetContextHolder();
  gin::ModuleRegistry* registry = gin::ModuleRegistry::From(
      context_holder->context());
  for (size_t i = 0; i < ids.size(); ++i) {
    if (fetched_modules_.find(ids[i]) == fetched_modules_.end() &&
        registry->available_modules().count(ids[i]) == 0) {
      FetchModule(ids[i]);
    }
  }
}

void WebUIMojoContextState::FetchModule(const std::string& id) {
  const GURL url(kModulePrefix + id);
  // TODO(sky): better error checks here?
  DCHECK(url.is_valid() && !url.is_empty());
  DCHECK(fetched_modules_.find(id) == fetched_modules_.end());
  fetched_modules_.insert(id);
  ResourceFetcher* fetcher = ResourceFetcher::Create(url);
  module_fetchers_.push_back(fetcher);
  fetcher->Start(frame_, blink::WebURLRequest::TargetIsScript,
                 base::Bind(&WebUIMojoContextState::OnFetchModuleComplete,
                            base::Unretained(this), fetcher));
}

void WebUIMojoContextState::OnFetchModuleComplete(
    ResourceFetcher* fetcher,
    const blink::WebURLResponse& response,
    const std::string& data) {
  DCHECK_EQ(kModulePrefix,
      response.url().string().utf8().substr(0, arraysize(kModulePrefix) - 1));
  const std::string module =
      response.url().string().utf8().substr(arraysize(kModulePrefix) - 1);
  // We can't delete fetch right now as the arguments to this function come from
  // it and are used below. Instead use a scope_ptr to cleanup.
  scoped_ptr<ResourceFetcher> deleter(fetcher);
  module_fetchers_.weak_erase(
      std::find(module_fetchers_.begin(), module_fetchers_.end(), fetcher));
  if (data.empty()) {
    NOTREACHED();
    return;  // TODO(sky): log something?
  }

  runner_->Run(data, module);
}

void WebUIMojoContextState::OnDidAddPendingModule(
    const std::string& id,
    const std::vector<std::string>& dependencies) {
  FetchModules(dependencies);

  gin::ContextHolder* context_holder = runner_->GetContextHolder();
  gin::ModuleRegistry* registry = gin::ModuleRegistry::From(
      context_holder->context());
  registry->AttemptToLoadMoreModules(context_holder->isolate());
}

}  // namespace content

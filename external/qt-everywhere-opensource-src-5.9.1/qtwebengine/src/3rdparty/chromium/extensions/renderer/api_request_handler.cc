// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_request_handler.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"

namespace extensions {

APIRequestHandler::PendingRequest::PendingRequest(
    v8::Isolate* isolate,
    v8::Local<v8::Function> callback,
    v8::Local<v8::Context> context)
    : isolate(isolate),
      callback(isolate, callback),
      context(isolate, context) {}

APIRequestHandler::PendingRequest::~PendingRequest() {}
APIRequestHandler::PendingRequest::PendingRequest(PendingRequest&&) = default;
APIRequestHandler::PendingRequest& APIRequestHandler::PendingRequest::operator=(
    PendingRequest&&) = default;

APIRequestHandler::APIRequestHandler(const CallJSFunction& call_js)
    : call_js_(call_js) {}

APIRequestHandler::~APIRequestHandler() {}

std::string APIRequestHandler::AddPendingRequest(
    v8::Isolate* isolate,
    v8::Local<v8::Function> callback,
    v8::Local<v8::Context> context) {
  // TODO(devlin): We could *probably* get away with just using an integer here,
  // but it's a little less foolproof. How slow is GenerateGUID?
  // base::UnguessableToken is another good option.
  std::string id = base::GenerateGUID();
  pending_requests_.insert(
      std::make_pair(id, PendingRequest(isolate, callback, context)));
  return id;
}

void APIRequestHandler::CompleteRequest(const std::string& request_id,
                                        const base::ListValue& response_args) {
  auto iter = pending_requests_.find(request_id);
  // The request may have been removed if the context was invalidated before a
  // response is ready.
  if (iter == pending_requests_.end())
    return;

  PendingRequest pending_request = std::move(iter->second);
  pending_requests_.erase(iter);

  v8::Isolate* isolate = pending_request.isolate;
  v8::Local<v8::Context> context = pending_request.context.Get(isolate);
  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  std::vector<v8::Local<v8::Value>> args;
  args.reserve(response_args.GetSize());
  for (const auto& response : response_args)
    args.push_back(converter->ToV8Value(response.get(), context));

  // args.size() is converted to int, but args is controlled by chrome and is
  // never close to std::numeric_limits<int>::max.
  call_js_.Run(pending_request.callback.Get(isolate), context, args.size(),
               args.data());
}

void APIRequestHandler::InvalidateContext(v8::Local<v8::Context> context) {
  for (auto iter = pending_requests_.begin();
       iter != pending_requests_.end();) {
    if (iter->second.context == context)
      iter = pending_requests_.erase(iter);
    else
      ++iter;
  }
}

std::set<std::string> APIRequestHandler::GetPendingRequestIdsForTesting()
    const {
  std::set<std::string> result;
  for (const auto& pair : pending_requests_)
    result.insert(pair.first);
  return result;
}

}  // namespace extensions

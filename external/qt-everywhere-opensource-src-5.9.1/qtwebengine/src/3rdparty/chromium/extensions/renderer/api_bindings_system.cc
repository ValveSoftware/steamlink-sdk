// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_bindings_system.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace extensions {

APIBindingsSystem::Request::Request() {}
APIBindingsSystem::Request::~Request() {}

APIBindingsSystem::APIBindingsSystem(const binding::RunJSFunction& call_js,
                                     const GetAPISchemaMethod& get_api_schema,
                                     const SendRequestMethod& send_request)
    : request_handler_(call_js),
      event_handler_(call_js),
      get_api_schema_(get_api_schema),
      send_request_(send_request) {}

APIBindingsSystem::~APIBindingsSystem() {}

v8::Local<v8::Object> APIBindingsSystem::CreateAPIInstance(
    const std::string& api_name,
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    const APIBinding::AvailabilityCallback& is_available) {
  std::unique_ptr<APIBinding>& binding = api_bindings_[api_name];
  if (!binding)
    binding = CreateNewAPIBinding(api_name);
  return binding->CreateInstance(
      context, isolate, &event_handler_, is_available);
}

std::unique_ptr<APIBinding> APIBindingsSystem::CreateNewAPIBinding(
    const std::string& api_name) {
  const base::DictionaryValue& api_schema = get_api_schema_.Run(api_name);

  const base::ListValue* function_definitions = nullptr;
  CHECK(api_schema.GetList("functions", &function_definitions));
  const base::ListValue* type_definitions = nullptr;
  // Type definitions might not exist for the given API.
  api_schema.GetList("types", &type_definitions);
  const base::ListValue* event_definitions = nullptr;
  api_schema.GetList("events", &event_definitions);

  return base::MakeUnique<APIBinding>(
      api_name, *function_definitions, type_definitions, event_definitions,
      base::Bind(&APIBindingsSystem::OnAPICall, base::Unretained(this)),
      &type_reference_map_);
}

void APIBindingsSystem::CompleteRequest(const std::string& request_id,
                                        const base::ListValue& response) {
  request_handler_.CompleteRequest(request_id, response);
}

void APIBindingsSystem::FireEventInContext(const std::string& event_name,
                                           v8::Local<v8::Context> context,
                                           const base::ListValue& response) {
  event_handler_.FireEventInContext(event_name, context, response);
}

void APIBindingsSystem::OnAPICall(const std::string& name,
                                  std::unique_ptr<base::ListValue> arguments,
                                  v8::Isolate* isolate,
                                  v8::Local<v8::Context> context,
                                  v8::Local<v8::Function> callback) {
  auto request = base::MakeUnique<Request>();
  if (!callback.IsEmpty()) {
    request->request_id =
        request_handler_.AddPendingRequest(isolate, callback, context);
  }
  request->arguments = std::move(arguments);
  request->method_name = name;

  send_request_.Run(std::move(request));
}

}  // namespace extensions

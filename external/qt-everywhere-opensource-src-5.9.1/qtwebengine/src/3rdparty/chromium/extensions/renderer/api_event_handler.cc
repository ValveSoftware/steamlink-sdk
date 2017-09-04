// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_event_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/renderer/event_emitter.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

const char kExtensionAPIEventPerContextKey[] = "extension_api_events";

struct APIEventPerContextData : public base::SupportsUserData::Data {
  APIEventPerContextData(v8::Isolate* isolate) : isolate(isolate) {}
  ~APIEventPerContextData() override {
    v8::HandleScope scope(isolate);
    // We explicitly clear the event data map here to remove all references to
    // v8 objects. This helps us avoid cycles in v8 where an event listener
    // could hold a reference to the event, which in turn holds the reference
    // to the listener.
    for (const auto& pair : event_data) {
      EventEmitter* emitter = nullptr;
      gin::Converter<EventEmitter*>::FromV8(
          isolate, pair.second.Get(isolate), &emitter);
      CHECK(emitter);
      emitter->listeners()->clear();
    }
  }

  // The associated v8::Isolate. Since this object is cleaned up at context
  // destruction, this should always be valid.
  v8::Isolate* isolate;

  // A map from event name -> event emitter.
  std::map<std::string, v8::Global<v8::Object>> event_data;
};

}  // namespace

APIEventHandler::APIEventHandler(const binding::RunJSFunction& call_js)
    : call_js_(call_js) {}
APIEventHandler::~APIEventHandler() {}

v8::Local<v8::Object> APIEventHandler::CreateEventInstance(
    const std::string& event_name,
    v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIEventPerContextData* data = static_cast<APIEventPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIEventPerContextKey));
  if (!data) {
    auto api_data =
        base::MakeUnique<APIEventPerContextData>(context->GetIsolate());
    data = api_data.get();
    per_context_data->SetUserData(kExtensionAPIEventPerContextKey,
                                  api_data.release());
  }

  DCHECK(data->event_data.find(event_name) == data->event_data.end());

  gin::Handle<EventEmitter> emitter_handle =
      gin::CreateHandle(context->GetIsolate(), new EventEmitter());
  CHECK(!emitter_handle.IsEmpty());
  v8::Local<v8::Value> emitter_value = emitter_handle.ToV8();
  CHECK(emitter_value->IsObject());
  v8::Local<v8::Object> emitter_object =
      v8::Local<v8::Object>::Cast(emitter_value);
  data->event_data[event_name] =
      v8::Global<v8::Object>(context->GetIsolate(), emitter_object);

  return emitter_object;
}

void APIEventHandler::FireEventInContext(const std::string& event_name,
                                         v8::Local<v8::Context> context,
                                         const base::ListValue& args) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIEventPerContextData* data = static_cast<APIEventPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIEventPerContextKey));
  if (!data)
    return;

  auto iter = data->event_data.find(event_name);
  if (iter == data->event_data.end())
    return;

  // Note: since we only convert the arguments once, if a listener modifies an
  // object (including an array), other listeners will see that modification.
  // TODO(devlin): This is how it's always been, but should it be?
  std::vector<v8::Local<v8::Value>> v8_args;
  v8_args.reserve(args.GetSize());
  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  for (const auto& arg : args)
    v8_args.push_back(converter->ToV8Value(arg.get(), context));

  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);

  for (const auto& listener : *emitter->listeners()) {
    call_js_.Run(listener.Get(context->GetIsolate()), context, v8_args.size(),
                 v8_args.data());
  }
}

size_t APIEventHandler::GetNumEventListenersForTesting(
    const std::string& event_name,
    v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIEventPerContextData* data = static_cast<APIEventPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIEventPerContextKey));
  DCHECK(data);

  auto iter = data->event_data.find(event_name);
  DCHECK(iter != data->event_data.end());
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(
      context->GetIsolate(), iter->second.Get(context->GetIsolate()), &emitter);
  CHECK(emitter);
  return emitter->listeners()->size();
}

}  // namespace extensions

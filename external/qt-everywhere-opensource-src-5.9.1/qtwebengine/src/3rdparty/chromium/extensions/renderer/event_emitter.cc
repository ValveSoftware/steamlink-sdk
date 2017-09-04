// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/event_emitter.h"

#include <algorithm>

#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"

namespace extensions {

gin::WrapperInfo EventEmitter::kWrapperInfo = {gin::kEmbedderNativeGin};

EventEmitter::EventEmitter() {}

EventEmitter::~EventEmitter() {}

gin::ObjectTemplateBuilder EventEmitter::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<EventEmitter>::GetObjectTemplateBuilder(isolate)
      .SetMethod("addListener", &EventEmitter::AddListener)
      .SetMethod("removeListener", &EventEmitter::RemoveListener)
      .SetMethod("hasListener", &EventEmitter::HasListener)
      .SetMethod("hasListeners", &EventEmitter::HasListeners);
}

void EventEmitter::AddListener(gin::Arguments* arguments) {
  v8::Local<v8::Function> listener;
  if (!arguments->GetNext(&listener))
    return;

  v8::Local<v8::Object> holder;
  CHECK(arguments->GetHolder(&holder));
  CHECK(!holder.IsEmpty());
  if (!gin::PerContextData::From(holder->CreationContext()))
    return;

  if (!HasListener(listener)) {
    listeners_.push_back(
        v8::Global<v8::Function>(arguments->isolate(), listener));
  }
}

void EventEmitter::RemoveListener(v8::Local<v8::Function> listener) {
  auto iter = std::find(listeners_.begin(), listeners_.end(), listener);
  if (iter != listeners_.end())
    listeners_.erase(iter);
}

bool EventEmitter::HasListener(v8::Local<v8::Function> listener) {
  return std::find(listeners_.begin(), listeners_.end(), listener) !=
         listeners_.end();
}

bool EventEmitter::HasListeners() {
  return !listeners_.empty();
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/bindings/monotonic_clock.h"

#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace apps {

namespace {

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

double GetMonotonicSeconds() {
  const double kMicrosecondsPerSecond = 1000000;
  return static_cast<double>(mojo::GetTimeTicksNow()) / kMicrosecondsPerSecond;
}

}  // namespace

const char MonotonicClock::kModuleName[] = "monotonic_clock";

v8::Local<v8::Value> MonotonicClock::GetModule(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ =
      data->GetObjectTemplate(&g_wrapper_info);
  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
        .SetMethod("seconds", GetMonotonicSeconds)
        .Build();
    data->SetObjectTemplate(&g_wrapper_info, templ);
  }
  return templ->NewInstance();
}

}  // namespace apps
}  // namespace mojo

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDING_MONOTONIC_CLOCK_H_
#define MOJO_APPS_JS_BINDING_MONOTONIC_CLOCK_H_

#include "v8/include/v8.h"

namespace mojo {
namespace apps {

class MonotonicClock {
 public:
  static const char kModuleName[];
  static v8::Local<v8::Value> GetModule(v8::Isolate* isolate);
};

}  // namespace apps
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDING_MONOTONIC_CLOCK_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_TYPES_H_
#define EXTENSIONS_RENDERER_API_BINDING_TYPES_H_

#include "base/callback.h"
#include "v8/include/v8.h"

namespace extensions {
namespace binding {

// A callback to execute the given v8::Function with the provided context and
// arguments.
using RunJSFunction = base::Callback<void(v8::Local<v8::Function>,
                                          v8::Local<v8::Context>,
                                          int argc,
                                          v8::Local<v8::Value>[])>;

}  // namespace binding
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_TYPES_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_GL_MODULE_H_
#define MOJO_APPS_JS_BINDINGS_GL_MODULE_H_

#include "gin/public/wrapper_info.h"
#include "v8/include/v8.h"

namespace mojo {
namespace js {
namespace gl {

extern const char* kModuleName;
v8::Local<v8::Value> GetModule(v8::Isolate* isolate);

}  // namespace gl
}  // namespace js
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDINGS_GL_H_

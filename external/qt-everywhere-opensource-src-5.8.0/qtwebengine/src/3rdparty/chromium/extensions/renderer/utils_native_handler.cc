// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/utils_native_handler.h"

#include "base/macros.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"

namespace extensions {

UtilsNativeHandler::UtilsNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "deepCopy",
      base::Bind(&UtilsNativeHandler::DeepCopy, base::Unretained(this)));
}

UtilsNativeHandler::~UtilsNativeHandler() {}

void UtilsNativeHandler::DeepCopy(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(
      blink::WebSerializedScriptValue::serialize(args[0]).deserialize());
}

}  // namespace extensions

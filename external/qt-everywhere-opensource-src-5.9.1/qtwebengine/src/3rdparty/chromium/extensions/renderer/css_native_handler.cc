// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/css_native_handler.h"

#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSelector.h"

namespace extensions {

using blink::WebString;

CssNativeHandler::CssNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("CanonicalizeCompoundSelector", "declarativeContent",
                base::Bind(&CssNativeHandler::CanonicalizeCompoundSelector,
                           base::Unretained(this)));
}

void CssNativeHandler::CanonicalizeCompoundSelector(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  std::string input_selector = *v8::String::Utf8Value(args[0]);
  // TODO(esprehn): This API shouldn't exist, the extension code should be
  // moved into blink.
  WebString output_selector = blink::canonicalizeSelector(
      WebString::fromUTF8(input_selector), blink::WebSelectorTypeCompound);
  args.GetReturnValue().Set(v8_helpers::ToV8StringUnsafe(
      args.GetIsolate(), output_selector.utf8().c_str()));
}

}  // namespace extensions

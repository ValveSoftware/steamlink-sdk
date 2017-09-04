// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/test_features_native_handler.h"

#include "base/bind.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

TestFeaturesNativeHandler::TestFeaturesNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("GetAPIFeatures", "test",
                base::Bind(&TestFeaturesNativeHandler::GetAPIFeatures,
                           base::Unretained(this)));
}

void TestFeaturesNativeHandler::GetAPIFeatures(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::unique_ptr<JSONFeatureProviderSource> source(
      ExtensionsClient::Get()->CreateAPIFeatureSource());
  std::unique_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  args.GetReturnValue().Set(
      converter->ToV8Value(&source->dictionary(), context()->v8_context()));
}

}  // namespace extensions

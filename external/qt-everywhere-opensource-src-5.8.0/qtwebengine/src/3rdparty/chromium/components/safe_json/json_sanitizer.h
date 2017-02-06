// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_JSON_SANITIZER_H_
#define COMPONENTS_SAFE_JSON_JSON_SANITIZER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <jni.h>
#endif

namespace safe_json {

// Sanitizes and normalizes JSON by parsing it in a safe environment and
// re-serializing it. Parsing the sanitized JSON should result in a value
// identical to parsing the original JSON.
// This allows parsing the sanitized JSON with the regular JSONParser while
// reducing the risk versus parsing completely untrusted JSON. It also minifies
// the resulting JSON, which might save some space.
class JsonSanitizer {
 public:
  using StringCallback = base::Callback<void(const std::string&)>;

  // Starts sanitizing the passed in unsafe JSON string. Either the passed
  // |success_callback| or the |error_callback| will be called with the result
  // of the sanitization or an error message, respectively, but not before the
  // method returns.
  static void Sanitize(const std::string& unsafe_json,
                       const StringCallback& success_callback,
                       const StringCallback& error_callback);

#if defined(OS_ANDROID)
  static bool Register(JNIEnv* env);
#endif

 protected:
  JsonSanitizer() {}
  ~JsonSanitizer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(JsonSanitizer);
};

}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_JSON_SANITIZER_H_

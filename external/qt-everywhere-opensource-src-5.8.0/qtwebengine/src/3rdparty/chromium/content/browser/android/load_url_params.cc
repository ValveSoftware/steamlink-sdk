// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/load_url_params.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/url_constants.h"
#include "jni/LoadUrlParams_jni.h"
#include "url/gurl.h"

namespace content {

bool RegisterLoadUrlParams(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jboolean IsDataScheme(JNIEnv* env,
                      const JavaParamRef<jclass>& clazz,
                      const JavaParamRef<jstring>& jurl) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  return url.SchemeIs(url::kDataScheme);
}

}  // namespace content

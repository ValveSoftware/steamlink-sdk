// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_formatter_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_formatter/url_formatter.h"
#include "jni/UrlFormatter_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

GURL ConvertJavaStringToGURL(JNIEnv* env, jstring url) {
  return url ? GURL(base::android::ConvertJavaStringToUTF8(env, url)) : GURL();
}

}  // namespace

namespace url_formatter {

namespace android {

static ScopedJavaLocalRef<jstring> FixupUrl(JNIEnv* env,
                                            const JavaParamRef<jclass>& clazz,
                                            const JavaParamRef<jstring>& url) {
  DCHECK(url);
  GURL fixed_url = url_formatter::FixupURL(
      base::android::ConvertJavaStringToUTF8(env, url), std::string());

  return fixed_url.is_valid()
             ? base::android::ConvertUTF8ToJavaString(env, fixed_url.spec())
             : ScopedJavaLocalRef<jstring>();
}

static ScopedJavaLocalRef<jstring> FormatUrlForDisplay(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(ConvertJavaStringToGURL(env, url)));
}

static ScopedJavaLocalRef<jstring> FormatUrlForSecurityDisplay(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               ConvertJavaStringToGURL(env, url)));
}

static ScopedJavaLocalRef<jstring> FormatUrlForSecurityDisplayOmitScheme(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               ConvertJavaStringToGURL(env, url),
               url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

bool RegisterUrlFormatterNatives(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

}  // namespace url_formatter

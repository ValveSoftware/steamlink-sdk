// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_string_util_icu_alternatives_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "jni/NetStringUtil_jni.h"
#include "net/base/net_string_util.h"

namespace net {

namespace {

// Attempts to convert |text| encoded in |charset| to a jstring (Java unicode
// string).  Returns the result jstring, or NULL on failure.
ScopedJavaLocalRef<jstring> ConvertToJstring(const std::string& text,
                                             const char* charset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject java_byte_buffer =
      env->NewDirectByteBuffer(const_cast<char*>(text.data()), text.length());
  base::android::ScopedJavaLocalRef<jstring> java_charset =
      base::android::ConvertUTF8ToJavaString(env, base::StringPiece(charset));
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_NetStringUtil_convertToUnicode(env, java_byte_buffer,
                                                   java_charset.obj());
  return java_result;
}

// Attempts to convert |text| encoded in |charset| to a jstring (Java unicode
// string) and then normalizes the string.  Returns the result jstring, or NULL
// on failure.
ScopedJavaLocalRef<jstring> ConvertToNormalizedJstring(
    const std::string& text, const char* charset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject java_byte_buffer =
      env->NewDirectByteBuffer(const_cast<char*>(text.data()), text.length());
  base::android::ScopedJavaLocalRef<jstring> java_charset =
      base::android::ConvertUTF8ToJavaString(env, base::StringPiece(charset));
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_NetStringUtil_convertToUnicodeAndNormalize(
          env, java_byte_buffer, java_charset.obj());
  return java_result;
}

// Converts |text| encoded in |charset| to a jstring (Java unicode string).
// Any characters that can not be converted are replaced with U+FFFD.
ScopedJavaLocalRef<jstring> ConvertToJstringWithSubstitutions(
    const std::string& text, const char* charset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject java_byte_buffer =
      env->NewDirectByteBuffer(const_cast<char*>(text.data()), text.length());
  base::android::ScopedJavaLocalRef<jstring> java_charset =
      base::android::ConvertUTF8ToJavaString(env, base::StringPiece(charset));
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_NetStringUtil_convertToUnicodeWithSubstitutions(
          env, java_byte_buffer, java_charset.obj());
  return java_result;
}

}  // namespace

const char* const kCharsetLatin1 = "ISO-8859-1";

bool ConvertToUtf8(const std::string& text, const char* charset,
                   std::string* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result = ConvertToJstring(text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF8(java_result);
  return true;
}

bool ConvertToUtf8AndNormalize(const std::string& text, const char* charset,
                               std::string* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result = ConvertToNormalizedJstring(
      text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF8(java_result);
  return true;
}

bool ConvertToUTF16(const std::string& text, const char* charset,
                    base::string16* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result = ConvertToJstring(text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF16(java_result);
  return true;
}

bool ConvertToUTF16WithSubstitutions(const std::string& text,
                                     const char* charset,
                                     base::string16* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result =
     ConvertToJstringWithSubstitutions(text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF16(java_result);
  return true;
}

bool RegisterNetStringUtils(JNIEnv* env) {
  return android::RegisterNativesImpl(env);
}

}  // namespace net

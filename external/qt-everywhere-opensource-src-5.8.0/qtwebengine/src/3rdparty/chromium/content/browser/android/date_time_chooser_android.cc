// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/date_time_chooser_android.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/char_iterator.h"
#include "content/common/date_time_suggestion.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_view_host.h"
#include "jni/DateTimeChooserAndroid_jni.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaRef;


namespace {

base::string16 SanitizeSuggestionString(const base::string16& string) {
  base::string16 trimmed = string.substr(0, 255);
  icu::UnicodeString sanitized;
  base::i18n::UTF16CharIterator sanitized_iterator(&trimmed);
  while (!sanitized_iterator.end()) {
    UChar c = sanitized_iterator.get();
    if (u_isprint(c))
      sanitized.append(c);
    sanitized_iterator.Advance();
  }
  return base::string16(sanitized.getBuffer(),
                        static_cast<size_t>(sanitized.length()));
}

}  // namespace

namespace content {

// DateTimeChooserAndroid implementation
DateTimeChooserAndroid::DateTimeChooserAndroid()
  : host_(NULL) {
}

DateTimeChooserAndroid::~DateTimeChooserAndroid() {
}

void DateTimeChooserAndroid::ReplaceDateTime(JNIEnv* env,
                                             const JavaRef<jobject>&,
                                             jdouble value) {
  host_->Send(new ViewMsg_ReplaceDateTime(host_->GetRoutingID(), value));
}

void DateTimeChooserAndroid::CancelDialog(JNIEnv* env,
                                          const JavaRef<jobject>&) {
  host_->Send(new ViewMsg_CancelDateTimeDialog(host_->GetRoutingID()));
}

void DateTimeChooserAndroid::ShowDialog(
    gfx::NativeWindow native_window,
    RenderViewHost* host,
    ui::TextInputType dialog_type,
    double dialog_value,
    double min,
    double max,
    double step,
    const std::vector<DateTimeSuggestion>& suggestions) {
  host_ = host;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> suggestions_array;

  if (suggestions.size() > 0) {
    suggestions_array =
        Java_DateTimeChooserAndroid_createSuggestionsArray(env,
                                                           suggestions.size());
    for (size_t i = 0; i < suggestions.size(); ++i) {
      const content::DateTimeSuggestion& suggestion = suggestions[i];
      ScopedJavaLocalRef<jstring> localized_value = ConvertUTF16ToJavaString(
          env, SanitizeSuggestionString(suggestion.localized_value));
      ScopedJavaLocalRef<jstring> label = ConvertUTF16ToJavaString(
          env, SanitizeSuggestionString(suggestion.label));
      Java_DateTimeChooserAndroid_setDateTimeSuggestionAt(env,
          suggestions_array.obj(), i,
          suggestion.value, localized_value.obj(), label.obj());
    }
  }

  j_date_time_chooser_.Reset(Java_DateTimeChooserAndroid_createDateTimeChooser(
      env,
      native_window->GetJavaObject().obj(),
      reinterpret_cast<intptr_t>(this),
      dialog_type,
      dialog_value,
      min,
      max,
      step,
      suggestions_array.obj()));
  if (j_date_time_chooser_.is_null())
    ReplaceDateTime(env, j_date_time_chooser_, dialog_value);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------
bool RegisterDateTimeChooserAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content

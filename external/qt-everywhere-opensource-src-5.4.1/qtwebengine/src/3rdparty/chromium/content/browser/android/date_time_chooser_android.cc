// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/date_time_chooser_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/char_iterator.h"
#include "content/common/date_time_suggestion.h"
#include "content/common/view_messages.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_view_host.h"
#include "jni/DateTimeChooserAndroid_jni.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;


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

// static
void DateTimeChooserAndroid::InitializeDateInputTypes(
      int text_input_type_date, int text_input_type_date_time,
      int text_input_type_date_time_local, int text_input_type_month,
      int text_input_type_time, int text_input_type_week) {
  JNIEnv* env = AttachCurrentThread();
  Java_DateTimeChooserAndroid_initializeDateInputTypes(
         env,
         text_input_type_date, text_input_type_date_time,
         text_input_type_date_time_local, text_input_type_month,
         text_input_type_time, text_input_type_week);
}

void DateTimeChooserAndroid::ReplaceDateTime(JNIEnv* env,
                                             jobject,
                                             jdouble value) {
  host_->Send(new ViewMsg_ReplaceDateTime(host_->GetRoutingID(), value));
}

void DateTimeChooserAndroid::CancelDialog(JNIEnv* env, jobject) {
  host_->Send(new ViewMsg_CancelDateTimeDialog(host_->GetRoutingID()));
}

void DateTimeChooserAndroid::ShowDialog(
    ContentViewCore* content,
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
      content->GetJavaObject().obj(),
      reinterpret_cast<intptr_t>(this),
      dialog_type,
      dialog_value,
      min,
      max,
      step,
      suggestions_array.obj()));
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------
bool RegisterDateTimeChooserAndroid(JNIEnv* env) {
  bool registered = RegisterNativesImpl(env);
  if (registered)
    DateTimeChooserAndroid::InitializeDateInputTypes(
        ui::TEXT_INPUT_TYPE_DATE,
        ui::TEXT_INPUT_TYPE_DATE_TIME,
        ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
        ui::TEXT_INPUT_TYPE_MONTH,
        ui::TEXT_INPUT_TYPE_TIME,
        ui::TEXT_INPUT_TYPE_WEEK);
  return registered;
}

}  // namespace content

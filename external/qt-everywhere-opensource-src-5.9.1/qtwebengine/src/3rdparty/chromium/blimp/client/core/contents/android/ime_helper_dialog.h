// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_ANDROID_IME_HELPER_DIALOG_H_
#define BLIMP_CLIENT_CORE_CONTENTS_ANDROID_IME_HELPER_DIALOG_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "ui/base/ime/text_input_type.h"

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace blimp {
namespace client {

// The native component of
// org.chromium.blimp.core.contents.input.ImeHelperDialog
class ImeHelperDialog : public ImeFeature::Delegate {
 public:
  static bool RegisterJni(JNIEnv* env);

  explicit ImeHelperDialog(ui::WindowAndroid* window);
  ~ImeHelperDialog() override;

  // ImeFeature::Delegate implementation.
  void OnShowImeRequested(const ImeFeature::WebInputRequest& request) override;
  void OnHideImeRequested() override;

  // Sends the text entered from IME to the blimp engine.
  void OnImeTextEntered(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jobj,
                        const base::android::JavaParamRef<jstring>& text,
                        jboolean submit);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // The current request is saved in order to populate the fields of the
  // subsequent response.
  ImeFeature::WebInputRequest current_request_;

  DISALLOW_COPY_AND_ASSIGN(ImeHelperDialog);
};

}  // namespace client
}  // namespace blimp
#endif  // BLIMP_CLIENT_CORE_CONTENTS_ANDROID_IME_HELPER_DIALOG_H_

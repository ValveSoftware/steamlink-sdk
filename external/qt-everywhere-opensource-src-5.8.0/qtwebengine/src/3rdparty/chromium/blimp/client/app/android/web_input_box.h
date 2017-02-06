// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_WEB_INPUT_BOX_H_
#define BLIMP_CLIENT_APP_ANDROID_WEB_INPUT_BOX_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "blimp/client/feature/ime_feature.h"
#include "ui/base/ime/text_input_type.h"

namespace blimp {
namespace client {

// The native component of org.chromium.blimp.input.WebInputBox.
class WebInputBox : public ImeFeature::Delegate {
 public:
  static bool RegisterJni(JNIEnv* env);

  // |ime_feature| is expected to outlive the WebInputBox.
  WebInputBox(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& jobj,
              ImeFeature* ime_feature);

  // Brings up IME for user to enter text.
  void OnShowImeRequested(ui::TextInputType input_type,
                          const std::string& text) override;

  // Hides IME.
  void OnHideImeRequested() override;

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  // Sends the text entered from IME to the blimp engine
  void OnImeTextEntered(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jobj,
                        const base::android::JavaParamRef<jstring>& text);

 private:
  ~WebInputBox() override;

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // A bridge to the network layer which does the work of (de)serializing the
  // outgoing and incoming BlimpMessage::IME messages from the engine.
  ImeFeature* ime_feature_;

  DISALLOW_COPY_AND_ASSIGN(WebInputBox);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_WEB_INPUT_BOX_H_

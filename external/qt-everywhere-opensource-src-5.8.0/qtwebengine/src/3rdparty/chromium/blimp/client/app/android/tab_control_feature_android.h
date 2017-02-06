// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_TAB_CONTROL_FEATURE_ANDROID_H_
#define BLIMP_CLIENT_APP_ANDROID_TAB_CONTROL_FEATURE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"

namespace blimp {
namespace client {

class TabControlFeature;

// TODO(dtrainor): Document this class.
class TabControlFeatureAndroid {
 public:
  static bool RegisterJni(JNIEnv* env);

  TabControlFeatureAndroid(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj,
                           TabControlFeature* tab_control_feature);

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);
  void OnContentAreaSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint width,
      jint height,
      jfloat dp_to_px);

 private:
  virtual ~TabControlFeatureAndroid();

  TabControlFeature* tab_control_feature_;

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(TabControlFeatureAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_TAB_CONTROL_FEATURE_ANDROID_H_

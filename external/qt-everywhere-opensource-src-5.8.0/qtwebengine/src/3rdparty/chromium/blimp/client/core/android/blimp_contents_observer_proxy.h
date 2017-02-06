// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_OBSERVER_PROXY_H_
#define BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_OBSERVER_PROXY_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "blimp/client/core/android/blimp_contents_impl_android.h"
#include "blimp/client/core/android/blimp_navigation_controller_impl_android.h"
#include "blimp/client/core/public/blimp_contents_observer.h"

namespace blimp {
namespace client {

class BlimpContentsObserverProxy : public BlimpContentsObserver {
 public:
  static bool RegisterJni(JNIEnv* env);

  BlimpContentsObserverProxy(
      JNIEnv* env,
      jobject obj,
      BlimpContentsImplAndroid* blimp_contents_impl_android);
  ~BlimpContentsObserverProxy() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // BlimpContentsObserver implementation.
  void OnURLUpdated(const GURL& url) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  BlimpContentsImplAndroid* blimp_contents_impl_android_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsObserverProxy);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_OBSERVER_PROXY_H_

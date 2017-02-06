// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/android/blimp_contents_factory.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/core/android/blimp_contents_impl_android.h"
#include "blimp/client/core/blimp_contents_impl.h"
#include "jni/BlimpContentsFactory_jni.h"

namespace blimp {
namespace client {

static base::android::ScopedJavaLocalRef<jobject> CreateBlimpContents(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  // To delete |blimp_contents_impl|, the Java caller must call the
  // destroy() method on BlimpContents.
  BlimpContentsImpl* blimp_contents_impl = new BlimpContentsImpl;
  return blimp_contents_impl->GetJavaBlimpContentsImpl();
}

bool RegisterBlimpContentsFactoryJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace client
}  // namespace blimp

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTEXT_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_
#define BLIMP_CLIENT_CORE_CONTEXT_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "blimp/client/core/context/blimp_client_context_impl.h"

namespace blimp {
namespace client {

class CompositorDependencies;

// JNI bridge between BlimpClientContextImpl in Java and C++.
class BlimpClientContextImplAndroid : public BlimpClientContextImpl {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpClientContextImplAndroid* FromJavaObject(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

  // The |io_thread_task_runner| must be the task runner to use for IO
  // operations.
  // The |file_thread_task_runner| must be the task runner to use for file
  // operations.
  explicit BlimpClientContextImplAndroid(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      std::unique_ptr<CompositorDependencies> compositor_dependencies,
      std::unique_ptr<Settings> settings);
  ~BlimpClientContextImplAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  base::android::ScopedJavaLocalRef<jobject> CreateBlimpContentsJava(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      jlong window_android_ptr);

  base::android::ScopedJavaLocalRef<jobject> CreateBlimpFeedbackDataJava(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

  // Start authentication flow from Java.
  void ConnectFromJava(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jobj);

  // Initialize blimp settings page, this involves setup neccessary data in
  // native for setting page.
  void InitSettingsPage(JNIEnv* env,
                        const base::android::JavaRef<jobject>& jobj,
                        const base::android::JavaRef<jobject>& blimp_settings);

  // Returns the Settings java object.
  base::android::ScopedJavaLocalRef<jobject> GetSettings(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);

 protected:
  // BlimpClientContextImpl implementation.
  GURL GetAssignerURL() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImplAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTEXT_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_

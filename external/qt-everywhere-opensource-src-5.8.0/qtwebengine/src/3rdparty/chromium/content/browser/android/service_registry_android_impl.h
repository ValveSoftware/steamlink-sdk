// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SERVICE_REGISTRY_ANDROID_IMPL_H_
#define CONTENT_BROWSER_ANDROID_SERVICE_REGISTRY_ANDROID_IMPL_H_

#include <jni.h>

#include "base/macros.h"
#include "content/public/browser/android/service_registry_android.h"

namespace content {

class ServiceRegistryAndroidImpl : public ServiceRegistryAndroid {
 public:
  static bool Register(JNIEnv* env);

  ~ServiceRegistryAndroidImpl() override;

 private:
  friend class ServiceRegistryAndroid;

  // Use ServiceRegistryAndroid::Create() to create an instance.
  ServiceRegistryAndroidImpl(shell::InterfaceRegistry* interface_registry,
                             shell::InterfaceProvider* remote_interfaces);

  // ServiceRegistryAndroid implementation:
  void AddService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jobject>& j_manager,
      const base::android::JavaParamRef<jobject>& j_factory,
      const base::android::JavaParamRef<jstring>& j_name) override;
  void RemoveService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jstring>& j_name) override;
  void ConnectToRemoteService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jstring>& j_name,
      jint handle) override;
  const base::android::ScopedJavaGlobalRef<jobject>& GetObj() override;

  shell::InterfaceRegistry* interface_registry_ = nullptr;
  shell::InterfaceProvider* remote_interfaces_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistryAndroidImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SERVICE_REGISTRY_ANDROID_IMPL_H_

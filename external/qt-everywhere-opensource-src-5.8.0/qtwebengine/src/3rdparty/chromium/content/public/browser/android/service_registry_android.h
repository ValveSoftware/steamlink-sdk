// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_SERVICE_REGISTRY_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_SERVICE_REGISTRY_ANDROID_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"

namespace shell {
class InterfaceRegistry;
class InterfaceProvider;
}

namespace content {

// Android wrapper over ServiceRegistry, allowing the browser services in Java
// to register with ServiceRegistry.java (and abstracting away the JNI calls).
class CONTENT_EXPORT ServiceRegistryAndroid {
 public:
  virtual ~ServiceRegistryAndroid() {}

  // The |registry| parameter must outlive |ServiceRegistryAndroid|.
  static std::unique_ptr<ServiceRegistryAndroid> Create(
      shell::InterfaceRegistry* interface_registry,
      shell::InterfaceProvider* remote_interfaces);

  // Called from Java.
  virtual void AddService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jobject>& j_manager,
      const base::android::JavaParamRef<jobject>& j_factory,
      const base::android::JavaParamRef<jstring>& j_name) = 0;
  virtual void RemoveService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jstring>& j_name) = 0;
  virtual void ConnectToRemoteService(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_service_registry,
      const base::android::JavaParamRef<jstring>& j_name,
      jint handle) = 0;

  // Accessor to the Java object.
  virtual const base::android::ScopedJavaGlobalRef<jobject>& GetObj() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_SERVICE_REGISTRY_ANDROID_H_

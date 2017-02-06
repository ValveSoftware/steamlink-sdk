// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/service_registry_android_impl.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "jni/ServiceRegistry_jni.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;

namespace content {

namespace {

// Callback passed to the wrapped ServiceRegistry upon AddService(). The
// ServiceRegistry will call it to create a registered Java service
void CreateImplAndAttach(
    const ScopedJavaGlobalRef<jobject>& j_scoped_service_registry,
    const ScopedJavaGlobalRef<jobject>& j_scoped_manager,
    const ScopedJavaGlobalRef<jobject>& j_scoped_factory,
    mojo::ScopedMessagePipeHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  Java_ServiceRegistry_createImplAndAttach(
      env, j_scoped_service_registry.obj(), handle.release().value(),
      j_scoped_manager.obj(), j_scoped_factory.obj());
}

}  // namespace

// static
std::unique_ptr<ServiceRegistryAndroid> ServiceRegistryAndroid::Create(
    shell::InterfaceRegistry* interface_registry,
    shell::InterfaceProvider* remote_interfaces) {
  return base::WrapUnique(new ServiceRegistryAndroidImpl(
      interface_registry, remote_interfaces));
}

// static
bool ServiceRegistryAndroidImpl::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ServiceRegistryAndroidImpl::~ServiceRegistryAndroidImpl() {
  Java_ServiceRegistry_destroy(AttachCurrentThread(), obj_.obj());
}

// Constructor and destructor call into Java.
ServiceRegistryAndroidImpl::ServiceRegistryAndroidImpl(
    shell::InterfaceRegistry* interface_registry,
    shell::InterfaceProvider* remote_interfaces)
    : interface_registry_(interface_registry),
      remote_interfaces_(remote_interfaces) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(
      env,
      Java_ServiceRegistry_create(env, reinterpret_cast<intptr_t>(this)).obj());
}

const base::android::ScopedJavaGlobalRef<jobject>&
ServiceRegistryAndroidImpl::GetObj() {
  return obj_;
}

// Methods called from Java.
void ServiceRegistryAndroidImpl::AddService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_service_registry,
    const JavaParamRef<jobject>& j_manager,
    const JavaParamRef<jobject>& j_factory,
    const JavaParamRef<jstring>& j_name) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));

  ScopedJavaGlobalRef<jobject> j_scoped_service_registry;
  j_scoped_service_registry.Reset(env, j_service_registry);

  ScopedJavaGlobalRef<jobject> j_scoped_manager;
  j_scoped_manager.Reset(env, j_manager);

  ScopedJavaGlobalRef<jobject> j_scoped_factory;
  j_scoped_factory.Reset(env, j_factory);

  interface_registry_->AddInterface(
      name, base::Bind(&CreateImplAndAttach, j_scoped_service_registry,
                        j_scoped_manager, j_scoped_factory),
      nullptr);
}

void ServiceRegistryAndroidImpl::RemoveService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_service_registry,
    const JavaParamRef<jstring>& j_name) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));
  interface_registry_->RemoveInterface(name);
}

void ServiceRegistryAndroidImpl::ConnectToRemoteService(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_service_registry,
    const JavaParamRef<jstring>& j_name,
    jint j_handle) {
  std::string name(ConvertJavaStringToUTF8(env, j_name));
  mojo::ScopedMessagePipeHandle handle((mojo::MessagePipeHandle(j_handle)));
  remote_interfaces_->GetInterface(name, std::move(handle));
}

}  // namespace content

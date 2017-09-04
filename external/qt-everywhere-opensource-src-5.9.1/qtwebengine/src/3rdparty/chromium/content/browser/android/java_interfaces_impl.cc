// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java_interfaces_impl.h"

#include <jni.h>

#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/memory/singleton.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/InterfaceRegistrarImpl_jni.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

class JavaInterfaceProviderHolder {
 public:
  JavaInterfaceProviderHolder() {
    service_manager::mojom::InterfaceProviderPtr provider;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InterfaceRegistrarImpl_createInterfaceRegistryForContext(
        env, mojo::GetProxy(&provider).PassMessagePipe().release().value(),
        base::android::GetApplicationContext());
    interface_provider_.Bind(std::move(provider));
  }

  static JavaInterfaceProviderHolder* GetInstance() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return base::Singleton<JavaInterfaceProviderHolder>::get();
  }

  service_manager::InterfaceProvider* GetJavaInterfaces() {
    return &interface_provider_;
  }

 private:
  service_manager::InterfaceProvider interface_provider_;
};

}  // namespace

service_manager::InterfaceProvider* GetGlobalJavaInterfaces() {
  return JavaInterfaceProviderHolder::GetInstance()->GetJavaInterfaces();
}

void BindInterfaceRegistryForWebContents(
    service_manager::mojom::InterfaceProviderRequest request,
    WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InterfaceRegistrarImpl_createInterfaceRegistryForWebContents(
      env, request.PassMessagePipe().release().value(),
      web_contents->GetJavaWebContents().obj());
}

}  // namespace content

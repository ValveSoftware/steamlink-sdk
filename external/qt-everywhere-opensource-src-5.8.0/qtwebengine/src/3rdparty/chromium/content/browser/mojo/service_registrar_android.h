// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_SERVICE_REGISTRAR_ANDROID_H_
#define CONTENT_BROWSER_MOJO_SERVICE_REGISTRAR_ANDROID_H_

#include <jni.h>

namespace content {

class ServiceRegistryAndroid;

// Registrar for mojo services in Java exposed by the browser. This calls into
// Java where the services are registered with the indicated registry.
class ServiceRegistrarAndroid {
 public:
  static bool Register(JNIEnv* env);
  static void RegisterProcessHostServices(ServiceRegistryAndroid* registry);
  static void RegisterFrameHostServices(ServiceRegistryAndroid* registry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_SERVICE_REGISTRAR_ANDROID_H_

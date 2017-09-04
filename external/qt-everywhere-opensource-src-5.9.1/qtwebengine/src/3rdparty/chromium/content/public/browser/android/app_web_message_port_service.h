// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_SERVICE_H_

#include <jni.h>
#include <vector>

#include "base/values.h"

namespace content {
class WebContents;

// The interface for AppWebMessagePortService
class AppWebMessagePortService {
 public:
  virtual ~AppWebMessagePortService() {}

  virtual void CreateMessageChannel(JNIEnv* env,
                                    jobjectArray ports,
                                    WebContents* web_contents) = 0;

  virtual void CleanupPort(int message_port_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_SERVICE_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_SETTINGS_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_SETTINGS_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class ContentSettings : public WebContentsObserver {
 public:
  ContentSettings(JNIEnv* env, jobject obj,
                  WebContents* contents);

  static bool RegisterContentSettings(JNIEnv* env);

  bool GetJavaScriptEnabled(JNIEnv* env, jobject obj);

 private:
  // Self-deletes when the underlying WebContents is destroyed.
  virtual ~ContentSettings();

  // WebContentsObserver overrides:
  virtual void WebContentsDestroyed() OVERRIDE;

  // The Java counterpart to this class.
  JavaObjectWeakGlobalRef content_settings_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_SETTINGS_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_INTERSTITIAL_PAGE_DELEGATE_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_INTERSTITIAL_PAGE_DELEGATE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/browser/interstitial_page_delegate.h"

namespace content {

class InterstitialPage;
class WebContents;

// Native counterpart that allows interstitial pages to be constructed and
// managed from Java.
class InterstitialPageDelegateAndroid : public InterstitialPageDelegate {
 public:
  InterstitialPageDelegateAndroid(JNIEnv* env,
                                  jobject obj,
                                  const std::string& html_content);
  virtual ~InterstitialPageDelegateAndroid();

  void set_interstitial_page(InterstitialPage* page) { page_ = page; }

  // Methods called from Java.
  void Proceed(JNIEnv* env, jobject obj);
  void DontProceed(JNIEnv* env, jobject obj);

  // Implementation of InterstitialPageDelegate
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;

  static bool RegisterInterstitialPageDelegateAndroid(JNIEnv* env);

 private:
  JavaObjectWeakGlobalRef weak_java_obj_;

  std::string html_content_;
  InterstitialPage* page_;  // Owns this.

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageDelegateAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_INTERSTITIAL_PAGE_DELEGATE_ANDROID_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESTRICTION_WEB_RESTRICTIONS_CLIENT_H_
#define COMPONENTS_WEB_RESTRICTION_WEB_RESTRICTIONS_CLIENT_H_

#include <jni.h>
#include <list>
#include <map>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "url/gurl.h"

namespace web_restrictions {

enum UrlAccess { ALLOW, DISALLOW, PENDING };

class WebRestrictionsClient {
 public:
  // An instance of this class is expected to live through the lifetime of a
  // browser and uses raw pointers in callbacks.
  // Any changes to the class, enable/disable/change should be done through the
  // SetAuthority(...) method.
  WebRestrictionsClient();
  ~WebRestrictionsClient();

  // Register JNI methods.
  static bool Register(JNIEnv* env);

  // Verify the content provider and query it for basic information like does it
  // support handling requests. This should be called everytime the provider
  // changes. An empty authority can be used to disable this class.
  void SetAuthority(const std::string& content_provider_authority);

  // WebRestrictionsProvider:
  UrlAccess ShouldProceed(bool is_main_frame,
                          const GURL& url,
                          const base::Callback<void(bool)>& callback);

  bool SupportsRequest() const;

  int GetResultColumnCount(const GURL& url) const;

  std::string GetResultColumnName(const GURL& url, int column) const;

  int GetResultIntValue(const GURL& url, int column) const;

  std::string GetResultStringValue(const GURL& url, int column) const;

  void RequestPermission(const GURL& url,
                         const base::Callback<void(bool)>& callback);

  void OnWebRestrictionsChanged();

 private:

  void RecordURLAccess(const GURL& url);

  void UpdateCache(std::string provider_authority,
                   GURL url,
                   base::android::ScopedJavaGlobalRef<jobject> result);

  void RequestSupportKnown(std::string provider_authority,
                           bool supports_request);

  void ClearCache();

  static base::android::ScopedJavaGlobalRef<jobject> ShouldProceedTask(
      const GURL& url,
      const base::android::JavaRef<jobject>& java_provider);

  void OnShouldProceedComplete(
      std::string provider_authority,
      const GURL& url,
      const base::Callback<void(bool)>& callback,
      const base::android::ScopedJavaGlobalRef<jobject>& result);

  // Set up after SetAuthority().
  bool initialized_;
  bool supports_request_;
  base::android::ScopedJavaGlobalRef<jobject> java_provider_;
  std::string provider_authority_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> single_thread_task_runner_;

  std::map<GURL, base::android::ScopedJavaGlobalRef<jobject>> cache_;
  std::list<GURL> recent_urls_;

  DISALLOW_COPY_AND_ASSIGN(WebRestrictionsClient);
};

}  // namespace web_restrictions

#endif  // COMPONENTS_WEB_RESTRICTION_WEB_RESTRICTIONS_CLIENT_H_

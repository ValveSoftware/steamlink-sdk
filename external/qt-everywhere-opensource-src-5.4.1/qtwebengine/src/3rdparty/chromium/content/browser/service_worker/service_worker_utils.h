// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "webkit/common/resource_type.h"

class GURL;

namespace content {

class ServiceWorkerUtils {
 public:
  static bool IsMainResourceType(ResourceType::Type type) {
    return ResourceType::IsFrame(type) ||
           ResourceType::IsSharedWorker(type);
  }

  static bool IsServiceWorkerResourceType(ResourceType::Type type) {
    return ResourceType::IsServiceWorker(type);
  }

  // Returns true if the feature is enabled (or not disabled) by command-line
  // flag.
  static bool IsFeatureEnabled();

  // A helper for creating a do-nothing status callback.
  static void NoOpStatusCallback(ServiceWorkerStatusCode status) {}

  // Returns true if |scope| matches |url|.
  CONTENT_EXPORT static bool ScopeMatches(const GURL& scope, const GURL& url);
};

class CONTENT_EXPORT LongestScopeMatcher {
 public:
  explicit LongestScopeMatcher(const GURL& url) : url_(url) {}
  virtual ~LongestScopeMatcher() {}

  // Returns true if |scope| matches |url_| longer than |match_|.
  bool MatchLongest(const GURL& scope);

 private:
  const GURL url_;
  GURL match_;

  DISALLOW_COPY_AND_ASSIGN(LongestScopeMatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_

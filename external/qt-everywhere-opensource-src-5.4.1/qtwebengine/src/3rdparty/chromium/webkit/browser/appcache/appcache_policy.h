// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_POLICY_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_POLICY_H_

class GURL;

namespace appcache {

class AppCachePolicy {
 public:
  AppCachePolicy() {}

  // Called prior to loading a main resource from the appache.
  // Returns true if allowed. This is expected to return immediately
  // without any user prompt.
  virtual bool CanLoadAppCache(const GURL& manifest_url,
                               const GURL& first_party) = 0;

  // Called prior to creating a new appcache. Returns true if allowed.
  virtual bool CanCreateAppCache(const GURL& manifest_url,
                                 const GURL& first_party) = 0;

 protected:
  ~AppCachePolicy() {}
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_POLICY_H_

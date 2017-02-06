// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_CLIENT_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "net/cookies/canonical_cookie.h"

class GURL;
class LocalSharedObjectsCounter;

namespace net {
class CookieOptions;
}

namespace content_settings {

// An abstraction of operations that depend on the embedder (e.g. Chrome).
class ContentSettingsClient {
 public:
  enum AccessType { BLOCKED, ALLOWED };

  ContentSettingsClient() {}
  virtual ~ContentSettingsClient() {}

  // Methods to notify the client about access to local shared objects.
  virtual void OnCookiesRead(const GURL& url,
                             const GURL& first_party_url,
                             const net::CookieList& cookie_list,
                             bool blocked_by_policy) = 0;

  virtual void OnCookieChanged(const GURL& url,
                               const GURL& first_party_url,
                               const std::string& cookie_line,
                               const net::CookieOptions& options,
                               bool blocked_by_policy) = 0;

  virtual void OnFileSystemAccessed(const GURL& url,
                                    bool blocked_by_policy) = 0;

  virtual void OnIndexedDBAccessed(const GURL& url,
                                   const base::string16& description,
                                   bool blocked_by_policy) = 0;

  virtual void OnLocalStorageAccessed(const GURL& url,
                                      bool local,
                                      bool blocked_by_policy) = 0;

  virtual void OnWebDatabaseAccessed(const GURL& url,
                                     const base::string16& name,
                                     const base::string16& display_name,
                                     bool blocked_by_policy) = 0;

  // Returns the |LocalSharedObjectsCounter| instances corresponding to all
  // allowed (or blocked, depending on |type|) local shared objects.
  virtual const LocalSharedObjectsCounter& local_shared_objects(
      AccessType type) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingsClient);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_CLIENT_H_

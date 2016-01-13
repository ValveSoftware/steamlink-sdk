// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_SERVICE_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_SERVICE_H_

#include <map>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/appcache/appcache_interfaces.h"

namespace appcache {

class AppCacheStorage;

// Refcounted container to avoid copying the collection in callbacks.
struct WEBKIT_STORAGE_BROWSER_EXPORT AppCacheInfoCollection
    : public base::RefCountedThreadSafe<AppCacheInfoCollection> {
  AppCacheInfoCollection();

  std::map<GURL, AppCacheInfoVector> infos_by_origin;

 private:
  friend class base::RefCountedThreadSafe<AppCacheInfoCollection>;
  virtual ~AppCacheInfoCollection();
};

// Class that manages the application cache service. Sends notifications
// to many frontends. One instance per user-profile. Each instance has
// exclusive access to its cache_directory on disk.
class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheService {
 public:
  virtual ~AppCacheService() { }

  // Determines if a request for 'url' can be satisfied while offline.
  // This method always completes asynchronously.
  virtual void CanHandleMainResourceOffline(const GURL& url,
                                            const GURL& first_party,
                                            const net::CompletionCallback&
                                            callback) = 0;

  // Populates 'collection' with info about all of the appcaches stored
  // within the service, 'callback' is invoked upon completion. The service
  // acquires a reference to the 'collection' until until completion.
  // This method always completes asynchronously.
  virtual void GetAllAppCacheInfo(AppCacheInfoCollection* collection,
                                  const net::CompletionCallback& callback) = 0;

  // Deletes the group identified by 'manifest_url', 'callback' is
  // invoked upon completion. Upon completion, the cache group and
  // any resources within the group are no longer loadable and all
  // subresource loads for pages associated with a deleted group
  // will fail. This method always completes asynchronously.
  virtual void DeleteAppCacheGroup(const GURL& manifest_url,
                                   const net::CompletionCallback& callback) = 0;
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_SERVICE_H_

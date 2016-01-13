// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_QUOTA_CLIENT_H_
#define WEBKIT_BROWSER_QUOTA_QUOTA_CLIENT_H_

#include <list>
#include <set>
#include <string>

#include "base/callback.h"
#include "url/gurl.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/quota/quota_types.h"

namespace quota {

// An abstract interface for quota manager clients.
// Each storage API must provide an implementation of this interface and
// register it to the quota manager.
// All the methods are assumed to be called on the IO thread in the browser.
class WEBKIT_STORAGE_BROWSER_EXPORT QuotaClient {
 public:
  typedef base::Callback<void(int64 usage)> GetUsageCallback;
  typedef base::Callback<void(const std::set<GURL>& origins)>
      GetOriginsCallback;
  typedef base::Callback<void(QuotaStatusCode status)> DeletionCallback;

  virtual ~QuotaClient() {}

  enum ID {
    kUnknown = 1 << 0,
    kFileSystem = 1 << 1,
    kDatabase = 1 << 2,
    kAppcache = 1 << 3,
    kIndexedDatabase = 1 << 4,
    kAllClientsMask = -1,
  };

  virtual ID id() const = 0;

  // Called when the quota manager is destroyed.
  virtual void OnQuotaManagerDestroyed() = 0;

  // Called by the QuotaManager.
  // Gets the amount of data stored in the storage specified by
  // |origin_url| and |type|.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginUsage(const GURL& origin_url,
                              StorageType type,
                              const GetUsageCallback& callback) = 0;

  // Called by the QuotaManager.
  // Returns a list of origins that has data in the |type| storage.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginsForType(StorageType type,
                                 const GetOriginsCallback& callback) = 0;

  // Called by the QuotaManager.
  // Returns a list of origins that match the |host|.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void GetOriginsForHost(StorageType type,
                                 const std::string& host,
                                 const GetOriginsCallback& callback) = 0;

  // Called by the QuotaManager.
  // Note it is safe to fire the callback after the QuotaClient is destructed.
  virtual void DeleteOriginData(const GURL& origin,
                                StorageType type,
                                const DeletionCallback& callback) = 0;

  virtual bool DoesSupport(StorageType type) const = 0;
};

// TODO(dmikurube): Replace it to std::vector for efficiency.
typedef std::list<QuotaClient*> QuotaClientList;

}  // namespace quota

#endif  // WEBKIT_BROWSER_QUOTA_QUOTA_CLIENT_H_

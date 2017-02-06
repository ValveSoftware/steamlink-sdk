// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_IDENTITY_STORE_BACKEND_H_
#define CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_IDENTITY_STORE_BACKEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

// This class represents a persistent cache of WebRTC identities.
// It can be created/destroyed/Close() on any thread. All other members should
// be accessed on the IO thread.
class WebRTCIdentityStoreBackend
    : public base::RefCountedThreadSafe<WebRTCIdentityStoreBackend> {
 public:
  typedef base::Callback<void(int error,
                              const std::string& certificate,
                              const std::string& private_key)>
      FindIdentityCallback;

  // No data is saved on disk if |path| is empty. Identites older than
  // |validity_period| will be removed lazily.
  WebRTCIdentityStoreBackend(const base::FilePath& path,
                             storage::SpecialStoragePolicy* policy,
                             base::TimeDelta validity_period);

  // Finds the identity with |origin|, |identity_name|, and |common_name| from
  // the DB.
  // |origin| is the origin of the identity;
  // |identity_name| is used to identify an identity within an origin;
  // |common_name| is the common name used to generate the certificate;
  // |callback| is the callback to return the find result.
  // Returns true if |callback| will be called.
  // Should be called on the IO thread.
  bool FindIdentity(const GURL& origin,
                    const std::string& identity_name,
                    const std::string& common_name,
                    const FindIdentityCallback& callback);

  // Adds the identity to the DB and overwrites any existing identity having the
  // same origin and identity_name.
  // |origin| is the origin of the identity;
  // |identity_name| is used to identify an identity within an origin;
  // |common_name| is the common name used to generate the certificate;
  // |certificate| is the DER string of the certificate;
  // |private_key| is the DER string of the private key.
  // Should be called on the IO thread.
  void AddIdentity(const GURL& origin,
                   const std::string& identity_name,
                   const std::string& common_name,
                   const std::string& certificate,
                   const std::string& private_key);

  // Commits all pending DB operations and closes the DB connection. Any API
  // call after this will fail.
  // Can be called on any thread.
  void Close();

  // Delete the data created between |delete_begin| and |delete_end|.
  // Should be called on the IO thread.
  void DeleteBetween(base::Time delete_begin,
                     base::Time delete_end,
                     const base::Closure& callback);

  // Changes the validity period. Should be called before the database is
  // loaded into memory.
  void SetValidityPeriodForTesting(base::TimeDelta validity_period);

 private:
  friend class base::RefCountedThreadSafe<WebRTCIdentityStoreBackend>;
  class SqlLiteStorage;
  enum LoadingState {
    NOT_STARTED,
    LOADING,
    LOADED,
    CLOSED,
  };
  struct PendingFindRequest;
  struct IdentityKey;
  struct Identity;
  typedef std::map<IdentityKey, Identity> IdentityMap;

  ~WebRTCIdentityStoreBackend();

  void OnLoaded(std::unique_ptr<IdentityMap> out_map);

  // Identities expires after |validity_period_|.
  base::TimeDelta validity_period_;
  // In-memory copy of the identities.
  IdentityMap identities_;
  // "Find identity" requests waiting for the DB to load.
  std::vector<PendingFindRequest*> pending_find_requests_;
  // The persistent storage loading state.
  LoadingState state_;
  // The persistent storage of identities.
  scoped_refptr<SqlLiteStorage> sql_lite_storage_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityStoreBackend);
};
}

#endif  // CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_IDENTITY_STORE_BACKEND_H_

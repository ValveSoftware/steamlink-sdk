// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "content/browser/geolocation/location_provider_base.h"
#include "content/browser/geolocation/network_location_request.h"
#include "content/browser/geolocation/wifi_data_provider_manager.h"
#include "content/common/content_export.h"
#include "content/public/common/geoposition.h"

namespace content {
class AccessTokenStore;


class NetworkLocationProvider
    : public base::NonThreadSafe,
      public LocationProviderBase {
 public:
  // Cache of recently resolved locations. Public for tests.
  class CONTENT_EXPORT PositionCache {
   public:
    // The maximum size of the cache of positions.
    static const size_t kMaximumSize;

    PositionCache();
    ~PositionCache();

    // Caches the current position response for the current set of cell ID and
    // WiFi data. In the case of the cache exceeding kMaximumSize this will
    // evict old entries in FIFO orderer of being added.
    // Returns true on success, false otherwise.
    bool CachePosition(const WifiData& wifi_data,
                       const Geoposition& position);

    // Searches for a cached position response for the current set of data.
    // Returns NULL if the position is not in the cache, or the cached
    // position if available. Ownership remains with the cache.
    const Geoposition* FindPosition(const WifiData& wifi_data);

   private:
    // Makes the key for the map of cached positions, using a set of
    // data. Returns true if a good key was generated, false otherwise.
    static bool MakeKey(const WifiData& wifi_data,
                        base::string16* key);

    // The cache of positions. This is stored as a map keyed on a string that
    // represents a set of data, and a list to provide
    // least-recently-added eviction.
    typedef std::map<base::string16, Geoposition> CacheMap;
    CacheMap cache_;
    typedef std::list<CacheMap::iterator> CacheAgeList;
    CacheAgeList cache_age_list_;  // Oldest first.
  };

  NetworkLocationProvider(AccessTokenStore* access_token_store,
                          net::URLRequestContextGetter* context,
                          const GURL& url,
                          const base::string16& access_token);
  ~NetworkLocationProvider() override;

  // LocationProvider implementation
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  void GetPosition(Geoposition* position) override;
  void RequestRefresh() override;
  void OnPermissionGranted() override;

 private:
  // Satisfies a position request from cache or network.
  void RequestPosition();

  // Gets called when new wifi data is available.
  void OnWifiDataUpdate();

  // Internal helper used by OnWifiDataUpdate.
  void OnWifiDataUpdated();

  bool IsStarted() const;

  void OnLocationResponse(const Geoposition& position,
                          bool server_error,
                          const base::string16& access_token,
                          const WifiData& wifi_data);

  scoped_refptr<AccessTokenStore> access_token_store_;

  // The wifi data provider, acquired via global factories.
  WifiDataProviderManager* wifi_data_provider_manager_;

  WifiDataProviderManager::WifiDataUpdateCallback wifi_data_update_callback_;

  // The  wifi data and a flag to indicate if the data set is complete.
  WifiData wifi_data_;
  bool is_wifi_data_complete_;

  // The timestamp for the latest wifi data update.
  base::Time wifi_timestamp_;

  // Cached value loaded from the token store or set by a previous server
  // response, and sent in each subsequent network request.
  base::string16 access_token_;

  // The current best position estimate.
  Geoposition position_;

  // Whether permission has been granted for the provider to operate.
  bool is_permission_granted_;

  bool is_new_data_available_;

  // The network location request object, and the url it uses.
  std::unique_ptr<NetworkLocationRequest> request_;

  // The cache of positions.
  std::unique_ptr<PositionCache> position_cache_;

  base::WeakPtrFactory<NetworkLocationProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLocationProvider);
};

// Factory functions for the various types of location provider to abstract
// over the platform-dependent implementations.
CONTENT_EXPORT LocationProviderBase* NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    net::URLRequestContextGetter* context,
    const GURL& url,
    const base::string16& access_token);

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_NETWORK_LOCATION_PROVIDER_H_

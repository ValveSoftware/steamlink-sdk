// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/location_arbitrator_impl.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/geolocation/network_location_provider.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace content {
namespace {

const char* kDefaultNetworkProviderUrl =
    "https://www.googleapis.com/geolocation/v1/geolocate";
}  // namespace

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const int64 LocationArbitratorImpl::kFixStaleTimeoutMilliseconds =
    11 * base::Time::kMillisecondsPerSecond;

LocationArbitratorImpl::LocationArbitratorImpl(
    const LocationUpdateCallback& callback)
    : callback_(callback),
      provider_callback_(
          base::Bind(&LocationArbitratorImpl::LocationUpdateAvailable,
                     base::Unretained(this))),
      position_provider_(NULL),
      is_permission_granted_(false),
      is_running_(false) {
}

LocationArbitratorImpl::~LocationArbitratorImpl() {
}

GURL LocationArbitratorImpl::DefaultNetworkProviderURL() {
  return GURL(kDefaultNetworkProviderUrl);
}

void LocationArbitratorImpl::OnPermissionGranted() {
  is_permission_granted_ = true;
  for (ScopedVector<LocationProvider>::iterator i = providers_.begin();
      i != providers_.end(); ++i) {
    (*i)->OnPermissionGranted();
  }
}

void LocationArbitratorImpl::StartProviders(bool use_high_accuracy) {
  // Stash options as OnAccessTokenStoresLoaded has not yet been called.
  is_running_ = true;
  use_high_accuracy_ = use_high_accuracy;
  if (providers_.empty()) {
    DCHECK(DefaultNetworkProviderURL().is_valid());
    GetAccessTokenStore()->LoadAccessTokens(
        base::Bind(&LocationArbitratorImpl::OnAccessTokenStoresLoaded,
                   base::Unretained(this)));
  } else {
    DoStartProviders();
  }
}

void LocationArbitratorImpl::DoStartProviders() {
  for (ScopedVector<LocationProvider>::iterator i = providers_.begin();
       i != providers_.end(); ++i) {
    (*i)->StartProvider(use_high_accuracy_);
  }
}

void LocationArbitratorImpl::StopProviders() {
  // Reset the reference location state (provider+position)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  position_provider_ = NULL;
  position_ = Geoposition();

  providers_.clear();
  is_running_ = false;
}

void LocationArbitratorImpl::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenSet access_token_set,
    net::URLRequestContextGetter* context_getter) {
  if (!is_running_ || !providers_.empty()) {
    // A second StartProviders() call may have arrived before the first
    // completed.
    return;
  }
  // If there are no access tokens, boot strap it with the default server URL.
  if (access_token_set.empty())
    access_token_set[DefaultNetworkProviderURL()];
  for (AccessTokenStore::AccessTokenSet::iterator i =
           access_token_set.begin();
      i != access_token_set.end(); ++i) {
    RegisterProvider(
        NewNetworkLocationProvider(
            GetAccessTokenStore(), context_getter,
            i->first, i->second));
  }

  LocationProvider* provider =
      GetContentClient()->browser()->OverrideSystemLocationProvider();
  if (!provider)
    provider = NewSystemLocationProvider();
  RegisterProvider(provider);
  DoStartProviders();
}

void LocationArbitratorImpl::RegisterProvider(
    LocationProvider* provider) {
  if (!provider)
    return;
  provider->SetUpdateCallback(provider_callback_);
  if (is_permission_granted_)
    provider->OnPermissionGranted();
  providers_.push_back(provider);
}

void LocationArbitratorImpl::LocationUpdateAvailable(
    const LocationProvider* provider,
    const Geoposition& new_position) {
  DCHECK(new_position.Validate() ||
         new_position.error_code != Geoposition::ERROR_CODE_NONE);
  providers_polled_.insert(provider);
  if (IsNewPositionBetter(position_, new_position,
                          provider == position_provider_)) {
      position_provider_ = provider;
      position_ = new_position;
  }
  // Don't fail until all providers had their say.
  if (position_.Validate() || (providers_polled_.size() == providers_.size()))
      callback_.Run(position_);
}

AccessTokenStore* LocationArbitratorImpl::NewAccessTokenStore() {
  return GetContentClient()->browser()->CreateAccessTokenStore();
}

AccessTokenStore* LocationArbitratorImpl::GetAccessTokenStore() {
  if (!access_token_store_.get())
    access_token_store_ = NewAccessTokenStore();
  return access_token_store_.get();
}

LocationProvider* LocationArbitratorImpl::NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    net::URLRequestContextGetter* context,
    const GURL& url,
    const base::string16& access_token) {
#if defined(OS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return NULL;
#else
  return new NetworkLocationProvider(access_token_store, context, url,
                                     access_token);
#endif
}

LocationProvider* LocationArbitratorImpl::NewSystemLocationProvider() {
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  return NULL;
#else
  return content::NewSystemLocationProvider();
#endif
}

base::Time LocationArbitratorImpl::GetTimeNow() const {
  return base::Time::Now();
}

bool LocationArbitratorImpl::IsNewPositionBetter(
    const Geoposition& old_position, const Geoposition& new_position,
    bool from_same_provider) const {
  // Updates location_info if it's better than what we currently have,
  // or if it's a newer update from the same provider.
  if (!old_position.Validate()) {
    // Older location wasn't locked.
    return true;
  }
  if (new_position.Validate()) {
    // New location is locked, let's check if it's any better.
    if (old_position.accuracy >= new_position.accuracy) {
      // Accuracy is better.
      return true;
    } else if (from_same_provider) {
      // Same provider, fresher location.
      return true;
    } else if ((GetTimeNow() - old_position.timestamp).InMilliseconds() >
               kFixStaleTimeoutMilliseconds) {
      // Existing fix is stale.
      return true;
    }
  }
  return false;
}

bool LocationArbitratorImpl::HasPermissionBeenGranted() const {
  return is_permission_granted_;
}

}  // namespace content

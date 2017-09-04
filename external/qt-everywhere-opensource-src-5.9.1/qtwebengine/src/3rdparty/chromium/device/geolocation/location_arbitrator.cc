// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/location_arbitrator.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "device/geolocation/access_token_store.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/network_location_provider.h"
#include "url/gurl.h"

namespace device {
namespace {

const char* kDefaultNetworkProviderUrl =
    "https://www.googleapis.com/geolocation/v1/geolocate";
}  // namespace

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const int64_t LocationArbitrator::kFixStaleTimeoutMilliseconds =
    11 * base::Time::kMillisecondsPerSecond;

LocationArbitrator::LocationArbitrator(
    std::unique_ptr<GeolocationDelegate> delegate)
    : delegate_(std::move(delegate)),
      position_provider_(nullptr),
      is_permission_granted_(false),
      is_running_(false) {}

LocationArbitrator::~LocationArbitrator() {}

GURL LocationArbitrator::DefaultNetworkProviderURL() {
  return GURL(kDefaultNetworkProviderUrl);
}

bool LocationArbitrator::HasPermissionBeenGrantedForTest() const {
  return is_permission_granted_;
}

void LocationArbitrator::OnPermissionGranted() {
  is_permission_granted_ = true;
  for (const auto& provider : providers_)
    provider->OnPermissionGranted();
}

bool LocationArbitrator::StartProvider(bool enable_high_accuracy) {
  // Stash options as OnAccessTokenStoresLoaded has not yet been called.
  is_running_ = true;
  enable_high_accuracy_ = enable_high_accuracy;

  if (providers_.empty()) {
    RegisterSystemProvider();

    const scoped_refptr<AccessTokenStore> access_token_store =
        GetAccessTokenStore();
    if (access_token_store && delegate_->UseNetworkLocationProviders()) {
      DCHECK(DefaultNetworkProviderURL().is_valid());
      token_store_callback_.Reset(
          base::Bind(&LocationArbitrator::OnAccessTokenStoresLoaded,
                     base::Unretained(this)));
      access_token_store->LoadAccessTokens(token_store_callback_.callback());
      return true;
    }
  }
  return DoStartProviders();
}

bool LocationArbitrator::DoStartProviders() {
  if (providers_.empty()) {
    // If no providers are available, we report an error to avoid
    // callers waiting indefinitely for a reply.
    Geoposition position;
    position.error_code = Geoposition::ERROR_CODE_PERMISSION_DENIED;
    arbitrator_update_callback_.Run(this, position);
    return false;
  }
  bool started = false;
  for (const auto& provider : providers_) {
    started = provider->StartProvider(enable_high_accuracy_) || started;
  }
  return started;
}

void LocationArbitrator::StopProvider() {
  // Reset the reference location state (provider+position)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  position_provider_ = nullptr;
  position_ = Geoposition();

  providers_.clear();
  is_running_ = false;
}

void LocationArbitrator::OnAccessTokenStoresLoaded(
    AccessTokenStore::AccessTokenMap access_token_map,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter) {
  // If there are no access tokens, boot strap it with the default server URL.
  if (access_token_map.empty())
    access_token_map[DefaultNetworkProviderURL()];
  for (const auto& entry : access_token_map) {
    RegisterProvider(NewNetworkLocationProvider(
        GetAccessTokenStore(), context_getter, entry.first, entry.second));
  }
  DoStartProviders();
}

void LocationArbitrator::RegisterProvider(
    std::unique_ptr<LocationProvider> provider) {
  if (!provider)
    return;
  provider->SetUpdateCallback(base::Bind(&LocationArbitrator::OnLocationUpdate,
                                         base::Unretained(this)));
  if (is_permission_granted_)
    provider->OnPermissionGranted();
  providers_.push_back(std::move(provider));
}

void LocationArbitrator::RegisterSystemProvider() {
  std::unique_ptr<LocationProvider> provider =
      delegate_->OverrideSystemLocationProvider();
  if (!provider)
    provider = NewSystemLocationProvider();
  RegisterProvider(std::move(provider));
}

void LocationArbitrator::OnLocationUpdate(const LocationProvider* provider,
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
      arbitrator_update_callback_.Run(this, position_);
}

const Geoposition& LocationArbitrator::GetPosition() {
  return position_;
}

void LocationArbitrator::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK(!callback.is_null());
  arbitrator_update_callback_ = callback;
}

scoped_refptr<AccessTokenStore> LocationArbitrator::NewAccessTokenStore() {
  return delegate_->CreateAccessTokenStore();
}

scoped_refptr<AccessTokenStore> LocationArbitrator::GetAccessTokenStore() {
  if (!access_token_store_)
    access_token_store_ = NewAccessTokenStore();
  return access_token_store_;
}

std::unique_ptr<LocationProvider>
LocationArbitrator::NewNetworkLocationProvider(
    const scoped_refptr<AccessTokenStore>& access_token_store,
    const scoped_refptr<net::URLRequestContextGetter>& context,
    const GURL& url,
    const base::string16& access_token) {
#if defined(OS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return nullptr;
#else
  return base::MakeUnique<NetworkLocationProvider>(access_token_store, context,
                                                   url, access_token);
#endif
}

std::unique_ptr<LocationProvider>
LocationArbitrator::NewSystemLocationProvider() {
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  return nullptr;
#else
  return device::NewSystemLocationProvider();
#endif
}

base::Time LocationArbitrator::GetTimeNow() const {
  return base::Time::Now();
}

bool LocationArbitrator::IsNewPositionBetter(const Geoposition& old_position,
                                             const Geoposition& new_position,
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

}  // namespace device

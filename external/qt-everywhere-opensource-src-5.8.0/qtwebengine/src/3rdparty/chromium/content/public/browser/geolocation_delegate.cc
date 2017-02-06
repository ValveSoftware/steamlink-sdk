// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/geolocation_delegate.h"

namespace content {

bool GeolocationDelegate::UseNetworkLocationProviders() {
  return true;
}

AccessTokenStore* GeolocationDelegate::CreateAccessTokenStore() {
  return nullptr;
}

LocationProvider* GeolocationDelegate::OverrideSystemLocationProvider() {
  return nullptr;
}

}  // namespace content

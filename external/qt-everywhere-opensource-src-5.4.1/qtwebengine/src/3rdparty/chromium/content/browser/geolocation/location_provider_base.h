// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_

#include "content/common/content_export.h"
#include "content/public/browser/location_provider.h"

namespace content {

class CONTENT_EXPORT LocationProviderBase
    : NON_EXPORTED_BASE(public LocationProvider) {
 public:
  LocationProviderBase();
  virtual ~LocationProviderBase();

 protected:
  void NotifyCallback(const Geoposition& position);

  // Overridden from LocationProvider:
  virtual void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) OVERRIDE;
  virtual void RequestRefresh() OVERRIDE;

 private:
  LocationProviderUpdateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(LocationProviderBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_

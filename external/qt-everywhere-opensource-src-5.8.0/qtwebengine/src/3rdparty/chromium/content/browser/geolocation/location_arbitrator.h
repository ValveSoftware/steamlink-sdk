// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_

#include "content/common/content_export.h"

namespace content {

// This class is responsible for handling updates from multiple underlying
// providers and resolving them to a single 'best' location fix at any given
// moment.
class CONTENT_EXPORT LocationArbitrator {
public:
  virtual ~LocationArbitrator() {};

  // See more details in geolocation_provider.
  virtual void StartProviders(bool enable_high_accuracy) = 0;
  virtual void StopProviders() = 0;

  // Called everytime permission is granted to a page for using geolocation.
  // This may either be through explicit user action (e.g. responding to the
  // infobar prompt) or inferred from a persisted site permission.
  // The arbitrator will inform all providers of this, which may in turn use
  // this information to modify their internal policy.
  virtual void OnPermissionGranted() = 0;

  // Returns true if this arbitrator has received at least one call to
  // OnPermissionGranted().
  virtual bool HasPermissionBeenGranted() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_ARBITRATOR_H_

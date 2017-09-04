// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_H_
#define COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_H_

#include <memory>

namespace base {
class ListValue;
}

class PhysicalWebListener;

// Dictionary keys for reading Physical Web URL metadata.
extern const char kPhysicalWebDescriptionKey[];
extern const char kPhysicalWebDistanceEstimateKey[];
extern const char kPhysicalWebGroupIdKey[];
extern const char kPhysicalWebIconUrlKey[];
extern const char kPhysicalWebResolvedUrlKey[];
extern const char kPhysicalWebScanTimestampKey[];
extern const char kPhysicalWebScannedUrlKey[];
extern const char kPhysicalWebTitleKey[];

// Helper class for accessing Physical Web metadata and controlling the scanner.
class PhysicalWebDataSource {
 public:
  virtual ~PhysicalWebDataSource() {}

  // Starts scanning for Physical Web URLs. If |network_request_enabled| is
  // true, discovered URLs will be sent to a resolution service.
  virtual void StartDiscovery(bool network_request_enabled) = 0;

  // Stops scanning for Physical Web URLs and clears cached URL content.
  virtual void StopDiscovery() = 0;

  // Returns a list of resolved URLs and associated page metadata. If network
  // requests are disabled or if discovery is not active, the list will be
  // empty. The method can be called at any time to receive the current metadata
  // list.
  virtual std::unique_ptr<base::ListValue> GetMetadata() = 0;

  // Returns boolean |true| if network requests are disabled and there are one
  // or more discovered URLs that have not been sent to the resolution service.
  // The method can be called at any time to check for unresolved discoveries.
  // If discovery is inactive or network requests are enabled, it will always
  // return false.
  virtual bool HasUnresolvedDiscoveries() = 0;

  // Register for changes to Physical Web URLs and associated page metadata.
  virtual void RegisterListener(PhysicalWebListener* physical_web_listener) = 0;

  // Unregister for changes to Physical Web URLs and associated page metadata.
  virtual void UnregisterListener(
      PhysicalWebListener* physical_web_listener) = 0;
};

#endif  // COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_H_

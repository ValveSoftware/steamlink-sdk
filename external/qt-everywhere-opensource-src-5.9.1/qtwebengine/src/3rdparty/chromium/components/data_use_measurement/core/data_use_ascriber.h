// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_

#include <stdint.h>

#include <memory>

#include "components/data_use_measurement/core/data_use_measurement.h"
#include "components/metrics/data_use_tracker.h"
#include "url/gurl.h"

namespace net {
class NetworkDelegate;
class URLRequest;
}

namespace data_use_measurement {

class DataUseRecorder;
class URLRequestClassifier;

// Abstract class that manages instances of DataUseRecorder and maps
// a URLRequest instance to its appropriate DataUseRecorder. An embedder
// should provide an override if it is interested in tracking data usage. Data
// use from all URLRequests mapped to the same DataUseRecorder will be grouped
// together and reported as a single use.
class DataUseAscriber {
 public:
  virtual ~DataUseAscriber() {}

  // Creates a network delegate that will be used to track data use.
  std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate,
      const metrics::UpdateUsagePrefCallbackType& metrics_data_use_forwarder);

  // Returns the DataUseRecorder to which data usage for the given URL should
  // be ascribed. If no existing DataUseRecorder exists, a new one will be
  // created.
  virtual DataUseRecorder* GetDataUseRecorder(net::URLRequest* request) = 0;

  // Methods called by DataUseNetworkDelegate to propagate data use information:
  virtual void OnBeforeUrlRequest(net::URLRequest* request);

  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location);

  virtual void OnNetworkBytesSent(net::URLRequest* request, int64_t bytes_sent);

  virtual void OnNetworkBytesReceived(net::URLRequest* request,
                                      int64_t bytes_received);

  virtual void OnUrlRequestDestroyed(net::URLRequest* request);

  virtual std::unique_ptr<URLRequestClassifier> CreateURLRequestClassifier()
      const = 0;
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_

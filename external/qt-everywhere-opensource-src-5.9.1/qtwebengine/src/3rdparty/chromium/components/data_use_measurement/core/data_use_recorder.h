// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_RECORDER_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_RECORDER_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/data_use_measurement/core/data_use.h"
#include "net/base/net_export.h"

namespace net {
class URLRequest;
}

namespace data_use_measurement {

// Tracks all network data used by a single high level entity. An entity
// can make multiple URLRequests, so there is a one:many relationship between
// DataUseRecorders and URLRequests. Data used by each URLRequest will be
// tracked by exactly one DataUseRecorder.
class DataUseRecorder {
 public:
  DataUseRecorder();
  ~DataUseRecorder();

  // Returns the actual data used by the entity being tracked.
  DataUse& data_use() { return data_use_; }

  // Returns whether data use is complete and no additional data can be used
  // by the entity tracked by this recorder. For example,
  bool IsDataUseComplete();

  // Methods for tracking data use sources. These sources can initiate
  // URLRequests directly or indirectly. The entity whose data use is being
  // tracked by this recorder may comprise of sub-entities each of which use
  // network data. These helper methods help track these sub-entities.
  // A recorder will not be marked as having completed data use as long as it
  // has pending data sources.
  void AddPendingDataSource(void* source);
  bool HasPendingDataSource(void* source);
  void RemovePendingDataSource(void* source);

  // Returns whether there are any pending URLRequests whose data use is tracked
  // by this DataUseRecorder.
  bool HasPendingURLRequest(const net::URLRequest* request);

  // Method to merge another DataUseRecorder to this instance.
  void MergeWith(DataUseRecorder* other);

 private:
  friend class DataUseAscriber;

  // Network Delegate methods:
  void OnBeforeUrlRequest(net::URLRequest* request);
  void OnUrlRequestDestroyed(net::URLRequest* request);
  void OnNetworkBytesSent(net::URLRequest* request, int64_t bytes_sent);
  void OnNetworkBytesReceived(net::URLRequest* request, int64_t bytes_received);

  // Pending URLRequests whose data is being tracked by this DataUseRecorder.
  base::hash_set<const net::URLRequest*> pending_url_requests_;

  // Data sources other than URLRequests, whose data is being tracked by this
  // DataUseRecorder.
  base::hash_set<const void*> pending_data_sources_;

  // The network data use measured by this DataUseRecorder.
  DataUse data_use_;

  DISALLOW_COPY_AND_ASSIGN(DataUseRecorder);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_RECORDER_H_

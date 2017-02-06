// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_CONNECTION_STATISTICS_H_
#define BLIMP_NET_BLIMP_CONNECTION_STATISTICS_H_

#include <map>
#include "base/macros.h"
#include "base/metrics/sparse_histogram.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

// Collects network traffic statistics. Maintains a counter for number of
// completed commits, bytes sent and received over network and notifies its
// observers on the main thread.
// This class is supposed to live an entire session, created and destroyed along
// it. This class is thread-safe.
class BLIMP_NET_EXPORT BlimpConnectionStatistics {
 public:
  enum EventType {
    BYTES_SENT = 0,
    BYTES_RECEIVED = 1,
    COMMIT = 2,
  };

  BlimpConnectionStatistics();

  ~BlimpConnectionStatistics();

  // Accumulates values for a metric.
  void Add(EventType type, int data);

  // Returns value for a metric.
  int Get(EventType type);

 private:
  // A histogram to hold the metrics. It will have one sample for each type of
  // metric which will be accumulated on each subsequent call. Histograms are
  // intentionally leaked at the process termination.
  base::HistogramBase* histogram_;

  DISALLOW_COPY_AND_ASSIGN(BlimpConnectionStatistics);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_CONNECTION_STATISTICS_H_

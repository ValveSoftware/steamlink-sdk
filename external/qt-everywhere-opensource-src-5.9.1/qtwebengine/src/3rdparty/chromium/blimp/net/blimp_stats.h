// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_STATS_H_
#define BLIMP_NET_BLIMP_STATS_H_

#include <memory>
#include <vector>

#include "base/atomicops.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

// Maintains counters for completed commits, network bytes sent and
// network bytes received. Counters increase monotonically over the
// lifetime of the process.
// Class is thread-safe. Individual metrics may be safely modified or retrieved
// concurrently.
class BLIMP_NET_EXPORT BlimpStats {
 public:
  enum EventType {
    BYTES_SENT,
    BYTES_RECEIVED,
    COMMIT,
    EVENT_TYPE_MAX = COMMIT,
  };

  static BlimpStats* GetInstance();

  // Increments the metric |type| by |amount|. |amount| must be a positive
  // value.
  void Add(EventType type, base::subtle::Atomic32 amount);

  // Returns value for the metric |type|.
  base::subtle::Atomic32 Get(EventType type) const;

 private:
  friend struct base::DefaultLazyInstanceTraits<BlimpStats>;

  static base::LazyInstance<BlimpStats> instance_;

  BlimpStats();
  ~BlimpStats();

  // Values must be read or modified using base::subtle::NoBarrier* functions.
  // We are using the NoBarrier* functions because we value performance over
  // accuracy of the data.
  base::subtle::Atomic32 values_[EVENT_TYPE_MAX + 1];

  DISALLOW_COPY_AND_ASSIGN(BlimpStats);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_STATS_H_

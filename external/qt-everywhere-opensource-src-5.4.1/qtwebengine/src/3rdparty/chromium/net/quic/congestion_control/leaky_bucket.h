// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class to track the rate data can leave the buffer for pacing.
// A leaky bucket drains the data at a constant rate regardless of fullness of
// the buffer.
// See http://en.wikipedia.org/wiki/Leaky_bucket for more details.

#ifndef NET_QUIC_CONGESTION_CONTROL_LEAKY_BUCKET_H_
#define NET_QUIC_CONGESTION_CONTROL_LEAKY_BUCKET_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/quic/quic_bandwidth.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

class NET_EXPORT_PRIVATE LeakyBucket {
 public:
  explicit LeakyBucket(QuicBandwidth draining_rate);

  // Set the rate at which the bytes leave the buffer.
  void SetDrainingRate(QuicTime now, QuicBandwidth draining_rate);

  // Add data to the buffer.
  void Add(QuicTime now, QuicByteCount bytes);

  // Time until the buffer is empty.
  QuicTime::Delta TimeRemaining(QuicTime now) const;

  // Number of bytes in the buffer.
  QuicByteCount BytesPending(QuicTime now);

 private:
  void Update(QuicTime now);

  QuicByteCount bytes_;
  QuicTime time_last_updated_;
  QuicBandwidth draining_rate_;

  DISALLOW_COPY_AND_ASSIGN(LeakyBucket);
};

}  // namespace net

#endif  // NET_QUIC_CONGESTION_CONTROL_LEAKY_BUCKET_H_

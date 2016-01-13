// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_TIMESTAMP_HELPER_H_
#define MEDIA_CAST_RTP_TIMESTAMP_HELPER_H_

#include "base/basictypes.h"
#include "base/time/time.h"

namespace media {
namespace cast {

// A helper class used to convert current time ticks into RTP timestamp.
class RtpTimestampHelper {
 public:
  explicit RtpTimestampHelper(int frequency);
  ~RtpTimestampHelper();

  // Compute a RTP timestamp using current time, last encoded time and
  // last encoded RTP timestamp.
  // Return true if |rtp_timestamp| is computed.
  bool GetCurrentTimeAsRtpTimestamp(const base::TimeTicks& now,
                                    uint32* rtp_timestamp) const;

  // Store the capture time and the corresponding RTP timestamp for the
  // last encoded frame.
  void StoreLatestTime(base::TimeTicks capture_time, uint32 rtp_timestamp);

 private:
  int frequency_;
  base::TimeTicks last_capture_time_;
  uint32 last_rtp_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(RtpTimestampHelper);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_DEFINES_H_

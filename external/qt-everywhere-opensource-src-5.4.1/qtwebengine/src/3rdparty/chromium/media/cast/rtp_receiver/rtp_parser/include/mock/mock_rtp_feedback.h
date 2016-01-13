// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_INCLUDE_MOCK_RTP_FEEDBACK_H_
#define MEDIA_CAST_RTP_INCLUDE_MOCK_RTP_FEEDBACK_H_

#include "media/cast/rtp_receiver/rtp_parser/rtp_feedback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockRtpFeedback : public RtpFeedback {
 public:
  MOCK_METHOD4(OnInitializeDecoder,
               int32(const int8 payloadType,
                     const int frequency,
                     const uint8 channels,
                     const uint32 rate));

  MOCK_METHOD1(OnPacketTimeout, void(const int32 id));
  MOCK_METHOD2(OnReceivedPacket,
               void(const int32 id, const RtpRtcpPacketField packet_type));
  MOCK_METHOD2(OnPeriodicDeadOrAlive,
               void(const int32 id, const RTPAliveType alive));
  MOCK_METHOD2(OnIncomingSSRCChanged, void(const int32 id, const uint32 ssrc));
  MOCK_METHOD3(OnIncomingCSRCChanged,
               void(const int32 id, const uint32 csrc, const bool added));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_INCLUDE_MOCK_RTP_FEEDBACK_H_

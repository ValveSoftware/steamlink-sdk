// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_RTCP_RTCP_BUILDER_H_
#define MEDIA_CAST_TRANSPORT_RTCP_RTCP_BUILDER_H_

#include <list>
#include <string>
#include <vector>

#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

class RtcpBuilder {
 public:
  explicit RtcpBuilder(PacedSender* const paced_packet_sender);

  virtual ~RtcpBuilder();

  void SendRtcpFromRtpSender(uint32 packet_type_flags,
                             const RtcpSenderInfo& sender_info,
                             const RtcpDlrrReportBlock& dlrr,
                             uint32 ssrc,
                             const std::string& c_name);

 private:
  bool BuildSR(const RtcpSenderInfo& sender_info, Packet* packet) const;
  bool BuildSdec(Packet* packet) const;
  bool BuildBye(Packet* packet) const;
  bool BuildDlrrRb(const RtcpDlrrReportBlock& dlrr,
                   Packet* packet) const;

  PacedSender* const transport_;  // Not owned by this class.
  uint32 ssrc_;
  std::string c_name_;

  DISALLOW_COPY_AND_ASSIGN(RtcpBuilder);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_RTCP_RTCP_BUILDER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the main interface for the cast transport sender.  It accepts encoded
// frames (both audio and video), encrypts their encoded data, packetizes them
// and feeds them into a transport (e.g., UDP).

// Construction of the Cast Sender and the Cast Transport Sender should be done
// in the following order:
// 1. Create CastTransportSender.
// 2. Create CastSender (accepts CastTransportSender as an input).
// 3. Call CastTransportSender::SetPacketReceiver to ensure that the packets
//    received by the CastTransportSender will be sent to the CastSender.
// Steps 3 can be done interchangeably.

// Destruction: The CastTransportSender is assumed to be valid as long as the
// CastSender is alive. Therefore the CastSender should be destructed before the
// CastTransportSender.
// This also works when the CastSender acts as a receiver for the RTCP packets
// due to the weak pointers in the ReceivedPacket method in cast_sender_impl.cc.

#ifndef MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_SENDER_H_
#define MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_SENDER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace net {
class NetLog;
}  // namespace net

namespace media {
namespace cast {
namespace transport {

// Following the initialization of either audio or video an initialization
// status will be sent via this callback.
typedef base::Callback<void(CastTransportStatus status)>
    CastTransportStatusCallback;

typedef base::Callback<void(const std::vector<PacketEvent>&)>
    BulkRawEventsCallback;

// The application should only trigger this class from the transport thread.
class CastTransportSender : public base::NonThreadSafe {
 public:
  static scoped_ptr<CastTransportSender> Create(
      net::NetLog* net_log,
      base::TickClock* clock,
      const net::IPEndPoint& remote_end_point,
      const CastTransportStatusCallback& status_callback,
      const BulkRawEventsCallback& raw_events_callback,
      base::TimeDelta raw_events_callback_interval,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner);

  virtual ~CastTransportSender() {}

  // Audio/Video initialization.
  // Encoded frames cannot be transmitted until the relevant initialize method
  // is called. Usually called by CastSender.
  virtual void InitializeAudio(const CastTransportAudioConfig& config) = 0;

  virtual void InitializeVideo(const CastTransportVideoConfig& config) = 0;

  // Sets the Cast packet receiver. Should be called after creation on the
  // Cast sender. Packets won't be received until this function is called.
  virtual void SetPacketReceiver(
      const PacketReceiverCallback& packet_receiver) = 0;

  // The following two functions handle the encoded media frames (audio and
  // video) to be processed.
  // Frames will be encrypted, packetized and transmitted to the network.
  virtual void InsertCodedAudioFrame(const EncodedFrame& audio_frame) = 0;
  virtual void InsertCodedVideoFrame(const EncodedFrame& video_frame) = 0;

  // Builds an RTCP packet and sends it to the network.
  // |ntp_seconds|, |ntp_fraction| and |rtp_timestamp| are used in the
  // RTCP Sender Report.
  virtual void SendRtcpFromRtpSender(uint32 packet_type_flags,
                                     uint32 ntp_seconds,
                                     uint32 ntp_fraction,
                                     uint32 rtp_timestamp,
                                     const RtcpDlrrReportBlock& dlrr,
                                     uint32 sending_ssrc,
                                     const std::string& c_name) = 0;

  // Retransmission request.
  // |missing_packets| includes the list of frames and packets in each
  // frame to be re-transmitted.
  // If |cancel_rtx_if_not_in_list| is used as an optimization to cancel
  // pending re-transmission requests of packets not listed in
  // |missing_packets|. If the requested packet(s) were sent recently
  // (how long is specified by |dedupe_window|) then this re-transmit
  // will be ignored.
  virtual void ResendPackets(
      bool is_audio,
      const MissingFramesAndPacketsMap& missing_packets,
      bool cancel_rtx_if_not_in_list,
      base::TimeDelta dedupe_window) = 0;
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_SENDER_H_

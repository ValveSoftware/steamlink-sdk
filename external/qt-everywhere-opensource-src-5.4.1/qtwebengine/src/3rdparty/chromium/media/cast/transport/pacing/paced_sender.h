// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_PACING_PACED_SENDER_H_
#define MEDIA_CAST_TRANSPORT_PACING_PACED_SENDER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/transport/udp_transport.h"

namespace media {
namespace cast {

class LoggingImpl;

namespace transport {

// Use std::pair for free comparison operators.
// { capture_time, ssrc, packet_id }
// The PacketKey is designed to meet two criteria:
// 1. When we re-send the same packet again, we can use the packet key
//    to identify it so that we can de-duplicate packets in the queue.
// 2. The sort order of the PacketKey determines the order that packets
//    are sent out. Using the capture_time as the first member basically
//    means that older packets are sent first.
typedef std::pair<base::TimeTicks, std::pair<uint32, uint16> > PacketKey;
typedef std::vector<std::pair<PacketKey, PacketRef> > SendPacketVector;

// We have this pure virtual class to enable mocking.
class PacedPacketSender {
 public:
  virtual bool SendPackets(const SendPacketVector& packets) = 0;
  virtual bool ResendPackets(const SendPacketVector& packets,
                             base::TimeDelta dedupe_window) = 0;
  virtual bool SendRtcpPacket(uint32 ssrc, PacketRef packet) = 0;
  virtual void CancelSendingPacket(const PacketKey& packet_key) = 0;

  virtual ~PacedPacketSender() {}

  static PacketKey MakePacketKey(const base::TimeTicks& ticks,
                                 uint32 ssrc,
                                 uint16 packet_id);
};

class PacedSender : public PacedPacketSender,
                    public base::NonThreadSafe,
                    public base::SupportsWeakPtr<PacedSender> {
 public:
  // The |external_transport| should only be used by the Cast receiver and for
  // testing.
  PacedSender(
      base::TickClock* clock,
      LoggingImpl* logging,
      PacketSender* external_transport,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner);

  virtual ~PacedSender();

  // These must be called before non-RTCP packets are sent.
  void RegisterAudioSsrc(uint32 audio_ssrc);
  void RegisterVideoSsrc(uint32 video_ssrc);

  // PacedPacketSender implementation.
  virtual bool SendPackets(const SendPacketVector& packets) OVERRIDE;
  virtual bool ResendPackets(const SendPacketVector& packets,
                             base::TimeDelta dedupe_window) OVERRIDE;
  virtual bool SendRtcpPacket(uint32 ssrc, PacketRef packet) OVERRIDE;
  virtual void CancelSendingPacket(const PacketKey& packet_key) OVERRIDE;

 private:
  // Actually sends the packets to the transport.
  void SendStoredPackets();
  void LogPacketEvent(const Packet& packet, CastLoggingEvent event);

  enum PacketType {
    PacketType_RTCP,
    PacketType_Resend,
    PacketType_Normal
  };
  enum State {
    // In an unblocked state, we can send more packets.
    // We have to check the current time against |burst_end_| to see if we are
    // appending to the current burst or if we can start a new one.
    State_Unblocked,
    // In this state, we are waiting for a callback from the udp transport.
    // This happens when the OS-level buffer is full. Once we receive the
    // callback, we go to State_Unblocked and see if we can write more packets
    // to the current burst. (Or the next burst if enough time has passed.)
    State_TransportBlocked,
    // Once we've written enough packets for a time slice, we go into this
    // state and PostDelayTask a call to ourselves to wake up when we can
    // send more data.
    State_BurstFull
  };

  bool empty() const;
  size_t size() const;

  // Returns the next packet to send. RTCP packets have highest priority,
  // resend packets have second highest priority and then comes everything
  // else.
  PacketRef GetNextPacket(PacketType* packet_type,
                          PacketKey* packet_key);

  base::TickClock* const clock_;  // Not owned by this class.
  LoggingImpl* const logging_;    // Not owned by this class.
  PacketSender* transport_;       // Not owned by this class.
  scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;
  uint32 audio_ssrc_;
  uint32 video_ssrc_;
  std::map<PacketKey, std::pair<PacketType, PacketRef> > packet_list_;
  std::map<PacketKey, base::TimeTicks> sent_time_;
  std::map<PacketKey, base::TimeTicks> sent_time_buffer_;

  // Maximum burst size for the next three bursts.
  size_t max_burst_size_;
  size_t next_max_burst_size_;
  size_t next_next_max_burst_size_;
  // Number of packets already sent in the current burst.
  size_t current_burst_size_;
  // This is when the current burst ends.
  base::TimeTicks burst_end_;

  State state_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PacedSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_PACING_PACED_SENDER_H_

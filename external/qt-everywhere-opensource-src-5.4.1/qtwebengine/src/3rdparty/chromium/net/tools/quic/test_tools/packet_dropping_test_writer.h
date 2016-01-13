// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_

#include <list>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "net/quic/quic_alarm.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_epoll_clock.h"
#include "net/tools/quic/quic_packet_writer_wrapper.h"
#include "net/tools/quic/test_tools/quic_test_client.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"

namespace net {
namespace tools {
namespace test {

// Simulates a connection that drops packets a configured percentage of the time
// and has a blocked socket a configured percentage of the time.  Also provides
// the options to delay packets and reorder packets if delay is enabled.
class PacketDroppingTestWriter : public QuicPacketWriterWrapper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnCanWrite() = 0;
  };

  PacketDroppingTestWriter();

  virtual ~PacketDroppingTestWriter();

  // Must be called before blocking, reordering or delaying (loss is OK). May be
  // called after connecting if the helper is not available before.
  // |on_can_write| will be triggered when fake-unblocking; ownership will be
  // assumed.
  void Initialize(QuicEpollConnectionHelper* helper, Delegate* on_can_write);

  // QuicPacketWriter methods:
  virtual WriteResult WritePacket(
      const char* buffer,
      size_t buf_len,
      const IPAddressNumber& self_address,
      const IPEndPoint& peer_address) OVERRIDE;

  virtual bool IsWriteBlocked() const OVERRIDE;

  virtual void SetWritable() OVERRIDE;

  // Writes out any packet which should have been sent by now
  // to the contained writer and returns the time
  // for the next delayed packet to be written.
  QuicTime ReleaseOldPackets();

  void OnCanWrite();

  // The percent of time a packet is simulated as being lost.
  void set_fake_packet_loss_percentage(int32 fake_packet_loss_percentage) {
    base::AutoLock locked(config_mutex_);
    fake_packet_loss_percentage_ = fake_packet_loss_percentage;
  }

  // The percent of time WritePacket will block and set WriteResult's status
  // to WRITE_STATUS_BLOCKED.
  void set_fake_blocked_socket_percentage(
      int32 fake_blocked_socket_percentage) {
    DCHECK(clock_);
    base::AutoLock locked(config_mutex_);
    fake_blocked_socket_percentage_  = fake_blocked_socket_percentage;
  }

  // The percent of time a packet is simulated as being reordered.
  void set_fake_reorder_percentage(int32 fake_packet_reorder_percentage) {
    DCHECK(clock_);
    base::AutoLock locked(config_mutex_);
    DCHECK(!fake_packet_delay_.IsZero());
    fake_packet_reorder_percentage_ = fake_packet_reorder_percentage;
  }

  // The percent of time WritePacket will block and set WriteResult's status
  // to WRITE_STATUS_BLOCKED.
  void set_fake_packet_delay(QuicTime::Delta fake_packet_delay) {
    DCHECK(clock_);
    base::AutoLock locked(config_mutex_);
    fake_packet_delay_  = fake_packet_delay;
  }

  // The maximum bandwidth and buffer size of the connection.  When these are
  // set, packets will be delayed until a connection with that bandwidth would
  // transmit it.  Once the |buffer_size| is reached, all new packets are
  // dropped.
  void set_max_bandwidth_and_buffer_size(QuicBandwidth fake_bandwidth,
                                         QuicByteCount buffer_size) {
    DCHECK(clock_);
    base::AutoLock locked(config_mutex_);
    fake_bandwidth_ = fake_bandwidth;
    buffer_size_ = buffer_size;
  }

  void set_seed(uint64 seed) {
    simple_random_.set_seed(seed);
  }

 private:
  // Writes out the next packet to the contained writer and returns the time
  // for the next delayed packet to be written.
  QuicTime ReleaseNextPacket();

  // A single packet which will be sent at the supplied send_time.
  struct DelayedWrite {
   public:
    DelayedWrite(const char* buffer,
                 size_t buf_len,
                 const IPAddressNumber& self_address,
                 const IPEndPoint& peer_address,
                 QuicTime send_time);
    ~DelayedWrite();

    string buffer;
    const IPAddressNumber self_address;
    const IPEndPoint peer_address;
    QuicTime send_time;
  };

  typedef std::list<DelayedWrite> DelayedPacketList;

  const QuicClock* clock_;
  scoped_ptr<QuicAlarm> write_unblocked_alarm_;
  scoped_ptr<QuicAlarm> delay_alarm_;
  scoped_ptr<Delegate> on_can_write_;
  net::test::SimpleRandom simple_random_;
  // Stored packets delayed by fake packet delay or bandwidth restrictions.
  DelayedPacketList delayed_packets_;
  QuicByteCount cur_buffer_size_;

  base::Lock config_mutex_;
  int32 fake_packet_loss_percentage_;
  int32 fake_blocked_socket_percentage_;
  int32 fake_packet_reorder_percentage_;
  QuicTime::Delta fake_packet_delay_;
  QuicBandwidth fake_bandwidth_;
  QuicByteCount buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(PacketDroppingTestWriter);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some socket related helper methods for quic.

#ifndef NET_TOOLS_QUIC_QUIC_SOCKET_UTILS_H_
#define NET_TOOLS_QUIC_QUIC_SOCKET_UTILS_H_

#include <stddef.h>
#include <sys/socket.h>
#include <string>

#include "base/basictypes.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_types.h"

namespace net {
namespace tools {

class QuicSocketUtils {
 public:
  // If the msghdr contains IP_PKTINFO or IPV6_PKTINFO, this will return the
  // IPAddressNumber in that header.  Returns an uninitialized IPAddress on
  // failure.
  static IPAddressNumber GetAddressFromMsghdr(struct msghdr *hdr);

  // If the msghdr contains an SO_RXQ_OVFL entry, this will set dropped_packets
  // to the correct value and return true. Otherwise it will return false.
  static bool GetOverflowFromMsghdr(struct msghdr *hdr,
                                    uint32 *dropped_packets);

  // Sets either IP_PKTINFO or IPV6_PKTINFO on the socket, based on
  // address_family.  Returns the return code from setsockopt.
  static int SetGetAddressInfo(int fd, int address_family);

  // Sets the send buffer size to |size| and returns false if it fails.
  static bool SetSendBufferSize(int fd, size_t size);

  // Sets the receive buffer size to |size| and returns false if it fails.
  static bool SetReceiveBufferSize(int fd, size_t size);

  // Reads buf_len from the socket.  If reading is successful, returns bytes
  // read and sets peer_address to the peer address.  Otherwise returns -1.
  //
  // If dropped_packets is non-null, it will be set to the number of packets
  // dropped on the socket since the socket was created, assuming the kernel
  // supports this feature.
  //
  // If self_address is non-null, it will be set to the address the peer sent
  // packets to, assuming a packet was read.
  static int ReadPacket(int fd,
                        char* buffer,
                        size_t buf_len,
                        uint32* dropped_packets,
                        IPAddressNumber* self_address,
                        IPEndPoint* peer_address);

  // Writes buf_len to the socket. If writing is successful, sets the result's
  // status to WRITE_STATUS_OK and sets bytes_written.  Otherwise sets the
  // result's status to WRITE_STATUS_BLOCKED or WRITE_STATUS_ERROR and sets
  // error_code to errno.
  static WriteResult WritePacket(int fd,
                                 const char* buffer,
                                 size_t buf_len,
                                 const IPAddressNumber& self_address,
                                 const IPEndPoint& peer_address);

  // A helper for WritePacket which fills in the cmsg with the supplied self
  // address.
  // Returns the length of the packet info structure used.
  static size_t SetIpInfoInCmsg(const IPAddressNumber& self_address,
                                cmsghdr* cmsg);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicSocketUtils);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SOCKET_UTILS_H_

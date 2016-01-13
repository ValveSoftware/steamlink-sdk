// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_socket_utils.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/quic/quic_protocol.h"

#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif

namespace net {
namespace tools {

// static
IPAddressNumber QuicSocketUtils::GetAddressFromMsghdr(struct msghdr *hdr) {
  if (hdr->msg_controllen > 0) {
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(hdr);
         cmsg != NULL;
         cmsg = CMSG_NXTHDR(hdr, cmsg)) {
      const uint8* addr_data = NULL;
      int len = 0;
      if (cmsg->cmsg_type == IPV6_PKTINFO) {
        in6_pktinfo* info = reinterpret_cast<in6_pktinfo*>CMSG_DATA(cmsg);
        in6_addr addr = info->ipi6_addr;
        addr_data = reinterpret_cast<const uint8*>(&addr);
        len = sizeof(addr);
      } else if (cmsg->cmsg_type == IP_PKTINFO) {
        in_pktinfo* info = reinterpret_cast<in_pktinfo*>CMSG_DATA(cmsg);
        in_addr addr = info->ipi_addr;
        addr_data = reinterpret_cast<const uint8*>(&addr);
        len = sizeof(addr);
      } else {
        continue;
      }
      return IPAddressNumber(addr_data, addr_data + len);
    }
  }
  DCHECK(false) << "Unable to get address from msghdr";
  return IPAddressNumber();
}

// static
bool QuicSocketUtils::GetOverflowFromMsghdr(struct msghdr *hdr,
                                            uint32 *dropped_packets) {
  if (hdr->msg_controllen > 0) {
    struct cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(hdr);
         cmsg != NULL;
         cmsg = CMSG_NXTHDR(hdr, cmsg)) {
      if (cmsg->cmsg_type == SO_RXQ_OVFL) {
        *dropped_packets = *(reinterpret_cast<int*>CMSG_DATA(cmsg));
        return true;
      }
    }
  }
  return false;
}

// static
int QuicSocketUtils::SetGetAddressInfo(int fd, int address_family) {
  int get_local_ip = 1;
  if (address_family == AF_INET) {
    return setsockopt(fd, IPPROTO_IP, IP_PKTINFO,
                      &get_local_ip, sizeof(get_local_ip));
  } else {
    return setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                      &get_local_ip, sizeof(get_local_ip));
  }
}

// static
bool QuicSocketUtils::SetSendBufferSize(int fd, size_t size) {
  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) != 0) {
    LOG(ERROR) << "Failed to set socket send size";
    return false;
  }
  return true;
}

// static
bool QuicSocketUtils::SetReceiveBufferSize(int fd, size_t size) {
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) != 0) {
    LOG(ERROR) << "Failed to set socket recv size";
    return false;
  }
  return true;
}

// static
int QuicSocketUtils::ReadPacket(int fd, char* buffer, size_t buf_len,
                                uint32* dropped_packets,
                                IPAddressNumber* self_address,
                                IPEndPoint* peer_address) {
  CHECK(peer_address != NULL);
  const int kSpaceForOverflowAndIp =
      CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(in6_pktinfo));
  char cbuf[kSpaceForOverflowAndIp];
  memset(cbuf, 0, arraysize(cbuf));

  iovec iov = {buffer, buf_len};
  struct sockaddr_storage raw_address;
  msghdr hdr;

  hdr.msg_name = &raw_address;
  hdr.msg_namelen = sizeof(sockaddr_storage);
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;

  struct cmsghdr *cmsg = (struct cmsghdr *) cbuf;
  cmsg->cmsg_len = arraysize(cbuf);
  hdr.msg_control = cmsg;
  hdr.msg_controllen = arraysize(cbuf);

  int bytes_read = recvmsg(fd, &hdr, 0);

  // Return before setting dropped packets: if we get EAGAIN, it will
  // be 0.
  if (bytes_read < 0 && errno != 0) {
    if (errno != EAGAIN) {
      LOG(ERROR) << "Error reading " << strerror(errno);
    }
    return -1;
  }

  if (dropped_packets != NULL) {
    GetOverflowFromMsghdr(&hdr, dropped_packets);
  }
  if (self_address != NULL) {
    *self_address = QuicSocketUtils::GetAddressFromMsghdr(&hdr);
  }

  if (raw_address.ss_family == AF_INET) {
    CHECK(peer_address->FromSockAddr(
        reinterpret_cast<const sockaddr*>(&raw_address),
        sizeof(struct sockaddr_in)));
  } else if (raw_address.ss_family == AF_INET6) {
    CHECK(peer_address->FromSockAddr(
        reinterpret_cast<const sockaddr*>(&raw_address),
        sizeof(struct sockaddr_in6)));
  }

  return bytes_read;
}

size_t QuicSocketUtils::SetIpInfoInCmsg(const IPAddressNumber& self_address,
                                        cmsghdr* cmsg) {
  if (GetAddressFamily(self_address) == ADDRESS_FAMILY_IPV4) {
    cmsg->cmsg_len = CMSG_LEN(sizeof(in_pktinfo));
    cmsg->cmsg_level = IPPROTO_IP;
    cmsg->cmsg_type = IP_PKTINFO;
    in_pktinfo* pktinfo = reinterpret_cast<in_pktinfo*>(CMSG_DATA(cmsg));
    memset(pktinfo, 0, sizeof(in_pktinfo));
    pktinfo->ipi_ifindex = 0;
    memcpy(&pktinfo->ipi_spec_dst, &self_address[0], self_address.size());
    return sizeof(in_pktinfo);
  } else {
    cmsg->cmsg_len = CMSG_LEN(sizeof(in6_pktinfo));
    cmsg->cmsg_level = IPPROTO_IPV6;
    cmsg->cmsg_type = IPV6_PKTINFO;
    in6_pktinfo* pktinfo = reinterpret_cast<in6_pktinfo*>(CMSG_DATA(cmsg));
    memset(pktinfo, 0, sizeof(in6_pktinfo));
    memcpy(&pktinfo->ipi6_addr, &self_address[0], self_address.size());
    return sizeof(in6_pktinfo);
  }
}

// static
WriteResult QuicSocketUtils::WritePacket(int fd,
                                         const char* buffer,
                                         size_t buf_len,
                                         const IPAddressNumber& self_address,
                                         const IPEndPoint& peer_address) {
  sockaddr_storage raw_address;
  socklen_t address_len = sizeof(raw_address);
  CHECK(peer_address.ToSockAddr(
      reinterpret_cast<struct sockaddr*>(&raw_address),
      &address_len));
  iovec iov = {const_cast<char*>(buffer), buf_len};

  msghdr hdr;
  hdr.msg_name = &raw_address;
  hdr.msg_namelen = address_len;
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;

  const int kSpaceForIpv4 = CMSG_SPACE(sizeof(in_pktinfo));
  const int kSpaceForIpv6 = CMSG_SPACE(sizeof(in6_pktinfo));
  // kSpaceForIp should be big enough to hold both IPv4 and IPv6 packet info.
  const int kSpaceForIp =
      (kSpaceForIpv4 < kSpaceForIpv6) ? kSpaceForIpv6 : kSpaceForIpv4;
  char cbuf[kSpaceForIp];
  if (self_address.empty()) {
    hdr.msg_control = 0;
    hdr.msg_controllen = 0;
  } else {
    hdr.msg_control = cbuf;
    hdr.msg_controllen = kSpaceForIp;
    cmsghdr *cmsg = CMSG_FIRSTHDR(&hdr);
    SetIpInfoInCmsg(self_address, cmsg);
    hdr.msg_controllen = cmsg->cmsg_len;
  }

  int rc = sendmsg(fd, &hdr, 0);
  if (rc >= 0) {
    return WriteResult(WRITE_STATUS_OK, rc);
  }
  return WriteResult((errno == EAGAIN || errno == EWOULDBLOCK) ?
      WRITE_STATUS_BLOCKED : WRITE_STATUS_ERROR, errno);
}

}  // namespace tools
}  // namespace net

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_PROTOCOL_H_
#define NET_DNS_DNS_PROTOCOL_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

namespace dns_protocol {

static const uint16 kDefaultPort = 53;
static const uint16 kDefaultPortMulticast = 5353;

// DNS packet consists of a header followed by questions and/or answers.
// For the meaning of specific fields, please see RFC 1035 and 2535

// Header format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      ID                       |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |QR|   Opcode  |AA|TC|RD|RA| Z|AD|CD|   RCODE   |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    QDCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ANCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    NSCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ARCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Question format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                                               |
//  /                     QNAME                     /
//  /                                               /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QTYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QCLASS                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Answer format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                                               |
//  /                                               /
//  /                      NAME                     /
//  |                                               |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      TYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     CLASS                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      TTL                      |
//  |                                               |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                   RDLENGTH                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
//  /                     RDATA                     /
//  /                                               /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

#pragma pack(push)
#pragma pack(1)

// On-the-wire header. All uint16 are in network order.
// Used internally in DnsQuery and DnsResponseParser.
struct NET_EXPORT_PRIVATE Header {
  uint16 id;
  uint16 flags;
  uint16 qdcount;
  uint16 ancount;
  uint16 nscount;
  uint16 arcount;
};

#pragma pack(pop)

static const uint8 kLabelMask = 0xc0;
static const uint8 kLabelPointer = 0xc0;
static const uint8 kLabelDirect = 0x0;
static const uint16 kOffsetMask = 0x3fff;

// In MDns the most significant bit of the rrclass is designated as the
// "cache-flush bit", as described in http://www.rfc-editor.org/rfc/rfc6762.txt
// section 10.2.
static const uint16 kMDnsClassMask = 0x7FFF;

static const int kMaxNameLength = 255;

// RFC 1035, section 4.2.1: Messages carried by UDP are restricted to 512
// bytes (not counting the IP nor UDP headers).
static const int kMaxUDPSize = 512;

// RFC 6762, section 17: Messages over the local link are restricted by the
// medium's MTU, and must be under 9000 bytes
static const int kMaxMulticastSize = 9000;

// DNS class types.
static const uint16 kClassIN = 1;

// DNS resource record types. See
// http://www.iana.org/assignments/dns-parameters
static const uint16 kTypeA = 1;
static const uint16 kTypeCNAME = 5;
static const uint16 kTypePTR = 12;
static const uint16 kTypeTXT = 16;
static const uint16 kTypeAAAA = 28;
static const uint16 kTypeSRV = 33;
static const uint16 kTypeNSEC = 47;


// DNS rcode values.
static const uint8 kRcodeMask = 0xf;
static const uint8 kRcodeNOERROR = 0;
static const uint8 kRcodeFORMERR = 1;
static const uint8 kRcodeSERVFAIL = 2;
static const uint8 kRcodeNXDOMAIN = 3;
static const uint8 kRcodeNOTIMP = 4;
static const uint8 kRcodeREFUSED = 5;

// DNS flags.
static const uint16 kFlagResponse = 0x8000;
static const uint16 kFlagRA = 0x80;
static const uint16 kFlagRD = 0x100;
static const uint16 kFlagTC = 0x200;
static const uint16 kFlagAA = 0x400;

}  // namespace dns_protocol

}  // namespace net

#endif  // NET_DNS_DNS_PROTOCOL_H_

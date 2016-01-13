// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_query.h"

#include <limits>

#include "base/big_endian.h"
#include "base/sys_byteorder.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_protocol.h"

namespace net {

// DNS query consists of a 12-byte header followed by a question section.
// For details, see RFC 1035 section 4.1.1.  This header template sets RD
// bit, which directs the name server to pursue query recursively, and sets
// the QDCOUNT to 1, meaning the question section has a single entry.
DnsQuery::DnsQuery(uint16 id, const base::StringPiece& qname, uint16 qtype)
    : qname_size_(qname.size()) {
  DCHECK(!DNSDomainToString(qname).empty());
  // QNAME + QTYPE + QCLASS
  size_t question_size = qname_size_ + sizeof(uint16) + sizeof(uint16);
  io_buffer_ = new IOBufferWithSize(sizeof(dns_protocol::Header) +
                                    question_size);
  dns_protocol::Header* header =
      reinterpret_cast<dns_protocol::Header*>(io_buffer_->data());
  memset(header, 0, sizeof(dns_protocol::Header));
  header->id = base::HostToNet16(id);
  header->flags = base::HostToNet16(dns_protocol::kFlagRD);
  header->qdcount = base::HostToNet16(1);

  // Write question section after the header.
  base::BigEndianWriter writer(reinterpret_cast<char*>(header + 1),
                               question_size);
  writer.WriteBytes(qname.data(), qname.size());
  writer.WriteU16(qtype);
  writer.WriteU16(dns_protocol::kClassIN);
}

DnsQuery::~DnsQuery() {
}

DnsQuery* DnsQuery::CloneWithNewId(uint16 id) const {
  return new DnsQuery(*this, id);
}

uint16 DnsQuery::id() const {
  const dns_protocol::Header* header =
      reinterpret_cast<const dns_protocol::Header*>(io_buffer_->data());
  return base::NetToHost16(header->id);
}

base::StringPiece DnsQuery::qname() const {
  return base::StringPiece(io_buffer_->data() + sizeof(dns_protocol::Header),
                           qname_size_);
}

uint16 DnsQuery::qtype() const {
  uint16 type;
  base::ReadBigEndian<uint16>(
      io_buffer_->data() + sizeof(dns_protocol::Header) + qname_size_, &type);
  return type;
}

base::StringPiece DnsQuery::question() const {
  return base::StringPiece(io_buffer_->data() + sizeof(dns_protocol::Header),
                           qname_size_ + sizeof(uint16) + sizeof(uint16));
}

DnsQuery::DnsQuery(const DnsQuery& orig, uint16 id) {
  qname_size_ = orig.qname_size_;
  io_buffer_ = new IOBufferWithSize(orig.io_buffer()->size());
  memcpy(io_buffer_.get()->data(), orig.io_buffer()->data(),
         io_buffer_.get()->size());
  dns_protocol::Header* header =
      reinterpret_cast<dns_protocol::Header*>(io_buffer_->data());
  header->id = base::HostToNet16(id);
}

void DnsQuery::set_flags(uint16 flags) {
  dns_protocol::Header* header =
      reinterpret_cast<dns_protocol::Header*>(io_buffer_->data());
  header->flags = flags;
}

}  // namespace net

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_RECORD_PARSED_H_
#define NET_DNS_RECORD_PARSED_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

class DnsRecordParser;
class RecordRdata;

// Parsed record. This is a form of DnsResourceRecord where the rdata section
// has been parsed into a data structure.
class NET_EXPORT_PRIVATE RecordParsed {
 public:
  virtual ~RecordParsed();

  // All records are inherently immutable. Return a const pointer.
  static scoped_ptr<const RecordParsed> CreateFrom(DnsRecordParser* parser,
                                                   base::Time time_created);

  const std::string& name() const { return name_; }
  uint16 type() const { return type_; }
  uint16 klass() const { return klass_; }
  uint32 ttl() const { return ttl_; }

  base::Time time_created() const { return time_created_; }

  template <class T> const T* rdata() const {
    if (T::kType != type_)
      return NULL;
    return static_cast<const T*>(rdata_.get());
  }

  // Check if two records have the same data. Ignores time_created and ttl.
  // If |is_mdns| is true, ignore the top bit of the class
  // (the cache flush bit).
  bool IsEqual(const RecordParsed* other, bool is_mdns) const;

 private:
  RecordParsed(const std::string& name, uint16 type, uint16 klass,
               uint32 ttl, scoped_ptr<const RecordRdata> rdata,
               base::Time time_created);

  std::string name_;  // in dotted form
  const uint16 type_;
  const uint16 klass_;
  const uint32 ttl_;

  const scoped_ptr<const RecordRdata> rdata_;

  const base::Time time_created_;
};

}  // namespace net

#endif  // NET_DNS_RECORD_PARSED_H_

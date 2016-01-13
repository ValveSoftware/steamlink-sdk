// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_HOSTS_H_
#define NET_DNS_DNS_HOSTS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "net/base/address_family.h"
#include "net/base/net_export.h"
#include "net/base/net_util.h"  // can't forward-declare IPAddressNumber

namespace net {
  typedef std::pair<std::string, AddressFamily> DnsHostsKey;
};

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_GCC)

template<>
struct hash<net::DnsHostsKey> {
  std::size_t operator()(const net::DnsHostsKey& key) const {
    hash<base::StringPiece> string_piece_hash;
    return string_piece_hash(key.first) + key.second;
  }
};

#elif defined(COMPILER_MSVC)

inline size_t hash_value(const net::DnsHostsKey& key) {
  return hash_value(key.first) + key.second;
}

#endif  // COMPILER

}  // namespace BASE_HASH_NAMESPACE

namespace net {

// Parsed results of a Hosts file.
//
// Although Hosts files map IP address to a list of domain names, for name
// resolution the desired mapping direction is: domain name to IP address.
// When parsing Hosts, we apply the "first hit" rule as Windows and glibc do.
// With a Hosts file of:
// 300.300.300.300 localhost # bad ip
// 127.0.0.1 localhost
// 10.0.0.1 localhost
// The expected resolution of localhost is 127.0.0.1.
#if !defined(OS_ANDROID)
typedef base::hash_map<DnsHostsKey, IPAddressNumber> DnsHosts;
#else
// Android's hash_map doesn't support ==, so fall back to map.  (Chromium on
// Android doesn't use the built-in DNS resolver anyway, so it's irrelevant.)
typedef std::map<DnsHostsKey, IPAddressNumber> DnsHosts;
#endif

// Parses |contents| (as read from /etc/hosts or equivalent) and stores results
// in |dns_hosts|. Invalid lines are ignored (as in most implementations).
void NET_EXPORT_PRIVATE ParseHosts(const std::string& contents,
                                   DnsHosts* dns_hosts);

// As above but reads the file pointed to by |path|.
bool NET_EXPORT_PRIVATE ParseHostsFile(const base::FilePath& path,
                                       DnsHosts* dns_hosts);



}  // namespace net

#endif  // NET_DNS_DNS_HOSTS_H_


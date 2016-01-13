// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NB: Modelled after Mozilla's code (originally written by Pamela Greene,
// later modified by others), but almost entirely rewritten for Chrome.
//   (netwerk/dns/src/nsEffectiveTLDService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Effective-TLD Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pamela Greene <pamg.bugs@gmail.com> (original author)
 *   Daniel Witte <dwitte@stanford.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_module.h"
#include "net/base/net_util.h"
#include "url/gurl.h"
#include "url/url_parse.h"

namespace net {
namespace registry_controlled_domains {

namespace {
#include "net/base/registry_controlled_domains/effective_tld_names-inc.cc"

// See make_dafsa.py for documentation of the generated dafsa byte array.

const unsigned char* g_graph = kDafsa;
size_t g_graph_length = sizeof(kDafsa);

const int kNotFound = -1;
const int kExceptionRule = 1;
const int kWildcardRule = 2;
const int kPrivateRule = 4;

// Read next offset from pos.
// Returns true if an offset could be read, false otherwise.
bool GetNextOffset(const unsigned char** pos, const unsigned char* end,
                   const unsigned char** offset) {
  if (*pos == end)
    return false;

  // When reading an offset the byte array must always contain at least
  // three more bytes to consume. First the offset to read, then a node
  // to skip over and finally a destination node. No object can be smaller
  // than one byte.
  CHECK_LT(*pos + 2, end);
  size_t bytes_consumed;
  switch (**pos & 0x60) {
    case 0x60:  // Read three byte offset
      *offset += (((*pos)[0] & 0x1F) << 16) | ((*pos)[1] << 8) | (*pos)[2];
      bytes_consumed = 3;
      break;
    case 0x40:  // Read two byte offset
      *offset += (((*pos)[0] & 0x1F) << 8) | (*pos)[1];
      bytes_consumed = 2;
      break;
    default:
      *offset += (*pos)[0] & 0x3F;
      bytes_consumed = 1;
  }
  if ((**pos & 0x80) != 0) {
    *pos = end;
  } else {
    *pos += bytes_consumed;
  }
  return true;
}

// Check if byte at offset is last in label.
bool IsEOL(const unsigned char* offset, const unsigned char* end) {
  CHECK_LT(offset, end);
  return (*offset & 0x80) != 0;
}

// Check if byte at offset matches first character in key.
// This version matches characters not last in label.
bool IsMatch(const unsigned char* offset, const unsigned char* end,
             const char* key) {
  CHECK_LT(offset, end);
  return *offset == *key;
}

// Check if byte at offset matches first character in key.
// This version matches characters last in label.
bool IsEndCharMatch(const unsigned char* offset, const unsigned char* end,
                    const char* key) {
  CHECK_LT(offset, end);
  return *offset == (*key | 0x80);
}

// Read return value at offset.
// Returns true if a return value could be read, false otherwise.
bool GetReturnValue(const unsigned char* offset, const unsigned char* end,
                    int* return_value) {
  CHECK_LT(offset, end);
  if ((*offset & 0xE0) == 0x80) {
    *return_value = *offset & 0x0F;
    return true;
  }
  return false;
}

// Lookup a domain key in a byte array generated by make_dafsa.py.
// The rule type is returned if key is found, otherwise kNotFound is returned.
int LookupString(const unsigned char* graph, size_t length, const char* key,
                 size_t key_length) {
  const unsigned char* pos = graph;
  const unsigned char* end = graph + length;
  const unsigned char* offset = pos;
  const char* key_end = key + key_length;
  while (GetNextOffset(&pos, end, &offset)) {
    //   char <char>+ end_char offsets
    //   char <char>+ return value
    //   char end_char offsets
    //   char return value
    //   end_char offsets
    //   return_value
    bool did_consume = false;
    if (key != key_end && !IsEOL(offset, end)) {
      // Leading <char> is not a match. Don't dive into this child
      if (!IsMatch(offset, end, key))
        continue;
      did_consume = true;
      ++offset;
      ++key;
      // Possible matches at this point:
      // <char>+ end_char offsets
      // <char>+ return value
      // end_char offsets
      // return value
      // Remove all remaining <char> nodes possible
      while (!IsEOL(offset, end) && key != key_end) {
        if (!IsMatch(offset, end, key))
          return kNotFound;
        ++key;
        ++offset;
      }
    }
    // Possible matches at this point:
    // end_char offsets
    // return_value
    // If one or more <char> elements were consumed, a failure
    // to match is terminal. Otherwise, try the next node.
    if (key == key_end) {
      int return_value;
      if (GetReturnValue(offset, end, &return_value))
        return return_value;
      // The DAFSA guarantees that if the first char is a match, all
      // remaining char elements MUST match if the key is truly present.
      if (did_consume)
        return kNotFound;
      continue;
    }
    if (!IsEndCharMatch(offset, end, key)) {
      if (did_consume)
        return kNotFound;  // Unexpected
      continue;
    }
    ++key;
    pos = ++offset;  // Dive into child
  }
  return kNotFound;  // No match
}

size_t GetRegistryLengthImpl(
    const std::string& host,
    UnknownRegistryFilter unknown_filter,
    PrivateRegistryFilter private_filter) {
  DCHECK(!host.empty());

  // Skip leading dots.
  const size_t host_check_begin = host.find_first_not_of('.');
  if (host_check_begin == std::string::npos)
    return 0;  // Host is only dots.

  // A single trailing dot isn't relevant in this determination, but does need
  // to be included in the final returned length.
  size_t host_check_len = host.length();
  if (host[host_check_len - 1] == '.') {
    --host_check_len;
    DCHECK(host_check_len > 0);  // If this weren't true, the host would be ".",
                                 // and we'd have already returned above.
    if (host[host_check_len - 1] == '.')
      return 0;  // Multiple trailing dots.
  }

  // Walk up the domain tree, most specific to least specific,
  // looking for matches at each level.
  size_t prev_start = std::string::npos;
  size_t curr_start = host_check_begin;
  size_t next_dot = host.find('.', curr_start);
  if (next_dot >= host_check_len)  // Catches std::string::npos as well.
    return 0;  // This can't have a registry + domain.
  while (1) {
    const char* domain_str = host.data() + curr_start;
    size_t domain_length = host_check_len - curr_start;
    int type = LookupString(g_graph, g_graph_length, domain_str, domain_length);
    bool do_check =
        type != kNotFound && (!(type & kPrivateRule) ||
                              private_filter == INCLUDE_PRIVATE_REGISTRIES);

    // If the apparent match is a private registry and we're not including
    // those, it can't be an actual match.
    if (do_check) {
      // Exception rules override wildcard rules when the domain is an exact
      // match, but wildcards take precedence when there's a subdomain.
      if (type & kWildcardRule && (prev_start != std::string::npos)) {
        // If prev_start == host_check_begin, then the host is the registry
        // itself, so return 0.
        return (prev_start == host_check_begin) ? 0
                                                : (host.length() - prev_start);
      }

      if (type & kExceptionRule) {
        if (next_dot == std::string::npos) {
          // If we get here, we had an exception rule with no dots (e.g.
          // "!foo").  This would only be valid if we had a corresponding
          // wildcard rule, which would have to be "*".  But we explicitly
          // disallow that case, so this kind of rule is invalid.
          NOTREACHED() << "Invalid exception rule";
          return 0;
        }
        return host.length() - next_dot - 1;
      }

      // If curr_start == host_check_begin, then the host is the registry
      // itself, so return 0.
      return (curr_start == host_check_begin) ? 0
                                              : (host.length() - curr_start);
    }

    if (next_dot >= host_check_len)  // Catches std::string::npos as well.
      break;

    prev_start = curr_start;
    curr_start = next_dot + 1;
    next_dot = host.find('.', curr_start);
  }

  // No rule found in the registry.  curr_start now points to the first
  // character of the last subcomponent of the host, so if we allow unknown
  // registries, return the length of this subcomponent.
  return unknown_filter == INCLUDE_UNKNOWN_REGISTRIES ?
      (host.length() - curr_start) : 0;
}

std::string GetDomainAndRegistryImpl(
    const std::string& host, PrivateRegistryFilter private_filter) {
  DCHECK(!host.empty());

  // Find the length of the registry for this host.
  const size_t registry_length =
      GetRegistryLengthImpl(host, INCLUDE_UNKNOWN_REGISTRIES, private_filter);
  if ((registry_length == std::string::npos) || (registry_length == 0))
    return std::string();  // No registry.
  // The "2" in this next line is 1 for the dot, plus a 1-char minimum preceding
  // subcomponent length.
  DCHECK(host.length() >= 2);
  if (registry_length > (host.length() - 2)) {
    NOTREACHED() <<
        "Host does not have at least one subcomponent before registry!";
    return std::string();
  }

  // Move past the dot preceding the registry, and search for the next previous
  // dot.  Return the host from after that dot, or the whole host when there is
  // no dot.
  const size_t dot = host.rfind('.', host.length() - registry_length - 2);
  if (dot == std::string::npos)
    return host;
  return host.substr(dot + 1);
}

}  // namespace

std::string GetDomainAndRegistry(
    const GURL& gurl,
    PrivateRegistryFilter filter) {
  const url::Component host = gurl.parsed_for_possibly_invalid_spec().host;
  if ((host.len <= 0) || gurl.HostIsIPAddress())
    return std::string();
  return GetDomainAndRegistryImpl(std::string(
      gurl.possibly_invalid_spec().data() + host.begin, host.len), filter);
}

std::string GetDomainAndRegistry(
    const std::string& host,
    PrivateRegistryFilter filter) {
  url::CanonHostInfo host_info;
  const std::string canon_host(CanonicalizeHost(host, &host_info));
  if (canon_host.empty() || host_info.IsIPAddress())
    return std::string();
  return GetDomainAndRegistryImpl(canon_host, filter);
}

bool SameDomainOrHost(
    const GURL& gurl1,
    const GURL& gurl2,
    PrivateRegistryFilter filter) {
  // See if both URLs have a known domain + registry, and those values are the
  // same.
  const std::string domain1(GetDomainAndRegistry(gurl1, filter));
  const std::string domain2(GetDomainAndRegistry(gurl2, filter));
  if (!domain1.empty() || !domain2.empty())
    return domain1 == domain2;

  // No domains.  See if the hosts are identical.
  const url::Component host1 = gurl1.parsed_for_possibly_invalid_spec().host;
  const url::Component host2 = gurl2.parsed_for_possibly_invalid_spec().host;
  if ((host1.len <= 0) || (host1.len != host2.len))
    return false;
  return !strncmp(gurl1.possibly_invalid_spec().data() + host1.begin,
                  gurl2.possibly_invalid_spec().data() + host2.begin,
                  host1.len);
}

size_t GetRegistryLength(
    const GURL& gurl,
    UnknownRegistryFilter unknown_filter,
    PrivateRegistryFilter private_filter) {
  const url::Component host = gurl.parsed_for_possibly_invalid_spec().host;
  if (host.len <= 0)
    return std::string::npos;
  if (gurl.HostIsIPAddress())
    return 0;
  return GetRegistryLengthImpl(
      std::string(gurl.possibly_invalid_spec().data() + host.begin, host.len),
      unknown_filter,
      private_filter);
}

size_t GetRegistryLength(
    const std::string& host,
    UnknownRegistryFilter unknown_filter,
    PrivateRegistryFilter private_filter) {
  url::CanonHostInfo host_info;
  const std::string canon_host(CanonicalizeHost(host, &host_info));
  if (canon_host.empty())
    return std::string::npos;
  if (host_info.IsIPAddress())
    return 0;
  return GetRegistryLengthImpl(canon_host, unknown_filter, private_filter);
}

void SetFindDomainGraph() {
  g_graph = kDafsa;
  g_graph_length = sizeof(kDafsa);
}

void SetFindDomainGraph(const unsigned char* domains, size_t length) {
  CHECK(domains);
  CHECK_NE(length, 0u);
  g_graph = domains;
  g_graph_length = length;
}

}  // namespace registry_controlled_domains
}  // namespace net

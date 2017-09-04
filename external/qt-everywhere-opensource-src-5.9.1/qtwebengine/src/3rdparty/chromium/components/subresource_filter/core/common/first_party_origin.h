// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FIRST_PARTY_ORIGIN_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FIRST_PARTY_ORIGIN_H_

#include <string>

#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

// Encapsulates the first-party origin of a document, and provides fast (cached)
// third-partiness checks against that origin, i.e. whether a given URL is
// third-party in relation to the document's origin.
//
// It uses a simple one-entry cache to optimize for the case when a significant
// number of consecutive URLs have the same domain.
class FirstPartyOrigin {
 public:
  explicit FirstPartyOrigin(url::Origin document_origin);

  const url::Origin& origin() const { return document_origin_; }

  // Returns whether |url| is a third-party in relation to |this| origin. May
  // return a cached value. May update the cache.
  bool IsThirdParty(const GURL& url) const;

  // Returns whether |url| is a third party in respect to |first_party_origin|.
  static bool IsThirdParty(const GURL& url,
                           const url::Origin& first_party_origin);

 private:
  url::Origin document_origin_;

  // NOTE: GURL is needed to work around the slightly inefficient interfaces of
  // net::registry_controlled_domains.
  GURL document_origin_as_gurl_;

  // One-entry cache that stores input/output of the last IsThirdParty
  // call.
  //
  // NOTE: registry_controlled_domains::SameDomainOrHost takes GURLs as inputs,
  // but the cache entry stores only the host part of a GURL. It is assumed that
  // SameDomainOrHost will return the same result for a GURL which has the same
  // non-empty host part as the cached one.
  mutable std::string last_checked_host_;
  mutable bool last_checked_host_was_third_party_ = false;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_FIRST_PARTY_ORIGIN_H_

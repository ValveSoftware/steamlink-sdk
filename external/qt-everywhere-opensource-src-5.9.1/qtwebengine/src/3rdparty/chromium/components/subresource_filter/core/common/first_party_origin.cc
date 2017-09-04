// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/first_party_origin.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace subresource_filter {

namespace {

bool IsThirdPartyImpl(const GURL& url, const GURL& first_party_url) {
  return !net::registry_controlled_domains::SameDomainOrHost(
      url, first_party_url,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

FirstPartyOrigin::FirstPartyOrigin(url::Origin document_origin)
    : document_origin_(std::move(document_origin)) {
  document_origin_as_gurl_ = GURL(document_origin_.Serialize());
}

bool FirstPartyOrigin::IsThirdParty(const GURL& url) const {
  if (document_origin_.unique())
    return true;
  if (!last_checked_host_.empty() && url.host_piece() == last_checked_host_)
    return last_checked_host_was_third_party_;

  url.host_piece().CopyToString(&last_checked_host_);
  last_checked_host_was_third_party_ =
      IsThirdPartyImpl(url, document_origin_as_gurl_);
  return last_checked_host_was_third_party_;
}

bool FirstPartyOrigin::IsThirdParty(const GURL& url,
                                    const url::Origin& first_party_origin) {
  // TODO(pkalinnikov): Avoid converting Origin to GURL.
  return first_party_origin.unique() ||
         IsThirdPartyImpl(url, GURL(first_party_origin.Serialize()));
}

}  // namespace subresouce_filter

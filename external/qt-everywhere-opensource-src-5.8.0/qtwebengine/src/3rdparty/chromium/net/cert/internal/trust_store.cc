// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/trust_store.h"

#include "net/cert/internal/parsed_certificate.h"

namespace net {

TrustStore::TrustStore() {}
TrustStore::~TrustStore() {}

void TrustStore::Clear() {
  anchors_.clear();
}

void TrustStore::AddTrustedCertificate(
    scoped_refptr<ParsedCertificate> anchor) {
  // TODO(mattm): should this check for duplicate certs?
  anchors_.insert(std::make_pair(anchor->normalized_subject().AsStringPiece(),
                                 std::move(anchor)));
}

void TrustStore::FindTrustAnchorsByNormalizedName(
    const der::Input& normalized_name,
    std::vector<scoped_refptr<ParsedCertificate>>* matches) const {
  auto range = anchors_.equal_range(normalized_name.AsStringPiece());
  for (auto it = range.first; it != range.second; ++it)
    matches->push_back(it->second);
}

bool TrustStore::IsTrustedCertificate(const ParsedCertificate* cert) const {
  auto range = anchors_.equal_range(cert->normalized_subject().AsStringPiece());
  for (auto it = range.first; it != range.second; ++it) {
    // First compare the ParsedCertificate pointers as an optimization, fall
    // back to comparing full DER encoding.
    if (it->second == cert || it->second->der_cert() == cert->der_cert())
      return true;
  }
  return false;
}

}  // namespace net

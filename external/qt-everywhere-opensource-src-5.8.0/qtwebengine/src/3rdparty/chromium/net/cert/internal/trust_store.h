// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_H_
#define NET_CERT_INTERNAL_TRUST_STORE_H_

#include <unordered_map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

namespace der {
class Input;
}

class ParsedCertificate;

// A very simple implementation of a TrustStore, which contains a set of
// trusted certificates.
// TODO(mattm): convert this into an interface, provide implementations that
// interface with OS trust store.
class NET_EXPORT TrustStore {
 public:
  TrustStore();
  ~TrustStore();

  // Empties the trust store, resetting it to original state.
  void Clear();

  // Adds a trusted certificate to the store.
  void AddTrustedCertificate(scoped_refptr<ParsedCertificate> anchor);

  // Returns the trust anchors that match |name| in |*matches|, if any.
  void FindTrustAnchorsByNormalizedName(
      const der::Input& normalized_name,
      std::vector<scoped_refptr<ParsedCertificate>>* matches) const;

  // Returns true if |cert| matches a certificate in the TrustStore.
  bool IsTrustedCertificate(const ParsedCertificate* cert) const
      WARN_UNUSED_RESULT;

 private:
  // Multimap from normalized subject -> ParsedCertificate.
  std::unordered_multimap<base::StringPiece,
                          scoped_refptr<ParsedCertificate>,
                          base::StringPieceHash>
      anchors_;

  DISALLOW_COPY_AND_ASSIGN(TrustStore);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_H_

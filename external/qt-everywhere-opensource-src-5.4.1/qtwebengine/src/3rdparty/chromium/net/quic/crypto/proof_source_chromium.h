// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROOF_SOURCE_CHROMIUM_H_
#define NET_QUIC_CRYPTO_PROOF_SOURCE_CHROMIUM_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/proof_source.h"

namespace net {

// ProofSourceChromium implements the QUIC ProofSource interface.
// TODO(rtenneti): implement details of this class.
class NET_EXPORT_PRIVATE ProofSourceChromium : public ProofSource {
 public:
  ProofSourceChromium();
  virtual ~ProofSourceChromium() {}

  // ProofSource interface
  virtual bool GetProof(const std::string& hostname,
                        const std::string& server_config,
                        bool ecdsa_ok,
                        const std::vector<std::string>** out_certs,
                        std::string* out_signature) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProofSourceChromium);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROOF_SOURCE_CHROMIUM_H_

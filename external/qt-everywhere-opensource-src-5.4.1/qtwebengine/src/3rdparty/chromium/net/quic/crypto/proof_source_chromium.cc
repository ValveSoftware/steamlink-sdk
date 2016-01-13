// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_source_chromium.h"

using std::string;
using std::vector;

namespace net {

ProofSourceChromium::ProofSourceChromium() {
}

bool ProofSourceChromium::GetProof(const string& hostname,
                                   const string& server_config,
                                   bool ecdsa_ok,
                                   const vector<string>** out_certs,
                                   string* out_signature) {
  return false;
}

}  // namespace net

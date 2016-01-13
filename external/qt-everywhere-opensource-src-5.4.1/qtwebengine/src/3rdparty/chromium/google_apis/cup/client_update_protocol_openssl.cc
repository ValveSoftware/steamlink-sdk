// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/cup/client_update_protocol.h"

#include "base/logging.h"

ClientUpdateProtocol::~ClientUpdateProtocol() {}

bool ClientUpdateProtocol::LoadPublicKey(const base::StringPiece& public_key) {
  NOTIMPLEMENTED();
  return false;
}

size_t ClientUpdateProtocol::PublicKeyLength() {
  NOTIMPLEMENTED();
  return 0;
}

bool ClientUpdateProtocol::EncryptKeySource(
    const std::vector<uint8>& key_source) {
  NOTIMPLEMENTED();
  return false;
}



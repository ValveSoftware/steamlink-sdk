// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/jwk_serializer.h"

#include "base/logging.h"

namespace net {

namespace JwkSerializer {

bool ConvertSpkiFromDerToJwk(
    const base::StringPiece& spki_der,
    base::DictionaryValue* public_key_jwk) {
  // TODO(juanlang): implement
  NOTIMPLEMENTED();
  return false;
}

}  // namespace JwkSerializer

}  // namespace net

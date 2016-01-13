// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_JSON_WEB_KEY_H_
#define MEDIA_CDM_JSON_WEB_KEY_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

// A JSON Web Key Set looks like the following in JSON:
//   { "keys": [ JWK1, JWK2, ... ] }
// A symmetric keys JWK looks like the following in JSON:
//   { "kty":"oct",
//     "kid":"AQIDBAUGBwgJCgsMDQ4PEA",
//     "k":"FBUWFxgZGhscHR4fICEiIw" }
// There may be other properties specified, but they are ignored.
// Ref: http://tools.ietf.org/html/draft-ietf-jose-json-web-key and:
// http://tools.ietf.org/html/draft-jones-jose-json-private-and-symmetric-key
//
// For EME WD, both 'kid' and 'k' are base64 encoded strings, without trailing
// padding.

// Vector of [key_id, key_value] pairs. Values are raw binary data, stored in
// strings for convenience.
typedef std::pair<std::string, std::string> KeyIdAndKeyPair;
typedef std::vector<KeyIdAndKeyPair> KeyIdAndKeyPairs;

// Converts a single |key|, |key_id| pair to a JSON Web Key Set.
MEDIA_EXPORT std::string GenerateJWKSet(const uint8* key, int key_length,
                                        const uint8* key_id, int key_id_length);

// Extracts the JSON Web Keys from a JSON Web Key Set. If |input| looks like
// a valid JWK Set, then true is returned and |keys| is updated to contain
// the list of keys found. Otherwise return false.
MEDIA_EXPORT bool ExtractKeysFromJWKSet(const std::string& jwk_set,
                                        KeyIdAndKeyPairs* keys);

}  // namespace media

#endif  // MEDIA_CDM_JSON_WEB_KEY_H_

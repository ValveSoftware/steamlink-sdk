// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/json_web_key.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace media {

const char kKeysTag[] = "keys";
const char kKeyTypeTag[] = "kty";
const char kSymmetricKeyValue[] = "oct";
const char kKeyTag[] = "k";
const char kKeyIdTag[] = "kid";
const char kBase64Padding = '=';

// Encodes |input| into a base64 string without padding.
static std::string EncodeBase64(const uint8* input, int input_length) {
  std::string encoded_text;
  base::Base64Encode(
      std::string(reinterpret_cast<const char*>(input), input_length),
      &encoded_text);

  // Remove any padding characters added by Base64Encode().
  size_t found = encoded_text.find_last_not_of(kBase64Padding);
  if (found != std::string::npos)
    encoded_text.erase(found + 1);

  return encoded_text;
}

// Decodes an unpadded base64 string. Returns empty string on error.
static std::string DecodeBase64(const std::string& encoded_text) {
  // EME spec doesn't allow padding characters.
  if (encoded_text.find_first_of(kBase64Padding) != std::string::npos)
    return std::string();

  // Since base::Base64Decode() requires padding characters, add them so length
  // of |encoded_text| is exactly a multiple of 4.
  size_t num_last_grouping_chars = encoded_text.length() % 4;
  std::string modified_text = encoded_text;
  if (num_last_grouping_chars > 0)
    modified_text.append(4 - num_last_grouping_chars, kBase64Padding);

  std::string decoded_text;
  if (!base::Base64Decode(modified_text, &decoded_text))
    return std::string();

  return decoded_text;
}

std::string GenerateJWKSet(const uint8* key, int key_length,
                           const uint8* key_id, int key_id_length) {
  // Both |key| and |key_id| need to be base64 encoded strings in the JWK.
  std::string key_base64 = EncodeBase64(key, key_length);
  std::string key_id_base64 = EncodeBase64(key_id, key_id_length);

  // Create the JWK, and wrap it into a JWK Set.
  scoped_ptr<base::DictionaryValue> jwk(new base::DictionaryValue());
  jwk->SetString(kKeyTypeTag, kSymmetricKeyValue);
  jwk->SetString(kKeyTag, key_base64);
  jwk->SetString(kKeyIdTag, key_id_base64);
  scoped_ptr<base::ListValue> list(new base::ListValue());
  list->Append(jwk.release());
  base::DictionaryValue jwk_set;
  jwk_set.Set(kKeysTag, list.release());

  // Finally serialize |jwk_set| into a string and return it.
  std::string serialized_jwk;
  JSONStringValueSerializer serializer(&serialized_jwk);
  serializer.Serialize(jwk_set);
  return serialized_jwk;
}

// Processes a JSON Web Key to extract the key id and key value. Sets |jwk_key|
// to the id/value pair and returns true on success.
static bool ConvertJwkToKeyPair(const base::DictionaryValue& jwk,
                                KeyIdAndKeyPair* jwk_key) {
  // Have found a JWK, start by checking that it is a symmetric key.
  std::string type;
  if (!jwk.GetString(kKeyTypeTag, &type) || type != kSymmetricKeyValue) {
    DVLOG(1) << "JWK is not a symmetric key";
    return false;
  }

  // Get the key id and actual key parameters.
  std::string encoded_key_id;
  std::string encoded_key;
  if (!jwk.GetString(kKeyIdTag, &encoded_key_id)) {
    DVLOG(1) << "Missing '" << kKeyIdTag << "' parameter";
    return false;
  }
  if (!jwk.GetString(kKeyTag, &encoded_key)) {
    DVLOG(1) << "Missing '" << kKeyTag << "' parameter";
    return false;
  }

  // Key ID and key are base64-encoded strings, so decode them.
  std::string raw_key_id = DecodeBase64(encoded_key_id);
  if (raw_key_id.empty()) {
    DVLOG(1) << "Invalid '" << kKeyIdTag << "' value: " << encoded_key_id;
    return false;
  }

  std::string raw_key = DecodeBase64(encoded_key);
  if (raw_key.empty()) {
    DVLOG(1) << "Invalid '" << kKeyTag << "' value: " << encoded_key;
    return false;
  }

  // Add the decoded key ID and the decoded key to the list.
  *jwk_key = std::make_pair(raw_key_id, raw_key);
  return true;
}

bool ExtractKeysFromJWKSet(const std::string& jwk_set, KeyIdAndKeyPairs* keys) {
  if (!base::IsStringASCII(jwk_set))
    return false;

  scoped_ptr<base::Value> root(base::JSONReader().ReadToValue(jwk_set));
  if (!root.get() || root->GetType() != base::Value::TYPE_DICTIONARY)
    return false;

  // Locate the set from the dictionary.
  base::DictionaryValue* dictionary =
      static_cast<base::DictionaryValue*>(root.get());
  base::ListValue* list_val = NULL;
  if (!dictionary->GetList(kKeysTag, &list_val)) {
    DVLOG(1) << "Missing '" << kKeysTag
             << "' parameter or not a list in JWK Set";
    return false;
  }

  // Create a local list of keys, so that |jwk_keys| only gets updated on
  // success.
  KeyIdAndKeyPairs local_keys;
  for (size_t i = 0; i < list_val->GetSize(); ++i) {
    base::DictionaryValue* jwk = NULL;
    if (!list_val->GetDictionary(i, &jwk)) {
      DVLOG(1) << "Unable to access '" << kKeysTag << "'[" << i
               << "] in JWK Set";
      return false;
    }
    KeyIdAndKeyPair key_pair;
    if (!ConvertJwkToKeyPair(*jwk, &key_pair)) {
      DVLOG(1) << "Error from '" << kKeysTag << "'[" << i << "]";
      return false;
    }
    local_keys.push_back(key_pair);
  }

  // Successfully processed all JWKs in the set.
  keys->swap(local_keys);
  return true;
}

}  // namespace media

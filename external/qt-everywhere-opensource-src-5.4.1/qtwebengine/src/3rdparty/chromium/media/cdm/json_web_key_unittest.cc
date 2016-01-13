// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/json_web_key.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class JSONWebKeyTest : public testing::Test {
 public:
  JSONWebKeyTest() {}

 protected:
  void ExtractJWKKeysAndExpect(const std::string& jwk,
                               bool expected_result,
                               size_t expected_number_of_keys) {
    DCHECK(!jwk.empty());
    KeyIdAndKeyPairs keys;
    EXPECT_EQ(expected_result, ExtractKeysFromJWKSet(jwk, &keys));
    EXPECT_EQ(expected_number_of_keys, keys.size());
  }
};

TEST_F(JSONWebKeyTest, GenerateJWKSet) {
  const uint8 data1[] = { 0x01, 0x02 };
  const uint8 data2[] = { 0x01, 0x02, 0x03, 0x04 };
  const uint8 data3[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 };

  EXPECT_EQ("{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQI\",\"kty\":\"oct\"}]}",
            GenerateJWKSet(data1, arraysize(data1), data1, arraysize(data1)));
  EXPECT_EQ(
      "{\"keys\":[{\"k\":\"AQIDBA\",\"kid\":\"AQIDBA\",\"kty\":\"oct\"}]}",
      GenerateJWKSet(data2, arraysize(data2), data2, arraysize(data2)));
  EXPECT_EQ("{\"keys\":[{\"k\":\"AQI\",\"kid\":\"AQIDBA\",\"kty\":\"oct\"}]}",
            GenerateJWKSet(data1, arraysize(data1), data2, arraysize(data2)));
  EXPECT_EQ("{\"keys\":[{\"k\":\"AQIDBA\",\"kid\":\"AQI\",\"kty\":\"oct\"}]}",
            GenerateJWKSet(data2, arraysize(data2), data1, arraysize(data1)));
  EXPECT_EQ(
      "{\"keys\":[{\"k\":\"AQIDBAUGBwgJCgsMDQ4PEA\",\"kid\":"
      "\"AQIDBAUGBwgJCgsMDQ4PEA\",\"kty\":\"oct\"}]}",
      GenerateJWKSet(data3, arraysize(data3), data3, arraysize(data3)));
}

TEST_F(JSONWebKeyTest, ExtractJWKKeys) {
  // Try a simple JWK key (i.e. not in a set)
  const std::string kJwkSimple =
      "{"
      "  \"kty\": \"oct\","
      "  \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "  \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "}";
  ExtractJWKKeysAndExpect(kJwkSimple, false, 0);

  // Try a key list with multiple entries.
  const std::string kJwksMultipleEntries =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAwQFBgcICQoLDA0ODxAREhM\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4/QA\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksMultipleEntries, true, 2);

  // Try a key with no spaces and some \n plus additional fields.
  const std::string kJwksNoSpaces =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\",\"alg\":\"A128KW\","
      "\"kid\":\"AAECAwQFBgcICQoLDA0ODxAREhM\",\"k\":\"GawgguFyGrWKav7AX4VKUg"
      "\",\"foo\":\"bar\"}]}\n\n";
  ExtractJWKKeysAndExpect(kJwksNoSpaces, true, 1);

  // Try some non-ASCII characters.
  ExtractJWKKeysAndExpect(
      "This is not ASCII due to \xff\xfe\xfd in it.", false, 0);

  // Try some non-ASCII characters in an otherwise valid JWK.
  const std::string kJwksInvalidCharacters =
      "\n\n{\"something\":1,\"keys\":[{\n\n\"kty\":\"oct\",\"alg\":\"A128KW\","
      "\"kid\":\"AAECAwQFBgcICQoLDA0ODxAREhM\",\"k\":\"\xff\xfe\xfd"
      "\",\"foo\":\"bar\"}]}\n\n";
  ExtractJWKKeysAndExpect(kJwksInvalidCharacters, false, 0);

  // Try a badly formatted key. Assume that the JSON parser is fully tested,
  // so we won't try a lot of combinations. However, need a test to ensure
  // that the code doesn't crash if invalid JSON received.
  ExtractJWKKeysAndExpect("This is not a JSON key.", false, 0);

  // Try passing some valid JSON that is not a dictionary at the top level.
  ExtractJWKKeysAndExpect("40", false, 0);

  // Try an empty dictionary.
  ExtractJWKKeysAndExpect("{ }", false, 0);

  // Try an empty 'keys' dictionary.
  ExtractJWKKeysAndExpect("{ \"keys\": [] }", true, 0);

  // Try with 'keys' not a dictionary.
  ExtractJWKKeysAndExpect("{ \"keys\":\"1\" }", false, 0);

  // Try with 'keys' a list of integers.
  ExtractJWKKeysAndExpect("{ \"keys\": [ 1, 2, 3 ] }", false, 0);

  // Try padding(=) at end of 'k' base64 string.
  const std::string kJwksWithPaddedKey =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAw\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw==\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithPaddedKey, false, 0);

  // Try padding(=) at end of 'kid' base64 string.
  const std::string kJwksWithPaddedKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"AAECAw==\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithPaddedKeyId, false, 0);

  // Try a key with invalid base64 encoding.
  const std::string kJwksWithInvalidBase64 =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"!@#$%^&*()\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithInvalidBase64, false, 0);

  // Empty key id.
  const std::string kJwksWithEmptyKeyId =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"\","
      "      \"k\": \"BAUGBwgJCgsMDQ4PEBESEw\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksWithEmptyKeyId, false, 0);

  // Try a list with multiple keys with the same kid.
  const std::string kJwksDuplicateKids =
      "{"
      "  \"keys\": ["
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\": \"FBUWFxgZGhscHR4fICEiIw\""
      "    },"
      "    {"
      "      \"kty\": \"oct\","
      "      \"kid\": \"JCUmJygpKissLS4vMA\","
      "      \"k\":\"MTIzNDU2Nzg5Ojs8PT4/QA\""
      "    }"
      "  ]"
      "}";
  ExtractJWKKeysAndExpect(kJwksDuplicateKids, true, 2);
}

}  // namespace media


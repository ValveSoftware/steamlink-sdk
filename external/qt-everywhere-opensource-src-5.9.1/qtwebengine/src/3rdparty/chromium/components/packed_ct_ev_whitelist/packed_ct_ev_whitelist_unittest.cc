// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/packed_ct_ev_whitelist/packed_ct_ev_whitelist.h"

#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/big_endian.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint8_t kFirstHashRaw[] =
    {0x00, 0x00, 0x03, 0xd7, 0xfc, 0x18, 0x02, 0xcb};
std::string GetFirstHash() {
  return std::string(reinterpret_cast<const char*>(kFirstHashRaw), 8);
}

// Second hash: Diff from first hash is > 2^47
const uint8_t kSecondHashRaw[] =
    {0x00, 0x01, 0x05, 0xd2, 0x58, 0x47, 0xa7, 0xbf};
std::string GetSecondHash() {
  return std::string(reinterpret_cast<const char*>(kSecondHashRaw), 8);
}

// Third hash: Diff from 2nd hash is < 2^47
const uint8_t kThirdHashRaw[] =
    {0x00, 0x01, 0x48, 0x45, 0x8c, 0x53, 0x03, 0x94};
std::string GetThirdHash() {
  return std::string(reinterpret_cast<const char*>(kThirdHashRaw), 8);
}

uint64_t HashToUint64(const std::string& hash_str) {
  uint64_t ret;
  base::ReadBigEndian(hash_str.c_str(), &ret);
  return ret;
}

const uint8_t kWhitelistData[] = {
    0x00, 0x00, 0x03, 0xd7, 0xfc, 0x18, 0x02, 0xcb,  // First hash
    0xc0, 0x7e, 0x97, 0x0b, 0xe9, 0x3d, 0x10, 0x9c,
    0xcd, 0x02, 0xd6, 0xf5, 0x40,
};

std::string GetPartialWhitelistData(uint8_t num_bytes) {
  return std::string(reinterpret_cast<const char*>(kWhitelistData), num_bytes);
}

std::string GetAllWhitelistData() {
  return GetPartialWhitelistData(arraysize(kWhitelistData));
}

}  // namespace

namespace packed_ct_ev_whitelist {

TEST(PackedEVCertsWhitelistTest, UncompressFailsForTooShortList) {
  // This list does not contain enough bytes even for the first hash.
  std::vector<uint64_t> res;
  EXPECT_FALSE(PackedEVCertsWhitelist::UncompressEVWhitelist(
      std::string(reinterpret_cast<const char*>(kWhitelistData), 7), &res));
}

TEST(PackedEVCertsWhitelistTest, UncompressFailsForTruncatedList) {
  // This list is missing bits for the second part of the diff.
  std::vector<uint64_t> res;
  EXPECT_FALSE(PackedEVCertsWhitelist::UncompressEVWhitelist(
      std::string(reinterpret_cast<const char*>(kWhitelistData), 14), &res));
}

TEST(PackedEVCertsWhitelistTest, UncompressFailsForInvalidValuesInList) {
  // A list with an invalid read_prefix value is the number 131072, unary
  // encoded, after the fist 8 bytes of a valid hash.
  // That translates to 16385 0xff bytes.
  // To make the hash otherwise valid, append 6 bytes of r value.
  const int num_ff_bytes = 16385;
  const int total_size = 8 + num_ff_bytes + 7;
  uint8_t* invalid_whitelist = new uint8_t[total_size];
  invalid_whitelist[total_size - 1] = '\0';
  // Valid first hash.
  memcpy(reinterpret_cast<char*>(invalid_whitelist),
         reinterpret_cast<const char*>(kWhitelistData),
         8 * sizeof(char));
  // 0xff 16385 times.
  for (int i = 0; i < num_ff_bytes; i++) {
    invalid_whitelist[8 + i] = 0xff;
  }
  // Valid r value (any 6 bytes will do).
  memcpy(reinterpret_cast<char*>(invalid_whitelist + 8 + num_ff_bytes),
         reinterpret_cast<const char*>(kWhitelistData),
         6 * sizeof(char));

  std::vector<uint64_t> res;
  EXPECT_FALSE(PackedEVCertsWhitelist::UncompressEVWhitelist(
      std::string(reinterpret_cast<const char*>(invalid_whitelist),
                  total_size - 1),
      &res));
  delete[] invalid_whitelist;
}

TEST(PackedEVCertsWhitelistTest, UncompressesWhitelistCorrectly) {
  std::vector<uint64_t> res;
  ASSERT_TRUE(PackedEVCertsWhitelist::UncompressEVWhitelist(
      GetAllWhitelistData(), &res));
  ASSERT_TRUE(res.size() == res.capacity());

  // Ensure first hash is found
  EXPECT_TRUE(std::find(res.begin(), res.end(), HashToUint64(GetFirstHash())) !=
              res.end());
  // Ensure second hash is found
  EXPECT_TRUE(std::find(res.begin(),
                        res.end(),
                        HashToUint64(GetSecondHash())) != res.end());
  // Ensure last hash is found
  EXPECT_TRUE(std::find(res.begin(), res.end(), HashToUint64(GetThirdHash())) !=
              res.end());
  // Ensure that there are exactly 3 hashes.
  EXPECT_EQ(3u, res.size());
}

TEST(PackedEVCertsWhitelistTest, CanFindHashInSetList) {
  scoped_refptr<PackedEVCertsWhitelist> whitelist(
      new PackedEVCertsWhitelist(GetAllWhitelistData(), base::Version()));

  EXPECT_TRUE(whitelist->IsValid());
  EXPECT_TRUE(whitelist->ContainsCertificateHash(GetFirstHash()));
  EXPECT_TRUE(whitelist->ContainsCertificateHash(GetSecondHash()));
  EXPECT_TRUE(whitelist->ContainsCertificateHash(GetThirdHash()));
}

TEST(PackedEVCertsWhitelistTest, CorrectlyIdentifiesEmptyWhitelistIsInvalid) {
  scoped_refptr<PackedEVCertsWhitelist> whitelist(
      new PackedEVCertsWhitelist(std::string(), base::Version()));

  EXPECT_FALSE(whitelist->IsValid());
}

TEST(PackedEVCertsWhitelistTest, CorrectlyIdentifiesPartialWhitelistIsInvalid) {
  scoped_refptr<PackedEVCertsWhitelist> whitelist(
      new PackedEVCertsWhitelist(GetPartialWhitelistData(14), base::Version()));

  EXPECT_FALSE(whitelist->IsValid());
}

TEST(PackedEVCertsWhitelistTest, CorrectlyIdentifiesWhitelistIsValid) {
  scoped_refptr<PackedEVCertsWhitelist> whitelist(
      new PackedEVCertsWhitelist(GetAllWhitelistData(), base::Version()));

  EXPECT_TRUE(whitelist->IsValid());
}

}  //  namespace packed_ct_ev_whitelist

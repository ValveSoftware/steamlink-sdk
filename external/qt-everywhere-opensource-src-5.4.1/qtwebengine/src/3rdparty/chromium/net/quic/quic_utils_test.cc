// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_utils.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;

namespace net {
namespace test {
namespace {

// A test string and a hex+ASCII dump of the same string.
const unsigned char kString[] = {
  0x00, 0x90, 0x69, 0xbd, 0x54, 0x00, 0x00, 0x0d,
  0x61, 0x0f, 0x01, 0x89, 0x08, 0x00, 0x45, 0x00,
  0x00, 0x1c, 0xfb, 0x98, 0x40, 0x00, 0x40, 0x01,
  0x7e, 0x18, 0xd8, 0xef, 0x23, 0x01, 0x45, 0x5d,
  0x7f, 0xe2, 0x08, 0x00, 0x6b, 0xcb, 0x0b, 0xc6,
  0x80, 0x6e
};

const unsigned char kHexDump[] =
"0x0000:  0090 69bd 5400 000d 610f 0189 0800 4500  ..i.T...a.....E.\n"
"0x0010:  001c fb98 4000 4001 7e18 d8ef 2301 455d  ....@.@.~...#.E]\n"
"0x0020:  7fe2 0800 6bcb 0bc6 806e                 ....k....n\n";

TEST(QuicUtilsTest, StreamErrorToString) {
  EXPECT_STREQ("QUIC_BAD_APPLICATION_PAYLOAD",
               QuicUtils::StreamErrorToString(QUIC_BAD_APPLICATION_PAYLOAD));
}

TEST(QuicUtilsTest, ErrorToString) {
  EXPECT_STREQ("QUIC_NO_ERROR",
               QuicUtils::ErrorToString(QUIC_NO_ERROR));
}

TEST(QuicUtilsTest, StringToHexASCIIDumpArgTypes) {
  // Verify that char*, string and StringPiece are all valid argument types.
  struct {
    const string input;
    const string expected;
  } tests[] = {
    { "", "", },
    { "A",   "0x0000:  41                                       A\n", },
    { "AB",  "0x0000:  4142                                     AB\n", },
    { "ABC", "0x0000:  4142 43                                  ABC\n", },
    { "original",
      "0x0000:  6f72 6967 696e 616c                      original\n", },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    EXPECT_EQ(tests[i].expected,
              QuicUtils::StringToHexASCIIDump(tests[i].input.c_str()));
    EXPECT_EQ(tests[i].expected,
              QuicUtils::StringToHexASCIIDump(tests[i].input));
    EXPECT_EQ(tests[i].expected,
              QuicUtils::StringToHexASCIIDump(StringPiece(tests[i].input)));
  }
}

TEST(QuicUtilsTest, StringToHexASCIIDumpSuccess) {
  EXPECT_EQ(string(reinterpret_cast<const char*>(kHexDump)),
      QuicUtils::StringToHexASCIIDump(
          string(reinterpret_cast<const char*>(kString), sizeof(kString))));
}

TEST(QuicUtilsTest, TagToString) {
  EXPECT_EQ("SCFG",
            QuicUtils::TagToString(kSCFG));
  EXPECT_EQ("SNO ",
            QuicUtils::TagToString(kServerNonceTag));
  EXPECT_EQ("CRT ",
            QuicUtils::TagToString(kCertificateTag));
  EXPECT_EQ("CHLO",
            QuicUtils::TagToString(MakeQuicTag('C', 'H', 'L', 'O')));
  // A tag that contains a non-printing character will be printed as a decimal
  // number.
  EXPECT_EQ("525092931",
            QuicUtils::TagToString(MakeQuicTag('C', 'H', 'L', '\x1f')));
}

}  // namespace
}  // namespace test
}  // namespace net

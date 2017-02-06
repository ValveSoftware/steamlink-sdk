// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_type_converters.h"

#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {
namespace common {
namespace test {
namespace {

void ExpectEqualsStringPiece(const std::string& expected,
                             const base::StringPiece& str) {
  EXPECT_EQ(expected, str.as_string());
}

void ExpectEqualsMojoString(const std::string& expected,
                            const String& str) {
  EXPECT_EQ(expected, str.get());
}

void ExpectEqualsString16(const base::string16& expected,
                          const base::string16& actual) {
  EXPECT_EQ(expected, actual);
}

void ExpectEqualsMojoString(const base::string16& expected,
                            const String& str) {
  EXPECT_EQ(expected, str.To<base::string16>());
}

}  // namespace

TEST(CommonTypeConvertersTest, StringPiece) {
  std::string kText("hello world");

  base::StringPiece string_piece(kText);
  String mojo_string(String::From(string_piece));

  ExpectEqualsMojoString(kText, mojo_string);
  ExpectEqualsStringPiece(kText, mojo_string.To<base::StringPiece>());

  // Test implicit construction and conversion:
  ExpectEqualsMojoString(kText, String::From(string_piece));
  ExpectEqualsStringPiece(kText, mojo_string.To<base::StringPiece>());

  // Test null String:
  base::StringPiece empty_string_piece = String().To<base::StringPiece>();
  EXPECT_TRUE(empty_string_piece.empty());
}

TEST(CommonTypeConvertersTest, String16) {
  const base::string16 string16(base::ASCIIToUTF16("hello world"));
  const String mojo_string(String::From(string16));

  ExpectEqualsMojoString(string16, mojo_string);
  EXPECT_EQ(string16, mojo_string.To<base::string16>());

  // Test implicit construction and conversion:
  ExpectEqualsMojoString(string16, String::From(string16));
  ExpectEqualsString16(string16, mojo_string.To<base::string16>());

  // Test empty string conversion.
  ExpectEqualsMojoString(base::string16(), String::From(base::string16()));
}

TEST(CommonTypeConvertersTest, ArrayUint8ToStdString) {
  Array<uint8_t> data(4);
  data[0] = 'd';
  data[1] = 'a';
  data[2] = 't';
  data[3] = 'a';

  EXPECT_EQ("data", data.To<std::string>());
}

TEST(CommonTypeConvertersTest, StdStringToArrayUint8) {
  std::string input("data");
  Array<uint8_t> data = Array<uint8_t>::From(input);

  ASSERT_EQ(4ul, data.size());
  EXPECT_EQ('d', data[0]);
  EXPECT_EQ('a', data[1]);
  EXPECT_EQ('t', data[2]);
  EXPECT_EQ('a', data[3]);
}

TEST(CommonTypeConvertersTest, ArrayUint8ToString16) {
  Array<uint8_t> data(8);
  data[0] = 'd';
  data[2] = 'a';
  data[4] = 't';
  data[6] = 'a';

  EXPECT_EQ(base::ASCIIToUTF16("data"), data.To<base::string16>());
}

TEST(CommonTypeConvertersTest, String16ToArrayUint8) {
  base::string16 input(base::ASCIIToUTF16("data"));
  Array<uint8_t> data = Array<uint8_t>::From(input);

  ASSERT_EQ(8ul, data.size());
  EXPECT_EQ('d', data[0]);
  EXPECT_EQ('a', data[2]);
  EXPECT_EQ('t', data[4]);
  EXPECT_EQ('a', data[6]);
}

TEST(CommonTypeConvertersTest, String16ToArrayUint8AndBack) {
  base::string16 input(base::ASCIIToUTF16("data"));
  Array<uint8_t> data = Array<uint8_t>::From(input);
  EXPECT_EQ(input, data.To<base::string16>());
}

TEST(CommonTypeConvertersTest, EmptyStringToArrayUint8) {
  Array<uint8_t> data = Array<uint8_t>::From(std::string());

  ASSERT_EQ(0ul, data.size());
  EXPECT_FALSE(data.is_null());
}

}  // namespace test
}  // namespace common
}  // namespace mojo

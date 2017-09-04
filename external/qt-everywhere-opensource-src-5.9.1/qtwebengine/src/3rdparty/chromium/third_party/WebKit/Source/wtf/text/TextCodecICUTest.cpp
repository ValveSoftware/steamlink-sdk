// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/TextCodecICU.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include "wtf/text/CharacterNames.h"

namespace WTF {

TEST(TextCodecICUTest, IgnorableCodePoint) {
  TextEncoding iso2022jp("iso-2022-jp");
  std::unique_ptr<TextCodec> codec = TextCodecICU::create(iso2022jp, nullptr);
  Vector<UChar> source;
  source.append('a');
  source.append(zeroWidthJoinerCharacter);
  CString encoded =
      codec->encode(source.data(), source.size(), EntitiesForUnencodables);
  EXPECT_STREQ("a&#8205;", encoded.data());
}

TEST(TextCodecICUTest, UTF32AndQuestionMarks) {
  Vector<String> aliases;
  Vector<CString> results;

  const UChar poo[] = {0xd83d, 0xdca9};  // U+1F4A9 PILE OF POO

  aliases.append("UTF-32");
  results.append("\xFF\xFE\x00\x00\xA9\xF4\x01\x00");

  aliases.append("UTF-32LE");
  results.append("\xA9\xF4\x01\x00");

  aliases.append("UTF-32BE");
  results.append("\x00\x01\xF4\xA9");

  ASSERT_EQ(aliases.size(), results.size());
  for (unsigned i = 0; i < aliases.size(); ++i) {
    const String& alias = aliases[i];
    const CString& expected = results[i];

    TextEncoding encoding(alias);
    std::unique_ptr<TextCodec> codec = TextCodecICU::create(encoding, nullptr);
    {
      const UChar* data = nullptr;
      CString encoded = codec->encode(data, 0, QuestionMarksForUnencodables);
      EXPECT_STREQ("", encoded.data());
    }
    {
      CString encoded = codec->encode(poo, WTF_ARRAY_LENGTH(poo),
                                      QuestionMarksForUnencodables);
      EXPECT_STREQ(expected.data(), encoded.data());
    }
  }
}
}

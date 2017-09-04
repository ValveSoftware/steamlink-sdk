// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace data_util {

TEST(AutofillDataUtilTest, IsCJKName) {
  typedef struct {
    const char* full_name;
    bool is_cjk;
  } TestCase;

  TestCase test_cases[] = {
     // Non-CJK language with only ASCII characters.
    {"Homer Jay Simpson", false},
    // Non-CJK language with some ASCII characters.
    {"Éloïse Paré", false},
    // Non-CJK language with no ASCII characters.
    {"Σωκράτης", false},

    // (Simplified) Chinese name, Unihan.
    {"刘翔", true},
    // (Simplified) Chinese name, Unihan, with an ASCII space.
    {"成 龙", true},
    // Korean name, Hangul.
    {"송지효", true},
    // Korean name, Hangul, with an 'IDEOGRAPHIC SPACE' (U+3000).
    {"김　종국", true},
    // Japanese name, Unihan.
    {"山田貴洋", true},
    // Japanese name, Katakana, with a 'KATAKANA MIDDLE DOT' (U+30FB).
    {"ビル・ゲイツ", true},
    // Japanese name, Katakana, with a 'MIDDLE DOT' (U+00B7) (likely a typo).
    {"ビル·ゲイツ", true},

    // CJK names don't have a middle name, so a 3-part name is bogus to us.
    {"반 기 문", false}
  };

  for (const TestCase& test_case : test_cases) {
    EXPECT_EQ(test_case.is_cjk,
              IsCJKName(base::UTF8ToUTF16(test_case.full_name)))
        << "Failed for: " << test_case.full_name;
  }
}

TEST(AutofillDataUtilTest, SplitName) {
  typedef struct {
    std::string full_name;
    std::string given_name;
    std::string middle_name;
    std::string family_name;

  } TestCase;

  const TestCase test_cases[] = {
      // Full name including given, middle and family names.
      {"Homer Jay Simpson", "Homer", "Jay", "Simpson"},
      // No middle name.
      {"Moe Szyslak", "Moe", "", "Szyslak"},
      // Common name prefixes removed.
      {"Reverend Timothy Lovejoy", "Timothy", "", "Lovejoy"},
      // Common name suffixes removed.
      {"John Frink Phd", "John", "", "Frink"},
      // Exception to the name suffix removal.
      {"John Ma", "John", "", "Ma"},
      // Common family name prefixes not considered a middle name.
      {"Milhouse Van Houten", "Milhouse", "", "Van Houten"},

      // CJK names have reverse order (surname goes first, given name goes
      // second).
      {"孫 德明", "德明", "", "孫"}, // Chinese name, Unihan
      {"孫　德明", "德明", "", "孫"}, // Chinese name, Unihan, 'IDEOGRAPHIC SPACE'
      {"홍 길동", "길동", "", "홍"}, // Korean name, Hangul
      {"山田 貴洋", "貴洋", "", "山田"}, // Japanese name, Unihan

      // In Japanese, foreign names use 'KATAKANA MIDDLE DOT' (U+30FB) as a
      // separator. There is no consensus for the ordering. For now, we use the
      // same ordering as regular Japanese names ("last・first").
      {"ゲイツ・ビル", "ビル", "", "ゲイツ"}, // Foreign name in Japanese, Katakana
      // 'KATAKANA MIDDLE DOT' is occasionally typoed as 'MIDDLE DOT' (U+00B7).
      {"ゲイツ·ビル", "ビル", "", "ゲイツ"}, // Foreign name in Japanese, Katakana

      // CJK names don't usually have a space in the middle, but most of the
      // time, the surname is only one character (in Chinese & Korean).
      {"최성훈", "성훈", "", "최"}, // Korean name, Hangul
      {"刘翔", "翔", "", "刘"}, // (Simplified) Chinese name, Unihan
      {"劉翔", "翔", "", "劉"}, // (Traditional) Chinese name, Unihan

      // There are a few exceptions. Occasionally, the surname has two
      // characters.
      {"남궁도", "도", "", "남궁"}, // Korean name, Hangul
      {"황보혜정", "혜정", "", "황보"}, // Korean name, Hangul
      {"歐陽靖", "靖", "", "歐陽"}, // (Traditional) Chinese name, Unihan

      // In Korean, some 2-character surnames are rare/ambiguous, like "강전":
      // "강" is a common surname, and "전" can be part of a given name. In
      // those cases, we assume it's 1/2 for 3-character names, or 2/2 for
      // 4-character names.
      {"강전희", "전희", "", "강"}, // Korean name, Hangul
      {"황목치승", "치승", "", "황목"}, // Korean name, Hangul

      // It occasionally happens that a full name is 2 characters, 1/1.
      {"이도", "도", "", "이"}, // Korean name, Hangul
      {"孫文", "文", "", "孫"} // Chinese name, Unihan
  };

  for (TestCase test_case : test_cases) {
    NameParts name_parts = SplitName(base::UTF8ToUTF16(test_case.full_name));

    EXPECT_EQ(base::UTF8ToUTF16(test_case.given_name), name_parts.given);
    EXPECT_EQ(base::UTF8ToUTF16(test_case.middle_name), name_parts.middle);
    EXPECT_EQ(base::UTF8ToUTF16(test_case.family_name), name_parts.family);
  }
}

TEST(AutofillDataUtilTest, JoinNameParts) {
  typedef struct {
    std::string given_name;
    std::string middle_name;
    std::string family_name;
    std::string full_name;
  } TestCase;

  TestCase test_cases[] = {
    // Full name including given, middle and family names.
    {"Homer", "Jay", "Simpson", "Homer Jay Simpson"},
    // No middle name.
    {"Moe", "", "Szyslak", "Moe Szyslak"},

    // CJK names have reversed order, no space.
    {"德明", "", "孫", "孫德明"}, // Chinese name, Unihan
    {"길동", "", "홍", "홍길동"}, // Korean name, Hangul
    {"貴洋", "", "山田", "山田貴洋"}, // Japanese name, Unihan

    // These are no CJK names for us, they're just bogus.
    {"Homer", "", "シンプソン", "Homer シンプソン"},
    {"ホーマー", "", "Simpson", "ホーマー Simpson"},
    {"반", "기", "문", "반 기 문"} // Has a middle-name, too unusual
  };

  for (const TestCase& test_case : test_cases) {
    base::string16 joined = JoinNameParts(
        base::UTF8ToUTF16(test_case.given_name),
        base::UTF8ToUTF16(test_case.middle_name),
        base::UTF8ToUTF16(test_case.family_name));

    EXPECT_EQ(base::UTF8ToUTF16(test_case.full_name), joined);
  }
}

TEST(AutofillDataUtilTest, ProfileMatchesFullName) {
  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(
      &profile, "First", "Middle", "Last", "fml@example.com", "Acme inc",
      "123 Main", "Apt 2", "Laredo", "TX", "77300", "US", "832-555-1000");

  EXPECT_TRUE(ProfileMatchesFullName(base::UTF8ToUTF16("First Last"), profile));

  EXPECT_TRUE(
      ProfileMatchesFullName(base::UTF8ToUTF16("First Middle Last"), profile));

  EXPECT_TRUE(
      ProfileMatchesFullName(base::UTF8ToUTF16("First M Last"), profile));

  EXPECT_TRUE(
      ProfileMatchesFullName(base::UTF8ToUTF16("First M. Last"), profile));

  EXPECT_TRUE(
      ProfileMatchesFullName(base::UTF8ToUTF16("Last First"), profile));

  EXPECT_TRUE(
      ProfileMatchesFullName(base::UTF8ToUTF16("LastFirst"), profile));

  EXPECT_FALSE(
      ProfileMatchesFullName(base::UTF8ToUTF16("Kirby Puckett"), profile));
}

}  // namespace data_util
}  // namespace autofill

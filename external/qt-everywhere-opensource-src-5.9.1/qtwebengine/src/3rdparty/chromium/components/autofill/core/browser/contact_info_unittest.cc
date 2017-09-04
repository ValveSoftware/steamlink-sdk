// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_info.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

struct FullNameTestCase {
  std::string full_name_input;
  std::string given_name_output;
  std::string middle_name_output;
  std::string family_name_output;
} full_name_test_cases[] = {
  { "", "", "", "" },
  { "John Smith", "John", "", "Smith" },
  { "Julien van der Poel", "Julien", "", "van der Poel" },
  { "John J Johnson", "John", "J", "Johnson" },
  { "John Smith, Jr.", "John", "", "Smith" },
  { "Mr John Smith", "John", "", "Smith" },
  { "Mr. John Smith", "John", "", "Smith" },
  { "Mr. John Smith, M.D.", "John", "", "Smith" },
  { "Mr. John Smith, MD", "John", "", "Smith" },
  { "Mr. John Smith MD", "John", "", "Smith" },
  { "William Hubert J.R.", "William", "Hubert", "J.R." },
  { "John Ma", "John", "", "Ma" },
  { "John Smith, MA", "John", "", "Smith" },
  { "John Jacob Jingleheimer Smith", "John Jacob", "Jingleheimer", "Smith" },
  { "Virgil", "Virgil", "", "" },
  { "Murray Gell-Mann", "Murray", "", "Gell-Mann" },
  { "Mikhail Yevgrafovich Saltykov-Shchedrin", "Mikhail", "Yevgrafovich",
    "Saltykov-Shchedrin" },
  { "Arthur Ignatius Conan Doyle", "Arthur Ignatius", "Conan", "Doyle" },
};

TEST(NameInfoTest, SetFullName) {
  for (const FullNameTestCase& test_case : full_name_test_cases) {
    SCOPED_TRACE(test_case.full_name_input);

    NameInfo name;
    name.SetInfo(AutofillType(NAME_FULL),
                 ASCIIToUTF16(test_case.full_name_input),
                 "en-US");
    EXPECT_EQ(ASCIIToUTF16(test_case.given_name_output),
              name.GetInfo(AutofillType(NAME_FIRST), "en-US"));
    EXPECT_EQ(ASCIIToUTF16(test_case.middle_name_output),
              name.GetInfo(AutofillType(NAME_MIDDLE), "en-US"));
    EXPECT_EQ(ASCIIToUTF16(test_case.family_name_output),
              name.GetInfo(AutofillType(NAME_LAST), "en-US"));
    EXPECT_EQ(ASCIIToUTF16(test_case.full_name_input),
              name.GetInfo(AutofillType(NAME_FULL), "en-US"));
  }
}

TEST(NameInfoTest, GetFullName) {
  NameInfo name;
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Middle"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Middle"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("Middle Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(ASCIIToUTF16("First"), name.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Middle"), name.GetRawInfo(NAME_MIDDLE));
  EXPECT_EQ(ASCIIToUTF16("Last"), name.GetRawInfo(NAME_LAST));
  EXPECT_EQ(base::string16(), name.GetRawInfo(NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  name.SetRawInfo(NAME_FULL, ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last, MD"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  // Setting a name to the value it already has: no change.
  name.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("First"), "en-US");
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last, MD"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  // Setting raw info: no change. (Even though this leads to a slightly
  // inconsitent state.)
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Second"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Second"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle Last, MD"));
  EXPECT_EQ(ASCIIToUTF16("First Middle Last, MD"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));

  // Changing something (e.g., the first name) clears the stored full name.
  name.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Third"), "en-US");
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Third"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(ASCIIToUTF16("Third Middle Last"),
            name.GetInfo(AutofillType(NAME_FULL), "en-US"));
}

TEST(NameInfoTest, ParsedNamesAreEqual) {
  struct TestCase {
    std::string starting_names[3];
    std::string additional_names[3];
    bool expected_result;
  };

  struct TestCase test_cases[] = {
      // Identical name comparison.
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion", "Mitchell", "Morrison"},
       true},

      // Case-sensitive comparisons.
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion", "Mitchell", "MORRISON"},
       false},
      {{"Marion", "Mitchell", "Morrison"},
       {"MARION", "Mitchell", "MORRISON"},
       false},
      {{"Marion", "Mitchell", "Morrison"},
       {"MARION", "MITCHELL", "MORRISON"},
       false},
      {{"Marion", "", "Mitchell Morrison"},
       {"MARION", "", "MITCHELL MORRISON"},
       false},
      {{"Marion Mitchell", "", "Morrison"},
       {"MARION MITCHELL", "", "MORRISON"},
       false},

      // Identical full names but different canonical forms.
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion", "", "Mitchell Morrison"},
       false},
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion Mitchell", "", "MORRISON"},
       false},

      // Different names.
      {{"Marion", "Mitchell", "Morrison"}, {"Marion", "M.", "Morrison"}, false},
      {{"Marion", "Mitchell", "Morrison"}, {"MARION", "M.", "MORRISON"}, false},
      {{"Marion", "Mitchell", "Morrison"},
       {"David", "Mitchell", "Morrison"},
       false},

      // Non-ASCII characters.
      {{"M\xc3\xa1rion Mitchell", "", "Morrison"},
       {"M\xc3\xa1rion Mitchell", "", "Morrison"},
       true},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));

    // Construct the starting_profile.
    NameInfo starting_profile;
    starting_profile.SetRawInfo(NAME_FIRST,
                                UTF8ToUTF16(test_cases[i].starting_names[0]));
    starting_profile.SetRawInfo(NAME_MIDDLE,
                                UTF8ToUTF16(test_cases[i].starting_names[1]));
    starting_profile.SetRawInfo(NAME_LAST,
                                UTF8ToUTF16(test_cases[i].starting_names[2]));

    // Construct the additional_profile.
    NameInfo additional_profile;
    additional_profile.SetRawInfo(
        NAME_FIRST, UTF8ToUTF16(test_cases[i].additional_names[0]));
    additional_profile.SetRawInfo(
        NAME_MIDDLE, UTF8ToUTF16(test_cases[i].additional_names[1]));
    additional_profile.SetRawInfo(
        NAME_LAST, UTF8ToUTF16(test_cases[i].additional_names[2]));

    // Verify the test expectations.
    EXPECT_EQ(test_cases[i].expected_result,
              starting_profile.ParsedNamesAreEqual(additional_profile));
  }
}

TEST(NameInfoTest, OverwriteName) {
  struct TestCase {
    std::string existing_name[4];
    std::string new_name[4];
    std::string expected_name[4];
  };

  struct TestCase test_cases[] = {
      // Missing information in the original name gets filled with the new
      // name's information.
      {
          {"", "", "", ""},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
      },
      // The new name's values overwrite the exsiting name values if they are
      // different
      {
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
          {"Mario", "Mitchell", "Thompson", "Mario Mitchell Morrison"},
          {"Mario", "Mitchell", "Thompson", "Mario Mitchell Morrison"},
      },
      // An existing name values do not get replaced with empty values.
      {
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
          {"", "", "", ""},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
      },
      // An existing full middle not does not get replaced by a middle name
      // initial.
      {
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
          {"Marion", "M", "Morrison", "Marion Mitchell Morrison"},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
      },
      // An existing middle name initial is overwritten by the new profile's
      // middle name value.
      {
          {"Marion", "M", "Morrison", "Marion Mitchell Morrison"},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
      },
      // A NameInfo with only the full name set overwritten with a NameInfo
      // with only the name parts set result in a NameInfo with all the name
      // parts and name full set.
      {
          {"", "", "", "Marion Mitchell Morrison"},
          {"Marion", "Mitchell", "Morrison", ""},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
      },
      // A NameInfo with only the name parts set overwritten with a NameInfo
      // with only the full name set result in a NameInfo with all the name
      // parts and name full set.
      {
          {"Marion", "Mitchell", "Morrison", ""},
          {"", "", "", "Marion Mitchell Morrison"},
          {"Marion", "Mitchell", "Morrison", "Marion Mitchell Morrison"},
      },
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));

    // Construct the starting_profile.
    NameInfo existing_name;
    existing_name.SetRawInfo(NAME_FIRST,
                             UTF8ToUTF16(test_cases[i].existing_name[0]));
    existing_name.SetRawInfo(NAME_MIDDLE,
                             UTF8ToUTF16(test_cases[i].existing_name[1]));
    existing_name.SetRawInfo(NAME_LAST,
                             UTF8ToUTF16(test_cases[i].existing_name[2]));
    existing_name.SetRawInfo(NAME_FULL,
                             UTF8ToUTF16(test_cases[i].existing_name[3]));

    // Construct the additional_profile.
    NameInfo new_name;
    new_name.SetRawInfo(NAME_FIRST, UTF8ToUTF16(test_cases[i].new_name[0]));
    new_name.SetRawInfo(NAME_MIDDLE, UTF8ToUTF16(test_cases[i].new_name[1]));
    new_name.SetRawInfo(NAME_LAST, UTF8ToUTF16(test_cases[i].new_name[2]));
    new_name.SetRawInfo(NAME_FULL, UTF8ToUTF16(test_cases[i].new_name[3]));

    existing_name.OverwriteName(new_name);

    // Verify the test expectations.
    EXPECT_EQ(UTF8ToUTF16(test_cases[i].expected_name[0]),
              existing_name.GetRawInfo(NAME_FIRST));
    EXPECT_EQ(UTF8ToUTF16(test_cases[i].expected_name[1]),
              existing_name.GetRawInfo(NAME_MIDDLE));
    EXPECT_EQ(UTF8ToUTF16(test_cases[i].expected_name[2]),
              existing_name.GetRawInfo(NAME_LAST));
    EXPECT_EQ(UTF8ToUTF16(test_cases[i].expected_name[3]),
              existing_name.GetRawInfo(NAME_FULL));
  }
}

TEST(NameInfoTest, NamePartsAreEmpty) {
  struct TestCase {
    std::string first;
    std::string middle;
    std::string last;
    std::string full;
    bool expected_result;
  };

  struct TestCase test_cases[] = {
      {"", "", "", "", true},
      {"", "", "", "Marion Mitchell Morrison", true},
      {"Marion", "", "", "", false},
      {"", "Mitchell", "", "", false},
      {"", "", "Morrison", "", false},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));

    // Construct the NameInfo.
    NameInfo name;
    name.SetRawInfo(NAME_FIRST, UTF8ToUTF16(test_cases[i].first));
    name.SetRawInfo(NAME_MIDDLE, UTF8ToUTF16(test_cases[i].middle));
    name.SetRawInfo(NAME_LAST, UTF8ToUTF16(test_cases[i].last));
    name.SetRawInfo(NAME_FULL, UTF8ToUTF16(test_cases[i].full));

    // Verify the test expectations.
    EXPECT_EQ(test_cases[i].expected_result, name.NamePartsAreEmpty());
  }
}

}  // namespace autofill

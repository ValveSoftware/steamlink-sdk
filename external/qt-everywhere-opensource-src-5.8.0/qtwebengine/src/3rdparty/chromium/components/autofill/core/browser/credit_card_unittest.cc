// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "grit/components_scaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

const CreditCard::RecordType LOCAL_CARD = CreditCard::LOCAL_CARD;
const CreditCard::RecordType MASKED_SERVER_CARD =
    CreditCard::MASKED_SERVER_CARD;
const CreditCard::RecordType FULL_SERVER_CARD = CreditCard::FULL_SERVER_CARD;

namespace {

// From https://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
const char* const kValidNumbers[] = {
  "378282246310005",
  "3714 4963 5398 431",
  "3787-3449-3671-000",
  "5610591081018250",
  "3056 9309 0259 04",
  "3852-0000-0232-37",
  "6011111111111117",
  "6011 0009 9013 9424",
  "3530-1113-3330-0000",
  "3566002020360505",
  "5555 5555 5555 4444",
  "5105-1051-0510-5100",
  "4111111111111111",
  "4012 8888 8888 1881",
  "4222-2222-2222-2",
  "5019717010103742",
  "6331101999990016",

  // A UnionPay card that doesn't pass the Luhn checksum
  "6200000000000000",
};
const char* const kInvalidNumbers[] = {
  "4111 1111 112", /* too short */
  "41111111111111111115", /* too long */
  "4111-1111-1111-1110", /* wrong Luhn checksum */
  "3056 9309 0259 04aa", /* non-digit characters */
};

}  // namespace

// Tests credit card summary string generation.  This test simulates a variety
// of different possible summary strings.  Variations occur based on the
// existence of credit card number, month, and year fields.
TEST(CreditCardTest, PreviewSummaryAndTypeAndLastFourDigitsStrings) {
  // Case 0: empty credit card.
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com/");
  base::string16 summary0 = credit_card0.Label();
  EXPECT_EQ(base::string16(), summary0);
  base::string16 obfuscated0 = credit_card0.TypeAndLastFourDigits();
  EXPECT_EQ(ASCIIToUTF16("Card"), obfuscated0);

  // Case 00: Empty credit card with empty strings.
  CreditCard credit_card00(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card00,"John Dillinger", "", "", "");
  base::string16 summary00 = credit_card00.Label();
  EXPECT_EQ(base::string16(ASCIIToUTF16("John Dillinger")), summary00);
  base::string16 obfuscated00 = credit_card00.TypeAndLastFourDigits();
  EXPECT_EQ(ASCIIToUTF16("Card"), obfuscated00);

  // Case 1: No credit card number.
  CreditCard credit_card1(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card1,"John Dillinger", "", "01", "2010");
  base::string16 summary1 = credit_card1.Label();
  EXPECT_EQ(base::string16(ASCIIToUTF16("John Dillinger")), summary1);
  base::string16 obfuscated1 = credit_card1.TypeAndLastFourDigits();
  EXPECT_EQ(ASCIIToUTF16("Card"), obfuscated1);

  // Case 2: No month.
  CreditCard credit_card2(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(
      &credit_card2, "John Dillinger", "5105 1051 0510 5100", "", "2010");
  base::string16 summary2 = credit_card2.Label();
  EXPECT_EQ(UTF8ToUTF16(
                "MasterCard\xC2\xA0\xE2\x8B\xAF"
                "5100"),
            summary2);
  base::string16 obfuscated2 = credit_card2.TypeAndLastFourDigits();
  EXPECT_EQ(UTF8ToUTF16(
                "MasterCard\xC2\xA0\xE2\x8B\xAF"
                "5100"),
            obfuscated2);

  // Case 3: No year.
  CreditCard credit_card3(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(
      &credit_card3, "John Dillinger", "5105 1051 0510 5100", "01", "");
  base::string16 summary3 = credit_card3.Label();
  EXPECT_EQ(UTF8ToUTF16(
                "MasterCard\xC2\xA0\xE2\x8B\xAF"
                "5100"),
            summary3);
  base::string16 obfuscated3 = credit_card3.TypeAndLastFourDigits();
  EXPECT_EQ(UTF8ToUTF16(
                "MasterCard\xC2\xA0\xE2\x8B\xAF"
                "5100"),
            obfuscated3);

  // Case 4: Have everything.
  CreditCard credit_card4(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(
      &credit_card4, "John Dillinger", "5105 1051 0510 5100", "01", "2010");
  base::string16 summary4 = credit_card4.Label();
  EXPECT_EQ(UTF8ToUTF16(
                "MasterCard\xC2\xA0\xE2\x8B\xAF"
                "5100, 01/2010"),
            summary4);
  base::string16 obfuscated4 = credit_card4.TypeAndLastFourDigits();
  EXPECT_EQ(UTF8ToUTF16(
                "MasterCard\xC2\xA0\xE2\x8B\xAF"
                "5100"),
            obfuscated4);

  // Case 5: Very long credit card
  CreditCard credit_card5(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(
      &credit_card5,
      "John Dillinger",
      "0123456789 0123456789 0123456789 5105 1051 0510 5100", "01", "2010");
  base::string16 summary5 = credit_card5.Label();
  EXPECT_EQ(UTF8ToUTF16(
                "Card\xC2\xA0\xE2\x8B\xAF"
                "5100, 01/2010"),
            summary5);
  base::string16 obfuscated5 = credit_card5.TypeAndLastFourDigits();
  EXPECT_EQ(UTF8ToUTF16(
                "Card\xC2\xA0\xE2\x8B\xAF"
                "5100"),
            obfuscated5);
}

TEST(CreditCardTest, AssignmentOperator) {
  CreditCard a(base::GenerateGUID(), "some origin");
  test::SetCreditCardInfo(&a, "John Dillinger", "123456789012", "01", "2010");

  // Result of assignment should be logically equal to the original profile.
  CreditCard b(base::GenerateGUID(), "some other origin");
  b = a;
  EXPECT_TRUE(a == b);

  // Assignment to self should not change the profile value.
  a = a;
  EXPECT_TRUE(a == b);
}

TEST(CreditCardTest, SetExpirationYearFromString) {
  static const struct {
    std::string expiration_year;
    int expected_year;
  } kTestCases[] = {
      // Valid values.
      {"2040", 2040},
      {"45", 2045},
      {"045", 2045},
      {"9", 2009},

      // Unrecognized year values.
      {"052045", 0},
      {"123", 0},
      {"y2045", 0},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    CreditCard card(base::GenerateGUID(), "some origin");
    card.SetExpirationYearFromString(
        ASCIIToUTF16(kTestCases[i].expiration_year));

    EXPECT_EQ(kTestCases[i].expected_year, card.expiration_year())
        << kTestCases[i].expiration_year << " " << kTestCases[i].expected_year;
  }
}

TEST(CreditCardTest, SetExpirationDateFromString) {
  static const struct {
    std::string expiration_date;
    int expected_month;
    int expected_year;
  } kTestCases[] = {{"10", 0, 0},       // Too small.
                    {"1020451", 0, 0},  // Too long.

                    // No separators.
                    {"105", 0, 0},  // Too ambiguous.
                    {"0545", 5, 2045},
                    {"52045", 0, 0},  // Too ambiguous.
                    {"052045", 5, 2045},

                    // "/" separator.
                    {"05/45", 5, 2045},
                    {"5/2045", 5, 2045},
                    {"05/2045", 5, 2045},

                    // "-" separator.
                    {"05-45", 5, 2045},
                    {"5-2045", 5, 2045},
                    {"05-2045", 5, 2045},

                    // "|" separator.
                    {"05|45", 5, 2045},
                    {"5|2045", 5, 2045},
                    {"05|2045", 5, 2045},

                    // Invalid values.
                    {"13/2016", 0, 2016},
                    {"16/13", 0, 2013},
                    {"May-2015", 0, 0},
                    {"05-/2045", 0, 0},
                    {"05_2045", 0, 0}};

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    CreditCard card(base::GenerateGUID(), "some origin");
    card.SetExpirationDateFromString(
        ASCIIToUTF16(kTestCases[i].expiration_date));

    EXPECT_EQ(kTestCases[i].expected_month, card.expiration_month());
    EXPECT_EQ(kTestCases[i].expected_year, card.expiration_year());
  }
}

TEST(CreditCardTest, Copy) {
  CreditCard a(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&a, "John Dillinger", "123456789012", "01", "2010");

  // Clone should be logically equal to the original.
  CreditCard b(a);
  EXPECT_TRUE(a == b);
}

TEST(CreditCardTest, IsLocalDuplicateOfServerCard) {
  struct {
    CreditCard::RecordType first_card_record_type;
    const char* first_card_name;
    const char* first_card_number;
    const char* first_card_exp_mo;
    const char* first_card_exp_yr;

    CreditCard::RecordType second_card_record_type;
    const char* second_card_name;
    const char* second_card_number;
    const char* second_card_exp_mo;
    const char* second_card_exp_yr;
    const char* second_card_type;

    bool is_local_duplicate;
  } test_cases[] = {
    { LOCAL_CARD, "", "", "", "",
      LOCAL_CARD, "", "", "", "", nullptr, false },
    { LOCAL_CARD, "", "", "", "",
      FULL_SERVER_CARD, "", "", "", "", nullptr, true},
    { FULL_SERVER_CARD, "", "", "", "",
      FULL_SERVER_CARD, "", "", "", "", nullptr, false},
    { LOCAL_CARD, "John Dillinger", "423456789012", "01", "2010",
      FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010", nullptr,
      true },
    { LOCAL_CARD, "J Dillinger", "423456789012", "01", "2010",
      FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010", nullptr,
      false },
    { LOCAL_CARD, "", "423456789012", "01", "2010",
      FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010", nullptr,
      true },
    { LOCAL_CARD, "", "423456789012", "", "",
      FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010", nullptr,
      true },
    { LOCAL_CARD, "", "423456789012", "", "",
      MASKED_SERVER_CARD, "John Dillinger", "9012", "01", "2010", kVisaCard,
      true },
    { LOCAL_CARD, "", "423456789012", "", "",
      MASKED_SERVER_CARD, "John Dillinger", "9012", "01", "2010", kMasterCard,
      false },
    { LOCAL_CARD, "John Dillinger", "4234-5678-9012", "01", "2010",
      FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010", nullptr,
      true },
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    CreditCard a(base::GenerateGUID(), std::string());
    a.set_record_type(test_cases[i].first_card_record_type);
    test::SetCreditCardInfo(&a,
                            test_cases[i].first_card_name,
                            test_cases[i].first_card_number,
                            test_cases[i].first_card_exp_mo,
                            test_cases[i].first_card_exp_yr);

    CreditCard b(base::GenerateGUID(), std::string());
    b.set_record_type(test_cases[i].second_card_record_type);
    test::SetCreditCardInfo(&b,
                            test_cases[i].second_card_name,
                            test_cases[i].second_card_number,
                            test_cases[i].second_card_exp_mo,
                            test_cases[i].second_card_exp_yr);

    if (test_cases[i].second_card_record_type == CreditCard::MASKED_SERVER_CARD)
      b.SetTypeForMaskedCard(test_cases[i].second_card_type);

    EXPECT_EQ(test_cases[i].is_local_duplicate,
              a.IsLocalDuplicateOfServerCard(b)) << " when comparing cards "
                  << a.Label() << " and " << b.Label();
  }
}

TEST(CreditCardTest, HasSameNumberAs) {
  CreditCard a(base::GenerateGUID(), std::string());
  CreditCard b(base::GenerateGUID(), std::string());

  // Empty cards have the same empty number.
  EXPECT_TRUE(a.HasSameNumberAs(b));
  EXPECT_TRUE(b.HasSameNumberAs(a));

  // Same number.
  a.set_record_type(CreditCard::LOCAL_CARD);
  a.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));
  a.set_record_type(CreditCard::LOCAL_CARD);
  b.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));
  EXPECT_TRUE(a.HasSameNumberAs(b));
  EXPECT_TRUE(b.HasSameNumberAs(a));

  // Local cards shouldn't match even if the last 4 are the same.
  a.set_record_type(CreditCard::LOCAL_CARD);
  a.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));
  a.set_record_type(CreditCard::LOCAL_CARD);
  b.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111222222221111"));
  EXPECT_FALSE(a.HasSameNumberAs(b));
  EXPECT_FALSE(b.HasSameNumberAs(a));

  // Likewise if one is an unmasked server card.
  a.set_record_type(CreditCard::FULL_SERVER_CARD);
  EXPECT_FALSE(a.HasSameNumberAs(b));
  EXPECT_FALSE(b.HasSameNumberAs(a));

  // But if one is a masked card, then they should.
  b.set_record_type(CreditCard::MASKED_SERVER_CARD);
  EXPECT_TRUE(a.HasSameNumberAs(b));
  EXPECT_TRUE(b.HasSameNumberAs(a));
}

TEST(CreditCardTest, Compare) {
  CreditCard a(base::GenerateGUID(), std::string());
  CreditCard b(base::GenerateGUID(), std::string());

  // Empty cards are the same.
  EXPECT_EQ(0, a.Compare(b));

  // GUIDs don't count.
  a.set_guid(base::GenerateGUID());
  b.set_guid(base::GenerateGUID());
  EXPECT_EQ(0, a.Compare(b));

  // Origins don't count.
  a.set_origin("apple");
  b.set_origin("banana");
  EXPECT_EQ(0, a.Compare(b));

  // Different values produce non-zero results.
  test::SetCreditCardInfo(&a, "Jimmy", NULL, NULL, NULL);
  test::SetCreditCardInfo(&b, "Ringo", NULL, NULL, NULL);
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));
}

// This method is not compiled for iOS because these resources are not used and
// should not be shipped.
#if !defined(OS_IOS)
// Test we get the correct icon for each card type.
TEST(CreditCardTest, IconResourceId) {
  EXPECT_EQ(IDR_AUTOFILL_CC_AMEX,
            CreditCard::IconResourceId(kAmericanExpressCard));
  EXPECT_EQ(IDR_AUTOFILL_CC_GENERIC,
            CreditCard::IconResourceId(kDinersCard));
  EXPECT_EQ(IDR_AUTOFILL_CC_DISCOVER,
            CreditCard::IconResourceId(kDiscoverCard));
  EXPECT_EQ(IDR_AUTOFILL_CC_GENERIC,
            CreditCard::IconResourceId(kJCBCard));
  EXPECT_EQ(IDR_AUTOFILL_CC_MASTERCARD,
            CreditCard::IconResourceId(kMasterCard));
  EXPECT_EQ(IDR_AUTOFILL_CC_VISA,
            CreditCard::IconResourceId(kVisaCard));
}
#endif  // #if !defined(OS_IOS)

TEST(CreditCardTest, UpdateFromImportedCard) {
  CreditCard original_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(
      &original_card, "John Dillinger", "123456789012", "09", "2017");

  CreditCard a = original_card;

  // The new card has a different name, expiration date, and origin.
  CreditCard b = a;
  b.set_guid(base::GenerateGUID());
  b.set_origin("https://www.example.org");
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("J. Dillinger"));
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("08"));
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2019"));

  // |a| should be updated with the information from |b|.
  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("https://www.example.org", a.origin());
  EXPECT_EQ(ASCIIToUTF16("J. Dillinger"), a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("08"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2019"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Try again, but with no name set for |b|.
  // |a| should be updated with |b|'s expiration date and keep its original
  // name.
  a = original_card;
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, base::string16());

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("https://www.example.org", a.origin());
  EXPECT_EQ(ASCIIToUTF16("John Dillinger"),
            a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("08"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2019"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Try again, but with only the original card having a verified origin.
  // |a| should be unchanged.
  a = original_card;
  a.set_origin(kSettingsOrigin);
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("J. Dillinger"));

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(kSettingsOrigin, a.origin());
  EXPECT_EQ(ASCIIToUTF16("John Dillinger"),
            a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("09"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2017"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Try again, but with using an expired verified original card.
  // |a| should not be updated because the name on the cards are not identical.
  a = original_card;
  a.set_origin("Chrome settings");
  a.SetExpirationYear(2010);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(ASCIIToUTF16("John Dillinger"),
            a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("09"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2010"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Try again, but with using identical name on the cards.
  // |a|'s expiration date should be updated.
  a = original_card;
  a.set_origin("Chrome settings");
  a.SetExpirationYear(2010);
  a.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("J. Dillinger"));

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(ASCIIToUTF16("J. Dillinger"), a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("08"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2019"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Try again, but with |b| being expired.
  // |a|'s expiration date should not be updated.
  a = original_card;
  a.set_origin("Chrome settings");
  a.SetExpirationYear(2010);
  a.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("J. Dillinger"));
  b.SetExpirationYear(2009);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(ASCIIToUTF16("J. Dillinger"), a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("09"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2010"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Put back |b|'s initial expiration date.
  b.SetExpirationYear(2019);

  // Try again, but with only the new card having a verified origin.
  // |a| should be updated.
  a = original_card;
  b.set_origin(kSettingsOrigin);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(kSettingsOrigin, a.origin());
  EXPECT_EQ(ASCIIToUTF16("J. Dillinger"), a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("08"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2019"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Try again, with both cards having a verified origin.
  // |a| should be updated.
  a = original_card;
  a.set_origin("Chrome Autofill dialog");
  b.set_origin(kSettingsOrigin);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(kSettingsOrigin, a.origin());
  EXPECT_EQ(ASCIIToUTF16("J. Dillinger"), a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(ASCIIToUTF16("08"), a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(ASCIIToUTF16("2019"), a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Try again, but with |b| having a different card number.
  // |a| should be unchanged.
  a = original_card;
  b.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));

  EXPECT_FALSE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(original_card, a);
}

TEST(CreditCardTest, IsValid) {
  CreditCard card;
  // Invalid because expired
  const base::Time now(base::Time::Now());
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH,
                  base::IntToString16(now_exploded.month));
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                  base::IntToString16(now_exploded.year - 1));
  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));
  EXPECT_FALSE(card.IsValid());

  // Invalid because card number is not complete
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("12"));
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2999"));
  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("41111"));
  EXPECT_FALSE(card.IsValid());

  for (size_t i = 0; i < arraysize(kValidNumbers); ++i) {
    SCOPED_TRACE(kValidNumbers[i]);
    card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16(kValidNumbers[i]));
    EXPECT_TRUE(card.IsValid());
  }
  for (size_t i = 0; i < arraysize(kInvalidNumbers); ++i) {
    SCOPED_TRACE(kInvalidNumbers[i]);
    card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16(kInvalidNumbers[i]));
    EXPECT_FALSE(card.IsValid());
  }
}

// Verify that we preserve exactly what the user typed for credit card numbers.
TEST(CreditCardTest, SetRawInfoCreditCardNumber) {
  CreditCard card(base::GenerateGUID(), "https://www.example.com/");

  test::SetCreditCardInfo(&card, "Bob Dylan",
                          "4321-5432-6543-xxxx", "07", "2013");
  EXPECT_EQ(ASCIIToUTF16("4321-5432-6543-xxxx"),
            card.GetRawInfo(CREDIT_CARD_NUMBER));
}

// Verify that we can handle both numeric and named months.
TEST(CreditCardTest, SetExpirationMonth) {
  CreditCard card(base::GenerateGUID(), "https://www.example.com/");

  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("05"));
  EXPECT_EQ(ASCIIToUTF16("05"), card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(5, card.expiration_month());

  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("7"));
  EXPECT_EQ(ASCIIToUTF16("07"), card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(7, card.expiration_month());

  // This should fail, and preserve the previous value.
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("January"));
  EXPECT_EQ(ASCIIToUTF16("07"), card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(7, card.expiration_month());

  card.SetInfo(
      AutofillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("January"), "en-US");
  EXPECT_EQ(ASCIIToUTF16("01"), card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(1, card.expiration_month());

  card.SetInfo(
      AutofillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("Apr"), "en-US");
  EXPECT_EQ(ASCIIToUTF16("04"), card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(4, card.expiration_month());

  card.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
               UTF8ToUTF16("FÉVRIER"), "fr-FR");
  EXPECT_EQ(ASCIIToUTF16("02"), card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(2, card.expiration_month());
}

TEST(CreditCardTest, CreditCardType) {
  CreditCard card(base::GenerateGUID(), "https://www.example.com/");

  // The card type cannot be set directly.
  card.SetRawInfo(CREDIT_CARD_TYPE, ASCIIToUTF16("Visa"));
  EXPECT_EQ(base::string16(), card.GetRawInfo(CREDIT_CARD_TYPE));

  // Setting the number should implicitly set the type.
  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111 1111 1111 1111"));
  EXPECT_EQ(ASCIIToUTF16("Visa"), card.GetRawInfo(CREDIT_CARD_TYPE));
}

TEST(CreditCardTest, CreditCardVerificationCode) {
  CreditCard card(base::GenerateGUID(), "https://www.example.com/");

  // The verification code cannot be set, as Chrome does not store this data.
  card.SetRawInfo(CREDIT_CARD_VERIFICATION_CODE, ASCIIToUTF16("999"));
  EXPECT_EQ(base::string16(), card.GetRawInfo(CREDIT_CARD_VERIFICATION_CODE));
}


TEST(CreditCardTest, GetCreditCardType) {
  struct {
    std::string card_number;
    std::string type;
    bool is_valid;
  } test_cases[] = {
    // The relevant sample numbers from
    // http://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
    { "378282246310005", kAmericanExpressCard, true },
    { "371449635398431", kAmericanExpressCard, true },
    { "378734493671000", kAmericanExpressCard, true },
    { "30569309025904", kDinersCard, true },
    { "38520000023237", kDinersCard, true },
    { "6011111111111117", kDiscoverCard, true },
    { "6011000990139424", kDiscoverCard, true },
    { "3530111333300000", kJCBCard, true },
    { "3566002020360505", kJCBCard, true },
    { "5555555555554444", kMasterCard, true },
    { "5105105105105100", kMasterCard, true },
    { "4111111111111111", kVisaCard, true },
    { "4012888888881881", kVisaCard, true },
    { "4222222222222", kVisaCard, true },

    // The relevant sample numbers from
    // http://auricsystems.com/support-center/sample-credit-card-numbers/
    { "343434343434343", kAmericanExpressCard, true },
    { "371144371144376", kAmericanExpressCard, true },
    { "341134113411347", kAmericanExpressCard, true },
    { "36438936438936", kDinersCard, true },
    { "36110361103612", kDinersCard, true },
    { "36111111111111", kDinersCard, true },
    { "6011016011016011", kDiscoverCard, true },
    { "6011000990139424", kDiscoverCard, true },
    { "6011000000000004", kDiscoverCard, true },
    { "6011000995500000", kDiscoverCard, true },
    { "6500000000000002", kDiscoverCard, true },
    { "3566002020360505", kJCBCard, true },
    { "3528000000000007", kJCBCard, true },
    { "5500005555555559", kMasterCard, true },
    { "5555555555555557", kMasterCard, true },
    { "5454545454545454", kMasterCard, true },
    { "5555515555555551", kMasterCard, true },
    { "5405222222222226", kMasterCard, true },
    { "5478050000000007", kMasterCard, true },
    { "5111005111051128", kMasterCard, true },
    { "5112345112345114", kMasterCard, true },
    { "5115915115915118", kMasterCard, true },

    // A UnionPay card that doesn't pass the Luhn checksum
    { "6200000000000000", kUnionPay, true },

    // Empty string
    { std::string(), kGenericCard, false },

    // Non-numeric
    { "garbage", kGenericCard, false },
    { "4garbage", kVisaCard, false },

    // Fails Luhn check.
    { "4111111111111112", kVisaCard, false },

    // Invalid length.
    { "3434343434343434", kAmericanExpressCard, false },
    { "411111111111116", kVisaCard, false },

    // Issuer Identification Numbers (IINs) that Chrome recognizes.
    { "4", kVisaCard, false },
    { "34", kAmericanExpressCard, false },
    { "37", kAmericanExpressCard, false },
    { "300", kDinersCard, false },
    { "301", kDinersCard, false },
    { "302", kDinersCard, false },
    { "303", kDinersCard, false },
    { "304", kDinersCard, false },
    { "305", kDinersCard, false },
    { "3095", kDinersCard, false },
    { "36", kDinersCard, false },
    { "38", kDinersCard, false },
    { "39", kDinersCard, false },
    { "6011", kDiscoverCard, false },
    { "644", kDiscoverCard, false },
    { "645", kDiscoverCard, false },
    { "646", kDiscoverCard, false },
    { "647", kDiscoverCard, false },
    { "648", kDiscoverCard, false },
    { "649", kDiscoverCard, false },
    { "65", kDiscoverCard, false },
    { "3528", kJCBCard, false },
    { "3531", kJCBCard, false },
    { "3589", kJCBCard, false },
    { "51", kMasterCard, false },
    { "52", kMasterCard, false },
    { "53", kMasterCard, false },
    { "54", kMasterCard, false },
    { "55", kMasterCard, false },
    { "62", kUnionPay, false },

    // Not enough data to determine an IIN uniquely.
    { "3", kGenericCard, false },
    { "30", kGenericCard, false },
    { "309", kGenericCard, false },
    { "35", kGenericCard, false },
    { "5", kGenericCard, false },
    { "6", kGenericCard, false },
    { "60", kGenericCard, false },
    { "601", kGenericCard, false },
    { "64", kGenericCard, false },

    // Unknown IINs.
    { "0", kGenericCard, false },
    { "1", kGenericCard, false },
    { "2", kGenericCard, false },
    { "306", kGenericCard, false },
    { "307", kGenericCard, false },
    { "308", kGenericCard, false },
    { "3091", kGenericCard, false },
    { "3094", kGenericCard, false },
    { "3096", kGenericCard, false },
    { "31", kGenericCard, false },
    { "32", kGenericCard, false },
    { "33", kGenericCard, false },
    { "351", kGenericCard, false },
    { "3527", kGenericCard, false },
    { "359", kGenericCard, false },
    { "50", kGenericCard, false },
    { "56", kGenericCard, false },
    { "57", kGenericCard, false },
    { "58", kGenericCard, false },
    { "59", kGenericCard, false },
    { "600", kGenericCard, false },
    { "602", kGenericCard, false },
    { "603", kGenericCard, false },
    { "604", kGenericCard, false },
    { "605", kGenericCard, false },
    { "606", kGenericCard, false },
    { "607", kGenericCard, false },
    { "608", kGenericCard, false },
    { "609", kGenericCard, false },
    { "61", kGenericCard, false },
    { "63", kGenericCard, false },
    { "640", kGenericCard, false },
    { "641", kGenericCard, false },
    { "642", kGenericCard, false },
    { "643", kGenericCard, false },
    { "66", kGenericCard, false },
    { "67", kGenericCard, false },
    { "68", kGenericCard, false },
    { "69", kGenericCard, false },
    { "7", kGenericCard, false },
    { "8", kGenericCard, false },
    { "9", kGenericCard, false },

    // Oddball case: Unknown issuer, but valid Luhn check and plausible length.
    { "7000700070007000", kGenericCard, true },
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::string16 card_number = ASCIIToUTF16(test_cases[i].card_number);
    SCOPED_TRACE(card_number);
    EXPECT_EQ(test_cases[i].type, CreditCard::GetCreditCardType(card_number));
    EXPECT_EQ(test_cases[i].is_valid, IsValidCreditCardNumber(card_number));
  }
}

TEST(CreditCardTest, LastFourDigits) {
  CreditCard card(base::GenerateGUID(), "https://www.example.com/");
  ASSERT_EQ(base::string16(), card.LastFourDigits());

  test::SetCreditCardInfo(&card, "Baby Face Nelson",
                          "5212341234123489", "01", "2010");
  ASSERT_EQ(base::ASCIIToUTF16("3489"), card.LastFourDigits());

  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("3489"));
  ASSERT_EQ(base::ASCIIToUTF16("3489"), card.LastFourDigits());

  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("489"));
  ASSERT_EQ(base::ASCIIToUTF16("489"), card.LastFourDigits());
}

TEST(CreditCardTest, CanBuildFromCardNumberAndExpirationDate) {
  base::string16 card_number = base::ASCIIToUTF16("test");
  int month = 1;
  int year = 2999;
  CreditCard card(card_number, month, year);
  EXPECT_EQ(card_number, card.number());
  EXPECT_EQ(month, card.expiration_month());
  EXPECT_EQ(year, card.expiration_year());
}

// Verifies that a credit card should be updated.
TEST(CreditCardTest, ShouldUpdateExpiration) {
  base::Time now = base::Time::Now();

  base::Time::Exploded last_year;
  (now - base::TimeDelta::FromDays(365)).LocalExplode(&last_year);

  base::Time::Exploded last_month;
  (now - base::TimeDelta::FromDays(31)).LocalExplode(&last_month);

  base::Time::Exploded current;
  now.LocalExplode(&current);

  base::Time::Exploded next_month;
  (now + base::TimeDelta::FromDays(31)).LocalExplode(&next_month);

  base::Time::Exploded next_year;
  (now + base::TimeDelta::FromDays(365)).LocalExplode(&next_year);

  static const struct {
    bool should_update_expiration;
    int month;
    int year;
    CreditCard::RecordType record_type;
    CreditCard::ServerStatus server_status;
  } kTestCases[] = {

      // Cards that expired last year should always be updated.
      {true, last_year.month, last_year.year, CreditCard::LOCAL_CARD},
      {true, last_year.month, last_year.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::OK},
      {true, last_year.month, last_year.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::OK},
      {true, last_year.month, last_year.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::EXPIRED},
      {true, last_year.month, last_year.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::EXPIRED},

      // Cards that expired last month should always be updated.
      {true, last_month.month, last_month.year, CreditCard::LOCAL_CARD},
      {true, last_month.month, last_month.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::OK},
      {true, last_month.month, last_month.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::OK},
      {true, last_month.month, last_month.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::EXPIRED},
      {true, last_month.month, last_month.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::EXPIRED},

      // Cards that expire this month should be updated only if the server
      // status is EXPIRED.
      {false, current.month, current.year, CreditCard::LOCAL_CARD},
      {false, current.month, current.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::OK},
      {false, current.month, current.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::OK},
      {true, current.month, current.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::EXPIRED},
      {true, current.month, current.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::EXPIRED},

      // Cards that expire next month should be updated only if the server
      // status is EXPIRED.
      {false, next_month.month, next_month.year, CreditCard::LOCAL_CARD},
      {false, next_month.month, next_month.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::OK},
      {false, next_month.month, next_month.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::OK},
      {true, next_month.month, next_month.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::EXPIRED},
      {true, next_month.month, next_month.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::EXPIRED},

      // Cards that expire next year should be updated only if the server status
      // is EXPIRED.
      {false, next_year.month, next_year.year, CreditCard::LOCAL_CARD},
      {false, next_year.month, next_year.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::OK},
      {false, next_year.month, next_year.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::OK},
      {true, next_year.month, next_year.year, CreditCard::MASKED_SERVER_CARD,
       CreditCard::EXPIRED},
      {true, next_year.month, next_year.year, CreditCard::FULL_SERVER_CARD,
       CreditCard::EXPIRED},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    CreditCard card(base::ASCIIToUTF16("1234"), kTestCases[i].month,
                    kTestCases[i].year);
    card.set_record_type(kTestCases[i].record_type);
    if (card.record_type() != CreditCard::LOCAL_CARD)
      card.SetServerStatus(kTestCases[i].server_status);

    EXPECT_EQ(kTestCases[i].should_update_expiration,
              card.ShouldUpdateExpiration(now));
  }
}

}  // namespace autofill

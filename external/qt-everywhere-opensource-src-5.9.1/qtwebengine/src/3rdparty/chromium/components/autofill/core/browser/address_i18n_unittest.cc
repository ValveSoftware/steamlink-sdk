// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_i18n.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"

namespace autofill {
namespace i18n {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;

using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::ORGANIZATION;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::RECIPIENT;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::STREET_ADDRESS;

TEST(AddressI18nTest, FieldTypeMirrorConversions) {
  static const struct {
    bool billing;
    ServerFieldType server_field;
    AddressField address_field;
  } kTestData[] = {
    {true, ADDRESS_BILLING_COUNTRY, COUNTRY},
    {true, ADDRESS_BILLING_STATE, ADMIN_AREA},
    {true, ADDRESS_BILLING_CITY, LOCALITY},
    {true, ADDRESS_BILLING_DEPENDENT_LOCALITY, DEPENDENT_LOCALITY},
    {true, ADDRESS_BILLING_SORTING_CODE, SORTING_CODE},
    {true, ADDRESS_BILLING_ZIP, POSTAL_CODE},
    {true, ADDRESS_BILLING_STREET_ADDRESS, STREET_ADDRESS},
    {true, COMPANY_NAME, ORGANIZATION},
    {true, NAME_BILLING_FULL, RECIPIENT},
    {false, ADDRESS_HOME_COUNTRY, COUNTRY},
    {false, ADDRESS_HOME_STATE, ADMIN_AREA},
    {false, ADDRESS_HOME_CITY, LOCALITY},
    {false, ADDRESS_HOME_DEPENDENT_LOCALITY, DEPENDENT_LOCALITY},
    {false, ADDRESS_HOME_SORTING_CODE, SORTING_CODE},
    {false, ADDRESS_HOME_ZIP, POSTAL_CODE},
    {false, ADDRESS_HOME_STREET_ADDRESS, STREET_ADDRESS},
    {false, COMPANY_NAME, ORGANIZATION},
    {false, NAME_FULL, RECIPIENT},
  };

  for (const auto& test_data : kTestData) {
    AddressField address_field;
    EXPECT_TRUE(FieldForType(test_data.server_field, &address_field));
    EXPECT_EQ(test_data.address_field, address_field);

    ServerFieldType server_field =
        TypeForField(test_data.address_field, test_data.billing);
    EXPECT_EQ(test_data.server_field, server_field);
  }
}

TEST(AddressI18nTest, FieldTypeUnidirectionalConversions) {
  static const struct {
    ServerFieldType server_field;
    AddressField expected_address_field;
  } kTestData[] = {
    {ADDRESS_BILLING_LINE1, STREET_ADDRESS},
    {ADDRESS_BILLING_LINE2, STREET_ADDRESS},
    {ADDRESS_HOME_LINE1, STREET_ADDRESS},
    {ADDRESS_HOME_LINE2, STREET_ADDRESS},
  };

  for (const auto& test_data : kTestData) {
    AddressField actual_address_field;
    FieldForType(test_data.server_field, &actual_address_field);
    EXPECT_EQ(test_data.expected_address_field, actual_address_field);
  }
}

TEST(AddressI18nTest, UnconvertableServerFields) {
  EXPECT_FALSE(FieldForType(PHONE_HOME_NUMBER, NULL));
  EXPECT_FALSE(FieldForType(EMAIL_ADDRESS, NULL));
}

TEST(AddressI18nTest, CreateAddressDataFromAutofillProfile) {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  test::SetProfileInfo(&profile,
                       "John",
                       "H.",
                       "Doe",
                       "johndoe@hades.com",
                       "Underworld",
                       "666 Erebus St.",
                       "Apt 8",
                       "Elysium", "CA",
                       "91111",
                       "US",
                       "16502111111");
  profile.set_language_code("en");
  std::unique_ptr<AddressData> actual =
      CreateAddressDataFromAutofillProfile(profile, "en_US");

  AddressData expected;
  expected.region_code = "US";
  expected.address_line.push_back("666 Erebus St.");
  expected.address_line.push_back("Apt 8");
  expected.administrative_area = "CA";
  expected.locality = "Elysium";
  expected.postal_code = "91111";
  expected.language_code = "en";
  expected.organization = "Underworld";
  expected.recipient = "John H. Doe";

  EXPECT_EQ(expected, *actual);
}

}  // namespace i18n
}  // namespace autofill

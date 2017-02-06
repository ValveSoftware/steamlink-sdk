// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/pref_hash_calculator.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefHashCalculatorTest, TestCurrentAlgorithm) {
  base::StringValue string_value_1("string value 1");
  base::StringValue string_value_2("string value 2");
  base::DictionaryValue dictionary_value_1;
  dictionary_value_1.SetInteger("int value", 1);
  dictionary_value_1.Set("nested empty map", new base::DictionaryValue);
  base::DictionaryValue dictionary_value_1_equivalent;
  dictionary_value_1_equivalent.SetInteger("int value", 1);
  base::DictionaryValue dictionary_value_2;
  dictionary_value_2.SetInteger("int value", 2);

  PrefHashCalculator calc1("seed1", "deviceid");
  PrefHashCalculator calc1_dup("seed1", "deviceid");
  PrefHashCalculator calc2("seed2", "deviceid");
  PrefHashCalculator calc3("seed1", "deviceid2");

  // Two calculators with same seed produce same hash.
  ASSERT_EQ(calc1.Calculate("pref_path", &string_value_1),
            calc1_dup.Calculate("pref_path", &string_value_1));
  ASSERT_EQ(PrefHashCalculator::VALID,
            calc1_dup.Validate("pref_path", &string_value_1,
                               calc1.Calculate("pref_path", &string_value_1)));

  // Different seeds, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc2.Calculate("pref_path", &string_value_1));
  ASSERT_EQ(PrefHashCalculator::INVALID,
            calc2.Validate("pref_path", &string_value_1,
                           calc1.Calculate("pref_path", &string_value_1)));

  // Different device IDs, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc3.Calculate("pref_path", &string_value_1));

  // Different values, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc1.Calculate("pref_path", &string_value_2));

  // Different paths, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc1.Calculate("pref_path_2", &string_value_1));

  // Works for dictionaries.
  ASSERT_EQ(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_1));
  ASSERT_NE(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_2));

  // Empty dictionary children are pruned.
  ASSERT_EQ(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_1_equivalent));

  // NULL value is supported.
  ASSERT_FALSE(calc1.Calculate("pref_path", NULL).empty());
}

// Tests the output against a known value to catch unexpected algorithm changes.
// The test hashes below must NEVER be updated, the serialization algorithm used
// must always be able to generate data that will produce these exact hashes.
TEST(PrefHashCalculatorTest, CatchHashChanges) {
  static const char kSeed[] = "0123456789ABCDEF0123456789ABCDEF";
  static const char kDeviceId[] = "test_device_id1";

  std::unique_ptr<base::Value> null_value = base::Value::CreateNullValue();
  std::unique_ptr<base::Value> bool_value(new base::FundamentalValue(false));
  std::unique_ptr<base::Value> int_value(
      new base::FundamentalValue(1234567890));
  std::unique_ptr<base::Value> double_value(
      new base::FundamentalValue(123.0987654321));
  std::unique_ptr<base::Value> string_value(
      new base::StringValue("testing with special chars:\n<>{}:^^@#$\\/"));

  // For legacy reasons, we have to support pruning of empty lists/dictionaries
  // and nested empty ists/dicts in the hash generation algorithm.
  std::unique_ptr<base::DictionaryValue> nested_empty_dict(
      new base::DictionaryValue);
  nested_empty_dict->Set("a", new base::DictionaryValue);
  nested_empty_dict->Set("b", new base::ListValue);
  std::unique_ptr<base::ListValue> nested_empty_list(new base::ListValue);
  nested_empty_list->Append(new base::DictionaryValue);
  nested_empty_list->Append(new base::ListValue);
  nested_empty_list->Append(nested_empty_dict->DeepCopy());

  // A dictionary with an empty dictionary, an empty list, and nested empty
  // dictionaries/lists in it.
  std::unique_ptr<base::DictionaryValue> dict_value(new base::DictionaryValue);
  dict_value->Set("a", new base::StringValue("foo"));
  dict_value->Set("d", new base::ListValue);
  dict_value->Set("b", new base::DictionaryValue);
  dict_value->Set("c", new base::StringValue("baz"));
  dict_value->Set("e", nested_empty_dict.release());
  dict_value->Set("f", nested_empty_list.release());

  std::unique_ptr<base::ListValue> list_value(new base::ListValue);
  list_value->AppendBoolean(true);
  list_value->AppendInteger(100);
  list_value->AppendDouble(1.0);

  ASSERT_EQ(base::Value::TYPE_NULL, null_value->GetType());
  ASSERT_EQ(base::Value::TYPE_BOOLEAN, bool_value->GetType());
  ASSERT_EQ(base::Value::TYPE_INTEGER, int_value->GetType());
  ASSERT_EQ(base::Value::TYPE_DOUBLE, double_value->GetType());
  ASSERT_EQ(base::Value::TYPE_STRING, string_value->GetType());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, dict_value->GetType());
  ASSERT_EQ(base::Value::TYPE_LIST, list_value->GetType());

  // Test every value type independently. Intentionally omits TYPE_BINARY which
  // isn't even allowed in JSONWriter's input.
  static const char kExpectedNullValue[] =
      "82A9F3BBC7F9FF84C76B033C854E79EEB162783FA7B3E99FF9372FA8E12C44F7";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", null_value.get(), kExpectedNullValue));

  static const char kExpectedBooleanValue[] =
      "A520D8F43EA307B0063736DC9358C330539D0A29417580514C8B9862632C4CCC";
  EXPECT_EQ(
      PrefHashCalculator::VALID,
      PrefHashCalculator(kSeed, kDeviceId)
          .Validate("pref.path", bool_value.get(), kExpectedBooleanValue));

  static const char kExpectedIntegerValue[] =
      "8D60DA1F10BF5AA29819D2D66D7CCEF9AABC5DA93C11A0D2BD21078D63D83682";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", int_value.get(), kExpectedIntegerValue));

  static const char kExpectedDoubleValue[] =
      "C9D94772516125BEEDAE68C109D44BC529E719EE020614E894CC7FB4098C545D";
  EXPECT_EQ(
      PrefHashCalculator::VALID,
      PrefHashCalculator(kSeed, kDeviceId)
          .Validate("pref.path", double_value.get(), kExpectedDoubleValue));

  static const char kExpectedStringValue[] =
      "05ACCBD3B05C45C36CD06190F63EC577112311929D8380E26E5F13182EB68318";
  EXPECT_EQ(
      PrefHashCalculator::VALID,
      PrefHashCalculator(kSeed, kDeviceId)
          .Validate("pref.path", string_value.get(), kExpectedStringValue));

  static const char kExpectedDictValue[] =
      "7A84DCC710D796C771F789A4DA82C952095AA956B6F1667EE42D0A19ECAA3C4A";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", dict_value.get(), kExpectedDictValue));

  static const char kExpectedListValue[] =
      "8D5A25972DF5AE20D041C780E7CA54E40F614AD53513A0724EE8D62D4F992740";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", list_value.get(), kExpectedListValue));

  // Also test every value type together in the same dictionary.
  base::DictionaryValue everything;
  everything.Set("null", null_value.release());
  everything.Set("bool", bool_value.release());
  everything.Set("int", int_value.release());
  everything.Set("double", double_value.release());
  everything.Set("string", string_value.release());
  everything.Set("list", list_value.release());
  everything.Set("dict", dict_value.release());
  static const char kExpectedEverythingValue[] =
      "B97D09BE7005693574DCBDD03D8D9E44FB51F4008B73FB56A49A9FA671A1999B";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId)
                .Validate("pref.path", &everything, kExpectedEverythingValue));
}

TEST(PrefHashCalculatorTest, TestCompatibilityWithLegacyPrefMetricsServiceId) {
  static const char kSeed[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
      0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
      0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00,
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
      0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  static const char kDeviceId[] =
      "D730D9CBD98C734A4FB097A1922275FE9F7E026A4EA1BE0E84";
  static const char kExpectedValue[] =
      "845EF34663FF8D32BE6707F40258FBA531C2BFC532E3B014AFB3476115C2A9DE";

  base::ListValue startup_urls;
  startup_urls.Set(0, new base::StringValue("http://www.chromium.org/"));

  EXPECT_EQ(
      PrefHashCalculator::VALID_SECURE_LEGACY,
      PrefHashCalculator(std::string(kSeed, arraysize(kSeed)), kDeviceId)
          .Validate("session.startup_urls", &startup_urls, kExpectedValue));
}

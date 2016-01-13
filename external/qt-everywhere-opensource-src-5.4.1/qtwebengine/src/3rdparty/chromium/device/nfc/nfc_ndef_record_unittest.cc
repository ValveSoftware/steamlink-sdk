// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "device/nfc/nfc_ndef_record.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;
using base::StringValue;

namespace device {

namespace {

const char kTestAction[] = "test-action";
const char kTestEncoding[] = "test-encoding";
const char kTestLanguageCode[] = "test-language-code";
const char kTestMimeType[] = "test-mime-type";
const uint32 kTestTargetSize = 0;
const char kTestText[] = "test-text";
const char kTestURI[] = "test://uri";

}  // namespace

TEST(NfcNdefRecordTest, PopulateTextRecord) {
  DictionaryValue data;
  scoped_ptr<NfcNdefRecord> record(new NfcNdefRecord());

  // Missing text field. Should fail.
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeText, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Text field with incorrect type. Should fail.
  data.SetInteger(NfcNdefRecord::kFieldText, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeText, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Text field with correct type but missing encoding and language.
  // Should fail.
  data.SetString(NfcNdefRecord::kFieldText, kTestText);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeText, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Populating a successfully populated record should fail.
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeText, &data));

  // Incorrect type for language code.
  data.SetInteger(NfcNdefRecord::kFieldLanguageCode, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeText, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Correct type for language code, invalid encoding.
  data.SetString(NfcNdefRecord::kFieldLanguageCode, kTestLanguageCode);
  data.SetInteger(NfcNdefRecord::kFieldEncoding, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeText, &data));
  EXPECT_FALSE(record->IsPopulated());

  // All entries valid. Should succeed.
  data.SetString(NfcNdefRecord::kFieldEncoding, kTestEncoding);
  EXPECT_TRUE(record->Populate(NfcNdefRecord::kTypeText, &data));
  EXPECT_TRUE(record->IsPopulated());
  EXPECT_EQ(NfcNdefRecord::kTypeText, record->type());

  // Compare record contents.
  std::string string_value;
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldText, &string_value));
  EXPECT_EQ(kTestText, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldLanguageCode, &string_value));
  EXPECT_EQ(kTestLanguageCode, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldEncoding, &string_value));
  EXPECT_EQ(kTestEncoding, string_value);
}

TEST(NfcNdefRecordTest, PopulateUriRecord) {
  DictionaryValue data;
  scoped_ptr<NfcNdefRecord> record(new NfcNdefRecord());

  // Missing URI field. Should fail.
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeURI, &data));
  EXPECT_FALSE(record->IsPopulated());

  // URI field with incorrect type. Should fail.
  data.SetInteger(NfcNdefRecord::kFieldURI, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeURI, &data));
  EXPECT_FALSE(record->IsPopulated());

  // URI field with correct type but invalid format.
  data.SetString(NfcNdefRecord::kFieldURI, "test.uri");
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeURI, &data));
  EXPECT_FALSE(record->IsPopulated());

  data.SetString(NfcNdefRecord::kFieldURI, kTestURI);
  EXPECT_TRUE(record->Populate(NfcNdefRecord::kTypeURI, &data));
  EXPECT_TRUE(record->IsPopulated());
  EXPECT_EQ(NfcNdefRecord::kTypeURI, record->type());

  // Populating a successfully populated record should fail.
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeURI, &data));

  // Recycle the record.
  record.reset(new NfcNdefRecord());
  EXPECT_FALSE(record->IsPopulated());

  // Incorrect optional fields. Should fail.
  data.SetInteger(NfcNdefRecord::kFieldMimeType, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeURI, &data));
  EXPECT_FALSE(record->IsPopulated());

  data.SetString(NfcNdefRecord::kFieldMimeType, kTestMimeType);
  data.SetInteger(NfcNdefRecord::kFieldTargetSize, kTestTargetSize);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeURI, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Optional fields are correct. Should succeed.
  data.SetDouble(NfcNdefRecord::kFieldTargetSize,
                 static_cast<double>(kTestTargetSize));
  EXPECT_TRUE(record->Populate(NfcNdefRecord::kTypeURI, &data));
  EXPECT_TRUE(record->IsPopulated());
  EXPECT_EQ(NfcNdefRecord::kTypeURI, record->type());

  // Compare record contents.
  std::string string_value;
  double double_value;
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldURI, &string_value));
  EXPECT_EQ(kTestURI, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldMimeType, &string_value));
  EXPECT_EQ(kTestMimeType, string_value);
  EXPECT_TRUE(record->data().GetDouble(
      NfcNdefRecord::kFieldTargetSize, &double_value));
  EXPECT_EQ(kTestTargetSize, static_cast<uint32>(double_value));
}

TEST(NfcNdefRecordTest, PopulateSmartPoster) {
  DictionaryValue data;
  scoped_ptr<NfcNdefRecord> record(new NfcNdefRecord());

  // Missing URI field. Should fail.
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  // URI field with incorrect entry. Should fail.
  data.SetInteger(NfcNdefRecord::kFieldURI, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  // URI field with correct entry. Should succeed.
  data.SetString(NfcNdefRecord::kFieldURI, kTestURI);
  EXPECT_TRUE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_TRUE(record->IsPopulated());
  EXPECT_EQ(NfcNdefRecord::kTypeSmartPoster, record->type());

  // Populating a successfully populated record should fail.
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));

  // Recycle the record.
  record.reset(new NfcNdefRecord());
  EXPECT_FALSE(record->IsPopulated());

  // Incorrect optional fields. Should fail.
  data.SetInteger(NfcNdefRecord::kFieldAction, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  data.SetString(NfcNdefRecord::kFieldAction, kTestAction);
  data.SetInteger(NfcNdefRecord::kFieldMimeType, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  data.SetString(NfcNdefRecord::kFieldMimeType, kTestMimeType);
  data.SetInteger(NfcNdefRecord::kFieldTargetSize, kTestTargetSize);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  data.SetDouble(NfcNdefRecord::kFieldTargetSize, kTestTargetSize);
  data.SetInteger(NfcNdefRecord::kFieldTitles, 0);
  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Empty titles value should fail.
  ListValue* list_value = new ListValue();
  data.Set(NfcNdefRecord::kFieldTitles, list_value);

  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Title value with missing required field.
  DictionaryValue* title_value = new DictionaryValue();
  list_value->Append(title_value);

  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Title value with invalid "text" field.
  title_value->SetInteger(NfcNdefRecord::kFieldText, 0);

  EXPECT_FALSE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_FALSE(record->IsPopulated());

  // Title value with valid entries.
  title_value->SetString(NfcNdefRecord::kFieldText, kTestText);
  title_value->SetString(NfcNdefRecord::kFieldLanguageCode, kTestLanguageCode);
  title_value->SetString(NfcNdefRecord::kFieldEncoding, kTestEncoding);

  EXPECT_TRUE(record->Populate(NfcNdefRecord::kTypeSmartPoster, &data));
  EXPECT_TRUE(record->IsPopulated());

  // Verify the record contents.
  std::string string_value;
  double double_value;
  const ListValue* const_list_value;
  const DictionaryValue* const_dictionary_value;
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldURI, &string_value));
  EXPECT_EQ(kTestURI, string_value);
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldMimeType, &string_value));
  EXPECT_EQ(kTestMimeType, string_value);
  EXPECT_TRUE(record->data().GetDouble(
      NfcNdefRecord::kFieldTargetSize, &double_value));
  EXPECT_EQ(kTestTargetSize, static_cast<uint32>(double_value));
  EXPECT_TRUE(record->data().GetString(
      NfcNdefRecord::kFieldAction, &string_value));
  EXPECT_EQ(kTestAction, string_value);
  EXPECT_TRUE(record->data().GetList(
      NfcNdefRecord::kFieldTitles, &const_list_value));
  EXPECT_EQ(static_cast<size_t>(1), const_list_value->GetSize());
  EXPECT_TRUE(const_list_value->GetDictionary(0, &const_dictionary_value));
  EXPECT_TRUE(const_dictionary_value->GetString(
      NfcNdefRecord::kFieldText, &string_value));
  EXPECT_EQ(kTestText, string_value);
  EXPECT_TRUE(const_dictionary_value->GetString(
      NfcNdefRecord::kFieldLanguageCode, &string_value));
  EXPECT_EQ(kTestLanguageCode, string_value);
  EXPECT_TRUE(const_dictionary_value->GetString(
      NfcNdefRecord::kFieldEncoding, &string_value));
  EXPECT_EQ(kTestEncoding, string_value);
}

}  // namespace device

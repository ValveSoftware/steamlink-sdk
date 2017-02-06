// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/public/interfaces/test_autofill_types.mojom.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

const std::vector<const char*> kOptions = {"Option1", "Option2", "Option3",
                                           "Option4"};
namespace {

void CreateTestFieldDataPredictions(const std::string& signature,
                                    FormFieldDataPredictions* field_predict) {
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, 4, &field_predict->field);
  field_predict->signature = signature;
  field_predict->heuristic_type = "TestSignature";
  field_predict->server_type = "TestServerType";
  field_predict->overall_type = "TestOverallType";
  field_predict->parseable_name = "TestParseableName";
}

void CreateTestPasswordFormFillData(PasswordFormFillData* fill_data) {
  fill_data->name = base::ASCIIToUTF16("TestName");
  fill_data->origin = GURL("https://foo.com/");
  fill_data->action = GURL("https://foo.com/login");
  test::CreateTestSelectField("TestUsernameFieldLabel", "TestUsernameFieldName",
                              "TestUsernameFieldValue", kOptions, kOptions, 4,
                              &fill_data->username_field);
  test::CreateTestSelectField("TestPasswordFieldLabel", "TestPasswordFieldName",
                              "TestPasswordFieldValue", kOptions, kOptions, 4,
                              &fill_data->password_field);
  fill_data->preferred_realm = "https://foo.com/";

  base::string16 name;
  PasswordAndRealm pr;
  name = base::ASCIIToUTF16("Tom");
  pr.password = base::ASCIIToUTF16("Tom_Password");
  pr.realm = "https://foo.com/";
  fill_data->additional_logins[name] = pr;
  name = base::ASCIIToUTF16("Jerry");
  pr.password = base::ASCIIToUTF16("Jerry_Password");
  pr.realm = "https://bar.com/";
  fill_data->additional_logins[name] = pr;

  UsernamesCollectionKey key;
  key.username = base::ASCIIToUTF16("Tom");
  key.password = base::ASCIIToUTF16("Tom_Password");
  key.realm = "https://foo.com/";
  std::vector<base::string16>& possible_names =
      fill_data->other_possible_usernames[key];
  possible_names.push_back(base::ASCIIToUTF16("Tom_1"));
  possible_names.push_back(base::ASCIIToUTF16("Tom_2"));
  key.username = base::ASCIIToUTF16("Jerry");
  key.password = base::ASCIIToUTF16("Jerry_Password");
  key.realm = "https://bar.com/";
  possible_names = fill_data->other_possible_usernames[key];
  possible_names.push_back(base::ASCIIToUTF16("Jerry_1"));
  possible_names.push_back(base::ASCIIToUTF16("Jerry_2"));

  fill_data->wait_for_username = true;
  fill_data->is_possible_change_password_form = false;
}

void CheckEqualPasswordFormFillData(const PasswordFormFillData& expected,
                                    const PasswordFormFillData& actual) {
  EXPECT_EQ(expected.name, actual.name);
  EXPECT_EQ(expected.origin, actual.origin);
  EXPECT_EQ(expected.action, actual.action);
  EXPECT_TRUE(expected.username_field.SameFieldAs(actual.username_field));
  EXPECT_TRUE(expected.password_field.SameFieldAs(actual.password_field));
  EXPECT_EQ(expected.preferred_realm, actual.preferred_realm);

  {
    EXPECT_EQ(expected.additional_logins.size(),
              actual.additional_logins.size());
    auto iter1 = expected.additional_logins.begin();
    auto end1 = expected.additional_logins.end();
    auto iter2 = actual.additional_logins.begin();
    auto end2 = actual.additional_logins.end();
    for (; iter1 != end1 && iter2 != end2; ++iter1, ++iter2) {
      EXPECT_EQ(iter1->first, iter2->first);
      EXPECT_EQ(iter1->second.password, iter2->second.password);
      EXPECT_EQ(iter1->second.realm, iter2->second.realm);
    }
    ASSERT_EQ(iter1, end1);
    ASSERT_EQ(iter2, end2);
  }

  {
    EXPECT_EQ(expected.other_possible_usernames.size(),
              actual.other_possible_usernames.size());
    auto iter1 = expected.other_possible_usernames.begin();
    auto end1 = expected.other_possible_usernames.end();
    auto iter2 = actual.other_possible_usernames.begin();
    auto end2 = actual.other_possible_usernames.end();
    for (; iter1 != end1 && iter2 != end2; ++iter1, ++iter2) {
      EXPECT_EQ(iter1->first.username, iter2->first.username);
      EXPECT_EQ(iter1->first.password, iter2->first.password);
      EXPECT_EQ(iter1->first.realm, iter2->first.realm);
      EXPECT_EQ(iter1->second, iter2->second);
    }
    ASSERT_EQ(iter1, end1);
    ASSERT_EQ(iter2, end2);
  }

  EXPECT_EQ(expected.wait_for_username, actual.wait_for_username);
  EXPECT_EQ(expected.is_possible_change_password_form,
            actual.is_possible_change_password_form);
}

}  // namespace

class AutofillTypeTraitsTestImpl : public testing::Test,
                                   public mojom::TypeTraitsTest {
 public:
  AutofillTypeTraitsTestImpl() {}

  mojom::TypeTraitsTestPtr GetTypeTraitsTestProxy() {
    return bindings_.CreateInterfacePtrAndBind(this);
  }

  // mojom::TypeTraitsTest:
  void PassFormData(const FormData& s,
                    const PassFormDataCallback& callback) override {
    callback.Run(s);
  }

  void PassFormFieldData(const FormFieldData& s,
                         const PassFormFieldDataCallback& callback) override {
    callback.Run(s);
  }

  void PassFormDataPredictions(
      const FormDataPredictions& s,
      const PassFormDataPredictionsCallback& callback) override {
    callback.Run(s);
  }

  void PassFormFieldDataPredictions(
      const FormFieldDataPredictions& s,
      const PassFormFieldDataPredictionsCallback& callback) override {
    callback.Run(s);
  }

  void PassPasswordFormFillData(
      const PasswordFormFillData& s,
      const PassPasswordFormFillDataCallback& callback) override {
    callback.Run(s);
  }

 private:
  base::MessageLoop loop_;

  mojo::BindingSet<TypeTraitsTest> bindings_;
};

void ExpectFormFieldData(const FormFieldData& expected,
                         const base::Closure& closure,
                         const FormFieldData& passed) {
  EXPECT_TRUE(expected.SameFieldAs(passed));
  EXPECT_EQ(expected.option_values, passed.option_values);
  EXPECT_EQ(expected.option_contents, passed.option_contents);
  closure.Run();
}

void ExpectFormData(const FormData& expected,
                    const base::Closure& closure,
                    const FormData& passed) {
  EXPECT_TRUE(expected.SameFormAs(passed));
  closure.Run();
}

void ExpectFormFieldDataPredictions(const FormFieldDataPredictions& expected,
                                    const base::Closure& closure,
                                    const FormFieldDataPredictions& passed) {
  EXPECT_EQ(expected, passed);
  closure.Run();
}

void ExpectFormDataPredictions(const FormDataPredictions& expected,
                               const base::Closure& closure,
                               const FormDataPredictions& passed) {
  EXPECT_EQ(expected, passed);
  closure.Run();
}

void ExpectPasswordFormFillData(const PasswordFormFillData& expected,
                                const base::Closure& closure,
                                const PasswordFormFillData& passed) {
  CheckEqualPasswordFormFillData(expected, passed);
  closure.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldData) {
  FormFieldData input;
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, 4, &input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormFieldData(
      input, base::Bind(&ExpectFormFieldData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormData) {
  FormData input;
  test::CreateTestAddressFormData(&input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormData(
      input, base::Bind(&ExpectFormData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldDataPredictions) {
  FormFieldDataPredictions input;
  CreateTestFieldDataPredictions("TestSignature", &input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormFieldDataPredictions(
      input,
      base::Bind(&ExpectFormFieldDataPredictions, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormDataPredictions) {
  FormDataPredictions input;
  test::CreateTestAddressFormData(&input.data);
  input.signature = "TestSignature";

  FormFieldDataPredictions field_predict;
  CreateTestFieldDataPredictions("Tom", &field_predict);
  input.fields.push_back(field_predict);
  CreateTestFieldDataPredictions("Jerry", &field_predict);
  input.fields.push_back(field_predict);
  CreateTestFieldDataPredictions("NoOne", &field_predict);
  input.fields.push_back(field_predict);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassFormDataPredictions(
      input,
      base::Bind(&ExpectFormDataPredictions, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassPasswordFormFillData) {
  PasswordFormFillData input;
  CreateTestPasswordFormFillData(&input);

  base::RunLoop loop;
  mojom::TypeTraitsTestPtr proxy = GetTypeTraitsTestProxy();
  proxy->PassPasswordFormFillData(input, base::Bind(&ExpectPasswordFormFillData,
                                                    input, loop.QuitClosure()));
  loop.Run();
}

}  // namespace autofill

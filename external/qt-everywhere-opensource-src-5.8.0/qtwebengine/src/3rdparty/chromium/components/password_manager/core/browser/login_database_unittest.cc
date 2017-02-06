// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using ::testing::Eq;

namespace password_manager {
namespace {
PasswordStoreChangeList AddChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::ADD, form));
}

PasswordStoreChangeList UpdateChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::UPDATE, form));
}

void GenerateExamplePasswordForm(PasswordForm* form) {
  form->origin = GURL("http://accounts.google.com/LoginAuth");
  form->action = GURL("http://accounts.google.com/Login");
  form->username_element = ASCIIToUTF16("Email");
  form->username_value = ASCIIToUTF16("test@gmail.com");
  form->password_element = ASCIIToUTF16("Passwd");
  form->password_value = ASCIIToUTF16("test");
  form->submit_element = ASCIIToUTF16("signIn");
  form->signon_realm = "http://www.google.com/";
  form->ssl_valid = false;
  form->preferred = false;
  form->scheme = PasswordForm::SCHEME_HTML;
  form->times_used = 1;
  form->form_data.name = ASCIIToUTF16("form_name");
  form->date_synced = base::Time::Now();
  form->display_name = ASCIIToUTF16("Mr. Smith");
  form->icon_url = GURL("https://accounts.google.com/Icon");
  form->federation_origin = url::Origin(GURL("https://accounts.google.com/"));
  form->skip_zero_click = true;
}

// Helper functions to read the value of the first column of an executed
// statement if we know its type. You must implement a specialization for
// every column type you use.
template<class T> struct must_be_specialized {
  static const bool is_specialized = false;
};

template<class T> T GetFirstColumn(const sql::Statement& s) {
  static_assert(must_be_specialized<T>::is_specialized,
                "Implement a specialization.");
}

template<> int64_t GetFirstColumn(const sql::Statement& s) {
  return s.ColumnInt64(0);
}

template<> std::string GetFirstColumn(const sql::Statement& s) {
  return s.ColumnString(0);
}

bool AddZeroClickableLogin(LoginDatabase* db,
                           const std::string& unique_string) {
  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://example.com/");
  form.username_element = ASCIIToUTF16(unique_string);
  form.username_value = ASCIIToUTF16(unique_string);
  form.password_element = ASCIIToUTF16(unique_string);
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = form.origin.spec();
  form.display_name = ASCIIToUTF16(unique_string);
  form.icon_url = GURL("https://example.com/");
  form.federation_origin = url::Origin(GURL("https://example.com/"));
  form.date_created = base::Time::Now();

  form.skip_zero_click = false;

  return db->AddLogin(form) == AddChangeForForm(form);
}

}  // namespace

// Serialization routines for vectors implemented in login_database.cc.
base::Pickle SerializeVector(const std::vector<base::string16>& vec);
std::vector<base::string16> DeserializeVector(const base::Pickle& pickle);

class LoginDatabaseTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.path().AppendASCII("TestMetadataStoreMacDatabase");
    OSCryptMocker::SetUpWithSingleton();

    db_.reset(new LoginDatabase(file_));
    ASSERT_TRUE(db_->Init());
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  LoginDatabase& db() { return *db_; }

  void TestNonHTMLFormPSLMatching(const PasswordForm::Scheme& scheme) {
    ScopedVector<autofill::PasswordForm> result;

    base::Time now = base::Time::Now();

    // Simple non-html auth form.
    PasswordForm non_html_auth;
    non_html_auth.origin = GURL("http://example.com");
    non_html_auth.username_value = ASCIIToUTF16("test@gmail.com");
    non_html_auth.password_value = ASCIIToUTF16("test");
    non_html_auth.signon_realm = "http://example.com/Realm";
    non_html_auth.scheme = scheme;
    non_html_auth.date_created = now;

    // Simple password form.
    PasswordForm html_form(non_html_auth);
    html_form.action = GURL("http://example.com/login");
    html_form.username_element = ASCIIToUTF16("username");
    html_form.username_value = ASCIIToUTF16("test2@gmail.com");
    html_form.password_element = ASCIIToUTF16("password");
    html_form.submit_element = ASCIIToUTF16("");
    html_form.signon_realm = "http://example.com/";
    html_form.scheme = PasswordForm::SCHEME_HTML;
    html_form.date_created = now;

    // Add them and make sure they are there.
    EXPECT_EQ(AddChangeForForm(non_html_auth), db().AddLogin(non_html_auth));
    EXPECT_EQ(AddChangeForForm(html_form), db().AddLogin(html_form));
    EXPECT_TRUE(db().GetAutofillableLogins(&result));
    EXPECT_EQ(2U, result.size());
    result.clear();

    PasswordForm second_non_html_auth(non_html_auth);
    second_non_html_auth.origin = GURL("http://second.example.com");
    second_non_html_auth.signon_realm = "http://second.example.com/Realm";

    // This shouldn't match anything.
    EXPECT_TRUE(db().GetLogins(second_non_html_auth, &result));
    EXPECT_EQ(0U, result.size());

    // non-html auth still matches against itself.
    EXPECT_TRUE(db().GetLogins(non_html_auth, &result));
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(result[0]->signon_realm, "http://example.com/Realm");

    // Clear state.
    db().RemoveLoginsCreatedBetween(now, base::Time());
  }

  // Checks that a form of a given |scheme|, once stored, can be successfully
  // retrieved from the database.
  void TestRetrievingIPAddress(const PasswordForm::Scheme& scheme) {
    SCOPED_TRACE(testing::Message() << "scheme = " << scheme);
    ScopedVector<autofill::PasswordForm> result;

    base::Time now = base::Time::Now();
    std::string origin("http://56.7.8.90");

    PasswordForm ip_form;
    ip_form.origin = GURL(origin);
    ip_form.username_value = ASCIIToUTF16("test@gmail.com");
    ip_form.password_value = ASCIIToUTF16("test");
    ip_form.signon_realm = origin;
    ip_form.scheme = scheme;
    ip_form.date_created = now;

    EXPECT_EQ(AddChangeForForm(ip_form), db().AddLogin(ip_form));
    EXPECT_TRUE(db().GetLogins(ip_form, &result));
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(result[0]->signon_realm, origin);

    // Clear state.
    db().RemoveLoginsCreatedBetween(now, base::Time());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_;
  std::unique_ptr<LoginDatabase> db_;
};

TEST_F(LoginDatabaseTest, Logins) {
  ScopedVector<autofill::PasswordForm> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  GenerateExamplePasswordForm(&form);

  // Add it and make sure it is there and that all the fields were retrieved
  // correctly.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(form, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.origin = GURL("http://www.google.com/new/accounts/LoginAuth");
  form2.submit_element = ASCIIToUTF16("reallySignIn");

  // Match against an inexact copy
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Uh oh, the site changed origin & action URLs all at once!
  PasswordForm form3(form2);
  form3.action = GURL("http://www.google.com/new/accounts/Login");

  // signon_realm is the same, should match.
  EXPECT_TRUE(db().GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Imagine the site moves to a secure server for login.
  PasswordForm form4(form3);
  form4.signon_realm = "https://www.google.com/";
  form4.ssl_valid = true;

  // We have only an http record, so no match for this.
  EXPECT_TRUE(db().GetLogins(form4, &result));
  EXPECT_EQ(0U, result.size());

  // Let's imagine the user logs into the secure site.
  EXPECT_EQ(AddChangeForForm(form4), db().AddLogin(form4));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Now the match works
  EXPECT_TRUE(db().GetLogins(form4, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db().RemoveLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db().GetLogins(form, &result));
  EXPECT_EQ(0U, result.size());

  // The user's request for the HTTPS site is intercepted
  // by an attacker who presents an invalid SSL cert.
  PasswordForm form5(form4);
  form5.ssl_valid = 0;

  // It will match in this case.
  EXPECT_TRUE(db().GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // User changes their password.
  PasswordForm form6(form5);
  form6.password_value = ASCIIToUTF16("test6");
  form6.preferred = true;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  EXPECT_EQ(UpdateChangeForForm(form6), db().UpdateLogin(form6));
  // matches
  EXPECT_TRUE(db().GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();
  // Only one record.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  // Password element was updated.
  EXPECT_EQ(form6.password_value, result[0]->password_value);
  // Preferred login.
  EXPECT_TRUE(form6.preferred);
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db().RemoveLogin(form4));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatching) {
  ScopedVector<autofill::PasswordForm> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.origin = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);

  // Try to remove PSL matched form
  EXPECT_FALSE(db().RemoveLogin(*result[0]));
  result.clear();
  // Ensure that the original form is still there
  EXPECT_TRUE(db().GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();
}

TEST_F(LoginDatabaseTest, TestFederatedMatching) {
  ScopedVector<autofill::PasswordForm> result;

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_value = ASCIIToUTF16("test");
  form.signon_realm = "https://foo.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.origin = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "federation://mobile.foo.com/accounts.google.com";
  form2.username_value = ASCIIToUTF16("test1@gmail.com");
  form2.type = autofill::PasswordForm::TYPE_API;
  form2.federation_origin = url::Origin(GURL("https://accounts.google.com/"));

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());

  // Match against desktop.
  PasswordForm form_request;
  form_request.origin = GURL("https://foo.com/");
  form_request.signon_realm = "https://foo.com/";
  form_request.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, testing::ElementsAre(testing::Pointee(form)));

  // Match against the mobile site.
  form_request.origin = GURL("https://mobile.foo.com/");
  form_request.signon_realm = "https://mobile.foo.com/";
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  form.is_public_suffix_match = true;
  EXPECT_THAT(result, testing::UnorderedElementsAre(testing::Pointee(form),
                                                    testing::Pointee(form2)));
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDisabledForNonHTMLForms) {
  TestNonHTMLFormPSLMatching(PasswordForm::SCHEME_BASIC);
  TestNonHTMLFormPSLMatching(PasswordForm::SCHEME_DIGEST);
  TestNonHTMLFormPSLMatching(PasswordForm::SCHEME_OTHER);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_HTML) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_HTML);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_basic) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_BASIC);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_digest) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_DIGEST);
}

TEST_F(LoginDatabaseTest, TestIPAddressMatches_other) {
  TestRetrievingIPAddress(PasswordForm::SCHEME_OTHER);
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingShouldMatchingApply) {
  ScopedVector<autofill::PasswordForm> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://accounts.google.com/");
  form.action = GURL("https://accounts.google.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://accounts.google.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // We go to a different site on the same domain where feature is not needed.
  PasswordForm form2(form);
  form2.origin = GURL("https://some.other.google.com/");
  form2.action = GURL("https://some.other.google.com/login");
  form2.signon_realm = "https://some.other.google.com/";

  // Match against the other site. Should not match since feature should not be
  // enabled for this domain.
  ASSERT_FALSE(ShouldPSLDomainMatchingApply(
      GetRegistryControlledDomain(GURL(form2.signon_realm))));

  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, TestFederatedMatchingWithoutPSLMatching) {
  ScopedVector<autofill::PasswordForm> result;

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://accounts.google.com/");
  form.action = GURL("https://accounts.google.com/login");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_value = ASCIIToUTF16("test");
  form.signon_realm = "https://accounts.google.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // We go to a different site on the same domain where PSL is disabled.
  PasswordForm form2(form);
  form2.origin = GURL("https://some.other.google.com/");
  form2.action = GURL("https://some.other.google.com/login");
  form2.signon_realm = "federation://some.other.google.com/accounts.google.com";
  form2.username_value = ASCIIToUTF16("test1@gmail.com");
  form2.type = autofill::PasswordForm::TYPE_API;
  form2.federation_origin = url::Origin(GURL("https://accounts.google.com/"));

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_EQ(AddChangeForForm(form2), db().AddLogin(form2));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());

  // Match against the first one.
  PasswordForm form_request;
  form_request.origin = form.origin;
  form_request.signon_realm = form.signon_realm;
  form_request.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  EXPECT_THAT(result, testing::ElementsAre(testing::Pointee(form)));

  // Match against the second one.
  ASSERT_FALSE(ShouldPSLDomainMatchingApply(
      GetRegistryControlledDomain(GURL(form2.signon_realm))));
  form_request.origin = form2.origin;
  form_request.signon_realm = form2.signon_realm;
  EXPECT_TRUE(db().GetLogins(form_request, &result));
  form.is_public_suffix_match = true;
  EXPECT_THAT(result, testing::ElementsAre(testing::Pointee(form2)));
}

// This test fails if the implementation of GetLogins uses GetCachedStatement
// instead of GetUniqueStatement, since REGEXP is in use. See
// http://crbug.com/248608.
TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingDifferentSites) {
  ScopedVector<autofill::PasswordForm> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("https://foo.com/");
  form.action = GURL("https://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://foo.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // We go to the mobile site.
  PasswordForm form2(form);
  form2.origin = GURL("https://mobile.foo.com/");
  form2.action = GURL("https://mobile.foo.com/login");
  form2.signon_realm = "https://mobile.foo.com/";

  // Match against the mobile site.
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://foo.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
  result.clear();

  // Add baz.com desktop site.
  form.origin = GURL("https://baz.com/login/");
  form.action = GURL("https://baz.com/login/");
  form.username_element = ASCIIToUTF16("email");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "https://baz.com/";
  form.ssl_valid = true;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // We go to the mobile site of baz.com.
  PasswordForm form3(form);
  form3.origin = GURL("https://m.baz.com/login/");
  form3.action = GURL("https://m.baz.com/login/");
  form3.signon_realm = "https://m.baz.com/";

  // Match against the mobile site of baz.com.
  EXPECT_TRUE(db().GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ("https://baz.com/", result[0]->signon_realm);
  EXPECT_TRUE(result[0]->is_public_suffix_match);
  result.clear();
}

PasswordForm GetFormWithNewSignonRealm(PasswordForm form,
                                       std::string signon_realm) {
  PasswordForm form2(form);
  form2.origin = GURL(signon_realm);
  form2.action = GURL(signon_realm);
  form2.signon_realm = signon_realm;
  return form2;
}

TEST_F(LoginDatabaseTest, TestPublicSuffixDomainMatchingRegexp) {
  ScopedVector<autofill::PasswordForm> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("http://foo.com/");
  form.action = GURL("http://foo.com/login");
  form.username_element = ASCIIToUTF16("username");
  form.username_value = ASCIIToUTF16("test@gmail.com");
  form.password_element = ASCIIToUTF16("password");
  form.password_value = ASCIIToUTF16("test");
  form.submit_element = ASCIIToUTF16("");
  form.signon_realm = "http://foo.com/";
  form.ssl_valid = false;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // Example password form that has - in the domain name.
  PasswordForm form_dash =
      GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");

  // Add it and make sure it is there.
  EXPECT_EQ(AddChangeForForm(form_dash), db().AddLogin(form_dash));
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db().GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // www.foo.com should match.
  PasswordForm form2 = GetFormWithNewSignonRealm(form, "http://www.foo.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a.b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a-b.foo.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // www.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://www.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a.b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a.b.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // a-b.foo-bar.com should match.
  form2 = GetFormWithNewSignonRealm(form, "http://a-b.foo-bar.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  result.clear();

  // foo.com with port 1337 should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo.com:1337/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // http://foo.com should not match since the scheme is wrong.
  form2 = GetFormWithNewSignonRealm(form, "https://foo.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // notfoo.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://notfoo.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://baz.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());

  // foo-baz.com should not match.
  form2 = GetFormWithNewSignonRealm(form, "http://foo-baz.com/");
  EXPECT_TRUE(db().GetLogins(form2, &result));
  EXPECT_EQ(0U, result.size());
}

static bool AddTimestampedLogin(LoginDatabase* db,
                                std::string url,
                                const std::string& unique_string,
                                const base::Time& time,
                                bool date_is_creation) {
  // Example password form.
  PasswordForm form;
  form.origin = GURL(url + std::string("/LoginAuth"));
  form.username_element = ASCIIToUTF16(unique_string);
  form.username_value = ASCIIToUTF16(unique_string);
  form.password_element = ASCIIToUTF16(unique_string);
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = url;
  form.display_name = ASCIIToUTF16(unique_string);
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin = url::Origin(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;

  if (date_is_creation)
    form.date_created = time;
  else
    form.date_synced = time;
  return db->AddLogin(form) == AddChangeForForm(form);
}

TEST_F(LoginDatabaseTest, ClearPrivateData_SavedPasswords) {
  ScopedVector<autofill::PasswordForm> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());

  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  // Create one with a 0 time.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://1.com", "foo1", base::Time(), true));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://2.com", "foo2", now - one_day, true));
  EXPECT_TRUE(AddTimestampedLogin(&db(), "http://3.com", "foo3", now, true));
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://4.com", "foo4", now + one_day, true));

  // Verify inserts worked.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(4U, result.size());
  result.clear();

  // Get everything from today's date and on.
  EXPECT_TRUE(db().GetLoginsCreatedBetween(now, base::Time(), &result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Delete everything from today's date and on.
  db().RemoveLoginsCreatedBetween(now, base::Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(2U, result.size());
  result.clear();

  // Delete with 0 date (should delete all).
  db().RemoveLoginsCreatedBetween(base::Time(), base::Time());

  // Verify nothing is left.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, RemoveLoginsSyncedBetween) {
  ScopedVector<autofill::PasswordForm> result;

  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  // Create one with a 0 time.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://1.com", "foo1", base::Time(), false));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://2.com", "foo2", now - one_day, false));
  EXPECT_TRUE(AddTimestampedLogin(&db(), "http://3.com", "foo3", now, false));
  EXPECT_TRUE(
      AddTimestampedLogin(&db(), "http://4.com", "foo4", now + one_day, false));

  // Verify inserts worked.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(4U, result.size());
  result.clear();

  // Get everything from today's date and on.
  EXPECT_TRUE(db().GetLoginsSyncedBetween(now, base::Time(), &result));
  ASSERT_EQ(2U, result.size());
  EXPECT_EQ("http://3.com", result[0]->signon_realm);
  EXPECT_EQ("http://4.com", result[1]->signon_realm);
  result.clear();

  // Delete everything from today's date and on.
  db().RemoveLoginsSyncedBetween(now, base::Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());
  EXPECT_EQ("http://1.com", result[0]->signon_realm);
  EXPECT_EQ("http://2.com", result[1]->signon_realm);
  result.clear();

  // Delete with 0 date (should delete all).
  db().RemoveLoginsSyncedBetween(base::Time(), now);

  // Verify nothing is left.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, GetAutoSignInLogins) {
  ScopedVector<autofill::PasswordForm> result;

  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1"));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2"));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3"));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4"));

  EXPECT_TRUE(db().GetAutoSignInLogins(&result));
  EXPECT_EQ(4U, result.size());
  for (const auto& form : result)
    EXPECT_FALSE(form->skip_zero_click);

  EXPECT_TRUE(db().DisableAutoSignInForAllLogins());
  EXPECT_TRUE(db().GetAutoSignInLogins(&result));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, DisableAutoSignInForAllLogins) {
  ScopedVector<autofill::PasswordForm> result;

  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo1"));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo2"));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo3"));
  EXPECT_TRUE(AddZeroClickableLogin(&db(), "foo4"));

  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  for (const auto& form : result)
    EXPECT_FALSE(form->skip_zero_click);

  EXPECT_TRUE(db().DisableAutoSignInForAllLogins());
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  for (const auto& form : result)
    EXPECT_TRUE(form->skip_zero_click);
}

TEST_F(LoginDatabaseTest, BlacklistedLogins) {
  ScopedVector<autofill::PasswordForm> result;

  // Verify the database is empty.
  EXPECT_TRUE(db().GetBlacklistLogins(&result));
  ASSERT_EQ(0U, result.size());

  // Save a form as blacklisted.
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.action = GURL("http://accounts.google.com/Login");
  form.username_element = ASCIIToUTF16("Email");
  form.password_element = ASCIIToUTF16("Passwd");
  form.submit_element = ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.google.com/";
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = true;
  form.scheme = PasswordForm::SCHEME_HTML;
  form.date_synced = base::Time::Now();
  form.display_name = ASCIIToUTF16("Mr. Smith");
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin = url::Origin(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

  // GetLogins should give the blacklisted result.
  EXPECT_TRUE(db().GetLogins(form, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();

  // So should GetAllBlacklistedLogins.
  EXPECT_TRUE(db().GetBlacklistLogins(&result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
  result.clear();
}

TEST_F(LoginDatabaseTest, VectorSerialization) {
  // Empty vector.
  std::vector<base::string16> vec;
  base::Pickle temp = SerializeVector(vec);
  std::vector<base::string16> output = DeserializeVector(temp);
  EXPECT_THAT(output, Eq(vec));

  // Normal data.
  vec.push_back(ASCIIToUTF16("first"));
  vec.push_back(ASCIIToUTF16("second"));
  vec.push_back(ASCIIToUTF16("third"));

  temp = SerializeVector(vec);
  output = DeserializeVector(temp);
  EXPECT_THAT(output, Eq(vec));
}

TEST_F(LoginDatabaseTest, UpdateIncompleteCredentials) {
  ScopedVector<autofill::PasswordForm> result;
  // Verify the database is empty.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(0U, result.size());

  // Save an incomplete form. Note that it only has a few fields set, ex. it's
  // missing 'action', 'username_element' and 'password_element'. Such forms
  // are sometimes inserted during import from other browsers (which may not
  // store this info).
  PasswordForm incomplete_form;
  incomplete_form.origin = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.ssl_valid = false;
  incomplete_form.preferred = true;
  incomplete_form.blacklisted_by_user = false;
  incomplete_form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(incomplete_form), db().AddLogin(incomplete_form));

  // A form on some website. It should trigger a match with the stored one.
  PasswordForm encountered_form;
  encountered_form.origin = GURL("http://accounts.google.com/LoginAuth");
  encountered_form.signon_realm = "http://accounts.google.com/";
  encountered_form.action = GURL("http://accounts.google.com/Login");
  encountered_form.username_element = ASCIIToUTF16("Email");
  encountered_form.password_element = ASCIIToUTF16("Passwd");
  encountered_form.submit_element = ASCIIToUTF16("signIn");

  // Get matches for encountered_form.
  EXPECT_TRUE(db().GetLogins(encountered_form, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(incomplete_form.origin, result[0]->origin);
  EXPECT_EQ(incomplete_form.signon_realm, result[0]->signon_realm);
  EXPECT_EQ(incomplete_form.username_value, result[0]->username_value);
  EXPECT_EQ(incomplete_form.password_value, result[0]->password_value);
  EXPECT_TRUE(result[0]->preferred);
  EXPECT_FALSE(result[0]->ssl_valid);

  // We should return empty 'action', 'username_element', 'password_element'
  // and 'submit_element' as we can't be sure if the credentials were entered
  // in this particular form on the page.
  EXPECT_EQ(GURL(), result[0]->action);
  EXPECT_TRUE(result[0]->username_element.empty());
  EXPECT_TRUE(result[0]->password_element.empty());
  EXPECT_TRUE(result[0]->submit_element.empty());
  result.clear();

  // Let's say this login form worked. Now update the stored credentials with
  // 'action', 'username_element', 'password_element' and 'submit_element' from
  // the encountered form.
  PasswordForm completed_form(incomplete_form);
  completed_form.action = encountered_form.action;
  completed_form.username_element = encountered_form.username_element;
  completed_form.password_element = encountered_form.password_element;
  completed_form.submit_element = encountered_form.submit_element;
  EXPECT_EQ(AddChangeForForm(completed_form), db().AddLogin(completed_form));
  EXPECT_TRUE(db().RemoveLogin(incomplete_form));

  // Get matches for encountered_form again.
  EXPECT_TRUE(db().GetLogins(encountered_form, &result));
  ASSERT_EQ(1U, result.size());

  // This time we should have all the info available.
  PasswordForm expected_form(completed_form);
  EXPECT_EQ(expected_form, *result[0]);
  result.clear();
}

TEST_F(LoginDatabaseTest, UpdateOverlappingCredentials) {
  // Save an incomplete form. Note that it only has a few fields set, ex. it's
  // missing 'action', 'username_element' and 'password_element'. Such forms
  // are sometimes inserted during import from other browsers (which may not
  // store this info).
  PasswordForm incomplete_form;
  incomplete_form.origin = GURL("http://accounts.google.com/LoginAuth");
  incomplete_form.signon_realm = "http://accounts.google.com/";
  incomplete_form.username_value = ASCIIToUTF16("my_username");
  incomplete_form.password_value = ASCIIToUTF16("my_password");
  incomplete_form.ssl_valid = false;
  incomplete_form.preferred = true;
  incomplete_form.blacklisted_by_user = false;
  incomplete_form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(incomplete_form), db().AddLogin(incomplete_form));

  // Save a complete version of the previous form. Both forms could exist if
  // the user created the complete version before importing the incomplete
  // version from a different browser.
  PasswordForm complete_form = incomplete_form;
  complete_form.action = GURL("http://accounts.google.com/Login");
  complete_form.username_element = ASCIIToUTF16("username_element");
  complete_form.password_element = ASCIIToUTF16("password_element");
  complete_form.submit_element = ASCIIToUTF16("submit");

  // An update fails because the primary key for |complete_form| is different.
  EXPECT_EQ(PasswordStoreChangeList(), db().UpdateLogin(complete_form));
  EXPECT_EQ(AddChangeForForm(complete_form), db().AddLogin(complete_form));

  // Make sure both passwords exist.
  ScopedVector<autofill::PasswordForm> result;
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());
  result.clear();

  // Simulate the user changing their password.
  complete_form.password_value = ASCIIToUTF16("new_password");
  complete_form.date_synced = base::Time::Now();
  EXPECT_EQ(UpdateChangeForForm(complete_form),
            db().UpdateLogin(complete_form));

  // Both still exist now.
  EXPECT_TRUE(db().GetAutofillableLogins(&result));
  ASSERT_EQ(2U, result.size());

  if (result[0]->username_element.empty())
    std::swap(result[0], result[1]);
  EXPECT_EQ(complete_form, *result[0]);
  EXPECT_EQ(incomplete_form, *result[1]);
}

TEST_F(LoginDatabaseTest, DoubleAdd) {
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  // Add almost the same form again.
  form.times_used++;
  PasswordStoreChangeList list;
  list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  EXPECT_EQ(list, db().AddLogin(form));
}

TEST_F(LoginDatabaseTest, AddWrongForm) {
  PasswordForm form;
  // |origin| shouldn't be empty.
  form.origin = GURL();
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));

  // |signon_realm| shouldn't be empty.
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm.clear();
  EXPECT_EQ(PasswordStoreChangeList(), db().AddLogin(form));
}

TEST_F(LoginDatabaseTest, UpdateLogin) {
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  form.action = GURL("http://accounts.google.com/login");
  form.password_value = ASCIIToUTF16("my_new_password");
  form.ssl_valid = true;
  form.preferred = false;
  form.other_possible_usernames.push_back(ASCIIToUTF16("my_new_username"));
  form.times_used = 20;
  form.submit_element = ASCIIToUTF16("submit_element");
  form.date_synced = base::Time::Now();
  form.date_created = base::Time::Now() - base::TimeDelta::FromDays(1);
  form.blacklisted_by_user = true;
  form.scheme = PasswordForm::SCHEME_BASIC;
  form.type = PasswordForm::TYPE_GENERATED;
  form.display_name = ASCIIToUTF16("Mr. Smith");
  form.icon_url = GURL("https://accounts.google.com/Icon");
  form.federation_origin = url::Origin(GURL("https://accounts.google.com/"));
  form.skip_zero_click = true;
  EXPECT_EQ(UpdateChangeForForm(form), db().UpdateLogin(form));

  ScopedVector<autofill::PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(form, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(form, *result[0]);
}

TEST_F(LoginDatabaseTest, RemoveWrongForm) {
  PasswordForm form;
  // |origin| shouldn't be empty.
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("my_password");
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = false;
  form.scheme = PasswordForm::SCHEME_HTML;
  // The form isn't in the database.
  EXPECT_FALSE(db().RemoveLogin(form));

  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));
  EXPECT_TRUE(db().RemoveLogin(form));
  EXPECT_FALSE(db().RemoveLogin(form));
}

TEST_F(LoginDatabaseTest, ReportMetricsTest) {
  PasswordForm password_form;
  password_form.origin = GURL("http://example.com");
  password_form.username_value = ASCIIToUTF16("test1@gmail.com");
  password_form.password_value = ASCIIToUTF16("test");
  password_form.signon_realm = "http://example.com/";
  password_form.times_used = 0;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("test2@gmail.com");
  password_form.times_used = 1;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://second.example.com");
  password_form.signon_realm = "http://second.example.com";
  password_form.times_used = 3;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("test3@gmail.com");
  password_form.type = PasswordForm::TYPE_GENERATED;
  password_form.times_used = 2;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("ftp://third.example.com/");
  password_form.signon_realm = "ftp://third.example.com/";
  password_form.times_used = 4;
  password_form.scheme = PasswordForm::SCHEME_OTHER;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://fourth.example.com/");
  password_form.signon_realm = "http://fourth.example.com/";
  password_form.type = PasswordForm::TYPE_MANUAL;
  password_form.username_value = ASCIIToUTF16("");
  password_form.times_used = 10;
  password_form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("https://fifth.example.com/");
  password_form.signon_realm = "https://fifth.example.com/";
  password_form.password_value = ASCIIToUTF16("");
  password_form.blacklisted_by_user = true;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("https://sixth.example.com/");
  password_form.signon_realm = "https://sixth.example.com/";
  password_form.username_value = ASCIIToUTF16("");
  password_form.password_value = ASCIIToUTF16("my_password");
  password_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_element = ASCIIToUTF16("some_other_input");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("my_username");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL();
  password_form.signon_realm = "android://hash@com.example.android/";
  password_form.username_value = ASCIIToUTF16("JohnDoe");
  password_form.password_value = ASCIIToUTF16("my_password");
  password_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.username_value = ASCIIToUTF16("JaneDoe");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccounts.UserCreated.WithoutCustomPassphrase", 9,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSite.UserCreated.WithoutCustomPassphrase",
      1,
      2);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.AccountsPerSite.UserCreated.WithoutCustomPassphrase", 2,
      3);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      0,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      1,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.UserCreated.WithoutCustomPassphrase",
      3,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccounts.AutoGenerated.WithoutCustomPassphrase",
      2,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Android", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Ftp", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Http", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Https", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.TotalAccountsHiRes.WithScheme.Other", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountsPerSite.AutoGenerated.WithoutCustomPassphrase",
      1, 2);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      2,
      1);
  histogram_tester.ExpectBucketCount(
      "PasswordManager.TimesPasswordUsed.AutoGenerated.WithoutCustomPassphrase",
      4,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.EmptyUsernames.CountInDatabase",
      3,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.EmptyUsernames.WithoutCorrespondingNonempty",
      1,
      1);
}

TEST_F(LoginDatabaseTest, PasswordReuseMetrics) {
  // -- Group of accounts that are reusing password #1.
  //
  //                                     Destination account
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+
  // |                 |   1   |   2   |   3   |   4   |   5   |   6   |   7   |
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+
  // | Scheme?         | HTTP  | HTTP  | HTTP  | HTTP  | HTTPS | HTTPS | HTTPS |
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+
  // |           |  1  |   -   | Same  |  PSL  | Diff. | Same  | Diff. | Diff. |
  // |           |  2  | Same  |   -   |  PSL  | Diff. | Same  | Diff. | Diff. |
  // | Relation  |  3  |  PSL  |  PSL  |   -   | Diff. | Diff. | Same  | Diff. |
  // | to host   |  4  | Diff. | Diff. | Diff. |   -   | Diff. | Diff. | Same  |
  // | of source |  5  | Same  | Same  | Diff. | Diff. |   -   |  PSL  | Diff. |
  // | account:  |  6  | Diff. | Diff. | Same  | Diff. |  PSL  |   -   | Diff. |
  // |           |  7  | Diff. | Diff. | Diff. | Same  | Diff. | Diff. |   -   |
  // +-----------------+-------+-------+-------+-------+-------+-------+-------+

  PasswordForm password_form;
  password_form.signon_realm = "http://example.com/";
  password_form.origin = GURL("http://example.com/");
  password_form.username_value = ASCIIToUTF16("username_1");
  password_form.password_value = ASCIIToUTF16("password_1");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.origin = GURL("http://example.com/");
  password_form.username_value = ASCIIToUTF16("username_2");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // Note: This PSL matches http://example.com, but not https://example.com.
  password_form.signon_realm = "http://www.example.com/";
  password_form.origin = GURL("http://www.example.com/");
  password_form.username_value = ASCIIToUTF16("username_3");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "http://not-example.com/";
  password_form.origin = GURL("http://not-example.com/");
  password_form.username_value = ASCIIToUTF16("username_4");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "https://example.com/";
  password_form.origin = GURL("https://example.com/");
  password_form.username_value = ASCIIToUTF16("username_5");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // Note: This PSL matches https://example.com, but not http://example.com.
  password_form.signon_realm = "https://www.example.com/";
  password_form.origin = GURL("https://www.example.com/");
  password_form.username_value = ASCIIToUTF16("username_6");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "https://not-example.com/";
  password_form.origin = GURL("https://not-example.com/");
  password_form.username_value = ASCIIToUTF16("username_7");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // -- Group of accounts that are reusing password #2.
  // Both HTTP, different host.
  password_form.signon_realm = "http://example.com/";
  password_form.origin = GURL("http://example.com/");
  password_form.username_value = ASCIIToUTF16("username_8");
  password_form.password_value = ASCIIToUTF16("password_2");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "http://not-example.com/";
  password_form.origin = GURL("http://not-example.com/");
  password_form.username_value = ASCIIToUTF16("username_9");
  password_form.password_value = ASCIIToUTF16("password_2");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // -- Group of accounts that are reusing password #3.
  // HTTP sites identified by different IP addresses, so they should not be
  // considered a public suffix match.
  password_form.signon_realm = "http://1.2.3.4/";
  password_form.origin = GURL("http://1.2.3.4/");
  password_form.username_value = ASCIIToUTF16("username_10");
  password_form.password_value = ASCIIToUTF16("password_3");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  password_form.signon_realm = "http://2.2.3.4/";
  password_form.origin = GURL("http://2.2.3.4/");
  password_form.username_value = ASCIIToUTF16("username_11");
  password_form.password_value = ASCIIToUTF16("password_3");
  EXPECT_EQ(AddChangeForForm(password_form), db().AddLogin(password_form));

  // -- Not HTML form based logins or blacklisted logins. Should be ignored.
  PasswordForm ignored_form;
  ignored_form.scheme = PasswordForm::SCHEME_HTML;
  ignored_form.signon_realm = "http://example.org/";
  ignored_form.origin = GURL("http://example.org/blacklist");
  ignored_form.blacklisted_by_user = true;
  ignored_form.username_value = ASCIIToUTF16("username_x");
  ignored_form.password_value = ASCIIToUTF16("password_y");
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  ignored_form.scheme = PasswordForm::SCHEME_BASIC;
  ignored_form.signon_realm = "http://example.org/HTTP Auth Realm";
  ignored_form.origin = GURL("http://example.org/");
  ignored_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  ignored_form.scheme = PasswordForm::SCHEME_HTML;
  ignored_form.signon_realm = "android://hash@com.example/";
  ignored_form.origin = GURL();
  ignored_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  ignored_form.scheme = PasswordForm::SCHEME_HTML;
  ignored_form.signon_realm = "federation://example.com/federation.com";
  ignored_form.origin = GURL("https://example.com/");
  ignored_form.blacklisted_by_user = false;
  EXPECT_EQ(AddChangeForForm(ignored_form), db().AddLogin(ignored_form));

  base::HistogramTester histogram_tester;
  db().ReportMetrics("", false);

  const std::string kPrefix("PasswordManager.AccountsReusingPassword.");
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(0, 6), base::Bucket(1, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpsRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(0, 4), base::Bucket(1, 4)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnPSLMatchingRealm"),
              testing::ElementsAre(base::Bucket(0, 5), base::Bucket(1, 2),
                                   base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpsRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(0, 4), base::Bucket(2, 4)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnHttpRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(1, 7), base::Bucket(3, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpRealm.OnAnyRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(1, 4), base::Bucket(3, 3),
                                   base::Bucket(5, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(1, 2), base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpsRealmWithSameHost"),
              testing::ElementsAre(base::Bucket(0, 3)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnPSLMatchingRealm"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 2)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnHttpsRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(1, 2), base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  kPrefix + "FromHttpsRealm.OnAnyRealmWithDifferentHost"),
              testing::ElementsAre(base::Bucket(3, 1), base::Bucket(4, 1),
                                   base::Bucket(5, 1)));
}

TEST_F(LoginDatabaseTest, ClearPasswordValues) {
  db().set_clear_password_values(true);

  // Add a PasswordForm, the password should be cleared.
  base::HistogramTester histogram_tester;
  PasswordForm form;
  form.origin = GURL("http://accounts.google.com/LoginAuth");
  form.signon_realm = "http://accounts.google.com/";
  form.username_value = ASCIIToUTF16("my_username");
  form.password_value = ASCIIToUTF16("12345");
  EXPECT_EQ(AddChangeForForm(form), db().AddLogin(form));

  ScopedVector<autofill::PasswordForm> result;
  EXPECT_TRUE(db().GetLogins(form, &result));
  ASSERT_EQ(1U, result.size());
  PasswordForm expected_form = form;
  expected_form.password_value.clear();
  EXPECT_EQ(expected_form, *result[0]);

  // Update the password, it should stay empty.
  form.password_value = ASCIIToUTF16("password");
  EXPECT_EQ(UpdateChangeForForm(form), db().UpdateLogin(form));
  EXPECT_TRUE(db().GetLogins(form, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(expected_form, *result[0]);

  // Encrypting/decrypting shouldn't happen. Thus there should be no keychain
  // access on Mac.
  histogram_tester.ExpectTotalCount("OSX.Keychain.Access", 0);
}

#if defined(OS_POSIX)
// Only the current user has permission to read the database.
//
// Only POSIX because GetPosixFilePermissions() only exists on POSIX.
// This tests that sql::Connection::set_restrict_to_user() was called,
// and that function is a noop on non-POSIX platforms in any case.
TEST_F(LoginDatabaseTest, FilePermissions) {
  int mode = base::FILE_PERMISSION_MASK;
  EXPECT_TRUE(base::GetPosixFilePermissions(file_, &mode));
  EXPECT_EQ((mode & base::FILE_PERMISSION_USER_MASK), mode);
}
#endif  // defined(OS_POSIX)

// Test the migration from GetParam() version to kCurrentVersionNumber.
class LoginDatabaseMigrationTest : public testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_dump_location_ = database_dump_location_.AppendASCII("components")
                                  .AppendASCII("test")
                                  .AppendASCII("data")
                                  .AppendASCII("password_manager");
    database_path_ = temp_dir_.path().AppendASCII("test.db");
    OSCryptMocker::SetUpWithSingleton();
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  // Creates the databse from |sql_file|.
  void CreateDatabase(base::StringPiece sql_file) {
    base::FilePath database_dump;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &database_dump));
    database_dump =
        database_dump.Append(database_dump_location_).AppendASCII(sql_file);
    ASSERT_TRUE(
        sql::test::CreateDatabaseFromSQL(database_path_, database_dump));
  }

  void DestroyDatabase() {
    if (!database_path_.empty())
      sql::Connection::Delete(database_path_);
  }

  // Returns an empty vector on failure. Otherwise returns values in the column
  // |column_name| of the logins table. The order of the
  // returned rows is well-defined.
  template <class T>
  std::vector<T> GetValues(const std::string& column_name) {
    sql::Connection db;
    std::vector<T> results;
    if (!db.Open(database_path_))
      return results;

    std::string statement = base::StringPrintf(
        "SELECT %s FROM logins ORDER BY username_value, %s DESC",
        column_name.c_str(), column_name.c_str());
    sql::Statement s(db.GetCachedStatement(SQL_FROM_HERE, statement.c_str()));
    if (!s.is_valid()) {
      db.Close();
      return results;
    }

    while (s.Step())
      results.push_back(GetFirstColumn<T>(s));

    s.Clear();
    db.Close();
    return results;
  }

  // Returns the database version for the test.
  int version() const { return GetParam(); }

  // Actual test body.
  void MigrationToVCurrent(base::StringPiece sql_file);

  base::FilePath database_path_;

 private:
  base::FilePath database_dump_location_;
  base::ScopedTempDir temp_dir_;
};

void LoginDatabaseMigrationTest::MigrationToVCurrent(
    base::StringPiece sql_file) {
  SCOPED_TRACE(testing::Message("Version file = ") << sql_file);
  CreateDatabase(sql_file);
  // Original date, in seconds since UTC epoch.
  std::vector<int64_t> date_created(GetValues<int64_t>("date_created"));
  ASSERT_EQ(2U, date_created.size());
  // Migration to version 8 performs changes dates to the new format.
  // So for versions less of equal to 8 create date should be in old
  // format before migration and in new format after.
  if (version() <= 8) {
    ASSERT_EQ(1402955745, date_created[0]);
    ASSERT_EQ(1402950000, date_created[1]);
  } else {
    ASSERT_EQ(13047429345000000, date_created[0]);
    ASSERT_EQ(13047423600000000, date_created[1]);
  }

  {
    // Assert that the database was successfully opened and updated
    // to current version.
    LoginDatabase db(database_path_);
    ASSERT_TRUE(db.Init());
    // Verifies that the final version can save all the appropriate fields.
    PasswordForm form;
    GenerateExamplePasswordForm(&form);
    // Add the same form twice to test the constraints in the database.
    EXPECT_EQ(AddChangeForForm(form), db.AddLogin(form));
    PasswordStoreChangeList list;
    list.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    list.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
    EXPECT_EQ(list, db.AddLogin(form));

    ScopedVector<autofill::PasswordForm> result;
    EXPECT_TRUE(db.GetLogins(form, &result));
    ASSERT_EQ(1U, result.size());
    EXPECT_EQ(form, *result[0]);
    EXPECT_TRUE(db.RemoveLogin(form));
  }
  // New date, in microseconds since platform independent epoch.
  std::vector<int64_t> new_date_created(GetValues<int64_t>("date_created"));
  if (version() <= 8) {
    ASSERT_EQ(2U, new_date_created.size());
    // Check that the two dates match up.
    for (size_t i = 0; i < date_created.size(); ++i) {
      EXPECT_EQ(base::Time::FromInternalValue(new_date_created[i]),
                base::Time::FromTimeT(date_created[i]));
    }
  } else if (version() == 10) {
    // The test data is setup on this version to cause a unique key collision.
    EXPECT_EQ(1U, new_date_created.size());
  } else {
    ASSERT_EQ(2U, new_date_created.size());
    ASSERT_EQ(13047429345000000, new_date_created[0]);
    ASSERT_EQ(13047423600000000, new_date_created[1]);
  }

  if (version() >= 7 && version() <= 13) {
    // The "avatar_url" column first appeared in version 7. In version 14,
    // it was renamed to "icon_url". Migration from a version <= 13
    // to >= 14 should not break theses URLs.
    std::vector<std::string> urls(GetValues<std::string>("icon_url"));

    if (version() == 10) {
      // The testcase for version 10 tests duplicate entries, so we only expect
      // one URL.
      EXPECT_THAT(urls, testing::ElementsAre("https://www.google.com/icon"));
    } else {
      // Otherwise, we expect one empty and one valid URL.
      EXPECT_THAT(
          urls, testing::ElementsAre("", "https://www.google.com/icon"));
    }
  }

  {
    // On versions < 15 |kCompatibleVersionNumber| was set to 1, but
    // the migration should bring it to the correct value.
    sql::Connection db;
    sql::MetaTable meta_table;
    ASSERT_TRUE(db.Open(database_path_));
    ASSERT_TRUE(
        meta_table.Init(&db, kCurrentVersionNumber, kCompatibleVersionNumber));
    EXPECT_EQ(password_manager::kCompatibleVersionNumber,
              meta_table.GetCompatibleVersionNumber());
  }
  DestroyDatabase();
}

// Tests the migration of the login database from version() to
// kCurrentVersionNumber.
TEST_P(LoginDatabaseMigrationTest, MigrationToVCurrent) {
  MigrationToVCurrent(base::StringPrintf("login_db_v%d.sql", version()));
}

class LoginDatabaseMigrationTestV9 : public LoginDatabaseMigrationTest {
};

// Tests migration from the alternative version #9, see crbug.com/423716.
TEST_P(LoginDatabaseMigrationTestV9, V9WithoutUseAdditionalAuthField) {
  ASSERT_EQ(9, version());
  MigrationToVCurrent("login_db_v9_without_use_additional_auth_field.sql");
}

class LoginDatabaseMigrationTestBroken : public LoginDatabaseMigrationTest {};

// Test migrating certain databases with incorrect version.
// http://crbug.com/295851
TEST_P(LoginDatabaseMigrationTestBroken, Broken) {
  MigrationToVCurrent(base::StringPrintf("login_db_v%d_broken.sql", version()));
}

INSTANTIATE_TEST_CASE_P(MigrationToVCurrent,
                        LoginDatabaseMigrationTest,
                        testing::Range(1, kCurrentVersionNumber + 1));
INSTANTIATE_TEST_CASE_P(MigrationToVCurrent,
                        LoginDatabaseMigrationTestV9,
                        testing::Values(9));
INSTANTIATE_TEST_CASE_P(MigrationToVCurrent,
                        LoginDatabaseMigrationTestBroken,
                        testing::Range(1, 4));

}  // namespace password_manager

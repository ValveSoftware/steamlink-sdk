// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliated_match_helper.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

class MockAffiliationService : public testing::StrictMock<AffiliationService> {
 public:
  MockAffiliationService() : testing::StrictMock<AffiliationService>(nullptr) {
    testing::DefaultValue<AffiliatedFacets>::Set(AffiliatedFacets());
  }

  ~MockAffiliationService() override {}

  MOCK_METHOD2(OnGetAffiliationsCalled,
               AffiliatedFacets(const FacetURI&, StrategyOnCacheMiss));
  MOCK_METHOD2(Prefetch, void(const FacetURI&, const base::Time&));
  MOCK_METHOD2(CancelPrefetch, void(const FacetURI&, const base::Time&));
  MOCK_METHOD1(TrimCacheForFacet, void(const FacetURI&));

  void GetAffiliations(const FacetURI& facet_uri,
                       StrategyOnCacheMiss cache_miss_strategy,
                       const ResultCallback& result_callback) override {
    AffiliatedFacets affiliation =
        OnGetAffiliationsCalled(facet_uri, cache_miss_strategy);
    result_callback.Run(affiliation, !affiliation.empty());
  }

  void ExpectCallToGetAffiliationsAndSucceedWithResult(
      const FacetURI& expected_facet_uri,
      StrategyOnCacheMiss expected_cache_miss_strategy,
      const AffiliatedFacets& affiliations_to_return) {
    EXPECT_CALL(*this, OnGetAffiliationsCalled(expected_facet_uri,
                                               expected_cache_miss_strategy))
        .WillOnce(testing::Return(affiliations_to_return));
  }

  void ExpectCallToGetAffiliationsAndEmulateFailure(
      const FacetURI& expected_facet_uri,
      StrategyOnCacheMiss expected_cache_miss_strategy) {
    EXPECT_CALL(*this, OnGetAffiliationsCalled(expected_facet_uri,
                                               expected_cache_miss_strategy))
        .WillOnce(testing::Return(std::vector<FacetURI>()));
  }

  void ExpectCallToPrefetch(const char* expected_facet_uri_spec) {
    EXPECT_CALL(*this,
                Prefetch(FacetURI::FromCanonicalSpec(expected_facet_uri_spec),
                         base::Time::Max())).RetiresOnSaturation();
  }

  void ExpectCallToCancelPrefetch(const char* expected_facet_uri_spec) {
    EXPECT_CALL(*this, CancelPrefetch(
                           FacetURI::FromCanonicalSpec(expected_facet_uri_spec),
                           base::Time::Max())).RetiresOnSaturation();
  }

  void ExpectCallToTrimCacheForFacet(const char* expected_facet_uri_spec) {
    EXPECT_CALL(*this, TrimCacheForFacet(FacetURI::FromCanonicalSpec(
                           expected_facet_uri_spec))).RetiresOnSaturation();
  }

 private:
  DISALLOW_ASSIGN(MockAffiliationService);
};

const char kTestWebFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestWebFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestAndroidFacetURIAlpha3[] =
    "android://hash@com.example.alpha.android";
const char kTestWebRealmAlpha1[] = "https://one.alpha.example.com/";
const char kTestWebRealmAlpha2[] = "https://two.alpha.example.com/";
const char kTestAndroidRealmAlpha3[] =
    "android://hash@com.example.alpha.android/";

const char kTestWebFacetURIBeta1[] = "https://one.beta.example.com";
const char kTestAndroidFacetURIBeta2[] =
    "android://hash@com.example.beta.android";
const char kTestAndroidFacetURIBeta3[] =
    "android://hash@com.yetanother.beta.android";
const char kTestWebRealmBeta1[] = "https://one.beta.example.com/";
const char kTestAndroidRealmBeta2[] =
    "android://hash@com.example.beta.android/";
const char kTestAndroidRealmBeta3[] =
    "android://hash@com.yetanother.beta.android/";

const char kTestAndroidFacetURIGamma[] =
    "android://hash@com.example.gamma.android";
const char kTestAndroidRealmGamma[] =
    "android://hash@com.example.gamma.android";

const char kTestUsername[] = "JohnDoe";
const char kTestPassword[] = "secret";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1));
  affiliated_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha2));
  affiliated_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3));
  return affiliated_facets;
}

AffiliatedFacets GetTestEquivalenceClassBeta() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1));
  affiliated_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2));
  affiliated_facets.push_back(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta3));
  return affiliated_facets;
}

autofill::PasswordForm GetTestAndroidCredentials(const char* signon_realm) {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::SCHEME_HTML;
  form.signon_realm = signon_realm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  form.ssl_valid = true;
  return form;
}

autofill::PasswordForm GetTestBlacklistedAndroidCredentials(
    const char* signon_realm) {
  autofill::PasswordForm form = GetTestAndroidCredentials(signon_realm);
  form.blacklisted_by_user = true;
  return form;
}

autofill::PasswordForm GetTestObservedWebForm(const char* signon_realm,
                                              const char* origin) {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::SCHEME_HTML;
  form.signon_realm = signon_realm;
  if (origin)
    form.origin = GURL(origin);
  form.ssl_valid = true;
  return form;
}

}  // namespace

class AffiliatedMatchHelperTest : public testing::Test {
 public:
  AffiliatedMatchHelperTest()
      : waiting_task_runner_(new base::TestSimpleTaskRunner),
        expecting_result_callback_(false),
        mock_affiliation_service_(nullptr) {}
  ~AffiliatedMatchHelperTest() override {}

 protected:
  void RunDeferredInitialization() {
    base::TimeDelta expected_init_delay = base::TimeDelta::FromSeconds(
        AffiliatedMatchHelper::kInitializationDelayOnStartupInSeconds);
    ASSERT_TRUE(waiting_task_runner()->HasPendingTask());
    ASSERT_EQ(expected_init_delay,
              waiting_task_runner()->NextPendingTaskDelay());
    waiting_task_runner()->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void AddLogin(const autofill::PasswordForm& form) {
    password_store_->AddLogin(form);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateLoginWithPrimaryKey(
      const autofill::PasswordForm& new_form,
      const autofill::PasswordForm& old_primary_key) {
    password_store_->UpdateLoginWithPrimaryKey(new_form, old_primary_key);
    base::RunLoop().RunUntilIdle();
  }

  void RemoveLogin(const autofill::PasswordForm& form) {
    password_store_->RemoveLogin(form);
    base::RunLoop().RunUntilIdle();
  }

  void AddAndroidAndNonAndroidTestLogins() {
    AddLogin(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
    AddLogin(GetTestAndroidCredentials(kTestAndroidRealmBeta2));
    AddLogin(GetTestBlacklistedAndroidCredentials(kTestAndroidRealmBeta3));
    AddLogin(GetTestAndroidCredentials(kTestAndroidRealmGamma));

    AddLogin(GetTestAndroidCredentials(kTestWebRealmAlpha1));
    AddLogin(GetTestAndroidCredentials(kTestWebRealmAlpha2));
  }

  void RemoveAndroidAndNonAndroidTestLogins() {
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmBeta2));
    RemoveLogin(GetTestBlacklistedAndroidCredentials(kTestAndroidRealmBeta3));
    RemoveLogin(GetTestAndroidCredentials(kTestAndroidRealmGamma));

    RemoveLogin(GetTestAndroidCredentials(kTestWebRealmAlpha1));
    RemoveLogin(GetTestAndroidCredentials(kTestWebRealmAlpha2));
  }

  void ExpectPrefetchForAndroidTestLogins() {
    mock_affiliation_service()->ExpectCallToPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToPrefetch(kTestAndroidFacetURIGamma);
  }

  void ExpectCancelPrefetchForAndroidTestLogins() {
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIGamma);
  }

  void ExpectTrimCacheForAndroidTestLogins() {
    mock_affiliation_service()->ExpectCallToTrimCacheForFacet(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacet(
        kTestAndroidFacetURIBeta2);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacet(
        kTestAndroidFacetURIBeta3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacet(
        kTestAndroidFacetURIGamma);
  }

  std::vector<std::string> GetAffiliatedAndroidRealms(
      const autofill::PasswordForm& observed_form) {
    expecting_result_callback_ = true;
    match_helper()->GetAffiliatedAndroidRealms(
        observed_form,
        base::Bind(&AffiliatedMatchHelperTest::OnAffiliatedRealmsCallback,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(expecting_result_callback_);
    return last_result_realms_;
  }

  std::vector<std::string> GetAffiliatedWebRealms(
      const autofill::PasswordForm& android_form) {
    expecting_result_callback_ = true;
    match_helper()->GetAffiliatedWebRealms(
        android_form,
        base::Bind(&AffiliatedMatchHelperTest::OnAffiliatedRealmsCallback,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(expecting_result_callback_);
    return last_result_realms_;
  }

  ScopedVector<autofill::PasswordForm> InjectAffiliatedWebRealms(
      ScopedVector<autofill::PasswordForm> forms) {
    expecting_result_callback_ = true;
    match_helper()->InjectAffiliatedWebRealms(
        std::move(forms),
        base::Bind(&AffiliatedMatchHelperTest::OnFormsCallback,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(expecting_result_callback_);
    return std::move(last_result_forms_);
  }

  void DestroyMatchHelper() { match_helper_.reset(); }

  base::TestSimpleTaskRunner* waiting_task_runner() {
    return waiting_task_runner_.get();
  }

  TestPasswordStore* password_store() { return password_store_.get(); }

  MockAffiliationService* mock_affiliation_service() {
    return mock_affiliation_service_;
  }

  AffiliatedMatchHelper* match_helper() { return match_helper_.get(); }

 private:
  void OnAffiliatedRealmsCallback(
      const std::vector<std::string>& affiliated_realms) {
    EXPECT_TRUE(expecting_result_callback_);
    expecting_result_callback_ = false;
    last_result_realms_ = affiliated_realms;
  }

  void OnFormsCallback(ScopedVector<autofill::PasswordForm> forms) {
    EXPECT_TRUE(expecting_result_callback_);
    expecting_result_callback_ = false;
    last_result_forms_.swap(forms);
  }

  // testing::Test:
  void SetUp() override {
    std::unique_ptr<MockAffiliationService> service(
        new MockAffiliationService());
    mock_affiliation_service_ = service.get();

    password_store_ = new TestPasswordStore;

    match_helper_.reset(
        new AffiliatedMatchHelper(password_store_.get(), std::move(service)));
    match_helper_->SetTaskRunnerUsedForWaitingForTesting(waiting_task_runner_);
  }

  void TearDown() override {
    match_helper_.reset();
    password_store_->ShutdownOnUIThread();
    password_store_ = nullptr;
  }

  scoped_refptr<base::TestSimpleTaskRunner> waiting_task_runner_;
  base::MessageLoop message_loop_;
  std::vector<std::string> last_result_realms_;
  ScopedVector<autofill::PasswordForm> last_result_forms_;
  bool expecting_result_callback_;

  scoped_refptr<TestPasswordStore> password_store_;
  std::unique_ptr<AffiliatedMatchHelper> match_helper_;

  // Owned by |match_helper_|.
  MockAffiliationService* mock_affiliation_service_;

  DISALLOW_COPY_AND_ASSIGN(AffiliatedMatchHelperTest);
};

// GetAffiliatedAndroidRealm* tests verify that GetAffiliatedAndroidRealms()
// returns the realms of affiliated Android applications, but only Android
// applications, and only if the observed form is a secure HTML login form.

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedAndroidRealmsYieldsResults) {
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndSucceedWithResult(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIBeta1),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassBeta());
  autofill::PasswordForm web_observed_form(
      GetTestObservedWebForm(kTestWebRealmBeta1, nullptr));
  EXPECT_THAT(GetAffiliatedAndroidRealms(web_observed_form),
              testing::UnorderedElementsAre(kTestAndroidRealmBeta2,
                                            kTestAndroidRealmBeta3));
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsOnlyAndroidApps) {
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndSucceedWithResult(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha());
  autofill::PasswordForm web_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  // This verifies that |kTestWebRealmAlpha2| is not returned.
  EXPECT_THAT(GetAffiliatedAndroidRealms(web_observed_form),
              testing::UnorderedElementsAre(kTestAndroidRealmAlpha3));
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForInsecureForms) {
  autofill::PasswordForm insecure_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  insecure_observed_form.ssl_valid = false;
  EXPECT_THAT(GetAffiliatedAndroidRealms(insecure_observed_form),
              testing::IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPBasicAuthForms) {
  autofill::PasswordForm http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = autofill::PasswordForm::SCHEME_BASIC;
  EXPECT_THAT(GetAffiliatedAndroidRealms(http_auth_observed_form),
              testing::IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForHTTPDigestAuthForms) {
  autofill::PasswordForm http_auth_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  http_auth_observed_form.scheme = autofill::PasswordForm::SCHEME_DIGEST;
  EXPECT_THAT(GetAffiliatedAndroidRealms(http_auth_observed_form),
              testing::IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsForAndroidKeyedForms) {
  autofill::PasswordForm android_observed_form(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  EXPECT_THAT(GetAffiliatedAndroidRealms(android_observed_form),
              testing::IsEmpty());
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedAndroidRealmsYieldsEmptyResultsWhenNoPrefetch) {
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndEmulateFailure(
      FacetURI::FromCanonicalSpec(kTestWebFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL);
  autofill::PasswordForm web_observed_form(
      GetTestObservedWebForm(kTestWebRealmAlpha1, nullptr));
  EXPECT_THAT(GetAffiliatedAndroidRealms(web_observed_form),
              testing::IsEmpty());
}

// GetAffiliatedWebRealms* tests verify that GetAffiliatedWebRealms() returns
// the realms of web sites affiliated with the given Android application, but
// only web sites, and only if an Android application is queried.

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedWebRealmsYieldsResults) {
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndSucceedWithResult(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK, GetTestEquivalenceClassAlpha());
  autofill::PasswordForm android_form(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  EXPECT_THAT(
      GetAffiliatedWebRealms(android_form),
      testing::UnorderedElementsAre(kTestWebRealmAlpha1, kTestWebRealmAlpha2));
}

TEST_F(AffiliatedMatchHelperTest, GetAffiliatedWebRealmsYieldsOnlyWebsites) {
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndSucceedWithResult(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK, GetTestEquivalenceClassBeta());
  autofill::PasswordForm android_form(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  // This verifies that |kTestAndroidRealmBeta3| is not returned.
  EXPECT_THAT(GetAffiliatedWebRealms(android_form),
              testing::UnorderedElementsAre(kTestWebRealmBeta1));
}

TEST_F(AffiliatedMatchHelperTest,
       GetAffiliatedWebRealmsYieldsEmptyResultsForWebKeyedForms) {
  autofill::PasswordForm web_form(
      GetTestObservedWebForm(kTestWebRealmBeta1, nullptr));
  EXPECT_THAT(GetAffiliatedWebRealms(web_form), testing::IsEmpty());
}

// Verifies that InjectAffiliatedWebRealms() injects the realms of web sites
// affiliated with the given Android application into password forms, if any.
TEST_F(AffiliatedMatchHelperTest, InjectAffiliatedWebRealms) {
  ScopedVector<autofill::PasswordForm> forms;

  forms.push_back(new autofill::PasswordForm(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3)));
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndSucceedWithResult(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha());

  forms.push_back(new autofill::PasswordForm(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2)));
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndSucceedWithResult(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassBeta());

  forms.push_back(new autofill::PasswordForm(
      GetTestAndroidCredentials(kTestAndroidRealmGamma)));
  mock_affiliation_service()->ExpectCallToGetAffiliationsAndEmulateFailure(
      FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma),
      StrategyOnCacheMiss::FAIL);

  forms.push_back(new autofill::PasswordForm(
      GetTestObservedWebForm(kTestWebRealmBeta1, nullptr)));

  size_t expected_form_count = forms.size();
  ScopedVector<autofill::PasswordForm> results(
      InjectAffiliatedWebRealms(std::move(forms)));
  ASSERT_EQ(expected_form_count, results.size());
  EXPECT_THAT(results[0]->affiliated_web_realm,
              testing::AnyOf(kTestWebRealmAlpha1, kTestWebRealmAlpha2));
  EXPECT_THAT(results[1]->affiliated_web_realm,
              testing::Eq(kTestWebRealmBeta1));
  EXPECT_THAT(results[2]->affiliated_web_realm, testing::IsEmpty());
  EXPECT_THAT(results[3]->affiliated_web_realm, testing::IsEmpty());
}

// Note: IsValidWebCredential() is tested as part of GetAffiliatedAndroidRealms
// tests above.
TEST_F(AffiliatedMatchHelperTest, IsValidAndroidCredential) {
  autofill::PasswordForm web_credential(
      GetTestObservedWebForm(kTestWebRealmBeta1, nullptr));
  EXPECT_FALSE(AffiliatedMatchHelper::IsValidAndroidCredential(web_credential));
  autofill::PasswordForm android_credential(
      GetTestAndroidCredentials(kTestAndroidRealmBeta2));
  EXPECT_TRUE(
      AffiliatedMatchHelper::IsValidAndroidCredential(android_credential));
}

// Verifies that affiliations for Android applications with pre-existing
// credentials on start-up are prefetched.
TEST_F(AffiliatedMatchHelperTest,
       PrefetchAffiliationsForPreexistingAndroidCredentialsOnStartup) {
  AddAndroidAndNonAndroidTestLogins();

  match_helper()->Initialize();
  base::RunLoop().RunUntilIdle();

  ExpectPrefetchForAndroidTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

// Stores credentials for Android applications between Initialize() and
// DoDeferredInitialization(). Verifies that corresponding affiliation
// information gets prefetched.
TEST_F(AffiliatedMatchHelperTest,
       PrefetchAffiliationsForAndroidCredentialsAddedInInitializationDelay) {
  match_helper()->Initialize();
  base::RunLoop().RunUntilIdle();

  AddAndroidAndNonAndroidTestLogins();

  ExpectPrefetchForAndroidTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

// Stores credentials for Android applications after DoDeferredInitialization().
// Verifies that corresponding affiliation information gets prefetched.
TEST_F(AffiliatedMatchHelperTest,
       PrefetchAffiliationsForAndroidCredentialsAddedAfterInitialization) {
  match_helper()->Initialize();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectPrefetchForAndroidTestLogins();
  AddAndroidAndNonAndroidTestLogins();
}

TEST_F(AffiliatedMatchHelperTest,
       CancelPrefetchingAffiliationsForRemovedAndroidCredentials) {
  AddAndroidAndNonAndroidTestLogins();
  match_helper()->Initialize();
  ExpectPrefetchForAndroidTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  ExpectCancelPrefetchForAndroidTestLogins();
  ExpectTrimCacheForAndroidTestLogins();
  RemoveAndroidAndNonAndroidTestLogins();
}

// Verify that whenever the primary key is updated for a credential (in which
// case both REMOVE and ADD change notifications are sent out), then Prefetch()
// is called in response to the addition before the call to TrimCacheForFacet()
// in response to the removal, so that cached data is not deleted and then
// immediately re-fetched.
TEST_F(AffiliatedMatchHelperTest, PrefetchBeforeTrimForPrimaryKeyUpdates) {
  AddAndroidAndNonAndroidTestLogins();
  match_helper()->Initialize();
  ExpectPrefetchForAndroidTestLogins();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  mock_affiliation_service()->ExpectCallToCancelPrefetch(
      kTestAndroidFacetURIAlpha3);

  {
    testing::InSequence in_sequence;
    mock_affiliation_service()->ExpectCallToPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacet(
        kTestAndroidFacetURIAlpha3);
  }

  autofill::PasswordForm old_form(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  autofill::PasswordForm new_form(old_form);
  new_form.username_value = base::ASCIIToUTF16("NewUserName");
  UpdateLoginWithPrimaryKey(new_form, old_form);
}

// Stores and removes four credentials for the same an Android application, and
// expects that Prefetch() and CancelPrefetch() will each be called four times.
TEST_F(AffiliatedMatchHelperTest,
       DuplicateCredentialsArePrefetchWithMultiplicity) {
  EXPECT_CALL(*mock_affiliation_service(),
              Prefetch(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha3),
                       base::Time::Max())).Times(4);

  autofill::PasswordForm android_form(
      GetTestAndroidCredentials(kTestAndroidRealmAlpha3));
  AddLogin(android_form);

  // Store two credentials before initialization.
  autofill::PasswordForm android_form2(android_form);
  android_form2.username_value = base::ASCIIToUTF16("JohnDoe2");
  AddLogin(android_form2);

  match_helper()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Store one credential between initialization and deferred initialization.
  autofill::PasswordForm android_form3(android_form);
  android_form3.username_value = base::ASCIIToUTF16("JohnDoe3");
  AddLogin(android_form3);

  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());

  // Store one credential after deferred initialization.
  autofill::PasswordForm android_form4(android_form);
  android_form4.username_value = base::ASCIIToUTF16("JohnDoe4");
  AddLogin(android_form4);

  for (size_t i = 0; i < 4; ++i) {
    mock_affiliation_service()->ExpectCallToCancelPrefetch(
        kTestAndroidFacetURIAlpha3);
    mock_affiliation_service()->ExpectCallToTrimCacheForFacet(
        kTestAndroidFacetURIAlpha3);
  }

  RemoveLogin(android_form);
  RemoveLogin(android_form2);
  RemoveLogin(android_form3);
  RemoveLogin(android_form4);
}

TEST_F(AffiliatedMatchHelperTest, DestroyBeforeDeferredInitialization) {
  match_helper()->Initialize();
  base::RunLoop().RunUntilIdle();
  DestroyMatchHelper();
  ASSERT_NO_FATAL_FAILURE(RunDeferredInitialization());
}

}  // namespace password_manager

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_backend.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/password_manager/core/browser/affiliation_database.h"
#include "components/password_manager/core/browser/affiliation_fetch_throttler.h"
#include "components/password_manager/core/browser/affiliation_fetch_throttler_delegate.h"
#include "components/password_manager/core/browser/facet_manager.h"
#include "components/password_manager/core/browser/fake_affiliation_api.h"
#include "components/password_manager/core/browser/mock_affiliation_consumer.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using StrategyOnCacheMiss = AffiliationBackend::StrategyOnCacheMiss;

// Mock fetch throttler that has some extra logic to accurately portray the real
// AffiliationFetchThrottler in how it ignores SignalNetworkRequestNeeded()
// requests when a request is already known to be needed or one is already in
// flight, and in how it goes back to the idle state afterwards.
class MockAffiliationFetchThrottler : public AffiliationFetchThrottler {
 public:
  MockAffiliationFetchThrottler(AffiliationFetchThrottlerDelegate* delegate)
      : AffiliationFetchThrottler(delegate, nullptr, nullptr),
        signaled_network_request_needed_(false) {
    EXPECT_CALL(*this, OnInformOfNetworkRequestComplete(testing::_)).Times(0);
  }

  ~MockAffiliationFetchThrottler() {
    EXPECT_FALSE(signaled_network_request_needed_);
  }

  // Expects that InformOfNetworkRequestComplete() will be called to indicate
  // either success or failure, depending on |expected_success|.
  void ExpectInformOfNetworkRequestComplete(bool expected_success) {
    EXPECT_CALL(*this, OnInformOfNetworkRequestComplete(expected_success));
  }

  // Informs the |delegate_| that it can send the needed network request.
  // Returns true if the |delegate_| reported that it actually ended up issuing
  // a request.
  bool LetNetworkRequestBeSent() {
    EXPECT_TRUE(has_signaled_network_request_needed());
    if (!delegate_->OnCanSendNetworkRequest()) {
      reset_signaled_network_request_needed();
      return false;
    }
    return true;
  }

  // Whether or not the throttler is 'signaled', meaning that the real throttler
  // would eventually call OnCanSendNetworkRequest() on the |delegate_|.
  bool has_signaled_network_request_needed() const {
    return signaled_network_request_needed_;
  }

  // Forces the mock throttler back to 'non-signaled' state. Normally, this does
  // not need to be manually called, as this is done by the mock automatically.
  void reset_signaled_network_request_needed() {
    signaled_network_request_needed_ = false;
  }

 private:
  MOCK_METHOD1(OnInformOfNetworkRequestComplete, void(bool));

  // AffiliationFetchThrottler:
  void SignalNetworkRequestNeeded() override {
    signaled_network_request_needed_ = true;
  }

  void InformOfNetworkRequestComplete(bool success) override {
    OnInformOfNetworkRequestComplete(success);
    reset_signaled_network_request_needed();
  }

  bool signaled_network_request_needed_;

  DISALLOW_COPY_AND_ASSIGN(MockAffiliationFetchThrottler);
};

const char kTestFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestFacetURIAlpha3[] = "https://three.alpha.example.com";
const char kTestFacetURIBeta1[] = "https://one.beta.example.com";
const char kTestFacetURIBeta2[] = "https://two.beta.example.com";
const char kTestFacetURIGamma1[] = "https://gamma.example.com";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha3));
  return affiliated_facets;
}

AffiliatedFacets GetTestEquivalenceClassBeta() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIBeta2));
  return affiliated_facets;
}

AffiliatedFacets GetTestEquivalenceClassGamma() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));
  return affiliated_facets;
}

base::TimeDelta GetCacheHardExpiryPeriod() {
  return base::TimeDelta::FromHours(FacetManager::kCacheHardExpiryInHours);
}

base::TimeDelta GetCacheSoftExpiryPeriod() {
  return base::TimeDelta::FromHours(FacetManager::kCacheSoftExpiryInHours);
}

base::TimeDelta GetShortTestPeriod() {
  return base::TimeDelta::FromHours(1);
}

// Returns a smallest time difference that this test cares about.
base::TimeDelta Epsilon() {
  return base::TimeDelta::FromMicroseconds(1);
}

}  // namespace

class AffiliationBackendTest : public testing::Test {
 public:
  AffiliationBackendTest()
      : backend_task_runner_(new base::TestMockTimeTaskRunner),
        consumer_task_runner_(new base::TestSimpleTaskRunner),
        mock_fetch_throttler_(nullptr) {}
  ~AffiliationBackendTest() override {}

 protected:
  void GetAffiliations(MockAffiliationConsumer* consumer,
                       const FacetURI& facet_uri,
                       StrategyOnCacheMiss cache_miss_strategy) {
    backend_->GetAffiliations(facet_uri, cache_miss_strategy,
                              consumer->GetResultCallback(),
                              consumer_task_runner());
  }

  void Prefetch(const FacetURI& facet_uri, base::Time keep_fresh_until) {
    backend_->Prefetch(facet_uri, keep_fresh_until);
  }

  void CancelPrefetch(const FacetURI& facet_uri, base::Time keep_fresh_until) {
    backend_->CancelPrefetch(facet_uri, keep_fresh_until);
  }

  void ExpectNeedForFetchAndLetItBeSent() {
    ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
    ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
    ASSERT_TRUE(mock_fetch_throttler()->LetNetworkRequestBeSent());
  }

  void ExpectAndCompleteFetch(
      const std::vector<FacetURI>& expected_requested_facet_uris) {
    ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
    EXPECT_THAT(
        fake_affiliation_api()->GetNextRequestedFacets(),
        testing::UnorderedElementsAreArray(expected_requested_facet_uris));
    mock_fetch_throttler()->ExpectInformOfNetworkRequestComplete(true);
    fake_affiliation_api()->ServeNextRequest();
    testing::Mock::VerifyAndClearExpectations(mock_fetch_throttler());
  }

  void ExpectAndCompleteFetch(const FacetURI& expected_requested_facet_uri) {
    std::vector<FacetURI> expected_facet_uris;
    expected_facet_uris.push_back(expected_requested_facet_uri);
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(expected_facet_uris));
  }

  void ExpectAndFailFetch(const FacetURI& expected_requested_facet_uri) {
    ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
    EXPECT_THAT(fake_affiliation_api()->GetNextRequestedFacets(),
                testing::UnorderedElementsAre(expected_requested_facet_uri));
    mock_fetch_throttler()->ExpectInformOfNetworkRequestComplete(false);
    fake_affiliation_api()->FailNextRequest();
    testing::Mock::VerifyAndClearExpectations(mock_fetch_throttler());
  }

  void ExpectNoFetchNeeded() {
    ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
    ASSERT_FALSE(mock_fetch_throttler()->has_signaled_network_request_needed());
  }

  void ExpectFailureWithoutFetch(MockAffiliationConsumer* consumer) {
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    consumer->ExpectFailure();
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(consumer);
  }

  void GetAffiliationsAndExpectFetchAndThenResult(
      const FacetURI& facet_uri,
      const AffiliatedFacets& expected_result) {
    GetAffiliations(mock_consumer(), facet_uri,
                    StrategyOnCacheMiss::FETCH_OVER_NETWORK);
    ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));
    mock_consumer()->ExpectSuccessWithResult(expected_result);
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  void GetAffiliationsAndExpectResultWithoutFetch(
      const FacetURI& facet_uri,
      StrategyOnCacheMiss cache_miss_strategy,
      const AffiliatedFacets& expected_result) {
    GetAffiliations(mock_consumer(), facet_uri, cache_miss_strategy);
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    mock_consumer()->ExpectSuccessWithResult(expected_result);
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  // TODO(engedy): Within this test fixture, the word "failure" refers to GTest
  // failures, simulated network failures (above), and also AffiliationService
  // failure callbacks. Make this less ambiguous.
  void GetAffiliationsAndExpectFailureWithoutFetch(const FacetURI& facet_uri) {
    GetAffiliations(mock_consumer(), facet_uri, StrategyOnCacheMiss::FAIL);
    ASSERT_NO_FATAL_FAILURE(ExpectFailureWithoutFetch(mock_consumer()));
  }

  void PrefetchAndExpectFetch(const FacetURI& facet_uri,
                              base::Time keep_fresh_until) {
    Prefetch(facet_uri, keep_fresh_until);
    ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));
  }

  // Verifies that both on-demand and cached-only GetAffiliations() requests for
  // each facet in |affiliated_facets| are served from cache with no fetches.
  void ExpectThatEquivalenceClassIsServedFromCache(
      const AffiliatedFacets& affiliated_facets) {
    for (const FacetURI& facet_uri : affiliated_facets) {
      SCOPED_TRACE(facet_uri);
      ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
          facet_uri, StrategyOnCacheMiss::FAIL, affiliated_facets));
      ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
          facet_uri, StrategyOnCacheMiss::FAIL, affiliated_facets));
    }
  }

  void DestroyBackend() { backend_.reset(); }

  void AdvanceTime(base::TimeDelta delta) {
    backend_task_runner_->FastForwardBy(delta);
  }

  // Directly opens the database and returns the number of equivalence classes
  // stored therein.
  size_t GetNumOfEquivalenceClassInDatabase() {
    AffiliationDatabase database;
    EXPECT_TRUE(database.Init(db_path()));
    std::vector<AffiliatedFacetsWithUpdateTime> all_affiliations;
    database.GetAllAffiliations(&all_affiliations);
    return all_affiliations.size();
  }

  size_t backend_facet_manager_count() {
    return backend()->facet_manager_count_for_testing();
  }

  bool IsCachedDataFreshForFacet(const FacetURI& facet_uri) {
    std::unique_ptr<base::Clock> clock(backend_task_runner_->GetMockClock());
    return FacetManager(facet_uri, backend(), clock.get()).IsCachedDataFresh();
  }

  bool IsCachedDataNearStaleForFacet(const FacetURI& facet_uri) {
    std::unique_ptr<base::Clock> clock(backend_task_runner_->GetMockClock());
    return FacetManager(facet_uri, backend(), clock.get())
        .IsCachedDataNearStale();
  }

  AffiliationBackend* backend() { return backend_.get(); }

  const base::FilePath& db_path() const { return db_path_; }

  base::TestMockTimeTaskRunner* backend_task_runner() {
    return backend_task_runner_.get();
  }

  base::TestSimpleTaskRunner* consumer_task_runner() {
    return consumer_task_runner_.get();
  }

  ScopedFakeAffiliationAPI* fake_affiliation_api() {
    return &fake_affiliation_api_;
  }

  MockAffiliationConsumer* mock_consumer() { return &mock_consumer_; }

  MockAffiliationFetchThrottler* mock_fetch_throttler() {
    return mock_fetch_throttler_;
  }

 private:
  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(CreateTemporaryFile(&db_path_));
    backend_.reset(new AffiliationBackend(
        NULL, backend_task_runner_, backend_task_runner_->GetMockClock(),
        backend_task_runner_->GetMockTickClock()));
    backend_->Initialize(db_path());
    mock_fetch_throttler_ = new MockAffiliationFetchThrottler(backend_.get());
    backend_->SetThrottlerForTesting(
        base::WrapUnique<AffiliationFetchThrottler>(mock_fetch_throttler_));

    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassAlpha());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassBeta());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassGamma());
  }

  scoped_refptr<base::TestMockTimeTaskRunner> backend_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> consumer_task_runner_;

  base::FilePath db_path_;
  ScopedFakeAffiliationAPI fake_affiliation_api_;
  MockAffiliationConsumer mock_consumer_;
  std::unique_ptr<AffiliationBackend> backend_;
  MockAffiliationFetchThrottler* mock_fetch_throttler_;  // Owned by |backend_|.

  DISALLOW_COPY_AND_ASSIGN(AffiliationBackendTest);
};

TEST_F(AffiliationBackendTest, OnDemandRequestSucceedsWithFetch) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
  EXPECT_EQ(0u, backend_facet_manager_count());

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      GetTestEquivalenceClassBeta()));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

// This test also verifies that the FacetManager is immediately discarded.
TEST_F(AffiliationBackendTest, CachedOnlyRequestFailsDueToCacheMiss) {
  GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, PrefetchTriggersInitialFetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
}

// This test also verifies that the FacetManager is immediately discarded.
TEST_F(AffiliationBackendTest, ExpiredPrefetchTriggersNoInitialFetch) {
  // Prefetch intervals are open from the right, thus intervals ending Now() are
  // already expired.
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
           backend_task_runner()->Now());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
}

// One additional GetAffiliations() and one Prefetch() request come in, both for
// unrelated facets, shortly after an initial GetAffiliations() request.
//
// Suppose that the network request triggered by the first GetAffiliations()
// request has already been initiated when the other requests arrive. As there
// should be no simultaneous requests, the additional facets should be queried
// together in a second fetch after the first fetch completes.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequests) {
  FacetURI facet_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  // Pretend the fetch is already away when the two other requests come in.
  MockAffiliationConsumer second_consumer;
  GetAffiliations(mock_consumer(), facet_alpha,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  GetAffiliations(&second_consumer, facet_beta,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  Prefetch(facet_gamma, base::Time::Max());

  std::vector<FacetURI> second_fetch_uris;
  second_fetch_uris.push_back(facet_beta);
  second_fetch_uris.push_back(facet_gamma);
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_alpha));
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(second_fetch_uris));

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassBeta());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // Now that the two GetAffiliation() requests have been completed, the first
  // two FacetManagers should be discarded. The third FacetManager corresponding
  // to the prefetched facet should be kept.
  EXPECT_GE(1u, backend_facet_manager_count());
}

// Now suppose that the first fetch is somewhat delayed (e.g., because network
// requests are throttled), so the other requests arrive before it is actually
// issued. In this case, all facet URIs should be queried together in one fetch.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequests2) {
  FacetURI facet_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  MockAffiliationConsumer second_consumer;
  GetAffiliations(mock_consumer(), facet_alpha,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  GetAffiliations(&second_consumer, facet_beta,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  Prefetch(facet_gamma, base::Time::Max());

  std::vector<FacetURI> fetched_uris;
  fetched_uris.push_back(facet_alpha);
  fetched_uris.push_back(facet_beta);
  fetched_uris.push_back(facet_gamma);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(fetched_uris));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassBeta());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // Now that the two GetAffiliation() requests have been completed, the first
  // two FacetManagers should be discarded. The third FacetManager corresponding
  // to the prefetched facet should be kept.
  EXPECT_GE(1u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, RetryIsMadeOnFailedFetch) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  GetAffiliations(mock_consumer(), facet_uri,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndFailFetch(facet_uri));
  EXPECT_EQ(1u, backend_facet_manager_count());

  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// The Prefetch() request expires before fetching corresponding affiliation
// information would be allowed. The fetch should be abandoned.
TEST_F(AffiliationBackendTest, FetchIsNoLongerNeededOnceAllowed) {
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
           backend_task_runner()->Now() + GetShortTestPeriod());
  ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  AdvanceTime(GetShortTestPeriod() + Epsilon());

  bool did_send_request = mock_fetch_throttler()->LetNetworkRequestBeSent();
  EXPECT_FALSE(did_send_request);
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesSubsequentRequestForSameFacet) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK, GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesSubsequentRequestForAffiliatedFacet) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesRequestsForPrefetchedFacets) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK, GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest,
       CacheServesRequestsForFacetsAffiliatedWithPrefetchedFacets) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

// A second GetAffiliations() request for the same facet and a third request
// for an affiliated facet comes in while the network fetch triggered by the
// first request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, all
// three requests should be served without further fetches (they have the data).
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentRequestsForAffiliatedFacets) {
  FacetURI facet_uri1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  MockAffiliationConsumer second_consumer;
  MockAffiliationConsumer third_consumer;
  GetAffiliations(mock_consumer(), facet_uri1,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  GetAffiliations(&second_consumer, facet_uri1,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  GetAffiliations(&third_consumer, facet_uri2,
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri1));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  third_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// A second Prefetch() request for the same facet and a third request for an
// affiliated facet comes in while the initial fetch triggered by the first
// request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, there
// should be no further initial fetches as the data needed is already there.
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentPrefetchesForAffiliatedFacets) {
  FacetURI facet_uri1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  Prefetch(facet_uri1, base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  Prefetch(facet_uri1, base::Time::Max());
  Prefetch(facet_uri2, base::Time::Max());

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri1));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest, SimpleCacheExpiryWithoutPrefetches) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());

  EXPECT_TRUE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(Epsilon());

  // After the data becomes stale, the cached-only request should fail, but the
  // subsequent on-demand request should fetch the data again and succeed.
  EXPECT_FALSE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// A Prefetch() request for a finite period. It should trigger an initial fetch
// and exactly one refetch, as the Prefetch() request expires exactly when the
// cached data obtained with the refetch expires.
TEST_F(AffiliationBackendTest,
       PrefetchTriggersOneInitialFetchAndOneRefetchBeforeExpiring) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      backend_task_runner()->Now() + GetCacheHardExpiryPeriod() +
          GetCacheSoftExpiryPeriod()));

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_FALSE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(Epsilon());

  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAndCompleteFetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());

  EXPECT_TRUE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(Epsilon());

  // The data should be allowed to expire and the FacetManager discarded.
  EXPECT_FALSE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)));

  // However, a subsequent on-demand request should be able to trigger a fetch.
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
}

// Affiliation data for prefetched facets should be automatically refetched once
// every 23 hours, and GetAffiliations() requests regarding affiliated facets
// should be continuously served from cache.
TEST_F(AffiliationBackendTest, PrefetchTriggersPeriodicRefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  for (int cycle = 0; cycle < 3; ++cycle) {
    SCOPED_TRACE(cycle);

    AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    EXPECT_TRUE(IsCachedDataFreshForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    EXPECT_FALSE(IsCachedDataNearStaleForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
        GetTestEquivalenceClassAlpha()));

    AdvanceTime(Epsilon());

    EXPECT_TRUE(IsCachedDataNearStaleForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
        GetTestEquivalenceClassAlpha()));
  }
}

TEST_F(AffiliationBackendTest,
       PrefetchTriggersNoInitialFetchIfDataIsAlreadyFresh) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  EXPECT_FALSE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest, CancelPrefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  // Cancel the prefetch the last microsecond before a refetch would take place.
  backend()->CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                            base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_TRUE(backend_task_runner()->HasPendingTask());

  AdvanceTime(GetCacheHardExpiryPeriod() - GetCacheSoftExpiryPeriod() +
              Epsilon());

  // The data should be allowed to expire.
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)));
}

TEST_F(AffiliationBackendTest, CancelDuplicatePrefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max());

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  // Cancel the prefetch the last microsecond before a refetch would take place.
  backend()->CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                            base::Time::Max());

  AdvanceTime(Epsilon());

  // However, there is a second Prefetch() request which should keep the data
  // fresh.
  EXPECT_EQ(1u, backend_facet_manager_count());
  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAndCompleteFetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(GetCacheHardExpiryPeriod() - GetCacheSoftExpiryPeriod());

  EXPECT_TRUE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

// Canceling a non-existing prefetch request for a non-prefetched facet.
TEST_F(AffiliationBackendTest, CancelingNonExistingPrefetchIsSilentlyIgnored) {
  CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                 backend_task_runner()->Now() + base::TimeDelta::FromHours(24));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
}

// Verify removal of equivalence classes that contain only facets for which
// there are no FacetManagers.
TEST_F(AffiliationBackendTest, TrimCacheDiscardsDataWithoutFacetManagers) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      backend_task_runner()->Now() + GetShortTestPeriod()));
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIBeta2), base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(2u, GetNumOfEquivalenceClassInDatabase());

  AdvanceTime(GetShortTestPeriod());

  // Worst-case scenario: the first prefetch has just expired, the other has
  // just been canceled when TrimCache() is called; plus immediately afterwards
  // the backend is destroyed.
  ASSERT_NO_FATAL_FAILURE(CancelPrefetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta2), base::Time::Max()));
  backend()->TrimCache();
  DestroyBackend();
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());
}

// Verify removal of equivalence classes that contain only facets for which
// there are only FacetManagers that do not need the data.
TEST_F(AffiliationBackendTest,
       TrimCacheDiscardsDataNoLongerNeededByFacetManagers) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  // Set up some stale data in the database.
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      facet_uri, GetTestEquivalenceClassAlpha()));
  AdvanceTime(GetCacheHardExpiryPeriod());
  EXPECT_FALSE(IsCachedDataFreshForFacet(facet_uri));

  // Now start prefetching the same facet, but keep the network fetch hanging.
  Prefetch(facet_uri, base::Time::Max());
  ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
  EXPECT_EQ(1u, GetNumOfEquivalenceClassInDatabase());

  // The already stale data should be removed regardless of the active prefetch.
  backend()->TrimCache();
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());

  mock_fetch_throttler()->reset_signaled_network_request_needed();
}

// Verify preservation of equivalence classes that contain >= 1 facet for which
// there is a FacetManager claiming that it needs to keep the data.
TEST_F(AffiliationBackendTest, TrimCacheRetainsDataThatNeededByFacetManagers) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      facet_uri, backend_task_runner()->Now() + GetCacheHardExpiryPeriod()));
  backend()->TrimCache();

  // Also verify that the last update time of the affiliation data is preserved,
  // i.e., that it expires when it would normally have expired.
  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());
  EXPECT_TRUE(IsCachedDataFreshForFacet(facet_uri));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
  AdvanceTime(Epsilon());
  EXPECT_FALSE(IsCachedDataFreshForFacet(facet_uri));
  ASSERT_NO_FATAL_FAILURE(
      GetAffiliationsAndExpectFailureWithoutFetch(facet_uri));
}

// Verify that TrimCacheForFacet() only removes the equivalence class for the
// given facet, and preserves others (even if they could be discarded).
TEST_F(AffiliationBackendTest,
       TrimCacheForFacetOnlyRemovesDataForTheGivenFacet) {
  FacetURI preserved_facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      preserved_facet_uri, GetTestEquivalenceClassBeta()));
  EXPECT_EQ(2u, GetNumOfEquivalenceClassInDatabase());

  backend()->TrimCacheForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  EXPECT_EQ(1u, GetNumOfEquivalenceClassInDatabase());

  // Also verify that the last update time of the affiliation data is preserved,
  // i.e., that it expires when it would normally have expired.
  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());
  EXPECT_TRUE(IsCachedDataFreshForFacet(preserved_facet_uri));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassBeta()));
  AdvanceTime(Epsilon());
  EXPECT_FALSE(IsCachedDataFreshForFacet(preserved_facet_uri));
  ASSERT_NO_FATAL_FAILURE(
      GetAffiliationsAndExpectFailureWithoutFetch(preserved_facet_uri));
}

TEST_F(AffiliationBackendTest, NothingExplodesWhenShutDownDuringFetch) {
  GetAffiliations(mock_consumer(),
                  FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
  mock_fetch_throttler()->reset_signaled_network_request_needed();
  DestroyBackend();
}

TEST_F(AffiliationBackendTest,
       FailureCallbacksAreCalledIfBackendIsDestroyedWithPendingRequest) {
  GetAffiliations(mock_consumer(),
                  FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                  StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  // Currently, a GetAffiliations() request can only be blocked due to fetch in
  // flight -- so emulate this condition when destroying the backend.
  ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
  mock_fetch_throttler()->reset_signaled_network_request_needed();
  DestroyBackend();
  mock_consumer()->ExpectFailure();
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationBackendTest, DeleteCache) {
  DestroyBackend();
  ASSERT_TRUE(base::PathExists(db_path()));
  AffiliationBackend::DeleteCache(db_path());
  ASSERT_FALSE(base::PathExists(db_path()));
}

}  // namespace password_manager

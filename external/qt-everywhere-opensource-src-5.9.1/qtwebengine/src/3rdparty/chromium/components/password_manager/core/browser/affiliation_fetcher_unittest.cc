// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_fetcher.h"

#include <utility>

#include "base/macros.h"
#include "base/test/null_task_runner.h"
#include "components/password_manager/core/browser/affiliation_api.pb.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

const char kExampleAndroidFacetURI[] = "android://hash@com.example";
const char kExampleWebFacet1URI[] = "https://www.example.com";
const char kExampleWebFacet2URI[] = "https://www.example.org";

class MockAffiliationFetcherDelegate
    : public testing::StrictMock<AffiliationFetcherDelegate> {
 public:
  MockAffiliationFetcherDelegate() {}
  ~MockAffiliationFetcherDelegate() {}

  MOCK_METHOD0(OnFetchSucceededProxy, void());
  MOCK_METHOD0(OnFetchFailed, void());
  MOCK_METHOD0(OnMalformedResponse, void());

  void OnFetchSucceeded(std::unique_ptr<Result> result) override {
    OnFetchSucceededProxy();
    result_ = std::move(result);
  }

  const Result& result() const { return *result_.get(); }

 private:
  std::unique_ptr<Result> result_;

  DISALLOW_COPY_AND_ASSIGN(MockAffiliationFetcherDelegate);
};

}  // namespace

class AffiliationFetcherTest : public testing::Test {
 public:
  AffiliationFetcherTest()
      : request_context_getter_(new net::TestURLRequestContextGetter(
            make_scoped_refptr(new base::NullTaskRunner))) {}

  ~AffiliationFetcherTest() override {}

 protected:
  void VerifyRequestPayload(const std::vector<FacetURI>& expected_facet_uris) {
    net::TestURLFetcher* url_fetcher =
        test_url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_NE(nullptr, url_fetcher);

    affiliation_pb::LookupAffiliationRequest request;
    ASSERT_TRUE(request.ParseFromString(url_fetcher->upload_data()));

    std::vector<FacetURI> actual_uris;
    for (int i = 0; i < request.facet_size(); ++i)
      actual_uris.push_back(FacetURI::FromCanonicalSpec(request.facet(i)));

    EXPECT_EQ("application/x-protobuf", url_fetcher->upload_content_type());
    EXPECT_THAT(actual_uris,
                testing::UnorderedElementsAreArray(expected_facet_uris));
  }

  void ServiceURLRequest(const std::string& response) {
    net::TestURLFetcher* url_fetcher =
        test_url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_NE(nullptr, url_fetcher);

    url_fetcher->set_response_code(200);
    url_fetcher->SetResponseString(response);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void SimulateServerError() {
    net::TestURLFetcher* url_fetcher =
        test_url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_NE(nullptr, url_fetcher);

    url_fetcher->set_response_code(500);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void SimulateNetworkError() {
    net::TestURLFetcher* url_fetcher =
        test_url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_NE(nullptr, url_fetcher);
    url_fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                  net::ERR_NETWORK_CHANGED));
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  net::TestURLRequestContextGetter* request_context_getter() {
    return request_context_getter_.get();
  }

 private:
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  net::TestURLFetcherFactory test_url_fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationFetcherTest);
};

TEST_F(AffiliationFetcherTest, BasicReqestAndResponse) {
  const char kNotExampleAndroidFacetURI[] =
      "android://hash1234@com.example.not";
  const char kNotExampleWebFacetURI[] = "https://not.example.com";

  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class1->add_facet()->set_id(kExampleAndroidFacetURI);
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id(kNotExampleWebFacetURI);
  eq_class2->add_facet()->set_id(kNotExampleAndroidFacetURI);

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), requested_uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnFetchSucceededProxy());
  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(requested_uris));
  ASSERT_NO_FATAL_FAILURE(ServiceURLRequest(test_response.SerializeAsString()));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(2u, mock_delegate.result().size());
  EXPECT_THAT(mock_delegate.result()[0],
              testing::UnorderedElementsAre(
                  FacetURI::FromCanonicalSpec(kExampleWebFacet1URI),
                  FacetURI::FromCanonicalSpec(kExampleWebFacet2URI),
                  FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)));
  EXPECT_THAT(mock_delegate.result()[1],
              testing::UnorderedElementsAre(
                  FacetURI::FromCanonicalSpec(kNotExampleWebFacetURI),
                  FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI)));
}

// The API contract of this class is to return an equivalence class for all
// requested facets; however, the server will not return anything for facets
// that are not affiliated with any other facet. Make sure an equivalence class
// of size one is created for each of the missing facets.
TEST_F(AffiliationFetcherTest, MissingEquivalenceClassesAreCreated) {
  affiliation_pb::LookupAffiliationResponse empty_test_response;

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), requested_uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnFetchSucceededProxy());
  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(requested_uris));
  ASSERT_NO_FATAL_FAILURE(
      ServiceURLRequest(empty_test_response.SerializeAsString()));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, mock_delegate.result().size());
  EXPECT_THAT(mock_delegate.result()[0],
              testing::UnorderedElementsAre(
                  FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)));
}

TEST_F(AffiliationFetcherTest, DuplicateEquivalenceClassesAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class1->add_facet()->set_id(kExampleAndroidFacetURI);
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class2->add_facet()->set_id(kExampleAndroidFacetURI);
  eq_class2->add_facet()->set_id(kExampleWebFacet1URI);

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), requested_uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnFetchSucceededProxy());
  ASSERT_NO_FATAL_FAILURE(ServiceURLRequest(test_response.SerializeAsString()));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, mock_delegate.result().size());
  EXPECT_THAT(mock_delegate.result()[0],
              testing::UnorderedElementsAre(
                  FacetURI::FromCanonicalSpec(kExampleWebFacet1URI),
                  FacetURI::FromCanonicalSpec(kExampleWebFacet2URI),
                  FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)));
}

TEST_F(AffiliationFetcherTest, EmptyEquivalenceClassesAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  // Empty class.
  test_response.add_affiliation();

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), requested_uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnFetchSucceededProxy());
  ASSERT_NO_FATAL_FAILURE(ServiceURLRequest(test_response.SerializeAsString()));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, mock_delegate.result().size());
  EXPECT_THAT(mock_delegate.result()[0],
              testing::UnorderedElementsAre(
                  FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)));
}

TEST_F(AffiliationFetcherTest, UnrecognizedFacetURIsAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  // Equivalence class having, alongside known facet URIs, a facet URI that
  // corresponds to new platform unknown to this version.
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class1->add_facet()->set_id(kExampleAndroidFacetURI);
  eq_class1->add_facet()->set_id("new-platform://app-id-on-new-platform");
  // Equivalence class consisting solely of an unknown facet URI.
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id("new-platform2://app2-id-on-new-platform2");

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), requested_uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnFetchSucceededProxy());
  ASSERT_NO_FATAL_FAILURE(ServiceURLRequest(test_response.SerializeAsString()));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, mock_delegate.result().size());
  EXPECT_THAT(mock_delegate.result()[0],
              testing::UnorderedElementsAre(
                  FacetURI::FromCanonicalSpec(kExampleWebFacet1URI),
                  FacetURI::FromCanonicalSpec(kExampleWebFacet2URI),
                  FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)));
}

TEST_F(AffiliationFetcherTest, FailureBecauseResponseIsNotAProtobuf) {
  const char kMalformedResponse[] = "This is not a protocol buffer!";

  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnMalformedResponse());
  ASSERT_NO_FATAL_FAILURE(ServiceURLRequest(kMalformedResponse));
}

// Partially overlapping equivalence classes violate the invariant that
// affiliations must form an equivalence relation. Such a response is malformed.
TEST_F(AffiliationFetcherTest,
       FailureBecausePartiallyOverlappingEquivalenceClasses) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class2->add_facet()->set_id(kExampleAndroidFacetURI);

  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnMalformedResponse());
  ASSERT_NO_FATAL_FAILURE(ServiceURLRequest(test_response.SerializeAsString()));
}

TEST_F(AffiliationFetcherTest, FailOnServerError) {
  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnFetchFailed());
  ASSERT_NO_FATAL_FAILURE(SimulateServerError());
}

TEST_F(AffiliationFetcherTest, FailOnNetworkError) {
  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  MockAffiliationFetcherDelegate mock_delegate;
  std::unique_ptr<AffiliationFetcher> fetcher(AffiliationFetcher::Create(
      request_context_getter(), uris, &mock_delegate));
  fetcher->StartRequest();

  EXPECT_CALL(mock_delegate, OnFetchFailed());
  ASSERT_NO_FATAL_FAILURE(SimulateNetworkError());
}

}  // namespace password_manager

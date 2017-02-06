// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_fetcher.h"

#include <stdint.h>

#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "components/precache/core/proto/unfinished_work.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace precache {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NotNull;
using ::testing::Property;

const char kConfigURL[] = "http://config-url.com";
const char kManifestURLPrefix[] = "http://manifest-url-prefix.com/";
const char kCustomConfigURL[] = "http://custom-config-url.com";
const char kCustomManifestURLPrefix[] =
    "http://custom-manifest-url-prefix.com/";
const char kManifestFetchFailureURL[] =
    "http://manifest-url-prefix.com/manifest-fetch-failure.com";
const char kBadManifestURL[] =
    "http://manifest-url-prefix.com/bad-manifest.com";
const char kGoodManifestURL[] =
    "http://manifest-url-prefix.com/good-manifest.com";
const char kCustomGoodManifestURL[] =
    "http://custom-manifest-url-prefix.com/good-manifest.com";
const char kResourceFetchFailureURL[] = "http://resource-fetch-failure.com";
const char kGoodResourceURL[] = "http://good-resource.com";
const char kGoodResourceURLA[] = "http://good-resource.com/a";
const char kGoodResourceURLB[] = "http://good-resource.com/b";
const char kGoodResourceURLC[] = "http://good-resource.com/c";
const char kGoodResourceURLD[] = "http://good-resource.com/d";
const char kForcedStartingURLManifestURL[] =
    "http://manifest-url-prefix.com/forced-starting-url.com";
const uint32_t kExperimentID = 123;

}  // namespace

class TestURLFetcherCallback {
 public:
  TestURLFetcherCallback() : total_response_bytes_(0) {}

  std::unique_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    std::unique_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));

    total_response_bytes_ += response_data.size();
    requested_urls_.insert(url);

    return fetcher;
  }

  const std::multiset<GURL>& requested_urls() const {
    return requested_urls_;
  }

  int total_response_bytes() const { return total_response_bytes_; }

 private:
  // Multiset with one entry for each URL requested.
  std::multiset<GURL> requested_urls_;
  int total_response_bytes_;
};

class TestPrecacheDelegate : public PrecacheFetcher::PrecacheDelegate {
 public:
  TestPrecacheDelegate()
      : on_done_was_called_(false) {}

  void OnDone() override { on_done_was_called_ = true; }

  bool was_on_done_called() const {
    return on_done_was_called_;
  }

 private:
  bool on_done_was_called_;
};

class MockURLFetcherFactory : public net::URLFetcherFactory {
 public:
  typedef net::URLFetcher* DoURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* delegate);

  std::unique_ptr<net::URLFetcher> CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* delegate) override {
    return base::WrapUnique(
        DoCreateURLFetcher(id, url, request_type, delegate));
  }

  // The method to mock out, instead of CreateURLFetcher. This is necessary
  // because gmock can't handle move-only types such as scoped_ptr.
  MOCK_METHOD4(DoCreateURLFetcher, DoURLFetcher);

  // A fake successful response. When the action runs, it saves off a pointer to
  // the FakeURLFetcher in its output parameter for later inspection.
  testing::Action<DoURLFetcher> RespondWith(const std::string& body,
                                            net::FakeURLFetcher** fetcher) {
    return RespondWith(body, [](net::FakeURLFetcher* fetcher) {
      fetcher->set_response_code(net::HTTP_OK);
    }, fetcher);
  }

  // A fake custom response. When the action runs, it runs the given modifier to
  // customize the FakeURLFetcher, and then saves off a pointer to the
  // FakeURLFetcher in its output parameter for later inspection. The modifier
  // should be a functor that takes a FakeURLFetcher* and returns void.
  template <typename F>
  testing::Action<DoURLFetcher> RespondWith(const std::string& body,
                                            F modifier,
                                            net::FakeURLFetcher** fetcher) {
    return testing::MakeAction(
        new FakeResponseAction<F>(body, modifier, fetcher));
  }

 private:
  template <typename F>
  class FakeResponseAction : public testing::ActionInterface<DoURLFetcher> {
   public:
    FakeResponseAction(const std::string& body,
                       F modifier,
                       net::FakeURLFetcher** fetcher)
        : body_(body), modifier_(modifier), fetcher_(fetcher) {}

    net::URLFetcher* Perform(
        const testing::tuple<int,
                             const GURL&,
                             net::URLFetcher::RequestType,
                             net::URLFetcherDelegate*>& args) {
      auto fetcher = new net::FakeURLFetcher(
          testing::get<1>(args), testing::get<3>(args), body_, net::HTTP_OK,
          net::URLRequestStatus::SUCCESS);
      modifier_(fetcher);
      if (fetcher_)
        *fetcher_ = fetcher;
      return fetcher;
    }

   private:
    std::string body_;
    F modifier_;
    net::FakeURLFetcher** fetcher_;
  };
};

class PrecacheFetcherFetcherTest : public testing::Test {
 public:
  PrecacheFetcherFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        scoped_url_fetcher_factory_(&factory_),
        callback_(base::Bind(&PrecacheFetcherFetcherTest::Callback,
                             base::Unretained(this))) {}

  MOCK_METHOD1(Callback, void(const PrecacheFetcher::Fetcher&));

 protected:
  base::MessageLoopForUI loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  MockURLFetcherFactory factory_;
  net::ScopedURLFetcherFactory scoped_url_fetcher_factory_;
  base::Callback<void(const PrecacheFetcher::Fetcher&)> callback_;
};

void CacheMiss(net::FakeURLFetcher* fetcher) {
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::ERR_CACHE_MISS));
}

void HasETag(net::FakeURLFetcher* fetcher) {
  std::string raw_headers("HTTP/1.1 200 OK\0ETag: foo\0\0", 27);
  fetcher->set_response_headers(
      make_scoped_refptr(new net::HttpResponseHeaders(raw_headers)));
}

TEST_F(PrecacheFetcherFetcherTest, Config) {
  GURL url(kConfigURL);

  net::FakeURLFetcher* fetcher = nullptr;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith("", &fetcher));
  EXPECT_CALL(*this,
              Callback(Property(&PrecacheFetcher::Fetcher::network_url_fetcher,
                                NotNull())));

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, false /* is_resource_request */,
      SIZE_MAX);

  loop_.RunUntilIdle();

  ASSERT_NE(nullptr, fetcher);
  EXPECT_EQ(kNoTracking, fetcher->GetLoadFlags());
}

TEST_F(PrecacheFetcherFetcherTest, ResourceNotInCache) {
  GURL url(kGoodResourceURL);

  net::FakeURLFetcher *fetcher1 = nullptr, *fetcher2 = nullptr;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith("", CacheMiss, &fetcher1))
      .WillOnce(factory_.RespondWith("", &fetcher2));
  EXPECT_CALL(*this,
              Callback(Property(&PrecacheFetcher::Fetcher::network_url_fetcher,
                                NotNull())))
      .Times(1);

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, true /* is_resource_request */,
      SIZE_MAX);

  loop_.RunUntilIdle();

  ASSERT_NE(nullptr, fetcher1);
  EXPECT_EQ(net::LOAD_ONLY_FROM_CACHE | kNoTracking, fetcher1->GetLoadFlags());
  ASSERT_NE(nullptr, fetcher2);
  EXPECT_EQ(net::LOAD_VALIDATE_CACHE | kNoTracking, fetcher2->GetLoadFlags());
}

TEST_F(PrecacheFetcherFetcherTest, ResourceHasValidators) {
  GURL url(kGoodResourceURL);

  net::FakeURLFetcher *fetcher1 = nullptr, *fetcher2 = nullptr;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith("", HasETag, &fetcher1))
      .WillOnce(factory_.RespondWith("", &fetcher2));
  EXPECT_CALL(*this,
              Callback(Property(&PrecacheFetcher::Fetcher::network_url_fetcher,
                                NotNull())));

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, true /* is_resource_request */,
      SIZE_MAX);

  loop_.RunUntilIdle();

  ASSERT_NE(nullptr, fetcher1);
  EXPECT_EQ(net::LOAD_ONLY_FROM_CACHE | kNoTracking, fetcher1->GetLoadFlags());
  ASSERT_NE(nullptr, fetcher2);
  EXPECT_EQ(net::LOAD_VALIDATE_CACHE | kNoTracking, fetcher2->GetLoadFlags());
}

TEST_F(PrecacheFetcherFetcherTest, ResourceHasNoValidators) {
  GURL url(kGoodResourceURL);

  net::FakeURLFetcher* fetcher;
  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      .WillOnce(factory_.RespondWith("", &fetcher));
  EXPECT_CALL(*this,
              Callback(Property(&PrecacheFetcher::Fetcher::network_url_fetcher,
                                nullptr)));  // It never reached the network.

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, true /* is_resource_request */,
      SIZE_MAX);

  loop_.RunUntilIdle();

  EXPECT_EQ(net::LOAD_ONLY_FROM_CACHE | kNoTracking, fetcher->GetLoadFlags());
}

TEST_F(PrecacheFetcherFetcherTest, ResourceTooBig) {
  GURL url(kGoodResourceURL);

  EXPECT_CALL(factory_, DoCreateURLFetcher(_, url, net::URLFetcher::GET, _))
      // Cache request will fail, so that a network request is made. Only
      // network requests are byte-capped.
      .WillOnce(factory_.RespondWith("", CacheMiss, nullptr))
      .WillOnce(factory_.RespondWith(std::string(100, '.'), nullptr));

  // The callback should be called even though the download was cancelled, so
  // that the next download can start. The network_url_fetcher within should be
  // null, to signify that either the network was never reached (which will be
  // flagged as an error due to the expectation above) or it was requested but
  // cancelled (which is the desired behavior).
  EXPECT_CALL(*this,
              Callback(Property(&PrecacheFetcher::Fetcher::network_url_fetcher,
                                nullptr)));

  PrecacheFetcher::Fetcher precache_fetcher(
      request_context_.get(), url, callback_, true /* is_resource_request */,
      99 /* max_bytes */);

  loop_.RunUntilIdle();
}

class PrecacheFetcherTest : public testing::Test {
 public:
  PrecacheFetcherTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        factory_(NULL,
                 base::Bind(&TestURLFetcherCallback::CreateURLFetcher,
                            base::Unretained(&url_callback_))),
        expected_total_response_bytes_(0),
        parallel_fetches_beyond_capacity_(false) {}

 protected:
  void SetDefaultFlags() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheConfigSettingsURL, kConfigURL);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrecacheManifestURLPrefix, kManifestURLPrefix);
  }

  // Posts a task to check if more parallel fetches of precache manifest and
  // resource URLs were attempted beyond the fetcher pool maximum defined
  // capacity. The task will be posted repeatedly until such condition is met.
  void CheckUntilParallelFetchesBeyondCapacity(
      const PrecacheFetcher* precache_fetcher) {
    if (!precache_fetcher->pool_.IsAvailable() &&
        !precache_fetcher->resource_urls_to_fetch_.empty() &&
        !precache_fetcher->manifest_urls_to_fetch_.empty()) {
      parallel_fetches_beyond_capacity_ = true;
      return;
    }

    // Check again after allowing the message loop to process some messages.
    loop_.PostTask(
        FROM_HERE,
        base::Bind(
            &PrecacheFetcherTest::CheckUntilParallelFetchesBeyondCapacity,
            base::Unretained(this), precache_fetcher));
  }

  base::MessageLoopForUI loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  TestURLFetcherCallback url_callback_;
  net::FakeURLFetcherFactory factory_;
  TestPrecacheDelegate precache_delegate_;
  int expected_total_response_bytes_;

  // True if more parallel fetches were attempted beyond the fetcher pool
  // maximum capacity.
  bool parallel_fetches_beyond_capacity_;
};

TEST_F(PrecacheFetcherTest, FullPrecache) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());
  unfinished_work->add_top_host()->set_hostname("manifest-fetch-failure.com");
  unfinished_work->add_top_host()->set_hostname("bad-manifest.com");
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");
  unfinished_work->add_top_host()->set_hostname("not-in-top-3.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(3);
  config.add_forced_site("forced-starting-url.com");
  // Duplicate starting URL, the manifest for this should only be fetched once.
  config.add_forced_site("good-manifest.com");

  PrecacheManifest good_manifest;
  good_manifest.add_resource()->set_url(kResourceFetchFailureURL);
  good_manifest.add_resource();  // Resource with no URL, should not be fetched.
  good_manifest.add_resource()->set_url(kGoodResourceURL);

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kManifestFetchFailureURL), "",
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(GURL(kBadManifestURL), "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kResourceFetchFailureURL),
                           "", net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kForcedStartingURLManifestURL),
                           PrecacheManifest().SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();

    // Destroy the PrecacheFetcher after it has finished, to record metrics.
  }

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kManifestFetchFailureURL));
  expected_requested_urls.insert(GURL(kBadManifestURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  expected_requested_urls.insert(GURL(kResourceFetchFailureURL));
  expected_requested_urls.insert(GURL(kGoodResourceURL));
  expected_requested_urls.insert(GURL(kForcedStartingURLManifestURL));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  histogram.ExpectUniqueSample("Precache.Fetch.PercentCompleted", 100, 1);
  histogram.ExpectUniqueSample("Precache.Fetch.ResponseBytes.Total",
                               url_callback_.total_response_bytes(), 1);
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

TEST_F(PrecacheFetcherTest, PrecacheResourceSelection) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());

  PrecacheConfigurationSettings config;

  PrecacheManifest good_manifest;
  PrecacheResourceSelection resource_selection;
  good_manifest.add_resource()->set_url(kGoodResourceURL);
  good_manifest.add_resource()->set_url(kGoodResourceURLA);
  good_manifest.add_resource()->set_url(kGoodResourceURLB);
  good_manifest.add_resource()->set_url(kGoodResourceURLC);
  good_manifest.add_resource()->set_url(kGoodResourceURLD);

  // Set bits for kGoodResourceURL, kGoodResourceURLB and kGoodResourceURLD.
  resource_selection.set_bitset(0b10101);
  (*good_manifest.mutable_experiments()
        ->mutable_resources_by_experiment_group())[kExperimentID] =
      resource_selection;

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURLB), "good URL B", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURLD), "good URL D", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();

    // Destroy the PrecacheFetcher after it has finished, to record metrics.
  }

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  expected_requested_urls.insert(GURL(kGoodResourceURL));
  expected_requested_urls.insert(GURL(kGoodResourceURLB));
  expected_requested_urls.insert(GURL(kGoodResourceURLD));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  histogram.ExpectUniqueSample("Precache.Fetch.PercentCompleted", 100, 1);
  histogram.ExpectUniqueSample("Precache.Fetch.ResponseBytes.Total",
                               url_callback_.total_response_bytes(), 1);
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

TEST_F(PrecacheFetcherTest, PrecacheResourceSelectionMissingBitset) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());

  PrecacheConfigurationSettings config;

  PrecacheManifest good_manifest;
  PrecacheResourceSelection resource_selection;
  good_manifest.add_resource()->set_url(kGoodResourceURL);
  good_manifest.add_resource()->set_url(kGoodResourceURLA);
  good_manifest.add_resource()->set_url(kGoodResourceURLB);
  good_manifest.add_resource()->set_url(kGoodResourceURLC);
  good_manifest.add_resource()->set_url(kGoodResourceURLD);

  // Set bits for a different experiment group.
  resource_selection.set_bitset(0b1);
  (*good_manifest.mutable_experiments()
        ->mutable_resources_by_experiment_group())[kExperimentID + 1] =
      resource_selection;

  // Resource selection bitset for the experiment group will be missing and all
  // resources will be fetched.
  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURLA), "good URL A", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURLB), "good URL B", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURLC), "good URL C", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURLD), "good URL D", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();

    // Destroy the PrecacheFetcher after it has finished, to record metrics.
  }

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  expected_requested_urls.insert(GURL(kGoodResourceURL));
  expected_requested_urls.insert(GURL(kGoodResourceURLA));
  expected_requested_urls.insert(GURL(kGoodResourceURLB));
  expected_requested_urls.insert(GURL(kGoodResourceURLC));
  expected_requested_urls.insert(GURL(kGoodResourceURLD));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  histogram.ExpectUniqueSample("Precache.Fetch.PercentCompleted", 100, 1);
  histogram.ExpectUniqueSample("Precache.Fetch.ResponseBytes.Total",
                               url_callback_.total_response_bytes(), 1);
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

TEST_F(PrecacheFetcherTest, PrecachePauseResume) {
  SetDefaultFlags();

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(3);

  std::unique_ptr<PrecacheUnfinishedWork> initial_work(
      new PrecacheUnfinishedWork());
  initial_work->add_manifest()->set_url(
      "http://manifest-url-prefix.com/manifest1.com");
  initial_work->add_manifest()->set_url(
      "http://manifest-url-prefix.com/manifest2.com");
  initial_work->add_resource()->set_url(kGoodResourceURL);
  initial_work->set_start_time(
      (base::Time::Now() - base::TimeDelta::FromHours(1)).ToInternalValue());

  PrecacheFetcher first_fetcher(request_context_.get(), GURL(), std::string(),
                                std::move(initial_work), kExperimentID,
                                &precache_delegate_);

  loop_.RunUntilIdle();
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work =
      first_fetcher.CancelPrecaching();

  std::multiset<GURL> expected_requested_urls;
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kBadManifestURL), "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL("http://manifest-url-prefix.com/manifest1.com"),
                           "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL("http://manifest-url-prefix.com/manifest2.com"),
                           "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  // Starting hosts should not be fetched.
  unfinished_work->add_top_host()->set_hostname("bad-manifest.com");
  PrecacheFetcher second_fetcher(request_context_.get(), GURL(), std::string(),
                                 std::move(unfinished_work), kExperimentID,
                                 &precache_delegate_);
  second_fetcher.Start();
  loop_.RunUntilIdle();
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(
      GURL("http://manifest-url-prefix.com/manifest1.com"));
  expected_requested_urls.insert(
      GURL("http://manifest-url-prefix.com/manifest2.com"));
  expected_requested_urls.insert(GURL(kGoodResourceURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());
  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, ResumeWithConfigOnly) {
  SetDefaultFlags();
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->mutable_config_settings()->add_forced_site(
      "good-manifest.com");
  unfinished_work->set_start_time(base::Time::Now().ToInternalValue());
  PrecacheManifest good_manifest;
  good_manifest.add_resource()->set_url(kGoodResourceURL);

  factory_.SetFakeResponse(GURL(kGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();
  }

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  expected_requested_urls.insert(GURL(kGoodResourceURL));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());
  EXPECT_TRUE(precache_delegate_.was_on_done_called());

}


TEST_F(PrecacheFetcherTest, CustomURLs) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");

  PrecacheConfigurationSettings config;

  PrecacheManifest good_manifest;
  good_manifest.add_resource()->set_url(kGoodResourceURL);

  factory_.SetFakeResponse(GURL(kCustomConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kCustomGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodResourceURL), "good", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(
      request_context_.get(), GURL(kCustomConfigURL), kCustomManifestURLPrefix,
      std::move(unfinished_work), kExperimentID, &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kCustomConfigURL));
  expected_requested_urls.insert(GURL(kCustomGoodManifestURL));
  expected_requested_urls.insert(GURL(kGoodResourceURL));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, ConfigFetchFailure) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");

  factory_.SetFakeResponse(GURL(kConfigURL), "",
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(GURL(kGoodManifestURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                   std::string(), std::move(unfinished_work),
                                   kExperimentID, &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, BadConfig) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");

  factory_.SetFakeResponse(GURL(kConfigURL), "bad protobuf", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodManifestURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                   std::string(), std::move(unfinished_work),
                                   kExperimentID, &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

TEST_F(PrecacheFetcherTest, Cancel) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("starting-url.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(1);

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    // Destroy the PrecacheFetcher, to cancel precaching. No metrics
    // should be recorded because this should not cause OnDone to be
    // called on the precache delegate.
  }

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_FALSE(precache_delegate_.was_on_done_called());

  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 0);
}

#if defined(PRECACHE_CONFIG_SETTINGS_URL)

// If the default precache configuration settings URL is defined, then test that
// it works with the PrecacheFetcher.
TEST_F(PrecacheFetcherTest, PrecacheUsingDefaultConfigSettingsURL) {
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("starting-url.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(0);

  factory_.SetFakeResponse(GURL(PRECACHE_CONFIG_SETTINGS_URL),
                           config.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                   std::string(), std::move(unfinished_work),
                                   kExperimentID, &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(PRECACHE_CONFIG_SETTINGS_URL));
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

#endif  // PRECACHE_CONFIG_SETTINGS_URL

#if defined(PRECACHE_MANIFEST_URL_PREFIX)

// If the default precache manifest URL prefix is defined, then test that it
// works with the PrecacheFetcher.
TEST_F(PrecacheFetcherTest, PrecacheUsingDefaultManifestURLPrefix) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheConfigSettingsURL, kConfigURL);

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("starting-url.com");

  PrecacheConfigurationSettings config;
  config.set_top_sites_count(1);

  GURL manifest_url(PRECACHE_MANIFEST_URL_PREFIX "starting-url.com");

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(manifest_url, PrecacheManifest().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                   std::string(), std::move(unfinished_work),
                                   kExperimentID, &precache_delegate_);
  precache_fetcher.Start();

  loop_.RunUntilIdle();

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(manifest_url);
  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());
}

#endif  // PRECACHE_MANIFEST_URL_PREFIX

TEST_F(PrecacheFetcherTest, TopResourcesCount) {
  SetDefaultFlags();

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");

  PrecacheConfigurationSettings config;
  config.set_top_resources_count(3);

  PrecacheManifest good_manifest;
  good_manifest.add_resource()->set_url("http://good-manifest.com/retrieved");
  good_manifest.add_resource()->set_url("http://good-manifest.com/retrieved");
  good_manifest.add_resource()->set_url("http://good-manifest.com/retrieved");
  good_manifest.add_resource()->set_url("http://good-manifest.com/skipped");
  good_manifest.add_resource()->set_url("http://good-manifest.com/skipped");

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL(kGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);
  factory_.SetFakeResponse(GURL("http://good-manifest.com/retrieved"), "good",
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();

    // Destroy the PrecacheFetcher after it has finished, to record metrics.
  }

  std::multiset<GURL> expected_requested_urls;
  expected_requested_urls.insert(GURL(kConfigURL));
  expected_requested_urls.insert(GURL(kGoodManifestURL));
  expected_requested_urls.insert(GURL("http://good-manifest.com/retrieved"));
  expected_requested_urls.insert(GURL("http://good-manifest.com/retrieved"));
  expected_requested_urls.insert(GURL("http://good-manifest.com/retrieved"));

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  histogram.ExpectUniqueSample("Precache.Fetch.PercentCompleted", 100, 1);
  histogram.ExpectUniqueSample("Precache.Fetch.ResponseBytes.Total",
                               url_callback_.total_response_bytes(), 1);
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

// MaxBytesPerResource is impossible to test with net::FakeURLFetcherFactory:
//
// - The PrecacheFetcher::Fetcher's max_bytes logic only applies to network
//   requests, and not cached requests.
// - Forcing PrecacheFetcher::Fetcher to do a network request (i.e. a second
//   request for the same URL) requires either setting a custom error of
//   ERR_CACHE_MISS or setting a custom ETag response header, neither of which
//   is possible under FakeURLFetcherFactory.
//
// PrecacheFetcherFetcherTest.ResourceTooBig tests the bulk of the code. We'll
// assume that PrecacheFetcher passes the right max_bytes to the
// PrecacheFetcher::Fetcher constructor.
//
// TODO(twifkak): Port these tests from FakeURLFetcherFactory to
// MockURLFetcherFactory or EmbeddedTestServer, and add a test that fetches are
// cancelled midstream.

TEST_F(PrecacheFetcherTest, MaxBytesTotal) {
  SetDefaultFlags();
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("good-manifest.com");
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());

  const size_t kNumResources = 15;  // > kMaxParallelFetches.
  const size_t kMaxBytesTotal = 1000;
  const size_t kBytesPerResource = kMaxBytesTotal / 3;
  // kBytesPerResource * kMaxParallelFeches > kMaxBytesTotal.

  PrecacheConfigurationSettings config;
  config.set_max_bytes_total(kMaxBytesTotal);

  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  PrecacheManifest good_manifest;
  for (size_t i = 0; i < kNumResources; ++i) {
    const std::string url = "http://good-manifest.com/" + std::to_string(i);
    good_manifest.add_resource()->set_url(url);
    factory_.SetFakeResponse(GURL(url), std::string(kBytesPerResource, '.'),
                             net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  }

  factory_.SetFakeResponse(GURL(kGoodManifestURL),
                           good_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();
  }

  // We don't know which will make it and which won't due to the parallelism in
  // the pool of Fetchers.
  EXPECT_GT(kNumResources, url_callback_.requested_urls().size());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  // good-manifest.com will not have been completed.
  EXPECT_THAT(histogram.GetAllSamples("Precache.Fetch.PercentCompleted"),
              ElementsAre(base::Bucket(0, 1)));
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

// Tests the parallel fetch behaviour when more precache resource and manifest
// requests are available than the maximum capacity of fetcher pool.
TEST_F(PrecacheFetcherTest, FetcherPoolMaxLimitReached) {
  SetDefaultFlags();

  const size_t kNumTopHosts = 5;
  const size_t kNumResources = 15;

  PrecacheConfigurationSettings config;
  PrecacheManifest top_host_manifest[kNumTopHosts];
  std::multiset<GURL> expected_requested_urls;

  config.set_top_sites_count(kNumTopHosts);
  factory_.SetFakeResponse(GURL(kConfigURL), config.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  expected_requested_urls.insert(GURL(kConfigURL));

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());

  for (size_t i = 0; i < kNumTopHosts; ++i) {
    const std::string top_host_url = base::StringPrintf("top-host-%zu.com", i);
    unfinished_work->add_top_host()->set_hostname(top_host_url);

    for (size_t j = 0; j < kNumResources; ++j) {
      const std::string resource_url =
          base::StringPrintf("http://top-host-%zu.com/resource-%zu", i, j);
      top_host_manifest[i].add_resource()->set_url(resource_url);
      factory_.SetFakeResponse(GURL(resource_url), "good", net::HTTP_OK,
                               net::URLRequestStatus::SUCCESS);
      expected_requested_urls.insert(GURL(resource_url));
    }
    factory_.SetFakeResponse(GURL(kManifestURLPrefix + top_host_url),
                             top_host_manifest[i].SerializeAsString(),
                             net::HTTP_OK, net::URLRequestStatus::SUCCESS);
    expected_requested_urls.insert(GURL(kManifestURLPrefix + top_host_url));
  }

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    EXPECT_GT(kNumResources, precache_fetcher.pool_.max_size());
    CheckUntilParallelFetchesBeyondCapacity(&precache_fetcher);

    loop_.RunUntilIdle();

    // Destroy the PrecacheFetcher after it has finished, to record metrics.
  }

  EXPECT_TRUE(parallel_fetches_beyond_capacity_);

  EXPECT_EQ(expected_requested_urls, url_callback_.requested_urls());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  histogram.ExpectUniqueSample("Precache.Fetch.PercentCompleted", 100, 1);
  histogram.ExpectUniqueSample("Precache.Fetch.ResponseBytes.Total",
                               url_callback_.total_response_bytes(), 1);
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

TEST_F(PrecacheFetcherTest, FilterInvalidManifestUrls) {
  SetDefaultFlags();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kPrecacheManifestURLPrefix, "invalid-manifest-prefix");

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("manifest.com");
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());

  factory_.SetFakeResponse(GURL(kConfigURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();
  }

  // The config is fetched, but not the invalid manifest URL.
  EXPECT_EQ(1UL, url_callback_.requested_urls().size());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  // manifest.com will have been failed to complete, in this case.
  EXPECT_THAT(histogram.GetAllSamples("Precache.Fetch.PercentCompleted"),
              ElementsAre(base::Bucket(0, 1)));
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

TEST_F(PrecacheFetcherTest, FilterInvalidResourceUrls) {
  SetDefaultFlags();
  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work(
      new PrecacheUnfinishedWork());
  unfinished_work->add_top_host()->set_hostname("bad-manifest.com");
  unfinished_work->set_start_time(base::Time::UnixEpoch().ToInternalValue());

  factory_.SetFakeResponse(GURL(kConfigURL), "", net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  PrecacheManifest bad_manifest;
  bad_manifest.add_resource()->set_url("http://");

  factory_.SetFakeResponse(GURL(kBadManifestURL),
                           bad_manifest.SerializeAsString(), net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  base::HistogramTester histogram;

  {
    PrecacheFetcher precache_fetcher(request_context_.get(), GURL(),
                                     std::string(), std::move(unfinished_work),
                                     kExperimentID, &precache_delegate_);
    precache_fetcher.Start();

    loop_.RunUntilIdle();
  }

  // The config and manifest are fetched, but not the invalid resource URL.
  EXPECT_EQ(2UL, url_callback_.requested_urls().size());

  EXPECT_TRUE(precache_delegate_.was_on_done_called());

  // bad-manifest.com will have been completed.
  EXPECT_THAT(histogram.GetAllSamples("Precache.Fetch.PercentCompleted"),
              ElementsAre(base::Bucket(100, 1)));
  histogram.ExpectTotalCount("Precache.Fetch.TimeToComplete", 1);
}

}  // namespace precache

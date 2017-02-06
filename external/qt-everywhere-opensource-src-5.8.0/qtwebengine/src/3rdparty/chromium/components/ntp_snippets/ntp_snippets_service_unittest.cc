// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_service.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/image_fetcher/image_decoder.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_database.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_scheduler.h"
#include "components/ntp_snippets/ntp_snippets_test_utils.h"
#include "components/ntp_snippets/switches.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Eq;
using testing::Return;
using testing::IsEmpty;
using testing::SizeIs;
using testing::StartsWith;
using testing::_;

namespace ntp_snippets {

namespace {

MATCHER_P(IdEq, value, "") {
  return arg->id() == value;
}

const base::Time::Exploded kDefaultCreationTime = {2015, 11, 4, 25, 13, 46, 45};
const char kTestContentSnippetsServerFormat[] =
    "https://chromereader-pa.googleapis.com/v1/fetch?key=%s";

const char kSnippetUrl[] = "http://localhost/foobar";
const char kSnippetTitle[] = "Title";
const char kSnippetText[] = "Snippet";
const char kSnippetSalientImage[] = "http://localhost/salient_image";
const char kSnippetPublisherName[] = "Foo News";
const char kSnippetAmpUrl[] = "http://localhost/amp";
const float kSnippetScore = 5.0;

base::Time GetDefaultCreationTime() {
  return base::Time::FromUTCExploded(kDefaultCreationTime);
}

base::Time GetDefaultExpirationTime() {
  return base::Time::Now() + base::TimeDelta::FromHours(1);
}

std::string GetTestJson(const std::vector<std::string>& snippets) {
  return base::StringPrintf("{\"recos\":[%s]}",
                            base::JoinString(snippets, ", ").c_str());
}

std::string GetSnippetWithUrlAndTimesAndSources(
    const std::string& url,
    const std::string& content_creation_time_str,
    const std::string& expiry_time_str,
    const std::vector<std::string>& source_urls,
    const std::vector<std::string>& publishers,
    const std::vector<std::string>& amp_urls) {
  char json_str_format[] =
      "{ \"contentInfo\": {"
      "\"url\" : \"%s\","
      "\"title\" : \"%s\","
      "\"snippet\" : \"%s\","
      "\"thumbnailUrl\" : \"%s\","
      "\"creationTimestampSec\" : \"%s\","
      "\"expiryTimestampSec\" : \"%s\","
      "\"sourceCorpusInfo\" : [%s]"
      "}, "
      "\"score\" : %f}";

  char source_corpus_info_format[] =
      "{\"corpusId\": \"%s\","
      "\"publisherData\": {"
      "\"sourceName\": \"%s\""
      "},"
      "\"ampUrl\": \"%s\"}";

  std::string source_corpus_info_list_str;
  for (size_t i = 0; i < source_urls.size(); ++i) {
    std::string source_corpus_info_str =
        base::StringPrintf(source_corpus_info_format,
                           source_urls[i].empty() ? "" : source_urls[i].c_str(),
                           publishers[i].empty() ? "" : publishers[i].c_str(),
                           amp_urls[i].empty() ? "" : amp_urls[i].c_str());
    source_corpus_info_list_str.append(source_corpus_info_str);
    source_corpus_info_list_str.append(",");
  }
  // Remove the last comma
  source_corpus_info_list_str.erase(source_corpus_info_list_str.size()-1, 1);
  return base::StringPrintf(json_str_format, url.c_str(), kSnippetTitle,
                            kSnippetText, kSnippetSalientImage,
                            content_creation_time_str.c_str(),
                            expiry_time_str.c_str(),
                            source_corpus_info_list_str.c_str(), kSnippetScore);
}

std::string GetSnippetWithSources(const std::vector<std::string>& source_urls,
                                  const std::vector<std::string>& publishers,
                                  const std::vector<std::string>& amp_urls) {
  return GetSnippetWithUrlAndTimesAndSources(
      kSnippetUrl, NTPSnippet::TimeToJsonString(GetDefaultCreationTime()),
      NTPSnippet::TimeToJsonString(GetDefaultExpirationTime()), source_urls,
      publishers, amp_urls);
}

std::string GetSnippetWithUrlAndTimes(
    const std::string& url,
    const std::string& content_creation_time_str,
    const std::string& expiry_time_str) {
  return GetSnippetWithUrlAndTimesAndSources(
      url, content_creation_time_str, expiry_time_str,
      {std::string(url)}, {std::string(kSnippetPublisherName)},
      {std::string(kSnippetAmpUrl)});
}

std::string GetSnippetWithTimes(const std::string& content_creation_time_str,
                                const std::string& expiry_time_str) {
  return GetSnippetWithUrlAndTimes(kSnippetUrl, content_creation_time_str,
                                   expiry_time_str);
}

std::string GetSnippetWithUrl(const std::string& url) {
  return GetSnippetWithUrlAndTimes(
      url, NTPSnippet::TimeToJsonString(GetDefaultCreationTime()),
      NTPSnippet::TimeToJsonString(GetDefaultExpirationTime()));
}

std::string GetSnippet() {
  return GetSnippetWithUrlAndTimes(
      kSnippetUrl, NTPSnippet::TimeToJsonString(GetDefaultCreationTime()),
      NTPSnippet::TimeToJsonString(GetDefaultExpirationTime()));
}

std::string GetExpiredSnippet() {
  return GetSnippetWithTimes(
      NTPSnippet::TimeToJsonString(GetDefaultCreationTime()),
      NTPSnippet::TimeToJsonString(base::Time::Now()));
}

std::string GetInvalidSnippet() {
  std::string json_str = GetSnippet();
  // Make the json invalid by removing the final closing brace.
  return json_str.substr(0, json_str.size() - 1);
}

std::string GetIncompleteSnippet() {
  std::string json_str = GetSnippet();
  // Rename the "url" entry. The result is syntactically valid json that will
  // fail to parse as snippets.
  size_t pos = json_str.find("\"url\"");
  if (pos == std::string::npos) {
    NOTREACHED();
    return std::string();
  }
  json_str[pos + 1] = 'x';
  return json_str;
}

void ParseJson(
    const std::string& json,
    const ntp_snippets::NTPSnippetsFetcher::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsFetcher::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    success_callback.Run(std::move(value));
  } else {
    error_callback.Run(json_reader.GetErrorMessage());
  }
}

// Factory for FakeURLFetcher objects that always generate errors.
class FailingFakeURLFetcherFactory : public net::URLFetcherFactory {
 public:
  std::unique_ptr<net::URLFetcher> CreateURLFetcher(
      int id, const GURL& url, net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) override {
    return base::MakeUnique<net::FakeURLFetcher>(
        url, d, /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::FAILED);
  }
};

class MockScheduler : public NTPSnippetsScheduler {
 public:
  MOCK_METHOD4(Schedule,
               bool(base::TimeDelta period_wifi_charging,
                    base::TimeDelta period_wifi,
                    base::TimeDelta period_fallback,
                    base::Time reschedule_time));
  MOCK_METHOD0(Unschedule, bool());
};

class MockServiceObserver : public NTPSnippetsServiceObserver {
 public:
  MOCK_METHOD0(NTPSnippetsServiceLoaded, void());
  MOCK_METHOD0(NTPSnippetsServiceShutdown, void());
  MOCK_METHOD1(NTPSnippetsServiceDisabledReasonChanged,
               void(DisabledReason disabled_reason));
};

class WaitForDBLoad : public NTPSnippetsServiceObserver {
 public:
  WaitForDBLoad(NTPSnippetsService* service) : service_(service) {
    service_->AddObserver(this);
    if (!service_->ready())
      run_loop_.Run();
  }

  ~WaitForDBLoad() override {
    service_->RemoveObserver(this);
  }

 private:
  void NTPSnippetsServiceLoaded() override {
    EXPECT_TRUE(service_->ready());
    run_loop_.Quit();
  }

  void NTPSnippetsServiceShutdown() override {}
  void NTPSnippetsServiceDisabledReasonChanged(
      DisabledReason disabled_reason) override {}

  NTPSnippetsService* service_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForDBLoad);
};

}  // namespace

class NTPSnippetsServiceTest : public test::NTPSnippetsTestBase {
 public:
  NTPSnippetsServiceTest()
      : fake_url_fetcher_factory_(
            /*default_factory=*/&failing_url_fetcher_factory_),
        test_url_(base::StringPrintf(kTestContentSnippetsServerFormat,
                                     google_apis::GetAPIKey().c_str())) {
    NTPSnippetsService::RegisterProfilePrefs(pref_service()->registry());

    // Since no SuggestionsService is injected in tests, we need to force the
    // service to fetch from all hosts.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDontRestrict);
    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  ~NTPSnippetsServiceTest() override {
    if (service_)
      service_->Shutdown();

    // We need to run the message loop after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously on the task
    // runner. Without this, we'd get reports of memory leaks.
    service_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    test::NTPSnippetsTestBase::SetUp();
    EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _)).Times(1);
    CreateSnippetsService(/*enabled=*/true);
  }

  void CreateSnippetsService(bool enabled) {
    if (service_)
      service_->Shutdown();

    scoped_refptr<base::SingleThreadTaskRunner> task_runner(
        base::ThreadTaskRunnerHandle::Get());
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(task_runner.get());

    // Delete the current service, so that the database is destroyed before we
    // create the new one, otherwise opening the new database will fail.
    service_.reset();

    ResetSigninManager();
    std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher =
        base::MakeUnique<NTPSnippetsFetcher>(
            fake_signin_manager(), fake_token_service_.get(),
            std::move(request_context_getter), base::Bind(&ParseJson),
            /*is_stable_channel=*/true);

    fake_signin_manager()->SignIn("foo@bar.com");
    snippets_fetcher->SetPersonalizationForTesting(
        NTPSnippetsFetcher::Personalization::kNonPersonal);

    service_.reset(new NTPSnippetsService(
        enabled, pref_service(), nullptr, "fr", &scheduler_,
        std::move(snippets_fetcher), /*image_fetcher=*/nullptr,
        /*image_fetcher=*/nullptr, base::MakeUnique<NTPSnippetsDatabase>(
                                       database_dir_.path(), task_runner),
        base::MakeUnique<NTPSnippetsStatusService>(fake_signin_manager(),
                                                   mock_sync_service())));

    if (enabled)
      WaitForDBLoad(service_.get());
  }

 protected:
  const GURL& test_url() { return test_url_; }
  NTPSnippetsService* service() { return service_.get(); }
  MockScheduler& mock_scheduler() {  return scheduler_; }

  // Provide the json to be returned by the fake fetcher.
  void SetUpFetchResponse(const std::string& json) {
    fake_url_fetcher_factory_.SetFakeResponse(test_url_, json, net::HTTP_OK,
                                              net::URLRequestStatus::SUCCESS);
  }

  void LoadFromJSONString(const std::string& json) {
    SetUpFetchResponse(json);
    service()->FetchSnippets();
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::MessageLoop message_loop_;
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Instantiation of factory automatically sets itself as URLFetcher's factory.
  net::FakeURLFetcherFactory fake_url_fetcher_factory_;
  const GURL test_url_;
  std::unique_ptr<OAuth2TokenService> fake_token_service_;
  MockScheduler scheduler_;
  // Last so that the dependencies are deleted after the service.
  std::unique_ptr<NTPSnippetsService> service_;

  base::ScopedTempDir database_dir_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsServiceTest);
};

class NTPSnippetsServiceDisabledTest : public NTPSnippetsServiceTest {
 public:
  void SetUp() override {
    test::NTPSnippetsTestBase::SetUp();
    EXPECT_CALL(mock_scheduler(), Unschedule()).Times(1);
    CreateSnippetsService(/*enabled=*/false);
  }
};

TEST_F(NTPSnippetsServiceTest, ScheduleIfEnabled) {
  // SetUp() checks that Schedule is called.
}

TEST_F(NTPSnippetsServiceDisabledTest, Unschedule) {
  // SetUp() checks that Unschedule is called.
}

TEST_F(NTPSnippetsServiceTest, Full) {
  std::string json_str(GetTestJson({GetSnippet()}));

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  const NTPSnippet& snippet = *service()->snippets().front();

  EXPECT_EQ(snippet.id(), kSnippetUrl);
  EXPECT_EQ(snippet.title(), kSnippetTitle);
  EXPECT_EQ(snippet.snippet(), kSnippetText);
  EXPECT_EQ(snippet.salient_image_url(), GURL(kSnippetSalientImage));
  EXPECT_EQ(GetDefaultCreationTime(), snippet.publish_date());
  EXPECT_EQ(snippet.best_source().publisher_name, kSnippetPublisherName);
  EXPECT_EQ(snippet.best_source().amp_url, GURL(kSnippetAmpUrl));
}

TEST_F(NTPSnippetsServiceTest, Clear) {
  std::string json_str(GetTestJson({GetSnippet()}));

  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), SizeIs(1));

  service()->ClearSnippets();
  EXPECT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, InsertAtFront) {
  std::string first("http://first");
  LoadFromJSONString(GetTestJson({GetSnippetWithUrl(first)}));
  EXPECT_THAT(service()->snippets(), ElementsAre(IdEq(first)));

  std::string second("http://second");
  LoadFromJSONString(GetTestJson({GetSnippetWithUrl(second)}));
  // The snippet loaded last should be at the first position in the list now.
  EXPECT_THAT(service()->snippets(), ElementsAre(IdEq(second), IdEq(first)));
}

TEST_F(NTPSnippetsServiceTest, LimitNumSnippets) {
  int max_snippet_count = NTPSnippetsService::GetMaxSnippetCountForTesting();
  int snippets_per_load = max_snippet_count / 2 + 1;
  char url_format[] = "http://localhost/%i";

  std::vector<std::string> snippets1;
  std::vector<std::string> snippets2;
  for (int i = 0; i < snippets_per_load; i++) {
    snippets1.push_back(GetSnippetWithUrl(base::StringPrintf(url_format, i)));
    snippets2.push_back(GetSnippetWithUrl(
        base::StringPrintf(url_format, snippets_per_load + i)));
  }

  LoadFromJSONString(GetTestJson(snippets1));
  ASSERT_THAT(service()->snippets(), SizeIs(snippets1.size()));

  LoadFromJSONString(GetTestJson(snippets2));
  EXPECT_THAT(service()->snippets(), SizeIs(max_snippet_count));
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJson) {
  LoadFromJSONString(GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(service()->snippets_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  EXPECT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, LoadInvalidJsonWithExistingSnippets) {
  LoadFromJSONString(GetTestJson({GetSnippet()}));
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  ASSERT_EQ("OK", service()->snippets_fetcher()->last_status());

  LoadFromJSONString(GetTestJson({GetInvalidSnippet()}));
  EXPECT_THAT(service()->snippets_fetcher()->last_status(),
              StartsWith("Received invalid JSON"));
  // This should not have changed the existing snippets.
  EXPECT_THAT(service()->snippets(), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJson) {
  LoadFromJSONString(GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.",
            service()->snippets_fetcher()->last_status());
  EXPECT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, LoadIncompleteJsonWithExistingSnippets) {
  LoadFromJSONString(GetTestJson({GetSnippet()}));
  ASSERT_THAT(service()->snippets(), SizeIs(1));

  LoadFromJSONString(GetTestJson({GetIncompleteSnippet()}));
  EXPECT_EQ("Invalid / empty list.",
            service()->snippets_fetcher()->last_status());
  // This should not have changed the existing snippets.
  EXPECT_THAT(service()->snippets(), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, Discard) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://site.com"));
  publishers.push_back(std::string("Source 1"));
  amp_urls.push_back(std::string());
  std::string json_str(
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)}));

  LoadFromJSONString(json_str);

  ASSERT_THAT(service()->snippets(), SizeIs(1));

  // Discarding a non-existent snippet shouldn't do anything.
  EXPECT_FALSE(service()->DiscardSnippet("http://othersite.com"));
  EXPECT_THAT(service()->snippets(), SizeIs(1));

  // Discard the snippet.
  EXPECT_TRUE(service()->DiscardSnippet(kSnippetUrl));
  EXPECT_THAT(service()->snippets(), IsEmpty());

  // Make sure that fetching the same snippet again does not re-add it.
  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), IsEmpty());

  // The snippet should stay discarded even after re-creating the service.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _)).Times(1);
  CreateSnippetsService(/*enabled=*/true);
  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), IsEmpty());

  // The snippet can be added again after clearing discarded snippets.
  service()->ClearDiscardedSnippets();
  EXPECT_THAT(service()->snippets(), IsEmpty());
  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), SizeIs(1));
}

TEST_F(NTPSnippetsServiceTest, GetDiscarded) {
  LoadFromJSONString(GetTestJson({GetSnippet()}));

  // For the test, we need the snippet to get discarded.
  ASSERT_TRUE(service()->DiscardSnippet(kSnippetUrl));
  const NTPSnippet::PtrVector& snippets = service()->discarded_snippets();
  EXPECT_EQ(1u, snippets.size());
  for (auto& snippet : snippets) {
    EXPECT_EQ(kSnippetUrl, snippet->id());
  }

  // There should be no discarded snippet after clearing the list.
  service()->ClearDiscardedSnippets();
  EXPECT_EQ(0u, service()->discarded_snippets().size());
}

TEST_F(NTPSnippetsServiceTest, CreationTimestampParseFail) {
  std::string json_str(GetTestJson({GetSnippetWithTimes(
      "aaa1448459205",
      NTPSnippet::TimeToJsonString(GetDefaultExpirationTime()))}));

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  const NTPSnippet& snippet = *service()->snippets().front();
  EXPECT_EQ(snippet.id(), kSnippetUrl);
  EXPECT_EQ(snippet.title(), kSnippetTitle);
  EXPECT_EQ(snippet.snippet(), kSnippetText);
  EXPECT_EQ(base::Time::UnixEpoch(), snippet.publish_date());
}

TEST_F(NTPSnippetsServiceTest, RemoveExpiredContent) {
  std::string json_str(GetTestJson({GetExpiredSnippet()}));

  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, TestSingleSource) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  publishers.push_back(std::string("Source 1"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  std::string json_str(
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)}));

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  const NTPSnippet& snippet = *service()->snippets().front();
  EXPECT_EQ(snippet.sources().size(), 1u);
  EXPECT_EQ(snippet.id(), kSnippetUrl);
  EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
  EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
  EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source1.amp.com"));
}

TEST_F(NTPSnippetsServiceTest, TestSingleSourceWithMalformedUrl) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("aaaa"));
  publishers.push_back(std::string("Source 1"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  std::string json_str(
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)}));

  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, TestSingleSourceWithMissingData) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  publishers.push_back(std::string());
  amp_urls.push_back(std::string());
  std::string json_str(
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)}));

  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, TestMultipleSources) {
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string("Source 2"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string("http://source2.amp.com"));
  std::string json_str(
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)}));

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  const NTPSnippet& snippet = *service()->snippets().front();
  // Expect the first source to be chosen
  EXPECT_EQ(snippet.sources().size(), 2u);
  EXPECT_EQ(snippet.id(), kSnippetUrl);
  EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
  EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
  EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source1.amp.com"));
}

TEST_F(NTPSnippetsServiceTest, TestMultipleIncompleteSources) {
  // Set Source 2 to have no AMP url, and Source 1 to have no publisher name
  // Source 2 should win since we favor publisher name over amp url
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.push_back(std::string());
  publishers.push_back(std::string("Source 2"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string());
  std::string json_str(
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)}));

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  {
    const NTPSnippet& snippet = *service()->snippets().front();
    EXPECT_EQ(snippet.sources().size(), 2u);
    EXPECT_EQ(snippet.id(), kSnippetUrl);
    EXPECT_EQ(snippet.best_source().url, GURL("http://source2.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 2"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL());
  }

  service()->ClearSnippets();
  // Set Source 1 to have no AMP url, and Source 2 to have no publisher name
  // Source 1 should win in this case since we prefer publisher name to AMP url
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.clear();
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string());
  amp_urls.clear();
  amp_urls.push_back(std::string());
  amp_urls.push_back(std::string("http://source2.amp.com"));
  json_str =
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)});

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  {
    const NTPSnippet& snippet = *service()->snippets().front();
    EXPECT_EQ(snippet.sources().size(), 2u);
    EXPECT_EQ(snippet.id(), kSnippetUrl);
    EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL());
  }

  service()->ClearSnippets();
  // Set source 1 to have no AMP url and no source, and source 2 to only have
  // amp url. There should be no snippets since we only add sources we consider
  // complete
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  publishers.clear();
  publishers.push_back(std::string());
  publishers.push_back(std::string());
  amp_urls.clear();
  amp_urls.push_back(std::string());
  amp_urls.push_back(std::string("http://source2.amp.com"));
  json_str =
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)});

  LoadFromJSONString(json_str);
  EXPECT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, TestMultipleCompleteSources) {
  // Test 2 complete sources, we should choose the first complete source
  std::vector<std::string> source_urls, publishers, amp_urls;
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  source_urls.push_back(std::string("http://source3.com"));
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string());
  publishers.push_back(std::string("Source 3"));
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string("http://source2.amp.com"));
  amp_urls.push_back(std::string("http://source3.amp.com"));
  std::string json_str(
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)}));

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  {
    const NTPSnippet& snippet = *service()->snippets().front();
    EXPECT_EQ(snippet.sources().size(), 3u);
    EXPECT_EQ(snippet.id(), kSnippetUrl);
    EXPECT_EQ(snippet.best_source().url, GURL("http://source1.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 1"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source1.amp.com"));
  }

  // Test 2 complete sources, we should choose the first complete source
  service()->ClearSnippets();
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  source_urls.push_back(std::string("http://source3.com"));
  publishers.clear();
  publishers.push_back(std::string());
  publishers.push_back(std::string("Source 2"));
  publishers.push_back(std::string("Source 3"));
  amp_urls.clear();
  amp_urls.push_back(std::string("http://source1.amp.com"));
  amp_urls.push_back(std::string("http://source2.amp.com"));
  amp_urls.push_back(std::string("http://source3.amp.com"));
  json_str =
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)});

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  {
    const NTPSnippet& snippet = *service()->snippets().front();
    EXPECT_EQ(snippet.sources().size(), 3u);
    EXPECT_EQ(snippet.id(), kSnippetUrl);
    EXPECT_EQ(snippet.best_source().url, GURL("http://source2.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 2"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source2.amp.com"));
  }

  // Test 3 complete sources, we should choose the first complete source
  service()->ClearSnippets();
  source_urls.clear();
  source_urls.push_back(std::string("http://source1.com"));
  source_urls.push_back(std::string("http://source2.com"));
  source_urls.push_back(std::string("http://source3.com"));
  publishers.clear();
  publishers.push_back(std::string("Source 1"));
  publishers.push_back(std::string("Source 2"));
  publishers.push_back(std::string("Source 3"));
  amp_urls.clear();
  amp_urls.push_back(std::string());
  amp_urls.push_back(std::string("http://source2.amp.com"));
  amp_urls.push_back(std::string("http://source3.amp.com"));
  json_str =
      GetTestJson({GetSnippetWithSources(source_urls, publishers, amp_urls)});

  LoadFromJSONString(json_str);
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  {
    const NTPSnippet& snippet = *service()->snippets().front();
    EXPECT_EQ(snippet.sources().size(), 3u);
    EXPECT_EQ(snippet.id(), kSnippetUrl);
    EXPECT_EQ(snippet.best_source().url, GURL("http://source2.com"));
    EXPECT_EQ(snippet.best_source().publisher_name, std::string("Source 2"));
    EXPECT_EQ(snippet.best_source().amp_url, GURL("http://source2.amp.com"));
  }
}

TEST_F(NTPSnippetsServiceTest, LogNumArticlesHistogram) {
  base::HistogramTester tester;
  LoadFromJSONString(GetTestJson({GetInvalidSnippet()}));

  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  // Invalid JSON shouldn't contribute to NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              IsEmpty());
  // Valid JSON with empty list.
  LoadFromJSONString(GetTestJson(std::vector<std::string>()));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  // Snippet list should be populated with size 1.
  LoadFromJSONString(GetTestJson({GetSnippet()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/1)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/1)));
  // Duplicate snippet shouldn't increase the list size.
  LoadFromJSONString(GetTestJson({GetSnippet()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/2),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      IsEmpty());
  // Discarding a snippet should decrease the list size. This will only be
  // logged after the next fetch.
  EXPECT_TRUE(service()->DiscardSnippet(kSnippetUrl));
  LoadFromJSONString(GetTestJson({GetSnippet()}));
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticles"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3),
                          base::Bucket(/*min=*/1, /*count=*/2)));
  // Discarded snippets shouldn't influence NumArticlesFetched.
  EXPECT_THAT(tester.GetAllSamples("NewTabPage.Snippets.NumArticlesFetched"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/3)));
  EXPECT_THAT(
      tester.GetAllSamples("NewTabPage.Snippets.NumArticlesZeroDueToDiscarded"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  // Recreating the service and loading from prefs shouldn't count as fetched
  // articles.
  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _)).Times(1);
  CreateSnippetsService(/*enabled=*/true);
  tester.ExpectTotalCount("NewTabPage.Snippets.NumArticlesFetched", 4);
}

TEST_F(NTPSnippetsServiceTest, DiscardShouldRespectAllKnownUrls) {
  const std::string creation =
      NTPSnippet::TimeToJsonString(GetDefaultCreationTime());
  const std::string expiry =
      NTPSnippet::TimeToJsonString(GetDefaultExpirationTime());
  const std::vector<std::string> source_urls = {
      "http://mashable.com/2016/05/11/stolen",
      "http://www.aol.com/article/2016/05/stolen-doggie",
      "http://mashable.com/2016/05/11/stolen?utm_cid=1"};
  const std::vector<std::string> publishers = {"Mashable", "AOL", "Mashable"};
  const std::vector<std::string> amp_urls = {
      "http://mashable-amphtml.googleusercontent.com/1",
      "http://t2.gstatic.com/images?q=tbn:3",
      "http://t2.gstatic.com/images?q=tbn:3"};

  // Add the snippet from the mashable domain.
  LoadFromJSONString(GetTestJson({GetSnippetWithUrlAndTimesAndSources(
      source_urls[0], creation, expiry, source_urls, publishers, amp_urls)}));
  ASSERT_THAT(service()->snippets(), SizeIs(1));
  // Discard the snippet via the mashable source corpus ID.
  EXPECT_TRUE(service()->DiscardSnippet(source_urls[0]));
  EXPECT_THAT(service()->snippets(), IsEmpty());

  // The same article from the AOL domain should now be detected as discarded.
  LoadFromJSONString(GetTestJson({GetSnippetWithUrlAndTimesAndSources(
      source_urls[1], creation, expiry, source_urls, publishers, amp_urls)}));
  ASSERT_THAT(service()->snippets(), IsEmpty());
}

TEST_F(NTPSnippetsServiceTest, HistorySyncStateChanges) {
  MockServiceObserver mock_observer;
  service()->AddObserver(&mock_observer);

  // Simulate user signed out
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  EXPECT_CALL(mock_observer, NTPSnippetsServiceDisabledReasonChanged(
                                 DisabledReason::SIGNED_OUT));
  service()->UpdateStateForStatus(DisabledReason::SIGNED_OUT);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NTPSnippetsService::State::DISABLED, service()->state_);
  EXPECT_THAT(service()->snippets(), IsEmpty()); // No fetch should be made.

  // Simulate user sign in. The service should be ready again and load snippets.
  SetUpFetchResponse(GetTestJson({GetSnippet()}));
  EXPECT_CALL(mock_observer,
              NTPSnippetsServiceDisabledReasonChanged(DisabledReason::NONE));
  EXPECT_CALL(mock_scheduler(), Schedule(_, _, _, _)).Times(1);
  service()->UpdateStateForStatus(DisabledReason::NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NTPSnippetsService::State::READY, service()->state_);
  EXPECT_FALSE(service()->snippets().empty());

  service()->RemoveObserver(&mock_observer);
}

}  // namespace ntp_snippets

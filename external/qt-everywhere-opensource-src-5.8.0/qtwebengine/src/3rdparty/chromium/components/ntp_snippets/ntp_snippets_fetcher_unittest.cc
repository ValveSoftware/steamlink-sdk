// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_fetcher.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::PrintToString;
using testing::SizeIs;
using testing::StartsWith;

const char kTestChromeReaderUrlFormat[] =
    "https://chromereader-pa.googleapis.com/v1/fetch?key=%s";
const char kTestChromeContentSuggestionsUrlFormat[] =
    "https://chromecontentsuggestions-pa.googleapis.com/v1/snippets/"
    "list?key=%s";
const char kContentSuggestionsBackend[] =
    "https://chromecontentsuggestions-pa.googleapis.com/v1/snippets/list";

// Artificial time delay for JSON parsing.
const int64_t kTestJsonParsingLatencyMs = 20;

MATCHER(HasValue, "") {
  return static_cast<bool>(arg);
}

MATCHER_P(PointeeSizeIs,
          size,
          std::string("contains a value with size ") + PrintToString(size)) {
  return arg && static_cast<int>(arg->size()) == size;
}

MATCHER_P(EqualsJSON, json, "equals JSON") {
  std::unique_ptr<base::Value> expected = base::JSONReader::Read(json);
  if (!expected) {
    *result_listener << "INTERNAL ERROR: couldn't parse expected JSON";
    return false;
  }

  std::string err_msg;
  int err_line, err_col;
  std::unique_ptr<base::Value> actual = base::JSONReader::ReadAndReturnError(
      arg, base::JSON_PARSE_RFC, nullptr, &err_msg, &err_line, &err_col);
  if (!actual) {
    *result_listener << "input:" << err_line << ":" << err_col << ": "
                     << "parse error: " << err_msg;
    return false;
  }
  return base::Value::Equals(actual.get(), expected.get());
}

class MockSnippetsAvailableCallback {
 public:
  // Workaround for gMock's lack of support for movable arguments.
  void WrappedRun(NTPSnippetsFetcher::OptionalSnippets snippets) {
    Run(snippets);
  }

  MOCK_METHOD1(Run, void(const NTPSnippetsFetcher::OptionalSnippets& snippets));
};

// Factory for FakeURLFetcher objects that always generate errors.
class FailingFakeURLFetcherFactory : public net::URLFetcherFactory {
 public:
  std::unique_ptr<net::URLFetcher> CreateURLFetcher(
      int id, const GURL& url, net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) override {
    return base::WrapUnique(new net::FakeURLFetcher(
        url, d, /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::FAILED));
  }
};

void ParseJson(
    const std::string& json,
    const ntp_snippets::NTPSnippetsFetcher::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsFetcher::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value)
    success_callback.Run(std::move(value));
  else
    error_callback.Run(json_reader.GetErrorMessage());
}

void ParseJsonDelayed(
    const std::string& json,
    const ntp_snippets::NTPSnippetsFetcher::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsFetcher::ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ParseJson, json, success_callback, error_callback),
      base::TimeDelta::FromMilliseconds(kTestJsonParsingLatencyMs));
}

class VariationParams {
 public:
  VariationParams(const std::string& trial_name,
                  const std::map<std::string, std::string>& params)
      : field_trial_list_(new metrics::SHA1EntropyProvider("foo")) {
    variations::AssociateVariationParams(trial_name, "Group1", params);
    base::FieldTrialList::CreateFieldTrial(trial_name, "Group1");
  }

  ~VariationParams() { variations::testing::ClearAllVariationParams(); }

 private:
  base::FieldTrialList field_trial_list_;
};

}  // namespace

class NTPSnippetsFetcherTest : public testing::Test {
 public:
  NTPSnippetsFetcherTest()
      : NTPSnippetsFetcherTest(
            GURL(base::StringPrintf(kTestChromeReaderUrlFormat,
                                    google_apis::GetAPIKey().c_str())),
            std::map<std::string, std::string>()) {}

  NTPSnippetsFetcherTest(const GURL& gurl,
                         const std::map<std::string, std::string>& params)
      : params_(ntp_snippets::kStudyName, params),
        mock_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_task_runner_handle_(mock_task_runner_),
        signin_client_(new TestSigninClient(nullptr)),
        account_tracker_(new AccountTrackerService()),
        fake_signin_manager_(new FakeSigninManagerBase(signin_client_.get(),
                                                       account_tracker_.get())),
        fake_token_service_(new FakeProfileOAuth2TokenService()),
        snippets_fetcher_(
            fake_signin_manager_.get(),
            fake_token_service_.get(),
            scoped_refptr<net::TestURLRequestContextGetter>(
                new net::TestURLRequestContextGetter(mock_task_runner_.get())),
            base::Bind(&ParseJsonDelayed),
            /*is_stable_channel=*/true),
        test_lang_("en-US"),
        test_url_(gurl) {
    snippets_fetcher_.SetCallback(
        base::Bind(&MockSnippetsAvailableCallback::WrappedRun,
                   base::Unretained(&mock_callback_)));
    snippets_fetcher_.SetTickClockForTesting(
        mock_task_runner_->GetMockTickClock());
    test_hosts_.insert("www.somehost.com");
    // Increase initial time such that ticks are non-zero.
    mock_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(1234));
  }

  NTPSnippetsFetcher& snippets_fetcher() { return snippets_fetcher_; }
  MockSnippetsAvailableCallback& mock_callback() { return mock_callback_; }
  void FastForwardUntilNoTasksRemain() {
    mock_task_runner_->FastForwardUntilNoTasksRemain();
  }
  const std::string& test_lang() const { return test_lang_; }
  const GURL& test_url() { return test_url_; }
  const std::set<std::string>& test_hosts() const { return test_hosts_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void InitFakeURLFetcherFactory() {
    if (fake_url_fetcher_factory_)
      return;
    // Instantiation of factory automatically sets itself as URLFetcher's
    // factory.
    fake_url_fetcher_factory_.reset(new net::FakeURLFetcherFactory(
        /*default_factory=*/&failing_url_fetcher_factory_));
  }

  void SetFakeResponse(const std::string& response_data,
                       net::HttpStatusCode response_code,
                       net::URLRequestStatus::Status status) {
    InitFakeURLFetcherFactory();
    fake_url_fetcher_factory_->SetFakeResponse(test_url_, response_data,
                                               response_code, status);
  }

 private:
  VariationParams params_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  base::ThreadTaskRunnerHandle mock_task_runner_handle_;
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Initialized lazily in SetFakeResponse().
  std::unique_ptr<net::FakeURLFetcherFactory> fake_url_fetcher_factory_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
  std::unique_ptr<SigninManagerBase> fake_signin_manager_;
  std::unique_ptr<OAuth2TokenService> fake_token_service_;
  NTPSnippetsFetcher snippets_fetcher_;
  MockSnippetsAvailableCallback mock_callback_;
  const std::string test_lang_;
  const GURL test_url_;
  std::set<std::string> test_hosts_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcherTest);
};

class NTPSnippetsContentSuggestionsFetcherTest : public NTPSnippetsFetcherTest {
 public:
  NTPSnippetsContentSuggestionsFetcherTest()
      : NTPSnippetsFetcherTest(
            GURL(base::StringPrintf(kTestChromeContentSuggestionsUrlFormat,
                                    google_apis::GetAPIKey().c_str())),
            std::map<std::string, std::string>{
                {"content_suggestions_backend", kContentSuggestionsBackend}}) {}
};

TEST_F(NTPSnippetsFetcherTest, BuildRequestAuthenticated) {
  NTPSnippetsFetcher::RequestParams params;
  params.obfuscated_gaia_id = "0BFUSGAIA";
  params.only_return_personalized_results = true;
  params.user_locale = "en";
  params.host_restricts = {"chromium.org"};
  params.count_to_fetch = 25;

  params.fetch_api = NTPSnippetsFetcher::CHROME_READER_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"response_detail_level\": \"STANDARD\","
                         "  \"obfuscated_gaia_id\": \"0BFUSGAIA\","
                         "  \"user_locale\": \"en\","
                         "  \"advanced_options\": {"
                         "    \"local_scoring_params\": {"
                         "      \"content_params\": {"
                         "        \"only_return_personalized_results\": true"
                         "      },"
                         "      \"content_restricts\": ["
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"TITLE\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"SNIPPET\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"THUMBNAIL\""
                         "        }"
                         "      ],"
                         "      \"content_selectors\": ["
                         "        {"
                         "          \"type\": \"HOST_RESTRICT\","
                         "          \"value\": \"chromium.org\""
                         "        }"
                         "      ]"
                         "    },"
                         "    \"global_scoring_params\": {"
                         "      \"num_to_return\": 25,"
                         "      \"sort_type\": 1"
                         "    }"
                         "  }"
                         "}"));

  params.fetch_api = NTPSnippetsFetcher::CHROME_CONTENT_SUGGESTIONS_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"uiLanguage\": \"en\","
                         "  \"regularlyVisitedHostName\": ["
                         "    \"chromium.org\""
                         "  ]"
                         "}"));
}

TEST_F(NTPSnippetsFetcherTest, BuildRequestUnauthenticated) {
  NTPSnippetsFetcher::RequestParams params;
  params.only_return_personalized_results = false;
  params.host_restricts = {};
  params.count_to_fetch = 10;

  params.fetch_api = NTPSnippetsFetcher::CHROME_READER_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"response_detail_level\": \"STANDARD\","
                         "  \"advanced_options\": {"
                         "    \"local_scoring_params\": {"
                         "      \"content_params\": {"
                         "        \"only_return_personalized_results\": false"
                         "      },"
                         "      \"content_restricts\": ["
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"TITLE\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"SNIPPET\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"THUMBNAIL\""
                         "        }"
                         "      ],"
                         "      \"content_selectors\": []"
                         "    },"
                         "    \"global_scoring_params\": {"
                         "      \"num_to_return\": 10,"
                         "      \"sort_type\": 1"
                         "    }"
                         "  }"
                         "}"));

  params.fetch_api = NTPSnippetsFetcher::CHROME_CONTENT_SUGGESTIONS_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"regularlyVisitedHostName\": []"
                         "}"));
}

TEST_F(NTPSnippetsFetcherTest, ShouldNotFetchOnCreation) {
  // The lack of registered baked in responses would cause any fetch to fail.
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              IsEmpty());
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              IsEmpty());
  EXPECT_THAT(snippets_fetcher().last_status(), IsEmpty());
}

TEST_F(NTPSnippetsFetcherTest, ShouldFetchSuccessfully) {
  const std::string kJsonStr =
      "{\"recos\": [{"
      "  \"contentInfo\": {"
      "    \"url\" : \"http://localhost/foobar\","
      "    \"sourceCorpusInfo\" : [{"
      "      \"ampUrl\" : \"http://localhost/amp\","
      "      \"corpusId\" : \"http://localhost/foobar\","
      "      \"publisherData\": { \"sourceName\" : \"Foo News\" }"
      "    }]"
      "  }"
      "}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/PointeeSizeIs(1))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsContentSuggestionsFetcherTest, ShouldFetchSuccessfully) {
  const std::string kJsonStr =
      "{\"snippet\" : [{"
      "  \"id\" : [\"http://localhost/foobar\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"summaryText\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"publishTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"publisherName\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/PointeeSizeIs(1))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldFetchSuccessfullyEmptyList) {
  const std::string kJsonStr = "{\"recos\": []}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/PointeeSizeIs(0))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

// TODO(jkrcal) Return the tests ShouldReportEmptyHostsError and
// ShouldRestrictToHosts once we have a way to change variation parameters from
// unittests. The tests were tailored to previous default value of the parameter
// fetching_host_restrict, which changed now.
TEST_F(NTPSnippetsFetcherTest, ShouldReportUrlStatusError) {
  SetFakeResponse(/*data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::URLRequestStatus::FAILED);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(),
              Eq("URLRequestStatus error -2"));
  EXPECT_THAT(snippets_fetcher().last_json(), IsEmpty());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/-2, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportHttpError) {
  SetFakeResponse(/*data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_json(), IsEmpty());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/404, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportJsonError) {
  const std::string kInvalidJsonStr = "{ \"recos\": []";
  SetFakeResponse(/*data=*/kInvalidJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(),
              StartsWith("Received invalid JSON (error "));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kInvalidJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportJsonErrorForEmptyResponse) {
  SetFakeResponse(/*data=*/std::string(), net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_json(), std::string());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportInvalidListError) {
  const std::string kJsonStr =
      "{\"recos\": [{ \"contentInfo\": { \"foo\" : \"bar\" }}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

// This test actually verifies that the test setup itself is sane, to prevent
// hard-to-reproduce test failures.
TEST_F(NTPSnippetsFetcherTest, ShouldReportHttpErrorForMissingBakedResponse) {
  InitFakeURLFetcherFactory();
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
}

TEST_F(NTPSnippetsFetcherTest, ShouldCancelOngoingFetch) {
  const std::string kJsonStr = "{ \"recos\": [] }";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/PointeeSizeIs(0))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  // Second call to FetchSnippetsFromHosts() overrides/cancels the previous.
  // Callback is expected to be called once.
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            /*count=*/1);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

::std::ostream& operator<<(
    ::std::ostream& os,
    const NTPSnippetsFetcher::OptionalSnippets& snippets) {
  if (snippets) {
    // Matchers above aren't any more precise than this, so this is sufficient
    // for test-failure diagnostics.
    return os << "list with " << snippets->size() << " elements";
  } else {
    return os << "null";
  }
}

}  // namespace ntp_snippets

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync_driver/fake_sync_service.h"
#include "components/sync_driver/sync_service.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using testing::DoAll;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::Return;
using testing::SetArgPointee;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::_;

namespace {

// SuggestionsService::AccessTokenFetcher provides an empty account ID if its
// SigninManager is null.
const char kAccountId[] = "";
const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";
const char kTestFaviconUrl[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url="
    "http://go.com&alt=s&sz=32";
const char kTestImpressionUrl[] =
    "https://www.google.com/chromesuggestions/click?q=123&cd=-1";
const char kTestClickUrl[] =
    "https://www.google.com/chromesuggestions/click?q=123&cd=0";
const char kBlacklistedUrl[] = "http://blacklist.com";
const char kBlacklistedUrlAlt[] = "http://blacklist-atl.com";
const int64_t kTestDefaultExpiry = 1402200000000000;
const int64_t kTestSetExpiry = 1404792000000000;

std::unique_ptr<net::FakeURLFetcher> CreateURLFetcher(
    const GURL& url,
    net::URLFetcherDelegate* delegate,
    const std::string& response_data,
    net::HttpStatusCode response_code,
    net::URLRequestStatus::Status status) {
  std::unique_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
      url, delegate, response_data, response_code, status));

  if (response_code == net::HTTP_OK) {
    scoped_refptr<net::HttpResponseHeaders> download_headers(
        new net::HttpResponseHeaders(""));
    download_headers->AddHeader("Content-Type: text/html");
    fetcher->set_response_headers(download_headers);
  }
  return fetcher;
}

// GMock matcher for protobuf equality.
MATCHER_P(EqualsProto, message, "") {
  // This implementation assumes protobuf serialization is deterministic, which
  // is true in practice but technically not something that code is supposed
  // to rely on.  However, it vastly simplifies the implementation.
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

namespace suggestions {

SuggestionsProfile CreateSuggestionsProfile() {
  SuggestionsProfile profile;
  profile.set_timestamp(123);
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  suggestion->set_expiry_ts(kTestSetExpiry);
  return profile;
}

// Creates one suggestion with expiry timestamp and one without.
SuggestionsProfile CreateSuggestionsProfileWithExpiryTimestamps() {
  SuggestionsProfile profile;
  profile.set_timestamp(123);
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  suggestion->set_expiry_ts(kTestSetExpiry);

  suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);

  return profile;
}

class MockSyncService : public sync_driver::FakeSyncService {
 public:
  MockSyncService() {}
  virtual ~MockSyncService() {}
  MOCK_CONST_METHOD0(CanSyncStart, bool());
  MOCK_CONST_METHOD0(IsSyncActive, bool());
  MOCK_CONST_METHOD0(ConfigurationDone, bool());
  MOCK_CONST_METHOD0(GetActiveDataTypes, syncer::ModelTypeSet());
};

class TestSuggestionsStore : public suggestions::SuggestionsStore {
 public:
  TestSuggestionsStore() {
    cached_suggestions = CreateSuggestionsProfile();
  }
  bool LoadSuggestions(SuggestionsProfile* suggestions) override {
    suggestions->CopyFrom(cached_suggestions);
    return cached_suggestions.suggestions_size();
  }
  bool StoreSuggestions(const SuggestionsProfile& suggestions) override {
    cached_suggestions.CopyFrom(suggestions);
    return true;
  }
  void ClearSuggestions() override {
    cached_suggestions = SuggestionsProfile();
  }

  SuggestionsProfile cached_suggestions;
};

class MockImageManager : public suggestions::ImageManager {
 public:
  MockImageManager() {}
  virtual ~MockImageManager() {}
  MOCK_METHOD1(Initialize, void(const SuggestionsProfile&));
  MOCK_METHOD2(GetImageForURL,
               void(const GURL&,
                    base::Callback<void(const GURL&, const gfx::Image&)>));
  MOCK_METHOD2(AddImageURL, void(const GURL&, const GURL&));
};

class MockBlacklistStore : public suggestions::BlacklistStore {
 public:
  MOCK_METHOD1(BlacklistUrl, bool(const GURL&));
  MOCK_METHOD0(IsEmpty, bool());
  MOCK_METHOD1(GetTimeUntilReadyForUpload, bool(base::TimeDelta*));
  MOCK_METHOD2(GetTimeUntilURLReadyForUpload,
               bool(const GURL&, base::TimeDelta*));
  MOCK_METHOD1(GetCandidateForUpload, bool(GURL*));
  MOCK_METHOD1(RemoveUrl, bool(const GURL&));
  MOCK_METHOD1(FilterSuggestions, void(SuggestionsProfile*));
  MOCK_METHOD0(ClearBlacklist, void());
};

class SuggestionsServiceTest : public testing::Test {
 public:
  void CheckCallback(const SuggestionsProfile& suggestions_profile) {
    ++suggestions_data_callback_count_;
    if (suggestions_profile.suggestions_size() == 0)
      ++suggestions_empty_data_count_;
  }

  void CheckSuggestionsData() {
    SuggestionsProfile suggestions_profile;
    test_suggestions_store_->LoadSuggestions(&suggestions_profile);
    EXPECT_EQ(1, suggestions_profile.suggestions_size());
    EXPECT_EQ(kTestTitle, suggestions_profile.suggestions(0).title());
    EXPECT_EQ(kTestUrl, suggestions_profile.suggestions(0).url());
    EXPECT_EQ(kTestFaviconUrl,
              suggestions_profile.suggestions(0).favicon_url());
    EXPECT_EQ(kTestImpressionUrl,
              suggestions_profile.suggestions(0).impression_url());
    EXPECT_EQ(kTestClickUrl, suggestions_profile.suggestions(0).click_url());
  }

  int suggestions_data_callback_count_;
  int suggestions_empty_data_count_;
  bool blacklisting_failed_;
  bool undo_blacklisting_failed_;

 protected:
  SuggestionsServiceTest()
      : suggestions_data_callback_count_(0),
        suggestions_empty_data_count_(0),
        blacklisting_failed_(false),
        undo_blacklisting_failed_(false),
        factory_(nullptr, base::Bind(&CreateURLFetcher)),
        mock_sync_service_(nullptr),
        mock_thumbnail_manager_(nullptr),
        mock_blacklist_store_(nullptr),
        test_suggestions_store_(nullptr) {
    token_service_.UpdateCredentials(kAccountId, "refresh_token");
    token_service_.set_auto_post_fetch_response_on_message_loop(true);
  }

  ~SuggestionsServiceTest() override {}

  void SetUp() override {
    request_context_ =
        new net::TestURLRequestContextGetter(io_message_loop_.task_runner());
  }

  SuggestionsService* CreateSuggestionsServiceWithMocks() {
    mock_sync_service_.reset(new MockSyncService);
    ON_CALL(*mock_sync_service_, CanSyncStart()).WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_, IsSyncActive()).WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_, ConfigurationDone())
        .WillByDefault(Return(true));
    ON_CALL(*mock_sync_service_, GetActiveDataTypes())
        .WillByDefault(
            Return(syncer::ModelTypeSet(syncer::HISTORY_DELETE_DIRECTIVES)));

    // These objects are owned by the returned SuggestionsService, but we keep
    // the pointer around for testing.
    test_suggestions_store_ = new TestSuggestionsStore();
    mock_thumbnail_manager_ = new StrictMock<MockImageManager>();
    mock_blacklist_store_ = new StrictMock<MockBlacklistStore>();
    return new SuggestionsService(
        nullptr /* signin_manager */, &token_service_, mock_sync_service_.get(),
        request_context_.get(), base::WrapUnique(test_suggestions_store_),
        base::WrapUnique(mock_thumbnail_manager_),
        base::WrapUnique(mock_blacklist_store_));
  }

  void Blacklist(SuggestionsService* suggestions_service, GURL url) {
    blacklisting_failed_ = !suggestions_service->BlacklistURL(url);
  }

  void UndoBlacklist(SuggestionsService* suggestions_service, GURL url) {
    undo_blacklisting_failed_ = !suggestions_service->UndoBlacklistURL(url);
  }

  // Helper for Undo failure tests. Depending on |is_uploaded|, tests either
  // the case where the URL is no longer in the local blacklist or the case
  // in which it's not yet candidate for upload.
  void UndoBlacklistURLFailsHelper(bool is_uploaded) {
    std::unique_ptr<SuggestionsService> suggestions_service(
        CreateSuggestionsServiceWithMocks());
    EXPECT_TRUE(suggestions_service != nullptr);
    // Ensure scheduling the request doesn't happen before undo.
    base::TimeDelta delay = base::TimeDelta::FromHours(1);
    suggestions_service->set_blacklist_delay(delay);

    auto subscription = suggestions_service->AddCallback(base::Bind(
        &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

    SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
    GURL blacklisted_url(kBlacklistedUrl);

    // Blacklist expectations.
    EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_thumbnail_manager_,
                Initialize(EqualsProto(suggestions_profile)));
    EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
    EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
        .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
    // Undo expectations.
    if (is_uploaded) {
      // URL is not in local blacklist.
      EXPECT_CALL(*mock_blacklist_store_,
                  GetTimeUntilURLReadyForUpload(Eq(blacklisted_url), _))
          .WillOnce(Return(false));
    } else {
      // URL is not yet candidate for upload.
      base::TimeDelta negative_delay = base::TimeDelta::FromHours(-1);
      EXPECT_CALL(*mock_blacklist_store_,
                  GetTimeUntilURLReadyForUpload(Eq(blacklisted_url), _))
          .WillOnce(DoAll(SetArgPointee<1>(negative_delay), Return(true)));
    }

    Blacklist(suggestions_service.get(), blacklisted_url);
    UndoBlacklist(suggestions_service.get(), blacklisted_url);

    EXPECT_EQ(1, suggestions_data_callback_count_);
    EXPECT_FALSE(blacklisting_failed_);
    EXPECT_TRUE(undo_blacklisting_failed_);
  }

  bool HasPendingSuggestionsRequest(SuggestionsService* suggestions_service) {
    return !!suggestions_service->pending_request_.get();
  }

 protected:
  base::MessageLoopForIO io_message_loop_;
  net::FakeURLFetcherFactory factory_;
  FakeProfileOAuth2TokenService token_service_;
  std::unique_ptr<MockSyncService> mock_sync_service_;
  // Only used if the SuggestionsService is built with mocks. Not owned.
  MockImageManager* mock_thumbnail_manager_;
  MockBlacklistStore* mock_blacklist_store_;
  TestSuggestionsStore* test_suggestions_store_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, FetchSuggestionsData) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);
  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();

  // Set up net::FakeURLFetcherFactory.
  factory_.SetFakeResponse(SuggestionsService::BuildSuggestionsURL(),
                           suggestions_profile.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Expectations.
  EXPECT_CALL(*mock_thumbnail_manager_, Initialize(_));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. The data should be returned to the callback.
  suggestions_service->FetchSuggestionsData();

  // Let the network request run.
  io_message_loop_.RunUntilIdle();

  // Ensure that CheckCallback() ran once.
  EXPECT_EQ(1, suggestions_data_callback_count_);

  CheckSuggestionsData();
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncNotInitializedEnabled) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);
  EXPECT_CALL(*mock_sync_service_, IsSyncActive())
      .WillRepeatedly(Return(false));

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service->FetchSuggestionsData();

  // Let any network request run.
  io_message_loop_.RunUntilIdle();

  // Ensure that CheckCallback() didn't run.
  EXPECT_EQ(0, suggestions_data_callback_count_);

  // |test_suggestions_store_| should still contain the default values.
  SuggestionsProfile suggestions;
  test_suggestions_store_->LoadSuggestions(&suggestions);
  EXPECT_EQ(CreateSuggestionsProfile().SerializeAsString(),
            suggestions.SerializeAsString());
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncDisabled) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);
  EXPECT_CALL(*mock_sync_service_, CanSyncStart())
      .WillRepeatedly(Return(false));

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  // Tell SuggestionsService that the sync state changed. The cache should be
  // cleared and empty data returned to the callback.
  suggestions_service->OnStateChanged();

  // Ensure that CheckCallback ran once with empty data.
  EXPECT_EQ(1, suggestions_data_callback_count_);
  EXPECT_EQ(1, suggestions_empty_data_count_);

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service->FetchSuggestionsData();

  // Let any network request run.
  io_message_loop_.RunUntilIdle();

  // Ensure that CheckCallback didn't run again.
  EXPECT_EQ(1, suggestions_data_callback_count_);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoAccessToken) {
  token_service_.RevokeCredentials(kAccountId);

  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service->FetchSuggestionsData();

  // No network request should be sent.
  io_message_loop_.RunUntilIdle();
  EXPECT_FALSE(HasPendingSuggestionsRequest(suggestions_service.get()));
  EXPECT_EQ(0, suggestions_data_callback_count_);
}

TEST_F(SuggestionsServiceTest, IssueRequestIfNoneOngoingError) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);

  // Fake a request error.
  factory_.SetFakeResponse(SuggestionsService::BuildSuggestionsURL(),
                           "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);

  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->IssueRequestIfNoneOngoing(
      SuggestionsService::BuildSuggestionsURL());

  // (Testing only) wait until suggestion fetch is complete.
  io_message_loop_.RunUntilIdle();
}

TEST_F(SuggestionsServiceTest, IssueRequestIfNoneOngoingResponseNotOK) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);

  // Fake a non-200 response code.
  factory_.SetFakeResponse(SuggestionsService::BuildSuggestionsURL(),
                           "irrelevant", net::HTTP_BAD_REQUEST,
                           net::URLRequestStatus::SUCCESS);

  // Expect that an upload to the blacklist is scheduled.
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->IssueRequestIfNoneOngoing(
      SuggestionsService::BuildSuggestionsURL());

  // (Testing only) wait until suggestion fetch is complete.
  io_message_loop_.RunUntilIdle();

  // Expect no suggestions in the cache.
  SuggestionsProfile empty_suggestions;
  EXPECT_FALSE(test_suggestions_store_->LoadSuggestions(&empty_suggestions));
}

TEST_F(SuggestionsServiceTest, BlacklistURL) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != nullptr);
  base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service->set_blacklist_delay(no_delay);

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  GURL blacklisted_url(kBlacklistedUrl);
  GURL request_url(
      SuggestionsService::BuildSuggestionsBlacklistURL(blacklisted_url));
  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
  factory_.SetFakeResponse(request_url,
                           suggestions_profile.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(*mock_thumbnail_manager_, Initialize(_)).Times(2);

  // Expected calls to the blacklist store.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(2);
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_blacklist_store_, GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(blacklisted_url), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));

  Blacklist(suggestions_service.get(), blacklisted_url);
  EXPECT_EQ(1, suggestions_data_callback_count_);

  // Wait on the upload task. This only works when the scheduling task is not
  // for future execution (note how both the SuggestionsService's scheduling
  // delay and the BlacklistStore's candidacy delay are zero). Then wait on
  // the blacklist request, then again on the next blacklist scheduling task.
  base::RunLoop().RunUntilIdle();
  io_message_loop_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);
  CheckSuggestionsData();
}

TEST_F(SuggestionsServiceTest, BlacklistURLFails) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  GURL blacklisted_url(kBlacklistedUrl);
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(false));

  Blacklist(suggestions_service.get(), blacklisted_url);

  EXPECT_TRUE(blacklisting_failed_);
  EXPECT_EQ(0, suggestions_data_callback_count_);
}

// Initial blacklist request fails, triggering a second which succeeds.
TEST_F(SuggestionsServiceTest, BlacklistURLRequestFails) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);
  base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service->set_blacklist_delay(no_delay);

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  GURL blacklisted_url(kBlacklistedUrl);
  GURL request_url(
      SuggestionsService::BuildSuggestionsBlacklistURL(blacklisted_url));
  GURL blacklisted_url_alt(kBlacklistedUrlAlt);
  GURL request_url_alt(
      SuggestionsService::BuildSuggestionsBlacklistURL(blacklisted_url_alt));
  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();

  // Note: we want to set the response for the blacklist URL to first
  // succeed, then fail. This doesn't seem possible. For simplicity of testing,
  // we'll pretend the URL changed in the BlacklistStore between the first and
  // the second request, and adjust expectations accordingly.
  factory_.SetFakeResponse(request_url, "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(request_url_alt,
                           suggestions_profile.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Expectations.
  EXPECT_CALL(*mock_thumbnail_manager_, Initialize(_)).Times(2);
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(2);
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_blacklist_store_, GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(blacklisted_url), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(blacklisted_url_alt), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklisted_url_alt)))
      .WillOnce(Return(true));

  // Blacklist call, first request attempt.
  Blacklist(suggestions_service.get(), blacklisted_url);
  EXPECT_EQ(1, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);

  // Wait for the first scheduling, the first request, the second scheduling,
  // second request and the third scheduling. Again, note that calling
  // RunUntilIdle on the MessageLoop only works when the task is not posted for
  // the future.
  base::RunLoop().RunUntilIdle();
  io_message_loop_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  io_message_loop_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  CheckSuggestionsData();
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURL) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);
  // Ensure scheduling the request doesn't happen before undo.
  base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service->set_blacklist_delay(delay);

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
  GURL blacklisted_url(kBlacklistedUrl);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(suggestions_profile)))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  // Undo expectations.
  EXPECT_CALL(*mock_blacklist_store_,
              GetTimeUntilURLReadyForUpload(Eq(blacklisted_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(delay), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklisted_url)))
    .WillOnce(Return(true));

  Blacklist(suggestions_service.get(), blacklisted_url);
  UndoBlacklist(suggestions_service.get(), blacklisted_url);

  EXPECT_EQ(2, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);
  EXPECT_FALSE(undo_blacklisting_failed_);
}

TEST_F(SuggestionsServiceTest, ClearBlacklist) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);
  // Ensure scheduling the request doesn't happen before undo.
  base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service->set_blacklist_delay(delay);

  auto subscription = suggestions_service->AddCallback(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
  GURL blacklisted_url(kBlacklistedUrl);

  factory_.SetFakeResponse(
      SuggestionsService::BuildSuggestionsBlacklistClearURL(),
      suggestions_profile.SerializeAsString(), net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(suggestions_profile)))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, ClearBlacklist());

  Blacklist(suggestions_service.get(), blacklisted_url);
  suggestions_service->ClearBlacklist();

  EXPECT_EQ(2, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfNotInBlacklist) {
  UndoBlacklistURLFailsHelper(true);
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfAlreadyCandidate) {
  UndoBlacklistURLFailsHelper(false);
}

TEST_F(SuggestionsServiceTest, GetBlacklistedUrl) {
  std::unique_ptr<GURL> request_url;
  std::unique_ptr<net::FakeURLFetcher> fetcher;
  GURL retrieved_url;

  // Not a blacklist request.
  request_url.reset(new GURL("http://not-blacklisting.com/a?b=c"));
  fetcher = CreateURLFetcher(*request_url, nullptr, "", net::HTTP_OK,
                             net::URLRequestStatus::SUCCESS);
  EXPECT_FALSE(SuggestionsService::GetBlacklistedUrl(*fetcher, &retrieved_url));

  // An actual blacklist request.
  std::string blacklisted_url = "http://blacklisted.com/a?b=c&d=e";
  std::string encoded_blacklisted_url =
      "http%3A%2F%2Fblacklisted.com%2Fa%3Fb%3Dc%26d%3De";
  std::string blacklist_request_prefix(
      SuggestionsService::BuildSuggestionsBlacklistURLPrefix());
  request_url.reset(
      new GURL(blacklist_request_prefix + encoded_blacklisted_url));
  fetcher.reset();
  fetcher = CreateURLFetcher(*request_url, nullptr, "", net::HTTP_OK,
                             net::URLRequestStatus::SUCCESS);
  EXPECT_TRUE(SuggestionsService::GetBlacklistedUrl(*fetcher, &retrieved_url));
  EXPECT_EQ(blacklisted_url, retrieved_url.spec());
}

TEST_F(SuggestionsServiceTest, UpdateBlacklistDelay) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  base::TimeDelta initial_delay = suggestions_service->blacklist_delay();

  // Delay unchanged on success.
  suggestions_service->UpdateBlacklistDelay(true);
  EXPECT_EQ(initial_delay, suggestions_service->blacklist_delay());

  // Delay increases on failure.
  suggestions_service->UpdateBlacklistDelay(false);
  EXPECT_GT(suggestions_service->blacklist_delay(), initial_delay);

  // Delay resets on success.
  suggestions_service->UpdateBlacklistDelay(true);
  EXPECT_EQ(initial_delay, suggestions_service->blacklist_delay());
}

TEST_F(SuggestionsServiceTest, CheckDefaultTimeStamps) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  SuggestionsProfile suggestions =
      CreateSuggestionsProfileWithExpiryTimestamps();
  suggestions_service->SetDefaultExpiryTimestamp(&suggestions,
                                                 kTestDefaultExpiry);
  EXPECT_EQ(kTestSetExpiry, suggestions.suggestions(0).expiry_ts());
  EXPECT_EQ(kTestDefaultExpiry, suggestions.suggestions(1).expiry_ts());
}

TEST_F(SuggestionsServiceTest, GetPageThumbnail) {
  std::unique_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != nullptr);

  GURL test_url(kTestUrl);
  GURL thumbnail_url("https://www.thumbnails.com/thumb.jpg");
  base::Callback<void(const GURL&, const gfx::Image&)> dummy_callback;

  EXPECT_CALL(*mock_thumbnail_manager_, GetImageForURL(test_url, _));
  suggestions_service->GetPageThumbnail(test_url, dummy_callback);

  EXPECT_CALL(*mock_thumbnail_manager_, AddImageURL(test_url, thumbnail_url));
  EXPECT_CALL(*mock_thumbnail_manager_, GetImageForURL(test_url, _));
  suggestions_service->GetPageThumbnailWithURL(test_url, thumbnail_url,
                                               dummy_callback);

}

}  // namespace suggestions

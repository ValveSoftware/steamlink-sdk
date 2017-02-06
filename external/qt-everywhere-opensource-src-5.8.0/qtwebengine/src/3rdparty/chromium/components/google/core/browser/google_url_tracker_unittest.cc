// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_url_tracker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_url_tracker_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TestCallbackListener ---------------------------------------------------

class TestCallbackListener {
 public:
  TestCallbackListener();
  virtual ~TestCallbackListener();

  bool HasRegisteredCallback();
  void RegisterCallback(GoogleURLTracker* google_url_tracker);

  bool notified() const { return notified_; }
  void clear_notified() { notified_ = false; }

 private:
  void OnGoogleURLUpdated();

  bool notified_;
  std::unique_ptr<GoogleURLTracker::Subscription>
      google_url_updated_subscription_;
};

TestCallbackListener::TestCallbackListener() : notified_(false) {
}

TestCallbackListener::~TestCallbackListener() {
}

void TestCallbackListener::OnGoogleURLUpdated() {
  notified_ = true;
}

bool TestCallbackListener::HasRegisteredCallback() {
  return google_url_updated_subscription_.get();
}

void TestCallbackListener::RegisterCallback(
    GoogleURLTracker* google_url_tracker) {
  google_url_updated_subscription_ =
      google_url_tracker->RegisterCallback(base::Bind(
          &TestCallbackListener::OnGoogleURLUpdated, base::Unretained(this)));
}


// TestGoogleURLTrackerClient -------------------------------------------------

class TestGoogleURLTrackerClient : public GoogleURLTrackerClient {
 public:
  explicit TestGoogleURLTrackerClient(PrefService* prefs_);
  ~TestGoogleURLTrackerClient() override;

  bool IsBackgroundNetworkingEnabled() override;
  PrefService* GetPrefs() override;
  net::URLRequestContextGetter* GetRequestContext() override;

 private:
  PrefService* prefs_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(TestGoogleURLTrackerClient);
};

TestGoogleURLTrackerClient::TestGoogleURLTrackerClient(PrefService* prefs)
    : prefs_(prefs),
      request_context_(new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get())) {
}

TestGoogleURLTrackerClient::~TestGoogleURLTrackerClient() {
}

bool TestGoogleURLTrackerClient::IsBackgroundNetworkingEnabled() {
  return true;
}

PrefService* TestGoogleURLTrackerClient::GetPrefs() {
  return prefs_;
}

net::URLRequestContextGetter* TestGoogleURLTrackerClient::GetRequestContext() {
  return request_context_.get();
}


}  // namespace

// GoogleURLTrackerTest -------------------------------------------------------

class GoogleURLTrackerTest : public testing::Test {
 protected:
  GoogleURLTrackerTest();
  ~GoogleURLTrackerTest() override;

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  net::TestURLFetcher* GetFetcher();
  void MockSearchDomainCheckResponse(const std::string& domain);
  void RequestServerCheck();
  void FinishSleep();
  void NotifyNetworkChanged();
  void set_google_url(const GURL& url) {
    google_url_tracker_->google_url_ = url;
  }
  GURL google_url() const { return google_url_tracker_->google_url(); }
  bool listener_notified() const { return listener_.notified(); }
  void clear_listener_notified() { listener_.clear_notified(); }

 private:
  base::MessageLoop message_loop_;
  TestingPrefServiceSimple prefs_;

  // Creating this allows us to call
  // net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests().
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  net::TestURLFetcherFactory fetcher_factory_;
  GoogleURLTrackerClient* client_;
  std::unique_ptr<GoogleURLTracker> google_url_tracker_;
  TestCallbackListener listener_;
};

GoogleURLTrackerTest::GoogleURLTrackerTest() {
  prefs_.registry()->RegisterStringPref(
      prefs::kLastKnownGoogleURL,
      GoogleURLTracker::kDefaultGoogleHomepage);
}

GoogleURLTrackerTest::~GoogleURLTrackerTest() {
}

void GoogleURLTrackerTest::SetUp() {
  network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
  // Ownership is passed to google_url_tracker_, but a weak pointer is kept;
  // this is safe since GoogleURLTracker keeps the client for its lifetime.
  client_ = new TestGoogleURLTrackerClient(&prefs_);
  std::unique_ptr<GoogleURLTrackerClient> client(client_);
  google_url_tracker_.reset(new GoogleURLTracker(
      std::move(client), GoogleURLTracker::UNIT_TEST_MODE));
}

void GoogleURLTrackerTest::TearDown() {
  google_url_tracker_->Shutdown();
}

net::TestURLFetcher* GoogleURLTrackerTest::GetFetcher() {
  // This will return the last fetcher created.  If no fetchers have been
  // created, we'll pass GetFetcherByID() "-1", and it will return NULL.
  return fetcher_factory_.GetFetcherByID(google_url_tracker_->fetcher_id_ - 1);
}

void GoogleURLTrackerTest::MockSearchDomainCheckResponse(
    const std::string& domain) {
  net::TestURLFetcher* fetcher = GetFetcher();
  if (!fetcher)
    return;
  fetcher_factory_.RemoveFetcherFromMap(fetcher->id());
  fetcher->set_url(GURL(GoogleURLTracker::kSearchDomainCheckURL));
  fetcher->set_response_code(200);
  fetcher->SetResponseString(domain);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  // At this point, |fetcher| is deleted.
}

void GoogleURLTrackerTest::RequestServerCheck() {
  if (!listener_.HasRegisteredCallback())
    listener_.RegisterCallback(google_url_tracker_.get());
  google_url_tracker_->SetNeedToFetch();
}

void GoogleURLTrackerTest::FinishSleep() {
  google_url_tracker_->FinishSleep();
}

void GoogleURLTrackerTest::NotifyNetworkChanged() {
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  // For thread safety, the NCN queues tasks to do the actual notifications, so
  // we need to spin the message loop so the tracker will actually be notified.
  base::RunLoop().RunUntilIdle();
}

// Tests ----------------------------------------------------------------------

TEST_F(GoogleURLTrackerTest, DontFetchWhenNoOneRequestsCheck) {
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  FinishSleep();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, Update) {
  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());

  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("https://www.google.co.uk/"), google_url());
  EXPECT_TRUE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, DontUpdateWhenUnchanged) {
  GURL original_google_url("https://www.google.co.uk/");
  set_google_url(original_google_url);

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(original_google_url, google_url());
  // No one should be notified, because the new URL matches the old.
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, DontUpdateOnBadReplies) {
  GURL original_google_url("https://www.google.co.uk/");
  set_google_url(original_google_url);

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  // Old-style URL string.
  FinishSleep();
  MockSearchDomainCheckResponse("https://www.google.com/");
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  // Not a Google domain.
  FinishSleep();
  MockSearchDomainCheckResponse(".google.evil.com");
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  // Doesn't start with .google.
  NotifyNetworkChanged();
  MockSearchDomainCheckResponse(".mail.google.com");
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  // Non-empty path.
  NotifyNetworkChanged();
  MockSearchDomainCheckResponse(".google.com/search");
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  // Non-empty query.
  NotifyNetworkChanged();
  MockSearchDomainCheckResponse(".google.com/?q=foo");
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  // Non-empty ref.
  NotifyNetworkChanged();
  MockSearchDomainCheckResponse(".google.com/#anchor");
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());

  // Complete garbage.
  NotifyNetworkChanged();
  MockSearchDomainCheckResponse("HJ)*qF)_*&@f1");
  EXPECT_EQ(original_google_url, google_url());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, RefetchOnNetworkChange) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("https://www.google.co.uk/"), google_url());
  EXPECT_TRUE(listener_notified());
  clear_listener_notified();

  NotifyNetworkChanged();
  MockSearchDomainCheckResponse(".google.co.in");
  EXPECT_EQ(GURL("https://www.google.co.in/"), google_url());
  EXPECT_TRUE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, DontRefetchWhenNoOneRequestsCheck) {
  FinishSleep();
  NotifyNetworkChanged();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, FetchOnLateRequest) {
  FinishSleep();
  NotifyNetworkChanged();
  MockSearchDomainCheckResponse(".google.co.jp");

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("https://www.google.co.uk/"), google_url());
  EXPECT_TRUE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, DontFetchTwiceOnLateRequests) {
  FinishSleep();
  NotifyNetworkChanged();
  MockSearchDomainCheckResponse(".google.co.jp");

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("https://www.google.co.uk/"), google_url());
  EXPECT_TRUE(listener_notified());
  clear_listener_notified();

  RequestServerCheck();
  // The second request should be ignored.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse(".google.co.in");
  EXPECT_EQ(GURL("https://www.google.co.uk/"), google_url());
  EXPECT_FALSE(listener_notified());
}

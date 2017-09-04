// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/downloads/download_notifying_observer.h"

#include "base/memory/ptr_util.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/downloads/offline_page_download_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
static const int64_t kTestOfflineId = 42L;
static const char kTestUrl[] = "http://foo.com/bar";
static const char kTestGuid[] = "cccccccc-cccc-4ccc-0ccc-ccccccccccc1";
static const ClientId kTestClientId(kDownloadNamespace, kTestGuid);
static const base::Time kTestCreationTime = base::Time::Now();
static const bool kTestUserRequested = true;

enum class LastNotificationType {
  NONE,
  DOWNLOAD_SUCCESSFUL,
  DOWNLOAD_FAILED,
  DOWNLOAD_PROGRESS,
  DOWNLOAD_PAUSED,
  DOWNLOAD_INTERRUPTED,
  DOWNLOAD_CANCELED,
};

class TestNotifier : public OfflinePageDownloadNotifier {
 public:
  TestNotifier();
  ~TestNotifier() override;

  // OfflinePageDownloadNotifier implementation:
  void NotifyDownloadSuccessful(const DownloadUIItem& item) override;
  void NotifyDownloadFailed(const DownloadUIItem& item) override;
  void NotifyDownloadProgress(const DownloadUIItem& item) override;
  void NotifyDownloadPaused(const DownloadUIItem& item) override;
  void NotifyDownloadInterrupted(const DownloadUIItem& item) override;
  void NotifyDownloadCanceled(const DownloadUIItem& item) override;

  void Reset();

  LastNotificationType last_notification_type() const {
    return last_notification_type_;
  }

  DownloadUIItem* download_item() const { return download_item_.get(); }

 private:
  LastNotificationType last_notification_type_;
  std::unique_ptr<DownloadUIItem> download_item_;
};

TestNotifier::TestNotifier()
    : last_notification_type_(LastNotificationType::NONE) {}

TestNotifier::~TestNotifier() {}

void TestNotifier::NotifyDownloadSuccessful(const DownloadUIItem& item) {
  last_notification_type_ = LastNotificationType::DOWNLOAD_SUCCESSFUL;
  download_item_.reset(new DownloadUIItem(item));
}

void TestNotifier::NotifyDownloadFailed(const DownloadUIItem& item) {
  last_notification_type_ = LastNotificationType::DOWNLOAD_FAILED;
  download_item_.reset(new DownloadUIItem(item));
}

void TestNotifier::NotifyDownloadProgress(const DownloadUIItem& item) {
  last_notification_type_ = LastNotificationType::DOWNLOAD_PROGRESS;
  download_item_.reset(new DownloadUIItem(item));
}

void TestNotifier::NotifyDownloadPaused(const DownloadUIItem& item) {
  last_notification_type_ = LastNotificationType::DOWNLOAD_PAUSED;
  download_item_.reset(new DownloadUIItem(item));
}

void TestNotifier::NotifyDownloadInterrupted(const DownloadUIItem& item) {
  last_notification_type_ = LastNotificationType::DOWNLOAD_INTERRUPTED;
  download_item_.reset(new DownloadUIItem(item));
}

void TestNotifier::NotifyDownloadCanceled(const DownloadUIItem& item) {
  last_notification_type_ = LastNotificationType::DOWNLOAD_CANCELED;
  download_item_.reset(new DownloadUIItem(item));
}

void TestNotifier::Reset() {
  last_notification_type_ = LastNotificationType::NONE;
  download_item_.reset(nullptr);
}

}  // namespace

class DownloadNotifyingObserverTest : public testing::Test {
 public:
  DownloadNotifyingObserverTest();
  ~DownloadNotifyingObserverTest() override;

  // testing::Test implementation:
  void SetUp() override;

  TestNotifier* notifier() const { return notifier_; }
  DownloadNotifyingObserver* observer() { return observer_.get(); }

 private:
  TestNotifier* notifier_;
  std::unique_ptr<DownloadNotifyingObserver> observer_;
  std::unique_ptr<ClientPolicyController> policy_controller_;
};

DownloadNotifyingObserverTest::DownloadNotifyingObserverTest() {}

DownloadNotifyingObserverTest::~DownloadNotifyingObserverTest() {}

void DownloadNotifyingObserverTest::SetUp() {
  std::unique_ptr<TestNotifier> notifier(new TestNotifier);
  policy_controller_.reset(new ClientPolicyController());
  notifier_ = notifier.get();
  observer_.reset(new DownloadNotifyingObserver(std::move(notifier),
                                                policy_controller_.get()));
}

TEST_F(DownloadNotifyingObserverTest, OnAddedAsAvailable) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  request.set_request_state(SavePageRequest::RequestState::AVAILABLE);
  observer()->OnAdded(request);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_INTERRUPTED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);
}

TEST_F(DownloadNotifyingObserverTest, OnAddedAsOffling) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  request.set_request_state(SavePageRequest::RequestState::OFFLINING);
  observer()->OnAdded(request);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_PROGRESS,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
}

TEST_F(DownloadNotifyingObserverTest, OnAddedAsPaused) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  request.set_request_state(SavePageRequest::RequestState::PAUSED);
  observer()->OnAdded(request);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_PAUSED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
}

TEST_F(DownloadNotifyingObserverTest, OnChangedToPaused) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  request.set_request_state(SavePageRequest::RequestState::PAUSED);
  observer()->OnChanged(request);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_PAUSED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);
}

TEST_F(DownloadNotifyingObserverTest, OnChangedToAvailable) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  request.set_request_state(SavePageRequest::RequestState::AVAILABLE);
  observer()->OnChanged(request);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_INTERRUPTED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);
}

TEST_F(DownloadNotifyingObserverTest, OnChangedToOfflining) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  request.set_request_state(SavePageRequest::RequestState::OFFLINING);
  observer()->OnChanged(request);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_PROGRESS,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);
}

TEST_F(DownloadNotifyingObserverTest, OnCompletedSuccess) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  observer()->OnCompleted(request,
                          RequestNotifier::BackgroundSavePageResult::SUCCESS);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_SUCCESSFUL,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);
}

TEST_F(DownloadNotifyingObserverTest, OnCompletedCanceled) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  observer()->OnCompleted(request,
                          RequestNotifier::BackgroundSavePageResult::REMOVED);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_CANCELED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);
}

TEST_F(DownloadNotifyingObserverTest, OnCompletedFailure) {
  SavePageRequest request(kTestOfflineId, GURL(kTestUrl), kTestClientId,
                          kTestCreationTime, kTestUserRequested);
  observer()->OnCompleted(
      request, RequestNotifier::BackgroundSavePageResult::PRERENDER_FAILURE);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_FAILED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);

  notifier()->Reset();
  observer()->OnCompleted(
      request, RequestNotifier::BackgroundSavePageResult::FOREGROUND_CANCELED);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_FAILED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);

  notifier()->Reset();
  observer()->OnCompleted(
      request, RequestNotifier::BackgroundSavePageResult::SAVE_FAILED);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_FAILED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);

  notifier()->Reset();
  observer()->OnCompleted(request,
                          RequestNotifier::BackgroundSavePageResult::EXPIRED);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_FAILED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);

  notifier()->Reset();
  observer()->OnCompleted(
      request, RequestNotifier::BackgroundSavePageResult::RETRY_COUNT_EXCEEDED);
  EXPECT_EQ(LastNotificationType::DOWNLOAD_FAILED,
            notifier()->last_notification_type());
  EXPECT_EQ(kTestGuid, notifier()->download_item()->guid);
  EXPECT_EQ(GURL(kTestUrl), notifier()->download_item()->url);
  EXPECT_EQ(kTestCreationTime, notifier()->download_item()->start_time);
}

TEST_F(DownloadNotifyingObserverTest, NamespacesNotVisibleInUI) {
  std::vector<std::string> name_spaces = {kBookmarkNamespace, kLastNNamespace,
                                          kCCTNamespace, kDefaultNamespace};

  for (auto name_space : name_spaces) {
    ClientId invisible_client_id(name_space, kTestGuid);
    SavePageRequest request(kTestOfflineId, GURL(kTestUrl), invisible_client_id,
                            kTestCreationTime, kTestUserRequested);
    observer()->OnAdded(request);
    EXPECT_EQ(LastNotificationType::NONE, notifier()->last_notification_type());
  }
}

}  // namespace offline_pages

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/change_requests_state_task.h"

#include <memory>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const int64_t kRequestId1 = 42;
const int64_t kRequestId2 = 43;
const int64_t kRequestId3 = 44;
const GURL kUrl1("http://example.com");
const GURL kUrl2("http://another-example.com");
const ClientId kClientId1("bookmark", "1234");
const ClientId kClientId2("async", "5678");
}  // namespace

class ChangeRequestsStateTaskTest : public testing::Test {
 public:
  ChangeRequestsStateTaskTest();
  ~ChangeRequestsStateTaskTest() override;

  void PumpLoop();

  void SetUpStore(RequestQueueStore* store);

  void AddRequestDone(ItemActionStatus status);

  void ChangeRequestsStateCallback(
      std::unique_ptr<UpdateRequestsResult> result);

  UpdateRequestsResult* last_result() const { return result_.get(); }

 private:
  std::unique_ptr<UpdateRequestsResult> result_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

ChangeRequestsStateTaskTest::ChangeRequestsStateTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

ChangeRequestsStateTaskTest::~ChangeRequestsStateTaskTest() {}

void ChangeRequestsStateTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void ChangeRequestsStateTaskTest::SetUpStore(RequestQueueStore* store) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request_1(kRequestId1, kUrl1, kClientId1, creation_time,
                            true);
  store->AddRequest(request_1,
                    base::Bind(&ChangeRequestsStateTaskTest::AddRequestDone,
                               base::Unretained(this)));
  SavePageRequest request_2(kRequestId2, kUrl2, kClientId2, creation_time,
                            true);
  store->AddRequest(request_2,
                    base::Bind(&ChangeRequestsStateTaskTest::AddRequestDone,
                               base::Unretained(this)));
  PumpLoop();
}

void ChangeRequestsStateTaskTest::AddRequestDone(ItemActionStatus status) {
  ASSERT_EQ(ItemActionStatus::SUCCESS, status);
}

void ChangeRequestsStateTaskTest::ChangeRequestsStateCallback(
    std::unique_ptr<UpdateRequestsResult> result) {
  result_ = std::move(result);
}

TEST_F(ChangeRequestsStateTaskTest, UpdateWhenStoreEmpty) {
  RequestQueueInMemoryStore store;
  std::vector<int64_t> request_ids{kRequestId1};
  ChangeRequestsStateTask task(
      &store, request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                 base::Unretained(this)));
  task.Run();
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(0UL, last_result()->updated_items.size());
}

TEST_F(ChangeRequestsStateTaskTest, UpdateSingleItem) {
  RequestQueueInMemoryStore store;
  SetUpStore(&store);
  std::vector<int64_t> request_ids{kRequestId1};
  ChangeRequestsStateTask task(
      &store, request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                 base::Unretained(this)));
  task.Run();
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(1UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(0).request_state());
}

TEST_F(ChangeRequestsStateTaskTest, UpdateMultipleItems) {
  RequestQueueInMemoryStore store;
  SetUpStore(&store);
  std::vector<int64_t> request_ids{kRequestId1, kRequestId2};
  ChangeRequestsStateTask task(
      &store, request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                 base::Unretained(this)));
  task.Run();
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(2UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(kRequestId2, last_result()->item_statuses.at(1).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(1).second);
  EXPECT_EQ(2UL, last_result()->updated_items.size());
  EXPECT_EQ(kRequestId1, last_result()->updated_items.at(0).request_id());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(0).request_state());
  EXPECT_EQ(kRequestId2, last_result()->updated_items.at(1).request_id());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(1).request_state());
}

TEST_F(ChangeRequestsStateTaskTest, EmptyRequestsList) {
  RequestQueueInMemoryStore store;
  std::vector<int64_t> request_ids;
  ChangeRequestsStateTask task(
      &store, request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                 base::Unretained(this)));
  task.Run();
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(0UL, last_result()->item_statuses.size());
  EXPECT_EQ(0UL, last_result()->updated_items.size());
}

TEST_F(ChangeRequestsStateTaskTest, UpdateMissingItem) {
  RequestQueueInMemoryStore store;
  SetUpStore(&store);
  std::vector<int64_t> request_ids{kRequestId1, kRequestId3};
  ChangeRequestsStateTask task(
      &store, request_ids, SavePageRequest::RequestState::PAUSED,
      base::Bind(&ChangeRequestsStateTaskTest::ChangeRequestsStateCallback,
                 base::Unretained(this)));
  task.Run();
  PumpLoop();
  ASSERT_TRUE(last_result());
  EXPECT_EQ(2UL, last_result()->item_statuses.size());
  EXPECT_EQ(kRequestId1, last_result()->item_statuses.at(0).first);
  EXPECT_EQ(ItemActionStatus::SUCCESS,
            last_result()->item_statuses.at(0).second);
  EXPECT_EQ(kRequestId3, last_result()->item_statuses.at(1).first);
  EXPECT_EQ(ItemActionStatus::NOT_FOUND,
            last_result()->item_statuses.at(1).second);
  EXPECT_EQ(1UL, last_result()->updated_items.size());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED,
            last_result()->updated_items.at(0).request_state());
}

}  // namespace offline_pages

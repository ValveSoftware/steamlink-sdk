// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include <memory>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using AddRequestResult = RequestQueue::AddRequestResult;
using GetRequestsResult = RequestQueue::GetRequestsResult;
using UpdateRequestResult = RequestQueue::UpdateRequestResult;

namespace {
// Data for request 1.
const int64_t kRequestId = 42;
const GURL kUrl("http://example.com");
const ClientId kClientId("bookmark", "1234");
// Data for request 2.
const int64_t kRequestId2 = 77;
const GURL kUrl2("http://test.com");
const ClientId kClientId2("bookmark", "567");
}  // namespace

// TODO(fgorski): Add tests for store failures in add/remove/get.
class RequestQueueTest : public testing::Test {
 public:
  RequestQueueTest();
  ~RequestQueueTest() override;

  // Test overrides.
  void SetUp() override;

  void PumpLoop();

  // Callback for adding requests.
  void AddRequestDone(AddRequestResult result, const SavePageRequest& request);
  // Callback for getting requests.
  void GetRequestsDone(GetRequestsResult result,
                       const std::vector<SavePageRequest>& requests);
  // Callback for removing request.
  void RemoveRequestDone(UpdateRequestResult result);

  RequestQueue* queue() { return queue_.get(); }

  AddRequestResult last_add_result() const { return last_add_result_; }
  SavePageRequest* last_added_request() {
    return last_added_request_.get();
  }

  UpdateRequestResult last_remove_result() const { return last_remove_result_; }

  GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }
  const std::vector<SavePageRequest>& last_requests() const {
    return last_requests_;
  }

 private:
  AddRequestResult last_add_result_;
  std::unique_ptr<SavePageRequest> last_added_request_;

  UpdateRequestResult last_remove_result_;

  GetRequestsResult last_get_requests_result_;
  std::vector<SavePageRequest> last_requests_;

  std::unique_ptr<RequestQueue> queue_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestQueueTest::RequestQueueTest()
    : last_add_result_(AddRequestResult::STORE_FAILURE),
      last_remove_result_(UpdateRequestResult::STORE_FAILURE),
      last_get_requests_result_(GetRequestsResult::STORE_FAILURE),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

RequestQueueTest::~RequestQueueTest() {}

void RequestQueueTest::SetUp() {
  std::unique_ptr<RequestQueueInMemoryStore> store(
      new RequestQueueInMemoryStore());
  queue_.reset(new RequestQueue(std::move(store)));
}

void RequestQueueTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestQueueTest::AddRequestDone(AddRequestResult result,
                                      const SavePageRequest& request) {
  last_add_result_ = result;
  last_added_request_.reset(new SavePageRequest(request));
}

void RequestQueueTest::GetRequestsDone(
    GetRequestsResult result,
    const std::vector<SavePageRequest>& requests) {
  last_get_requests_result_ = result;
  last_requests_ = requests;
}

void RequestQueueTest::RemoveRequestDone(UpdateRequestResult result) {
  last_remove_result_ = result;
}

TEST_F(RequestQueueTest, GetRequestsEmpty) {
  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

TEST_F(RequestQueueTest, AddRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(AddRequestResult::SUCCESS, last_add_result());
  ASSERT_TRUE(last_added_request());
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
}

TEST_F(RequestQueueTest, RemoveRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request(kRequestId, kUrl, kClientId, creation_time);
  queue()->AddRequest(request, base::Bind(&RequestQueueTest::AddRequestDone,
                                          base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(kRequestId, last_added_request()->request_id());

  queue()->RemoveRequest(
      kRequestId,
      base::Bind(&RequestQueueTest::RemoveRequestDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(UpdateRequestResult::SUCCESS, last_remove_result());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(0ul, last_requests().size());
}

// A longer test populating the request queue with more than one item, properly
// listing multiple items and removing the right item.
TEST_F(RequestQueueTest, MultipleRequestsAddGetRemove) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId, kUrl, kClientId, creation_time);
  queue()->AddRequest(request1, base::Bind(&RequestQueueTest::AddRequestDone,
                                           base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(request1.request_id(), last_added_request()->request_id());
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time);
  queue()->AddRequest(request2, base::Bind(&RequestQueueTest::AddRequestDone,
                                           base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(request2.request_id(), last_added_request()->request_id());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(2ul, last_requests().size());

  queue()->RemoveRequest(
      request1.request_id(),
      base::Bind(&RequestQueueTest::RemoveRequestDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(UpdateRequestResult::SUCCESS, last_remove_result());

  queue()->GetRequests(
      base::Bind(&RequestQueueTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(GetRequestsResult::SUCCESS, last_get_requests_result());
  ASSERT_EQ(1ul, last_requests().size());
  ASSERT_EQ(request2.request_id(), last_requests()[0].request_id());
}

}  // namespace offline_pages

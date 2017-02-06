// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_picker.h"

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// Data for request 1.
const int64_t kRequestId1 = 17;
const GURL kUrl1("https://google.com");
const ClientId kClientId1("bookmark", "1234");
// Data for request 2.
const int64_t kRequestId2 = 42;
const GURL kUrl2("http://nytimes.com");
const ClientId kClientId2("bookmark", "5678");
}  // namespace

class RequestPickerTest : public testing::Test {
 public:
  RequestPickerTest();

  ~RequestPickerTest() override;

  void SetUp() override;

  void PumpLoop();

  void AddRequestDone(RequestQueue::AddRequestResult result,
                      const SavePageRequest& request);

  void RequestPicked(const SavePageRequest& request);

  void RequestQueueEmpty();

 protected:
  // The request queue is simple enough we will use a real queue with a memory
  // store instead of a stub.
  std::unique_ptr<RequestQueue> queue_;
  std::unique_ptr<RequestPicker> picker_;
  std::unique_ptr<SavePageRequest> last_picked_;
  bool request_queue_empty_called_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RequestPickerTest::RequestPickerTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

RequestPickerTest::~RequestPickerTest() {}

void RequestPickerTest::SetUp() {
  std::unique_ptr<RequestQueueInMemoryStore> store(
      new RequestQueueInMemoryStore());
  queue_.reset(new RequestQueue(std::move(store)));
  picker_.reset(new RequestPicker(queue_.get()));
  request_queue_empty_called_ = false;
}

void RequestPickerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestPickerTest::AddRequestDone(RequestQueue::AddRequestResult result,
                                       const SavePageRequest& request) {}

void RequestPickerTest::RequestPicked(const SavePageRequest& request) {
  last_picked_.reset(new SavePageRequest(request));
}

void RequestPickerTest::RequestQueueEmpty() {
  request_queue_empty_called_ = true;
}

TEST_F(RequestPickerTest, ChooseNextRequest) {
  base::Time creation_time = base::Time::Now();
  SavePageRequest request1(kRequestId1, kUrl1, kClientId1, creation_time);
  SavePageRequest request2(kRequestId2, kUrl2, kClientId2, creation_time);
  // Put some test requests on the Queue.
  queue_->AddRequest(request1, base::Bind(&RequestPickerTest::AddRequestDone,
                                          base::Unretained(this)));
  queue_->AddRequest(request2, base::Bind(&RequestPickerTest::AddRequestDone,
                                          base::Unretained(this)));

  // Pump the loop to give the async queue the opportunity to do the adds.
  PumpLoop();

  picker_->ChooseNextRequest(
      base::Bind(&RequestPickerTest::RequestPicked, base::Unretained(this)),
      base::Bind(&RequestPickerTest::RequestQueueEmpty,
                 base::Unretained(this)));

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "picked"
  // callback.
  PumpLoop();

  EXPECT_EQ(kRequestId1, last_picked_->request_id());
  EXPECT_FALSE(request_queue_empty_called_);
}

TEST_F(RequestPickerTest, PickFromEmptyQueue) {
  picker_->ChooseNextRequest(
      base::Bind(&RequestPickerTest::RequestPicked, base::Unretained(this)),
      base::Bind(&RequestPickerTest::RequestQueueEmpty,
                 base::Unretained(this)));

  // Pump the loop again to give the async queue the opportunity to return
  // results from the Get operation, and for the picker to call the "QueueEmpty"
  // callback.
  PumpLoop();

  EXPECT_TRUE(request_queue_empty_called_);
}

}  // namespace offline_pages

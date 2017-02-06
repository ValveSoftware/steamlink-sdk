// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/background/scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// put test constants here
const GURL kUrl("http://universe.com/everything");
const ClientId kClientId("bookmark", "42");
const int kRequestId(1);
}  // namespace

class SchedulerStub : public Scheduler {
 public:
  SchedulerStub()
      : schedule_called_(false),
        unschedule_called_(false),
        conditions_(false, 0, false) {}

  void Schedule(const TriggerConditions& trigger_conditions) override {
    schedule_called_ = true;
    conditions_ = trigger_conditions;
  }

  // Unschedules the currently scheduled task, if any.
  void Unschedule() override {
    unschedule_called_ = true;
  }

  bool schedule_called() const { return schedule_called_; }

  bool unschedule_called() const { return unschedule_called_; }

  TriggerConditions const* conditions() const { return &conditions_; }

 private:
  bool schedule_called_;
  bool unschedule_called_;
  TriggerConditions conditions_;
};

class OfflinerStub : public Offliner {
 public:
  OfflinerStub() : request_(kRequestId, kUrl, kClientId, base::Time::Now()),
                   enable_callback_(false) {}

  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& callback) override {
    callback_ = callback;
    request_ = request;
    // Post the callback on the run loop.
    if (enable_callback_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(callback, request, Offliner::RequestStatus::SAVED));
    }
    return true;
  }

  // Clears the currently processing request, if any.  Must have called
  // LoadAndSave first to set the callback and request.
  // Clears the currently processing request, if any.
  void Cancel() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback_, request_, Offliner::RequestStatus::CANCELED));
  }

  void enable_callback(bool enable) {
    enable_callback_ = enable;
  }

 private:
  CompletionCallback callback_;
  SavePageRequest request_;
  bool enable_callback_;
};

class OfflinerFactoryStub : public OfflinerFactory {
 public:

  OfflinerFactoryStub() : offliner_(nullptr) {}

  Offliner* GetOffliner(const OfflinerPolicy* policy) override {
    if (offliner_.get() == nullptr) {
      offliner_.reset(new OfflinerStub());
    }
    return offliner_.get();
  }

 private:
  std::unique_ptr<OfflinerStub> offliner_;
};

class RequestCoordinatorTest
    : public testing::Test {
 public:
  RequestCoordinatorTest();
  ~RequestCoordinatorTest() override;

  void SetUp() override;

  void PumpLoop();

  RequestCoordinator* coordinator() {
    return coordinator_.get();
  }

  bool is_busy() {
    return coordinator_->is_busy();
  }

  void SendRequestToOffliner(SavePageRequest& request) {
    coordinator_->SendRequestToOffliner(request);
  }

  // Empty callback function
  void EmptyCallbackFunction(bool result) {
  }

  // Callback for Add requests
  void AddRequestDone(RequestQueue::AddRequestResult result,
                      const SavePageRequest& request);

  // Callback for getting requests.
  void GetRequestsDone(RequestQueue::GetRequestsResult result,
                       const std::vector<SavePageRequest>& requests);

  void SendOfflinerDoneCallback(const SavePageRequest& request,
                                Offliner::RequestStatus status);

  RequestQueue::GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }

  const std::vector<SavePageRequest>& last_requests() const {
    return last_requests_;
  }

  void EnableOfflinerCallback(bool enable) {
    offliner_->enable_callback(enable);
  }

  Offliner::RequestStatus last_offlining_status() const {
    return coordinator_->last_offlining_status_;
  }

 private:
  RequestQueue::GetRequestsResult last_get_requests_result_;
  std::vector<SavePageRequest> last_requests_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<RequestCoordinator> coordinator_;
  OfflinerStub* offliner_;
};

RequestCoordinatorTest::RequestCoordinatorTest()
    : last_get_requests_result_(RequestQueue::GetRequestsResult::STORE_FAILURE),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      offliner_(nullptr) {}

RequestCoordinatorTest::~RequestCoordinatorTest() {}

void RequestCoordinatorTest::SetUp() {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<OfflinerFactory> factory(new OfflinerFactoryStub());
  // Save the offliner for use by the tests.
  offliner_ =
      reinterpret_cast<OfflinerStub*>(factory->GetOffliner(policy.get()));
  std::unique_ptr<RequestQueueInMemoryStore>
      store(new RequestQueueInMemoryStore());
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(store)));
  std::unique_ptr<Scheduler> scheduler_stub(new SchedulerStub());
  coordinator_.reset(new RequestCoordinator(
      std::move(policy), std::move(factory), std::move(queue),
      std::move(scheduler_stub)));
}

void RequestCoordinatorTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestCoordinatorTest::GetRequestsDone(
    RequestQueue::GetRequestsResult result,
    const std::vector<SavePageRequest>& requests) {
  last_get_requests_result_ = result;
  last_requests_ = requests;
}

void RequestCoordinatorTest::AddRequestDone(
    RequestQueue::AddRequestResult result,
    const SavePageRequest& request) {}

void RequestCoordinatorTest::SendOfflinerDoneCallback(
    const SavePageRequest& request, Offliner::RequestStatus status) {
  // Using the fact that the test class is a friend, call to the callback
  coordinator_->OfflinerDoneCallback(request, status);
}

TEST_F(RequestCoordinatorTest, StartProcessingWithNoRequests) {
  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
}

TEST_F(RequestCoordinatorTest, StartProcessingWithRequestInProgress) {
  // Put the request on the queue.
  EXPECT_TRUE(coordinator()->SavePageLater(kUrl, kClientId));

  // Set up for the call to StartProcessing by building arguments.
  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(&RequestCoordinatorTest::EmptyCallbackFunction,
                 base::Unretained(this));

  // Ensure that the forthcoming request does not finish - we simulate it being
  // in progress by asking it to skip making the completion callback.
  EnableOfflinerCallback(false);

  // Sending the request to the offliner should make it busy.
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));
  PumpLoop();
  EXPECT_TRUE(is_busy());

  // Now trying to start processing on another request should return false.
  EXPECT_FALSE(coordinator()->StartProcessing(device_conditions, callback));
}

TEST_F(RequestCoordinatorTest, SavePageLater) {
  EXPECT_TRUE(coordinator()->SavePageLater(kUrl, kClientId));

  // Expect that a request got placed on the queue.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));

  // Wait for callbacks to finish, both request queue and offliner.
  PumpLoop();

  // Check the request queue is as expected.
  EXPECT_EQ(1UL, last_requests().size());
  EXPECT_EQ(kUrl, last_requests()[0].url());
  EXPECT_EQ(kClientId, last_requests()[0].client_id());

  // Expect that the scheduler got notified.
  SchedulerStub* scheduler_stub = reinterpret_cast<SchedulerStub*>(
      coordinator()->scheduler());
  EXPECT_TRUE(scheduler_stub->schedule_called());
  EXPECT_EQ(coordinator()
                ->GetTriggerConditionsForUserRequest()
                .minimum_battery_percentage,
            scheduler_stub->conditions()->minimum_battery_percentage);
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestSucceeded) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId, kUrl, kClientId, base::Time::Now());
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // We need to give a callback to the request.
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  coordinator()->SetProcessingCallbackForTest(callback);

  // Call the OfflinerDoneCallback to simulate the page being completed, wait
  // for callbacks.
  EnableOfflinerCallback(true);
  SendOfflinerDoneCallback(request, Offliner::RequestStatus::SAVED);
  PumpLoop();

  // Verify the request gets removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // We should not find any requests in the queue anymore.
  // RequestPicker should *not* have tried to start an additional job,
  // because the request queue is empty now.
  EXPECT_EQ(0UL, last_requests().size());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestFailed) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId, kUrl, kClientId, base::Time::Now());
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // We need to give a callback to the request.
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  coordinator()->SetProcessingCallbackForTest(callback);

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  EnableOfflinerCallback(true);
  SendOfflinerDoneCallback(request, Offliner::RequestStatus::FAILED);
  PumpLoop();

  // Verify the request is not removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // Still one request in the queue.
  EXPECT_EQ(1UL, last_requests().size());
  // TODO(dougarnett): Verify retry count gets incremented.
}

// This tests a StopProcessing call before we have actually started the
// prerenderer.
TEST_F(RequestCoordinatorTest, StartProcessingThenStopProcessingImmediately) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId, kUrl, kClientId, base::Time::Now());
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));

  // Now, quick, before it can do much (we haven't called PumpLoop), cancel it.
  coordinator()->StopProcessing();

  // Let the async callbacks in the request coordinator run.
  PumpLoop();

  // OfflinerDoneCallback will not end up getting called with status SAVED,
  // Since we cancelled the event before it called offliner_->LoadAndSave().
  EXPECT_NE(Offliner::RequestStatus::SAVED, last_offlining_status());
}

// This tests a StopProcessing call after the prerenderer has been started.
TEST_F(RequestCoordinatorTest, StartProcessingThenStopProcessingLater) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId, kUrl, kClientId, base::Time::Now());
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // Ensure the start processing request stops before the completion callback.
  EnableOfflinerCallback(false);

  DeviceConditions device_conditions(false, 75,
                                     net::NetworkChangeNotifier::CONNECTION_3G);
  base::Callback<void(bool)> callback =
      base::Bind(
          &RequestCoordinatorTest::EmptyCallbackFunction,
          base::Unretained(this));
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions, callback));

  // Let all the async parts of the start processing pipeline run to completion.
  PumpLoop();

  // Now we cancel it while the prerenderer is busy.
  coordinator()->StopProcessing();

  // Let the async callbacks in the cancel run.
  PumpLoop();

  // OfflinerDoneCallback will not end up getting called with status SAVED,
  // Since we cancelled the event before it called offliner_->LoadAndSave().
  EXPECT_EQ(Offliner::RequestStatus::CANCELED, last_offlining_status());
}

}  // namespace offline_pages

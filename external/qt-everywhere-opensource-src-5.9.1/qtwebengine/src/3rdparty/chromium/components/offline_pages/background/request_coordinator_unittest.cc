// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_coordinator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/offliner.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/pick_request_task_factory.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/background/scheduler.h"
#include "components/offline_pages/offline_page_feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// put test constants here
const GURL kUrl1("http://universe.com/everything");
const GURL kUrl2("http://universe.com/toinfinityandbeyond");
const std::string kClientNamespace("bookmark");
const std::string kId1("42");
const std::string kId2("life*universe+everything");
const ClientId kClientId1(kClientNamespace, kId1);
const ClientId kClientId2(kClientNamespace, kId2);
const int kRequestId1(1);
const int kRequestId2(2);
const long kTestTimeBudgetSeconds = 200;
const int kBatteryPercentageHigh = 75;
const int kMaxCompletedTries = 3;
const bool kPowerRequired = true;
const bool kUserRequested = true;
const int kAttemptCount = 1;
}  // namespace

class SchedulerStub : public Scheduler {
 public:
  SchedulerStub()
      : schedule_called_(false),
        backup_schedule_called_(false),
        unschedule_called_(false),
        schedule_delay_(0L),
        conditions_(false, 0, false) {}

  void Schedule(const TriggerConditions& trigger_conditions) override {
    schedule_called_ = true;
    conditions_ = trigger_conditions;
  }

  void BackupSchedule(const TriggerConditions& trigger_conditions,
                      long delay_in_seconds) override {
    backup_schedule_called_ = true;
    schedule_delay_ = delay_in_seconds;
    conditions_ = trigger_conditions;
  }

  // Unschedules the currently scheduled task, if any.
  void Unschedule() override {
    unschedule_called_ = true;
  }

  bool schedule_called() const { return schedule_called_; }

  bool backup_schedule_called() const { return backup_schedule_called_;}

  bool unschedule_called() const { return unschedule_called_; }

  TriggerConditions const* conditions() const { return &conditions_; }

 private:
  bool schedule_called_;
  bool backup_schedule_called_;
  bool unschedule_called_;
  long schedule_delay_;
  TriggerConditions conditions_;
};

class OfflinerStub : public Offliner {
 public:
  OfflinerStub()
      : request_(kRequestId1, kUrl1, kClientId1, base::Time::Now(),
                 kUserRequested),
        disable_loading_(false),
        enable_callback_(false),
        cancel_called_(false) {}

  bool LoadAndSave(const SavePageRequest& request,
                   const CompletionCallback& callback) override {
    if (disable_loading_)
      return false;

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

  void Cancel() override { cancel_called_ = true; }

  void disable_loading() {
    disable_loading_ = true;
  }

  void enable_callback(bool enable) {
    enable_callback_ = enable;
  }

  bool cancel_called() { return cancel_called_; }

 private:
  CompletionCallback callback_;
  SavePageRequest request_;
  bool disable_loading_;
  bool enable_callback_;
  bool cancel_called_;
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

class NetworkQualityEstimatorStub
    : public net::NetworkQualityEstimator::NetworkQualityProvider {
 public:
  NetworkQualityEstimatorStub()
      : connection_type_(
            net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G) {}

  net::EffectiveConnectionType GetEffectiveConnectionType() const override {
    return connection_type_;
  }

  void AddEffectiveConnectionTypeObserver(
      net::NetworkQualityEstimator::EffectiveConnectionTypeObserver* observer)
      override {}

  void RemoveEffectiveConnectionTypeObserver(
      net::NetworkQualityEstimator::EffectiveConnectionTypeObserver* observer)
      override {}

  void SetEffectiveConnectionTypeForTest(net::EffectiveConnectionType type) {
    connection_type_ = type;
  }

 private:
  net::EffectiveConnectionType connection_type_;
};

class ObserverStub : public RequestCoordinator::Observer {
 public:
  ObserverStub()
      : added_called_(false),
        completed_called_(false),
        changed_called_(false),
        last_status_(RequestCoordinator::BackgroundSavePageResult::SUCCESS),
        state_(SavePageRequest::RequestState::OFFLINING) {}

  void Clear() {
    added_called_ = false;
    completed_called_ = false;
    changed_called_ = false;
    state_ = SavePageRequest::RequestState::OFFLINING;
    last_status_ = RequestCoordinator::BackgroundSavePageResult::SUCCESS;
  }

  void OnAdded(const SavePageRequest& request) override {
    added_called_ = true;
  }

  void OnCompleted(
      const SavePageRequest& request,
      RequestCoordinator::BackgroundSavePageResult status) override {
    completed_called_ = true;
    last_status_ = status;
  }

  void OnChanged(const SavePageRequest& request) override {
    changed_called_ = true;
    state_ = request.request_state();
  }

  bool added_called() { return added_called_; }
  bool completed_called() { return completed_called_; }
  bool changed_called() { return changed_called_; }
  RequestCoordinator::BackgroundSavePageResult last_status() {
    return last_status_;
  }
  SavePageRequest::RequestState state() { return state_; }

 private:
  bool added_called_;
  bool completed_called_;
  bool changed_called_;
  RequestCoordinator::BackgroundSavePageResult last_status_;
  SavePageRequest::RequestState state_;
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

  bool is_starting() { return coordinator_->is_starting(); }

  // Empty callback function.
  void ImmediateScheduleCallbackFunction(bool result) {
    immediate_schedule_callback_called_ = true;
    immediate_schedule_callback_result_ = result;
  }

  // Callback function which releases a wait for it.
  void WaitingCallbackFunction(bool result) {
    waiter_.Signal();
  }

  net::NetworkChangeNotifier::ConnectionType GetConnectionType() {
    return coordinator()->GetConnectionType();
  }

  // Callback for Add requests.
  void AddRequestDone(AddRequestResult result, const SavePageRequest& request);

  // Callback for getting requests.
  void GetRequestsDone(GetRequestsResult result,
                       std::vector<std::unique_ptr<SavePageRequest>> requests);

  // Callback for removing requests.
  void RemoveRequestsDone(const MultipleItemStatuses& results);

  // Callback for getting request statuses.
  void GetQueuedRequestsDone(
      std::vector<std::unique_ptr<SavePageRequest>> requests);

  void SetupForOfflinerDoneCallbackTest(
      offline_pages::SavePageRequest* request);

  void SendOfflinerDoneCallback(const SavePageRequest& request,
                                Offliner::RequestStatus status);

  GetRequestsResult last_get_requests_result() const {
    return last_get_requests_result_;
  }

  const std::vector<std::unique_ptr<SavePageRequest>>& last_requests() const {
    return last_requests_;
  }

  const MultipleItemStatuses& last_remove_results() const {
    return last_remove_results_;
  }

  void DisableLoading() {
    offliner_->disable_loading();
  }

  void EnableOfflinerCallback(bool enable) {
    offliner_->enable_callback(enable);
  }

  void SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType connection) {
    coordinator()->SetNetworkConditionsForTest(connection);
  }

  void SetEffectiveConnectionTypeForTest(net::EffectiveConnectionType type) {
    network_quality_estimator_->SetEffectiveConnectionTypeForTest(type);
  }

  void SetNetworkConnected(bool connected) {
    if (connected) {
      SetNetworkConditionsForTest(
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
      SetEffectiveConnectionTypeForTest(
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G);
    } else {
      SetNetworkConditionsForTest(
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);
      SetEffectiveConnectionTypeForTest(
          net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    }
  }

  void CallConnectionTypeObserver() {
    if (coordinator()->connection_notifier_) {
      coordinator()->connection_notifier_->OnConnectionTypeChanged(
          GetConnectionType());
    }
  }

  void SetIsLowEndDeviceForTest(bool is_low_end_device) {
    coordinator()->is_low_end_device_ = is_low_end_device;
  }

  void SetProcessingStateForTest(
      RequestCoordinator::ProcessingWindowState processing_state) {
    coordinator()->processing_state_ = processing_state;
  }

  void SetOperationStartTimeForTest(base::Time start_time) {
    coordinator()->operation_start_time_ = start_time;
  }

  void ScheduleForTest() { coordinator_->ScheduleAsNeeded(); }

  void CallRequestNotPicked(bool non_user_requested_tasks_remaining,
                            bool disabled_tasks_remaining) {
    if (disabled_tasks_remaining)
      coordinator_->disabled_requests_.insert(kRequestId1);
    else
      coordinator_->disabled_requests_.clear();

    coordinator_->RequestNotPicked(non_user_requested_tasks_remaining);
  }

  void SetDeviceConditionsForTest(DeviceConditions device_conditions) {
    coordinator_->SetDeviceConditionsForTest(device_conditions);
  }

  void WaitForCallback() {
    waiter_.Wait();
  }

  void AdvanceClockBy(base::TimeDelta delta) {
    task_runner_->FastForwardBy(delta);
  }

  SavePageRequest AddRequest1();

  SavePageRequest AddRequest2();

  Offliner::RequestStatus last_offlining_status() const {
    return coordinator_->last_offlining_status_;
  }

  bool OfflinerWasCanceled() const { return offliner_->cancel_called(); }

  ObserverStub observer() { return observer_; }

  DeviceConditions device_conditions() { return device_conditions_; }

  base::Callback<void(bool)> immediate_callback() {
    return immediate_callback_;
  }

  base::Callback<void(bool)> waiting_callback() { return waiting_callback_; }
  bool immediate_schedule_callback_called() const {
    return immediate_schedule_callback_called_;
  }

  bool immediate_schedule_callback_result() const {
    return immediate_schedule_callback_result_;
  }

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 private:
  GetRequestsResult last_get_requests_result_;
  MultipleItemStatuses last_remove_results_;
  std::vector<std::unique_ptr<SavePageRequest>> last_requests_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<NetworkQualityEstimatorStub> network_quality_estimator_;
  std::unique_ptr<RequestCoordinator> coordinator_;
  OfflinerStub* offliner_;
  base::WaitableEvent waiter_;
  ObserverStub observer_;
  bool immediate_schedule_callback_called_;
  bool immediate_schedule_callback_result_;
  DeviceConditions device_conditions_;
  base::Callback<void(bool)> immediate_callback_;
  base::Callback<void(bool)> waiting_callback_;
  base::HistogramTester histogram_tester_;
};

RequestCoordinatorTest::RequestCoordinatorTest()
    : last_get_requests_result_(GetRequestsResult::STORE_FAILURE),
      task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_),
      offliner_(nullptr),
      waiter_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED),
      immediate_schedule_callback_called_(false),
      immediate_schedule_callback_result_(false),
      device_conditions_(!kPowerRequired,
                         kBatteryPercentageHigh,
                         net::NetworkChangeNotifier::CONNECTION_3G) {}

RequestCoordinatorTest::~RequestCoordinatorTest() {}

void RequestCoordinatorTest::SetUp() {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<OfflinerFactory> offliner_factory(new OfflinerFactoryStub());
  // Save the offliner for use by the tests.
  offliner_ = reinterpret_cast<OfflinerStub*>(
      offliner_factory->GetOffliner(policy.get()));
  std::unique_ptr<RequestQueueInMemoryStore>
      store(new RequestQueueInMemoryStore());
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(store)));
  std::unique_ptr<Scheduler> scheduler_stub(new SchedulerStub());
  network_quality_estimator_.reset(new NetworkQualityEstimatorStub());
  coordinator_.reset(new RequestCoordinator(
      std::move(policy), std::move(offliner_factory), std::move(queue),
      std::move(scheduler_stub), network_quality_estimator_.get()));
  coordinator_->AddObserver(&observer_);
  SetNetworkConnected(true);
  std::unique_ptr<PickRequestTaskFactory> picker_factory(
      new PickRequestTaskFactory(
          coordinator_->policy(),
          static_cast<RequestNotifier*>(coordinator_.get()),
          coordinator_->GetLogger()));
  coordinator_->queue()->SetPickerFactory(std::move(picker_factory));
  immediate_callback_ =
      base::Bind(&RequestCoordinatorTest::ImmediateScheduleCallbackFunction,
                 base::Unretained(this));
  // Override the normal immediate callback with a wait releasing callback.
  waiting_callback_ = base::Bind(
      &RequestCoordinatorTest::WaitingCallbackFunction, base::Unretained(this));
  SetDeviceConditionsForTest(device_conditions_);
  EnableOfflinerCallback(true);
}

void RequestCoordinatorTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void RequestCoordinatorTest::GetRequestsDone(
    GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  last_get_requests_result_ = result;
  last_requests_ = std::move(requests);
}

void RequestCoordinatorTest::RemoveRequestsDone(
    const MultipleItemStatuses& results) {
  last_remove_results_ = results;
  waiter_.Signal();
}

void RequestCoordinatorTest::GetQueuedRequestsDone(
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  last_requests_ = std::move(requests);
  waiter_.Signal();
}

void RequestCoordinatorTest::AddRequestDone(AddRequestResult result,
                                            const SavePageRequest& request) {}

void RequestCoordinatorTest::SetupForOfflinerDoneCallbackTest(
    offline_pages::SavePageRequest* request) {
  // Mark request as started and add it to the queue,
  // then wait for callback to finish.
  request->MarkAttemptStarted(base::Time::Now());
  coordinator()->queue()->AddRequest(
      *request, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();

  // Override the processing callback for test visiblity.
  base::Callback<void(bool)> callback =
      base::Bind(&RequestCoordinatorTest::ImmediateScheduleCallbackFunction,
                 base::Unretained(this));
  coordinator()->SetProcessingCallbackForTest(callback);

  // Mock that coordinator is in actively processing state starting now.
  SetProcessingStateForTest(
      RequestCoordinator::ProcessingWindowState::IMMEDIATE_WINDOW);
  SetOperationStartTimeForTest(base::Time::Now());
}

void RequestCoordinatorTest::SendOfflinerDoneCallback(
    const SavePageRequest& request, Offliner::RequestStatus status) {
  // Using the fact that the test class is a friend, call to the callback
  coordinator_->OfflinerDoneCallback(request, status);
}

SavePageRequest RequestCoordinatorTest::AddRequest1() {
  offline_pages::SavePageRequest request1(kRequestId1, kUrl1, kClientId1,
                                          base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request1, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  return request1;
}

SavePageRequest RequestCoordinatorTest::AddRequest2() {
  offline_pages::SavePageRequest request2(kRequestId2, kUrl2, kClientId2,
                                          base::Time::Now(), kUserRequested);
  coordinator()->queue()->AddRequest(
      request2, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  return request2;
}

TEST_F(RequestCoordinatorTest, StartProcessingWithNoRequests) {
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions(),
                                             immediate_callback()));
  PumpLoop();

  EXPECT_TRUE(immediate_schedule_callback_called());

  // Verify queue depth UMA for starting scheduled processing on empty queue.
  if (base::SysInfo::IsLowEndDevice()) {
    histograms().ExpectBucketCount(
        "OfflinePages.Background.ScheduledStart.AvailableRequestCount.Svelte",
        0, 1);
  } else {
    histograms().ExpectBucketCount(
        "OfflinePages.Background.ScheduledStart.AvailableRequestCount", 0, 1);
  }
}

TEST_F(RequestCoordinatorTest, StartProcessingWithRequestInProgress) {
  // Start processing for this request.
  EXPECT_NE(
      coordinator()->SavePageLater(
          kUrl1, kClientId1, kUserRequested,
          RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER), 0);

  // Ensure that the forthcoming request does not finish - we simulate it being
  // in progress by asking it to skip making the completion callback.
  EnableOfflinerCallback(false);

  // Sending the request to the offliner should make it busy.
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions(),
                                             immediate_callback()));
  PumpLoop();

  EXPECT_TRUE(is_busy());
  // Since the offliner is disabled, this callback should not be called.
  EXPECT_FALSE(immediate_schedule_callback_called());

  // Now trying to start processing on another request should return false.
  EXPECT_FALSE(coordinator()->StartProcessing(device_conditions(),
                                              immediate_callback()));
}

TEST_F(RequestCoordinatorTest, SavePageLater) {
  // The user-requested request which gets processed by SavePageLater
  // would invoke user request callback.
  coordinator()->SetImmediateScheduleCallbackForTest(immediate_callback());

  EXPECT_NE(
      coordinator()->SavePageLater(
          kUrl1, kClientId1, kUserRequested,
          RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER), 0);

  // Expect that a request got placed on the queue.
  coordinator()->queue()->GetRequests(base::Bind(
      &RequestCoordinatorTest::GetRequestsDone, base::Unretained(this)));

  // Wait for callbacks to finish, both request queue and offliner.
  PumpLoop();
  EXPECT_TRUE(immediate_schedule_callback_called());

  // Check the request queue is as expected.
  EXPECT_EQ(1UL, last_requests().size());
  EXPECT_EQ(kUrl1, last_requests().at(0)->url());
  EXPECT_EQ(kClientId1, last_requests().at(0)->client_id());

  // Expect that the scheduler got notified.
  SchedulerStub* scheduler_stub =
      reinterpret_cast<SchedulerStub*>(coordinator()->scheduler());
  EXPECT_TRUE(scheduler_stub->schedule_called());
  EXPECT_EQ(coordinator()
                ->GetTriggerConditions(last_requests()[0]->user_requested())
                .minimum_battery_percentage,
            scheduler_stub->conditions()->minimum_battery_percentage);

  // Check that the observer got the notification that a page is available
  EXPECT_TRUE(observer().added_called());

  // Verify queue depth UMA for starting immediate processing.
  if (base::SysInfo::IsLowEndDevice()) {
    histograms().ExpectBucketCount(
        "OfflinePages.Background.ImmediateStart.AvailableRequestCount.Svelte",
        1, 1);
  } else {
    histograms().ExpectBucketCount(
        "OfflinePages.Background.ImmediateStart.AvailableRequestCount", 1, 1);
  }
}

TEST_F(RequestCoordinatorTest, SavePageLaterFailed) {
  // The user-requested request which gets processed by SavePageLater
  // would invoke user request callback.
  coordinator()->SetImmediateScheduleCallbackForTest(immediate_callback());

  EXPECT_TRUE(
      coordinator()->SavePageLater(
          kUrl1, kClientId1, kUserRequested,
          RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER) != 0);

  // Expect that a request got placed on the queue.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));

  // Wait for callbacks to finish, both request queue and offliner.
  PumpLoop();

  // On low-end devices the callback will be called with false since the
  // processing started but failed due to svelte devices.
  EXPECT_TRUE(immediate_schedule_callback_called());
  if (base::SysInfo::IsLowEndDevice()) {
    EXPECT_FALSE(immediate_schedule_callback_result());
  } else {
    EXPECT_TRUE(immediate_schedule_callback_result());
  }

  // Check the request queue is as expected.
  EXPECT_EQ(1UL, last_requests().size());
  EXPECT_EQ(kUrl1, last_requests().at(0)->url());
  EXPECT_EQ(kClientId1, last_requests().at(0)->client_id());

  // Expect that the scheduler got notified.
  SchedulerStub* scheduler_stub = reinterpret_cast<SchedulerStub*>(
      coordinator()->scheduler());
  EXPECT_TRUE(scheduler_stub->schedule_called());
  EXPECT_EQ(coordinator()
                ->GetTriggerConditions(last_requests()[0]->user_requested())
                .minimum_battery_percentage,
            scheduler_stub->conditions()->minimum_battery_percentage);

  // Check that the observer got the notification that a page is available
  EXPECT_TRUE(observer().added_called());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestSucceeded) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  SetupForOfflinerDoneCallbackTest(&request);

  // Call the OfflinerDoneCallback to simulate the page being completed, wait
  // for callbacks.
  SendOfflinerDoneCallback(request, Offliner::RequestStatus::SAVED);
  PumpLoop();
  EXPECT_TRUE(immediate_schedule_callback_called());

  // Verify the request gets removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // We should not find any requests in the queue anymore.
  // RequestPicker should *not* have tried to start an additional job,
  // because the request queue is empty now.
  EXPECT_EQ(0UL, last_requests().size());
  // Check that the observer got the notification that we succeeded, and that
  // the request got removed from the queue.
  EXPECT_TRUE(observer().completed_called());
  EXPECT_EQ(RequestCoordinator::BackgroundSavePageResult::SUCCESS,
            observer().last_status());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestSucceededButLostNetwork) {
  // Add a request to the queue and set offliner done callback for it.
  offline_pages::SavePageRequest request(kRequestId1, kUrl1, kClientId1,
                                         base::Time::Now(), kUserRequested);
  SetupForOfflinerDoneCallbackTest(&request);
  EnableOfflinerCallback(false);

  // Add a 2nd request to the queue.
  AddRequest2();

  // Disconnect network.
  SetNetworkConnected(false);

  // Call the OfflinerDoneCallback to simulate the page being completed, wait
  // for callbacks.
  SendOfflinerDoneCallback(request, Offliner::RequestStatus::SAVED);
  PumpLoop();
  EXPECT_TRUE(immediate_schedule_callback_called());

  // Verify not busy with 2nd request (since no connection).
  EXPECT_FALSE(is_busy());

  // Now connect network and verify processing starts.
  SetNetworkConnected(true);
  CallConnectionTypeObserver();
  PumpLoop();
  EXPECT_TRUE(is_busy());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestFailed) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  request.set_completed_attempt_count(kMaxCompletedTries - 1);
  SetupForOfflinerDoneCallbackTest(&request);
  // Stop processing before completing the second request on the queue.
  EnableOfflinerCallback(false);

  // Add second request to the queue to check handling when first fails.
  AddRequest2();
  PumpLoop();

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  SendOfflinerDoneCallback(request,
                           Offliner::RequestStatus::PRERENDERING_FAILED);
  PumpLoop();

  // For retriable failure, processing should stop and scheduler callback
  // called (so that request can be retried first next processing window).
  EXPECT_TRUE(immediate_schedule_callback_called());

  // TODO(dougarnett): Consider injecting mock RequestPicker for this test
  // and verifying that there is no attempt to pick another request following
  // this failure code.

  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // Now just one request in the queue since failed request removed
  // (max number of attempts exceeded).
  EXPECT_EQ(1UL, last_requests().size());
  // Check that the observer got the notification that we failed (and the
  // subsequent notification that the request was removed).
  EXPECT_TRUE(observer().completed_called());
  EXPECT_EQ(RequestCoordinator::BackgroundSavePageResult::RETRY_COUNT_EXCEEDED,
            observer().last_status());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneRequestFailedNoRetryFailure) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(kRequestId1, kUrl1, kClientId1,
                                         base::Time::Now(), kUserRequested);
  SetupForOfflinerDoneCallbackTest(&request);
  EnableOfflinerCallback(false);

  // Add second request to the queue to check handling when first fails.
  AddRequest2();
  PumpLoop();

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  SendOfflinerDoneCallback(
      request, Offliner::RequestStatus::PRERENDERING_FAILED_NO_RETRY);
  PumpLoop();

  // For no retry failure, processing should continue to 2nd request so
  // no scheduler callback yet.
  EXPECT_FALSE(immediate_schedule_callback_called());

  // TODO(dougarnett): Consider injecting mock RequestPicker for this test
  // and verifying that there is as attempt to pick another request following
  // this non-retryable failure code.

  coordinator()->queue()->GetRequests(base::Bind(
      &RequestCoordinatorTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Now just one request in the queue since non-retryable failure.
  EXPECT_EQ(1UL, last_requests().size());
  // Check that the observer got the notification that we failed (and the
  // subsequent notification that the request was removed).
  EXPECT_TRUE(observer().completed_called());
  EXPECT_EQ(RequestCoordinator::BackgroundSavePageResult::PRERENDER_FAILURE,
            observer().last_status());
}

TEST_F(RequestCoordinatorTest, OfflinerDoneForegroundCancel) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  SetupForOfflinerDoneCallbackTest(&request);

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  SendOfflinerDoneCallback(request,
                           Offliner::RequestStatus::FOREGROUND_CANCELED);
  PumpLoop();
  EXPECT_TRUE(immediate_schedule_callback_called());

  // Verify the request is not removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(base::Bind(
      &RequestCoordinatorTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Request no longer in the queue (for single attempt policy).
  EXPECT_EQ(1UL, last_requests().size());
  // Verify foreground cancel not counted as an attempt after all.
  EXPECT_EQ(0L, last_requests().at(0)->completed_attempt_count());
}

TEST_F(RequestCoordinatorTest, OfflinerDonePrerenderingCancel) {
  // Add a request to the queue, wait for callbacks to finish.
  offline_pages::SavePageRequest request(kRequestId1, kUrl1, kClientId1,
                                         base::Time::Now(), kUserRequested);
  SetupForOfflinerDoneCallbackTest(&request);

  // Call the OfflinerDoneCallback to simulate the request failed, wait
  // for callbacks.
  SendOfflinerDoneCallback(request,
                           Offliner::RequestStatus::PRERENDERING_CANCELED);
  PumpLoop();
  EXPECT_TRUE(immediate_schedule_callback_called());

  // Verify the request is not removed from the queue, and wait for callbacks.
  coordinator()->queue()->GetRequests(base::Bind(
      &RequestCoordinatorTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Request still in the queue.
  EXPECT_EQ(1UL, last_requests().size());
  // Verify prerendering cancel not counted as an attempt after all.
  const std::unique_ptr<SavePageRequest>& found_request =
      last_requests().front();
  EXPECT_EQ(0L, found_request->completed_attempt_count());
}

// If one item completes, and there are no more user requeted items left,
// we should make a scheduler entry for a non-user requested item.
TEST_F(RequestCoordinatorTest, RequestNotPickedDisabledItemsRemain) {
  coordinator()->StartProcessing(device_conditions(), immediate_callback());
  EXPECT_TRUE(is_starting());

  // Call RequestNotPicked, simulating a request on the disabled list.
  CallRequestNotPicked(false, true);
  PumpLoop();

  EXPECT_FALSE(is_starting());

  // The scheduler should have been called to schedule the disabled task for
  // 5 minutes from now.
  SchedulerStub* scheduler_stub =
      reinterpret_cast<SchedulerStub*>(coordinator()->scheduler());
  EXPECT_TRUE(scheduler_stub->backup_schedule_called());
  EXPECT_TRUE(scheduler_stub->unschedule_called());
}

// If one item completes, and there are no more user requeted items left,
// we should make a scheduler entry for a non-user requested item.
TEST_F(RequestCoordinatorTest, RequestNotPickedNonUserRequestedItemsRemain) {
  coordinator()->StartProcessing(device_conditions(), immediate_callback());
  EXPECT_TRUE(is_starting());

  // Call RequestNotPicked, and make sure we pick schedule a task for non user
  // requested conditions, with no tasks on the disabled list.
  CallRequestNotPicked(true, false);
  PumpLoop();

  EXPECT_FALSE(is_starting());
  EXPECT_TRUE(immediate_schedule_callback_called());

  // The scheduler should have been called to schedule the non-user requested
  // task.
  SchedulerStub* scheduler_stub =
      reinterpret_cast<SchedulerStub*>(coordinator()->scheduler());
  EXPECT_TRUE(scheduler_stub->schedule_called());
  EXPECT_TRUE(scheduler_stub->unschedule_called());
  const Scheduler::TriggerConditions* conditions = scheduler_stub->conditions();
  EXPECT_EQ(conditions->require_power_connected,
            coordinator()->policy()->PowerRequired(!kUserRequested));
  EXPECT_EQ(
      conditions->minimum_battery_percentage,
      coordinator()->policy()->BatteryPercentageRequired(!kUserRequested));
  EXPECT_EQ(conditions->require_unmetered_network,
            coordinator()->policy()->UnmeteredNetworkRequired(!kUserRequested));
}

TEST_F(RequestCoordinatorTest, SchedulerGetsLeastRestrictiveConditions) {
  // Put two requests on the queue - The first is user requested, and
  // the second is not user requested.
  AddRequest1();
  offline_pages::SavePageRequest request2(kRequestId2, kUrl2, kClientId2,
                                          base::Time::Now(), !kUserRequested);
  coordinator()->queue()->AddRequest(
      request2, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();

  // Trigger the scheduler to schedule for the least restrictive condition.
  ScheduleForTest();
  PumpLoop();

  // Expect that the scheduler got notified, and it is at user_requested
  // priority.
  SchedulerStub* scheduler_stub =
      reinterpret_cast<SchedulerStub*>(coordinator()->scheduler());
  const Scheduler::TriggerConditions* conditions = scheduler_stub->conditions();
  EXPECT_TRUE(scheduler_stub->schedule_called());
  EXPECT_EQ(conditions->require_power_connected,
            coordinator()->policy()->PowerRequired(kUserRequested));
  EXPECT_EQ(conditions->minimum_battery_percentage,
            coordinator()->policy()->BatteryPercentageRequired(kUserRequested));
  EXPECT_EQ(conditions->require_unmetered_network,
            coordinator()->policy()->UnmeteredNetworkRequired(kUserRequested));
}

TEST_F(RequestCoordinatorTest, StartProcessingWithLoadingDisabled) {
  // Add a request to the queue, wait for callbacks to finish.
  AddRequest1();
  PumpLoop();

  DisableLoading();
  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions(),
                                             immediate_callback()));

  // Let the async callbacks in the request coordinator run.
  PumpLoop();
  EXPECT_TRUE(immediate_schedule_callback_called());

  EXPECT_FALSE(is_starting());
  EXPECT_EQ(Offliner::PRERENDERING_NOT_STARTED, last_offlining_status());
}

// This tests a StopProcessing call before we have actually started the
// prerenderer.
TEST_F(RequestCoordinatorTest, StartProcessingThenStopProcessingImmediately) {
  // Add a request to the queue, wait for callbacks to finish.
  AddRequest1();
  PumpLoop();

  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions(),
                                             immediate_callback()));
  EXPECT_TRUE(is_starting());

  // Now, quick, before it can do much (we haven't called PumpLoop), cancel it.
  coordinator()->StopProcessing(Offliner::REQUEST_COORDINATOR_CANCELED);

  // Let the async callbacks in the request coordinator run.
  PumpLoop();
  EXPECT_TRUE(immediate_schedule_callback_called());

  EXPECT_FALSE(is_starting());

  // OfflinerDoneCallback will not end up getting called with status SAVED,
  // since we cancelled the event before it called offliner_->LoadAndSave().
  EXPECT_EQ(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED,
            last_offlining_status());

  // Since offliner was not started, it will not have seen cancel call.
  EXPECT_FALSE(OfflinerWasCanceled());
}

// This tests a StopProcessing call after the prerenderer has been started.
TEST_F(RequestCoordinatorTest, StartProcessingThenStopProcessingLater) {
  // Add a request to the queue, wait for callbacks to finish.
  AddRequest1();
  PumpLoop();

  // Ensure the start processing request stops before the completion callback.
  EnableOfflinerCallback(false);

  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions(),
                                             immediate_callback()));
  EXPECT_TRUE(is_starting());

  // Let all the async parts of the start processing pipeline run to completion.
  PumpLoop();

  // Observer called for starting processing.
  EXPECT_TRUE(observer().changed_called());
  EXPECT_EQ(SavePageRequest::RequestState::OFFLINING, observer().state());
  observer().Clear();

  // Since the offliner is disabled, this callback should not be called.
  EXPECT_FALSE(immediate_schedule_callback_called());

  // Coordinator should now be busy.
  EXPECT_TRUE(is_busy());
  EXPECT_FALSE(is_starting());

  // Now we cancel it while the prerenderer is busy.
  coordinator()->StopProcessing(Offliner::REQUEST_COORDINATOR_CANCELED);

  // Let the async callbacks in the cancel run.
  PumpLoop();

  // Observer called for stopped processing.
  EXPECT_TRUE(observer().changed_called());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE, observer().state());
  observer().Clear();

  EXPECT_FALSE(is_busy());

  // OfflinerDoneCallback will not end up getting called with status SAVED,
  // since we cancelled the event before the LoadAndSave completed.
  EXPECT_EQ(Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED,
            last_offlining_status());

  // Since offliner was started, it will have seen cancel call.
  EXPECT_TRUE(OfflinerWasCanceled());
}

// This tests that canceling a request will result in TryNextRequest() getting
// called.
TEST_F(RequestCoordinatorTest, RemoveInflightRequest) {
  // Add a request to the queue, wait for callbacks to finish.
  AddRequest1();
  PumpLoop();

  // Ensure the start processing request stops before the completion callback.
  EnableOfflinerCallback(false);

  EXPECT_TRUE(coordinator()->StartProcessing(device_conditions(),
                                             immediate_callback()));

  // Let all the async parts of the start processing pipeline run to completion.
  PumpLoop();
  // Since the offliner is disabled, this callback should not be called.
  EXPECT_FALSE(immediate_schedule_callback_called());

  // Remove the request while it is processing.
  std::vector<int64_t> request_ids{kRequestId1};
  coordinator()->RemoveRequests(
      request_ids, base::Bind(&RequestCoordinatorTest::RemoveRequestsDone,
                              base::Unretained(this)));

  // Let the async callbacks in the cancel run.
  PumpLoop();

  // Since offliner was started, it will have seen cancel call.
  EXPECT_TRUE(OfflinerWasCanceled());
}

TEST_F(RequestCoordinatorTest, MarkRequestCompleted) {
  int64_t request_id = coordinator()->SavePageLater(
      kUrl1, kClientId1, kUserRequested,
      RequestCoordinator::RequestAvailability::DISABLED_FOR_OFFLINER);
  PumpLoop();
  EXPECT_NE(request_id, 0l);

  // Verify request added in OFFLINING state.
  EXPECT_TRUE(observer().added_called());
  EXPECT_EQ(SavePageRequest::RequestState::OFFLINING, observer().state());

  // Call the method under test, making sure we send SUCCESS to the observer.
  coordinator()->MarkRequestCompleted(request_id);
  PumpLoop();

  // Our observer should have seen SUCCESS instead of REMOVED.
  EXPECT_EQ(RequestCoordinator::BackgroundSavePageResult::SUCCESS,
            observer().last_status());
  EXPECT_TRUE(observer().completed_called());
}

TEST_F(RequestCoordinatorTest, EnableForOffliner) {
  // Pretend we are on low-end device so immediate start won't happen.
  SetIsLowEndDeviceForTest(true);

  int64_t request_id = coordinator()->SavePageLater(
      kUrl1, kClientId1, kUserRequested,
      RequestCoordinator::RequestAvailability::DISABLED_FOR_OFFLINER);
  PumpLoop();
  EXPECT_NE(request_id, 0l);

  // Verify request added and initial change to OFFLINING (in foreground).
  EXPECT_TRUE(observer().added_called());
  EXPECT_TRUE(observer().changed_called());
  EXPECT_EQ(SavePageRequest::RequestState::OFFLINING, observer().state());
  observer().Clear();

  // Ensure that the new request does not finish so we can verify state change.
  EnableOfflinerCallback(false);

  coordinator()->EnableForOffliner(request_id, kClientId1);
  PumpLoop();

  // Verify request changed again.
  EXPECT_TRUE(observer().changed_called());
  EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE, observer().state());
}

TEST_F(RequestCoordinatorTest, WatchdogTimeoutForScheduledProcessing) {
  // Build a request to use with the pre-renderer, and put it on the queue.
  offline_pages::SavePageRequest request(
      kRequestId1, kUrl1, kClientId1, base::Time::Now(), kUserRequested);
  // Set request to allow one more completed attempt.
  int max_tries = coordinator()->policy()->GetMaxCompletedTries();
  request.set_completed_attempt_count(max_tries - 1);
  coordinator()->queue()->AddRequest(
      request,
      base::Bind(&RequestCoordinatorTest::AddRequestDone,
                 base::Unretained(this)));
  PumpLoop();

  // Ensure that the new request does not finish - we simulate it being
  // in progress by asking it to skip making the completion callback.
  EnableOfflinerCallback(false);

  // Sending the request to the offliner.
  EXPECT_TRUE(
      coordinator()->StartProcessing(device_conditions(), waiting_callback()));
  PumpLoop();

  // Advance the mock clock far enough to cause a watchdog timeout
  AdvanceClockBy(base::TimeDelta::FromSeconds(
      coordinator()
          ->policy()
          ->GetSinglePageTimeLimitWhenBackgroundScheduledInSeconds() + 1));
  PumpLoop();

  // Wait for timeout to expire.  Use a TaskRunner with a DelayedTaskRunner
  // which won't time out immediately, so the watchdog thread doesn't kill valid
  // tasks too soon.
  WaitForCallback();
  PumpLoop();

  EXPECT_FALSE(is_starting());
  EXPECT_FALSE(coordinator()->is_busy());
  EXPECT_TRUE(OfflinerWasCanceled());
}

TEST_F(RequestCoordinatorTest, WatchdogTimeoutForImmediateProcessing) {
  // If low end device, pretend it is not so that immediate start happens.
  SetIsLowEndDeviceForTest(false);

  // Ensure that the new request does not finish - we simulate it being
  // in progress by asking it to skip making the completion callback.
  EnableOfflinerCallback(false);

  EXPECT_NE(
      coordinator()->SavePageLater(
          kUrl1, kClientId1, kUserRequested,
          RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER), 0);
  PumpLoop();

  // Verify that immediate start from adding the request did happen.
  EXPECT_TRUE(coordinator()->is_busy());

  // Advance the mock clock 1 second before the watchdog timeout.
  AdvanceClockBy(base::TimeDelta::FromSeconds(
      coordinator()
          ->policy()
          ->GetSinglePageTimeLimitForImmediateLoadInSeconds() - 1));
  PumpLoop();

  // Verify still busy.
  EXPECT_TRUE(coordinator()->is_busy());
  EXPECT_FALSE(OfflinerWasCanceled());

  // Advance the mock clock past the watchdog timeout now.
  AdvanceClockBy(base::TimeDelta::FromSeconds(2));
  PumpLoop();

  // Verify the request timed out.
  EXPECT_TRUE(OfflinerWasCanceled());
}

TEST_F(RequestCoordinatorTest, TimeBudgetExceeded) {
  EnableOfflinerCallback(false);
  // Build two requests to use with the pre-renderer, and put it on the queue.
  AddRequest1();
  // The second request will have a larger completed attempt count.
  offline_pages::SavePageRequest request2(kRequestId1 + 1, kUrl1, kClientId1,
                                          base::Time::Now(), kUserRequested);
  request2.set_completed_attempt_count(kAttemptCount);
  coordinator()->queue()->AddRequest(
      request2, base::Bind(&RequestCoordinatorTest::AddRequestDone,
                           base::Unretained(this)));
  PumpLoop();

  // Sending the request to the offliner.
  EXPECT_TRUE(
      coordinator()->StartProcessing(device_conditions(), waiting_callback()));
  PumpLoop();

  // Advance the mock clock far enough to exceed our time budget.
  // The first request will time out, and because we are over time budget,
  // the second request will not be started.
  AdvanceClockBy(base::TimeDelta::FromSeconds(kTestTimeBudgetSeconds));
  PumpLoop();

  // TryNextRequest should decide that there is no more work to be done,
  // and call back to the scheduler, even though there is another request in the
  // queue.  Both requests should be left in the queue.
  coordinator()->queue()->GetRequests(
      base::Bind(&RequestCoordinatorTest::GetRequestsDone,
                 base::Unretained(this)));
  PumpLoop();

  // We should find two requests in the queue.
  // The first request should now have a completed count of 1.
  EXPECT_EQ(2UL, last_requests().size());
  EXPECT_EQ(1L, last_requests().at(0)->completed_attempt_count());
}

TEST_F(RequestCoordinatorTest, TryNextRequestWithNoNetwork) {
  SavePageRequest request1 = AddRequest1();
  AddRequest2();
  PumpLoop();

  // Set up for the call to StartProcessing.
  EnableOfflinerCallback(false);

  // Sending the request to the offliner.
  EXPECT_TRUE(
      coordinator()->StartProcessing(device_conditions(), waiting_callback()));
  PumpLoop();
  EXPECT_TRUE(coordinator()->is_busy());

  // Now lose the network connection.
  SetNetworkConnected(false);

  // Complete first request and then TryNextRequest should decide not
  // to pick another request (because of no network connection).
  SendOfflinerDoneCallback(request1, Offliner::RequestStatus::SAVED);
  PumpLoop();

  // Not starting nor busy with next request.
  EXPECT_FALSE(coordinator()->is_starting());
  EXPECT_FALSE(coordinator()->is_busy());

  // Get queued requests.
  coordinator()->queue()->GetRequests(base::Bind(
      &RequestCoordinatorTest::GetRequestsDone, base::Unretained(this)));
  PumpLoop();

  // We should find one request in the queue.
  EXPECT_EQ(1UL, last_requests().size());
}

TEST_F(RequestCoordinatorTest, GetAllRequests) {
  // Add two requests to the queue.
  AddRequest1();
  AddRequest2();
  PumpLoop();

  // Start the async status fetching.
  coordinator()->GetAllRequests(base::Bind(
      &RequestCoordinatorTest::GetQueuedRequestsDone, base::Unretained(this)));
  PumpLoop();

  // Wait for async get to finish.
  WaitForCallback();
  PumpLoop();

  // Check that the statuses found in the callback match what we expect.
  EXPECT_EQ(2UL, last_requests().size());
  EXPECT_EQ(kRequestId1, last_requests().at(0)->request_id());
  EXPECT_EQ(kRequestId2, last_requests().at(1)->request_id());
}

#if defined(OS_IOS)
// Flaky on IOS. http://crbug/663311
#define MAYBE_PauseAndResumeObserver DISABLED_PauseAndResumeObserver
#else
#define MAYBE_PauseAndResumeObserver PauseAndResumeObserver
#endif
TEST_F(RequestCoordinatorTest, MAYBE_PauseAndResumeObserver) {
  // Add a request to the queue.
  AddRequest1();
  PumpLoop();

  // Pause the request.
  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId1);
  coordinator()->PauseRequests(request_ids);
  PumpLoop();

  EXPECT_TRUE(observer().changed_called());
  EXPECT_EQ(SavePageRequest::RequestState::PAUSED, observer().state());

  // Clear out the observer before the next call.
  observer().Clear();

  // Resume the request.
  coordinator()->ResumeRequests(request_ids);
  PumpLoop();

  EXPECT_TRUE(observer().changed_called());

  // Now whether request is offlining or just available depends on whether test
  // is run on svelte device or not.
  if (base::SysInfo::IsLowEndDevice()) {
    EXPECT_EQ(SavePageRequest::RequestState::AVAILABLE, observer().state());
  } else {
    EXPECT_EQ(SavePageRequest::RequestState::OFFLINING, observer().state());
  }
}

TEST_F(RequestCoordinatorTest, RemoveRequest) {
  // Add a request to the queue.
  AddRequest1();
  PumpLoop();

  // Remove the request.
  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId1);
  coordinator()->RemoveRequests(
      request_ids, base::Bind(&RequestCoordinatorTest::RemoveRequestsDone,
                              base::Unretained(this)));

  PumpLoop();
  WaitForCallback();
  PumpLoop();

  EXPECT_TRUE(observer().completed_called());
  EXPECT_EQ(RequestCoordinator::BackgroundSavePageResult::REMOVED,
            observer().last_status());
  EXPECT_EQ(1UL, last_remove_results().size());
  EXPECT_EQ(kRequestId1, std::get<0>(last_remove_results().at(0)));
}

TEST_F(RequestCoordinatorTest,
       SavePageStartsProcessingWhenConnectedAndNotLowEndDevice) {
  // Turn off the callback so that the request stops before processing in
  // PumpLoop.
  EnableOfflinerCallback(false);
  EXPECT_NE(
      coordinator()->SavePageLater(
          kUrl1, kClientId1, kUserRequested,
          RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER), 0);
  PumpLoop();

  // Now whether processing triggered immediately depends on whether test
  // is run on svelte device or not.
  if (base::SysInfo::IsLowEndDevice()) {
    EXPECT_FALSE(is_busy());
  } else {
    EXPECT_TRUE(is_busy());
  }
}

TEST_F(RequestCoordinatorTest,
       SavePageStartsProcessingWhenConnectedOnLowEndDeviceIfFlagEnabled) {
  // Mark device as low-end device.
  SetIsLowEndDeviceForTest(true);
  EXPECT_FALSE(offline_pages::IsOfflinePagesSvelteConcurrentLoadingEnabled());

  // Make a request.
  EXPECT_NE(coordinator()->SavePageLater(
                kUrl1, kClientId1, kUserRequested,
                RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER),
            0);
  PumpLoop();

  // Verify not immediately busy (since low-end device).
  EXPECT_FALSE(is_busy());

  // Set feature flag to allow concurrent loads.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesSvelteConcurrentLoadingFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesSvelteConcurrentLoadingEnabled());

  // Turn off the callback so that the request stops before processing in
  // PumpLoop.
  EnableOfflinerCallback(false);

  // Make another request.
  EXPECT_NE(coordinator()->SavePageLater(
                kUrl2, kClientId2, kUserRequested,
                RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER),
            0);
  PumpLoop();

  // Verify immediate processing did start this time.
  EXPECT_TRUE(is_busy());
}

TEST_F(RequestCoordinatorTest, SavePageDoesntStartProcessingWhenDisconnected) {
  // If low end device, pretend it is not so that immediate start allowed.
  SetIsLowEndDeviceForTest(false);

  SetNetworkConnected(false);
  EnableOfflinerCallback(false);
  EXPECT_NE(
      coordinator()->SavePageLater(
          kUrl1, kClientId1, kUserRequested,
          RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER), 0);
  PumpLoop();
  EXPECT_FALSE(is_busy());

  // Now connect network and verify processing starts.
  SetNetworkConnected(true);
  CallConnectionTypeObserver();
  PumpLoop();
  EXPECT_TRUE(is_busy());
}

TEST_F(RequestCoordinatorTest,
       SavePageDoesStartProcessingWhenPoorlyConnected) {
  // Set specific network type for 2G with poor effective connection.
  SetNetworkConditionsForTest(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  SetEffectiveConnectionTypeForTest(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // Turn off the callback so that the request stops before processing in
  // PumpLoop.
  EnableOfflinerCallback(false);

  EXPECT_NE(
      coordinator()->SavePageLater(
          kUrl1, kClientId1, kUserRequested,
          RequestCoordinator::RequestAvailability::ENABLED_FOR_OFFLINER), 0);
  PumpLoop();
  EXPECT_TRUE(is_busy());
}

TEST_F(RequestCoordinatorTest,
       ResumeStartsProcessingWhenConnectedAndNotLowEndDevice) {
  // Start unconnected.
  SetNetworkConnected(false);

  // Turn off the callback so that the request stops before processing in
  // PumpLoop.
  EnableOfflinerCallback(false);

  // Add a request to the queue.
  AddRequest1();
  PumpLoop();
  EXPECT_FALSE(is_busy());

  // Pause the request.
  std::vector<int64_t> request_ids;
  request_ids.push_back(kRequestId1);
  coordinator()->PauseRequests(request_ids);
  PumpLoop();

  // Resume the request while disconnected.
  coordinator()->ResumeRequests(request_ids);
  PumpLoop();
  EXPECT_FALSE(is_busy());

  // Pause the request again.
  coordinator()->PauseRequests(request_ids);
  PumpLoop();

  // Now simulate reasonable connection.
  SetNetworkConnected(true);

  // Resume the request while connected.
  coordinator()->ResumeRequests(request_ids);
  EXPECT_FALSE(is_busy());
  PumpLoop();

  // Now whether processing triggered immediately depends on whether test
  // is run on svelte device or not.
  if (base::SysInfo::IsLowEndDevice()) {
    EXPECT_FALSE(is_busy());
  } else {
    EXPECT_TRUE(is_busy());
  }
}

}  // namespace offline_pages

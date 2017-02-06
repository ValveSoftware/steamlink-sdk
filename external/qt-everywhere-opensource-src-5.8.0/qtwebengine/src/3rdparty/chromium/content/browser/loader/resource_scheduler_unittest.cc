// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler.h"

#include <utility>

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host_factory.h"
#include "content/test/test_web_contents.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/http/http_server_properties_impl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/latency_info.h"
#include "url/scheme_host_port.h"

using std::string;

namespace content {

namespace {

class TestRequestFactory;

const int kChildId = 30;
const int kRouteId = 75;
const int kChildId2 = 43;
const int kRouteId2 = 67;
const int kBackgroundChildId = 35;
const int kBackgroundRouteId = 43;

class TestRequest : public ResourceController {
 public:
  TestRequest(std::unique_ptr<net::URLRequest> url_request,
              std::unique_ptr<ResourceThrottle> throttle,
              ResourceScheduler* scheduler)
      : started_(false),
        url_request_(std::move(url_request)),
        throttle_(std::move(throttle)),
        scheduler_(scheduler) {
    throttle_->set_controller_for_testing(this);
  }
  ~TestRequest() override {
    // The URLRequest must still be valid when the ScheduledResourceRequest is
    // destroyed, so that it can unregister itself.
    throttle_.reset();
  }

  bool started() const { return started_; }

  void Start() {
    bool deferred = false;
    throttle_->WillStartRequest(&deferred);
    started_ = !deferred;
  }

  void ChangePriority(net::RequestPriority new_priority, int intra_priority) {
    scheduler_->ReprioritizeRequest(url_request_.get(), new_priority,
                                    intra_priority);
  }

  void Cancel() override {
    // Alert the scheduler that the request can be deleted.
    throttle_.reset();
  }

  const net::URLRequest* url_request() const { return url_request_.get(); }

 protected:
  // ResourceController interface:
  void CancelAndIgnore() override {}
  void CancelWithError(int error_code) override {}
  void Resume() override { started_ = true; }

 private:
  bool started_;
  std::unique_ptr<net::URLRequest> url_request_;
  std::unique_ptr<ResourceThrottle> throttle_;
  ResourceScheduler* scheduler_;
};

class CancelingTestRequest : public TestRequest {
 public:
  CancelingTestRequest(std::unique_ptr<net::URLRequest> url_request,
                       std::unique_ptr<ResourceThrottle> throttle,
                       ResourceScheduler* scheduler)
      : TestRequest(std::move(url_request), std::move(throttle), scheduler) {}

  void set_request_to_cancel(std::unique_ptr<TestRequest> request_to_cancel) {
    request_to_cancel_ = std::move(request_to_cancel);
  }

 private:
  void Resume() override {
    TestRequest::Resume();
    request_to_cancel_.reset();
  }

  std::unique_ptr<TestRequest> request_to_cancel_;
};

class FakeResourceContext : public ResourceContext {
 private:
  net::HostResolver* GetHostResolver() override { return NULL; }
  net::URLRequestContext* GetRequestContext() override { return NULL; }
};

class ResourceSchedulerTest : public testing::Test {
 protected:
  ResourceSchedulerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        field_trial_list_(new base::MockEntropyProvider()) {
    InitializeScheduler();
    context_.set_http_server_properties(&http_server_properties_);
  }

  ~ResourceSchedulerTest() override {
    CleanupScheduler();
  }

  // Done separately from construction to allow for modification of command
  // line flags in tests.
  void InitializeScheduler() {
    CleanupScheduler();

    // Destroys previous scheduler, also destroys any previously created
    // mock_timer_.
    scheduler_.reset(new ResourceScheduler());

    scheduler_->OnClientCreated(kChildId, kRouteId);
    scheduler_->OnClientCreated(
        kBackgroundChildId, kBackgroundRouteId);
  }

  void CleanupScheduler() {
    if (scheduler_) {
      scheduler_->OnClientDeleted(kChildId, kRouteId);
      scheduler_->OnClientDeleted(kBackgroundChildId, kBackgroundRouteId);
    }
  }

  // Create field trials based on the argument, which has the same format
  // as the argument to kForceFieldTrials.
  bool InitializeFieldTrials(const std::string& force_field_trial_argument) {
    return base::FieldTrialList::CreateTrialsFromString(
        force_field_trial_argument, std::set<std::string>());
  }

  std::unique_ptr<net::URLRequest> NewURLRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    std::unique_ptr<net::URLRequest> url_request(
        context_.CreateRequest(GURL(url), priority, NULL));
    return url_request;
  }

  std::unique_ptr<net::URLRequest> NewURLRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewURLRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  TestRequest* NewRequestWithRoute(const char* url,
                                   net::RequestPriority priority,
                                   int route_id) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, route_id);
  }

  TestRequest* NewRequestWithChildAndRoute(const char* url,
                                           net::RequestPriority priority,
                                           int child_id,
                                           int route_id) {
    return GetNewTestRequest(url, priority, child_id, route_id, true);
  }

  TestRequest* NewRequest(const char* url, net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  TestRequest* NewBackgroundRequest(const char* url,
                                    net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(
        url, priority, kBackgroundChildId, kBackgroundRouteId);
  }

  TestRequest* NewSyncRequest(const char* url, net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  TestRequest* NewBackgroundSyncRequest(const char* url,
                                        net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(
        url, priority, kBackgroundChildId, kBackgroundRouteId);
  }

  TestRequest* NewSyncRequestWithChildAndRoute(const char* url,
                                               net::RequestPriority priority,
                                               int child_id,
                                               int route_id) {
    return GetNewTestRequest(url, priority, child_id, route_id, false);
  }

  TestRequest* GetNewTestRequest(const char* url,
                                 net::RequestPriority priority,
                                 int child_id,
                                 int route_id,
                                 bool is_async) {
    std::unique_ptr<net::URLRequest> url_request(
        NewURLRequestWithChildAndRoute(url, priority, child_id, route_id));
    std::unique_ptr<ResourceThrottle> throttle(scheduler_->ScheduleRequest(
        child_id, route_id, is_async, url_request.get()));
    TestRequest* request = new TestRequest(std::move(url_request),
                                           std::move(throttle), scheduler());
    request->Start();
    return request;
  }

  void ChangeRequestPriority(TestRequest* request,
                             net::RequestPriority new_priority,
                             int intra_priority = 0) {
    request->ChangePriority(new_priority, intra_priority);
  }

  void FireCoalescingTimer() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    mock_timer_->Fire();
  }

  ResourceScheduler* scheduler() {
    return scheduler_.get();
  }

  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl ui_thread_;
  BrowserThreadImpl io_thread_;
  ResourceDispatcherHostImpl rdh_;
  std::unique_ptr<ResourceScheduler> scheduler_;
  base::FieldTrialList field_trial_list_;
  base::MockTimer* mock_timer_;
  net::HttpServerPropertiesImpl http_server_properties_;
  net::TestURLRequestContext context_;
};

TEST_F(ResourceSchedulerTest, OneIsolatedLowRequest) {
  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/1", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilBodyInserted) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilCriticalComplete) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, MediumDoesNotBlockCriticalComplete) {
  // kLayoutBlockingPriorityThreshold determines what priority level above which
  // requests are considered layout-blocking and must be completed before the
  // critical loading period is complete.  It is currently set to net::MEDIUM.
  std::unique_ptr<TestRequest> medium(
      NewRequest("http://host/low", net::MEDIUM));
  std::unique_ptr<TestRequest> lowest(
      NewRequest("http://host/lowest", net::LOWEST));
  std::unique_ptr<TestRequest> lowest2(
      NewRequest("http://host/lowest", net::LOWEST));
  EXPECT_TRUE(medium->started());
  EXPECT_TRUE(lowest->started());
  EXPECT_FALSE(lowest2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilBodyInsertedExceptSpdy) {
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low_spdy(
      NewRequest("https://spdyhost/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_TRUE(low_spdy->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, SpdyLowBlocksOtherLowUntilBodyInserted) {
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low_spdy(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low_spdy->started());
  EXPECT_FALSE(low->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
}

TEST_F(ResourceSchedulerTest, NavigationResetsState) {
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  scheduler()->OnNavigate(kChildId, kRouteId);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
}

TEST_F(ResourceSchedulerTest, BackgroundRequestStartsImmediately) {
  const int route_id = 0;  // Indicates a background request.
  std::unique_ptr<TestRequest> request(
      NewRequestWithRoute("http://host/1", net::LOWEST, route_id));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, MoreThanOneHighRequestBlocksDelayableRequests) {
  // If there are more than kInFlightNonDelayableRequestCountThreshold (=1)
  // high-priority / non-delayable requests, block all low priority fetches and
  // allow them through one at a time once the number of high priority requests
  // drops.
  std::unique_ptr<TestRequest> high1(
      NewRequest("http://host/high1", net::HIGHEST));
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high2", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high1->started());
  EXPECT_TRUE(high2->started());
  EXPECT_FALSE(low->started());
  EXPECT_FALSE(low2->started());

  high1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
}

TEST_F(ResourceSchedulerTest, CancelOtherRequestsWhileResuming) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low1(
      NewRequest("http://host/low1", net::LOWEST));

  std::unique_ptr<net::URLRequest> url_request(
      NewURLRequest("http://host/low2", net::LOWEST));
  std::unique_ptr<ResourceThrottle> throttle(scheduler()->ScheduleRequest(
      kChildId, kRouteId, true, url_request.get()));
  std::unique_ptr<CancelingTestRequest> low2(new CancelingTestRequest(
      std::move(url_request), std::move(throttle), scheduler()));
  low2->Start();

  std::unique_ptr<TestRequest> low3(
      NewRequest("http://host/low3", net::LOWEST));
  low2->set_request_to_cancel(std::move(low3));
  std::unique_ptr<TestRequest> low4(
      NewRequest("http://host/low4", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low2->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());
  EXPECT_TRUE(low2->started());
  EXPECT_TRUE(low4->started());
}

TEST_F(ResourceSchedulerTest, LimitedNumberOfDelayableRequestsInFlight) {
  // We only load low priority resources if there's a body.
  scheduler()->OnWillInsertBody(kChildId, kRouteId);

  // Throw in one high priority request to make sure that's not a factor.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  const int kMaxNumDelayableRequestsPerHost = 6;
  ScopedVector<TestRequest> lows_singlehost;
  // Queue up to the per-host limit (we subtract the current high-pri request).
  for (int i = 0; i < kMaxNumDelayableRequestsPerHost - 1; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  std::unique_ptr<TestRequest> second_last_singlehost(
      NewRequest("http://host/last", net::LOWEST));
  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/s_last", net::LOWEST));

  EXPECT_FALSE(second_last_singlehost->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(second_last_singlehost->started());
  EXPECT_FALSE(last_singlehost->started());

  lows_singlehost.erase(lows_singlehost.begin());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_singlehost->started());

  // Queue more requests from different hosts until we reach the total limit.
  int expected_slots_left =
      kMaxNumDelayableRequestsPerClient - kMaxNumDelayableRequestsPerHost;
  EXPECT_GT(expected_slots_left, 0);
  ScopedVector<TestRequest> lows_different_host;
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < expected_slots_left; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows_different_host.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_different_host[i]->started());
  }

  std::unique_ptr<TestRequest> last_different_host(
      NewRequest("http://host_new/last", net::LOWEST));
  EXPECT_FALSE(last_different_host->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityAndStart) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::HIGHEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityInQueue) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request->started());
  EXPECT_FALSE(idle->started());
}

TEST_F(ResourceSchedulerTest, LowerPriority) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::IDLE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  // 2 fewer filler requests: 1 for the "low" dummy at the start, and 1 for the
  // one at the end, which will be tested.
  const int kNumFillerRequests = kMaxNumDelayableRequestsPerClient - 2;
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kNumFillerRequests; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(request->started());
  EXPECT_TRUE(idle->started());
}

TEST_F(ResourceSchedulerTest, ReprioritizedRequestGoesToBackOfQueue) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  ChangeRequestPriority(request.get(), net::IDLE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());
}

TEST_F(ResourceSchedulerTest, HigherIntraPriorityGoesToFrontOfQueue) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::IntToString(i);
    lows.push_back(NewRequest(url.c_str(), net::IDLE));
  }

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::IDLE, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());

  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, NonHTTPSchedulesImmediately) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("chrome-extension://req", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, SpdyProxySchedulesImmediately) {
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  scheduler()->OnReceivedSpdyProxiedHttpResponse(kChildId, kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());

  std::unique_ptr<TestRequest> after(
      NewRequest("http://host/after", net::IDLE));
  EXPECT_TRUE(after->started());
}

TEST_F(ResourceSchedulerTest, NewSpdyHostInDelayableRequests) {
  scheduler()->OnWillInsertBody(kChildId, kRouteId);
  const int kMaxNumDelayableRequestsPerClient = 10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  ScopedVector<TestRequest> lows;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(low2->started());
}

TEST_F(ResourceSchedulerTest, OustandingRequestLimitEnforced) {
    const int kRequestLimit = 3;
    ASSERT_TRUE(InitializeFieldTrials(
        base::StringPrintf("OutstandingRequestLimiting/Limit=%d/",
                           kRequestLimit)));
    InitializeScheduler();

    // Throw in requests up to the above limit; make sure they are started.
    ScopedVector<TestRequest> requests;
    for (int i = 0; i < kRequestLimit; ++i) {
      string url = "http://host/medium";
      requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
      EXPECT_TRUE(requests[i]->started());
    }

    // Confirm that another request will indeed fail.
    string url = "http://host/medium";
    requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
    EXPECT_FALSE(requests[kRequestLimit]->started());
  }

  // Confirm that outstanding requests limits apply to requests to hosts
  // that support request priority.
  TEST_F(ResourceSchedulerTest,
         OutstandingRequestsLimitsEnforcedForRequestPriority) {
    const int kRequestLimit = 3;
    ASSERT_TRUE(InitializeFieldTrials(
        base::StringPrintf("OutstandingRequestLimiting/Limit=%d/",
                           kRequestLimit)));
    InitializeScheduler();

    http_server_properties_.SetSupportsSpdy(
        url::SchemeHostPort("https", "spdyhost", 443), true);

    // Throw in requests up to the above limit; make sure they are started.
    ScopedVector<TestRequest> requests;
    for (int i = 0; i < kRequestLimit; ++i) {
      string url = "http://spdyhost/medium";
      requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
      EXPECT_TRUE(requests[i]->started());
    }

    // Confirm that another request will indeed fail.
    string url = "http://spdyhost/medium";
    requests.push_back(NewRequest(url.c_str(), net::MEDIUM));
    EXPECT_FALSE(requests[kRequestLimit]->started());
  }

  TEST_F(ResourceSchedulerTest, OutstandingRequestLimitDelays) {
    const int kRequestLimit = 3;
    ASSERT_TRUE(InitializeFieldTrials(
        base::StringPrintf("OutstandingRequestLimiting/Limit=%d/",
                           kRequestLimit)));

    InitializeScheduler();
    std::unique_ptr<TestRequest> high(
        NewRequest("http://host/high", net::HIGHEST));
    std::unique_ptr<TestRequest> low(NewRequest("http://host/low",
                                                net::LOWEST));
    std::unique_ptr<TestRequest> low2(NewRequest("http://host/low",
                                                 net::LOWEST));
    EXPECT_TRUE(high->started());
    EXPECT_FALSE(low->started());
    EXPECT_FALSE(low2->started());

    high.reset();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(low->started());
    EXPECT_FALSE(low2->started());
  }

  // Async revalidations which are not started when the tab is closed must be
// started at some point, or they will hang around forever and prevent other
// async revalidations to the same URL from being issued.
TEST_F(ResourceSchedulerTest, RequestStartedAfterClientDeleted) {
  scheduler_->OnClientCreated(kChildId2, kRouteId2);
  std::unique_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  std::unique_ptr<TestRequest> lowest1(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  std::unique_ptr<TestRequest> lowest2(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  EXPECT_FALSE(lowest2->started());

  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  high.reset();
  lowest1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
}

// The ResourceScheduler::Client destructor calls
// LoadAnyStartablePendingRequests(), which may start some pending requests.
// This test is to verify that requests will be started at some point
// even if they were not started by the destructor.
TEST_F(ResourceSchedulerTest, RequestStartedAfterClientDeletedManyDelayable) {
  scheduler_->OnClientCreated(kChildId2, kRouteId2);
  std::unique_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  const int kMaxNumDelayableRequestsPerClient = 10;
  ScopedVector<TestRequest> delayable_requests;
  for (int i = 0; i < kMaxNumDelayableRequestsPerClient + 1; ++i) {
    delayable_requests.push_back(NewRequestWithChildAndRoute(
        "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  }
  std::unique_ptr<TestRequest> lowest(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  EXPECT_FALSE(lowest->started());

  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  high.reset();
  delayable_requests.clear();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest->started());
}

}  // unnamed namespace

}  // namespace content

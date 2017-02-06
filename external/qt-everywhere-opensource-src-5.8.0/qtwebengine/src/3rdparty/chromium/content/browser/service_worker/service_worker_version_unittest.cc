// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stdint.h>
#include <tuple>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_mojo_service.mojom.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

// IPC messages for testing ---------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

IPC_MESSAGE_CONTROL0(TestMsg_Message)
IPC_MESSAGE_ROUTED1(TestMsg_MessageFromWorker, int)

IPC_MESSAGE_CONTROL1(TestMsg_TestEvent, int)
IPC_MESSAGE_CONTROL2(TestMsg_TestEvent_Multiple, int, int)
IPC_MESSAGE_ROUTED2(TestMsg_TestEventResult, int, std::string)
IPC_MESSAGE_ROUTED2(TestMsg_TestSimpleEventResult,
                    int,
                    blink::WebServiceWorkerEventResult)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerEventResult,
                          blink::WebServiceWorkerEventResultLast)

// ---------------------------------------------------------------------------

namespace content {

namespace {

class MessageReceiver : public EmbeddedWorkerTestHelper {
 public:
  MessageReceiver()
      : EmbeddedWorkerTestHelper(base::FilePath()),
        current_embedded_worker_id_(0) {}
  ~MessageReceiver() override {}

  bool OnMessageToWorker(int thread_id,
                         int embedded_worker_id,
                         const IPC::Message& message) override {
    if (EmbeddedWorkerTestHelper::OnMessageToWorker(
            thread_id, embedded_worker_id, message)) {
      return true;
    }
    current_embedded_worker_id_ = embedded_worker_id;
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageReceiver, message)
      IPC_MESSAGE_HANDLER(TestMsg_Message, OnMessage)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void SimulateSendValueToBrowser(int embedded_worker_id, int value) {
    SimulateSend(new TestMsg_MessageFromWorker(embedded_worker_id, value));
  }

  void SimulateSendEventResult(int embedded_worker_id,
                               int request_id,
                               const std::string& reply) {
    SimulateSend(
        new TestMsg_TestEventResult(embedded_worker_id, request_id, reply));
  }

  void SimulateSendSimpleEventResult(int embedded_worker_id,
                                     int request_id,
                                     blink::WebServiceWorkerEventResult reply) {
    SimulateSend(new TestMsg_TestSimpleEventResult(embedded_worker_id,
                                                   request_id, reply));
  }

 private:
  void OnMessage() {
    // Do nothing.
  }

  int current_embedded_worker_id_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiver);
};

void VerifyCalled(bool* called) {
  *called = true;
}

void ObserveStatusChanges(ServiceWorkerVersion* version,
                          std::vector<ServiceWorkerVersion::Status>* statuses) {
  statuses->push_back(version->status());
  version->RegisterStatusChangeCallback(
      base::Bind(&ObserveStatusChanges, base::Unretained(version), statuses));
}

void ReceiveTestEventResult(int* request_id,
                            std::string* data,
                            const base::Closure& callback,
                            int actual_request_id,
                            const std::string& actual_data) {
  *request_id = actual_request_id;
  *data = actual_data;
  callback.Run();
}

// A specialized listener class to receive test messages from a worker.
class MessageReceiverFromWorker : public EmbeddedWorkerInstance::Listener {
 public:
  explicit MessageReceiverFromWorker(EmbeddedWorkerInstance* instance)
      : instance_(instance) {
    instance_->AddListener(this);
  }
  ~MessageReceiverFromWorker() override { instance_->RemoveListener(this); }

  void OnStarted() override { NOTREACHED(); }
  void OnStopped(EmbeddedWorkerStatus old_status) override { NOTREACHED(); }
  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageReceiverFromWorker, message)
      IPC_MESSAGE_HANDLER(TestMsg_MessageFromWorker, OnMessageFromWorker)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnMessageFromWorker(int value) { received_values_.push_back(value); }
  const std::vector<int>& received_values() const { return received_values_; }

 private:
  EmbeddedWorkerInstance* instance_;
  std::vector<int> received_values_;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverFromWorker);
};

base::Time GetYesterday() {
  return base::Time::Now() - base::TimeDelta::FromDays(1) -
         base::TimeDelta::FromSeconds(1);
}

class TestMojoServiceImpl : public mojom::TestMojoService {
 public:
  static void Create(mojo::InterfaceRequest<mojom::TestMojoService> request) {
    new TestMojoServiceImpl(std::move(request));
  }

  void DoSomething(const DoSomethingCallback& callback) override {
    callback.Run();
  }

  void DoTerminateProcess(const DoTerminateProcessCallback& callback) override {
    NOTREACHED();
  }

  void CreateFolder(const CreateFolderCallback& callback) override {
    NOTREACHED();
  }

  void GetRequestorName(const GetRequestorNameCallback& callback) override {
    callback.Run(mojo::String(""));
  }

 private:
  explicit TestMojoServiceImpl(
      mojo::InterfaceRequest<mojom::TestMojoService> request)
      : binding_(this, std::move(request)) {}

  mojo::StrongBinding<mojom::TestMojoService> binding_;
};

}  // namespace

class ServiceWorkerVersionTest : public testing::Test {
 protected:
  struct RunningStateListener : public ServiceWorkerVersion::Listener {
    RunningStateListener() : last_status(EmbeddedWorkerStatus::STOPPED) {}
    ~RunningStateListener() override {}
    void OnRunningStateChanged(ServiceWorkerVersion* version) override {
      last_status = version->running_status();
    }
    EmbeddedWorkerStatus last_status;
  };

  ServiceWorkerVersionTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = GetMessageReceiver();

    helper_->context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();

    pattern_ = GURL("http://www.example.com/test/");
    registration_ = new ServiceWorkerRegistration(
        pattern_,
        1L,
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(),
        GURL("http://www.example.com/test/service_worker.js"),
        helper_->context()->storage()->NewVersionId(),
        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);

    // Make the registration findable via storage functions.
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(),
        version_.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    // Simulate adding one process to the pattern.
    helper_->SimulateAddProcessToPattern(pattern_,
                                         helper_->mock_render_process_id());
    ASSERT_TRUE(helper_->context()->process_manager()
        ->PatternHasProcessToRun(pattern_));
  }

  virtual std::unique_ptr<MessageReceiver> GetMessageReceiver() {
    return base::WrapUnique(new MessageReceiver());
  }

  void TearDown() override {
    version_ = 0;
    registration_ = 0;
    helper_.reset();
  }

  void SimulateDispatchEvent(ServiceWorkerMetrics::EventType event_type) {
    ServiceWorkerStatusCode status =
        SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value

    // Make sure worker is running.
    scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
    version_->RunAfterStartWorker(event_type, runner->QuitClosure(),
                                  CreateReceiverOnCurrentThread(&status));
    runner->Run();
    EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

    // Start request, as if an event is being dispatched.
    int request_id = version_->StartRequest(
        event_type, CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();

    // And finish request, as if a response to the event was received.
    EXPECT_TRUE(version_->FinishRequest(request_id, true));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);
  }

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<MessageReceiver> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  GURL pattern_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersionTest);
};

class MessageReceiverDisallowStart : public MessageReceiver {
 public:
  MessageReceiverDisallowStart()
      : MessageReceiver() {}
  ~MessageReceiverDisallowStart() override {}

  enum class StartMode { STALL, FAIL, SUCCEED };

  void OnStartWorker(int embedded_worker_id,
                     int64_t service_worker_version_id,
                     const GURL& scope,
                     const GURL& script_url,
                     bool pause_after_download) override {
    switch (mode_) {
      case StartMode::STALL:
        break;  // Do nothing.
      case StartMode::FAIL:
        OnStopWorker(embedded_worker_id);
        break;
      case StartMode::SUCCEED:
        MessageReceiver::OnStartWorker(embedded_worker_id,
                                       service_worker_version_id, scope,
                                       script_url, pause_after_download);
        break;
    }
  }

  void set_start_mode(StartMode mode) { mode_ = mode; }

 private:
  StartMode mode_ = StartMode::STALL;
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverDisallowStart);
};

class ServiceWorkerFailToStartTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerFailToStartTest()
      : ServiceWorkerVersionTest() {}

  void set_start_mode(MessageReceiverDisallowStart::StartMode mode) {
    MessageReceiverDisallowStart* helper =
        static_cast<MessageReceiverDisallowStart*>(helper_.get());
    helper->set_start_mode(mode);
  }

  std::unique_ptr<MessageReceiver> GetMessageReceiver() override {
    return base::WrapUnique(new MessageReceiverDisallowStart());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFailToStartTest);
};

class MessageReceiverDisallowStop : public MessageReceiver {
 public:
  MessageReceiverDisallowStop() : MessageReceiver() {}
  ~MessageReceiverDisallowStop() override {}

  void OnStopWorker(int embedded_worker_id) override {
    // Do nothing.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverDisallowStop);
};

class ServiceWorkerStallInStoppingTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerStallInStoppingTest() : ServiceWorkerVersionTest() {}

  std::unique_ptr<MessageReceiver> GetMessageReceiver() override {
    return base::WrapUnique(new MessageReceiverDisallowStop());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStallInStoppingTest);
};

class MessageReceiverMojoTestService : public MessageReceiver {
 public:
  MessageReceiverMojoTestService() : MessageReceiver() {}
  ~MessageReceiverMojoTestService() override {}

  void OnSetupMojo(shell::InterfaceRegistry* registry) override {
    registry->AddInterface(base::Bind(&TestMojoServiceImpl::Create));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageReceiverMojoTestService);
};

class ServiceWorkerVersionWithMojoTest : public ServiceWorkerVersionTest {
 protected:
  ServiceWorkerVersionWithMojoTest() : ServiceWorkerVersionTest() {}

  std::unique_ptr<MessageReceiver> GetMessageReceiver() override {
    return base::WrapUnique(new MessageReceiverMojoTestService());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersionWithMojoTest);
};

TEST_F(ServiceWorkerVersionTest, ConcurrentStartAndStop) {
  // Call StartWorker() multiple times.
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_ERROR_FAILED;
  ServiceWorkerStatusCode status3 = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status1));
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status2));

  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Call StartWorker() after it's started.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status3));
  base::RunLoop().RunUntilIdle();

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);

  // Call StopWorker() multiple times.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status1));
  version_->StopWorker(CreateReceiverOnCurrentThread(&status2));

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // All StopWorker should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);

  // Start worker again.
  status1 = SERVICE_WORKER_ERROR_FAILED;
  status2 = SERVICE_WORKER_ERROR_FAILED;
  status3 = SERVICE_WORKER_ERROR_FAILED;

  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status1));

  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Call StopWorker()
  status2 = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status2));

  // And try calling StartWorker while StopWorker is in queue.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status3));

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // All should just succeed.
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_EQ(SERVICE_WORKER_OK, status3);
}

TEST_F(ServiceWorkerVersionTest, DispatchEventToStoppedWorker) {
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Dispatch an event without starting the worker.
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  // The worker should be now started.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Stop the worker, and then dispatch an event immediately after that.
  ServiceWorkerStatusCode stop_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&stop_status));
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);
  EXPECT_EQ(SERVICE_WORKER_OK, stop_status);

  // The worker should be now started again.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, StartUnregisteredButStillLiveWorker) {
  // Start the worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Delete the registration.
  status = SERVICE_WORKER_ERROR_FAILED;
  helper_->context()->storage()->DeleteRegistration(
      registration_->id(), registration_->pattern().GetOrigin(),
      CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  // The live registration is marked as deleted, but still exists.
  ASSERT_TRUE(registration_->is_deleted());

  // Stop the worker.
  ServiceWorkerStatusCode stop_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&stop_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, stop_status);

  // Dispatch an event on the unregistered and stopped but still live worker.
  status = SERVICE_WORKER_ERROR_FAILED;
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME);

  // The worker should be now started again.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, ReceiveMessageFromWorker) {
  // Start worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  MessageReceiverFromWorker receiver(version_->embedded_worker());

  // Simulate sending some dummy values from the worker.
  helper_->SimulateSendValueToBrowser(
      version_->embedded_worker()->embedded_worker_id(), 555);
  helper_->SimulateSendValueToBrowser(
      version_->embedded_worker()->embedded_worker_id(), 777);

  // Verify the receiver received the values.
  ASSERT_EQ(2U, receiver.received_values().size());
  EXPECT_EQ(555, receiver.received_values()[0]);
  EXPECT_EQ(777, receiver.received_values()[1]);
}

TEST_F(ServiceWorkerVersionTest, InstallAndWaitCompletion) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  // Dispatch an install event.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  // Version's status must not have changed during installation.
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, ActivateAndWaitCompletion) {
  // TODO(mek): This test (and the one above for the install event) made more
  // sense back when ServiceWorkerVersion was responsible for updating the
  // status. Now a better version of this test should probably be added to
  // ServiceWorkerRegistrationTest instead.

  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(
      base::Bind(&VerifyCalled, &status_change_called));

  // Dispatch an activate event.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::ACTIVATE);

  // Version's status must not have changed during activation.
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
}

TEST_F(ServiceWorkerVersionTest, RepeatedlyObserveStatusChanges) {
  EXPECT_EQ(ServiceWorkerVersion::NEW, version_->status());

  // Repeatedly observe status changes (the callback re-registers itself).
  std::vector<ServiceWorkerVersion::Status> statuses;
  version_->RegisterStatusChangeCallback(base::Bind(
      &ObserveStatusChanges, base::RetainedRef(version_), &statuses));

  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->SetStatus(ServiceWorkerVersion::REDUNDANT);

  // Verify that we could successfully observe repeated status changes.
  ASSERT_EQ(5U, statuses.size());
  ASSERT_EQ(ServiceWorkerVersion::INSTALLING, statuses[0]);
  ASSERT_EQ(ServiceWorkerVersion::INSTALLED, statuses[1]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, statuses[2]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATED, statuses[3]);
  ASSERT_EQ(ServiceWorkerVersion::REDUNDANT, statuses[4]);
}

TEST_F(ServiceWorkerVersionTest, IdleTimeout) {
  // Used to reliably test when the idle time gets reset regardless of clock
  // granularity.
  const base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);

  // Verify the timer is not running when version initializes its status.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  EXPECT_FALSE(version_->timeout_timer_.IsRunning());

  // Verify the timer is running after the worker is started.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_FALSE(version_->idle_time_.is_null());

  // The idle time should be reset if the worker is restarted without
  // controllee.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->idle_time_ -= kOneSecond;
  base::TimeTicks idle_time = version_->idle_time_;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_LT(idle_time, version_->idle_time_);

  // Adding a controllee resets the idle time.
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  std::unique_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      33 /* dummy render process id */, MSG_ROUTING_NONE /* render_frame_id */,
      1 /* dummy provider_id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
      helper_->context()->AsWeakPtr(), NULL));
  version_->AddControllee(host.get());
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_LT(idle_time, version_->idle_time_);

  // Completing an event resets the idle time.
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME);
  EXPECT_LT(idle_time, version_->idle_time_);

  // Starting and finishing a request resets the idle time.
  version_->idle_time_ -= kOneSecond;
  idle_time = version_->idle_time_;
  int request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::SYNC,
                             CreateReceiverOnCurrentThread(&status));
  EXPECT_TRUE(version_->FinishRequest(request_id, true));

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_LT(idle_time, version_->idle_time_);
}

TEST_F(ServiceWorkerVersionTest, SetDevToolsAttached) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));

  ASSERT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  ASSERT_TRUE(version_->timeout_timer_.IsRunning());
  ASSERT_FALSE(version_->start_time_.is_null());
  ASSERT_FALSE(version_->skip_recording_startup_time_);

  // Simulate DevTools is attached. This should deactivate the timer for start
  // timeout, but not stop the timer itself.
  version_->SetDevToolsAttached(true);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_TRUE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  // Simulate DevTools is detached. This should reactivate the timer for start
  // timeout.
  version_->SetDevToolsAttached(false);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_FALSE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, StoppingBeforeDestruct) {
  RunningStateListener listener;
  version_->AddListener(&listener);

  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, listener.last_status);

  version_ = nullptr;
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, listener.last_status);
}

// Test that update isn't triggered for a non-stale worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_FreshWorker) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);

  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that update isn't triggered for a non-active worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_NonActiveWorker) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  registration_->SetInstallingVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected when starting a worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_StartWorker) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;

  // Starting the worker marks it as stale.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Update is actually scheduled after the worker stops.
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected on a running worker.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_RunningWorker) {
  // Start a fresh worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  EXPECT_TRUE(version_->stale_time_.is_null());

  // Simulate it running for a day. It will be marked stale.
  registration_->set_last_update_check(GetYesterday());
  version_->OnTimeoutTimer();
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Simulate it running for past the wait threshold. The update will be
  // scheduled.
  version_->stale_time_ =
      base::TimeTicks::Now() -
      base::TimeDelta::FromMinutes(
          ServiceWorkerVersion::kStartNewWorkerTimeoutMinutes + 1);
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that a stream of events doesn't restart the timer.
TEST_F(ServiceWorkerVersionTest, StaleUpdate_DoNotDeferTimer) {
  // Make a stale worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  base::TimeTicks stale_time =
      base::TimeTicks::Now() -
      base::TimeDelta::FromMinutes(
          ServiceWorkerVersion::kStartNewWorkerTimeoutMinutes + 1);
  version_->stale_time_ = stale_time;

  // Stale time is not deferred.
  version_->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN, base::Bind(&base::DoNothing),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  version_->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN, base::Bind(&base::DoNothing),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stale_time, version_->stale_time_);

  // Timeout triggers the update.
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());

  // Update timer is not deferred.
  base::TimeTicks run_time = version_->update_timer_.desired_run_time();
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_EQ(run_time, version_->update_timer_.desired_run_time());
}

TEST_F(ServiceWorkerVersionTest, RequestTimeout) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a request.
  int request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                             CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Simulate timeout.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->SetAllRequestExpirations(base::TimeTicks::Now());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
  EXPECT_FALSE(version_->FinishRequest(request_id, true));
}

TEST_F(ServiceWorkerVersionTest, RequestCustomizedTimeout) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a request that should expire Now().
  int request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status), base::TimeDelta(),
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);

  EXPECT_FALSE(version_->FinishRequest(request_id, true));

  // CONTINUE_ON_TIMEOUT timeouts don't stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, RequestCustomizedTimeoutKill) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a request that should expire Now().
  int request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status), base::TimeDelta(),
      ServiceWorkerVersion::KILL_ON_TIMEOUT);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);

  EXPECT_FALSE(version_->FinishRequest(request_id, true));

  // KILL_ON_TIMEOUT timeouts should stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, MixedRequestTimeouts) {
  ServiceWorkerStatusCode sync_status =
      SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  ServiceWorkerStatusCode fetch_status =
      SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  version_->StartWorker(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Create a fetch request that should expire sometime later.
  int fetch_request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
                             CreateReceiverOnCurrentThread(&fetch_status));
  // Create a request that should expire Now().
  int sync_request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&sync_status), base::TimeDelta(),
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, sync_status);

  // Verify the sync has timed out but not the fetch.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, sync_status);
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, fetch_status);

  // Background sync timeouts don't stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Gracefully handle the sync event finishing after the timeout.
  EXPECT_FALSE(version_->FinishRequest(sync_request_id, true));

  // Verify that the fetch times out later.
  version_->SetAllRequestExpirations(base::TimeTicks::Now());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, fetch_status);

  // Fetch request should no longer exist.
  EXPECT_FALSE(version_->FinishRequest(fetch_request_id, true));

  // Other timeouts do stop the service worker.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerFailToStartTest, RendererCrash) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  // Simulate renderer crash: do what
  // ServiceWorkerDispatcherHost::OnFilterRemoved does.
  int process_id = helper_->mock_render_process_id();
  helper_->context()->RemoveAllProviderHostsForProcess(process_id);
  helper_->context()->embedded_worker_registry()->RemoveChildProcessSender(
      process_id);
  base::RunLoop().RunUntilIdle();

  // Callback completed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

TEST_F(ServiceWorkerFailToStartTest, Timeout) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  // Start starting the worker.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NETWORK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, version_->running_status());

  // Simulate timeout.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->start_time_ =
      base::TimeTicks::Now() -
      base::TimeDelta::FromMinutes(
          ServiceWorkerVersion::kStartNewWorkerTimeoutMinutes + 1);
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_TIMEOUT, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());
}

// Test that a service worker stalled in stopping will timeout and not get in a
// sticky error state.
TEST_F(ServiceWorkerStallInStoppingTest, DetachThenStart) {
  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Try to stop the worker.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());
  base::RunLoop().RunUntilIdle();

  // Worker is now stalled in stopping. Verify a fast timeout is in place.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_EQ(base::TimeDelta::FromSeconds(
                ServiceWorkerVersion::kStopWorkerTimeoutSeconds),
            version_->timeout_timer_.GetCurrentDelay());

  // Simulate timeout.
  version_->stop_time_ =
      base::TimeTicks::Now() -
      base::TimeDelta::FromSeconds(
          ServiceWorkerVersion::kStopWorkerTimeoutSeconds + 1);
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Try to start the worker again. It should work.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // The timeout interval should be reset to normal.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_EQ(base::TimeDelta::FromSeconds(
                ServiceWorkerVersion::kTimeoutTimerDelaySeconds),
            version_->timeout_timer_.GetCurrentDelay());
}

// Test that a service worker stalled in stopping with a start worker
// request queued up will timeout and restart.
TEST_F(ServiceWorkerStallInStoppingTest, DetachThenRestart) {
  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Try to stop the worker.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());

  // Worker is now stalled in stopping. Add a start worker requset.
  ServiceWorkerStatusCode start_status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&start_status));

  // Simulate timeout. The worker should stop and get restarted.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->stop_time_ =
      base::TimeTicks::Now() -
      base::TimeDelta::FromSeconds(
          ServiceWorkerVersion::kStopWorkerTimeoutSeconds + 1);
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(SERVICE_WORKER_OK, start_status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
}

TEST_F(ServiceWorkerVersionTest, RegisterForeignFetchScopes) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  // Start a worker.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());
  EXPECT_EQ(0, helper_->mock_render_process_host()->bad_msg_count());

  GURL valid_scope_1("http://www.example.com/test/subscope");
  GURL valid_scope_2("http://www.example.com/test/othersubscope");
  std::vector<GURL> valid_scopes;
  valid_scopes.push_back(valid_scope_1);
  valid_scopes.push_back(valid_scope_2);

  std::vector<url::Origin> all_origins;
  url::Origin valid_origin(GURL("https://chromium.org/"));
  std::vector<url::Origin> valid_origin_list(1, valid_origin);

  // Invalid subscope, should kill worker (but in tests will only increase bad
  // message count).
  version_->OnRegisterForeignFetchScopes(std::vector<GURL>(1, GURL()),
                                         valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Subscope outside the scope of the worker.
  version_->OnRegisterForeignFetchScopes(
      std::vector<GURL>(1, GURL("http://www.example.com/wrong")),
      valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Subscope on wrong origin.
  version_->OnRegisterForeignFetchScopes(
      std::vector<GURL>(1, GURL("http://example.com/test/")),
      valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Invalid origin.
  version_->OnRegisterForeignFetchScopes(
      valid_scopes, std::vector<url::Origin>(1, url::Origin()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(0u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Valid subscope, no origins.
  version_->OnRegisterForeignFetchScopes(std::vector<GURL>(1, valid_scope_1),
                                         all_origins);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(1u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(valid_scope_1, version_->foreign_fetch_scopes_[0]);
  EXPECT_EQ(0u, version_->foreign_fetch_origins_.size());

  // Valid subscope, explicit origins.
  version_->OnRegisterForeignFetchScopes(valid_scopes, valid_origin_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, helper_->mock_render_process_host()->bad_msg_count());
  EXPECT_EQ(2u, version_->foreign_fetch_scopes_.size());
  EXPECT_EQ(valid_scope_1, version_->foreign_fetch_scopes_[0]);
  EXPECT_EQ(valid_scope_2, version_->foreign_fetch_scopes_[1]);
  EXPECT_EQ(1u, version_->foreign_fetch_origins_.size());
  EXPECT_EQ(valid_origin, version_->foreign_fetch_origins_[0]);
}

TEST_F(ServiceWorkerVersionTest, RendererCrashDuringEvent) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  int request_id =
      version_->StartRequest(ServiceWorkerMetrics::EventType::SYNC,
                             CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // Simulate renderer crash: do what
  // ServiceWorkerDispatcherHost::OnFilterRemoved does.
  int process_id = helper_->mock_render_process_id();
  helper_->context()->RemoveAllProviderHostsForProcess(process_id);
  helper_->context()->embedded_worker_registry()->RemoveChildProcessSender(
      process_id);
  base::RunLoop().RunUntilIdle();

  // Callback completed.
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED, status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // Request already failed, calling finsh should return false.
  EXPECT_FALSE(version_->FinishRequest(request_id, true));
}

TEST_F(ServiceWorkerVersionWithMojoTest, MojoService) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
  int request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status, runner->QuitClosure()));
  base::WeakPtr<mojom::TestMojoService> service =
      version_->GetMojoServiceForRequest<mojom::TestMojoService>(request_id);
  service->DoSomething(runner->QuitClosure());
  runner->Run();

  // Mojo service does exist in worker, so error callback should not have been
  // called and FinishRequest should return true.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->FinishRequest(request_id, true));
}

TEST_F(ServiceWorkerVersionTest, NonExistentMojoService) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
  int request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status, runner->QuitClosure()));
  base::WeakPtr<mojom::TestMojoService> service =
      version_->GetMojoServiceForRequest<mojom::TestMojoService>(request_id);
  service->DoSomething(runner->QuitClosure());
  runner->Run();

  // Mojo service doesn't exist in worker, so error callback should have been
  // called and FinishRequest should return false.
  EXPECT_EQ(SERVICE_WORKER_ERROR_FAILED, status);
  EXPECT_FALSE(version_->FinishRequest(request_id, true));
}

TEST_F(ServiceWorkerVersionTest, DispatchEvent) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  // Activate and start worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Start request and dispatch test event.
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
  int request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status, runner->QuitClosure()));
  int received_request_id = 0;
  std::string received_data;
  version_->RegisterRequestCallback<TestMsg_TestEventResult>(
      request_id, base::Bind(&ReceiveTestEventResult, &received_request_id,
                             &received_data, runner->QuitClosure()));
  version_->DispatchEvent({request_id}, TestMsg_TestEvent(request_id));

  // Verify event got dispatched to worker.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, helper_->inner_ipc_sink()->message_count());
  const IPC::Message* msg = helper_->inner_ipc_sink()->GetMessageAt(0);
  EXPECT_EQ(TestMsg_TestEvent::ID, msg->type());

  // Simulate sending reply to event.
  std::string reply("foobar");
  helper_->SimulateSendEventResult(
      version_->embedded_worker()->embedded_worker_id(), request_id, reply);
  runner->Run();

  // Verify message callback got called with correct reply.
  EXPECT_EQ(request_id, received_request_id);
  EXPECT_EQ(reply, received_data);

  // Should not have timed out, so error callback should not have been
  // called and FinishRequest should return true.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->FinishRequest(request_id, true));
}

TEST_F(ServiceWorkerFailToStartTest, FailingWorkerUsesNewRendererProcess) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;

  helper_->SimulateAddProcessToPattern(pattern_,
                                       helper_->new_render_process_id());
  ServiceWorkerContextCore* context = helper_->context();
  int64_t id = version_->version_id();
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Start once. It should choose the "existing process".
  set_start_mode(MessageReceiverDisallowStart::StartMode::SUCCEED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(helper_->mock_render_process_id(),
            version_->embedded_worker()->process_id());
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Fail once.
  set_start_mode(MessageReceiverDisallowStart::StartMode::FAIL);
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(1, context->GetVersionFailureCount(id));

  // Fail again.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);
  EXPECT_EQ(2, context->GetVersionFailureCount(id));

  // Succeed. It should choose the "new process".
  set_start_mode(MessageReceiverDisallowStart::StartMode::SUCCEED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(helper_->new_render_process_id(),
            version_->embedded_worker()->process_id());
  EXPECT_EQ(0, context->GetVersionFailureCount(id));
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();

  // Start again. It should choose the "existing process" again as we no longer
  // force creation of a new process.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(helper_->mock_render_process_id(),
            version_->embedded_worker()->process_id());
  version_->StopWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ServiceWorkerVersionTest, DispatchConcurrentEvent) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NETWORK;  // dummy value

  // Activate and start worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Start first request and dispatch test event.
  scoped_refptr<MessageLoopRunner> runner1(new MessageLoopRunner);
  ServiceWorkerStatusCode status1 = SERVICE_WORKER_OK;  // dummy value
  int request_id1 = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status1, runner1->QuitClosure()));
  int received_request_id1 = 0;
  std::string received_data1;
  version_->RegisterRequestCallback<TestMsg_TestEventResult>(
      request_id1, base::Bind(&ReceiveTestEventResult, &received_request_id1,
                              &received_data1, runner1->QuitClosure()));
  version_->DispatchEvent({request_id1}, TestMsg_TestEvent(request_id1));

  // Start second request and dispatch test event.
  scoped_refptr<MessageLoopRunner> runner2(new MessageLoopRunner);
  ServiceWorkerStatusCode status2 = SERVICE_WORKER_OK;  // dummy value
  int request_id2 = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status2, runner2->QuitClosure()));
  int received_request_id2 = 0;
  std::string received_data2;
  version_->RegisterRequestCallback<TestMsg_TestEventResult>(
      request_id2, base::Bind(&ReceiveTestEventResult, &received_request_id2,
                              &received_data2, runner2->QuitClosure()));
  version_->DispatchEvent({request_id2}, TestMsg_TestEvent(request_id2));

  // Make sure events got dispatched in same order.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, helper_->inner_ipc_sink()->message_count());
  const IPC::Message* msg = helper_->inner_ipc_sink()->GetMessageAt(0);
  ASSERT_EQ(TestMsg_TestEvent::ID, msg->type());
  TestMsg_TestEvent::Param params;
  TestMsg_TestEvent::Read(msg, &params);
  EXPECT_EQ(request_id1, std::get<0>(params));
  msg = helper_->inner_ipc_sink()->GetMessageAt(1);
  ASSERT_EQ(TestMsg_TestEvent::ID, msg->type());
  TestMsg_TestEvent::Read(msg, &params);
  EXPECT_EQ(request_id2, std::get<0>(params));

  // Reply to second event.
  std::string reply2("foobar");
  helper_->SimulateSendEventResult(
      version_->embedded_worker()->embedded_worker_id(), request_id2, reply2);
  runner2->Run();

  // Verify correct message callback got called with correct reply.
  EXPECT_EQ(0, received_request_id1);
  EXPECT_EQ(request_id2, received_request_id2);
  EXPECT_EQ(reply2, received_data2);
  EXPECT_EQ(SERVICE_WORKER_OK, status2);
  EXPECT_TRUE(version_->FinishRequest(request_id2, true));

  // Reply to first event.
  std::string reply1("hello world");
  helper_->SimulateSendEventResult(
      version_->embedded_worker()->embedded_worker_id(), request_id1, reply1);
  runner1->Run();

  // Verify correct response was received.
  EXPECT_EQ(request_id1, received_request_id1);
  EXPECT_EQ(request_id2, received_request_id2);
  EXPECT_EQ(reply1, received_data1);
  EXPECT_EQ(SERVICE_WORKER_OK, status1);
  EXPECT_TRUE(version_->FinishRequest(request_id1, true));
}

TEST_F(ServiceWorkerVersionTest, DispatchSimpleEvent_Completed) {
  ServiceWorkerStatusCode status =
      SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value

  // Activate and start worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Start request and dispatch test event.
  status = SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
  int request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status, runner->QuitClosure()));
  version_->DispatchSimpleEvent<TestMsg_TestSimpleEventResult>(
      request_id, TestMsg_TestEvent(request_id));

  // Verify event got dispatched to worker.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, helper_->inner_ipc_sink()->message_count());
  const IPC::Message* msg = helper_->inner_ipc_sink()->GetMessageAt(0);
  EXPECT_EQ(TestMsg_TestEvent::ID, msg->type());

  // Simulate sending reply to event.
  helper_->SimulateSendSimpleEventResult(
      version_->embedded_worker()->embedded_worker_id(), request_id,
      blink::WebServiceWorkerEventResultCompleted);
  runner->Run();

  // Verify callback was called with correct status.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
}

TEST_F(ServiceWorkerVersionTest, DispatchSimpleEvent_Rejected) {
  ServiceWorkerStatusCode status =
      SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value

  // Activate and start worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::SYNC,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Start request and dispatch test event.
  status = SERVICE_WORKER_ERROR_MAX_VALUE;  // dummy value
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
  int request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      CreateReceiverOnCurrentThread(&status, runner->QuitClosure()));
  version_->DispatchSimpleEvent<TestMsg_TestSimpleEventResult>(
      request_id, TestMsg_TestEvent(request_id));

  // Verify event got dispatched to worker.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, helper_->inner_ipc_sink()->message_count());
  const IPC::Message* msg = helper_->inner_ipc_sink()->GetMessageAt(0);
  EXPECT_EQ(TestMsg_TestEvent::ID, msg->type());

  // Simulate sending reply to event.
  helper_->SimulateSendSimpleEventResult(
      version_->embedded_worker()->embedded_worker_id(), request_id,
      blink::WebServiceWorkerEventResultRejected);
  runner->Run();

  // Verify callback was called with correct status.
  EXPECT_EQ(SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED, status);
}

TEST_F(ServiceWorkerVersionTest, DispatchEvent_MultipleResponse) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;

  // Activate and start worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Start request and dispatch test event.
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner);
  int request_id1 = version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
      CreateReceiverOnCurrentThread(&status, runner->QuitClosure()));
  int request_id2 = version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL,
      CreateReceiverOnCurrentThread(&status, runner->QuitClosure()));
  int received_request_id1 = 0;
  int received_request_id2 = 0;
  std::string received_data1;
  std::string received_data2;
  version_->RegisterRequestCallback<TestMsg_TestEventResult>(
      request_id1, base::Bind(&ReceiveTestEventResult, &received_request_id1,
                              &received_data1, runner->QuitClosure()));
  version_->RegisterRequestCallback<TestMsg_TestEventResult>(
      request_id2, base::Bind(&ReceiveTestEventResult, &received_request_id2,
                              &received_data2, runner->QuitClosure()));
  version_->DispatchEvent({request_id1, request_id2},
                          TestMsg_TestEvent_Multiple(request_id1, request_id2));

  // Verify event got dispatched to worker.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, helper_->inner_ipc_sink()->message_count());
  const IPC::Message* msg = helper_->inner_ipc_sink()->GetMessageAt(0);
  EXPECT_EQ(TestMsg_TestEvent_Multiple::ID, msg->type());

  // Simulate sending reply to event.
  std::string reply1("foobar1");
  std::string reply2("foobar2");
  helper_->SimulateSendEventResult(
      version_->embedded_worker()->embedded_worker_id(), request_id1, reply1);
  runner->Run();

  // Verify message callback got called with correct reply.
  EXPECT_EQ(request_id1, received_request_id1);
  EXPECT_EQ(reply1, received_data1);
  EXPECT_NE(request_id2, received_request_id2);
  EXPECT_NE(reply2, received_data2);

  // Simulate sending reply to event.
  helper_->SimulateSendEventResult(
      version_->embedded_worker()->embedded_worker_id(), request_id2, reply2);
  runner->Run();

  // Verify message callback got called with correct reply.
  EXPECT_EQ(request_id2, received_request_id2);
  EXPECT_EQ(reply2, received_data2);

  // Should not have timed out, so error callback should not have been
  // called and FinishRequest should return true.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(version_->FinishRequest(request_id1, true));
  EXPECT_TRUE(version_->FinishRequest(request_id2, true));
}

}  // namespace content

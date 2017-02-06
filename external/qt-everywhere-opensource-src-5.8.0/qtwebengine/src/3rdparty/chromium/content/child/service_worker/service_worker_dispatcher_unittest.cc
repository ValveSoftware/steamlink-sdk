// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"

namespace content {

namespace {

class ServiceWorkerTestSender : public ThreadSafeSender {
 public:
  explicit ServiceWorkerTestSender(IPC::TestSink* ipc_sink)
      : ThreadSafeSender(nullptr, nullptr),
        ipc_sink_(ipc_sink) {}

  bool Send(IPC::Message* message) override {
    return ipc_sink_->Send(message);
  }

 private:
  ~ServiceWorkerTestSender() override {}

  IPC::TestSink* ipc_sink_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTestSender);
};

}  // namespace

class ServiceWorkerDispatcherTest : public testing::Test {
 public:
  ServiceWorkerDispatcherTest() {}

  void SetUp() override {
    sender_ = new ServiceWorkerTestSender(&ipc_sink_);
    dispatcher_.reset(new ServiceWorkerDispatcher(sender_.get(), nullptr));
  }

  void CreateObjectInfoAndVersionAttributes(
      ServiceWorkerRegistrationObjectInfo* info,
      ServiceWorkerVersionAttributes* attrs) {
    info->handle_id = 10;
    info->registration_id = 20;

    attrs->active.handle_id = 100;
    attrs->active.version_id = 200;
    attrs->waiting.handle_id = 101;
    attrs->waiting.version_id = 201;
    attrs->installing.handle_id = 102;
    attrs->installing.version_id = 202;
  }

  bool ContainsServiceWorker(int handle_id) {
    return ContainsKey(dispatcher_->service_workers_, handle_id);
  }

  bool ContainsRegistration(int registration_handle_id) {
    return ContainsKey(dispatcher_->registrations_, registration_handle_id);
  }

  void OnAssociateRegistration(int thread_id,
                               int provider_id,
                               const ServiceWorkerRegistrationObjectInfo& info,
                               const ServiceWorkerVersionAttributes& attrs) {
    dispatcher_->OnAssociateRegistration(thread_id, provider_id, info, attrs);
  }

  void OnDisassociateRegistration(int thread_id, int provider_id) {
    dispatcher_->OnDisassociateRegistration(thread_id, provider_id);
  }

  void OnSetControllerServiceWorker(int thread_id,
                                    int provider_id,
                                    const ServiceWorkerObjectInfo& info,
                                    bool should_notify_controllerchange) {
    dispatcher_->OnSetControllerServiceWorker(thread_id, provider_id, info,
                                              should_notify_controllerchange);
  }

  void OnPostMessage(const ServiceWorkerMsg_MessageToDocument_Params& params) {
    dispatcher_->OnPostMessage(params);
  }

  std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      const ServiceWorkerObjectInfo& info) {
    return dispatcher_->Adopt(info);
  }

  ServiceWorkerDispatcher* dispatcher() { return dispatcher_.get(); }
  ThreadSafeSender* thread_safe_sender() { return sender_.get(); }
  IPC::TestSink* ipc_sink() { return &ipc_sink_; }

 private:
  base::MessageLoop message_loop_;
  IPC::TestSink ipc_sink_;
  std::unique_ptr<ServiceWorkerDispatcher> dispatcher_;
  scoped_refptr<ServiceWorkerTestSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherTest);
};

class MockWebServiceWorkerProviderClientImpl
    : public blink::WebServiceWorkerProviderClient {
 public:
  MockWebServiceWorkerProviderClientImpl(int provider_id,
                                         ServiceWorkerDispatcher* dispatcher)
      : provider_id_(provider_id), dispatcher_(dispatcher) {
    dispatcher_->AddProviderClient(provider_id, this);
  }

  ~MockWebServiceWorkerProviderClientImpl() override {
    dispatcher_->RemoveProviderClient(provider_id_);
  }

  void setController(std::unique_ptr<blink::WebServiceWorker::Handle> handle,
                     bool shouldNotifyControllerChange) override {
    // WebPassOwnPtr cannot be owned in Chromium, so drop the handle here.
    // The destruction releases ServiceWorkerHandleReference.
    is_set_controlled_called_ = true;
  }

  void dispatchMessageEvent(
      std::unique_ptr<blink::WebServiceWorker::Handle> handle,
      const blink::WebString& message,
      const blink::WebMessagePortChannelArray& channels) override {
    // WebPassOwnPtr cannot be owned in Chromium, so drop the handle here.
    // The destruction releases ServiceWorkerHandleReference.
    is_dispatch_message_event_called_ = true;
  }

  bool is_set_controlled_called() const { return is_set_controlled_called_; }

  bool is_dispatch_message_event_called() const {
    return is_dispatch_message_event_called_;
  }

 private:
  const int provider_id_;
  bool is_set_controlled_called_ = false;
  bool is_dispatch_message_event_called_ = false;
  ServiceWorkerDispatcher* dispatcher_;
};

TEST_F(ServiceWorkerDispatcherTest, OnAssociateRegistration_NoProviderContext) {
  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // The passed references should be adopted but immediately released because
  // there is no provider context to own the references.
  const int kProviderId = 10;
  OnAssociateRegistration(kDocumentMainThreadId, kProviderId, info, attrs);
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());
}

TEST_F(ServiceWorkerDispatcherTest,
       OnAssociateRegistration_ProviderContextForController) {
  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Set up ServiceWorkerProviderContext for ServiceWorkerGlobalScope.
  const int kProviderId = 10;
  scoped_refptr<ServiceWorkerProviderContext> provider_context(
      new ServiceWorkerProviderContext(kProviderId,
                                       SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
                                       thread_safe_sender()));

  // The passed references should be adopted and owned by the provider context.
  OnAssociateRegistration(kDocumentMainThreadId, kProviderId, info, attrs);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Destruction of the provider context should release references to the
  // associated registration and its versions.
  provider_context = nullptr;
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());
}

TEST_F(ServiceWorkerDispatcherTest,
       OnAssociateRegistration_ProviderContextForControllee) {
  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Set up ServiceWorkerProviderContext for a document context.
  const int kProviderId = 10;
  scoped_refptr<ServiceWorkerProviderContext> provider_context(
      new ServiceWorkerProviderContext(kProviderId,
                                       SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                       thread_safe_sender()));

  // The passed references should be adopted and only the registration reference
  // should be owned by the provider context.
  OnAssociateRegistration(kDocumentMainThreadId, kProviderId, info, attrs);
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  ipc_sink()->ClearMessages();

  // Disassociating the provider context from the registration should release
  // the reference.
  OnDisassociateRegistration(kDocumentMainThreadId, kProviderId);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
}

TEST_F(ServiceWorkerDispatcherTest, OnSetControllerServiceWorker) {
  const int kProviderId = 10;
  bool should_notify_controllerchange = true;

  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // (1) In the case there are no SWProviderContext and WebSWProviderClient for
  // the provider, the passed reference to the active worker should be adopted
  // but immediately released because there is no provider context to own it.
  OnSetControllerServiceWorker(kDocumentMainThreadId, kProviderId, attrs.active,
                               should_notify_controllerchange);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  ipc_sink()->ClearMessages();

  // (2) In the case there is no WebSWProviderClient but SWProviderContext for
  // the provider, the passed referecence should be adopted and owned by the
  // provider context.
  scoped_refptr<ServiceWorkerProviderContext> provider_context(
      new ServiceWorkerProviderContext(kProviderId,
                                       SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                       thread_safe_sender()));
  OnAssociateRegistration(kDocumentMainThreadId, kProviderId, info, attrs);
  ipc_sink()->ClearMessages();
  OnSetControllerServiceWorker(kDocumentMainThreadId, kProviderId, attrs.active,
                               should_notify_controllerchange);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Destruction of the provider context should release references to the
  // associated registration and the controller.
  provider_context = nullptr;
  ASSERT_EQ(2UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  ipc_sink()->ClearMessages();

  // (3) In the case there is no SWProviderContext but WebSWProviderClient for
  // the provider, the new reference should be created and owned by the provider
  // client (but the reference is immediately released due to limitation of the
  // mock provider client. See the comment on setController() of the mock).
  // In addition, the passed reference should be adopted but immediately
  // released because there is no provider context to own it.
  std::unique_ptr<MockWebServiceWorkerProviderClientImpl> provider_client(
      new MockWebServiceWorkerProviderClientImpl(kProviderId, dispatcher()));
  ASSERT_FALSE(provider_client->is_set_controlled_called());
  OnSetControllerServiceWorker(kDocumentMainThreadId, kProviderId, attrs.active,
                               should_notify_controllerchange);
  EXPECT_TRUE(provider_client->is_set_controlled_called());
  ASSERT_EQ(3UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  provider_client.reset();
  ipc_sink()->ClearMessages();

  // (4) In the case there are both SWProviderContext and SWProviderClient for
  // the provider, the passed referecence should be adopted and owned by the
  // provider context. In addition, the new reference should be created for the
  // provider client and immediately released due to limitation of the mock
  // implementation.
  provider_context = new ServiceWorkerProviderContext(
      kProviderId, SERVICE_WORKER_PROVIDER_FOR_WINDOW, thread_safe_sender());
  OnAssociateRegistration(kDocumentMainThreadId, kProviderId, info, attrs);
  provider_client.reset(
      new MockWebServiceWorkerProviderClientImpl(kProviderId, dispatcher()));
  ASSERT_FALSE(provider_client->is_set_controlled_called());
  ipc_sink()->ClearMessages();
  OnSetControllerServiceWorker(kDocumentMainThreadId, kProviderId, attrs.active,
                               should_notify_controllerchange);
  EXPECT_TRUE(provider_client->is_set_controlled_called());
  ASSERT_EQ(2UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
}

// Test that clearing the controller by sending a kInvalidServiceWorkerHandle
// results in the provider context having a null controller.
TEST_F(ServiceWorkerDispatcherTest, OnSetControllerServiceWorker_Null) {
  const int kProviderId = 10;
  bool should_notify_controllerchange = true;

  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  std::unique_ptr<MockWebServiceWorkerProviderClientImpl> provider_client(
      new MockWebServiceWorkerProviderClientImpl(kProviderId, dispatcher()));
  scoped_refptr<ServiceWorkerProviderContext> provider_context(
      new ServiceWorkerProviderContext(kProviderId,
                                       SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                       thread_safe_sender()));

  OnAssociateRegistration(kDocumentMainThreadId, kProviderId, info, attrs);

  // Set the controller to kInvalidServiceWorkerHandle.
  OnSetControllerServiceWorker(kDocumentMainThreadId, kProviderId,
                               ServiceWorkerObjectInfo(),
                               should_notify_controllerchange);

  // Check that it became null.
  EXPECT_EQ(nullptr, provider_context->controller());
  EXPECT_TRUE(provider_client->is_set_controlled_called());
}

TEST_F(ServiceWorkerDispatcherTest, OnPostMessage) {
  const int kProviderId = 10;

  // Assume that these objects are passed from the browser process and own
  // references to browser-side registration/worker representations.
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  ServiceWorkerMsg_MessageToDocument_Params params;
  params.thread_id = kDocumentMainThreadId;
  params.provider_id = kProviderId;
  params.service_worker_info = attrs.active;

  // The passed reference should be adopted but immediately released because
  // there is no provider client.
  OnPostMessage(params);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  ipc_sink()->ClearMessages();

  std::unique_ptr<MockWebServiceWorkerProviderClientImpl> provider_client(
      new MockWebServiceWorkerProviderClientImpl(kProviderId, dispatcher()));
  ASSERT_FALSE(provider_client->is_dispatch_message_event_called());

  // The passed reference should be owned by the provider client (but the
  // reference is immediately released due to limitation of the mock provider
  // client. See the comment on dispatchMessageEvent() of the mock).
  OnPostMessage(params);
  EXPECT_TRUE(provider_client->is_dispatch_message_event_called());
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
}

TEST_F(ServiceWorkerDispatcherTest, GetServiceWorker) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a worker object newly created with the given reference.
  scoped_refptr<WebServiceWorkerImpl> worker(
      dispatcher()->GetOrCreateServiceWorker(Adopt(attrs.installing)));
  EXPECT_TRUE(worker);
  EXPECT_TRUE(ContainsServiceWorker(attrs.installing.handle_id));
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  // Should return the same worker object and release the given reference.
  scoped_refptr<WebServiceWorkerImpl> existing_worker =
      dispatcher()->GetOrCreateServiceWorker(Adopt(attrs.installing));
  EXPECT_EQ(worker, existing_worker);
  ASSERT_EQ(1UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  ipc_sink()->ClearMessages();

  // Should return nullptr when a given object is invalid.
  scoped_refptr<WebServiceWorkerImpl> invalid_worker =
      dispatcher()->GetOrCreateServiceWorker(Adopt(ServiceWorkerObjectInfo()));
  EXPECT_FALSE(invalid_worker);
  EXPECT_EQ(0UL, ipc_sink()->message_count());
}

TEST_F(ServiceWorkerDispatcherTest, GetOrCreateRegistration) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a registration object newly created with incrementing
  // the refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration1(
      dispatcher()->GetOrCreateRegistration(info, attrs));
  EXPECT_TRUE(registration1);
  EXPECT_TRUE(ContainsRegistration(info.handle_id));
  EXPECT_EQ(info.registration_id, registration1->registration_id());
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());

  ipc_sink()->ClearMessages();

  // Should return the same registration object without incrementing the
  // refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration2(
      dispatcher()->GetOrCreateRegistration(info, attrs));
  EXPECT_TRUE(registration2);
  EXPECT_EQ(registration1, registration2);
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  ipc_sink()->ClearMessages();

  // The registration dtor decrements the refcounts.
  registration1 = nullptr;
  registration2 = nullptr;
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());
}

TEST_F(ServiceWorkerDispatcherTest, GetOrAdoptRegistration) {
  ServiceWorkerRegistrationObjectInfo info;
  ServiceWorkerVersionAttributes attrs;
  CreateObjectInfoAndVersionAttributes(&info, &attrs);

  // Should return a registration object newly created with adopting the
  // refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration1(
      dispatcher()->GetOrAdoptRegistration(info, attrs));
  EXPECT_TRUE(registration1);
  EXPECT_TRUE(ContainsRegistration(info.handle_id));
  EXPECT_EQ(info.registration_id, registration1->registration_id());
  EXPECT_EQ(0UL, ipc_sink()->message_count());

  ipc_sink()->ClearMessages();

  // Should return the same registration object without incrementing the
  // refcounts.
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration2(
      dispatcher()->GetOrAdoptRegistration(info, attrs));
  EXPECT_TRUE(registration2);
  EXPECT_EQ(registration1, registration2);
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());

  ipc_sink()->ClearMessages();

  // The registration dtor decrements the refcounts.
  registration1 = nullptr;
  registration2 = nullptr;
  ASSERT_EQ(4UL, ipc_sink()->message_count());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(0)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(1)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount::ID,
            ipc_sink()->GetMessageAt(2)->type());
  EXPECT_EQ(ServiceWorkerHostMsg_DecrementRegistrationRefCount::ID,
            ipc_sink()->GetMessageAt(3)->type());
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_service_impl.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/common/message_port_messages.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_sync_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SharedWorkerServiceImplTest : public testing::Test {
 public:
  static void RegisterRunningProcessID(int process_id) {
    base::AutoLock lock(s_lock_);
    s_running_process_id_set_.insert(process_id);
  }
  static void UnregisterRunningProcessID(int process_id) {
    base::AutoLock lock(s_lock_);
    s_running_process_id_set_.erase(process_id);
  }

 protected:
  SharedWorkerServiceImplTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext()),
        partition_(new WorkerStoragePartition(
            BrowserContext::GetDefaultStoragePartition(browser_context_.get())->
                GetURLRequestContext(),
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL)) {
    SharedWorkerServiceImpl::GetInstance()
        ->ChangeUpdateWorkerDependencyFuncForTesting(
            &SharedWorkerServiceImplTest::MockUpdateWorkerDependency);
    SharedWorkerServiceImpl::GetInstance()
        ->ChangeTryIncrementWorkerRefCountFuncForTesting(
            &SharedWorkerServiceImplTest::MockTryIncrementWorkerRefCount);
  }

  void SetUp() override {}
  void TearDown() override {
    s_update_worker_dependency_call_count_ = 0;
    s_worker_dependency_added_ids_.clear();
    s_worker_dependency_removed_ids_.clear();
    s_running_process_id_set_.clear();
    SharedWorkerServiceImpl::GetInstance()->ResetForTesting();
  }
  static void MockUpdateWorkerDependency(const std::vector<int>& added_ids,
                                         const std::vector<int>& removed_ids) {
    ++s_update_worker_dependency_call_count_;
    s_worker_dependency_added_ids_ = added_ids;
    s_worker_dependency_removed_ids_ = removed_ids;
  }
  static bool MockTryIncrementWorkerRefCount(int worker_process_id) {
    base::AutoLock lock(s_lock_);
    return s_running_process_id_set_.find(worker_process_id) !=
           s_running_process_id_set_.end();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<WorkerStoragePartition> partition_;
  static int s_update_worker_dependency_call_count_;
  static std::vector<int> s_worker_dependency_added_ids_;
  static std::vector<int> s_worker_dependency_removed_ids_;
  static base::Lock s_lock_;
  static std::set<int> s_running_process_id_set_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImplTest);
};

// static
int SharedWorkerServiceImplTest::s_update_worker_dependency_call_count_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_added_ids_;
std::vector<int> SharedWorkerServiceImplTest::s_worker_dependency_removed_ids_;
base::Lock SharedWorkerServiceImplTest::s_lock_;
std::set<int> SharedWorkerServiceImplTest::s_running_process_id_set_;

namespace {

static const int kProcessIDs[] = {100, 101, 102};
static const unsigned long long kDocumentIDs[] = {200, 201, 202};
static const int kRenderFrameRouteIDs[] = {300, 301, 302};

class MockMessagePortMessageFilter : public MessagePortMessageFilter {
 public:
  MockMessagePortMessageFilter(const NextRoutingIDCallback& callback,
                               ScopedVector<IPC::Message>* message_queue)
      : MessagePortMessageFilter(callback), message_queue_(message_queue) {}

  bool Send(IPC::Message* message) override {
    if (!message_queue_) {
      delete message;
      return false;
    }
    message_queue_->push_back(message);
    return true;
  }

  void Close() {
    message_queue_ = NULL;
    OnChannelClosing();
  }

 private:
  ~MockMessagePortMessageFilter() override {}
  ScopedVector<IPC::Message>* message_queue_;
};

class MockSharedWorkerMessageFilter : public SharedWorkerMessageFilter {
 public:
  MockSharedWorkerMessageFilter(int render_process_id,
                                ResourceContext* resource_context,
                                const WorkerStoragePartition& partition,
                                MessagePortMessageFilter* message_port_filter,
                                ScopedVector<IPC::Message>* message_queue)
      : SharedWorkerMessageFilter(render_process_id,
                                  resource_context,
                                  partition,
                                  message_port_filter),
        message_queue_(message_queue) {}

  bool Send(IPC::Message* message) override {
    if (!message_queue_) {
      delete message;
      return false;
    }
    message_queue_->push_back(message);
    return true;
  }

  void Close() {
    message_queue_ = NULL;
    OnChannelClosing();
  }

 private:
  ~MockSharedWorkerMessageFilter() override {}
  ScopedVector<IPC::Message>* message_queue_;
};

class MockRendererProcessHost {
 public:
  MockRendererProcessHost(int process_id,
                          ResourceContext* resource_context,
                          const WorkerStoragePartition& partition)
      : process_id_(process_id),
        message_filter_(new MockMessagePortMessageFilter(
            base::Bind(&base::AtomicSequenceNumber::GetNext,
                       base::Unretained(&next_routing_id_)),
            &queued_messages_)),
        worker_filter_(new MockSharedWorkerMessageFilter(process_id,
                                                         resource_context,
                                                         partition,
                                                         message_filter_.get(),
                                                         &queued_messages_)) {
    SharedWorkerServiceImplTest::RegisterRunningProcessID(process_id);
  }

  ~MockRendererProcessHost() {
    SharedWorkerServiceImplTest::UnregisterRunningProcessID(process_id_);
    message_filter_->Close();
    worker_filter_->Close();
  }

  bool OnMessageReceived(IPC::Message* message) {
    std::unique_ptr<IPC::Message> msg(message);
    const bool ret = message_filter_->OnMessageReceived(*message) ||
                     worker_filter_->OnMessageReceived(*message);
    if (message->is_sync()) {
      CHECK(!queued_messages_.empty());
      const IPC::Message* response_msg = queued_messages_.back();
      IPC::SyncMessage* sync_msg = static_cast<IPC::SyncMessage*>(message);
      std::unique_ptr<IPC::MessageReplyDeserializer> reply_serializer(
          sync_msg->GetReplyDeserializer());
      bool result = reply_serializer->SerializeOutputParameters(*response_msg);
      CHECK(result);
      queued_messages_.pop_back();
    }
    return ret;
  }

  size_t QueuedMessageCount() const { return queued_messages_.size(); }

  std::unique_ptr<IPC::Message> PopMessage() {
    CHECK(queued_messages_.size());
    std::unique_ptr<IPC::Message> msg(*queued_messages_.begin());
    queued_messages_.weak_erase(queued_messages_.begin());
    return msg;
  }

  void FastShutdownIfPossible() {
    SharedWorkerServiceImplTest::UnregisterRunningProcessID(process_id_);
  }

 private:
  const int process_id_;
  ScopedVector<IPC::Message> queued_messages_;
  base::AtomicSequenceNumber next_routing_id_;
  scoped_refptr<MockMessagePortMessageFilter> message_filter_;
  scoped_refptr<MockSharedWorkerMessageFilter> worker_filter_;
};

void CreateMessagePortPair(MockRendererProcessHost* renderer,
                           int* route_1,
                           int* port_1,
                           int* route_2,
                           int* port_2) {
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_CreateMessagePort(route_1, port_1)));
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_CreateMessagePort(route_2, port_2)));
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_Entangle(*port_1, *port_2)));
  EXPECT_TRUE(renderer->OnMessageReceived(
      new MessagePortHostMsg_Entangle(*port_2, *port_1)));
}

void PostCreateWorker(MockRendererProcessHost* renderer,
                      const std::string& url,
                      const std::string& name,
                      unsigned long long document_id,
                      int render_frame_route_id,
                      ViewHostMsg_CreateWorker_Reply* reply) {
  ViewHostMsg_CreateWorker_Params params;
  params.url = GURL(url);
  params.name = base::ASCIIToUTF16(name);
  params.content_security_policy = base::string16();
  params.security_policy_type = blink::WebContentSecurityPolicyTypeReport;
  params.document_id = document_id;
  params.render_frame_route_id = render_frame_route_id;
  params.creation_context_type =
      blink::WebSharedWorkerCreationContextTypeSecure;
  EXPECT_TRUE(
      renderer->OnMessageReceived(new ViewHostMsg_CreateWorker(params, reply)));
}

class MockSharedWorkerConnector {
 public:
  MockSharedWorkerConnector(MockRendererProcessHost* renderer_host)
      : renderer_host_(renderer_host),
        temporary_remote_port_route_id_(0),
        remote_port_id_(0),
        local_port_route_id_(0),
        local_port_id_(0) {}
  void Create(const std::string& url,
              const std::string& name,
              unsigned long long document_id,
              int render_frame_route_id) {
    CreateMessagePortPair(renderer_host_,
                          &temporary_remote_port_route_id_,
                          &remote_port_id_,
                          &local_port_route_id_,
                          &local_port_id_);
    PostCreateWorker(renderer_host_, url, name, document_id,
                     render_frame_route_id, &create_worker_reply_);
  }
  void SendQueueMessages() {
    EXPECT_TRUE(renderer_host_->OnMessageReceived(
        new MessagePortHostMsg_QueueMessages(remote_port_id_)));
  }
  void SendPostMessage(const std::string& data) {
    const std::vector<int> empty_ports;
    EXPECT_TRUE(
        renderer_host_->OnMessageReceived(new MessagePortHostMsg_PostMessage(
            local_port_id_, base::ASCIIToUTF16(data), empty_ports)));
  }
  void SendConnect() {
    EXPECT_TRUE(
        renderer_host_->OnMessageReceived(new ViewHostMsg_ForwardToWorker(
            WorkerMsg_Connect(create_worker_reply_.route_id, remote_port_id_,
                              MSG_ROUTING_NONE))));
  }
  void SendSendQueuedMessages(
      const std::vector<QueuedMessage>& queued_messages) {
    EXPECT_TRUE(renderer_host_->OnMessageReceived(
        new MessagePortHostMsg_SendQueuedMessages(remote_port_id_,
                                                  queued_messages)));
  }
  int temporary_remote_port_route_id() {
    return temporary_remote_port_route_id_;
  }
  int remote_port_id() { return remote_port_id_; }
  int local_port_route_id() { return local_port_route_id_; }
  int local_port_id() { return local_port_id_; }
  int route_id() { return create_worker_reply_.route_id; }
  blink::WebWorkerCreationError creation_error() {
    return create_worker_reply_.error;
  }

 private:
  MockRendererProcessHost* renderer_host_;
  int temporary_remote_port_route_id_;
  int remote_port_id_;
  int local_port_route_id_;
  int local_port_id_;
  ViewHostMsg_CreateWorker_Reply create_worker_reply_;
};

void CheckWorkerProcessMsgCreateWorker(
    MockRendererProcessHost* renderer_host,
    const std::string& expected_url,
    const std::string& expected_name,
    blink::WebContentSecurityPolicyType expected_security_policy_type,
    int* route_id) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerProcessMsg_CreateWorker::ID, msg->type());
  std::tuple<WorkerProcessMsg_CreateWorker_Params> param;
  EXPECT_TRUE(WorkerProcessMsg_CreateWorker::Read(msg.get(), &param));
  EXPECT_EQ(GURL(expected_url), std::get<0>(param).url);
  EXPECT_EQ(base::ASCIIToUTF16(expected_name), std::get<0>(param).name);
  EXPECT_EQ(expected_security_policy_type,
            std::get<0>(param).security_policy_type);
  *route_id = std::get<0>(param).route_id;
}

void CheckViewMsgWorkerCreated(MockRendererProcessHost* renderer_host,
                               MockSharedWorkerConnector* connector) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(ViewMsg_WorkerCreated::ID, msg->type());
  EXPECT_EQ(connector->route_id(), msg->routing_id());
}

void CheckMessagePortMsgMessagesQueued(MockRendererProcessHost* renderer_host,
                                       MockSharedWorkerConnector* connector) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(MessagePortMsg_MessagesQueued::ID, msg->type());
  EXPECT_EQ(connector->temporary_remote_port_route_id(), msg->routing_id());
}

void CheckWorkerMsgConnect(MockRendererProcessHost* renderer_host,
                           int expected_msg_route_id,
                           int expected_sent_message_port_id,
                           int* routing_id) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(WorkerMsg_Connect::ID, msg->type());
  EXPECT_EQ(expected_msg_route_id, msg->routing_id());
  WorkerMsg_Connect::Param params;
  EXPECT_TRUE(WorkerMsg_Connect::Read(msg.get(), &params));
  int port_id = std::get<0>(params);
  *routing_id = std::get<1>(params);
  EXPECT_EQ(expected_sent_message_port_id, port_id);
}

void CheckMessagePortMsgMessage(MockRendererProcessHost* renderer_host,
                                int expected_msg_route_id,
                                std::string expected_data) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(MessagePortMsg_Message::ID, msg->type());
  EXPECT_EQ(expected_msg_route_id, msg->routing_id());
  MessagePortMsg_Message::Param params;
  EXPECT_TRUE(MessagePortMsg_Message::Read(msg.get(), &params));
  base::string16 data = std::get<0>(params);
  EXPECT_EQ(base::ASCIIToUTF16(expected_data), data);
}

void CheckViewMsgWorkerConnected(MockRendererProcessHost* renderer_host,
                                 MockSharedWorkerConnector* connector) {
  std::unique_ptr<IPC::Message> msg(renderer_host->PopMessage());
  EXPECT_EQ(ViewMsg_WorkerConnected::ID, msg->type());
  EXPECT_EQ(connector->route_id(), msg->routing_id());
}

}  // namespace

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<MockRendererProcessHost> renderer_host(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector(
      new MockSharedWorkerConnector(renderer_host.get()));
  int worker_route_id;
  int worker_msg_port_route_id;

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector->Create("http://example.com/w.js",
                    "name",
                    kDocumentIDs[0],
                    kRenderFrameRouteIDs[0]);
  // We need to go to UI thread to call ReserveRenderProcessOnUI().
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host->QueuedMessageCount());
  // WorkerProcessMsg_CreateWorker should be sent to the renderer in which
  // SharedWorker will be created.
  CheckWorkerProcessMsgCreateWorker(renderer_host.get(),
                                    "http://example.com/w.js",
                                    "name",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  // ViewMsg_WorkerCreated(1) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host.get(), connector.get());

  // SharedWorkerConnector side sends MessagePortHostMsg_QueueMessages in
  // WebSharedWorkerProxy::connect.
  connector->SendQueueMessages();
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // MessagePortMsg_MessagesQueued(2) should be sent back to
  // SharedWorkerConnector side.
  CheckMessagePortMsgMessagesQueued(renderer_host.get(), connector.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(1), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector->SendConnect();
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // WorkerMsg_Connect should be sent to SharedWorker side.
  CheckWorkerMsgConnect(renderer_host.get(),
                        worker_route_id,
                        connector->remote_port_id(),
                        &worker_msg_port_route_id);

  // When SharedWorkerConnector receives MessagePortMsg_MessagesQueued(2), it
  // sends MessagePortHostMsg_SendQueuedMessages.
  std::vector<QueuedMessage> empty_messages;
  connector->SendSendQueuedMessages(empty_messages);
  EXPECT_EQ(0U, renderer_host->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerReadyForInspection in
  // EmbeddedSharedWorkerStub::WorkerReadyForInspection().
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_WorkerReadyForInspection(worker_route_id)));
  EXPECT_EQ(0U, renderer_host->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerScriptLoaded in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(renderer_host->OnMessageReceived(
      new WorkerHostMsg_WorkerScriptLoaded(worker_route_id)));
  EXPECT_EQ(0U, renderer_host->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(
      renderer_host->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          connector->remote_port_id(), worker_route_id)));
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host.get(), connector.get());

  // When SharedWorkerConnector side sends MessagePortHostMsg_PostMessage,
  // SharedWorker side shuold receive MessagePortMsg_Message.
  connector->SendPostMessage("test1");
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host.get(), worker_msg_port_route_id, "test1");

  // When SharedWorker side sends MessagePortHostMsg_PostMessage,
  // SharedWorkerConnector side shuold receive MessagePortMsg_Message.
  const std::vector<int> empty_ports;
  EXPECT_TRUE(
      renderer_host->OnMessageReceived(new MessagePortHostMsg_PostMessage(
          connector->remote_port_id(),
          base::ASCIIToUTF16("test2"), empty_ports)));
  EXPECT_EQ(1U, renderer_host->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host.get(), connector->local_port_route_id(), "test2");

  // UpdateWorkerDependency should not be called.
  EXPECT_EQ(0, s_update_worker_dependency_call_count_);
}

TEST_F(SharedWorkerServiceImplTest, TwoRendererTest) {
  // The first renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  int worker_route_id;
  int worker_msg_port_route_id1;

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector0->Create("http://example.com/w.js",
                     "name",
                     kDocumentIDs[0],
                     kRenderFrameRouteIDs[0]);
  // We need to go to UI thread to call ReserveRenderProcessOnUI().
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
  // WorkerProcessMsg_CreateWorker should be sent to the renderer in which
  // SharedWorker will be created.
  CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                    "http://example.com/w.js",
                                    "name",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  // ViewMsg_WorkerCreated(1) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());

  // SharedWorkerConnector side sends MessagePortHostMsg_QueueMessages in
  // WebSharedWorkerProxy::connect.
  connector0->SendQueueMessages();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  // MessagePortMsg_MessagesQueued(2) should be sent back to
  // SharedWorkerConnector side.
  CheckMessagePortMsgMessagesQueued(renderer_host0.get(), connector0.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(1), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector0->SendConnect();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  // WorkerMsg_Connect should be sent to SharedWorker side.
  CheckWorkerMsgConnect(renderer_host0.get(),
                        worker_route_id,
                        connector0->remote_port_id(),
                        &worker_msg_port_route_id1);

  // When SharedWorkerConnector receives MessagePortMsg_MessagesQueued(2), it
  // sends MessagePortHostMsg_SendQueuedMessages.
  std::vector<QueuedMessage> empty_messages;
  connector0->SendSendQueuedMessages(empty_messages);
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerReadyForInspection in
  // EmbeddedSharedWorkerStub::WorkerReadyForInspection().
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_WorkerReadyForInspection(worker_route_id)));
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerScriptLoaded in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(renderer_host0->OnMessageReceived(
      new WorkerHostMsg_WorkerScriptLoaded(worker_route_id)));
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::workerScriptLoaded().
  EXPECT_TRUE(
      renderer_host0->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          connector0->remote_port_id(), worker_route_id)));
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host0.get(), connector0.get());

  // When SharedWorkerConnector side sends MessagePortHostMsg_PostMessage,
  // SharedWorker side shuold receive MessagePortMsg_Message.
  connector0->SendPostMessage("test1");
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host0.get(), worker_msg_port_route_id1, "test1");

  // When SharedWorker side sends MessagePortHostMsg_PostMessage,
  // SharedWorkerConnector side shuold receive MessagePortMsg_Message.
  const std::vector<int> empty_ports;
  EXPECT_TRUE(
      renderer_host0->OnMessageReceived(new MessagePortHostMsg_PostMessage(
          connector0->remote_port_id(),
          base::ASCIIToUTF16("test2"), empty_ports)));
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host0.get(), connector0->local_port_route_id(), "test2");

  // The second renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  int worker_msg_port_route_id2;

  // UpdateWorkerDependency should not be called yet.
  EXPECT_EQ(0, s_update_worker_dependency_call_count_);

  // SharedWorkerConnector creates two message ports and sends
  // ViewHostMsg_CreateWorker.
  connector1->Create("http://example.com/w.js",
                     "name",
                     kDocumentIDs[1],
                     kRenderFrameRouteIDs[1]);
  // We need to go to UI thread to call ReserveRenderProcessOnUI().
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // ViewMsg_WorkerCreated(3) should be sent back to SharedWorkerConnector side.
  CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());

  // UpdateWorkerDependency should be called.
  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  EXPECT_EQ(1U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(kProcessIDs[0], s_worker_dependency_added_ids_[0]);
  EXPECT_EQ(0U, s_worker_dependency_removed_ids_.size());

  // SharedWorkerConnector side sends MessagePortHostMsg_QueueMessages in
  // WebSharedWorkerProxy::connect.
  connector1->SendQueueMessages();
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // MessagePortMsg_MessagesQueued(4) should be sent back to
  // SharedWorkerConnector side.
  CheckMessagePortMsgMessagesQueued(renderer_host1.get(), connector1.get());

  // When SharedWorkerConnector receives ViewMsg_WorkerCreated(3), it sends
  // WorkerMsg_Connect wrapped in ViewHostMsg_ForwardToWorker.
  connector1->SendConnect();
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  // WorkerMsg_Connect should be sent to SharedWorker side.
  CheckWorkerMsgConnect(renderer_host0.get(),
                        worker_route_id,
                        connector1->remote_port_id(),
                        &worker_msg_port_route_id2);

  // When SharedWorkerConnector receives MessagePortMsg_MessagesQueued(4), it
  // sends MessagePortHostMsg_SendQueuedMessages.
  connector1->SendSendQueuedMessages(empty_messages);
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());

  // SharedWorker sends WorkerHostMsg_WorkerConnected in
  // EmbeddedSharedWorkerStub::OnConnect().
  EXPECT_TRUE(
      renderer_host0->OnMessageReceived(new WorkerHostMsg_WorkerConnected(
          connector1->remote_port_id(), worker_route_id)));
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  // ViewMsg_WorkerConnected should be sent to SharedWorkerConnector side.
  CheckViewMsgWorkerConnected(renderer_host1.get(), connector1.get());

  // When SharedWorkerConnector side sends MessagePortHostMsg_PostMessage,
  // SharedWorker side shuold receive MessagePortMsg_Message.
  connector1->SendPostMessage("test3");
  EXPECT_EQ(1U, renderer_host0->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host0.get(), worker_msg_port_route_id2, "test3");

  // When SharedWorker side sends MessagePortHostMsg_PostMessage,
  // SharedWorkerConnector side shuold receive MessagePortMsg_Message.
  EXPECT_TRUE(
      renderer_host0->OnMessageReceived(new MessagePortHostMsg_PostMessage(
          connector1->remote_port_id(),
          base::ASCIIToUTF16("test4"), empty_ports)));
  EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
  CheckMessagePortMsgMessage(
      renderer_host1.get(), connector1->local_port_route_id(), "test4");

  EXPECT_EQ(1, s_update_worker_dependency_call_count_);
  renderer_host1.reset();
  // UpdateWorkerDependency should be called.
  EXPECT_EQ(2, s_update_worker_dependency_call_count_);
  EXPECT_EQ(0U, s_worker_dependency_added_ids_.size());
  EXPECT_EQ(1U, s_worker_dependency_removed_ids_.size());
  EXPECT_EQ(kProcessIDs[0], s_worker_dependency_removed_ids_[0]);
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest) {
  // The first renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  // The second renderer host.
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  int worker_route_id;

  // Normal case.
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w1.js",
                       "name1",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                      "http://example.com/w1.js",
                                      "name1",
                                      blink::WebContentSecurityPolicyTypeReport,
                                      &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    connector1->Create("http://example.com/w1.js",
                       "name1",
                       kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }

  // Normal case (URL mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w2.js",
                       "name2",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                      "http://example.com/w2.js",
                                      "name2",
                                      blink::WebContentSecurityPolicyTypeReport,
                                      &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    connector1->Create("http://example.com/w2x.js",
                       "name2",
                       kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_EQ(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(blink::WebWorkerCreationErrorURLMismatch,
              connector1->creation_error());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  }

  // Pending case.
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w3.js",
                       "name3",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    connector1->Create("http://example.com/w3.js",
                       "name3",
                       kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                      "http://example.com/w3.js",
                                      "name3",
                                      blink::WebContentSecurityPolicyTypeReport,
                                      &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    EXPECT_EQ(1U, renderer_host1->QueuedMessageCount());
    CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());
  }

  // Pending case (URL mismatch).
  {
    std::unique_ptr<MockSharedWorkerConnector> connector0(
        new MockSharedWorkerConnector(renderer_host0.get()));
    std::unique_ptr<MockSharedWorkerConnector> connector1(
        new MockSharedWorkerConnector(renderer_host1.get()));
    connector0->Create("http://example.com/w4.js",
                       "name4",
                       kDocumentIDs[0],
                       kRenderFrameRouteIDs[0]);
    EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
    EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
    connector1->Create("http://example.com/w4x.js",
                       "name4",
                       kDocumentIDs[1],
                       kRenderFrameRouteIDs[1]);
    EXPECT_EQ(MSG_ROUTING_NONE, connector1->route_id());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
    RunAllPendingInMessageLoop();
    EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
    CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                      "http://example.com/w4.js",
                                      "name4",
                                      blink::WebContentSecurityPolicyTypeReport,
                                      &worker_route_id);
    CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
    EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  }
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest) {
  // Create three renderer hosts.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host2(
      new MockRendererProcessHost(kProcessIDs[2],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  int worker_route_id;

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector2(
      new MockSharedWorkerConnector(renderer_host2.get()));
  connector0->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[0],
                     kRenderFrameRouteIDs[0]);
  EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host0->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(renderer_host0.get(),
                                    "http://example.com/w1.js",
                                    "name1",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  CheckViewMsgWorkerCreated(renderer_host0.get(), connector0.get());
  renderer_host0->FastShutdownIfPossible();

  connector1->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[1],
                     kRenderFrameRouteIDs[1]);
  EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(renderer_host1.get(),
                                    "http://example.com/w1.js",
                                    "name1",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());

  connector2->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[2],
                     kRenderFrameRouteIDs[2]);
  EXPECT_NE(MSG_ROUTING_NONE, connector2->route_id());
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  CheckViewMsgWorkerCreated(renderer_host2.get(), connector2.get());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest2) {
  // Create three renderer hosts.
  std::unique_ptr<MockRendererProcessHost> renderer_host0(
      new MockRendererProcessHost(kProcessIDs[0],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host1(
      new MockRendererProcessHost(kProcessIDs[1],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  std::unique_ptr<MockRendererProcessHost> renderer_host2(
      new MockRendererProcessHost(kProcessIDs[2],
                                  browser_context_->GetResourceContext(),
                                  *partition_.get()));
  int worker_route_id;

  std::unique_ptr<MockSharedWorkerConnector> connector0(
      new MockSharedWorkerConnector(renderer_host0.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector1(
      new MockSharedWorkerConnector(renderer_host1.get()));
  std::unique_ptr<MockSharedWorkerConnector> connector2(
      new MockSharedWorkerConnector(renderer_host2.get()));
  connector0->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[0],
                     kRenderFrameRouteIDs[0]);
  EXPECT_NE(MSG_ROUTING_NONE, connector0->route_id());
  EXPECT_EQ(0U, renderer_host0->QueuedMessageCount());
  renderer_host0->FastShutdownIfPossible();

  connector1->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[1],
                     kRenderFrameRouteIDs[1]);
  EXPECT_NE(MSG_ROUTING_NONE, connector1->route_id());
  EXPECT_EQ(0U, renderer_host1->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2U, renderer_host1->QueuedMessageCount());
  CheckWorkerProcessMsgCreateWorker(renderer_host1.get(),
                                    "http://example.com/w1.js",
                                    "name1",
                                    blink::WebContentSecurityPolicyTypeReport,
                                    &worker_route_id);
  CheckViewMsgWorkerCreated(renderer_host1.get(), connector1.get());

  connector2->Create("http://example.com/w1.js",
                     "name1",
                     kDocumentIDs[2],
                     kRenderFrameRouteIDs[2]);
  EXPECT_NE(MSG_ROUTING_NONE, connector2->route_id());
  EXPECT_EQ(0U, renderer_host2->QueuedMessageCount());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1U, renderer_host2->QueuedMessageCount());
  CheckViewMsgWorkerCreated(renderer_host2.get(), connector2.get());
}

}  // namespace content

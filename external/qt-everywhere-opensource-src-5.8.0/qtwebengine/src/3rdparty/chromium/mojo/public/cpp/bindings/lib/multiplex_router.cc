// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/sync_handle_watcher.h"

namespace mojo {
namespace internal {

// InterfaceEndpoint stores the information of an interface endpoint registered
// with the router.
// No one other than the router's |endpoints_| and |tasks_| should hold refs to
// this object.
class MultiplexRouter::InterfaceEndpoint
    : public base::RefCounted<InterfaceEndpoint>,
      public InterfaceEndpointController {
 public:
  InterfaceEndpoint(MultiplexRouter* router, InterfaceId id)
      : router_(router),
        id_(id),
        closed_(false),
        peer_closed_(false),
        client_(nullptr),
        event_signalled_(false) {}

  // ---------------------------------------------------------------------------
  // The following public methods are safe to call from any threads without
  // locking.

  InterfaceId id() const { return id_; }

  // ---------------------------------------------------------------------------
  // The following public methods are called under the router's lock.

  bool closed() const { return closed_; }
  void set_closed() {
    router_->lock_.AssertAcquired();
    closed_ = true;
  }

  bool peer_closed() const { return peer_closed_; }
  void set_peer_closed() {
    router_->lock_.AssertAcquired();
    peer_closed_ = true;
  }

  base::SingleThreadTaskRunner* task_runner() const {
    return task_runner_.get();
  }

  InterfaceEndpointClient* client() const { return client_; }

  void AttachClient(InterfaceEndpointClient* client,
                    scoped_refptr<base::SingleThreadTaskRunner> runner) {
    router_->lock_.AssertAcquired();
    DCHECK(!client_);
    DCHECK(!closed_);
    DCHECK(runner->BelongsToCurrentThread());

    task_runner_ = std::move(runner);
    client_ = client;
  }

  // This method must be called on the same thread as the corresponding
  // AttachClient() call.
  void DetachClient() {
    router_->lock_.AssertAcquired();
    DCHECK(client_);
    DCHECK(task_runner_->BelongsToCurrentThread());
    DCHECK(!closed_);

    task_runner_ = nullptr;
    client_ = nullptr;
    sync_watcher_.reset();
  }

  void SignalSyncMessageEvent() {
    router_->lock_.AssertAcquired();
    if (event_signalled_)
      return;

    EnsureEventMessagePipeExists();
    event_signalled_ = true;
    MojoResult result =
        WriteMessageRaw(sync_message_event_sender_.get(), nullptr, 0, nullptr,
                        0, MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }

  // ---------------------------------------------------------------------------
  // The following public methods (i.e., InterfaceEndpointController
  // implementation) are called by the client on the same thread as the
  // AttachClient() call. They are called outside of the router's lock.

  bool SendMessage(Message* message) override {
    DCHECK(task_runner_->BelongsToCurrentThread());
    message->set_interface_id(id_);
    return router_->connector_.Accept(message);
  }

  void AllowWokenUpBySyncWatchOnSameThread() override {
    DCHECK(task_runner_->BelongsToCurrentThread());

    EnsureSyncWatcherExists();
    sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }

  bool SyncWatch(const bool* should_stop) override {
    DCHECK(task_runner_->BelongsToCurrentThread());

    EnsureSyncWatcherExists();
    return sync_watcher_->SyncWatch(should_stop);
  }

 private:
  friend class base::RefCounted<InterfaceEndpoint>;

  ~InterfaceEndpoint() override {
    router_->lock_.AssertAcquired();

    DCHECK(!client_);
    DCHECK(closed_);
    DCHECK(peer_closed_);
    DCHECK(!sync_watcher_);
  }

  void OnHandleReady(MojoResult result) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    scoped_refptr<InterfaceEndpoint> self_protector(this);
    scoped_refptr<MultiplexRouter> router_protector(router_);

    // Because we never close |sync_message_event_{sender,receiver}_| before
    // destruction or set a deadline, |result| should always be MOJO_RESULT_OK.
    DCHECK_EQ(MOJO_RESULT_OK, result);
    bool reset_sync_watcher = false;
    {
      base::AutoLock locker(router_->lock_);

      bool more_to_process = router_->ProcessFirstSyncMessageForEndpoint(id_);

      if (!more_to_process)
        ResetSyncMessageSignal();

      // Currently there are no queued sync messages and the peer has closed so
      // there won't be incoming sync messages in the future.
      reset_sync_watcher = !more_to_process && peer_closed_;
    }
    if (reset_sync_watcher) {
      // If a SyncWatch() call (or multiple ones) of this interface endpoint is
      // on the call stack, resetting the sync watcher will allow it to exit
      // when the call stack unwinds to that frame.
      sync_watcher_.reset();
    }
  }

  void EnsureSyncWatcherExists() {
    DCHECK(task_runner_->BelongsToCurrentThread());
    if (sync_watcher_)
      return;

    {
      base::AutoLock locker(router_->lock_);
      EnsureEventMessagePipeExists();

      auto iter = router_->sync_message_tasks_.find(id_);
      if (iter != router_->sync_message_tasks_.end() && !iter->second.empty())
        SignalSyncMessageEvent();
    }

    sync_watcher_.reset(new SyncHandleWatcher(
        sync_message_event_receiver_.get(), MOJO_HANDLE_SIGNAL_READABLE,
        base::Bind(&InterfaceEndpoint::OnHandleReady, base::Unretained(this))));
  }

  void EnsureEventMessagePipeExists() {
    router_->lock_.AssertAcquired();

    if (sync_message_event_receiver_.is_valid())
      return;

    MojoResult result = CreateMessagePipe(nullptr, &sync_message_event_sender_,
                                          &sync_message_event_receiver_);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }

  void ResetSyncMessageSignal() {
    router_->lock_.AssertAcquired();

    if (!event_signalled_)
      return;

    DCHECK(sync_message_event_receiver_.is_valid());
    MojoResult result = ReadMessageRaw(sync_message_event_receiver_.get(),
                                       nullptr, nullptr, nullptr, nullptr,
                                       MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
    DCHECK_EQ(MOJO_RESULT_OK, result);
    event_signalled_ = false;
  }

  // ---------------------------------------------------------------------------
  // The following members are safe to access from any threads.

  MultiplexRouter* const router_;
  const InterfaceId id_;

  // ---------------------------------------------------------------------------
  // The following members are accessed under the router's lock.

  // Whether the endpoint has been closed.
  bool closed_;
  // Whether the peer endpoint has been closed.
  bool peer_closed_;

  // The task runner on which |client_|'s methods can be called.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Not owned. It is null if no client is attached to this endpoint.
  InterfaceEndpointClient* client_;

  // A message pipe used as an event to signal that sync messages are available.
  // The message pipe handles are initialized under the router's lock and remain
  // unchanged afterwards. They may be accessed outside of the router's lock
  // later.
  ScopedMessagePipeHandle sync_message_event_sender_;
  ScopedMessagePipeHandle sync_message_event_receiver_;
  bool event_signalled_;

  // ---------------------------------------------------------------------------
  // The following members are only valid while a client is attached. They are
  // used exclusively on the client's thread. They may be accessed outside of
  // the router's lock.

  std::unique_ptr<SyncHandleWatcher> sync_watcher_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceEndpoint);
};

struct MultiplexRouter::Task {
 public:
  // Doesn't take ownership of |message| but takes its contents.
  static std::unique_ptr<Task> CreateMessageTask(Message* message) {
    Task* task = new Task(MESSAGE);
    task->message.reset(new Message);
    message->MoveTo(task->message.get());
    return base::WrapUnique(task);
  }
  static std::unique_ptr<Task> CreateNotifyErrorTask(
      InterfaceEndpoint* endpoint) {
    Task* task = new Task(NOTIFY_ERROR);
    task->endpoint_to_notify = endpoint;
    return base::WrapUnique(task);
  }

  ~Task() {}

  bool IsMessageTask() const { return type == MESSAGE; }
  bool IsNotifyErrorTask() const { return type == NOTIFY_ERROR; }

  std::unique_ptr<Message> message;
  scoped_refptr<InterfaceEndpoint> endpoint_to_notify;

  enum Type { MESSAGE, NOTIFY_ERROR };
  Type type;

 private:
  explicit Task(Type in_type) : type(in_type) {}
};

MultiplexRouter::MultiplexRouter(
    bool set_interface_id_namesapce_bit,
    ScopedMessagePipeHandle message_pipe,
    scoped_refptr<base::SingleThreadTaskRunner> runner)
    : AssociatedGroupController(base::ThreadTaskRunnerHandle::Get()),
      set_interface_id_namespace_bit_(set_interface_id_namesapce_bit),
      header_validator_(this),
      connector_(std::move(message_pipe),
                 Connector::MULTI_THREADED_SEND,
                 std::move(runner)),
      control_message_handler_(this),
      control_message_proxy_(&connector_),
      next_interface_id_value_(1),
      posted_to_process_tasks_(false),
      encountered_error_(false),
      testing_mode_(false) {
  // Always participate in sync handle watching, because even if it doesn't
  // expect sync requests during sync handle watching, it may still need to
  // dispatch messages to associated endpoints on a different thread.
  connector_.AllowWokenUpBySyncWatchOnSameThread();
  connector_.set_incoming_receiver(&header_validator_);
  connector_.set_connection_error_handler(
      base::Bind(&MultiplexRouter::OnPipeConnectionError,
                 base::Unretained(this)));
}

MultiplexRouter::~MultiplexRouter() {
  base::AutoLock locker(lock_);

  sync_message_tasks_.clear();
  tasks_.clear();

  for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
    InterfaceEndpoint* endpoint = iter->second.get();
    // Increment the iterator before calling UpdateEndpointStateMayRemove()
    // because it may remove the corresponding value from the map.
    ++iter;

    DCHECK(endpoint->closed());
    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }

  DCHECK(endpoints_.empty());
}

void MultiplexRouter::SetMasterInterfaceName(const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  header_validator_.SetDescription(name + " [master] MessageHeaderValidator");
  control_message_handler_.SetDescription(
      name + " [master] PipeControlMessageHandler");
}

void MultiplexRouter::CreateEndpointHandlePair(
    ScopedInterfaceEndpointHandle* local_endpoint,
    ScopedInterfaceEndpointHandle* remote_endpoint) {
  base::AutoLock locker(lock_);
  uint32_t id = 0;
  do {
    if (next_interface_id_value_ >= kInterfaceIdNamespaceMask)
      next_interface_id_value_ = 1;
    id = next_interface_id_value_++;
    if (set_interface_id_namespace_bit_)
      id |= kInterfaceIdNamespaceMask;
  } while (ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = new InterfaceEndpoint(this, id);
  endpoints_[id] = endpoint;
  if (encountered_error_)
    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);

  *local_endpoint = CreateScopedInterfaceEndpointHandle(id, true);
  *remote_endpoint = CreateScopedInterfaceEndpointHandle(id, false);
}

ScopedInterfaceEndpointHandle MultiplexRouter::CreateLocalEndpointHandle(
    InterfaceId id) {
  if (!IsValidInterfaceId(id))
    return ScopedInterfaceEndpointHandle();

  base::AutoLock locker(lock_);
  bool inserted = false;
  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
  if (inserted) {
    if (encountered_error_)
      UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  } else {
    // If the endpoint already exist, it is because we have received a
    // notification that the peer endpoint has closed.
    CHECK(!endpoint->closed());
    CHECK(endpoint->peer_closed());
  }
  return CreateScopedInterfaceEndpointHandle(id, true);
}

void MultiplexRouter::CloseEndpointHandle(InterfaceId id, bool is_local) {
  if (!IsValidInterfaceId(id))
    return;

  base::AutoLock locker(lock_);

  if (!is_local) {
    DCHECK(ContainsKey(endpoints_, id));
    DCHECK(!IsMasterInterfaceId(id));

    // We will receive a NotifyPeerEndpointClosed message from the other side.
    control_message_proxy_.NotifyEndpointClosedBeforeSent(id);

    return;
  }

  DCHECK(ContainsKey(endpoints_, id));
  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(!endpoint->client());
  DCHECK(!endpoint->closed());
  UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

  if (!IsMasterInterfaceId(id))
    control_message_proxy_.NotifyPeerEndpointClosed(id);

  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);
}

InterfaceEndpointController* MultiplexRouter::AttachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle,
    InterfaceEndpointClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> runner) {
  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));
  DCHECK(client);

  base::AutoLock locker(lock_);
  DCHECK(ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  endpoint->AttachClient(client, std::move(runner));

  if (endpoint->peer_closed())
    tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);

  return endpoint;
}

void MultiplexRouter::DetachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle) {
  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));

  base::AutoLock locker(lock_);
  DCHECK(ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  endpoint->DetachClient();
}

void MultiplexRouter::RaiseError() {
  if (task_runner_->BelongsToCurrentThread()) {
    connector_.RaiseError();
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MultiplexRouter::RaiseError, this));
  }
}

void MultiplexRouter::CloseMessagePipe() {
  DCHECK(thread_checker_.CalledOnValidThread());
  connector_.CloseMessagePipe();
  // CloseMessagePipe() above won't trigger connection error handler.
  // Explicitly call OnPipeConnectionError() so that associated endpoints will
  // get notified.
  OnPipeConnectionError();
}

bool MultiplexRouter::HasAssociatedEndpoints() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock locker(lock_);

  if (endpoints_.size() > 1)
    return true;
  if (endpoints_.size() == 0)
    return false;

  return !ContainsKey(endpoints_, kMasterInterfaceId);
}

void MultiplexRouter::EnableTestingMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock locker(lock_);

  testing_mode_ = true;
  connector_.set_enforce_errors_from_incoming_receiver(false);
}

bool MultiplexRouter::Accept(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<MultiplexRouter> protector(this);
  base::AutoLock locker(lock_);

  ClientCallBehavior client_call_behavior =
      connector_.during_sync_handle_watcher_callback()
          ? ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES
          : ALLOW_DIRECT_CLIENT_CALLS;

  bool processed =
      tasks_.empty() && ProcessIncomingMessage(message, client_call_behavior,
                                               connector_.task_runner());

  if (!processed) {
    // Either the task queue is not empty or we cannot process the message
    // directly. In both cases, there is no need to call ProcessTasks().
    tasks_.push_back(Task::CreateMessageTask(message));
    Task* task = tasks_.back().get();

    if (task->message->has_flag(Message::kFlagIsSync)) {
      InterfaceId id = task->message->interface_id();
      sync_message_tasks_[id].push_back(task);
      auto iter = endpoints_.find(id);
      if (iter != endpoints_.end())
        iter->second->SignalSyncMessageEvent();
    }
  } else if (!tasks_.empty()) {
    // Processing the message may result in new tasks (for error notification)
    // being added to the queue. In this case, we have to attempt to process the
    // tasks.
    ProcessTasks(client_call_behavior, connector_.task_runner());
  }

  // Always return true. If we see errors during message processing, we will
  // explicitly call Connector::RaiseError() to disconnect the message pipe.
  return true;
}

bool MultiplexRouter::OnPeerAssociatedEndpointClosed(InterfaceId id) {
  lock_.AssertAcquired();

  if (IsMasterInterfaceId(id))
    return false;

  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, nullptr);

  // It is possible that this endpoint has been set as peer closed. That is
  // because when the message pipe is closed, all the endpoints are updated with
  // PEER_ENDPOINT_CLOSED. We continue to process remaining tasks in the queue,
  // as long as there are refs keeping the router alive. If there is a
  // PeerAssociatedEndpointClosedEvent control message in the queue, we will get
  // here and see that the endpoint has been marked as peer closed.
  if (!endpoint->peer_closed()) {
    if (endpoint->client())
      tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }

  // No need to trigger a ProcessTasks() because it is already on the stack.

  return true;
}

bool MultiplexRouter::OnAssociatedEndpointClosedBeforeSent(InterfaceId id) {
  lock_.AssertAcquired();

  if (IsMasterInterfaceId(id))
    return false;

  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, nullptr);
  DCHECK(!endpoint->closed());
  UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

  control_message_proxy_.NotifyPeerEndpointClosed(id);

  return true;
}

void MultiplexRouter::OnPipeConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<MultiplexRouter> protector(this);
  base::AutoLock locker(lock_);

  encountered_error_ = true;

  for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
    InterfaceEndpoint* endpoint = iter->second.get();
    // Increment the iterator before calling UpdateEndpointStateMayRemove()
    // because it may remove the corresponding value from the map.
    ++iter;

    if (endpoint->client())
      tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));

    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }

  ProcessTasks(connector_.during_sync_handle_watcher_callback()
                   ? ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES
                   : ALLOW_DIRECT_CLIENT_CALLS,
               connector_.task_runner());
}

void MultiplexRouter::ProcessTasks(
    ClientCallBehavior client_call_behavior,
    base::SingleThreadTaskRunner* current_task_runner) {
  lock_.AssertAcquired();

  if (posted_to_process_tasks_)
    return;

  while (!tasks_.empty()) {
    std::unique_ptr<Task> task(std::move(tasks_.front()));
    tasks_.pop_front();

    InterfaceId id = kInvalidInterfaceId;
    bool sync_message = task->IsMessageTask() && task->message &&
                        task->message->has_flag(Message::kFlagIsSync);
    if (sync_message) {
      id = task->message->interface_id();
      auto& sync_message_queue = sync_message_tasks_[id];
      DCHECK_EQ(task.get(), sync_message_queue.front());
      sync_message_queue.pop_front();
    }

    bool processed =
        task->IsNotifyErrorTask()
            ? ProcessNotifyErrorTask(task.get(), client_call_behavior,
                                     current_task_runner)
            : ProcessIncomingMessage(task->message.get(), client_call_behavior,
                                     current_task_runner);

    if (!processed) {
      if (sync_message) {
        auto& sync_message_queue = sync_message_tasks_[id];
        sync_message_queue.push_front(task.get());
      }
      tasks_.push_front(std::move(task));
      break;
    } else {
      if (sync_message) {
        auto iter = sync_message_tasks_.find(id);
        if (iter != sync_message_tasks_.end() && iter->second.empty())
          sync_message_tasks_.erase(iter);
      }
    }
  }
}

bool MultiplexRouter::ProcessFirstSyncMessageForEndpoint(InterfaceId id) {
  lock_.AssertAcquired();

  auto iter = sync_message_tasks_.find(id);
  if (iter == sync_message_tasks_.end())
    return false;

  MultiplexRouter::Task* task = iter->second.front();
  iter->second.pop_front();

  DCHECK(task->IsMessageTask());
  std::unique_ptr<Message> message(std::move(task->message));

  // Note: after this call, |task| and  |iter| may be invalidated.
  bool processed = ProcessIncomingMessage(
      message.get(), ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES, nullptr);
  DCHECK(processed);

  iter = sync_message_tasks_.find(id);
  if (iter == sync_message_tasks_.end())
    return false;

  if (iter->second.empty()) {
    sync_message_tasks_.erase(iter);
    return false;
  }

  return true;
}

bool MultiplexRouter::ProcessNotifyErrorTask(
    Task* task,
    ClientCallBehavior client_call_behavior,
    base::SingleThreadTaskRunner* current_task_runner) {
  DCHECK(!current_task_runner || current_task_runner->BelongsToCurrentThread());
  lock_.AssertAcquired();
  InterfaceEndpoint* endpoint = task->endpoint_to_notify.get();
  if (!endpoint->client())
    return true;

  if (client_call_behavior != ALLOW_DIRECT_CLIENT_CALLS ||
      endpoint->task_runner() != current_task_runner) {
    MaybePostToProcessTasks(endpoint->task_runner());
    return false;
  }

  DCHECK(endpoint->task_runner()->BelongsToCurrentThread());

  InterfaceEndpointClient* client = endpoint->client();
  {
    // We must unlock before calling into |client| because it may call this
    // object within NotifyError(). Holding the lock will lead to deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same thread, including DetachEndpointClient().
    base::AutoUnlock unlocker(lock_);
    client->NotifyError();
  }
  return true;
}

bool MultiplexRouter::ProcessIncomingMessage(
    Message* message,
    ClientCallBehavior client_call_behavior,
    base::SingleThreadTaskRunner* current_task_runner) {
  DCHECK(!current_task_runner || current_task_runner->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (!message) {
    // This is a sync message and has been processed during sync handle
    // watching.
    return true;
  }

  if (PipeControlMessageHandler::IsPipeControlMessage(message)) {
    if (!control_message_handler_.Accept(message))
      RaiseErrorInNonTestingMode();
    return true;
  }

  InterfaceId id = message->interface_id();
  DCHECK(IsValidInterfaceId(id));

  bool inserted = false;
  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
  if (inserted) {
    // Currently, it is legitimate to receive messages for an endpoint
    // that is not registered. For example, the endpoint is transferred in
    // a message that is discarded. Once we add support to specify all
    // enclosing endpoints in message header, we should be able to remove
    // this.
    UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

    // It is also possible that this newly-inserted endpoint is the master
    // endpoint. When the master InterfacePtr/Binding goes away, the message
    // pipe is closed and we explicitly trigger a pipe connection error. The
    // error updates all the endpoints, including the master endpoint, with
    // PEER_ENDPOINT_CLOSED and removes the master endpoint from the
    // registration. We continue to process remaining tasks in the queue, as
    // long as there are refs keeping the router alive. If there are remaining
    // messages for the master endpoint, we will get here.
    if (!IsMasterInterfaceId(id))
      control_message_proxy_.NotifyPeerEndpointClosed(id);
    return true;
  }

  if (endpoint->closed())
    return true;

  if (!endpoint->client()) {
    // We need to wait until a client is attached in order to dispatch further
    // messages.
    return false;
  }

  bool can_direct_call;
  if (message->has_flag(Message::kFlagIsSync)) {
    can_direct_call = client_call_behavior != NO_DIRECT_CLIENT_CALLS &&
                      endpoint->task_runner()->BelongsToCurrentThread();
  } else {
    can_direct_call = client_call_behavior == ALLOW_DIRECT_CLIENT_CALLS &&
                      endpoint->task_runner() == current_task_runner;
  }

  if (!can_direct_call) {
    MaybePostToProcessTasks(endpoint->task_runner());
    return false;
  }

  DCHECK(endpoint->task_runner()->BelongsToCurrentThread());

  InterfaceEndpointClient* client = endpoint->client();
  bool result = false;
  {
    // We must unlock before calling into |client| because it may call this
    // object within HandleIncomingMessage(). Holding the lock will lead to
    // deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same thread, including DetachEndpointClient().
    base::AutoUnlock unlocker(lock_);
    result = client->HandleIncomingMessage(message);
  }
  if (!result)
    RaiseErrorInNonTestingMode();

  return true;
}

void MultiplexRouter::MaybePostToProcessTasks(
    base::SingleThreadTaskRunner* task_runner) {
  lock_.AssertAcquired();
  if (posted_to_process_tasks_)
    return;

  posted_to_process_tasks_ = true;
  posted_to_task_runner_ = task_runner;
  task_runner->PostTask(
      FROM_HERE, base::Bind(&MultiplexRouter::LockAndCallProcessTasks, this));
}

void MultiplexRouter::LockAndCallProcessTasks() {
  // There is no need to hold a ref to this class in this case because this is
  // always called using base::Bind(), which holds a ref.
  base::AutoLock locker(lock_);
  posted_to_process_tasks_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> runner(
      std::move(posted_to_task_runner_));
  ProcessTasks(ALLOW_DIRECT_CLIENT_CALLS, runner.get());
}

void MultiplexRouter::UpdateEndpointStateMayRemove(
    InterfaceEndpoint* endpoint,
    EndpointStateUpdateType type) {
  switch (type) {
    case ENDPOINT_CLOSED:
      endpoint->set_closed();
      break;
    case PEER_ENDPOINT_CLOSED:
      endpoint->set_peer_closed();
      // If the interface endpoint is performing a sync watch, this makes sure
      // it is notified and eventually exits the sync watch.
      endpoint->SignalSyncMessageEvent();
      break;
  }
  if (endpoint->closed() && endpoint->peer_closed())
    endpoints_.erase(endpoint->id());
}

void MultiplexRouter::RaiseErrorInNonTestingMode() {
  lock_.AssertAcquired();
  if (!testing_mode_)
    RaiseError();
}

MultiplexRouter::InterfaceEndpoint* MultiplexRouter::FindOrInsertEndpoint(
    InterfaceId id,
    bool* inserted) {
  lock_.AssertAcquired();
  // Either |inserted| is nullptr or it points to a boolean initialized as
  // false.
  DCHECK(!inserted || !*inserted);

  auto iter = endpoints_.find(id);
  InterfaceEndpoint* endpoint;
  if (iter == endpoints_.end()) {
    endpoint = new InterfaceEndpoint(this, id);
    endpoints_[id] = endpoint;
    if (inserted)
      *inserted = true;
  } else {
    endpoint = iter->second.get();
  }

  return endpoint;
}

}  // namespace internal
}  // namespace mojo

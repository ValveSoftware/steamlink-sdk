// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/node_controller.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/named_platform_channel_pair.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/broker_host.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/ports_message.h"
#include "mojo/edk/system/request_context.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "mojo/edk/system/mach_port_relay.h"
#endif

#if !defined(OS_NACL)
#include "crypto/random.h"
#endif

namespace mojo {
namespace edk {

namespace {

#if defined(OS_NACL)
template <typename T>
void GenerateRandomName(T* out) { base::RandBytes(out, sizeof(T)); }
#else
template <typename T>
void GenerateRandomName(T* out) { crypto::RandBytes(out, sizeof(T)); }
#endif

ports::NodeName GetRandomNodeName() {
  ports::NodeName name;
  GenerateRandomName(&name);
  return name;
}

void RecordPeerCount(size_t count) {
  DCHECK_LE(count, static_cast<size_t>(std::numeric_limits<int32_t>::max()));

  // 8k is the maximum number of file descriptors allowed in Chrome.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Mojo.System.Node.ConnectedPeers",
                              static_cast<int32_t>(count),
                              1 /* min */,
                              8000 /* max */,
                              50 /* bucket count */);
}

void RecordPendingChildCount(size_t count) {
  DCHECK_LE(count, static_cast<size_t>(std::numeric_limits<int32_t>::max()));

  // 8k is the maximum number of file descriptors allowed in Chrome.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Mojo.System.Node.PendingChildren",
                              static_cast<int32_t>(count),
                              1 /* min */,
                              8000 /* max */,
                              50 /* bucket count */);
}

bool ParsePortsMessage(Channel::Message* message,
                       void** data,
                       size_t* num_data_bytes,
                       size_t* num_header_bytes,
                       size_t* num_payload_bytes,
                       size_t* num_ports_bytes) {
  DCHECK(data && num_data_bytes && num_header_bytes && num_payload_bytes &&
         num_ports_bytes);

  NodeChannel::GetPortsMessageData(message, data, num_data_bytes);
  if (!*num_data_bytes)
    return false;

  if (!ports::Message::Parse(*data, *num_data_bytes, num_header_bytes,
                             num_payload_bytes, num_ports_bytes)) {
    return false;
  }

  return true;
}

// Used by NodeController to watch for shutdown. Since no IO can happen once
// the IO thread is killed, the NodeController can cleanly drop all its peers
// at that time.
class ThreadDestructionObserver :
    public base::MessageLoop::DestructionObserver {
 public:
  static void Create(scoped_refptr<base::TaskRunner> task_runner,
                     const base::Closure& callback) {
    if (task_runner->RunsTasksOnCurrentThread()) {
      // Owns itself.
      new ThreadDestructionObserver(callback);
    } else {
      task_runner->PostTask(FROM_HERE,
                            base::Bind(&Create, task_runner, callback));
    }
  }

 private:
  explicit ThreadDestructionObserver(const base::Closure& callback)
      : callback_(callback) {
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  ~ThreadDestructionObserver() override {
    base::MessageLoop::current()->RemoveDestructionObserver(this);
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    callback_.Run();
    delete this;
  }

  const base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadDestructionObserver);
};

}  // namespace

NodeController::~NodeController() {}

NodeController::NodeController(Core* core)
    : core_(core),
      name_(GetRandomNodeName()),
      node_(new ports::Node(name_, this)) {
  DVLOG(1) << "Initializing node " << name_;
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void NodeController::CreateMachPortRelay(
    base::PortProvider* port_provider) {
  base::AutoLock lock(mach_port_relay_lock_);
  DCHECK(!mach_port_relay_);
  mach_port_relay_.reset(new MachPortRelay(port_provider));
}
#endif

void NodeController::SetIOTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {
  io_task_runner_ = task_runner;
  ThreadDestructionObserver::Create(
      io_task_runner_,
      base::Bind(&NodeController::DropAllPeers, base::Unretained(this)));
}

void NodeController::ConnectToChild(
    base::ProcessHandle process_handle,
    ScopedPlatformHandle platform_handle,
    const std::string& child_token,
    const ProcessErrorCallback& process_error_callback) {
  // Generate the temporary remote node name here so that it can be associated
  // with the embedder's child_token. If an error occurs in the child process
  // after it is launched, but before any reserved ports are connected, this can
  // be used to clean up any dangling ports.
  ports::NodeName node_name;
  GenerateRandomName(&node_name);

  {
    base::AutoLock lock(reserved_ports_lock_);
    bool inserted = pending_child_tokens_.insert(
        std::make_pair(node_name, child_token)).second;
    DCHECK(inserted);
  }

#if defined(OS_WIN)
  // On Windows, we need to duplicate the process handle because we have no
  // control over its lifetime and it may become invalid by the time the posted
  // task runs.
  HANDLE dup_handle = INVALID_HANDLE_VALUE;
  BOOL ok = ::DuplicateHandle(
      base::GetCurrentProcessHandle(), process_handle,
      base::GetCurrentProcessHandle(), &dup_handle,
      0, FALSE, DUPLICATE_SAME_ACCESS);
  DPCHECK(ok);
  process_handle = dup_handle;
#endif

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NodeController::ConnectToChildOnIOThread,
                 base::Unretained(this),
                 process_handle,
                 base::Passed(&platform_handle),
                 node_name,
                 process_error_callback));
}

void NodeController::CloseChildPorts(const std::string& child_token) {
  std::vector<ports::PortRef> ports_to_close;
  {
    std::vector<std::string> port_tokens;
    base::AutoLock lock(reserved_ports_lock_);
    for (const auto& port : reserved_ports_) {
      if (port.second.child_token == child_token) {
        DVLOG(1) << "Closing reserved port " << port.second.port.name();
        ports_to_close.push_back(port.second.port);
        port_tokens.push_back(port.first);
      }
    }

    for (const auto& token : port_tokens)
      reserved_ports_.erase(token);
  }

  for (const auto& port : ports_to_close)
    node_->ClosePort(port);

  // Ensure local port closure messages are processed.
  AcceptIncomingMessages();
}

void NodeController::ClosePeerConnection(const std::string& peer_token) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NodeController::ClosePeerConnectionOnIOThread,
                            base::Unretained(this), peer_token));
}

void NodeController::ConnectToParent(ScopedPlatformHandle platform_handle) {
#if !defined(OS_MACOSX) && !defined(OS_NACL_SFI)
  // Use the bootstrap channel for the broker and receive the node's channel
  // synchronously as the first message from the broker.
  base::ElapsedTimer timer;
  broker_.reset(new Broker(std::move(platform_handle)));
  platform_handle = broker_->GetParentPlatformHandle();
  UMA_HISTOGRAM_TIMES("Mojo.System.GetParentPlatformHandleSyncTime",
                      timer.Elapsed());

  if (!platform_handle.is_valid()) {
    // Most likely the browser side of the channel has already been closed and
    // the broker was unable to negotiate a NodeChannel pipe. In this case we
    // can cancel parent connection.
    DVLOG(1) << "Cannot connect to invalid parent channel.";
    CancelPendingPortMerges();
    return;
  }
#endif

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NodeController::ConnectToParentOnIOThread,
                 base::Unretained(this),
                 base::Passed(&platform_handle)));
}

void NodeController::ConnectToPeer(ScopedPlatformHandle handle,
                                   const ports::PortRef& port,
                                   const std::string& peer_token) {
  ports::NodeName node_name;
  GenerateRandomName(&node_name);
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NodeController::ConnectToPeerOnIOThread,
                            base::Unretained(this), base::Passed(&handle),
                            node_name, port, peer_token));
}

void NodeController::SetPortObserver(
    const ports::PortRef& port,
    const scoped_refptr<PortObserver>& observer) {
  node_->SetUserData(port, observer);
}

void NodeController::ClosePort(const ports::PortRef& port) {
  SetPortObserver(port, nullptr);
  int rv = node_->ClosePort(port);
  DCHECK_EQ(rv, ports::OK) << " Failed to close port: " << port.name();

  AcceptIncomingMessages();
}

int NodeController::SendMessage(const ports::PortRef& port,
                                std::unique_ptr<PortsMessage> message) {
  ports::ScopedMessage ports_message(message.release());
  int rv = node_->SendMessage(port, std::move(ports_message));

  AcceptIncomingMessages();
  return rv;
}

void NodeController::ReservePort(const std::string& token,
                                 const ports::PortRef& port,
                                 const std::string& child_token) {
  DVLOG(2) << "Reserving port " << port.name() << "@" << name_ << " for token "
           << token;

  base::AutoLock lock(reserved_ports_lock_);
  auto result = reserved_ports_.insert(
      std::make_pair(token, ReservedPort{port, child_token}));
  DCHECK(result.second);
}

void NodeController::MergePortIntoParent(const std::string& token,
                                         const ports::PortRef& port) {
  bool was_merged = false;
  {
    // This request may be coming from within the process that reserved the
    // "parent" side (e.g. for Chrome single-process mode), so if this token is
    // reserved locally, merge locally instead.
    base::AutoLock lock(reserved_ports_lock_);
    auto it = reserved_ports_.find(token);
    if (it != reserved_ports_.end()) {
      node_->MergePorts(port, name_, it->second.port.name());
      reserved_ports_.erase(it);
      was_merged = true;
    }
  }
  if (was_merged) {
    AcceptIncomingMessages();
    return;
  }

  scoped_refptr<NodeChannel> parent;
  bool reject_merge = false;
  {
    // Hold |pending_port_merges_lock_| while getting |parent|. Otherwise,
    // there is a race where the parent can be set, and |pending_port_merges_|
    // be processed between retrieving |parent| and adding the merge to
    // |pending_port_merges_|.
    base::AutoLock lock(pending_port_merges_lock_);
    parent = GetParentChannel();
    if (reject_pending_merges_) {
      reject_merge = true;
    } else if (!parent) {
      pending_port_merges_.push_back(std::make_pair(token, port));
      return;
    }
  }
  if (reject_merge) {
    node_->ClosePort(port);
    DVLOG(2) << "Rejecting port merge for token " << token
             << " due to closed parent channel.";
    AcceptIncomingMessages();
    return;
  }

  parent->RequestPortMerge(port.name(), token);
}

int NodeController::MergeLocalPorts(const ports::PortRef& port0,
                                    const ports::PortRef& port1) {
  int rv = node_->MergeLocalPorts(port0, port1);
  AcceptIncomingMessages();
  return rv;
}

scoped_refptr<PlatformSharedBuffer> NodeController::CreateSharedBuffer(
    size_t num_bytes) {
#if !defined(OS_MACOSX) && !defined(OS_NACL_SFI)
  // Shared buffer creation failure is fatal, so always use the broker when we
  // have one. This does mean that a non-root process that has children will use
  // the broker for shared buffer creation even though that process is
  // privileged.
  if (broker_) {
    return broker_->GetSharedBuffer(num_bytes);
  }
#endif
  return PlatformSharedBuffer::Create(num_bytes);
}

void NodeController::RequestShutdown(const base::Closure& callback) {
  {
    base::AutoLock lock(shutdown_lock_);
    shutdown_callback_ = callback;
    shutdown_callback_flag_.Set(true);
  }

  AttemptShutdownIfRequested();
}

void NodeController::NotifyBadMessageFrom(const ports::NodeName& source_node,
                                          const std::string& error) {
  scoped_refptr<NodeChannel> peer = GetPeerChannel(source_node);
  if (peer)
    peer->NotifyBadMessage(error);
}

void NodeController::ConnectToChildOnIOThread(
    base::ProcessHandle process_handle,
    ScopedPlatformHandle platform_handle,
    ports::NodeName token,
    const ProcessErrorCallback& process_error_callback) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

#if !defined(OS_MACOSX) && !defined(OS_NACL)
  PlatformChannelPair node_channel;
  ScopedPlatformHandle server_handle = node_channel.PassServerHandle();
  // BrokerHost owns itself.
  BrokerHost* broker_host =
      new BrokerHost(process_handle, std::move(platform_handle));
  bool channel_ok = broker_host->SendChannel(node_channel.PassClientHandle());

#if defined(OS_WIN)
  if (!channel_ok) {
    // On Windows the above operation may fail if the channel is crossing a
    // session boundary. In that case we fall back to a named pipe.
    NamedPlatformChannelPair named_channel;
    server_handle = named_channel.PassServerHandle();
    broker_host->SendNamedChannel(named_channel.handle().name);
  }
#else
  CHECK(channel_ok);
#endif  // defined(OS_WIN)

  scoped_refptr<NodeChannel> channel = NodeChannel::Create(
      this, std::move(server_handle), io_task_runner_, process_error_callback);

#else  // !defined(OS_MACOSX) && !defined(OS_NACL)
  scoped_refptr<NodeChannel> channel =
      NodeChannel::Create(this, std::move(platform_handle), io_task_runner_,
                          process_error_callback);
#endif  // !defined(OS_MACOSX) && !defined(OS_NACL)

  // We set up the child channel with a temporary name so it can be identified
  // as a pending child if it writes any messages to the channel. We may start
  // receiving messages from it (though we shouldn't) as soon as Start() is
  // called below.

  pending_children_.insert(std::make_pair(token, channel));
  RecordPendingChildCount(pending_children_.size());

  channel->SetRemoteNodeName(token);
  channel->SetRemoteProcessHandle(process_handle);
  channel->Start();

  channel->AcceptChild(name_, token);
}

void NodeController::ConnectToParentOnIOThread(
    ScopedPlatformHandle platform_handle) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  {
    base::AutoLock lock(parent_lock_);
    DCHECK(parent_name_ == ports::kInvalidNodeName);

    // At this point we don't know the parent's name, so we can't yet insert it
    // into our |peers_| map. That will happen as soon as we receive an
    // AcceptChild message from them.
    bootstrap_parent_channel_ =
        NodeChannel::Create(this, std::move(platform_handle), io_task_runner_,
                            ProcessErrorCallback());
    // Prevent the parent pipe handle from being closed on shutdown. Pipe
    // closure is used by the parent to detect the child process has exited.
    // Relying on message pipes to be closed is not enough because the parent
    // may see the message pipe closure before the child is dead, causing the
    // child process to be unexpectedly SIGKILL'd.
    bootstrap_parent_channel_->LeakHandleOnShutdown();
  }
  bootstrap_parent_channel_->Start();
}

void NodeController::ConnectToPeerOnIOThread(ScopedPlatformHandle handle,
                                             ports::NodeName token,
                                             ports::PortRef port,
                                             const std::string& peer_token) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<NodeChannel> channel =
      NodeChannel::Create(this, std::move(handle), io_task_runner_, {});
  peer_connections_.insert(
      {token, PeerConnection{channel, port, peer_token}});
  peers_by_token_.insert({peer_token, token});

  channel->SetRemoteNodeName(token);
  channel->Start();

  channel->AcceptPeer(name_, token, port.name());
}

void NodeController::ClosePeerConnectionOnIOThread(
    const std::string& peer_token) {
  RequestContext request_context(RequestContext::Source::SYSTEM);
  auto peer = peers_by_token_.find(peer_token);
  // The connection may already be closed.
  if (peer == peers_by_token_.end())
    return;

  // |peer| may be removed so make a copy of |name|.
  ports::NodeName name = peer->second;
  DropPeer(name, nullptr);
}

scoped_refptr<NodeChannel> NodeController::GetPeerChannel(
    const ports::NodeName& name) {
  base::AutoLock lock(peers_lock_);
  auto it = peers_.find(name);
  if (it == peers_.end())
    return nullptr;
  return it->second;
}

scoped_refptr<NodeChannel> NodeController::GetParentChannel() {
  ports::NodeName parent_name;
  {
    base::AutoLock lock(parent_lock_);
    parent_name = parent_name_;
  }
  return GetPeerChannel(parent_name);
}

scoped_refptr<NodeChannel> NodeController::GetBrokerChannel() {
  ports::NodeName broker_name;
  {
    base::AutoLock lock(broker_lock_);
    broker_name = broker_name_;
  }
  return GetPeerChannel(broker_name);
}

void NodeController::AddPeer(const ports::NodeName& name,
                             scoped_refptr<NodeChannel> channel,
                             bool start_channel) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  DCHECK(name != ports::kInvalidNodeName);
  DCHECK(channel);

  channel->SetRemoteNodeName(name);

  OutgoingMessageQueue pending_messages;
  {
    base::AutoLock lock(peers_lock_);
    if (peers_.find(name) != peers_.end()) {
      // This can happen normally if two nodes race to be introduced to each
      // other. The losing pipe will be silently closed and introduction should
      // not be affected.
      DVLOG(1) << "Ignoring duplicate peer name " << name;
      return;
    }

    auto result = peers_.insert(std::make_pair(name, channel));
    DCHECK(result.second);

    DVLOG(2) << "Accepting new peer " << name << " on node " << name_;

    RecordPeerCount(peers_.size());

    auto it = pending_peer_messages_.find(name);
    if (it != pending_peer_messages_.end()) {
      std::swap(pending_messages, it->second);
      pending_peer_messages_.erase(it);
    }
  }

  if (start_channel)
    channel->Start();

  // Flush any queued message we need to deliver to this node.
  while (!pending_messages.empty()) {
    channel->PortsMessage(std::move(pending_messages.front()));
    pending_messages.pop();
  }
}

void NodeController::DropPeer(const ports::NodeName& name,
                              NodeChannel* channel) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  {
    base::AutoLock lock(peers_lock_);
    auto it = peers_.find(name);

    if (it != peers_.end()) {
      ports::NodeName peer = it->first;
      peers_.erase(it);
      DVLOG(1) << "Dropped peer " << peer;
    }

    pending_peer_messages_.erase(name);
    pending_children_.erase(name);

    RecordPeerCount(peers_.size());
    RecordPendingChildCount(pending_children_.size());
  }

  std::vector<ports::PortRef> ports_to_close;
  {
    // Clean up any reserved ports.
    base::AutoLock lock(reserved_ports_lock_);
    auto it = pending_child_tokens_.find(name);
    if (it != pending_child_tokens_.end()) {
      const std::string& child_token = it->second;

      std::vector<std::string> port_tokens;
      for (const auto& port : reserved_ports_) {
        if (port.second.child_token == child_token) {
          DVLOG(1) << "Closing reserved port: " << port.second.port.name();
          ports_to_close.push_back(port.second.port);
          port_tokens.push_back(port.first);
        }
      }

      // We have to erase reserved ports in a two-step manner because the usual
      // manner of using the returned iterator from map::erase isn't technically
      // valid in C++11 (although it is in C++14).
      for (const auto& token : port_tokens)
        reserved_ports_.erase(token);

      pending_child_tokens_.erase(it);
    }
  }

  bool is_parent;
  {
    base::AutoLock lock(parent_lock_);
    is_parent = (name == parent_name_ || channel == bootstrap_parent_channel_);
  }

  // If the error comes from the parent channel, we also need to cancel any
  // port merge requests, so that errors can be propagated to the message
  // pipes.
  if (is_parent)
    CancelPendingPortMerges();

  auto peer = peer_connections_.find(name);
  if (peer != peer_connections_.end()) {
    peers_by_token_.erase(peer->second.peer_token);
    ports_to_close.push_back(peer->second.local_port);
    peer_connections_.erase(peer);
  }

  for (const auto& port : ports_to_close)
    node_->ClosePort(port);

  node_->LostConnectionToNode(name);

  AcceptIncomingMessages();
}

void NodeController::SendPeerMessage(const ports::NodeName& name,
                                     ports::ScopedMessage message) {
  Channel::MessagePtr channel_message =
      static_cast<PortsMessage*>(message.get())->TakeChannelMessage();

  scoped_refptr<NodeChannel> peer = GetPeerChannel(name);
#if defined(OS_WIN)
  if (channel_message->has_handles()) {
    // If we're sending a message with handles we aren't the destination
    // node's parent or broker (i.e. we don't know its process handle), ask
    // the broker to relay for us.
    scoped_refptr<NodeChannel> broker = GetBrokerChannel();
    if (!peer || !peer->HasRemoteProcessHandle()) {
      if (broker) {
        broker->RelayPortsMessage(name, std::move(channel_message));
      } else {
        base::AutoLock lock(broker_lock_);
        pending_relay_messages_[name].emplace(std::move(channel_message));
      }
      return;
    }
  }
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (channel_message->has_mach_ports()) {
    // Messages containing Mach ports are always routed through the broker, even
    // if the broker process is the intended recipient.
    bool use_broker = false;
    {
      base::AutoLock lock(parent_lock_);
      use_broker = (bootstrap_parent_channel_ ||
                    parent_name_ != ports::kInvalidNodeName);
    }
    if (use_broker) {
      scoped_refptr<NodeChannel> broker = GetBrokerChannel();
      if (broker) {
        broker->RelayPortsMessage(name, std::move(channel_message));
      } else {
        base::AutoLock lock(broker_lock_);
        pending_relay_messages_[name].emplace(std::move(channel_message));
      }
      return;
    }
  }
#endif  // defined(OS_WIN)

  if (peer) {
    peer->PortsMessage(std::move(channel_message));
    return;
  }

  // If we don't know who the peer is, queue the message for delivery. If this
  // is the first message queued for the peer, we also ask the broker to
  // introduce us to them.

  bool needs_introduction = false;
  {
    base::AutoLock lock(peers_lock_);
    auto& queue = pending_peer_messages_[name];
    needs_introduction = queue.empty();
    queue.emplace(std::move(channel_message));
  }

  if (needs_introduction) {
    scoped_refptr<NodeChannel> broker = GetBrokerChannel();
    if (!broker) {
      DVLOG(1) << "Dropping message for unknown peer: " << name;
      return;
    }
    broker->RequestIntroduction(name);
  }
}

void NodeController::AcceptIncomingMessages() {
  while (incoming_messages_flag_) {
    // TODO: We may need to be more careful to avoid starving the rest of the
    // thread here. Revisit this if it turns out to be a problem. One
    // alternative would be to schedule a task to continue pumping messages
    // after flushing once.

    messages_lock_.Acquire();
    if (incoming_messages_.empty()) {
      messages_lock_.Release();
      break;
    }

    // libstdc++'s deque creates an internal buffer on construction, even when
    // the size is 0. So avoid creating it until it is necessary.
    std::queue<ports::ScopedMessage> messages;
    std::swap(messages, incoming_messages_);
    incoming_messages_flag_.Set(false);
    messages_lock_.Release();

    while (!messages.empty()) {
      node_->AcceptMessage(std::move(messages.front()));
      messages.pop();
    }
  }

  AttemptShutdownIfRequested();
}

void NodeController::ProcessIncomingMessages() {
  RequestContext request_context(RequestContext::Source::SYSTEM);

  {
    base::AutoLock lock(messages_lock_);
    // Allow a new incoming messages processing task to be posted. This can't be
    // done after AcceptIncomingMessages() otherwise a message might be missed.
    // Doing it here may result in at most two tasks existing at the same time;
    // this running one, and one pending in the task runner.
    incoming_messages_task_posted_ = false;
  }

  AcceptIncomingMessages();
}

void NodeController::DropAllPeers() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  std::vector<scoped_refptr<NodeChannel>> all_peers;
  {
    base::AutoLock lock(parent_lock_);
    if (bootstrap_parent_channel_) {
      // |bootstrap_parent_channel_| isn't null'd here becuase we rely on its
      // existence to determine whether or not this is the root node. Once
      // bootstrap_parent_channel_->ShutDown() has been called,
      // |bootstrap_parent_channel_| is essentially a dead object and it doesn't
      // matter if it's deleted now or when |this| is deleted.
      // Note: |bootstrap_parent_channel_| is only modified on the IO thread.
      all_peers.push_back(bootstrap_parent_channel_);
    }
  }

  {
    base::AutoLock lock(peers_lock_);
    for (const auto& peer : peers_)
      all_peers.push_back(peer.second);
    for (const auto& peer : pending_children_)
      all_peers.push_back(peer.second);
    peers_.clear();
    pending_children_.clear();
    pending_peer_messages_.clear();
    peer_connections_.clear();
  }

  for (const auto& peer : all_peers)
    peer->ShutDown();

  if (destroy_on_io_thread_shutdown_)
    delete this;
}

void NodeController::GenerateRandomPortName(ports::PortName* port_name) {
  GenerateRandomName(port_name);
}

void NodeController::AllocMessage(size_t num_header_bytes,
                                  ports::ScopedMessage* message) {
  message->reset(new PortsMessage(num_header_bytes, 0, 0, nullptr));
}

void NodeController::ForwardMessage(const ports::NodeName& node,
                                    ports::ScopedMessage message) {
  DCHECK(message);
  bool schedule_pump_task = false;
  if (node == name_) {
    // NOTE: We need to avoid re-entering the Node instance within
    // ForwardMessage. Because ForwardMessage is only ever called
    // (synchronously) in response to Node's ClosePort, SendMessage, or
    // AcceptMessage, we flush the queue after calling any of those methods.
    base::AutoLock lock(messages_lock_);
    // |io_task_runner_| may be null in tests or processes that don't require
    // multi-process Mojo.
    schedule_pump_task = incoming_messages_.empty() && io_task_runner_ &&
        !incoming_messages_task_posted_;
    incoming_messages_task_posted_ |= schedule_pump_task;
    incoming_messages_.emplace(std::move(message));
    incoming_messages_flag_.Set(true);
  } else {
    SendPeerMessage(node, std::move(message));
  }

  if (schedule_pump_task) {
    // Normally, the queue is processed after the action that added the local
    // message is done (i.e. SendMessage, ClosePort, etc). However, it's also
    // possible for a local message to be added as a result of a remote message,
    // and OnChannelMessage() doesn't process this queue (although
    // OnPortsMessage() does). There may also be other code paths, now or added
    // in the future, which cause local messages to be added but don't process
    // this message queue.
    //
    // Instead of adding a call to AcceptIncomingMessages() on every possible
    // code path, post a task to the IO thread to process the queue. If the
    // current call stack processes the queue, this may end up doing nothing.
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&NodeController::ProcessIncomingMessages,
                   base::Unretained(this)));
  }
}

void NodeController::BroadcastMessage(ports::ScopedMessage message) {
  CHECK_EQ(message->num_ports(), 0u);
  Channel::MessagePtr channel_message =
      static_cast<PortsMessage*>(message.get())->TakeChannelMessage();
  CHECK(!channel_message->has_handles());

  scoped_refptr<NodeChannel> broker = GetBrokerChannel();
  if (broker)
    broker->Broadcast(std::move(channel_message));
  else
    OnBroadcast(name_, std::move(channel_message));
}

void NodeController::PortStatusChanged(const ports::PortRef& port) {
  scoped_refptr<ports::UserData> user_data;
  node_->GetUserData(port, &user_data);

  PortObserver* observer = static_cast<PortObserver*>(user_data.get());
  if (observer) {
    observer->OnPortStatusChanged();
  } else {
    DVLOG(2) << "Ignoring status change for " << port.name() << " because it "
             << "doesn't have an observer.";
  }
}

void NodeController::OnAcceptChild(const ports::NodeName& from_node,
                                   const ports::NodeName& parent_name,
                                   const ports::NodeName& token) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<NodeChannel> parent;
  {
    base::AutoLock lock(parent_lock_);
    if (bootstrap_parent_channel_ && parent_name_ == ports::kInvalidNodeName) {
      parent_name_ = parent_name;
      parent = bootstrap_parent_channel_;
    }
  }

  if (!parent) {
    DLOG(ERROR) << "Unexpected AcceptChild message from " << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  parent->SetRemoteNodeName(parent_name);
  parent->AcceptParent(token, name_);

  // NOTE: The child does not actually add its parent as a peer until
  // receiving an AcceptBrokerClient message from the broker. The parent
  // will request that said message be sent upon receiving AcceptParent.

  DVLOG(1) << "Child " << name_ << " accepting parent " << parent_name;
}

void NodeController::OnAcceptParent(const ports::NodeName& from_node,
                                    const ports::NodeName& token,
                                    const ports::NodeName& child_name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  auto it = pending_children_.find(from_node);
  if (it == pending_children_.end() || token != from_node) {
    DLOG(ERROR) << "Received unexpected AcceptParent message from "
                << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  scoped_refptr<NodeChannel> channel = it->second;
  pending_children_.erase(it);

  DCHECK(channel);

  DVLOG(1) << "Parent " << name_ << " accepted child " << child_name;

  AddPeer(child_name, channel, false /* start_channel */);

  // TODO(rockot/amistry): We could simplify child initialization if we could
  // synchronously get a new async broker channel from the broker. For now we do
  // it asynchronously since it's only used to facilitate handle passing, not
  // handle creation.
  scoped_refptr<NodeChannel> broker = GetBrokerChannel();
  if (broker) {
    // Inform the broker of this new child.
    broker->AddBrokerClient(child_name, channel->CopyRemoteProcessHandle());
  } else {
    // If we have no broker, either we need to wait for one, or we *are* the
    // broker.
    scoped_refptr<NodeChannel> parent = GetParentChannel();
    if (!parent) {
      base::AutoLock lock(parent_lock_);
      parent = bootstrap_parent_channel_;
    }

    if (!parent) {
      // Yes, we're the broker. We can initialize the child directly.
      channel->AcceptBrokerClient(name_, ScopedPlatformHandle());
    } else {
      // We aren't the broker, so wait for a broker connection.
      base::AutoLock lock(broker_lock_);
      pending_broker_clients_.push(child_name);
    }
  }
}

void NodeController::OnAddBrokerClient(const ports::NodeName& from_node,
                                       const ports::NodeName& client_name,
                                       base::ProcessHandle process_handle) {
#if defined(OS_WIN)
  // Scoped handle to avoid leaks on error.
  ScopedPlatformHandle scoped_process_handle =
      ScopedPlatformHandle(PlatformHandle(process_handle));
#endif
  scoped_refptr<NodeChannel> sender = GetPeerChannel(from_node);
  if (!sender) {
    DLOG(ERROR) << "Ignoring AddBrokerClient from unknown sender.";
    return;
  }

  if (GetPeerChannel(client_name)) {
    DLOG(ERROR) << "Ignoring AddBrokerClient for known client.";
    DropPeer(from_node, nullptr);
    return;
  }

  PlatformChannelPair broker_channel;
  scoped_refptr<NodeChannel> client = NodeChannel::Create(
      this, broker_channel.PassServerHandle(), io_task_runner_,
      ProcessErrorCallback());

#if defined(OS_WIN)
  // The broker must have a working handle to the client process in order to
  // properly copy other handles to and from the client.
  if (!scoped_process_handle.is_valid()) {
    DLOG(ERROR) << "Broker rejecting client with invalid process handle.";
    return;
  }
  client->SetRemoteProcessHandle(scoped_process_handle.release().handle);
#else
  client->SetRemoteProcessHandle(process_handle);
#endif

  AddPeer(client_name, client, true /* start_channel */);

  DVLOG(1) << "Broker " << name_ << " accepting client " << client_name
           << " from peer " << from_node;

  sender->BrokerClientAdded(client_name, broker_channel.PassClientHandle());
}

void NodeController::OnBrokerClientAdded(const ports::NodeName& from_node,
                                         const ports::NodeName& client_name,
                                         ScopedPlatformHandle broker_channel) {
  scoped_refptr<NodeChannel> client = GetPeerChannel(client_name);
  if (!client) {
    DLOG(ERROR) << "BrokerClientAdded for unknown child " << client_name;
    return;
  }

  // This should have come from our own broker.
  if (GetBrokerChannel() != GetPeerChannel(from_node)) {
    DLOG(ERROR) << "BrokerClientAdded from non-broker node " << from_node;
    return;
  }

  DVLOG(1) << "Child " << client_name << " accepted by broker " << from_node;

  client->AcceptBrokerClient(from_node, std::move(broker_channel));
}

void NodeController::OnAcceptBrokerClient(const ports::NodeName& from_node,
                                          const ports::NodeName& broker_name,
                                          ScopedPlatformHandle broker_channel) {
  // This node should already have a parent in bootstrap mode.
  ports::NodeName parent_name;
  scoped_refptr<NodeChannel> parent;
  {
    base::AutoLock lock(parent_lock_);
    parent_name = parent_name_;
    parent = bootstrap_parent_channel_;
    bootstrap_parent_channel_ = nullptr;
  }
  DCHECK(parent_name == from_node);
  DCHECK(parent);

  std::queue<ports::NodeName> pending_broker_clients;
  std::unordered_map<ports::NodeName, OutgoingMessageQueue>
      pending_relay_messages;
  {
    base::AutoLock lock(broker_lock_);
    broker_name_ = broker_name;
    std::swap(pending_broker_clients, pending_broker_clients_);
    std::swap(pending_relay_messages, pending_relay_messages_);
  }
  DCHECK(broker_name != ports::kInvalidNodeName);

  // It's now possible to add both the broker and the parent as peers.
  // Note that the broker and parent may be the same node.
  scoped_refptr<NodeChannel> broker;
  if (broker_name == parent_name) {
    DCHECK(!broker_channel.is_valid());
    broker = parent;
  } else {
    DCHECK(broker_channel.is_valid());
    broker = NodeChannel::Create(this, std::move(broker_channel),
                                 io_task_runner_, ProcessErrorCallback());
    AddPeer(broker_name, broker, true /* start_channel */);
  }

  AddPeer(parent_name, parent, false /* start_channel */);

  {
    // Complete any port merge requests we have waiting for the parent.
    base::AutoLock lock(pending_port_merges_lock_);
    for (const auto& request : pending_port_merges_)
      parent->RequestPortMerge(request.second.name(), request.first);
    pending_port_merges_.clear();
  }

  // Feed the broker any pending children of our own.
  while (!pending_broker_clients.empty()) {
    const ports::NodeName& child_name = pending_broker_clients.front();
    auto it = pending_children_.find(child_name);
    DCHECK(it != pending_children_.end());
    broker->AddBrokerClient(child_name, it->second->CopyRemoteProcessHandle());
    pending_broker_clients.pop();
  }

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
  // Have the broker relay any messages we have waiting.
  for (auto& entry : pending_relay_messages) {
    const ports::NodeName& destination = entry.first;
    auto& message_queue = entry.second;
    while (!message_queue.empty()) {
      broker->RelayPortsMessage(destination, std::move(message_queue.front()));
      message_queue.pop();
    }
  }
#endif

  DVLOG(1) << "Child " << name_ << " accepted by broker " << broker_name;
}

void NodeController::OnPortsMessage(const ports::NodeName& from_node,
                                    Channel::MessagePtr channel_message) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  void* data;
  size_t num_data_bytes, num_header_bytes, num_payload_bytes, num_ports_bytes;
  if (!ParsePortsMessage(channel_message.get(), &data, &num_data_bytes,
                         &num_header_bytes, &num_payload_bytes,
                         &num_ports_bytes)) {
    DropPeer(from_node, nullptr);
    return;
  }

  CHECK(channel_message);
  std::unique_ptr<PortsMessage> ports_message(
      new PortsMessage(num_header_bytes,
                       num_payload_bytes,
                       num_ports_bytes,
                       std::move(channel_message)));
  ports_message->set_source_node(from_node);
  node_->AcceptMessage(ports::ScopedMessage(ports_message.release()));
  AcceptIncomingMessages();
}

void NodeController::OnRequestPortMerge(
    const ports::NodeName& from_node,
    const ports::PortName& connector_port_name,
    const std::string& token) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  DVLOG(2) << "Node " << name_ << " received RequestPortMerge for token "
           << token << " and port " << connector_port_name << "@" << from_node;

  ports::PortRef local_port;
  {
    base::AutoLock lock(reserved_ports_lock_);
    auto it = reserved_ports_.find(token);
    if (it == reserved_ports_.end()) {
      DVLOG(1) << "Ignoring request to connect to port for unknown token "
               << token;
      return;
    }
    local_port = it->second.port;
  }

  int rv = node_->MergePorts(local_port, from_node, connector_port_name);
  if (rv != ports::OK)
    DLOG(ERROR) << "MergePorts failed: " << rv;

  AcceptIncomingMessages();
}

void NodeController::OnRequestIntroduction(const ports::NodeName& from_node,
                                           const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<NodeChannel> requestor = GetPeerChannel(from_node);
  if (from_node == name || name == ports::kInvalidNodeName || !requestor) {
    DLOG(ERROR) << "Rejecting invalid OnRequestIntroduction message from "
                << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  scoped_refptr<NodeChannel> new_friend = GetPeerChannel(name);
  if (!new_friend) {
    // We don't know who they're talking about!
    requestor->Introduce(name, ScopedPlatformHandle());
  } else {
    PlatformChannelPair new_channel;
    requestor->Introduce(name, new_channel.PassServerHandle());
    new_friend->Introduce(from_node, new_channel.PassClientHandle());
  }
}

void NodeController::OnIntroduce(const ports::NodeName& from_node,
                                 const ports::NodeName& name,
                                 ScopedPlatformHandle channel_handle) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  if (!channel_handle.is_valid()) {
    node_->LostConnectionToNode(name);

    DVLOG(1) << "Could not be introduced to peer " << name;
    base::AutoLock lock(peers_lock_);
    pending_peer_messages_.erase(name);
    return;
  }

  scoped_refptr<NodeChannel> channel =
      NodeChannel::Create(this, std::move(channel_handle), io_task_runner_,
                          ProcessErrorCallback());

  DVLOG(1) << "Adding new peer " << name << " via parent introduction.";
  AddPeer(name, channel, true /* start_channel */);
}

void NodeController::OnBroadcast(const ports::NodeName& from_node,
                                 Channel::MessagePtr message) {
  DCHECK(!message->has_handles());

  void* data;
  size_t num_data_bytes, num_header_bytes, num_payload_bytes, num_ports_bytes;
  if (!ParsePortsMessage(message.get(), &data, &num_data_bytes,
                         &num_header_bytes, &num_payload_bytes,
                         &num_ports_bytes)) {
    DropPeer(from_node, nullptr);
    return;
  }

  // Broadcast messages must not contain ports.
  if (num_ports_bytes > 0) {
    DropPeer(from_node, nullptr);
    return;
  }

  base::AutoLock lock(peers_lock_);
  for (auto& iter : peers_) {
    // Copy and send the message to each known peer.
    Channel::MessagePtr peer_message(
        new Channel::Message(message->payload_size(), 0));
    memcpy(peer_message->mutable_payload(), message->payload(),
           message->payload_size());
    iter.second->PortsMessage(std::move(peer_message));
  }
}

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
void NodeController::OnRelayPortsMessage(const ports::NodeName& from_node,
                                         base::ProcessHandle from_process,
                                         const ports::NodeName& destination,
                                         Channel::MessagePtr message) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  if (GetBrokerChannel()) {
    // Only the broker should be asked to relay a message.
    LOG(ERROR) << "Non-broker refusing to relay message.";
    DropPeer(from_node, nullptr);
    return;
  }

  // The parent should always know which process this came from.
  DCHECK(from_process != base::kNullProcessHandle);

#if defined(OS_WIN)
  // Rewrite the handles to this (the parent) process. If the message is
  // destined for another child process, the handles will be rewritten to that
  // process before going out (see NodeChannel::WriteChannelMessage).
  //
  // TODO: We could avoid double-duplication.
  //
  // Note that we explicitly mark the handles as being owned by the sending
  // process before rewriting them, in order to accommodate RewriteHandles'
  // internal sanity checks.
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  for (size_t i = 0; i < handles->size(); ++i)
    (*handles)[i].owning_process = from_process;
  if (!Channel::Message::RewriteHandles(from_process,
                                        base::GetCurrentProcessHandle(),
                                        handles.get())) {
    DLOG(ERROR) << "Failed to relay one or more handles.";
  }
  message->SetHandles(std::move(handles));
#else
  MachPortRelay* relay = GetMachPortRelay();
  if (!relay) {
    LOG(ERROR) << "Receiving Mach ports without a port relay from "
               << from_node << ". Dropping message.";
    return;
  }
  if (!relay->ExtractPortRights(message.get(), from_process)) {
    // NodeChannel should ensure that MachPortRelay is ready for the remote
    // process. At this point, if the port extraction failed, either something
    // went wrong in the mach stuff, or the remote process died.
    LOG(ERROR) << "Error on receiving Mach ports " << from_node
               << ". Dropping message.";
    return;
  }
#endif  // defined(OS_WIN)

  if (destination == name_) {
    // Great, we can deliver this message locally.
    OnPortsMessage(from_node, std::move(message));
    return;
  }

  scoped_refptr<NodeChannel> peer = GetPeerChannel(destination);
  if (peer)
    peer->PortsMessageFromRelay(from_node, std::move(message));
  else
    DLOG(ERROR) << "Dropping relay message for unknown node " << destination;
}

void NodeController::OnPortsMessageFromRelay(const ports::NodeName& from_node,
                                             const ports::NodeName& source_node,
                                             Channel::MessagePtr message) {
  if (GetPeerChannel(from_node) != GetBrokerChannel()) {
    LOG(ERROR) << "Refusing relayed message from non-broker node.";
    DropPeer(from_node, nullptr);
    return;
  }

  OnPortsMessage(source_node, std::move(message));
}
#endif

void NodeController::OnAcceptPeer(const ports::NodeName& from_node,
                                  const ports::NodeName& token,
                                  const ports::NodeName& peer_name,
                                  const ports::PortName& port_name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  auto it = peer_connections_.find(from_node);
  if (it == peer_connections_.end()) {
    DLOG(ERROR) << "Received unexpected AcceptPeer message from " << from_node;
    DropPeer(from_node, nullptr);
    return;
  }

  scoped_refptr<NodeChannel> channel = std::move(it->second.channel);
  ports::PortRef local_port = it->second.local_port;
  std::string peer_token = std::move(it->second.peer_token);
  peer_connections_.erase(it);
  DCHECK(channel);

  // If the peer connection is a self connection (which is used in tests),
  // drop the channel to it and skip straight to merging the ports.
  if (name_ == peer_name) {
    peers_by_token_.erase(peer_token);
  } else {
    peers_by_token_[peer_token] = peer_name;
    peer_connections_.insert(
        {peer_name, PeerConnection{nullptr, local_port, peer_token}});
    DVLOG(1) << "Node " << name_ << " accepted peer " << peer_name;

    AddPeer(peer_name, channel, false /* start_channel */);
  }

  // We need to choose one side to initiate the port merge. It doesn't matter
  // who does it as long as they don't both try. Simple solution: pick the one
  // with the "smaller" port name.
  if (local_port.name() < port_name) {
    node()->MergePorts(local_port, peer_name, port_name);
  }
}

void NodeController::OnChannelError(const ports::NodeName& from_node,
                                    NodeChannel* channel) {
  if (io_task_runner_->RunsTasksOnCurrentThread()) {
    DropPeer(from_node, channel);
    // DropPeer may have caused local port closures, so be sure to process any
    // pending local messages.
    AcceptIncomingMessages();
  } else {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&NodeController::OnChannelError, base::Unretained(this),
                   from_node, channel));
  }
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
MachPortRelay* NodeController::GetMachPortRelay() {
  {
    base::AutoLock lock(parent_lock_);
    // Return null if we're not the root.
    if (bootstrap_parent_channel_ || parent_name_ != ports::kInvalidNodeName)
      return nullptr;
  }

  base::AutoLock lock(mach_port_relay_lock_);
  return mach_port_relay_.get();
}
#endif

void NodeController::CancelPendingPortMerges() {
  std::vector<ports::PortRef> ports_to_close;

  {
    base::AutoLock lock(pending_port_merges_lock_);
    reject_pending_merges_ = true;
    for (const auto& port : pending_port_merges_)
      ports_to_close.push_back(port.second);
    pending_port_merges_.clear();
  }

  for (const auto& port : ports_to_close)
    node_->ClosePort(port);
}

void NodeController::DestroyOnIOThreadShutdown() {
  destroy_on_io_thread_shutdown_ = true;
}

void NodeController::AttemptShutdownIfRequested() {
  if (!shutdown_callback_flag_)
    return;

  base::Closure callback;
  {
    base::AutoLock lock(shutdown_lock_);
    if (shutdown_callback_.is_null())
      return;
    if (!node_->CanShutdownCleanly(
          ports::Node::ShutdownPolicy::ALLOW_LOCAL_PORTS)) {
      DVLOG(2) << "Unable to cleanly shut down node " << name_;
      return;
    }

    callback = shutdown_callback_;
    shutdown_callback_.Reset();
    shutdown_callback_flag_.Set(false);
  }

  DCHECK(!callback.is_null());

  callback.Run();
}

NodeController::PeerConnection::PeerConnection() = default;

NodeController::PeerConnection::PeerConnection(
    const PeerConnection& other) = default;

NodeController::PeerConnection::PeerConnection(
    PeerConnection&& other) = default;

NodeController::PeerConnection::PeerConnection(
    const scoped_refptr<NodeChannel>& channel,
    const ports::PortRef& local_port,
    const std::string& peer_token)
    : channel(channel), local_port(local_port), peer_token(peer_token) {}

NodeController::PeerConnection::~PeerConnection() = default;

NodeController::PeerConnection& NodeController::PeerConnection::
operator=(const PeerConnection& other) = default;

NodeController::PeerConnection& NodeController::PeerConnection::
operator=(PeerConnection&& other) = default;

}  // namespace edk
}  // namespace mojo

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_NODE_CONTROLLER_H_
#define MOJO_EDK_SYSTEM_NODE_CONTROLLER_H_

#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/atomic_flag.h"
#include "mojo/edk/system/node_channel.h"
#include "mojo/edk/system/ports/name.h"
#include "mojo/edk/system/ports/node.h"
#include "mojo/edk/system/ports/node_delegate.h"

namespace base {
class PortProvider;
}

namespace mojo {
namespace edk {

class Broker;
class Core;
class MachPortRelay;
class PortsMessage;

// The owner of ports::Node which facilitates core EDK implementation. All
// public interface methods are safe to call from any thread.
class NodeController : public ports::NodeDelegate,
                       public NodeChannel::Delegate {
 public:
  class PortObserver : public ports::UserData {
   public:
    virtual void OnPortStatusChanged() = 0;

   protected:
    ~PortObserver() override {}
  };

  // |core| owns and out-lives us.
  explicit NodeController(Core* core);
  ~NodeController() override;

  const ports::NodeName& name() const { return name_; }
  Core* core() const { return core_; }
  ports::Node* node() const { return node_.get(); }
  scoped_refptr<base::TaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Create the relay used to transfer mach ports between processes.
  void CreateMachPortRelay(base::PortProvider* port_provider);
#endif

  // Called exactly once, shortly after construction, and before any other
  // methods are called on this object.
  void SetIOTaskRunner(scoped_refptr<base::TaskRunner> io_task_runner);

  // Connects this node to a child node. This node will initiate a handshake.
  void ConnectToChild(base::ProcessHandle process_handle,
                      ScopedPlatformHandle platform_handle,
                      const std::string& child_token,
                      const ProcessErrorCallback& process_error_callback);

  // Closes all reserved ports which associated with the child process
  // |child_token|.
  void CloseChildPorts(const std::string& child_token);

  // Connects this node to a parent node. The parent node will initiate a
  // handshake.
  void ConnectToParent(ScopedPlatformHandle platform_handle);

  // Sets a port's observer. If |observer| is null the port's current observer
  // is removed.
  void SetPortObserver(const ports::PortRef& port,
                       const scoped_refptr<PortObserver>& observer);

  // Closes a port. Use this in lieu of calling Node::ClosePort() directly, as
  // it ensures the port's observer has also been removed.
  void ClosePort(const ports::PortRef& port);

  // Sends a message on a port to its peer.
  int SendMessage(const ports::PortRef& port_ref,
                  std::unique_ptr<PortsMessage> message);

  // Reserves a local port |port| associated with |token|. A peer holding a copy
  // of |token| can merge one of its own ports into this one.
  void ReservePort(const std::string& token, const ports::PortRef& port,
                   const std::string& child_token);

  // Merges a local port |port| into a port reserved by |token| in the parent.
  void MergePortIntoParent(const std::string& token,
                           const ports::PortRef& port);

  // Merges two local ports together.
  int MergeLocalPorts(const ports::PortRef& port0, const ports::PortRef& port1);

  // Creates a new shared buffer for use in the current process.
  scoped_refptr<PlatformSharedBuffer> CreateSharedBuffer(size_t num_bytes);

  // Request that the Node be shut down cleanly. This may take an arbitrarily
  // long time to complete, at which point |callback| will be called.
  //
  // Note that while it is safe to continue using the NodeController's public
  // interface after requesting shutdown, you do so at your own risk and there
  // is NO guarantee that new messages will be sent or ports will complete
  // transfer.
  void RequestShutdown(const base::Closure& callback);

  // Notifies the NodeController that we received a bad message from the given
  // node.
  void NotifyBadMessageFrom(const ports::NodeName& source_node,
                            const std::string& error);

 private:
  friend Core;

  using NodeMap = std::unordered_map<ports::NodeName,
                                     scoped_refptr<NodeChannel>>;
  using OutgoingMessageQueue = std::queue<Channel::MessagePtr>;

  struct ReservedPort {
    ports::PortRef port;
    const std::string child_token;
  };

  void ConnectToChildOnIOThread(
      base::ProcessHandle process_handle,
      ScopedPlatformHandle platform_handle,
      ports::NodeName token,
      const ProcessErrorCallback& process_error_callback);
  void ConnectToParentOnIOThread(ScopedPlatformHandle platform_handle);

  scoped_refptr<NodeChannel> GetPeerChannel(const ports::NodeName& name);
  scoped_refptr<NodeChannel> GetParentChannel();
  scoped_refptr<NodeChannel> GetBrokerChannel();

  void AddPeer(const ports::NodeName& name,
               scoped_refptr<NodeChannel> channel,
               bool start_channel);
  void DropPeer(const ports::NodeName& name);
  void SendPeerMessage(const ports::NodeName& name,
                       ports::ScopedMessage message);
  void AcceptIncomingMessages();
  void DropAllPeers();

  // ports::NodeDelegate:
  void GenerateRandomPortName(ports::PortName* port_name) override;
  void AllocMessage(size_t num_header_bytes,
                    ports::ScopedMessage* message) override;
  void ForwardMessage(const ports::NodeName& node,
                      ports::ScopedMessage message) override;
  void BroadcastMessage(ports::ScopedMessage message) override;
  void PortStatusChanged(const ports::PortRef& port) override;

  // NodeChannel::Delegate:
  void OnAcceptChild(const ports::NodeName& from_node,
                     const ports::NodeName& parent_name,
                     const ports::NodeName& token) override;
  void OnAcceptParent(const ports::NodeName& from_node,
                      const ports::NodeName& token,
                      const ports::NodeName& child_name) override;
  void OnAddBrokerClient(const ports::NodeName& from_node,
                         const ports::NodeName& client_name,
                         base::ProcessHandle process_handle) override;
  void OnBrokerClientAdded(const ports::NodeName& from_node,
                           const ports::NodeName& client_name,
                           ScopedPlatformHandle broker_channel) override;
  void OnAcceptBrokerClient(const ports::NodeName& from_node,
                            const ports::NodeName& broker_name,
                            ScopedPlatformHandle broker_channel) override;
  void OnPortsMessage(const ports::NodeName& from_node,
                      Channel::MessagePtr message) override;
  void OnRequestPortMerge(const ports::NodeName& from_node,
                          const ports::PortName& connector_port_name,
                          const std::string& token) override;
  void OnRequestIntroduction(const ports::NodeName& from_node,
                             const ports::NodeName& name) override;
  void OnIntroduce(const ports::NodeName& from_node,
                   const ports::NodeName& name,
                   ScopedPlatformHandle channel_handle) override;
  void OnBroadcast(const ports::NodeName& from_node,
                   Channel::MessagePtr message) override;
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
  void OnRelayPortsMessage(const ports::NodeName& from_node,
                           base::ProcessHandle from_process,
                           const ports::NodeName& destination,
                           Channel::MessagePtr message) override;
  void OnPortsMessageFromRelay(const ports::NodeName& from_node,
                               const ports::NodeName& source_node,
                               Channel::MessagePtr message) override;
#endif
  void OnChannelError(const ports::NodeName& from_node) override;
#if defined(OS_MACOSX) && !defined(OS_IOS)
  MachPortRelay* GetMachPortRelay() override;
#endif

  // Marks this NodeController for destruction when the IO thread shuts down.
  // This is used in case Core is torn down before the IO thread. Must only be
  // called on the IO thread.
  void DestroyOnIOThreadShutdown();

  // If there is a registered shutdown callback (meaning shutdown has been
  // requested, this checks the Node's status to see if clean shutdown is
  // possible. If so, shutdown is performed and the shutdown callback is run.
  void AttemptShutdownIfRequested();

  // These are safe to access from any thread as long as the Node is alive.
  Core* const core_;
  const ports::NodeName name_;
  const std::unique_ptr<ports::Node> node_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  // Guards |peers_| and |pending_peer_messages_|.
  base::Lock peers_lock_;

  // Channels to known peers, including parent and children, if any.
  NodeMap peers_;

  // Outgoing message queues for peers we've heard of but can't yet talk to.
  std::unordered_map<ports::NodeName, OutgoingMessageQueue>
      pending_peer_messages_;

  // Guards |reserved_ports_| and |pending_child_tokens_|.
  base::Lock reserved_ports_lock_;

  // Ports reserved by token. Key is the port token.
  base::hash_map<std::string, ReservedPort> reserved_ports_;
  // TODO(amistry): This _really_ needs to be a bimap. Unfortunately, we don't
  // have one yet :(
  std::unordered_map<ports::NodeName, std::string> pending_child_tokens_;

  // Guards |pending_port_merges_|.
  base::Lock pending_port_merges_lock_;

  // A set of port merge requests awaiting parent connection.
  std::vector<std::pair<std::string, ports::PortRef>> pending_port_merges_;

  // Guards |parent_name_| and |bootstrap_parent_channel_|.
  base::Lock parent_lock_;

  // The name of our parent node, if any.
  ports::NodeName parent_name_;

  // A temporary reference to the parent channel before we know their name.
  scoped_refptr<NodeChannel> bootstrap_parent_channel_;

  // Guards |broker_name_|, |pending_broker_clients_|, and
  // |pending_relay_messages_|.
  base::Lock broker_lock_;

  // The name of our broker node, if any.
  ports::NodeName broker_name_;

  // A queue of pending child names waiting to be connected to a broker.
  std::queue<ports::NodeName> pending_broker_clients_;

  // Messages waiting to be relayed by the broker once it's known.
  std::unordered_map<ports::NodeName, OutgoingMessageQueue>
      pending_relay_messages_;

  // Guards |incoming_messages_|.
  base::Lock messages_lock_;
  std::queue<ports::ScopedMessage> incoming_messages_;
  // Flag to fast-path checking |incoming_messages_|.
  AtomicFlag incoming_messages_flag_;

  // Guards |shutdown_callback_|.
  base::Lock shutdown_lock_;

  // Set by RequestShutdown(). If this is non-null, the controller will
  // begin polling the Node to see if clean shutdown is possible any time the
  // Node's state is modified by the controller.
  base::Closure shutdown_callback_;
  // Flag to fast-path checking |shutdown_callback_|.
  AtomicFlag shutdown_callback_flag_;

  // All other fields below must only be accessed on the I/O thread, i.e., the
  // thread on which core_->io_task_runner() runs tasks.

  // Channels to children during handshake.
  NodeMap pending_children_;

  // Indicates whether this object should delete itself on IO thread shutdown.
  // Must only be accessed from the IO thread.
  bool destroy_on_io_thread_shutdown_ = false;

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_NACL_SFI)
  // Broker for sync shared buffer creation (non-Mac posix-only) in children.
  std::unique_ptr<Broker> broker_;
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
  base::Lock mach_port_relay_lock_;
  // Relay for transferring mach ports to/from children.
  std::unique_ptr<MachPortRelay> mach_port_relay_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NodeController);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_NODE_CONTROLLER_H_

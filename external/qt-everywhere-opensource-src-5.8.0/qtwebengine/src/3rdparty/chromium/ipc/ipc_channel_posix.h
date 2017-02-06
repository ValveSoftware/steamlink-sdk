// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_POSIX_H_
#define IPC_IPC_CHANNEL_POSIX_H_

#include "ipc/ipc_channel.h"

#include <stddef.h>
#include <sys/socket.h>  // for CMSG macros

#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_reader.h"
#include "ipc/ipc_message_attachment_set.h"

namespace IPC {

class IPC_EXPORT ChannelPosix : public Channel,
                                public internal::ChannelReader,
                                public base::MessageLoopForIO::Watcher {
 public:
  // |broker| must outlive the newly created object.
  ChannelPosix(const IPC::ChannelHandle& channel_handle,
               Mode mode,
               Listener* listener);
  ~ChannelPosix() override;

  // Channel implementation
  bool Connect() override;
  void Close() override;
  bool Send(Message* message) override;
  AttachmentBroker* GetAttachmentBroker() override;
  base::ProcessId GetPeerPID() const override;
  base::ProcessId GetSelfPID() const override;
  int GetClientFileDescriptor() const override;
  base::ScopedFD TakeClientFileDescriptor() override;

  // Returns true if the channel supports listening for connections.
  bool AcceptsConnections() const;

  // Returns true if the channel supports listening for connections and is
  // currently connected.
  bool HasAcceptedConnection() const;

  // Closes any currently connected socket, and returns to a listening state
  // for more connections.
  void ResetToAcceptingConnectionState();

  // Returns true if the peer process' effective user id can be determined, in
  // which case the supplied peer_euid is updated with it.
  bool GetPeerEuid(uid_t* peer_euid) const;

  void CloseClientFileDescriptor();

  static bool IsNamedServerInitialized(const std::string& channel_id);
#if defined(OS_LINUX)
  static void SetGlobalPid(int pid);
  static int GetGlobalPid();
#endif  // OS_LINUX

 private:
  bool CreatePipe(const IPC::ChannelHandle& channel_handle);

  // Returns false on recoverable error.
  // There are two reasons why this method might leave messages in the
  // output_queue_.
  //   1. |waiting_connect_| is |true|.
  //   2. |is_blocked_on_write_| is |true|.
  // If any of these conditionals change, this method should be called, as
  // previously blocked messages may no longer be blocked.
  bool ProcessOutgoingMessages();

  bool AcceptConnection();
  void ClosePipeOnError();
  int GetHelloMessageProcId() const;
  void QueueHelloMessage();
  void CloseFileDescriptors(Message* msg);
  void QueueCloseFDMessage(int fd, int hops);

  // ChannelReader implementation.
  ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) override;
  bool ShouldDispatchInputMessage(Message* msg) override;
  bool GetNonBrokeredAttachments(Message* msg) override;
  bool DidEmptyInputBuffers() override;
  void HandleInternalMessage(const Message& msg) override;
  base::ProcessId GetSenderPID() override;
  bool IsAttachmentBrokerEndpoint() override;

  // Finds the set of file descriptors in the given message.  On success,
  // appends the descriptors to the input_fds_ member and returns true
  //
  // Returns false if the message was truncated. In this case, any handles that
  // were sent will be closed.
  bool ExtractFileDescriptorsFromMsghdr(msghdr* msg);

  // Closes all handles in the input_fds_ list and clears the list. This is
  // used to clean up handles in error conditions to avoid leaking the handles.
  void ClearInputFDs();

  // MessageLoopForIO::Watcher implementation.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Returns |false| on channel error.
  // If |message| has brokerable attachments, those attachments are passed to
  // the AttachmentBroker (which in turn invokes Send()), so this method must
  // be re-entrant.
  // Adds |message| to |output_queue_| and calls ProcessOutgoingMessages().
  bool ProcessMessageForDelivery(Message* message);

  // Moves all messages from |prelim_queue_| to |output_queue_| by calling
  // ProcessMessageForDelivery().
  // Returns |false| on channel error.
  bool FlushPrelimQueue();

  Mode mode_;

  base::ProcessId peer_pid_;

  // After accepting one client connection on our server socket we want to
  // stop listening.
  base::MessageLoopForIO::FileDescriptorWatcher
  server_listen_connection_watcher_;
  base::MessageLoopForIO::FileDescriptorWatcher read_watcher_;
  base::MessageLoopForIO::FileDescriptorWatcher write_watcher_;

  // Indicates whether we're currently blocked waiting for a write to complete.
  bool is_blocked_on_write_;
  bool waiting_connect_;

  // If sending a message blocks then we use this variable
  // to keep track of where we are.
  size_t message_send_bytes_written_;

  // File descriptor we're listening on for new connections if we listen
  // for connections.
  base::ScopedFD server_listen_pipe_;

  // The pipe used for communication.
  base::ScopedFD pipe_;

  // For a server, the client end of our socketpair() -- the other end of our
  // pipe_ that is passed to the client.
  base::ScopedFD client_pipe_;
  mutable base::Lock client_pipe_lock_;  // Lock that protects |client_pipe_|.

  // The "name" of our pipe.  On Windows this is the global identifier for
  // the pipe.  On POSIX it's used as a key in a local map of file descriptors.
  std::string pipe_name_;

  // Messages not yet ready to be sent are queued here. Messages removed from
  // this queue are placed in the output_queue_. The double queue is
  // unfortunate, but is necessary because messages with brokerable attachments
  // can generate multiple messages to be sent (possibly from other channels).
  // Some of these generated messages cannot be sent until |peer_pid_| has been
  // configured.
  // As soon as |peer_pid| has been configured, there is no longer any need for
  // |prelim_queue_|. All messages are flushed, and no new messages are added.
  std::queue<Message*> prelim_queue_;

  // Messages to be sent are queued here.
  std::queue<OutputElement*> output_queue_;

  // We assume a worst case: kReadBufferSize bytes of messages, where each
  // message has no payload and a full complement of descriptors.
  static const size_t kMaxReadFDs =
      (Channel::kReadBufferSize / sizeof(IPC::Message::Header)) *
      MessageAttachmentSet::kMaxDescriptorsPerMessage;

  // Buffer size for file descriptors used for recvmsg. On Mac the CMSG macros
  // are not constant so we have to pick a "large enough" padding for headers.
#if defined(OS_MACOSX)
  static const size_t kMaxReadFDBuffer = 1024 + sizeof(int) * kMaxReadFDs;
#else
  static const size_t kMaxReadFDBuffer = CMSG_SPACE(sizeof(int) * kMaxReadFDs);
#endif
  static_assert(kMaxReadFDBuffer <= 8192,
                "kMaxReadFDBuffer too big for a stack buffer");

  // File descriptors extracted from messages coming off of the channel. The
  // handles may span messages and come off different channels from the message
  // data (in the case of READWRITE), and are processed in FIFO here.
  // NOTE: The implementation assumes underlying storage here is contiguous, so
  // don't change to something like std::deque<> without changing the
  // implementation!
  std::vector<int> input_fds_;


  void ResetSafely(base::ScopedFD* fd);
  bool in_dtor_;

#if defined(OS_MACOSX)
  // On OSX, sent FDs must not be closed until we get an ack.
  // Keep track of sent FDs here to make sure the remote is not
  // trying to bamboozle us.
  std::set<int> fds_to_close_;
#endif

  // True if we are responsible for unlinking the unix domain socket file.
  bool must_unlink_;

#if defined(OS_LINUX)
  // If non-zero, overrides the process ID sent in the hello message.
  static int global_pid_;
#endif  // OS_LINUX

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChannelPosix);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_POSIX_H_

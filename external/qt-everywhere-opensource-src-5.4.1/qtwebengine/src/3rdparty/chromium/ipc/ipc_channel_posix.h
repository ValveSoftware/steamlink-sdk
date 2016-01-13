// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_POSIX_H_
#define IPC_IPC_CHANNEL_POSIX_H_

#include "ipc/ipc_channel.h"

#include <sys/socket.h>  // for CMSG macros

#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "ipc/file_descriptor_set_posix.h"
#include "ipc/ipc_channel_reader.h"

#if !defined(OS_MACOSX)
// On Linux, the seccomp sandbox makes it very expensive to call
// recvmsg() and sendmsg(). The restriction on calling read() and write(), which
// are cheap, is that we can't pass file descriptors over them.
//
// As we cannot anticipate when the sender will provide us with file
// descriptors, we have to make the decision about whether we call read() or
// recvmsg() before we actually make the call. The easiest option is to
// create a dedicated socketpair() for exchanging file descriptors. Any file
// descriptors are split out of a message, with the non-file-descriptor payload
// going over the normal connection, and the file descriptors being sent
// separately over the other channel. When read()ing from a channel, we'll
// notice if the message was supposed to have come with file descriptors and
// use recvmsg on the other socketpair to retrieve them and combine them
// back with the rest of the message.
//
// Mac can also run in IPC_USES_READWRITE mode if necessary, but at this time
// doesn't take a performance hit from recvmsg and sendmsg, so it doesn't
// make sense to waste resources on having the separate dedicated socketpair.
// It is however useful for debugging between Linux and Mac to be able to turn
// this switch 'on' on the Mac as well.
//
// The HELLO message from the client to the server is always sent using
// sendmsg because it will contain the file descriptor that the server
// needs to send file descriptors in later messages.
#define IPC_USES_READWRITE 1
#endif

namespace IPC {

class IPC_EXPORT ChannelPosix : public Channel,
                                public internal::ChannelReader,
                                public base::MessageLoopForIO::Watcher {
 public:
  ChannelPosix(const IPC::ChannelHandle& channel_handle, Mode mode,
               Listener* listener);
  virtual ~ChannelPosix();

  // Channel implementation
  virtual bool Connect() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual bool Send(Message* message) OVERRIDE;
  virtual base::ProcessId GetPeerPID() const OVERRIDE;
  virtual int GetClientFileDescriptor() const OVERRIDE;
  virtual int TakeClientFileDescriptor() OVERRIDE;

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
#endif  // OS_LINUX

 private:
  bool CreatePipe(const IPC::ChannelHandle& channel_handle);

  bool ProcessOutgoingMessages();

  bool AcceptConnection();
  void ClosePipeOnError();
  int GetHelloMessageProcId();
  void QueueHelloMessage();
  void CloseFileDescriptors(Message* msg);
  void QueueCloseFDMessage(int fd, int hops);

  // ChannelReader implementation.
  virtual ReadState ReadData(char* buffer,
                             int buffer_len,
                             int* bytes_read) OVERRIDE;
  virtual bool WillDispatchInputMessage(Message* msg) OVERRIDE;
  virtual bool DidEmptyInputBuffers() OVERRIDE;
  virtual void HandleInternalMessage(const Message& msg) OVERRIDE;

#if defined(IPC_USES_READWRITE)
  // Reads the next message from the fd_pipe_ and appends them to the
  // input_fds_ queue. Returns false if there was a message receiving error.
  // True means there was a message and it was processed properly, or there was
  // no messages.
  bool ReadFileDescriptorsFromFDPipe();
#endif

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
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

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
  int server_listen_pipe_;

  // The pipe used for communication.
  int pipe_;

  // For a server, the client end of our socketpair() -- the other end of our
  // pipe_ that is passed to the client.
  int client_pipe_;
  mutable base::Lock client_pipe_lock_;  // Lock that protects |client_pipe_|.

#if defined(IPC_USES_READWRITE)
  // Linux/BSD use a dedicated socketpair() for passing file descriptors.
  int fd_pipe_;
  int remote_fd_pipe_;
#endif

  // The "name" of our pipe.  On Windows this is the global identifier for
  // the pipe.  On POSIX it's used as a key in a local map of file descriptors.
  std::string pipe_name_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // We assume a worst case: kReadBufferSize bytes of messages, where each
  // message has no payload and a full complement of descriptors.
  static const size_t kMaxReadFDs =
      (Channel::kReadBufferSize / sizeof(IPC::Message::Header)) *
      FileDescriptorSet::kMaxDescriptorsPerMessage;

  // Buffer size for file descriptors used for recvmsg. On Mac the CMSG macros
  // don't seem to be constant so we have to pick a "large enough" value.
#if defined(OS_MACOSX)
  static const size_t kMaxReadFDBuffer = 1024;
#else
  static const size_t kMaxReadFDBuffer = CMSG_SPACE(sizeof(int) * kMaxReadFDs);
#endif

  // Temporary buffer used to receive the file descriptors from recvmsg.
  // Code that writes into this should immediately read them out and save
  // them to input_fds_, since this buffer will be re-used anytime we call
  // recvmsg.
  char input_cmsg_buf_[kMaxReadFDBuffer];

  // File descriptors extracted from messages coming off of the channel. The
  // handles may span messages and come off different channels from the message
  // data (in the case of READWRITE), and are processed in FIFO here.
  // NOTE: The implementation assumes underlying storage here is contiguous, so
  // don't change to something like std::deque<> without changing the
  // implementation!
  std::vector<int> input_fds_;

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

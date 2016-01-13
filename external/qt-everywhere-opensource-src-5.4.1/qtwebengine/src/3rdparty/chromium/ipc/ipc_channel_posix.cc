// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#if defined(OS_OPENBSD)
#include <sys/uio.h>
#endif

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "ipc/file_descriptor_set_posix.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_switches.h"
#include "ipc/unix_domain_socket_util.h"

namespace IPC {

// IPC channels on Windows use named pipes (CreateNamedPipe()) with
// channel ids as the pipe names.  Channels on POSIX use sockets as
// pipes  These don't quite line up.
//
// When creating a child subprocess we use a socket pair and the parent side of
// the fork arranges it such that the initial control channel ends up on the
// magic file descriptor kPrimaryIPCChannel in the child.  Future
// connections (file descriptors) can then be passed via that
// connection via sendmsg().
//
// A POSIX IPC channel can also be set up as a server for a bound UNIX domain
// socket, and will handle multiple connect and disconnect sequences.  Currently
// it is limited to one connection at a time.

//------------------------------------------------------------------------------
namespace {

// The PipeMap class works around this quirk related to unit tests:
//
// When running as a server, we install the client socket in a
// specific file descriptor number (@kPrimaryIPCChannel). However, we
// also have to support the case where we are running unittests in the
// same process.  (We do not support forking without execing.)
//
// Case 1: normal running
//   The IPC server object will install a mapping in PipeMap from the
//   name which it was given to the client pipe. When forking the client, the
//   GetClientFileDescriptorMapping will ensure that the socket is installed in
//   the magic slot (@kPrimaryIPCChannel). The client will search for the
//   mapping, but it won't find any since we are in a new process. Thus the
//   magic fd number is returned. Once the client connects, the server will
//   close its copy of the client socket and remove the mapping.
//
// Case 2: unittests - client and server in the same process
//   The IPC server will install a mapping as before. The client will search
//   for a mapping and find out. It duplicates the file descriptor and
//   connects. Once the client connects, the server will close the original
//   copy of the client socket and remove the mapping. Thus, when the client
//   object closes, it will close the only remaining copy of the client socket
//   in the fd table and the server will see EOF on its side.
//
// TODO(port): a client process cannot connect to multiple IPC channels with
// this scheme.

class PipeMap {
 public:
  static PipeMap* GetInstance() {
    return Singleton<PipeMap>::get();
  }

  ~PipeMap() {
    // Shouldn't have left over pipes.
    DCHECK(map_.empty());
  }

  // Lookup a given channel id. Return -1 if not found.
  int Lookup(const std::string& channel_id) {
    base::AutoLock locked(lock_);

    ChannelToFDMap::const_iterator i = map_.find(channel_id);
    if (i == map_.end())
      return -1;
    return i->second;
  }

  // Remove the mapping for the given channel id. No error is signaled if the
  // channel_id doesn't exist
  void Remove(const std::string& channel_id) {
    base::AutoLock locked(lock_);
    map_.erase(channel_id);
  }

  // Insert a mapping from @channel_id to @fd. It's a fatal error to insert a
  // mapping if one already exists for the given channel_id
  void Insert(const std::string& channel_id, int fd) {
    base::AutoLock locked(lock_);
    DCHECK_NE(-1, fd);

    ChannelToFDMap::const_iterator i = map_.find(channel_id);
    CHECK(i == map_.end()) << "Creating second IPC server (fd " << fd << ") "
                           << "for '" << channel_id << "' while first "
                           << "(fd " << i->second << ") still exists";
    map_[channel_id] = fd;
  }

 private:
  base::Lock lock_;
  typedef std::map<std::string, int> ChannelToFDMap;
  ChannelToFDMap map_;

  friend struct DefaultSingletonTraits<PipeMap>;
#if defined(OS_ANDROID)
  friend void ::IPC::Channel::NotifyProcessForkedForTesting();
#endif
};

//------------------------------------------------------------------------------

bool SocketWriteErrorIsRecoverable() {
#if defined(OS_MACOSX)
  // On OS X if sendmsg() is trying to send fds between processes and there
  // isn't enough room in the output buffer to send the fd structure over
  // atomically then EMSGSIZE is returned.
  //
  // EMSGSIZE presents a problem since the system APIs can only call us when
  // there's room in the socket buffer and not when there is "enough" room.
  //
  // The current behavior is to return to the event loop when EMSGSIZE is
  // received and hopefull service another FD.  This is however still
  // technically a busy wait since the event loop will call us right back until
  // the receiver has read enough data to allow passing the FD over atomically.
  return errno == EAGAIN || errno == EMSGSIZE;
#else
  return errno == EAGAIN;
#endif  // OS_MACOSX
}

}  // namespace

#if defined(OS_ANDROID)
// When we fork for simple tests on Android, we can't 'exec', so we need to
// reset these entries manually to get the expected testing behavior.
void Channel::NotifyProcessForkedForTesting() {
  PipeMap::GetInstance()->map_.clear();
}
#endif

//------------------------------------------------------------------------------

#if defined(OS_LINUX)
int ChannelPosix::global_pid_ = 0;
#endif  // OS_LINUX

ChannelPosix::ChannelPosix(const IPC::ChannelHandle& channel_handle,
                           Mode mode, Listener* listener)
    : ChannelReader(listener),
      mode_(mode),
      peer_pid_(base::kNullProcessId),
      is_blocked_on_write_(false),
      waiting_connect_(true),
      message_send_bytes_written_(0),
      server_listen_pipe_(-1),
      pipe_(-1),
      client_pipe_(-1),
#if defined(IPC_USES_READWRITE)
      fd_pipe_(-1),
      remote_fd_pipe_(-1),
#endif  // IPC_USES_READWRITE
      pipe_name_(channel_handle.name),
      must_unlink_(false) {
  memset(input_cmsg_buf_, 0, sizeof(input_cmsg_buf_));
  if (!CreatePipe(channel_handle)) {
    // The pipe may have been closed already.
    const char *modestr = (mode_ & MODE_SERVER_FLAG) ? "server" : "client";
    LOG(WARNING) << "Unable to create pipe named \"" << channel_handle.name
                 << "\" in " << modestr << " mode";
  }
}

ChannelPosix::~ChannelPosix() {
  Close();
}

bool SocketPair(int* fd1, int* fd2) {
  int pipe_fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds) != 0) {
    PLOG(ERROR) << "socketpair()";
    return false;
  }

  // Set both ends to be non-blocking.
  if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1 ||
      fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK) == -1) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK)";
    if (IGNORE_EINTR(close(pipe_fds[0])) < 0)
      PLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(pipe_fds[1])) < 0)
      PLOG(ERROR) << "close";
    return false;
  }

  *fd1 = pipe_fds[0];
  *fd2 = pipe_fds[1];

  return true;
}

bool ChannelPosix::CreatePipe(
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(server_listen_pipe_ == -1 && pipe_ == -1);

  // Four possible cases:
  // 1) It's a channel wrapping a pipe that is given to us.
  // 2) It's for a named channel, so we create it.
  // 3) It's for a client that we implement ourself. This is used
  //    in single-process unittesting.
  // 4) It's the initial IPC channel:
  //   4a) Client side: Pull the pipe out of the GlobalDescriptors set.
  //   4b) Server side: create the pipe.

  int local_pipe = -1;
  if (channel_handle.socket.fd != -1) {
    // Case 1 from comment above.
    local_pipe = channel_handle.socket.fd;
#if defined(IPC_USES_READWRITE)
    // Test the socket passed into us to make sure it is nonblocking.
    // We don't want to call read/write on a blocking socket.
    int value = fcntl(local_pipe, F_GETFL);
    if (value == -1) {
      PLOG(ERROR) << "fcntl(F_GETFL) " << pipe_name_;
      return false;
    }
    if (!(value & O_NONBLOCK)) {
      LOG(ERROR) << "Socket " << pipe_name_ << " must be O_NONBLOCK";
      return false;
    }
#endif   // IPC_USES_READWRITE
  } else if (mode_ & MODE_NAMED_FLAG) {
    // Case 2 from comment above.
    if (mode_ & MODE_SERVER_FLAG) {
      if (!CreateServerUnixDomainSocket(base::FilePath(pipe_name_),
                                        &local_pipe)) {
        return false;
      }
      must_unlink_ = true;
    } else if (mode_ & MODE_CLIENT_FLAG) {
      if (!CreateClientUnixDomainSocket(base::FilePath(pipe_name_),
                                        &local_pipe)) {
        return false;
      }
    } else {
      LOG(ERROR) << "Bad mode: " << mode_;
      return false;
    }
  } else {
    local_pipe = PipeMap::GetInstance()->Lookup(pipe_name_);
    if (mode_ & MODE_CLIENT_FLAG) {
      if (local_pipe != -1) {
        // Case 3 from comment above.
        // We only allow one connection.
        local_pipe = HANDLE_EINTR(dup(local_pipe));
        PipeMap::GetInstance()->Remove(pipe_name_);
      } else {
        // Case 4a from comment above.
        // Guard against inappropriate reuse of the initial IPC channel.  If
        // an IPC channel closes and someone attempts to reuse it by name, the
        // initial channel must not be recycled here.  http://crbug.com/26754.
        static bool used_initial_channel = false;
        if (used_initial_channel) {
          LOG(FATAL) << "Denying attempt to reuse initial IPC channel for "
                     << pipe_name_;
          return false;
        }
        used_initial_channel = true;

        local_pipe =
            base::GlobalDescriptors::GetInstance()->Get(kPrimaryIPCChannel);
      }
    } else if (mode_ & MODE_SERVER_FLAG) {
      // Case 4b from comment above.
      if (local_pipe != -1) {
        LOG(ERROR) << "Server already exists for " << pipe_name_;
        return false;
      }
      base::AutoLock lock(client_pipe_lock_);
      if (!SocketPair(&local_pipe, &client_pipe_))
        return false;
      PipeMap::GetInstance()->Insert(pipe_name_, client_pipe_);
    } else {
      LOG(ERROR) << "Bad mode: " << mode_;
      return false;
    }
  }

#if defined(IPC_USES_READWRITE)
  // Create a dedicated socketpair() for exchanging file descriptors.
  // See comments for IPC_USES_READWRITE for details.
  if (mode_ & MODE_CLIENT_FLAG) {
    if (!SocketPair(&fd_pipe_, &remote_fd_pipe_)) {
      return false;
    }
  }
#endif  // IPC_USES_READWRITE

  if ((mode_ & MODE_SERVER_FLAG) && (mode_ & MODE_NAMED_FLAG)) {
    server_listen_pipe_ = local_pipe;
    local_pipe = -1;
  }

  pipe_ = local_pipe;
  return true;
}

bool ChannelPosix::Connect() {
  if (server_listen_pipe_ == -1 && pipe_ == -1) {
    DLOG(WARNING) << "Channel creation failed: " << pipe_name_;
    return false;
  }

  bool did_connect = true;
  if (server_listen_pipe_ != -1) {
    // Watch the pipe for connections, and turn any connections into
    // active sockets.
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        server_listen_pipe_,
        true,
        base::MessageLoopForIO::WATCH_READ,
        &server_listen_connection_watcher_,
        this);
  } else {
    did_connect = AcceptConnection();
  }
  return did_connect;
}

void ChannelPosix::CloseFileDescriptors(Message* msg) {
#if defined(OS_MACOSX)
  // There is a bug on OSX which makes it dangerous to close
  // a file descriptor while it is in transit. So instead we
  // store the file descriptor in a set and send a message to
  // the recipient, which is queued AFTER the message that
  // sent the FD. The recipient will reply to the message,
  // letting us know that it is now safe to close the file
  // descriptor. For more information, see:
  // http://crbug.com/298276
  std::vector<int> to_close;
  msg->file_descriptor_set()->ReleaseFDsToClose(&to_close);
  for (size_t i = 0; i < to_close.size(); i++) {
    fds_to_close_.insert(to_close[i]);
    QueueCloseFDMessage(to_close[i], 2);
  }
#else
  msg->file_descriptor_set()->CommitAll();
#endif
}

bool ChannelPosix::ProcessOutgoingMessages() {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  if (output_queue_.empty())
    return true;

  if (pipe_ == -1)
    return false;

  // Write out all the messages we can till the write blocks or there are no
  // more outgoing messages.
  while (!output_queue_.empty()) {
    Message* msg = output_queue_.front();

    size_t amt_to_write = msg->size() - message_send_bytes_written_;
    DCHECK_NE(0U, amt_to_write);
    const char* out_bytes = reinterpret_cast<const char*>(msg->data()) +
        message_send_bytes_written_;

    struct msghdr msgh = {0};
    struct iovec iov = {const_cast<char*>(out_bytes), amt_to_write};
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    char buf[CMSG_SPACE(
        sizeof(int) * FileDescriptorSet::kMaxDescriptorsPerMessage)];

    ssize_t bytes_written = 1;
    int fd_written = -1;

    if (message_send_bytes_written_ == 0 &&
        !msg->file_descriptor_set()->empty()) {
      // This is the first chunk of a message which has descriptors to send
      struct cmsghdr *cmsg;
      const unsigned num_fds = msg->file_descriptor_set()->size();

      DCHECK(num_fds <= FileDescriptorSet::kMaxDescriptorsPerMessage);
      if (msg->file_descriptor_set()->ContainsDirectoryDescriptor()) {
        LOG(FATAL) << "Panic: attempting to transport directory descriptor over"
                      " IPC. Aborting to maintain sandbox isolation.";
        // If you have hit this then something tried to send a file descriptor
        // to a directory over an IPC channel. Since IPC channels span
        // sandboxes this is very bad: the receiving process can use openat
        // with ".." elements in the path in order to reach the real
        // filesystem.
      }

      msgh.msg_control = buf;
      msgh.msg_controllen = CMSG_SPACE(sizeof(int) * num_fds);
      cmsg = CMSG_FIRSTHDR(&msgh);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN(sizeof(int) * num_fds);
      msg->file_descriptor_set()->GetDescriptors(
          reinterpret_cast<int*>(CMSG_DATA(cmsg)));
      msgh.msg_controllen = cmsg->cmsg_len;

      // DCHECK_LE above already checks that
      // num_fds < kMaxDescriptorsPerMessage so no danger of overflow.
      msg->header()->num_fds = static_cast<uint16>(num_fds);

#if defined(IPC_USES_READWRITE)
      if (!IsHelloMessage(*msg)) {
        // Only the Hello message sends the file descriptor with the message.
        // Subsequently, we can send file descriptors on the dedicated
        // fd_pipe_ which makes Seccomp sandbox operation more efficient.
        struct iovec fd_pipe_iov = { const_cast<char *>(""), 1 };
        msgh.msg_iov = &fd_pipe_iov;
        fd_written = fd_pipe_;
        bytes_written = HANDLE_EINTR(sendmsg(fd_pipe_, &msgh, MSG_DONTWAIT));
        msgh.msg_iov = &iov;
        msgh.msg_controllen = 0;
        if (bytes_written > 0) {
          CloseFileDescriptors(msg);
        }
      }
#endif  // IPC_USES_READWRITE
    }

    if (bytes_written == 1) {
      fd_written = pipe_;
#if defined(IPC_USES_READWRITE)
      if ((mode_ & MODE_CLIENT_FLAG) && IsHelloMessage(*msg)) {
        DCHECK_EQ(msg->file_descriptor_set()->size(), 1U);
      }
      if (!msgh.msg_controllen) {
        bytes_written = HANDLE_EINTR(write(pipe_, out_bytes, amt_to_write));
      } else
#endif  // IPC_USES_READWRITE
      {
        bytes_written = HANDLE_EINTR(sendmsg(pipe_, &msgh, MSG_DONTWAIT));
      }
    }
    if (bytes_written > 0)
      CloseFileDescriptors(msg);

    if (bytes_written < 0 && !SocketWriteErrorIsRecoverable()) {
      // We can't close the pipe here, because calling OnChannelError
      // may destroy this object, and that would be bad if we are
      // called from Send(). Instead, we return false and hope the
      // caller will close the pipe. If they do not, the pipe will
      // still be closed next time OnFileCanReadWithoutBlocking is
      // called.
#if defined(OS_MACOSX)
      // On OSX writing to a pipe with no listener returns EPERM.
      if (errno == EPERM) {
        return false;
      }
#endif  // OS_MACOSX
      if (errno == EPIPE) {
        return false;
      }
      PLOG(ERROR) << "pipe error on "
                  << fd_written
                  << " Currently writing message of size: "
                  << msg->size();
      return false;
    }

    if (static_cast<size_t>(bytes_written) != amt_to_write) {
      if (bytes_written > 0) {
        // If write() fails with EAGAIN then bytes_written will be -1.
        message_send_bytes_written_ += bytes_written;
      }

      // Tell libevent to call us back once things are unblocked.
      is_blocked_on_write_ = true;
      base::MessageLoopForIO::current()->WatchFileDescriptor(
          pipe_,
          false,  // One shot
          base::MessageLoopForIO::WATCH_WRITE,
          &write_watcher_,
          this);
      return true;
    } else {
      message_send_bytes_written_ = 0;

      // Message sent OK!
      DVLOG(2) << "sent message @" << msg << " on channel @" << this
               << " with type " << msg->type() << " on fd " << pipe_;
      delete output_queue_.front();
      output_queue_.pop();
    }
  }
  return true;
}

bool ChannelPosix::Send(Message* message) {
  DVLOG(2) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type()
           << " (" << output_queue_.size() << " in queue)";

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::GetInstance()->OnSendMessage(message, "");
#endif  // IPC_MESSAGE_LOG_ENABLED

  message->TraceMessageBegin();
  output_queue_.push(message);
  if (!is_blocked_on_write_ && !waiting_connect_) {
    return ProcessOutgoingMessages();
  }

  return true;
}

int ChannelPosix::GetClientFileDescriptor() const {
  base::AutoLock lock(client_pipe_lock_);
  return client_pipe_;
}

int ChannelPosix::TakeClientFileDescriptor() {
  base::AutoLock lock(client_pipe_lock_);
  int fd = client_pipe_;
  if (client_pipe_ != -1) {
    PipeMap::GetInstance()->Remove(pipe_name_);
    client_pipe_ = -1;
  }
  return fd;
}

void ChannelPosix::CloseClientFileDescriptor() {
  base::AutoLock lock(client_pipe_lock_);
  if (client_pipe_ != -1) {
    PipeMap::GetInstance()->Remove(pipe_name_);
    if (IGNORE_EINTR(close(client_pipe_)) < 0)
      PLOG(ERROR) << "close " << pipe_name_;
    client_pipe_ = -1;
  }
}

bool ChannelPosix::AcceptsConnections() const {
  return server_listen_pipe_ != -1;
}

bool ChannelPosix::HasAcceptedConnection() const {
  return AcceptsConnections() && pipe_ != -1;
}

bool ChannelPosix::GetPeerEuid(uid_t* peer_euid) const {
  DCHECK(!(mode_ & MODE_SERVER) || HasAcceptedConnection());
  return IPC::GetPeerEuid(pipe_, peer_euid);
}

void ChannelPosix::ResetToAcceptingConnectionState() {
  // Unregister libevent for the unix domain socket and close it.
  read_watcher_.StopWatchingFileDescriptor();
  write_watcher_.StopWatchingFileDescriptor();
  if (pipe_ != -1) {
    if (IGNORE_EINTR(close(pipe_)) < 0)
      PLOG(ERROR) << "close pipe_ " << pipe_name_;
    pipe_ = -1;
  }
#if defined(IPC_USES_READWRITE)
  if (fd_pipe_ != -1) {
    if (IGNORE_EINTR(close(fd_pipe_)) < 0)
      PLOG(ERROR) << "close fd_pipe_ " << pipe_name_;
    fd_pipe_ = -1;
  }
  if (remote_fd_pipe_ != -1) {
    if (IGNORE_EINTR(close(remote_fd_pipe_)) < 0)
      PLOG(ERROR) << "close remote_fd_pipe_ " << pipe_name_;
    remote_fd_pipe_ = -1;
  }
#endif  // IPC_USES_READWRITE

  while (!output_queue_.empty()) {
    Message* m = output_queue_.front();
    output_queue_.pop();
    delete m;
  }

  // Close any outstanding, received file descriptors.
  ClearInputFDs();

#if defined(OS_MACOSX)
  // Clear any outstanding, sent file descriptors.
  for (std::set<int>::iterator i = fds_to_close_.begin();
       i != fds_to_close_.end();
       ++i) {
    if (IGNORE_EINTR(close(*i)) < 0)
      PLOG(ERROR) << "close";
  }
  fds_to_close_.clear();
#endif
}

// static
bool ChannelPosix::IsNamedServerInitialized(
    const std::string& channel_id) {
  return base::PathExists(base::FilePath(channel_id));
}

#if defined(OS_LINUX)
// static
void ChannelPosix::SetGlobalPid(int pid) {
  global_pid_ = pid;
}
#endif  // OS_LINUX

// Called by libevent when we can read from the pipe without blocking.
void ChannelPosix::OnFileCanReadWithoutBlocking(int fd) {
  if (fd == server_listen_pipe_) {
    int new_pipe = 0;
    if (!ServerAcceptConnection(server_listen_pipe_, &new_pipe) ||
        new_pipe < 0) {
      Close();
      listener()->OnChannelListenError();
    }

    if (pipe_ != -1) {
      // We already have a connection. We only handle one at a time.
      // close our new descriptor.
      if (HANDLE_EINTR(shutdown(new_pipe, SHUT_RDWR)) < 0)
        DPLOG(ERROR) << "shutdown " << pipe_name_;
      if (IGNORE_EINTR(close(new_pipe)) < 0)
        DPLOG(ERROR) << "close " << pipe_name_;
      listener()->OnChannelDenied();
      return;
    }
    pipe_ = new_pipe;

    if ((mode_ & MODE_OPEN_ACCESS_FLAG) == 0) {
      // Verify that the IPC channel peer is running as the same user.
      uid_t client_euid;
      if (!GetPeerEuid(&client_euid)) {
        DLOG(ERROR) << "Unable to query client euid";
        ResetToAcceptingConnectionState();
        return;
      }
      if (client_euid != geteuid()) {
        DLOG(WARNING) << "Client euid is not authorised";
        ResetToAcceptingConnectionState();
        return;
      }
    }

    if (!AcceptConnection()) {
      NOTREACHED() << "AcceptConnection should not fail on server";
    }
    waiting_connect_ = false;
  } else if (fd == pipe_) {
    if (waiting_connect_ && (mode_ & MODE_SERVER_FLAG)) {
      waiting_connect_ = false;
    }
    if (!ProcessIncomingMessages()) {
      // ClosePipeOnError may delete this object, so we mustn't call
      // ProcessOutgoingMessages.
      ClosePipeOnError();
      return;
    }
  } else {
    NOTREACHED() << "Unknown pipe " << fd;
  }

  // If we're a server and handshaking, then we want to make sure that we
  // only send our handshake message after we've processed the client's.
  // This gives us a chance to kill the client if the incoming handshake
  // is invalid. This also flushes any closefd messages.
  if (!is_blocked_on_write_) {
    if (!ProcessOutgoingMessages()) {
      ClosePipeOnError();
    }
  }
}

// Called by libevent when we can write to the pipe without blocking.
void ChannelPosix::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK_EQ(pipe_, fd);
  is_blocked_on_write_ = false;
  if (!ProcessOutgoingMessages()) {
    ClosePipeOnError();
  }
}

bool ChannelPosix::AcceptConnection() {
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      pipe_, true, base::MessageLoopForIO::WATCH_READ, &read_watcher_, this);
  QueueHelloMessage();

  if (mode_ & MODE_CLIENT_FLAG) {
    // If we are a client we want to send a hello message out immediately.
    // In server mode we will send a hello message when we receive one from a
    // client.
    waiting_connect_ = false;
    return ProcessOutgoingMessages();
  } else if (mode_ & MODE_SERVER_FLAG) {
    waiting_connect_ = true;
    return true;
  } else {
    NOTREACHED();
    return false;
  }
}

void ChannelPosix::ClosePipeOnError() {
  if (HasAcceptedConnection()) {
    ResetToAcceptingConnectionState();
    listener()->OnChannelError();
  } else {
    Close();
    if (AcceptsConnections()) {
      listener()->OnChannelListenError();
    } else {
      listener()->OnChannelError();
    }
  }
}

int ChannelPosix::GetHelloMessageProcId() {
  int pid = base::GetCurrentProcId();
#if defined(OS_LINUX)
  // Our process may be in a sandbox with a separate PID namespace.
  if (global_pid_) {
    pid = global_pid_;
  }
#endif
  return pid;
}

void ChannelPosix::QueueHelloMessage() {
  // Create the Hello message
  scoped_ptr<Message> msg(new Message(MSG_ROUTING_NONE,
                                      HELLO_MESSAGE_TYPE,
                                      IPC::Message::PRIORITY_NORMAL));
  if (!msg->WriteInt(GetHelloMessageProcId())) {
    NOTREACHED() << "Unable to pickle hello message proc id";
  }
#if defined(IPC_USES_READWRITE)
  scoped_ptr<Message> hello;
  if (remote_fd_pipe_ != -1) {
    if (!msg->WriteFileDescriptor(base::FileDescriptor(remote_fd_pipe_,
                                                       false))) {
      NOTREACHED() << "Unable to pickle hello message file descriptors";
    }
    DCHECK_EQ(msg->file_descriptor_set()->size(), 1U);
  }
#endif  // IPC_USES_READWRITE
  output_queue_.push(msg.release());
}

ChannelPosix::ReadState ChannelPosix::ReadData(
    char* buffer,
    int buffer_len,
    int* bytes_read) {
  if (pipe_ == -1)
    return READ_FAILED;

  struct msghdr msg = {0};

  struct iovec iov = {buffer, static_cast<size_t>(buffer_len)};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  msg.msg_control = input_cmsg_buf_;

  // recvmsg() returns 0 if the connection has closed or EAGAIN if no data
  // is waiting on the pipe.
#if defined(IPC_USES_READWRITE)
  if (fd_pipe_ >= 0) {
    *bytes_read = HANDLE_EINTR(read(pipe_, buffer, buffer_len));
    msg.msg_controllen = 0;
  } else
#endif  // IPC_USES_READWRITE
  {
    msg.msg_controllen = sizeof(input_cmsg_buf_);
    *bytes_read = HANDLE_EINTR(recvmsg(pipe_, &msg, MSG_DONTWAIT));
  }
  if (*bytes_read < 0) {
    if (errno == EAGAIN) {
      return READ_PENDING;
#if defined(OS_MACOSX)
    } else if (errno == EPERM) {
      // On OSX, reading from a pipe with no listener returns EPERM
      // treat this as a special case to prevent spurious error messages
      // to the console.
      return READ_FAILED;
#endif  // OS_MACOSX
    } else if (errno == ECONNRESET || errno == EPIPE) {
      return READ_FAILED;
    } else {
      PLOG(ERROR) << "pipe error (" << pipe_ << ")";
      return READ_FAILED;
    }
  } else if (*bytes_read == 0) {
    // The pipe has closed...
    return READ_FAILED;
  }
  DCHECK(*bytes_read);

  CloseClientFileDescriptor();

  // Read any file descriptors from the message.
  if (!ExtractFileDescriptorsFromMsghdr(&msg))
    return READ_FAILED;
  return READ_SUCCEEDED;
}

#if defined(IPC_USES_READWRITE)
bool ChannelPosix::ReadFileDescriptorsFromFDPipe() {
  char dummy;
  struct iovec fd_pipe_iov = { &dummy, 1 };

  struct msghdr msg = { 0 };
  msg.msg_iov = &fd_pipe_iov;
  msg.msg_iovlen = 1;
  msg.msg_control = input_cmsg_buf_;
  msg.msg_controllen = sizeof(input_cmsg_buf_);
  ssize_t bytes_received = HANDLE_EINTR(recvmsg(fd_pipe_, &msg, MSG_DONTWAIT));

  if (bytes_received != 1)
    return true;  // No message waiting.

  if (!ExtractFileDescriptorsFromMsghdr(&msg))
    return false;
  return true;
}
#endif

// On Posix, we need to fix up the file descriptors before the input message
// is dispatched.
//
// This will read from the input_fds_ (READWRITE mode only) and read more
// handles from the FD pipe if necessary.
bool ChannelPosix::WillDispatchInputMessage(Message* msg) {
  uint16 header_fds = msg->header()->num_fds;
  if (!header_fds)
    return true;  // Nothing to do.

  // The message has file descriptors.
  const char* error = NULL;
  if (header_fds > input_fds_.size()) {
    // The message has been completely received, but we didn't get
    // enough file descriptors.
#if defined(IPC_USES_READWRITE)
    if (!ReadFileDescriptorsFromFDPipe())
      return false;
    if (header_fds > input_fds_.size())
#endif  // IPC_USES_READWRITE
      error = "Message needs unreceived descriptors";
  }

  if (header_fds > FileDescriptorSet::kMaxDescriptorsPerMessage)
    error = "Message requires an excessive number of descriptors";

  if (error) {
    LOG(WARNING) << error
                 << " channel:" << this
                 << " message-type:" << msg->type()
                 << " header()->num_fds:" << header_fds;
    // Abort the connection.
    ClearInputFDs();
    return false;
  }

  // The shenaniganery below with &foo.front() requires input_fds_ to have
  // contiguous underlying storage (such as a simple array or a std::vector).
  // This is why the header warns not to make input_fds_ a deque<>.
  msg->file_descriptor_set()->SetDescriptors(&input_fds_.front(),
                                             header_fds);
  input_fds_.erase(input_fds_.begin(), input_fds_.begin() + header_fds);
  return true;
}

bool ChannelPosix::DidEmptyInputBuffers() {
  // When the input data buffer is empty, the fds should be too. If this is
  // not the case, we probably have a rogue renderer which is trying to fill
  // our descriptor table.
  return input_fds_.empty();
}

bool ChannelPosix::ExtractFileDescriptorsFromMsghdr(msghdr* msg) {
  // Check that there are any control messages. On OSX, CMSG_FIRSTHDR will
  // return an invalid non-NULL pointer in the case that controllen == 0.
  if (msg->msg_controllen == 0)
    return true;

  for (cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
       cmsg;
       cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      DCHECK_EQ(0U, payload_len % sizeof(int));
      const int* file_descriptors = reinterpret_cast<int*>(CMSG_DATA(cmsg));
      unsigned num_file_descriptors = payload_len / 4;
      input_fds_.insert(input_fds_.end(),
                        file_descriptors,
                        file_descriptors + num_file_descriptors);

      // Check this after adding the FDs so we don't leak them.
      if (msg->msg_flags & MSG_CTRUNC) {
        ClearInputFDs();
        return false;
      }

      return true;
    }
  }

  // No file descriptors found, but that's OK.
  return true;
}

void ChannelPosix::ClearInputFDs() {
  for (size_t i = 0; i < input_fds_.size(); ++i) {
    if (IGNORE_EINTR(close(input_fds_[i])) < 0)
      PLOG(ERROR) << "close ";
  }
  input_fds_.clear();
}

void ChannelPosix::QueueCloseFDMessage(int fd, int hops) {
  switch (hops) {
    case 1:
    case 2: {
      // Create the message
      scoped_ptr<Message> msg(new Message(MSG_ROUTING_NONE,
                                          CLOSE_FD_MESSAGE_TYPE,
                                          IPC::Message::PRIORITY_NORMAL));
      if (!msg->WriteInt(hops - 1) || !msg->WriteInt(fd)) {
        NOTREACHED() << "Unable to pickle close fd.";
      }
      // Send(msg.release());
      output_queue_.push(msg.release());
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void ChannelPosix::HandleInternalMessage(const Message& msg) {
  // The Hello message contains only the process id.
  PickleIterator iter(msg);

  switch (msg.type()) {
    default:
      NOTREACHED();
      break;

    case Channel::HELLO_MESSAGE_TYPE:
      int pid;
      if (!msg.ReadInt(&iter, &pid))
        NOTREACHED();

#if defined(IPC_USES_READWRITE)
      if (mode_ & MODE_SERVER_FLAG) {
        // With IPC_USES_READWRITE, the Hello message from the client to the
        // server also contains the fd_pipe_, which  will be used for all
        // subsequent file descriptor passing.
        DCHECK_EQ(msg.file_descriptor_set()->size(), 1U);
        base::FileDescriptor descriptor;
        if (!msg.ReadFileDescriptor(&iter, &descriptor)) {
          NOTREACHED();
        }
        fd_pipe_ = descriptor.fd;
        CHECK(descriptor.auto_close);
      }
#endif  // IPC_USES_READWRITE
      peer_pid_ = pid;
      listener()->OnChannelConnected(pid);
      break;

#if defined(OS_MACOSX)
    case Channel::CLOSE_FD_MESSAGE_TYPE:
      int fd, hops;
      if (!msg.ReadInt(&iter, &hops))
        NOTREACHED();
      if (!msg.ReadInt(&iter, &fd))
        NOTREACHED();
      if (hops == 0) {
        if (fds_to_close_.erase(fd) > 0) {
          if (IGNORE_EINTR(close(fd)) < 0)
            PLOG(ERROR) << "close";
        } else {
          NOTREACHED();
        }
      } else {
        QueueCloseFDMessage(fd, hops);
      }
      break;
#endif
  }
}

void ChannelPosix::Close() {
  // Close can be called multiple time, so we need to make sure we're
  // idempotent.

  ResetToAcceptingConnectionState();

  if (must_unlink_) {
    unlink(pipe_name_.c_str());
    must_unlink_ = false;
  }
  if (server_listen_pipe_ != -1) {
    if (IGNORE_EINTR(close(server_listen_pipe_)) < 0)
      DPLOG(ERROR) << "close " << server_listen_pipe_;
    server_listen_pipe_ = -1;
    // Unregister libevent for the listening socket and close it.
    server_listen_connection_watcher_.StopWatchingFileDescriptor();
  }

  CloseClientFileDescriptor();
}

base::ProcessId ChannelPosix::GetPeerPID() const {
  return peer_pid_;
}

//------------------------------------------------------------------------------
// Channel's methods

// static
scoped_ptr<Channel> Channel::Create(
    const IPC::ChannelHandle &channel_handle, Mode mode, Listener* listener) {
  return make_scoped_ptr(new ChannelPosix(
      channel_handle, mode, listener)).PassAs<Channel>();
}

// static
std::string Channel::GenerateVerifiedChannelID(const std::string& prefix) {
  // A random name is sufficient validation on posix systems, so we don't need
  // an additional shared secret.

  std::string id = prefix;
  if (!id.empty())
    id.append(".");

  return id.append(GenerateUniqueRandomChannelID());
}


bool Channel::IsNamedServerInitialized(
    const std::string& channel_id) {
  return ChannelPosix::IsNamedServerInitialized(channel_id);
}

#if defined(OS_LINUX)
// static
void Channel::SetGlobalPid(int pid) {
  ChannelPosix::SetGlobalPid(pid);
}
#endif  // OS_LINUX

}  // namespace IPC

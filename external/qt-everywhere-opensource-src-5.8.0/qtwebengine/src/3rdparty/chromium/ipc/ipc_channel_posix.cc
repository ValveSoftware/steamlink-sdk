// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "base/memory/ptr_util.h"

#if defined(OS_OPENBSD)
#include <sys/uio.h>
#endif

#if !defined(OS_NACL_NONSFI)
#include <sys/un.h>
#endif

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ipc/attachment_broker.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
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
  static PipeMap* GetInstance() { return base::Singleton<PipeMap>::get(); }

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

  friend struct base::DefaultSingletonTraits<PipeMap>;
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
                           Mode mode,
                           Listener* listener)
    : ChannelReader(listener),
      mode_(mode),
      peer_pid_(base::kNullProcessId),
      is_blocked_on_write_(false),
      waiting_connect_(true),
      message_send_bytes_written_(0),
      pipe_name_(channel_handle.name),
      in_dtor_(false),
      must_unlink_(false) {
  if (!CreatePipe(channel_handle)) {
    // The pipe may have been closed already.
    const char *modestr = (mode_ & MODE_SERVER_FLAG) ? "server" : "client";
    LOG(WARNING) << "Unable to create pipe named \"" << channel_handle.name
                 << "\" in " << modestr << " mode";
  }
}

ChannelPosix::~ChannelPosix() {
  in_dtor_ = true;
  CleanUp();
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
  DCHECK(!server_listen_pipe_.is_valid() && !pipe_.is_valid());

  // Four possible cases:
  // 1) It's a channel wrapping a pipe that is given to us.
  // 2) It's for a named channel, so we create it.
  // 3) It's for a client that we implement ourself. This is used
  //    in single-process unittesting.
  // 4) It's the initial IPC channel:
  //   4a) Client side: Pull the pipe out of the GlobalDescriptors set.
  //   4b) Server side: create the pipe.

  base::ScopedFD local_pipe;
  if (channel_handle.socket.fd != -1) {
    // Case 1 from comment above.
    local_pipe.reset(channel_handle.socket.fd);
  } else if (mode_ & MODE_NAMED_FLAG) {
#if defined(OS_NACL_NONSFI)
    LOG(FATAL)
        << "IPC channels in nacl_helper_nonsfi should not be in NAMED mode.";
#else
    // Case 2 from comment above.
    int local_pipe_fd = -1;

    if (mode_ & MODE_SERVER_FLAG) {
      if (!CreateServerUnixDomainSocket(base::FilePath(pipe_name_),
                                        &local_pipe_fd)) {
        return false;
      }

      must_unlink_ = true;
    } else if (mode_ & MODE_CLIENT_FLAG) {
      if (!CreateClientUnixDomainSocket(base::FilePath(pipe_name_),
                                        &local_pipe_fd)) {
        return false;
      }
    } else {
      LOG(ERROR) << "Bad mode: " << mode_;
      return false;
    }

    local_pipe.reset(local_pipe_fd);
#endif  // !defined(OS_NACL_NONSFI)
  } else {
    local_pipe.reset(PipeMap::GetInstance()->Lookup(pipe_name_));
    if (mode_ & MODE_CLIENT_FLAG) {
      if (local_pipe.is_valid()) {
        // Case 3 from comment above.
        // We only allow one connection.
        local_pipe.reset(HANDLE_EINTR(dup(local_pipe.release())));
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

        local_pipe.reset(
            base::GlobalDescriptors::GetInstance()->Get(kPrimaryIPCChannel));
      }
    } else if (mode_ & MODE_SERVER_FLAG) {
      // Case 4b from comment above.
      if (local_pipe.is_valid()) {
        LOG(ERROR) << "Server already exists for " << pipe_name_;
        // This is a client side pipe registered by other server and
        // shouldn't be closed.
        ignore_result(local_pipe.release());
        return false;
      }
      base::AutoLock lock(client_pipe_lock_);
      int local_pipe_fd = -1, client_pipe_fd = -1;
      if (!SocketPair(&local_pipe_fd, &client_pipe_fd))
        return false;
      local_pipe.reset(local_pipe_fd);
      client_pipe_.reset(client_pipe_fd);
      PipeMap::GetInstance()->Insert(pipe_name_, client_pipe_fd);
    } else {
      LOG(ERROR) << "Bad mode: " << mode_;
      return false;
    }
  }

  if ((mode_ & MODE_SERVER_FLAG) && (mode_ & MODE_NAMED_FLAG)) {
#if defined(OS_NACL_NONSFI)
    LOG(FATAL) << "IPC channels in nacl_helper_nonsfi "
               << "should not be in NAMED or SERVER mode.";
#else
    server_listen_pipe_.reset(local_pipe.release());
#endif
  } else {
    pipe_.reset(local_pipe.release());
  }
  return true;
}

bool ChannelPosix::Connect() {
  WillConnect();

  if (!server_listen_pipe_.is_valid() && !pipe_.is_valid()) {
    DLOG(WARNING) << "Channel creation failed: " << pipe_name_;
    return false;
  }

  bool did_connect = true;
  if (server_listen_pipe_.is_valid()) {
#if defined(OS_NACL_NONSFI)
    LOG(FATAL) << "IPC channels in nacl_helper_nonsfi "
               << "should always be in client mode.";
#else
    // Watch the pipe for connections, and turn any connections into
    // active sockets.
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        server_listen_pipe_.get(),
        true,
        base::MessageLoopForIO::WATCH_READ,
        &server_listen_connection_watcher_,
        this);
#endif
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
  msg->attachment_set()->ReleaseFDsToClose(&to_close);
  for (size_t i = 0; i < to_close.size(); i++) {
    fds_to_close_.insert(to_close[i]);
    QueueCloseFDMessage(to_close[i], 2);
  }
#else
  msg->attachment_set()->CommitAllDescriptors();
#endif
}

bool ChannelPosix::ProcessOutgoingMessages() {
  if (waiting_connect_)
    return true;
  if (is_blocked_on_write_)
    return true;
  if (output_queue_.empty())
    return true;
  if (!pipe_.is_valid())
    return false;

  // Write out all the messages we can till the write blocks or there are no
  // more outgoing messages.
  while (!output_queue_.empty()) {
    OutputElement* element = output_queue_.front();

    size_t amt_to_write = element->size() - message_send_bytes_written_;
    DCHECK_NE(0U, amt_to_write);
    const char* out_bytes = reinterpret_cast<const char*>(element->data()) +
        message_send_bytes_written_;

    struct msghdr msgh = {0};
    struct iovec iov = {const_cast<char*>(out_bytes), amt_to_write};
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    char buf[CMSG_SPACE(sizeof(int) *
                        MessageAttachmentSet::kMaxDescriptorsPerMessage)];

    ssize_t bytes_written = 1;
    int fd_written = -1;

    Message* msg = element->get_message();
    if (message_send_bytes_written_ == 0 && msg &&
        msg->attachment_set()->num_non_brokerable_attachments()) {
      // This is the first chunk of a message which has descriptors to send
      struct cmsghdr *cmsg;
      const unsigned num_fds =
          msg->attachment_set()->num_non_brokerable_attachments();

      DCHECK(num_fds <= MessageAttachmentSet::kMaxDescriptorsPerMessage);
      if (msg->attachment_set()->ContainsDirectoryDescriptor()) {
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
      msg->attachment_set()->PeekDescriptors(
          reinterpret_cast<int*>(CMSG_DATA(cmsg)));
      msgh.msg_controllen = cmsg->cmsg_len;

      // DCHECK_LE above already checks that
      // num_fds < kMaxDescriptorsPerMessage so no danger of overflow.
      msg->header()->num_fds = static_cast<uint16_t>(num_fds);
    }

    if (bytes_written == 1) {
      fd_written = pipe_.get();
      bytes_written = HANDLE_EINTR(sendmsg(pipe_.get(), &msgh, MSG_DONTWAIT));
    }
    if (bytes_written > 0 && msg)
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
                  << element->size();
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
          pipe_.get(),
          false,  // One shot
          base::MessageLoopForIO::WATCH_WRITE,
          &write_watcher_,
          this);
      return true;
    } else {
      message_send_bytes_written_ = 0;

      // Message sent OK!
      if (msg) {
        DVLOG(2) << "sent message @" << msg << " on channel @" << this
                 << " with type " << msg->type() << " on fd " << pipe_.get();
      } else {
        DVLOG(2) << "sent buffer @" << element->data() << " on channel @"
                 << this << " on fd " << pipe_.get();
      }
      delete output_queue_.front();
      output_queue_.pop();
    }
  }
  return true;
}

bool ChannelPosix::Send(Message* message) {
  DCHECK(!message->HasMojoHandles());
  DVLOG(2) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type()
           << " (" << output_queue_.size() << " in queue)";

  if (!prelim_queue_.empty()) {
    prelim_queue_.push(message);
    return true;
  }

  if (message->HasBrokerableAttachments() &&
      peer_pid_ == base::kNullProcessId) {
    prelim_queue_.push(message);
    return true;
  }

  return ProcessMessageForDelivery(message);
}

AttachmentBroker* ChannelPosix::GetAttachmentBroker() {
  return AttachmentBroker::GetGlobal();
}

int ChannelPosix::GetClientFileDescriptor() const {
  base::AutoLock lock(client_pipe_lock_);
  return client_pipe_.get();
}

base::ScopedFD ChannelPosix::TakeClientFileDescriptor() {
  base::AutoLock lock(client_pipe_lock_);
  if (!client_pipe_.is_valid())
    return base::ScopedFD();
  PipeMap::GetInstance()->Remove(pipe_name_);
  return std::move(client_pipe_);
}

void ChannelPosix::CloseClientFileDescriptor() {
  base::AutoLock lock(client_pipe_lock_);
  if (!client_pipe_.is_valid())
    return;
  PipeMap::GetInstance()->Remove(pipe_name_);
  client_pipe_.reset();
}

bool ChannelPosix::AcceptsConnections() const {
  return server_listen_pipe_.is_valid();
}

bool ChannelPosix::HasAcceptedConnection() const {
  return AcceptsConnections() && pipe_.is_valid();
}

#if !defined(OS_NACL_NONSFI)
// GetPeerEuid is not supported in nacl_helper_nonsfi.
bool ChannelPosix::GetPeerEuid(uid_t* peer_euid) const {
  DCHECK(!(mode_ & MODE_SERVER) || HasAcceptedConnection());
  return IPC::GetPeerEuid(pipe_.get(), peer_euid);
}
#endif

void ChannelPosix::ResetToAcceptingConnectionState() {
  // Unregister libevent for the unix domain socket and close it.
  read_watcher_.StopWatchingFileDescriptor();
  write_watcher_.StopWatchingFileDescriptor();
  ResetSafely(&pipe_);

  while (!output_queue_.empty()) {
    OutputElement* element = output_queue_.front();
    output_queue_.pop();
    if (element->get_message())
      CloseFileDescriptors(element->get_message());
    delete element;
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
// static
int ChannelPosix::GetGlobalPid() {
  return global_pid_;
}
#endif  // OS_LINUX

// Called by libevent when we can read from the pipe without blocking.
void ChannelPosix::OnFileCanReadWithoutBlocking(int fd) {
  if (fd == server_listen_pipe_.get()) {
#if defined(OS_NACL_NONSFI)
    LOG(FATAL)
        << "IPC channels in nacl_helper_nonsfi should not be SERVER mode.";
#else
    int new_pipe = 0;
    if (!ServerAcceptConnection(server_listen_pipe_.get(), &new_pipe) ||
        new_pipe < 0) {
      Close();
      listener()->OnChannelListenError();
    }

    if (pipe_.is_valid()) {
      // We already have a connection. We only handle one at a time.
      // close our new descriptor.
      if (HANDLE_EINTR(shutdown(new_pipe, SHUT_RDWR)) < 0)
        DPLOG(ERROR) << "shutdown " << pipe_name_;
      if (IGNORE_EINTR(close(new_pipe)) < 0)
        DPLOG(ERROR) << "close " << pipe_name_;
      listener()->OnChannelDenied();
      return;
    }
    pipe_.reset(new_pipe);

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
#endif
  } else if (fd == pipe_) {
    if (waiting_connect_ && (mode_ & MODE_SERVER_FLAG)) {
      waiting_connect_ = false;
    }
    if (ProcessIncomingMessages() == DISPATCH_ERROR) {
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
  if (!ProcessOutgoingMessages()) {
    ClosePipeOnError();
  }
}

// Called by libevent when we can write to the pipe without blocking.
void ChannelPosix::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK_EQ(pipe_.get(), fd);
  is_blocked_on_write_ = false;
  if (!ProcessOutgoingMessages()) {
    ClosePipeOnError();
  }
}

bool ChannelPosix::ProcessMessageForDelivery(Message* message) {
  // Sending a brokerable attachment requires a call to Channel::Send(), so
  // Send() may be re-entrant.
  if (message->HasBrokerableAttachments()) {
    DCHECK(GetAttachmentBroker());
    DCHECK(peer_pid_ != base::kNullProcessId);
    for (const scoped_refptr<IPC::BrokerableAttachment>& attachment :
         message->attachment_set()->GetBrokerableAttachments()) {
      if (!GetAttachmentBroker()->SendAttachmentToProcess(attachment,
                                                          peer_pid_)) {
        delete message;
        return false;
      }
    }
  }

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::GetInstance()->OnSendMessage(message, "");
#endif  // IPC_MESSAGE_LOG_ENABLED

  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "ChannelPosix::Send",
                         message->flags(),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // |output_queue_| takes ownership of |message|.
  OutputElement* element = new OutputElement(message);
  output_queue_.push(element);

  if (message->HasBrokerableAttachments()) {
    // |output_queue_| takes ownership of |ids.buffer|.
    Message::SerializedAttachmentIds ids =
        message->SerializedIdsOfBrokerableAttachments();
    output_queue_.push(new OutputElement(ids.buffer, ids.size));
  }

  return ProcessOutgoingMessages();
}

bool ChannelPosix::FlushPrelimQueue() {
  DCHECK_NE(peer_pid_, base::kNullProcessId);

  // Due to the possibly re-entrant nature of ProcessMessageForDelivery(),
  // |prelim_queue_| should appear empty.
  std::queue<Message*> prelim_queue;
  std::swap(prelim_queue_, prelim_queue);

  bool processing_error = false;
  while (!prelim_queue.empty()) {
    Message* m = prelim_queue.front();
    processing_error = !ProcessMessageForDelivery(m);
    prelim_queue.pop();
    if (processing_error)
      break;
  }

  // Delete any unprocessed messages.
  while (!prelim_queue.empty()) {
    Message* m = prelim_queue.front();
    delete m;
    prelim_queue.pop();
  }

  return !processing_error;
}

bool ChannelPosix::AcceptConnection() {
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      pipe_.get(),
      true,
      base::MessageLoopForIO::WATCH_READ,
      &read_watcher_,
      this);
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

int ChannelPosix::GetHelloMessageProcId() const {
#if defined(OS_NACL_NONSFI)
  // In nacl_helper_nonsfi, getpid() invoked by GetCurrentProcId() is not
  // allowed and would cause a SIGSYS crash because of the seccomp sandbox.
  return -1;
#else
  int pid = base::GetCurrentProcId();
#if defined(OS_LINUX)
  // Our process may be in a sandbox with a separate PID namespace.
  if (global_pid_) {
    pid = global_pid_;
  }
#endif  // defined(OS_LINUX)
  return pid;
#endif  // defined(OS_NACL_NONSFI)
}

void ChannelPosix::QueueHelloMessage() {
  // Create the Hello message
  std::unique_ptr<Message> msg(new Message(MSG_ROUTING_NONE, HELLO_MESSAGE_TYPE,
                                           IPC::Message::PRIORITY_NORMAL));
  if (!msg->WriteInt(GetHelloMessageProcId())) {
    NOTREACHED() << "Unable to pickle hello message proc id";
  }
  OutputElement* element = new OutputElement(msg.release());
  output_queue_.push(element);
}

ChannelPosix::ReadState ChannelPosix::ReadData(
    char* buffer,
    int buffer_len,
    int* bytes_read) {
  if (!pipe_.is_valid())
    return READ_FAILED;

  struct msghdr msg = {0};

  struct iovec iov = {buffer, static_cast<size_t>(buffer_len)};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char input_cmsg_buf[kMaxReadFDBuffer];
  msg.msg_control = input_cmsg_buf;

  // recvmsg() returns 0 if the connection has closed or EAGAIN if no data
  // is waiting on the pipe.
  msg.msg_controllen = sizeof(input_cmsg_buf);
  *bytes_read = HANDLE_EINTR(recvmsg(pipe_.get(), &msg, MSG_DONTWAIT));

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
      PLOG(ERROR) << "pipe error (" << pipe_.get() << ")";
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

bool ChannelPosix::ShouldDispatchInputMessage(Message* msg) {
  return true;
}

// On Posix, we need to fix up the file descriptors before the input message
// is dispatched.
//
// This will read from the input_fds_ (READWRITE mode only) and read more
// handles from the FD pipe if necessary.
bool ChannelPosix::GetNonBrokeredAttachments(Message* msg) {
  uint16_t header_fds = msg->header()->num_fds;
  if (!header_fds)
    return true;  // Nothing to do.

  // The message has file descriptors.
  const char* error = NULL;
  if (header_fds > input_fds_.size()) {
    // The message has been completely received, but we didn't get
    // enough file descriptors.
    error = "Message needs unreceived descriptors";
  }

  if (header_fds > MessageAttachmentSet::kMaxDescriptorsPerMessage)
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
  msg->attachment_set()->AddDescriptorsToOwn(&input_fds_.front(), header_fds);
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
      std::unique_ptr<Message> msg(new Message(MSG_ROUTING_NONE,
                                               CLOSE_FD_MESSAGE_TYPE,
                                               IPC::Message::PRIORITY_NORMAL));
      if (!msg->WriteInt(hops - 1) || !msg->WriteInt(fd)) {
        NOTREACHED() << "Unable to pickle close fd.";
      }

      OutputElement* element = new OutputElement(msg.release());
      output_queue_.push(element);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void ChannelPosix::HandleInternalMessage(const Message& msg) {
  // The Hello message contains only the process id.
  base::PickleIterator iter(msg);

  switch (msg.type()) {
    default:
      NOTREACHED();
      break;

    case Channel::HELLO_MESSAGE_TYPE:
      int pid;
      if (!iter.ReadInt(&pid))
        NOTREACHED();

      peer_pid_ = pid;
      listener()->OnChannelConnected(pid);

      if (!FlushPrelimQueue())
        ClosePipeOnError();

      if (IsAttachmentBrokerEndpoint() &&
          AttachmentBroker::GetGlobal() &&
          AttachmentBroker::GetGlobal()->IsPrivilegedBroker()) {
        AttachmentBroker::GetGlobal()->ReceivedPeerPid(pid);
      }
      break;

#if defined(OS_MACOSX)
    case Channel::CLOSE_FD_MESSAGE_TYPE:
      int fd, hops;
      if (!iter.ReadInt(&hops))
        NOTREACHED();
      if (!iter.ReadInt(&fd))
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

base::ProcessId ChannelPosix::GetSenderPID() {
  return GetPeerPID();
}

bool ChannelPosix::IsAttachmentBrokerEndpoint() {
  return is_attachment_broker_endpoint();
}

void ChannelPosix::Close() {
  // Close can be called multiple time, so we need to make sure we're
  // idempotent.

  ResetToAcceptingConnectionState();

  if (must_unlink_) {
    unlink(pipe_name_.c_str());
    must_unlink_ = false;
  }

  if (server_listen_pipe_.is_valid()) {
#if defined(OS_NACL_NONSFI)
    LOG(FATAL)
        << "IPC channels in nacl_helper_nonsfi should not be SERVER mode.";
#else
    server_listen_pipe_.reset();
    // Unregister libevent for the listening socket and close it.
    server_listen_connection_watcher_.StopWatchingFileDescriptor();
#endif
  }

  CloseClientFileDescriptor();
}

base::ProcessId ChannelPosix::GetPeerPID() const {
  return peer_pid_;
}

base::ProcessId ChannelPosix::GetSelfPID() const {
  return GetHelloMessageProcId();
}

void ChannelPosix::ResetSafely(base::ScopedFD* fd) {
  if (!in_dtor_) {
    fd->reset();
    return;
  }

  // crbug.com/449233
  // The CL [1] tightened the error check for closing FDs, but it turned
  // out that there are existing cases that hit the newly added check.
  // ResetSafely() is the workaround for that crash, turning it from
  // from PCHECK() to DPCHECK() so that it doesn't crash in production.
  // [1] https://crrev.com/ce44fef5fd60dd2be5c587d4b084bdcd36adcee4
  int fd_to_close = fd->release();
  if (-1 != fd_to_close) {
    int rv = IGNORE_EINTR(close(fd_to_close));
    DPCHECK(0 == rv);
  }
}

//------------------------------------------------------------------------------
// Channel's methods

// static
std::unique_ptr<Channel> Channel::Create(
    const IPC::ChannelHandle& channel_handle,
    Mode mode,
    Listener* listener) {
  return base::WrapUnique(new ChannelPosix(channel_handle, mode, listener));
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
int Channel::GetGlobalPid() {
  return ChannelPosix::GetGlobalPid();
}
#endif  // OS_LINUX

}  // namespace IPC

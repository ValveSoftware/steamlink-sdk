// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_H_
#define IPC_IPC_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_endpoint.h"
#include "ipc/ipc_message.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#endif

namespace IPC {

class Listener;

//------------------------------------------------------------------------------
// See
// http://www.chromium.org/developers/design-documents/inter-process-communication
// for overview of IPC in Chromium.

// Channels are implemented using named pipes on Windows, and
// socket pairs (or in some special cases unix domain sockets) on POSIX.
// On Windows we access pipes in various processes by name.
// On POSIX we pass file descriptors to child processes and assign names to them
// in a lookup table.
// In general on POSIX we do not use unix domain sockets due to security
// concerns and the fact that they can leave garbage around the file system
// (MacOS does not support abstract named unix domain sockets).
// You can use unix domain sockets if you like on POSIX by constructing the
// the channel with the mode set to one of the NAMED modes. NAMED modes are
// currently used by automation and service processes.

class IPC_EXPORT Channel : public Endpoint {
  // Security tests need access to the pipe handle.
  friend class ChannelTest;

 public:
  // Flags to test modes
  enum ModeFlags {
    MODE_NO_FLAG = 0x0,
    MODE_SERVER_FLAG = 0x1,
    MODE_CLIENT_FLAG = 0x2,
    MODE_NAMED_FLAG = 0x4,
#if defined(OS_POSIX)
    MODE_OPEN_ACCESS_FLAG = 0x8, // Don't restrict access based on client UID.
#endif
  };

  // Some Standard Modes
  // TODO(morrita): These are under deprecation work. You should use Create*()
  // functions instead.
  enum Mode {
    MODE_NONE = MODE_NO_FLAG,
    MODE_SERVER = MODE_SERVER_FLAG,
    MODE_CLIENT = MODE_CLIENT_FLAG,
    MODE_NAMED_SERVER = MODE_SERVER_FLAG | MODE_NAMED_FLAG,
    MODE_NAMED_CLIENT = MODE_CLIENT_FLAG | MODE_NAMED_FLAG,
#if defined(OS_POSIX)
    MODE_OPEN_NAMED_SERVER = MODE_OPEN_ACCESS_FLAG | MODE_SERVER_FLAG |
                             MODE_NAMED_FLAG
#endif
  };

  // Messages internal to the IPC implementation are defined here.
  // Uses Maximum value of message type (uint16_t), to avoid conflicting
  // with normal message types, which are enumeration constants starting from 0.
  enum {
    // The Hello message is sent by the peer when the channel is connected.
    // The message contains just the process id (pid).
    // The message has a special routing_id (MSG_ROUTING_NONE)
    // and type (HELLO_MESSAGE_TYPE).
    HELLO_MESSAGE_TYPE = UINT16_MAX,
    // The CLOSE_FD_MESSAGE_TYPE is used in the IPC class to
    // work around a bug in sendmsg() on Mac. When an FD is sent
    // over the socket, a CLOSE_FD_MESSAGE is sent with hops = 2.
    // The client will return the message with hops = 1, *after* it
    // has received the message that contains the FD. When we
    // receive it again on the sender side, we close the FD.
    CLOSE_FD_MESSAGE_TYPE = HELLO_MESSAGE_TYPE - 1
  };

  // The maximum message size in bytes. Attempting to receive a message of this
  // size or bigger results in a channel error.
  static const size_t kMaximumMessageSize = 128 * 1024 * 1024;

  // Amount of data to read at once from the pipe.
  static const size_t kReadBufferSize = 4 * 1024;

  // Maximum persistent read buffer size. Read buffer can grow larger to
  // accommodate large messages, but it's recommended to shrink back to this
  // value because it fits 99.9% of all messages (see issue 529940 for data).
  static const size_t kMaximumReadBufferSize = 64 * 1024;

  // Initialize a Channel.
  //
  // |channel_handle| identifies the communication Channel. For POSIX, if
  // the file descriptor in the channel handle is != -1, the channel takes
  // ownership of the file descriptor and will close it appropriately, otherwise
  // it will create a new descriptor internally.
  // |listener| receives a callback on the current thread for each newly
  // received message.
  //
  // There are four type of modes how channels operate:
  //
  // - Server and named server: In these modes, the Channel is
  //   responsible for settingb up the IPC object
  // - An "open" named server: It accepts connections from ANY client.
  //   The caller must then implement their own access-control based on the
  //   client process' user Id.
  // - Client and named client: In these mode, the Channel merely
  //   connects to the already established IPC object.
  //
  // Each mode has its own Create*() API to create the Channel object.
  //
  // TODO(morrita): Replace CreateByModeForProxy() with one of above Create*().
  static std::unique_ptr<Channel> Create(
      const IPC::ChannelHandle& channel_handle,
      Mode mode,
      Listener* listener);

  static std::unique_ptr<Channel> CreateClient(
      const IPC::ChannelHandle& channel_handle,
      Listener* listener);

  // Channels on Windows are named by default and accessible from other
  // processes. On POSIX channels are anonymous by default and not accessible
  // from other processes. Named channels work via named unix domain sockets.
  // On Windows MODE_NAMED_SERVER is equivalent to MODE_SERVER and
  // MODE_NAMED_CLIENT is equivalent to MODE_CLIENT.
  static std::unique_ptr<Channel> CreateNamedServer(
      const IPC::ChannelHandle& channel_handle,
      Listener* listener);
  static std::unique_ptr<Channel> CreateNamedClient(
      const IPC::ChannelHandle& channel_handle,
      Listener* listener);
#if defined(OS_POSIX)
  // An "open" named server accepts connections from ANY client.
  // The caller must then implement their own access-control based on the
  // client process' user Id.
  static std::unique_ptr<Channel> CreateOpenNamedServer(
      const IPC::ChannelHandle& channel_handle,
      Listener* listener);
#endif
  static std::unique_ptr<Channel> CreateServer(
      const IPC::ChannelHandle& channel_handle,
      Listener* listener);

  ~Channel() override;

  // Connect the pipe.  On the server side, this will initiate
  // waiting for connections.  On the client, it attempts to
  // connect to a pre-existing pipe.  Note, calling Connect()
  // will not block the calling thread and may complete
  // asynchronously.
  //
  // The subclass implementation must call WillConnect() at the beginning of its
  // implementation.
  virtual bool Connect() WARN_UNUSED_RESULT = 0;

  // Close this Channel explicitly.  May be called multiple times.
  // On POSIX calling close on an IPC channel that listens for connections will
  // cause it to close any accepted connections, and it will stop listening for
  // new connections. If you just want to close the currently accepted
  // connection and listen for new ones, use ResetToAcceptingConnectionState.
  virtual void Close() = 0;

  // Get its own process id. This value is told to the peer.
  virtual base::ProcessId GetSelfPID() const = 0;

  // Overridden from ipc::Sender.
  // Send a message over the Channel to the listener on the other end.
  //
  // |message| must be allocated using operator new.  This object will be
  // deleted once the contents of the Message have been sent.
  bool Send(Message* message) override = 0;

  // IsSendThreadSafe returns true iff it's safe to call |Send| from non-IO
  // threads. This is constant for the lifetime of the |Channel|.
  virtual bool IsSendThreadSafe() const;

  // NaCl in Non-SFI mode runs on Linux directly, and the following functions
  // compiled on Linux are also needed. Please see also comments in
  // components/nacl_nonsfi.gyp for more details.
#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
  // On POSIX an IPC::Channel wraps a socketpair(), this method returns the
  // FD # for the client end of the socket.
  // This method may only be called on the server side of a channel.
  // This method can be called on any thread.
  virtual int GetClientFileDescriptor() const = 0;

  // Same as GetClientFileDescriptor, but transfers the ownership of the
  // file descriptor to the caller.
  // This method can be called on any thread.
  virtual base::ScopedFD TakeClientFileDescriptor() = 0;
#endif

  // Returns true if a named server channel is initialized on the given channel
  // ID. Even if true, the server may have already accepted a connection.
  static bool IsNamedServerInitialized(const std::string& channel_id);

#if !defined(OS_NACL_SFI)
  // Generates a channel ID that's non-predictable and unique.
  static std::string GenerateUniqueRandomChannelID();

  // Generates a channel ID that, if passed to the client as a shared secret,
  // will validate that the client's authenticity. On platforms that do not
  // require additional this is simply calls GenerateUniqueRandomChannelID().
  // For portability the prefix should not include the \ character.
  static std::string GenerateVerifiedChannelID(const std::string& prefix);
#endif

  // Generates a pair of channel handles that can be used as the client and
  // server ends of a ChannelMojo. |name_postfix| is appended to the end of the
  // handle name to help identify the handles.
  //
  // Note, when using ChannelMojo, |ChannelHandle::name| serves no functional
  // purpose other than to identify the channel in logging.
  static void GenerateMojoChannelHandlePair(
      const std::string& name_postfix,
      IPC::ChannelHandle* handle0,
      IPC::ChannelHandle* handle1);

#if defined(OS_LINUX)
  // Sandboxed processes live in a PID namespace, so when sending the IPC hello
  // message from client to server we need to send the PID from the global
  // PID namespace.
  static void SetGlobalPid(int pid);
  static int GetGlobalPid();
#endif

#if defined(OS_ANDROID)
  // Most tests are single process and work the same on all platforms. However
  // in some cases we want to test multi-process, and Android differs in that it
  // can't 'exec' after forking. This callback resets any data in the forked
  // process such that it acts similar to if it was exec'd, for tests.
  static void NotifyProcessForkedForTesting();
#endif

 protected:
  // An OutputElement is a wrapper around a Message or raw buffer while it is
  // waiting to be passed to the system's underlying IPC mechanism.
  class OutputElement {
   public:
    // Takes ownership of message.
    OutputElement(Message* message);
    // Takes ownership of the buffer. |buffer| is freed via free(), so it
    // must be malloced.
    OutputElement(void* buffer, size_t length);
    ~OutputElement();
    size_t size() const { return message_ ? message_->size() : length_; }
    const void* data() const { return message_ ? message_->data() : buffer_; }
    Message* get_message() const { return message_.get(); }

   private:
    std::unique_ptr<Message> message_;
    void* buffer_;
    size_t length_;
  };

  // Endpoint overrides.
  void OnSetAttachmentBrokerEndpoint() override;

  // Subclasses must call this method at the beginning of their implementation
  // of Connect().
  void WillConnect();

 private:
  bool did_start_connect_ = false;
};

#if defined(OS_POSIX)
// SocketPair() creates a pair of socket FDs suitable for using with
// IPC::Channel.
IPC_EXPORT bool SocketPair(int* fd1, int* fd2);
#endif

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_H_

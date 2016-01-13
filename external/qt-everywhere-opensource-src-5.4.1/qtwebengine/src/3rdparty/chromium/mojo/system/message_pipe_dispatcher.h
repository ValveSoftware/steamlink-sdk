// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_
#define MOJO_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_

#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class MessagePipe;
class MessagePipeDispatcherTransport;

// This is the |Dispatcher| implementation for message pipes (created by the
// Mojo primitive |MojoCreateMessagePipe()|). This class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT MessagePipeDispatcher : public Dispatcher {
 public:
  // The default options to use for |MojoCreateMessagePipe()|. (Real uses
  // should obtain this via |ValidateCreateOptions()| with a null |in_options|;
  // this is exposed directly for testing convenience.)
  static const MojoCreateMessagePipeOptions kDefaultCreateOptions;

  MessagePipeDispatcher(
      const MojoCreateMessagePipeOptions& /*validated_options*/);

  // Validates and/or sets default options for |MojoCreateMessagePipeOptions|.
  // If non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateMessagePipeOptions| and will be entirely overwritten on success
  // (it may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      const MojoCreateMessagePipeOptions* in_options,
      MojoCreateMessagePipeOptions* out_options);

  // Must be called before any other methods. (This method is not thread-safe.)
  void Init(scoped_refptr<MessagePipe> message_pipe, unsigned port);

  // |Dispatcher| public methods:
  virtual Type GetType() const OVERRIDE;

  // Creates a |MessagePipe| with a local endpoint (at port 0) and a proxy
  // endpoint, and creates/initializes a |MessagePipeDispatcher| (attached to
  // the message pipe, port 0).
  // TODO(vtl): This currently uses |kDefaultCreateOptions|, which is okay since
  // there aren't any options, but eventually options should be plumbed through.
  static std::pair<scoped_refptr<MessagePipeDispatcher>,
                   scoped_refptr<MessagePipe> > CreateRemoteMessagePipe();

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<MessagePipeDispatcher> Deserialize(Channel* channel,
                                                          const void* source,
                                                          size_t size);

 private:
  friend class MessagePipeDispatcherTransport;

  virtual ~MessagePipeDispatcher();

  // Gets a dumb pointer to |message_pipe_|. This must be called under the
  // |Dispatcher| lock (that it's a dumb pointer is okay since it's under lock).
  // This is needed when sending handles across processes, where nontrivial,
  // invasive work needs to be done.
  MessagePipe* GetMessagePipeNoLock() const;
  // Similarly for the port.
  unsigned GetPortNoLock() const;

  // |Dispatcher| protected methods:
  virtual void CancelAllWaitersNoLock() OVERRIDE;
  virtual void CloseImplNoLock() OVERRIDE;
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE;
  virtual MojoResult WriteMessageImplNoLock(
      const void* bytes,
      uint32_t num_bytes,
      std::vector<DispatcherTransport>* transports,
      MojoWriteMessageFlags flags) OVERRIDE;
  virtual MojoResult ReadMessageImplNoLock(void* bytes,
                                           uint32_t* num_bytes,
                                           DispatcherVector* dispatchers,
                                           uint32_t* num_dispatchers,
                                           MojoReadMessageFlags flags) OVERRIDE;
  virtual MojoResult AddWaiterImplNoLock(Waiter* waiter,
                                         MojoHandleSignals signals,
                                         uint32_t context) OVERRIDE;
  virtual void RemoveWaiterImplNoLock(Waiter* waiter) OVERRIDE;
  virtual void StartSerializeImplNoLock(Channel* channel,
                                        size_t* max_size,
                                        size_t* max_platform_handles) OVERRIDE;
  virtual bool EndSerializeAndCloseImplNoLock(
      Channel* channel,
      void* destination,
      size_t* actual_size,
      embedder::PlatformHandleVector* platform_handles) OVERRIDE;

  // Protected by |lock()|:
  scoped_refptr<MessagePipe> message_pipe_;  // This will be null if closed.
  unsigned port_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeDispatcher);
};

class MessagePipeDispatcherTransport : public DispatcherTransport {
 public:
  explicit MessagePipeDispatcherTransport(DispatcherTransport transport);

  MessagePipe* GetMessagePipe() {
    return message_pipe_dispatcher()->GetMessagePipeNoLock();
  }
  unsigned GetPort() { return message_pipe_dispatcher()->GetPortNoLock(); }

 private:
  MessagePipeDispatcher* message_pipe_dispatcher() {
    return static_cast<MessagePipeDispatcher*>(dispatcher());
  }

  // Copy and assign allowed.
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_

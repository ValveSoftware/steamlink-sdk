// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_WIN_H_
#define IPC_IPC_CHANNEL_WIN_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_reader.h"

namespace base {
class ThreadChecker;
}

namespace IPC {

class ChannelWin : public Channel,
                   public internal::ChannelReader,
                   public base::MessageLoopForIO::IOHandler {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  // |broker| must outlive the newly created object.
  ChannelWin(const IPC::ChannelHandle& channel_handle,
             Mode mode,
             Listener* listener);
  ~ChannelWin() override;

  // Channel implementation
  bool Connect() override;
  void Close() override;
  bool Send(Message* message) override;
  AttachmentBroker* GetAttachmentBroker() override;
  base::ProcessId GetPeerPID() const override;
  base::ProcessId GetSelfPID() const override;

  static bool IsNamedServerInitialized(const std::string& channel_id);


 private:
  // ChannelReader implementation.
  ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) override;
  bool ShouldDispatchInputMessage(Message* msg) override;
  bool GetNonBrokeredAttachments(Message* msg) override;
  bool DidEmptyInputBuffers() override;
  void HandleInternalMessage(const Message& msg) override;
  base::ProcessId GetSenderPID() override;
  bool IsAttachmentBrokerEndpoint() override;

  static const base::string16 PipeName(const std::string& channel_id,
                                       int32_t* secret);
  bool CreatePipe(const IPC::ChannelHandle &channel_handle, Mode mode);

  bool ProcessConnection();
  bool ProcessOutgoingMessages(base::MessageLoopForIO::IOContext* context,
                               DWORD bytes_written);

  // Returns |false| on channel error.
  // If |message| has brokerable attachments, those attachments are passed to
  // the AttachmentBroker (which in turn invokes Send()), so this method must
  // be re-entrant.
  // Adds |message| to |output_queue_| and calls ProcessOutgoingMessages().
  bool ProcessMessageForDelivery(Message* message);

  // Moves all messages from |prelim_queue_| to |output_queue_| by calling
  // ProcessMessageForDelivery().
  void FlushPrelimQueue();

  // MessageLoop::IOHandler implementation.
  void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override;

 private:
  struct State {
    State();
    ~State();
    base::MessageLoopForIO::IOContext context;
    bool is_pending = false;
  };

  State input_state_;
  State output_state_;

  base::win::ScopedHandle pipe_;

  base::ProcessId peer_pid_;

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

  // In server-mode, we have to wait for the client to connect before we
  // can begin reading.  We make use of the input_state_ when performing
  // the connect operation in overlapped mode.
  bool waiting_connect_;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_;

  // Determines if we should validate a client's secret on connection.
  bool validate_client_;

  // This is a unique per-channel value used to authenticate the client end of
  // a connection. If the value is non-zero, the client passes it in the hello
  // and the host validates. (We don't send the zero value fto preserve IPC
  // compatability with existing clients that don't validate the channel.)
  int32_t client_secret_;

  std::unique_ptr<base::ThreadChecker> thread_check_;

  base::WeakPtrFactory<ChannelWin> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChannelWin);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_WIN_H_

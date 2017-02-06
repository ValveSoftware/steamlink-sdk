// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_win.h"

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/pickle.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/win/scoped_handle.h"
#include "ipc/attachment_broker.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

ChannelWin::State::State() = default;

ChannelWin::State::~State() {
  static_assert(offsetof(ChannelWin::State, context) == 0,
                "ChannelWin::State should have context as its first data"
                "member.");
}

ChannelWin::ChannelWin(const IPC::ChannelHandle& channel_handle,
                       Mode mode,
                       Listener* listener)
    : ChannelReader(listener),
      peer_pid_(base::kNullProcessId),
      waiting_connect_(mode & MODE_SERVER_FLAG),
      processing_incoming_(false),
      validate_client_(false),
      client_secret_(0),
      weak_factory_(this) {
  CreatePipe(channel_handle, mode);
}

ChannelWin::~ChannelWin() {
  CleanUp();
  Close();
}

void ChannelWin::Close() {
  if (thread_check_.get())
    DCHECK(thread_check_->CalledOnValidThread());

  if (input_state_.is_pending || output_state_.is_pending)
    CancelIo(pipe_.Get());

  // Closing the handle at this point prevents us from issuing more requests
  // form OnIOCompleted().
  if (pipe_.IsValid())
    pipe_.Close();

  // Make sure all IO has completed.
  while (input_state_.is_pending || output_state_.is_pending) {
    base::MessageLoopForIO::current()->WaitForIOCompletion(INFINITE, this);
  }

  while (!output_queue_.empty()) {
    OutputElement* element = output_queue_.front();
    output_queue_.pop();
    delete element;
  }
}

bool ChannelWin::Send(Message* message) {
  DCHECK(thread_check_->CalledOnValidThread());
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

bool ChannelWin::ProcessMessageForDelivery(Message* message) {
  // Sending a brokerable attachment requires a call to Channel::Send(), so
  // both Send() and ProcessMessageForDelivery() may be re-entrant.
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
#endif

  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "ChannelWin::ProcessMessageForDelivery",
                         message->flags(),
                         TRACE_EVENT_FLAG_FLOW_OUT);

  // |output_queue_| takes ownership of |message|.
  OutputElement* element = new OutputElement(message);
  output_queue_.push(element);

#if USE_ATTACHMENT_BROKER
  if (message->HasBrokerableAttachments()) {
    // |output_queue_| takes ownership of |ids.buffer|.
    Message::SerializedAttachmentIds ids =
        message->SerializedIdsOfBrokerableAttachments();
    output_queue_.push(new OutputElement(ids.buffer, ids.size));
  }
#endif

  // ensure waiting to write
  if (!waiting_connect_) {
    if (!output_state_.is_pending) {
      if (!ProcessOutgoingMessages(NULL, 0))
        return false;
    }
  }

  return true;
}

void ChannelWin::FlushPrelimQueue() {
  DCHECK_NE(peer_pid_, base::kNullProcessId);

  // Due to the possibly re-entrant nature of ProcessMessageForDelivery(), it
  // is critical that |prelim_queue_| appears empty.
  std::queue<Message*> prelim_queue;
  prelim_queue_.swap(prelim_queue);

  while (!prelim_queue.empty()) {
    Message* m = prelim_queue.front();
    bool success = ProcessMessageForDelivery(m);
    prelim_queue.pop();

    if (!success)
      break;
  }

  // Delete any unprocessed messages.
  while (!prelim_queue.empty()) {
    Message* m = prelim_queue.front();
    delete m;
    prelim_queue.pop();
  }
}

AttachmentBroker* ChannelWin::GetAttachmentBroker() {
  return AttachmentBroker::GetGlobal();
}

base::ProcessId ChannelWin::GetPeerPID() const {
  return peer_pid_;
}

base::ProcessId ChannelWin::GetSelfPID() const {
  return GetCurrentProcessId();
}

// static
bool ChannelWin::IsNamedServerInitialized(
    const std::string& channel_id) {
  if (WaitNamedPipe(PipeName(channel_id, NULL).c_str(), 1))
    return true;
  // If ERROR_SEM_TIMEOUT occurred, the pipe exists but is handling another
  // connection.
  return GetLastError() == ERROR_SEM_TIMEOUT;
}

ChannelWin::ReadState ChannelWin::ReadData(
    char* buffer,
    int buffer_len,
    int* /* bytes_read */) {
  if (!pipe_.IsValid())
    return READ_FAILED;

  DWORD bytes_read = 0;
  BOOL ok = ReadFile(pipe_.Get(), buffer, buffer_len,
                     &bytes_read, &input_state_.context.overlapped);
  if (!ok) {
    DWORD err = GetLastError();
    if (err == ERROR_IO_PENDING) {
      input_state_.is_pending = true;
      return READ_PENDING;
    }
    LOG(ERROR) << "pipe error: " << err;
    return READ_FAILED;
  }

  // We could return READ_SUCCEEDED here. But the way that this code is
  // structured we instead go back to the message loop. Our completion port
  // will be signalled even in the "synchronously completed" state.
  //
  // This allows us to potentially process some outgoing messages and
  // interleave other work on this thread when we're getting hammered with
  // input messages. Potentially, this could be tuned to be more efficient
  // with some testing.
  input_state_.is_pending = true;
  return READ_PENDING;
}

bool ChannelWin::ShouldDispatchInputMessage(Message* msg) {
  // Make sure we get a hello when client validation is required.
  if (validate_client_)
    return IsHelloMessage(*msg);
  return true;
}

bool ChannelWin::GetNonBrokeredAttachments(Message* msg) {
  return true;
}

void ChannelWin::HandleInternalMessage(const Message& msg) {
  DCHECK_EQ(msg.type(), static_cast<unsigned>(Channel::HELLO_MESSAGE_TYPE));
  // The hello message contains one parameter containing the PID.
  base::PickleIterator it(msg);
  int32_t claimed_pid;
  bool failed = !it.ReadInt(&claimed_pid);

  if (!failed && validate_client_) {
    int32_t secret;
    failed = it.ReadInt(&secret) ? (secret != client_secret_) : true;
  }

  if (failed) {
    NOTREACHED();
    Close();
    listener()->OnChannelError();
    return;
  }

  peer_pid_ = claimed_pid;
  // Validation completed.
  validate_client_ = false;

  listener()->OnChannelConnected(claimed_pid);

  FlushPrelimQueue();

  if (IsAttachmentBrokerEndpoint() &&
      AttachmentBroker::GetGlobal()->IsPrivilegedBroker()) {
    AttachmentBroker::GetGlobal()->ReceivedPeerPid(claimed_pid);
  }
}

base::ProcessId ChannelWin::GetSenderPID() {
  return GetPeerPID();
}

bool ChannelWin::IsAttachmentBrokerEndpoint() {
  return is_attachment_broker_endpoint();
}

bool ChannelWin::DidEmptyInputBuffers() {
  // We don't need to do anything here.
  return true;
}

// static
const base::string16 ChannelWin::PipeName(const std::string& channel_id,
                                          int32_t* secret) {
  std::string name("\\\\.\\pipe\\chrome.");

  // Prevent the shared secret from ending up in the pipe name.
  size_t index = channel_id.find_first_of('\\');
  if (index != std::string::npos) {
    if (secret)  // Retrieve the secret if asked for.
      base::StringToInt(channel_id.substr(index + 1), secret);
    return base::ASCIIToUTF16(name.append(channel_id.substr(0, index - 1)));
  }

  // This case is here to support predictable named pipes in tests.
  if (secret)
    *secret = 0;
  return base::ASCIIToUTF16(name.append(channel_id));
}

bool ChannelWin::CreatePipe(const IPC::ChannelHandle &channel_handle,
                            Mode mode) {
  DCHECK(!pipe_.IsValid());
  base::string16 pipe_name;
  // If we already have a valid pipe for channel just copy it.
  if (channel_handle.pipe.handle) {
    // TODO(rvargas) crbug.com/415294: ChannelHandle should either go away in
    // favor of two independent entities (name/file), or it should be a move-
    // only type with a base::File member. In any case, this code should not
    // call DuplicateHandle.
    DCHECK(channel_handle.name.empty());
    pipe_name = L"Not Available";  // Just used for LOG
    // Check that the given pipe confirms to the specified mode.  We can
    // only check for PIPE_TYPE_MESSAGE & PIPE_SERVER_END flags since the
    // other flags (PIPE_TYPE_BYTE, and PIPE_CLIENT_END) are defined as 0.
    DWORD flags = 0;
    GetNamedPipeInfo(channel_handle.pipe.handle, &flags, NULL, NULL, NULL);
    DCHECK(!(flags & PIPE_TYPE_MESSAGE));
    if (((mode & MODE_SERVER_FLAG) && !(flags & PIPE_SERVER_END)) ||
        ((mode & MODE_CLIENT_FLAG) && (flags & PIPE_SERVER_END))) {
      LOG(WARNING) << "Inconsistent open mode. Mode :" << mode;
      return false;
    }
    HANDLE local_handle;
    if (!DuplicateHandle(GetCurrentProcess(),
                         channel_handle.pipe.handle,
                         GetCurrentProcess(),
                         &local_handle,
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      LOG(WARNING) << "DuplicateHandle failed. Error :" << GetLastError();
      return false;
    }
    pipe_.Set(local_handle);
  } else if (mode & MODE_SERVER_FLAG) {
    DCHECK(!channel_handle.pipe.handle);
    const DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                            FILE_FLAG_FIRST_PIPE_INSTANCE;
    pipe_name = PipeName(channel_handle.name, &client_secret_);
    validate_client_ = !!client_secret_;
    pipe_.Set(CreateNamedPipeW(pipe_name.c_str(),
                               open_mode,
                               PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                               1,
                               Channel::kReadBufferSize,
                               Channel::kReadBufferSize,
                               5000,
                               NULL));
  } else if (mode & MODE_CLIENT_FLAG) {
    DCHECK(!channel_handle.pipe.handle);
    pipe_name = PipeName(channel_handle.name, &client_secret_);
    pipe_.Set(CreateFileW(pipe_name.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          NULL,
                          OPEN_EXISTING,
                          SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS |
                              FILE_FLAG_OVERLAPPED,
                          NULL));
  } else {
    NOTREACHED();
  }

  if (!pipe_.IsValid()) {
    // If this process is being closed, the pipe may be gone already.
    PLOG(WARNING) << "Unable to create pipe \"" << pipe_name << "\" in "
                  << (mode & MODE_SERVER_FLAG ? "server" : "client") << " mode";
    return false;
  }

  // Create the Hello message to be sent when Connect is called
  std::unique_ptr<Message> m(new Message(MSG_ROUTING_NONE, HELLO_MESSAGE_TYPE,
                                         IPC::Message::PRIORITY_NORMAL));

  // Don't send the secret to the untrusted process, and don't send a secret
  // if the value is zero (for IPC backwards compatability).
  int32_t secret = validate_client_ ? 0 : client_secret_;
  if (!m->WriteInt(GetCurrentProcessId()) ||
      (secret && !m->WriteUInt32(secret))) {
    pipe_.Close();
    return false;
  }

  OutputElement* element = new OutputElement(m.release());
  output_queue_.push(element);
  return true;
}

bool ChannelWin::Connect() {
  WillConnect();

  DLOG_IF(WARNING, thread_check_.get()) << "Connect called more than once";

  if (!thread_check_.get())
    thread_check_.reset(new base::ThreadChecker());

  if (!pipe_.IsValid())
    return false;

  base::MessageLoopForIO::current()->RegisterIOHandler(pipe_.Get(), this);

  // Check to see if there is a client connected to our pipe...
  if (waiting_connect_)
    ProcessConnection();

  if (!input_state_.is_pending) {
    // Complete setup asynchronously. By not setting input_state_.is_pending
    // to true, we indicate to OnIOCompleted that this is the special
    // initialization signal.
    base::MessageLoopForIO::current()->PostTask(
        FROM_HERE,
        base::Bind(&ChannelWin::OnIOCompleted,
                   weak_factory_.GetWeakPtr(),
                   &input_state_.context,
                   0,
                   0));
  }

  if (!waiting_connect_)
    ProcessOutgoingMessages(NULL, 0);
  return true;
}

bool ChannelWin::ProcessConnection() {
  DCHECK(thread_check_->CalledOnValidThread());
  if (input_state_.is_pending)
    input_state_.is_pending = false;

  // Do we have a client connected to our pipe?
  if (!pipe_.IsValid())
    return false;

  BOOL ok = ConnectNamedPipe(pipe_.Get(), &input_state_.context.overlapped);
  DWORD err = GetLastError();
  if (ok) {
    // Uhm, the API documentation says that this function should never
    // return success when used in overlapped mode.
    NOTREACHED();
    return false;
  }

  switch (err) {
  case ERROR_IO_PENDING:
    input_state_.is_pending = true;
    break;
  case ERROR_PIPE_CONNECTED:
    waiting_connect_ = false;
    break;
  case ERROR_NO_DATA:
    // The pipe is being closed.
    return false;
  default:
    NOTREACHED();
    return false;
  }

  return true;
}

bool ChannelWin::ProcessOutgoingMessages(
    base::MessageLoopForIO::IOContext* context,
    DWORD bytes_written) {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  DCHECK(thread_check_->CalledOnValidThread());

  if (output_state_.is_pending) {
    DCHECK(context);
    output_state_.is_pending = false;
    if (!context || bytes_written == 0) {
      DWORD err = GetLastError();
      LOG(ERROR) << "pipe error: " << err;
      return false;
    }
    // Message was sent.
    CHECK(!output_queue_.empty());
    OutputElement* element = output_queue_.front();
    output_queue_.pop();
    delete element;
  }

  if (output_queue_.empty())
    return true;

  if (!pipe_.IsValid())
    return false;

  // Write to pipe...
  OutputElement* element = output_queue_.front();
  DCHECK(element->size() <= INT_MAX);
  BOOL ok = WriteFile(pipe_.Get(),
                      element->data(),
                      static_cast<uint32_t>(element->size()),
                      NULL,
                      &output_state_.context.overlapped);
  if (!ok) {
    DWORD write_error = GetLastError();
    if (write_error == ERROR_IO_PENDING) {
      output_state_.is_pending = true;

      const Message* m = element->get_message();
      if (m) {
        DVLOG(2) << "sent pending message @" << m << " on channel @" << this
                 << " with type " << m->type();
      }

      return true;
    }
    LOG(ERROR) << "pipe error: " << write_error;
    return false;
  }

  const Message* m = element->get_message();
  if (m) {
    DVLOG(2) << "sent message @" << m << " on channel @" << this
             << " with type " << m->type();
  }

  output_state_.is_pending = true;
  return true;
}

void ChannelWin::OnIOCompleted(
    base::MessageLoopForIO::IOContext* context,
    DWORD bytes_transfered,
    DWORD error) {
  bool ok = true;
  DCHECK(thread_check_->CalledOnValidThread());
  if (context == &input_state_.context) {
    if (waiting_connect_) {
      if (!ProcessConnection())
        return;
      // We may have some messages queued up to send...
      if (!output_queue_.empty() && !output_state_.is_pending)
        ProcessOutgoingMessages(NULL, 0);
      if (input_state_.is_pending)
        return;
      // else, fall-through and look for incoming messages...
    }

    // We don't support recursion through OnMessageReceived yet!
    DCHECK(!processing_incoming_);
    base::AutoReset<bool> auto_reset_processing_incoming(
        &processing_incoming_, true);

    // Process the new data.
    if (input_state_.is_pending) {
      // This is the normal case for everything except the initialization step.
      input_state_.is_pending = false;
      if (!bytes_transfered) {
        ok = false;
      } else if (pipe_.IsValid()) {
        ok = (AsyncReadComplete(bytes_transfered) != DISPATCH_ERROR);
      }
    } else {
      DCHECK(!bytes_transfered);
    }

    // Request more data.
    if (ok)
      ok = (ProcessIncomingMessages() != DISPATCH_ERROR);
  } else {
    DCHECK(context == &output_state_.context);
    CHECK(output_state_.is_pending);
    ok = ProcessOutgoingMessages(context, bytes_transfered);
  }
  if (!ok && pipe_.IsValid()) {
    // We don't want to re-enter Close().
    Close();
    listener()->OnChannelError();
  }
}

//------------------------------------------------------------------------------
// Channel's methods

// static
std::unique_ptr<Channel> Channel::Create(
    const IPC::ChannelHandle& channel_handle,
    Mode mode,
    Listener* listener) {
  return base::WrapUnique(new ChannelWin(channel_handle, mode, listener));
}

// static
bool Channel::IsNamedServerInitialized(const std::string& channel_id) {
  return ChannelWin::IsNamedServerInitialized(channel_id);
}

// static
std::string Channel::GenerateVerifiedChannelID(const std::string& prefix) {
  // Windows pipes can be enumerated by low-privileged processes. So, we
  // append a strong random value after the \ character. This value is not
  // included in the pipe name, but sent as part of the client hello, to
  // hijacking the pipe name to spoof the client.

  std::string id = prefix;
  if (!id.empty())
    id.append(".");

  int secret;
  do {  // Guarantee we get a non-zero value.
    secret = base::RandInt(0, std::numeric_limits<int>::max());
  } while (secret == 0);

  id.append(GenerateUniqueRandomChannelID());
  return id.append(base::StringPrintf("\\%d", secret));
}

}  // namespace IPC

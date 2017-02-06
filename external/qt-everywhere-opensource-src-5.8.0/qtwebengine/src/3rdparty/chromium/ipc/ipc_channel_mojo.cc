// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_mojo.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if defined(OS_POSIX)
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

#if defined(OS_MACOSX)
#include "ipc/mach_port_attachment_mac.h"
#endif

#if defined(OS_WIN)
#include "ipc/handle_attachment_win.h"
#endif

namespace IPC {

namespace {

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(mojo::ScopedMessagePipeHandle handle, Channel::Mode mode)
      : handle_(std::move(handle)), mode_(mode) {}

  std::string GetName() const override { return ""; }

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
    return ChannelMojo::Create(std::move(handle_), mode_, listener);
  }

 private:
  mojo::ScopedMessagePipeHandle handle_;
  const Channel::Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(MojoChannelFactory);
};

mojom::SerializedHandlePtr CreateSerializedHandle(
    mojo::ScopedHandle handle,
    mojom::SerializedHandle::Type type) {
  mojom::SerializedHandlePtr serialized_handle = mojom::SerializedHandle::New();
  serialized_handle->the_handle = std::move(handle);
  serialized_handle->type = type;
  return serialized_handle;
}

MojoResult WrapPlatformHandle(base::PlatformFile handle,
                              mojom::SerializedHandle::Type type,
                              mojom::SerializedHandlePtr* serialized) {
  mojo::ScopedHandle wrapped_handle = mojo::WrapPlatformFile(handle);
  if (!wrapped_handle.is_valid())
    return MOJO_RESULT_UNKNOWN;

  *serialized = CreateSerializedHandle(std::move(wrapped_handle), type);
  return MOJO_RESULT_OK;
}

#if defined(OS_MACOSX)

MojoResult WrapMachPort(mach_port_t mach_port,
                        mojom::SerializedHandlePtr* serialized) {
  MojoPlatformHandle platform_handle = {
    sizeof(MojoPlatformHandle), MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT,
    static_cast<uint64_t>(mach_port)
  };

  MojoHandle wrapped_handle;
  MojoResult result = MojoWrapPlatformHandle(&platform_handle, &wrapped_handle);
  if (result != MOJO_RESULT_OK)
    return result;

  *serialized = CreateSerializedHandle(
      mojo::MakeScopedHandle(mojo::Handle(wrapped_handle)),
      mojom::SerializedHandle::Type::MACH_PORT);
  return MOJO_RESULT_OK;
}

#endif

#if defined(OS_POSIX)

base::ScopedFD TakeOrDupFile(internal::PlatformFileAttachment* attachment) {
  return attachment->Owns() ? base::ScopedFD(attachment->TakePlatformFile())
                            : base::ScopedFD(dup(attachment->file()));
}

#endif

MojoResult WrapAttachmentImpl(MessageAttachment* attachment,
                              mojom::SerializedHandlePtr* serialized) {
  if (attachment->GetType() == MessageAttachment::TYPE_MOJO_HANDLE) {
    *serialized = CreateSerializedHandle(
        static_cast<internal::MojoHandleAttachment&>(*attachment).TakeHandle(),
        mojom::SerializedHandle::Type::MOJO_HANDLE);
    return MOJO_RESULT_OK;
  }
#if defined(OS_POSIX)
  if (attachment->GetType() == MessageAttachment::TYPE_PLATFORM_FILE) {
    // We dup() the handles in IPC::Message to transmit.
    // IPC::MessageAttachmentSet has intricate lifecycle semantics
    // of FDs, so just to dup()-and-own them is the safest option.
    base::ScopedFD file = TakeOrDupFile(
        static_cast<IPC::internal::PlatformFileAttachment*>(attachment));
    if (!file.is_valid()) {
      DPLOG(WARNING) << "Failed to dup FD to transmit.";
      return MOJO_RESULT_UNKNOWN;
    }

    return WrapPlatformHandle(file.release(),
                              mojom::SerializedHandle::Type::PLATFORM_FILE,
                              serialized);
  }
#endif
#if defined(OS_MACOSX)
  DCHECK_EQ(attachment->GetType(),
            MessageAttachment::TYPE_BROKERABLE_ATTACHMENT);
  DCHECK_EQ(static_cast<BrokerableAttachment&>(*attachment).GetBrokerableType(),
            BrokerableAttachment::MACH_PORT);
  internal::MachPortAttachmentMac& mach_port_attachment =
      static_cast<internal::MachPortAttachmentMac&>(*attachment);
  MojoResult result = WrapMachPort(mach_port_attachment.get_mach_port(),
                                   serialized);
  mach_port_attachment.reset_mach_port_ownership();
  return result;
#elif defined(OS_WIN)
  DCHECK_EQ(attachment->GetType(),
            MessageAttachment::TYPE_BROKERABLE_ATTACHMENT);
  DCHECK_EQ(static_cast<BrokerableAttachment&>(*attachment).GetBrokerableType(),
            BrokerableAttachment::WIN_HANDLE);
  internal::HandleAttachmentWin& handle_attachment =
      static_cast<internal::HandleAttachmentWin&>(*attachment);
  MojoResult result = WrapPlatformHandle(
      handle_attachment.get_handle(),
      mojom::SerializedHandle::Type::WIN_HANDLE, serialized);
  handle_attachment.reset_handle_ownership();
  return result;
#else
  NOTREACHED();
  return MOJO_RESULT_UNKNOWN;
#endif  // defined(OS_MACOSX)
}

MojoResult WrapAttachment(MessageAttachment* attachment,
                          mojo::Array<mojom::SerializedHandlePtr>* handles) {
  mojom::SerializedHandlePtr serialized_handle;
  MojoResult wrap_result = WrapAttachmentImpl(attachment, &serialized_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    return wrap_result;
  }
  handles->push_back(std::move(serialized_handle));
  return MOJO_RESULT_OK;
}

MojoResult UnwrapAttachment(mojom::SerializedHandlePtr handle,
                            scoped_refptr<MessageAttachment>* attachment) {
  if (handle->type == mojom::SerializedHandle::Type::MOJO_HANDLE) {
    *attachment =
        new IPC::internal::MojoHandleAttachment(std::move(handle->the_handle));
    return MOJO_RESULT_OK;
  }
  MojoPlatformHandle platform_handle = { sizeof(MojoPlatformHandle), 0, 0 };
  MojoResult unwrap_result = MojoUnwrapPlatformHandle(
          handle->the_handle.release().value(), &platform_handle);
  if (unwrap_result != MOJO_RESULT_OK)
    return unwrap_result;
#if defined(OS_POSIX)
  if (handle->type == mojom::SerializedHandle::Type::PLATFORM_FILE) {
    base::PlatformFile file = base::kInvalidPlatformFile;
    if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
      file = static_cast<base::PlatformFile>(platform_handle.value);
    *attachment = new internal::PlatformFileAttachment(file);
    return MOJO_RESULT_OK;
  }
#endif  // defined(OS_POSIX)
#if defined(OS_MACOSX)
  if (handle->type == mojom::SerializedHandle::Type::MACH_PORT) {
    mach_port_t mach_port = MACH_PORT_NULL;
    if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT)
      mach_port = static_cast<mach_port_t>(platform_handle.value);
    *attachment = new internal::MachPortAttachmentMac(
        mach_port, internal::MachPortAttachmentMac::FROM_WIRE);
    return MOJO_RESULT_OK;
  }
#endif  // defined(OS_MACOSX)
#if defined(OS_WIN)
  if (handle->type == mojom::SerializedHandle::Type::WIN_HANDLE) {
    base::PlatformFile handle = base::kInvalidPlatformFile;
    if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE)
      handle = reinterpret_cast<base::PlatformFile>(platform_handle.value);
    *attachment = new internal::HandleAttachmentWin(
        handle, internal::HandleAttachmentWin::FROM_WIRE);
    return MOJO_RESULT_OK;
  }
#endif  // defined(OS_WIN)
  NOTREACHED();
  return MOJO_RESULT_UNKNOWN;
}

}  // namespace

//------------------------------------------------------------------------------

// static
std::unique_ptr<ChannelMojo> ChannelMojo::Create(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener) {
  return base::WrapUnique(new ChannelMojo(std::move(handle), mode, listener));
}

// static
std::unique_ptr<ChannelFactory> ChannelMojo::CreateServerFactory(
    mojo::ScopedMessagePipeHandle handle) {
  return base::WrapUnique(
      new MojoChannelFactory(std::move(handle), Channel::MODE_SERVER));
}

// static
std::unique_ptr<ChannelFactory> ChannelMojo::CreateClientFactory(
    mojo::ScopedMessagePipeHandle handle) {
  return base::WrapUnique(
      new MojoChannelFactory(std::move(handle), Channel::MODE_CLIENT));
}

ChannelMojo::ChannelMojo(mojo::ScopedMessagePipeHandle handle,
                         Mode mode,
                         Listener* listener)
    : pipe_(handle.get()),
      listener_(listener),
      waiting_connect_(true),
      weak_factory_(this) {
  // Create MojoBootstrap after all members are set as it touches
  // ChannelMojo from a different thread.
  bootstrap_ = MojoBootstrap::Create(std::move(handle), mode, this);
}

ChannelMojo::~ChannelMojo() {
  Close();
}

bool ChannelMojo::Connect() {
  WillConnect();
  {
    base::AutoLock lock(lock_);
    DCHECK(!task_runner_);
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    DCHECK(!message_reader_);
  }
  bootstrap_->Connect();
  return true;
}

void ChannelMojo::Close() {
  std::unique_ptr<internal::MessagePipeReader, ReaderDeleter> reader;
  {
    base::AutoLock lock(lock_);
    if (!message_reader_)
      return;
    // The reader's destructor may re-enter Close, so we swap it out first to
    // avoid deadlock when freeing it below.
    std::swap(message_reader_, reader);

    // We might Close() before we Connect().
    waiting_connect_ = false;
  }

  reader.reset();
}

// MojoBootstrap::Delegate implementation
void ChannelMojo::OnPipesAvailable(
    mojom::ChannelAssociatedPtrInfo send_channel,
    mojom::ChannelAssociatedRequest receive_channel,
    int32_t peer_pid) {
  InitMessageReader(std::move(send_channel), std::move(receive_channel),
                    peer_pid);
}

void ChannelMojo::OnBootstrapError() {
  listener_->OnChannelError();
}

void ChannelMojo::InitMessageReader(mojom::ChannelAssociatedPtrInfo sender,
                                    mojom::ChannelAssociatedRequest receiver,
                                    base::ProcessId peer_pid) {
  mojom::ChannelAssociatedPtr sender_ptr;
  sender_ptr.Bind(std::move(sender));
  std::unique_ptr<internal::MessagePipeReader, ChannelMojo::ReaderDeleter>
      reader(new internal::MessagePipeReader(
          pipe_, std::move(sender_ptr), std::move(receiver), peer_pid, this));

  bool connected = true;
  {
    base::AutoLock lock(lock_);
    for (size_t i = 0; i < pending_messages_.size(); ++i) {
      if (!reader->Send(std::move(pending_messages_[i]))) {
        LOG(ERROR) << "Failed to flush pending messages";
        pending_messages_.clear();
        connected = false;
        break;
      }
    }

    if (connected) {
      // We set |message_reader_| here and won't get any |pending_messages_|
      // hereafter. Although we might have some if there is an error, we don't
      // care. They cannot be sent anyway.
      message_reader_ = std::move(reader);
      pending_messages_.clear();
      waiting_connect_ = false;
    }
  }

  if (connected)
    listener_->OnChannelConnected(static_cast<int32_t>(GetPeerPID()));
  else
    OnPipeError();
}

void ChannelMojo::OnPipeError() {
  DCHECK(task_runner_);
  if (task_runner_->RunsTasksOnCurrentThread()) {
    listener_->OnChannelError();
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ChannelMojo::OnPipeError, weak_factory_.GetWeakPtr()));
  }
}

bool ChannelMojo::Send(Message* message) {
  bool sent = false;
  {
    base::AutoLock lock(lock_);
    if (!message_reader_) {
      pending_messages_.push_back(base::WrapUnique(message));
      // Counts as OK before the connection is established, but it's an
      // error otherwise.
      return waiting_connect_;
    }

    sent = message_reader_->Send(base::WrapUnique(message));
  }

  if (!sent) {
    OnPipeError();
    return false;
  }

  return true;
}

bool ChannelMojo::IsSendThreadSafe() const {
  return false;
}

base::ProcessId ChannelMojo::GetPeerPID() const {
  base::AutoLock lock(lock_);
  if (!message_reader_)
    return base::kNullProcessId;

  return message_reader_->GetPeerPid();
}

base::ProcessId ChannelMojo::GetSelfPID() const {
  return bootstrap_->GetSelfPID();
}

void ChannelMojo::OnMessageReceived(const Message& message) {
  TRACE_EVENT2("ipc,toplevel", "ChannelMojo::OnMessageReceived",
               "class", IPC_MESSAGE_ID_CLASS(message.type()),
               "line", IPC_MESSAGE_ID_LINE(message.type()));
  if (AttachmentBroker* broker = AttachmentBroker::GetGlobal()) {
    if (broker->OnMessageReceived(message))
      return;
  }
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
int ChannelMojo::GetClientFileDescriptor() const {
  return -1;
}

base::ScopedFD ChannelMojo::TakeClientFileDescriptor() {
  return base::ScopedFD(GetClientFileDescriptor());
}
#endif  // defined(OS_POSIX) && !defined(OS_NACL_SFI)

// static
MojoResult ChannelMojo::ReadFromMessageAttachmentSet(
    Message* message,
    mojo::Array<mojom::SerializedHandlePtr>* handles) {
  if (message->HasAttachments()) {
    MessageAttachmentSet* set = message->attachment_set();
    for (unsigned i = 0; i < set->num_non_brokerable_attachments(); ++i) {
      MojoResult result = WrapAttachment(
              set->GetNonBrokerableAttachmentAt(i).get(), handles);
      if (result != MOJO_RESULT_OK) {
        set->CommitAllDescriptors();
        return result;
      }
    }
    for (unsigned i = 0; i < set->num_brokerable_attachments(); ++i) {
      MojoResult result =
          WrapAttachment(set->GetBrokerableAttachmentAt(i).get(), handles);
      if (result != MOJO_RESULT_OK) {
        set->CommitAllDescriptors();
        return result;
      }
    }
    set->CommitAllDescriptors();
  }
  return MOJO_RESULT_OK;
}

// static
MojoResult ChannelMojo::WriteToMessageAttachmentSet(
    mojo::Array<mojom::SerializedHandlePtr> handle_buffer,
    Message* message) {
  for (size_t i = 0; i < handle_buffer.size(); ++i) {
    scoped_refptr<MessageAttachment> unwrapped_attachment;
    MojoResult unwrap_result = UnwrapAttachment(std::move(handle_buffer[i]),
                                                    &unwrapped_attachment);
    if (unwrap_result != MOJO_RESULT_OK) {
      LOG(WARNING) << "Pipe failed to unwrap handles. Closing: "
                   << unwrap_result;
      return unwrap_result;
    }
    DCHECK(unwrapped_attachment);

    bool ok = message->attachment_set()->AddAttachment(
        std::move(unwrapped_attachment));
    DCHECK(ok);
    if (!ok) {
      LOG(ERROR) << "Failed to add new Mojo handle.";
      return MOJO_RESULT_UNKNOWN;
    }
  }
  return MOJO_RESULT_OK;
}

}  // namespace IPC

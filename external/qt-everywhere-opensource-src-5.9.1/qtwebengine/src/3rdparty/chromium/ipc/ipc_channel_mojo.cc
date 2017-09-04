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
#include "base/process/process_handle.h"
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
  MojoChannelFactory(
      mojo::ScopedMessagePipeHandle handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
      : handle_(std::move(handle)),
        mode_(mode),
        ipc_task_runner_(ipc_task_runner) {}

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
    return ChannelMojo::Create(
        std::move(handle_), mode_, listener, ipc_task_runner_);
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() override {
    return ipc_task_runner_;
  }

 private:
  mojo::ScopedMessagePipeHandle handle_;
  const Channel::Mode mode_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

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
                          std::vector<mojom::SerializedHandlePtr>* handles) {
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

base::ProcessId GetSelfPID() {
#if defined(OS_LINUX)
  if (int global_pid = Channel::GetGlobalPid())
    return global_pid;
#endif  // OS_LINUX
#if defined(OS_NACL)
  return -1;
#else
  return base::GetCurrentProcId();
#endif  // defined(OS_NACL)
}

}  // namespace

//------------------------------------------------------------------------------

// static
std::unique_ptr<ChannelMojo> ChannelMojo::Create(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  return base::WrapUnique(
      new ChannelMojo(std::move(handle), mode, listener, ipc_task_runner));
}

// static
std::unique_ptr<ChannelFactory> ChannelMojo::CreateServerFactory(
    mojo::ScopedMessagePipeHandle handle,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  return base::MakeUnique<MojoChannelFactory>(
      std::move(handle), Channel::MODE_SERVER, ipc_task_runner);
}

// static
std::unique_ptr<ChannelFactory> ChannelMojo::CreateClientFactory(
    mojo::ScopedMessagePipeHandle handle,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  return base::MakeUnique<MojoChannelFactory>(
      std::move(handle), Channel::MODE_CLIENT, ipc_task_runner);
}

ChannelMojo::ChannelMojo(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
    : pipe_(handle.get()), listener_(listener), weak_factory_(this) {
  // Create MojoBootstrap after all members are set as it touches
  // ChannelMojo from a different thread.
  bootstrap_ =
      MojoBootstrap::Create(std::move(handle), mode, this, ipc_task_runner);
}

ChannelMojo::~ChannelMojo() {
  Close();
}

bool ChannelMojo::Connect() {
  WillConnect();

  DCHECK(!task_runner_);
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(!message_reader_);

  bootstrap_->Connect();
  return true;
}

void ChannelMojo::Pause() {
  bootstrap_->Pause();
}

void ChannelMojo::Unpause(bool flush) {
  bootstrap_->Unpause();
  if (flush)
    Flush();
}

void ChannelMojo::Flush() {
  bootstrap_->Flush();
}

void ChannelMojo::Close() {
  // NOTE: The MessagePipeReader's destructor may re-enter this function. Use
  // caution when changing this method.
  std::unique_ptr<internal::MessagePipeReader> reader =
      std::move(message_reader_);
  reader.reset();

  base::AutoLock lock(associated_interface_lock_);
  associated_interfaces_.clear();
}

// MojoBootstrap::Delegate implementation
void ChannelMojo::OnPipesAvailable(mojom::ChannelAssociatedPtr sender,
                                   mojom::ChannelAssociatedRequest receiver) {
  sender->SetPeerPid(GetSelfPID());
  message_reader_.reset(new internal::MessagePipeReader(
      pipe_, std::move(sender), std::move(receiver), this));
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

void ChannelMojo::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  GenericAssociatedInterfaceFactory factory;
  {
    base::AutoLock locker(associated_interface_lock_);
    auto iter = associated_interfaces_.find(name);
    if (iter != associated_interfaces_.end())
      factory = iter->second;
  }

  if (!factory.is_null())
    factory.Run(std::move(handle));
  else
    listener_->OnAssociatedInterfaceRequest(name, std::move(handle));
}

bool ChannelMojo::Send(Message* message) {
  std::unique_ptr<Message> scoped_message = base::WrapUnique(message);
  if (!message_reader_)
    return false;

  // Comment copied from ipc_channel_posix.cc:
  // We can't close the pipe here, because calling OnChannelError may destroy
  // this object, and that would be bad if we are called from Send(). Instead,
  // we return false and hope the caller will close the pipe. If they do not,
  // the pipe will still be closed next time OnFileCanReadWithoutBlocking is
  // called.
  //
  // With Mojo, there's no OnFileCanReadWithoutBlocking, but we expect the
  // pipe's connection error handler will be invoked in its place.
  return message_reader_->Send(std::move(scoped_message));
}

Channel::AssociatedInterfaceSupport*
ChannelMojo::GetAssociatedInterfaceSupport() { return this; }

void ChannelMojo::OnPeerPidReceived(int32_t peer_pid) {
  listener_->OnChannelConnected(peer_pid);
}

void ChannelMojo::OnMessageReceived(const Message& message) {
  TRACE_EVENT2("ipc,toplevel", "ChannelMojo::OnMessageReceived",
               "class", IPC_MESSAGE_ID_CLASS(message.type()),
               "line", IPC_MESSAGE_ID_LINE(message.type()));
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

// static
MojoResult ChannelMojo::ReadFromMessageAttachmentSet(
    Message* message,
    base::Optional<std::vector<mojom::SerializedHandlePtr>>* handles) {
  DCHECK(!*handles);

  MojoResult result = MOJO_RESULT_OK;
  if (!message->HasAttachments())
    return result;

  std::vector<mojom::SerializedHandlePtr> output_handles;
  MessageAttachmentSet* set = message->attachment_set();

  for (unsigned i = 0;
       result == MOJO_RESULT_OK && i < set->num_non_brokerable_attachments();
       ++i) {
    result = WrapAttachment(set->GetNonBrokerableAttachmentAt(i).get(),
                            &output_handles);
  }
  for (unsigned i = 0;
       result == MOJO_RESULT_OK && i < set->num_brokerable_attachments(); ++i) {
    result = WrapAttachment(set->GetBrokerableAttachmentAt(i).get(),
                            &output_handles);
  }

  set->CommitAllDescriptors();

  if (!output_handles.empty())
    *handles = std::move(output_handles);

  return result;
}

// static
MojoResult ChannelMojo::WriteToMessageAttachmentSet(
    base::Optional<std::vector<mojom::SerializedHandlePtr>> handle_buffer,
    Message* message) {
  if (!handle_buffer)
    return MOJO_RESULT_OK;
  for (size_t i = 0; i < handle_buffer->size(); ++i) {
    scoped_refptr<MessageAttachment> unwrapped_attachment;
    MojoResult unwrap_result =
        UnwrapAttachment(std::move((*handle_buffer)[i]), &unwrapped_attachment);
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

mojo::AssociatedGroup* ChannelMojo::GetAssociatedGroup() {
  DCHECK(bootstrap_);
  return bootstrap_->GetAssociatedGroup();
}

void ChannelMojo::AddGenericAssociatedInterface(
    const std::string& name,
    const GenericAssociatedInterfaceFactory& factory) {
  base::AutoLock locker(associated_interface_lock_);
  auto result = associated_interfaces_.insert({ name, factory });
  DCHECK(result.second);
}

void ChannelMojo::GetGenericRemoteAssociatedInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (message_reader_)
    message_reader_->GetRemoteInterface(name, std::move(handle));
}

}  // namespace IPC

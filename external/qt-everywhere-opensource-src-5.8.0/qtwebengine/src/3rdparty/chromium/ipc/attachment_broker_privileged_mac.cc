// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged_mac.h"

#include <stdint.h>

#include <tuple>

#include "base/mac/mach_port_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/memory/shared_memory.h"
#include "base/process/port_provider_mac.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "ipc/attachment_broker_messages.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_channel.h"
#include "ipc/mach_port_attachment_mac.h"

namespace IPC {

AttachmentBrokerPrivilegedMac::AttachmentBrokerPrivilegedMac(
    base::PortProvider* port_provider)
    : port_provider_(port_provider) {
  port_provider_->AddObserver(this);
}

AttachmentBrokerPrivilegedMac::~AttachmentBrokerPrivilegedMac() {
  port_provider_->RemoveObserver(this);
  {
    base::AutoLock l(precursors_lock_);
    for (auto it : precursors_)
      delete it.second;
  }
  {
    base::AutoLock l(extractors_lock_);
    for (auto it : extractors_)
      delete it.second;
  }
}

bool AttachmentBrokerPrivilegedMac::SendAttachmentToProcess(
    const scoped_refptr<IPC::BrokerableAttachment>& attachment,
    base::ProcessId destination_process) {
  switch (attachment->GetBrokerableType()) {
    case BrokerableAttachment::MACH_PORT: {
      internal::MachPortAttachmentMac* mach_port_attachment =
          static_cast<internal::MachPortAttachmentMac*>(attachment.get());
      MachPortWireFormat wire_format =
          mach_port_attachment->GetWireFormat(destination_process);
      AddPrecursor(wire_format.destination_process,
                   base::mac::ScopedMachSendRight(wire_format.mach_port),
                   wire_format.attachment_id);
      mach_port_attachment->reset_mach_port_ownership();
      SendPrecursorsForProcess(wire_format.destination_process, true);
      return true;
    }
    default:
      NOTREACHED();
      return false;
  }
  return false;
}

void AttachmentBrokerPrivilegedMac::DeregisterCommunicationChannel(
    Endpoint* endpoint) {
  AttachmentBrokerPrivileged::DeregisterCommunicationChannel(endpoint);

  if (!endpoint)
    return;

  base::ProcessId pid = endpoint->GetPeerPID();
  if (pid == base::kNullProcessId)
    return;

  {
    base::AutoLock l(precursors_lock_);
    auto it = precursors_.find(pid);
    if (it != precursors_.end()) {
      delete it->second;
      precursors_.erase(pid);
    }
  }

  {
    base::AutoLock l(extractors_lock_);
    auto it = extractors_.find(pid);
    if (it != extractors_.end()) {
      delete it->second;
      extractors_.erase(pid);
    }
  }
}

void AttachmentBrokerPrivilegedMac::ReceivedPeerPid(base::ProcessId peer_pid) {
  ProcessExtractorsForProcess(peer_pid, false);
  SendPrecursorsForProcess(peer_pid, false);
}

bool AttachmentBrokerPrivilegedMac::OnMessageReceived(const Message& msg) {
  bool handled = true;
  switch (msg.type()) {
    IPC_MESSAGE_HANDLER_GENERIC(AttachmentBrokerMsg_DuplicateMachPort,
                                OnDuplicateMachPort(msg))
    IPC_MESSAGE_UNHANDLED(handled = false)
  }
  return handled;
}

void AttachmentBrokerPrivilegedMac::OnReceivedTaskPort(
    base::ProcessHandle process) {
  SendPrecursorsForProcess(process, true);
}

AttachmentBrokerPrivilegedMac::AttachmentPrecursor::AttachmentPrecursor(
    const base::ProcessId& pid,
    base::mac::ScopedMachSendRight port,
    const BrokerableAttachment::AttachmentId& id)
    : pid_(pid), port_(port.release()), id_(id) {}

AttachmentBrokerPrivilegedMac::AttachmentPrecursor::~AttachmentPrecursor() {}

base::mac::ScopedMachSendRight
AttachmentBrokerPrivilegedMac::AttachmentPrecursor::TakePort() {
  return base::mac::ScopedMachSendRight(port_.release());
}

AttachmentBrokerPrivilegedMac::AttachmentExtractor::AttachmentExtractor(
    const base::ProcessId& source_pid,
    const base::ProcessId& dest_pid,
    mach_port_name_t port,
    const BrokerableAttachment::AttachmentId& id)
    : source_pid_(source_pid),
      dest_pid_(dest_pid),
      port_to_extract_(port),
      id_(id) {}

AttachmentBrokerPrivilegedMac::AttachmentExtractor::~AttachmentExtractor() {}

void AttachmentBrokerPrivilegedMac::OnDuplicateMachPort(
    const IPC::Message& message) {
  DCHECK_NE(0, message.get_sender_pid());
  AttachmentBrokerMsg_DuplicateMachPort::Param param;
  if (!AttachmentBrokerMsg_DuplicateMachPort::Read(&message, &param)) {
    LogError(ERROR_PARSE_DUPLICATE_MACH_PORT_MESSAGE);
    return;
  }
  IPC::internal::MachPortAttachmentMac::WireFormat wire_format =
      std::get<0>(param);

  if (wire_format.destination_process == base::kNullProcessId) {
    LogError(NO_DESTINATION);
    return;
  }

  AddExtractor(message.get_sender_pid(), wire_format.destination_process,
               wire_format.mach_port, wire_format.attachment_id);
  ProcessExtractorsForProcess(message.get_sender_pid(), true);
}

void AttachmentBrokerPrivilegedMac::RoutePrecursorToSelf(
    AttachmentPrecursor* precursor) {
  DCHECK_EQ(base::Process::Current().Pid(), precursor->pid());

  // Intentionally leak the port, since the attachment takes ownership.
  internal::MachPortAttachmentMac::WireFormat wire_format(
      precursor->TakePort().release(), precursor->pid(), precursor->id());
  scoped_refptr<BrokerableAttachment> attachment(
      new internal::MachPortAttachmentMac(wire_format));
  HandleReceivedAttachment(attachment);
}

bool AttachmentBrokerPrivilegedMac::RouteWireFormatToAnother(
    const MachPortWireFormat& wire_format) {
  DCHECK_NE(wire_format.destination_process, base::Process::Current().Pid());

  // Another process is the destination.
  base::ProcessId dest = wire_format.destination_process;
  base::AutoLock auto_lock(*get_lock());
  AttachmentBrokerPrivileged::EndpointRunnerPair pair =
      GetSenderWithProcessId(dest);
  if (!pair.first) {
    // The extractor was successfully processed, which implies that the
    // communication channel was established. This implies that the
    // communication was taken down in the interim.
    LOG(ERROR) << "Failed to deliver brokerable attachment to process with id: "
               << dest;
    LogError(DESTINATION_NOT_FOUND);
    return false;
  }

  LogError(DESTINATION_FOUND);
  SendMessageToEndpoint(
      pair, new AttachmentBrokerMsg_MachPortHasBeenDuplicated(wire_format));
  return true;
}

base::mac::ScopedMachSendRight AttachmentBrokerPrivilegedMac::ExtractNamedRight(
    mach_port_t task_port,
    mach_port_name_t named_right) {
  if (named_right == MACH_PORT_NULL)
    return base::mac::ScopedMachSendRight(MACH_PORT_NULL);

  mach_port_t extracted_right = MACH_PORT_NULL;
  mach_msg_type_name_t extracted_right_type;
  kern_return_t kr =
      mach_port_extract_right(task_port, named_right, MACH_MSG_TYPE_MOVE_SEND,
                              &extracted_right, &extracted_right_type);
  if (kr != KERN_SUCCESS) {
    LogError(ERROR_EXTRACT_SOURCE_RIGHT);
    return base::mac::ScopedMachSendRight(MACH_PORT_NULL);
  }

  DCHECK_EQ(static_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND),
            extracted_right_type);

  return base::mac::ScopedMachSendRight(extracted_right);
}

AttachmentBrokerPrivilegedMac::MachPortWireFormat
AttachmentBrokerPrivilegedMac::CopyWireFormat(
    const MachPortWireFormat& wire_format,
    uint32_t mach_port) {
  return MachPortWireFormat(mach_port, wire_format.destination_process,
                            wire_format.attachment_id);
}

void AttachmentBrokerPrivilegedMac::SendPrecursorsForProcess(
    base::ProcessId pid,
    bool store_on_failure) {
  base::AutoLock l(precursors_lock_);
  auto it = precursors_.find(pid);
  if (it == precursors_.end())
    return;

  // Whether this process is the destination process.
  bool to_self = pid == base::GetCurrentProcId();

  if (!to_self) {
    base::AutoLock auto_lock(*get_lock());
    AttachmentBrokerPrivileged::EndpointRunnerPair pair =
        GetSenderWithProcessId(pid);
    if (!pair.first) {
      // Try again later.
      if (store_on_failure)
        return;

      // If there is no sender, then permanently fail.
      LogError(DESTINATION_NOT_FOUND);
      delete it->second;
      precursors_.erase(it);
      return;
    }
  }

  mach_port_t task_port = port_provider_->TaskForPid(pid);

  // It's possible that the destination process has not yet provided the
  // privileged process with its task port.
  if (!to_self && task_port == MACH_PORT_NULL)
    return;

  while (!it->second->empty()) {
    auto precursor_it = it->second->begin();
    if (to_self) {
      RoutePrecursorToSelf(*precursor_it);
    } else {
      if (!SendPrecursor(*precursor_it, task_port))
        break;
    }
    it->second->erase(precursor_it);
  }

  delete it->second;
  precursors_.erase(it);
}

bool AttachmentBrokerPrivilegedMac::SendPrecursor(
    AttachmentPrecursor* precursor,
    mach_port_t task_port) {
  DCHECK(task_port);
  internal::MachPortAttachmentMac::WireFormat wire_format(
      MACH_PORT_NULL, precursor->pid(), precursor->id());
  base::mac::ScopedMachSendRight port_to_insert = precursor->TakePort();
  mach_port_name_t intermediate_port = MACH_PORT_NULL;
  if (port_to_insert.get() != MACH_PORT_NULL) {
    base::MachCreateError error_code;
    intermediate_port = base::CreateIntermediateMachPort(
        task_port, base::mac::ScopedMachSendRight(port_to_insert.release()),
        &error_code);
    if (intermediate_port == MACH_PORT_NULL) {
      UMAError uma_error;
      switch (error_code) {
        case base::MachCreateError::ERROR_MAKE_RECEIVE_PORT:
          uma_error = ERROR_MAKE_RECEIVE_PORT;
          break;
        case base::MachCreateError::ERROR_SET_ATTRIBUTES:
          uma_error = ERROR_SET_ATTRIBUTES;
          break;
        case base::MachCreateError::ERROR_EXTRACT_DEST_RIGHT:
          uma_error = ERROR_EXTRACT_DEST_RIGHT;
          break;
        case base::MachCreateError::ERROR_SEND_MACH_PORT:
          uma_error = ERROR_SEND_MACH_PORT;
          break;
      }
      LogError(uma_error);
    }
  }
  return RouteWireFormatToAnother(
      CopyWireFormat(wire_format, intermediate_port));
}

void AttachmentBrokerPrivilegedMac::AddPrecursor(
    base::ProcessId pid,
    base::mac::ScopedMachSendRight port,
    const BrokerableAttachment::AttachmentId& id) {
  base::AutoLock l(precursors_lock_);
  auto it = precursors_.find(pid);
  if (it == precursors_.end())
    precursors_[pid] = new ScopedVector<AttachmentPrecursor>;

  precursors_[pid]->push_back(new AttachmentPrecursor(
      pid, base::mac::ScopedMachSendRight(port.release()), id));
}

void AttachmentBrokerPrivilegedMac::ProcessExtractorsForProcess(
    base::ProcessId pid,
    bool store_on_failure) {
  base::AutoLock l(extractors_lock_);
  auto it = extractors_.find(pid);
  if (it == extractors_.end())
    return;

  {
    base::AutoLock auto_lock(*get_lock());
    AttachmentBrokerPrivileged::EndpointRunnerPair pair =
        GetSenderWithProcessId(pid);
    if (!pair.first) {
      // If there is no sender, then the communication channel with the source
      // process has not yet been established. Try again later.
      if (store_on_failure)
        return;

      // There is no sender. Permanently fail.
      LogError(ERROR_SOURCE_NOT_FOUND);
      delete it->second;
      extractors_.erase(it);
      return;
    }
  }

  mach_port_t task_port = port_provider_->TaskForPid(pid);

  // It's possible that the source process has not yet provided the privileged
  // process with its task port.
  if (task_port == MACH_PORT_NULL)
    return;

  while (!it->second->empty()) {
    auto extractor_it = it->second->begin();
    ProcessExtractor(*extractor_it, task_port);
    it->second->erase(extractor_it);
  }

  delete it->second;
  extractors_.erase(it);
}

void AttachmentBrokerPrivilegedMac::ProcessExtractor(
    AttachmentExtractor* extractor,
    mach_port_t task_port) {
  DCHECK(task_port);
  base::mac::ScopedMachSendRight send_right =
      ExtractNamedRight(task_port, extractor->port());
  AddPrecursor(extractor->dest_pid(),
               base::mac::ScopedMachSendRight(send_right.release()),
               extractor->id());
  SendPrecursorsForProcess(extractor->dest_pid(), true);
}

void AttachmentBrokerPrivilegedMac::AddExtractor(
    base::ProcessId source_pid,
    base::ProcessId dest_pid,
    mach_port_name_t port,
    const BrokerableAttachment::AttachmentId& id) {
  base::AutoLock l(extractors_lock_);
  DCHECK_NE(base::GetCurrentProcId(), source_pid);

  auto it = extractors_.find(source_pid);
  if (it == extractors_.end())
    extractors_[source_pid] = new ScopedVector<AttachmentExtractor>;

  extractors_[source_pid]->push_back(
      new AttachmentExtractor(source_pid, dest_pid, port, id));
}

}  // namespace IPC

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/mach_port_relay.h"

#include <mach/mach.h>

#include <utility>

#include "base/logging.h"
#include "base/mac/mach_port_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "mojo/edk/embedder/platform_handle_vector.h"

namespace mojo {
namespace edk {

namespace {

// Errors that can occur in the broker (privileged parent) process.
// These match tools/metrics/histograms.xml.
// This enum is append-only.
enum class BrokerUMAError : int {
  SUCCESS = 0,
  // Couldn't get a task port for the process with a given pid.
  ERROR_TASK_FOR_PID = 1,
  // Couldn't make a port with receive rights in the destination process.
  ERROR_MAKE_RECEIVE_PORT = 2,
  // Couldn't change the attributes of a Mach port.
  ERROR_SET_ATTRIBUTES = 3,
  // Couldn't extract a right from the destination.
  ERROR_EXTRACT_DEST_RIGHT = 4,
  // Couldn't send a Mach port in a call to mach_msg().
  ERROR_SEND_MACH_PORT = 5,
  // Couldn't extract a right from the source.
  ERROR_EXTRACT_SOURCE_RIGHT = 6,
  ERROR_MAX
};

// Errors that can occur in a child process.
// These match tools/metrics/histograms.xml.
// This enum is append-only.
enum class ChildUMAError : int {
  SUCCESS = 0,
  // An error occurred while trying to receive a Mach port with mach_msg().
  ERROR_RECEIVE_MACH_MESSAGE = 1,
  ERROR_MAX
};

void ReportBrokerError(BrokerUMAError error) {
  UMA_HISTOGRAM_ENUMERATION("Mojo.MachPortRelay.BrokerError",
                            static_cast<int>(error),
                            static_cast<int>(BrokerUMAError::ERROR_MAX));
}

void ReportChildError(ChildUMAError error) {
  UMA_HISTOGRAM_ENUMERATION("Mojo.MachPortRelay.ChildError",
                            static_cast<int>(error),
                            static_cast<int>(ChildUMAError::ERROR_MAX));
}

}  // namespace

// static
bool MachPortRelay::ReceivePorts(PlatformHandleVector* handles) {
  DCHECK(handles);

  for (size_t i = 0; i < handles->size(); i++) {
    PlatformHandle* handle = handles->data() + i;
    DCHECK(handle->type != PlatformHandle::Type::MACH);
    if (handle->type != PlatformHandle::Type::MACH_NAME)
      continue;

    if (handle->port == MACH_PORT_NULL) {
      handle->type = PlatformHandle::Type::MACH;
      continue;
    }

    base::mac::ScopedMachReceiveRight message_port(handle->port);
    base::mac::ScopedMachSendRight received_port(
        base::ReceiveMachPort(message_port.get()));
    if (received_port.get() == MACH_PORT_NULL) {
      ReportChildError(ChildUMAError::ERROR_RECEIVE_MACH_MESSAGE);
      handle->port = MACH_PORT_NULL;
      LOG(ERROR) << "Error receiving mach port";
      return false;
    }

    ReportChildError(ChildUMAError::SUCCESS);
    handle->port = received_port.release();
    handle->type = PlatformHandle::Type::MACH;
  }

  return true;
}

MachPortRelay::MachPortRelay(base::PortProvider* port_provider)
    : port_provider_(port_provider) {
  DCHECK(port_provider);
  port_provider_->AddObserver(this);
}

MachPortRelay::~MachPortRelay() {
  port_provider_->RemoveObserver(this);
}

bool MachPortRelay::SendPortsToProcess(Channel::Message* message,
                                       base::ProcessHandle process) {
  DCHECK(message);
  mach_port_t task_port = port_provider_->TaskForPid(process);
  if (task_port == MACH_PORT_NULL) {
    // Callers check the port provider for the task port before calling this
    // function, in order to queue pending messages. Therefore, if this fails,
    // it should be considered a genuine, bona fide, electrified, six-car error.
    ReportBrokerError(BrokerUMAError::ERROR_TASK_FOR_PID);
    return false;
  }

  size_t num_sent = 0;
  bool error = false;
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  // Message should have handles, otherwise there's no point in calling this
  // function.
  DCHECK(handles);
  for (size_t i = 0; i < handles->size(); i++) {
    PlatformHandle* handle = &(*handles)[i];
    DCHECK(handle->type != PlatformHandle::Type::MACH_NAME);
    if (handle->type != PlatformHandle::Type::MACH)
      continue;

    if (handle->port == MACH_PORT_NULL) {
      handle->type = PlatformHandle::Type::MACH_NAME;
      num_sent++;
      continue;
    }

    mach_port_name_t intermediate_port;
    base::MachCreateError error_code;
    intermediate_port = base::CreateIntermediateMachPort(
        task_port, base::mac::ScopedMachSendRight(handle->port), &error_code);
    if (intermediate_port == MACH_PORT_NULL) {
      BrokerUMAError uma_error;
      switch (error_code) {
        case base::MachCreateError::ERROR_MAKE_RECEIVE_PORT:
          uma_error = BrokerUMAError::ERROR_MAKE_RECEIVE_PORT;
          break;
        case base::MachCreateError::ERROR_SET_ATTRIBUTES:
          uma_error = BrokerUMAError::ERROR_SET_ATTRIBUTES;
          break;
        case base::MachCreateError::ERROR_EXTRACT_DEST_RIGHT:
          uma_error = BrokerUMAError::ERROR_EXTRACT_DEST_RIGHT;
          break;
        case base::MachCreateError::ERROR_SEND_MACH_PORT:
          uma_error = BrokerUMAError::ERROR_SEND_MACH_PORT;
          break;
      }
      ReportBrokerError(uma_error);
      handle->port = MACH_PORT_NULL;
      error = true;
      break;
    }

    ReportBrokerError(BrokerUMAError::SUCCESS);
    handle->port = intermediate_port;
    handle->type = PlatformHandle::Type::MACH_NAME;
    num_sent++;
  }
  DCHECK(error || num_sent);
  message->SetHandles(std::move(handles));

  return !error;
}

bool MachPortRelay::ExtractPortRights(Channel::Message* message,
                                      base::ProcessHandle process) {
  DCHECK(message);

  mach_port_t task_port = port_provider_->TaskForPid(process);
  if (task_port == MACH_PORT_NULL) {
    ReportBrokerError(BrokerUMAError::ERROR_TASK_FOR_PID);
    return false;
  }

  size_t num_received = 0;
  bool error = false;
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  // Message should have handles, otherwise there's no point in calling this
  // function.
  DCHECK(handles);
  for (size_t i = 0; i < handles->size(); i++) {
    PlatformHandle* handle = handles->data() + i;
    DCHECK(handle->type != PlatformHandle::Type::MACH);
    if (handle->type != PlatformHandle::Type::MACH_NAME)
      continue;

    if (handle->port == MACH_PORT_NULL) {
      handle->type = PlatformHandle::Type::MACH;
      num_received++;
      continue;
    }

    mach_port_t extracted_right = MACH_PORT_NULL;
    mach_msg_type_name_t extracted_right_type;
    kern_return_t kr =
        mach_port_extract_right(task_port, handle->port,
                                MACH_MSG_TYPE_MOVE_SEND,
                                &extracted_right, &extracted_right_type);
    if (kr != KERN_SUCCESS) {
      ReportBrokerError(BrokerUMAError::ERROR_EXTRACT_SOURCE_RIGHT);
      error = true;
      break;
    }

    ReportBrokerError(BrokerUMAError::SUCCESS);
    DCHECK_EQ(static_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND),
              extracted_right_type);
    handle->port = extracted_right;
    handle->type = PlatformHandle::Type::MACH;
    num_received++;
  }
  DCHECK(error || num_received);
  message->SetHandles(std::move(handles));

  return !error;
}

void MachPortRelay::AddObserver(Observer* observer) {
  base::AutoLock locker(observers_lock_);
  bool inserted = observers_.insert(observer).second;
  DCHECK(inserted);
}

void MachPortRelay::RemoveObserver(Observer* observer) {
  base::AutoLock locker(observers_lock_);
  observers_.erase(observer);
}

void MachPortRelay::OnReceivedTaskPort(base::ProcessHandle process) {
  base::AutoLock locker(observers_lock_);
  for (auto* observer : observers_)
    observer->OnProcessReady(process);
}

}  // namespace edk
}  // namespace mojo

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains Mac-specific utility functions used by multiple test
// suites.

#ifndef IPC_TEST_UTIL_MAC_H_
#define IPC_TEST_UTIL_MAC_H_

#include <mach/mach.h>
#include <stddef.h>

#include <string>

#include "base/mac/scoped_mach_port.h"

namespace IPC {

// Returns a random name suitable for Mach Server registration.
std::string CreateRandomServiceName();

// Makes the current process into a Mach Server with the given |service_name|.
// Returns a receive right for the service port.
base::mac::ScopedMachReceiveRight BecomeMachServer(const char* service_name);

// Returns a send right to the service port for the Mach Server with the given
// |service_name|.
base::mac::ScopedMachSendRight LookupServer(const char* service_name);

// Returns the receive right to a newly minted Mach port.
base::mac::ScopedMachReceiveRight MakeReceivingPort();

// Blocks until a Mach message is sent to |port_to_listen_on|. This Mach message
// must contain a Mach port. Returns that Mach port.
base::mac::ScopedMachSendRight ReceiveMachPort(mach_port_t port_to_listen_on);

// Passes a copy of the send right of |port_to_send| to |receiving_port|, using
// the given |disposition|.
void SendMachPort(mach_port_t receiving_port,
                  mach_port_t port_to_send,
                  int disposition);

// The number of active names in the current task's port name space.
mach_msg_type_number_t GetActiveNameCount();

// The number of references the current task has for a given name.
mach_port_urefs_t GetMachRefCount(mach_port_name_t name,
                                  mach_port_right_t right);

// Increments the ref count for the right/name pair.
void IncrementMachRefCount(mach_port_name_t name, mach_port_right_t right);

// Gets the current and maximum protection levels of the memory region.
// Returns whether the operation was successful.
// |current| and |max| are output variables only populated on success.
bool GetMachProtections(void* address, size_t size, int* current, int* max);

}  // namespace IPC

#endif  // IPC_TEST_UTIL_MAC_H_

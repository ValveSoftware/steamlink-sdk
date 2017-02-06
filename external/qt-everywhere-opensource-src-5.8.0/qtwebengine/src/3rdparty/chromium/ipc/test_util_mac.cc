// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/test_util_mac.h"

#include <mach/mach_vm.h>
#include <servers/bootstrap.h>
#include <stddef.h>

#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"

namespace {

// Structs used to pass a Mach port over mach_msg().
struct MachSendPortMessage {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t data;
};

struct MachReceivePortMessage : public MachSendPortMessage {
  mach_msg_trailer_t trailer;
};

}  // namespace

namespace IPC {

std::string CreateRandomServiceName() {
  return base::StringPrintf("TestUtilMac.%llu", base::RandUint64());
}

base::mac::ScopedMachReceiveRight BecomeMachServer(const char* service_name) {
  mach_port_t port;
  kern_return_t kr = bootstrap_check_in(bootstrap_port, service_name, &port);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "BecomeMachServer ";
  return base::mac::ScopedMachReceiveRight(port);
}

base::mac::ScopedMachSendRight LookupServer(const char* service_name) {
  mach_port_t server_port;
  kern_return_t kr =
      bootstrap_look_up(bootstrap_port, service_name, &server_port);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "LookupServer";
  return base::mac::ScopedMachSendRight(server_port);
}

base::mac::ScopedMachReceiveRight MakeReceivingPort() {
  mach_port_t client_port;
  kern_return_t kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE, &client_port);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "MakeReceivingPort";
  return base::mac::ScopedMachReceiveRight(client_port);
}

base::mac::ScopedMachSendRight ReceiveMachPort(mach_port_t port_to_listen_on) {
  MachReceivePortMessage recv_msg;
  mach_msg_header_t* recv_hdr = &recv_msg.header;
  recv_hdr->msgh_local_port = port_to_listen_on;
  recv_hdr->msgh_size = sizeof(recv_msg);
  kern_return_t kr =
      mach_msg(recv_hdr, MACH_RCV_MSG, 0, recv_hdr->msgh_size,
               port_to_listen_on, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "ReceiveMachPort";
  mach_port_t other_task_port = recv_msg.data.name;
  return base::mac::ScopedMachSendRight(other_task_port);
}

// Passes a copy of the send right of |port_to_send| to |receiving_port|.
void SendMachPort(mach_port_t receiving_port,
                  mach_port_t port_to_send,
                  int disposition) {
  MachSendPortMessage send_msg;
  send_msg.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
  send_msg.header.msgh_size = sizeof(send_msg);
  send_msg.header.msgh_remote_port = receiving_port;
  send_msg.header.msgh_local_port = MACH_PORT_NULL;
  send_msg.header.msgh_reserved = 0;
  send_msg.header.msgh_id = 0;
  send_msg.body.msgh_descriptor_count = 1;
  send_msg.data.name = port_to_send;
  send_msg.data.disposition = disposition;
  send_msg.data.type = MACH_MSG_PORT_DESCRIPTOR;
  int kr = mach_msg(&send_msg.header, MACH_SEND_MSG, send_msg.header.msgh_size,
                    0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "SendMachPort";
}

mach_msg_type_number_t GetActiveNameCount() {
  mach_port_name_array_t name_array;
  mach_msg_type_number_t names_count;
  mach_port_type_array_t type_array;
  mach_msg_type_number_t types_count;
  kern_return_t kr = mach_port_names(mach_task_self(), &name_array,
                                     &names_count, &type_array, &types_count);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "GetActiveNameCount";
  return names_count;
}

mach_port_urefs_t GetMachRefCount(mach_port_name_t name,
                                  mach_port_right_t right) {
  mach_port_urefs_t ref_count;
  kern_return_t kr = mach_port_get_refs(mach_task_self(), name,
                                        MACH_PORT_RIGHT_SEND, &ref_count);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "GetRefCount";
  return ref_count;
}

void IncrementMachRefCount(mach_port_name_t name, mach_port_right_t right) {
  kern_return_t kr = mach_port_mod_refs(mach_task_self(), name, right, 1);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "GetRefCount";
}

bool GetMachProtections(void* address, size_t size, int* current, int* max) {
  vm_region_info_t region_info;
  mach_vm_address_t mem_address = reinterpret_cast<mach_vm_address_t>(address);
  mach_vm_size_t mem_size = size;
  vm_region_basic_info_64 basic_info;

  region_info = reinterpret_cast<vm_region_recurse_info_t>(&basic_info);
  vm_region_flavor_t flavor = VM_REGION_BASIC_INFO_64;
  memory_object_name_t memory_object;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;

  kern_return_t kr =
      mach_vm_region(mach_task_self(), &mem_address, &mem_size, flavor,
                     region_info, &count, &memory_object);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to get region info.";
    return false;
  }

  *current = basic_info.protection;
  *max = basic_info.max_protection;
  return true;
}

}  // namespace IPC

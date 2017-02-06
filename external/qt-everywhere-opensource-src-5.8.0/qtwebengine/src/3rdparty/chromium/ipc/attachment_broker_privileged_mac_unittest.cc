// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged_mac.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/mac/mach_port_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/process/port_provider_mac.h"
#include "base/process/process_handle.h"
#include "base/sys_info.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "ipc/test_util_mac.h"
#include "testing/multiprocess_func_list.h"

namespace IPC {

namespace {

static const std::string g_service_switch_name = "service_name";

// Sends a uint32_t to a mach port.
void SendUInt32(mach_port_t port, uint32_t message) {
  int message_size = sizeof(uint32_t);
  int total_size = message_size + sizeof(mach_msg_header_t);
  void* buffer = malloc(total_size);
  mach_msg_header_t* header = (mach_msg_header_t*)buffer;
  header->msgh_remote_port = port;
  header->msgh_local_port = MACH_PORT_NULL;
  header->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  header->msgh_reserved = 0;
  header->msgh_id = 0;
  header->msgh_size = total_size;
  memcpy(static_cast<char*>(buffer) + sizeof(mach_msg_header_t), &message,
         message_size);

  kern_return_t kr;
  kr = mach_msg(static_cast<mach_msg_header_t*>(buffer), MACH_SEND_MSG,
                total_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE,
                MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "SendUInt32";
  free(buffer);
}

// Receives a uint32_t from a mach port.
uint32_t ReceiveUInt32(mach_port_t listening_port) {
  int message_size = sizeof(uint32_t);
  int total_size =
      message_size + sizeof(mach_msg_header_t) + sizeof(mach_msg_trailer_t);
  int options = MACH_RCV_MSG;
  void* buffer = malloc(total_size);

  int kr =
      mach_msg(static_cast<mach_msg_header_t*>(buffer), options, 0, total_size,
               listening_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "ReceiveUInt32";

  uint32_t response;
  memcpy(&response, static_cast<char*>(buffer) + sizeof(mach_msg_header_t),
         message_size);

  free(buffer);
  return response;
}

// Sets up the mach communication ports with the server. Returns a port to which
// the server will send mach objects.
// |original_name_count| is an output variable that describes the number of
// active names in this task before the task port is shared with the server.
base::mac::ScopedMachReceiveRight CommonChildProcessSetUp(
    mach_msg_type_number_t* original_name_count) {
  base::CommandLine cmd_line = *base::CommandLine::ForCurrentProcess();
  std::string service_name =
      cmd_line.GetSwitchValueASCII(g_service_switch_name);
  base::mac::ScopedMachSendRight server_port(
      LookupServer(service_name.c_str()));
  base::mac::ScopedMachReceiveRight client_port(MakeReceivingPort());

  // |server_port| is a newly allocated right which will be deallocated once
  // this method returns.
  *original_name_count = GetActiveNameCount() - 1;

  // Send the port that this process is listening on to the server.
  SendMachPort(server_port.get(), client_port.get(), MACH_MSG_TYPE_MAKE_SEND);

  // Send the task port for this process.
  SendMachPort(server_port.get(), mach_task_self(), MACH_MSG_TYPE_COPY_SEND);
  return client_port;
}

// Creates a new shared memory region populated with 'a'.
std::unique_ptr<base::SharedMemory> CreateAndPopulateSharedMemoryHandle(
    size_t size) {
  base::SharedMemoryHandle shm(size);
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(shm, false));
  shared_memory->Map(size);
  memset(shared_memory->memory(), 'a', size);
  return shared_memory;
}

// Create a shared memory region from a memory object. The returned object takes
// ownership of |memory_object|.
std::unique_ptr<base::SharedMemory> MapMemoryObject(mach_port_t memory_object,
                                                    size_t size) {
  base::SharedMemoryHandle shm(memory_object, size, base::GetCurrentProcId());
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(shm, false));
  shared_memory->Map(size);
  return shared_memory;
}

class MockPortProvider : public base::PortProvider {
 public:
  MockPortProvider() {}
  ~MockPortProvider() override {}
  mach_port_t TaskForPid(base::ProcessHandle process) const override {
    return MACH_PORT_NULL;
  }
};

}  // namespace

class AttachmentBrokerPrivilegedMacMultiProcessTest
    : public base::MultiProcessTest {
 public:
  AttachmentBrokerPrivilegedMacMultiProcessTest() {}

  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine command_line = MultiProcessTest::MakeCmdLine(procname);
    // Pass the service name to the child process.
    command_line.AppendSwitchASCII(g_service_switch_name, service_name_);
    return command_line;
  }

  void SetUpChild(const std::string& name) {
    // Make a random service name so that this test doesn't conflict with other
    // similar tests.
    service_name_ = CreateRandomServiceName();
    server_port_.reset(BecomeMachServer(service_name_.c_str()).release());
    child_process_ = SpawnChild(name);
    client_port_.reset(ReceiveMachPort(server_port_.get()).release());
    client_task_port_.reset(ReceiveMachPort(server_port_.get()).release());
  }

  static const int s_memory_size = 99999;

 protected:
  std::string service_name_;

  // A port on which the main process listens for mach messages from the child
  // process.
  base::mac::ScopedMachReceiveRight server_port_;

  // A port on which the child process listens for mach messages from the main
  // process.
  base::mac::ScopedMachSendRight client_port_;

  // Child process's task port.
  base::mac::ScopedMachSendRight client_task_port_;

  // Dummy port provider.
  MockPortProvider port_provider_;

  base::Process child_process_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivilegedMacMultiProcessTest);
};

// The attachment broker inserts a right for a memory object into the
// destination task.
TEST_F(AttachmentBrokerPrivilegedMacMultiProcessTest, InsertRight) {
  SetUpChild("InsertRightClient");
  mach_msg_type_number_t original_name_count = GetActiveNameCount();
  IPC::AttachmentBrokerPrivilegedMac broker(&port_provider_);

  // Create some shared memory.
  std::unique_ptr<base::SharedMemory> shared_memory =
      CreateAndPopulateSharedMemoryHandle(s_memory_size);
  ASSERT_TRUE(shared_memory->handle().IsValid());

  // Insert the memory object into the destination task, via an intermediate
  // port.
  IncrementMachRefCount(shared_memory->handle().GetMemoryObject(),
                        MACH_PORT_RIGHT_SEND);
  mach_port_name_t inserted_memory_object = base::CreateIntermediateMachPort(
      client_task_port_.get(),
      base::mac::ScopedMachSendRight(shared_memory->handle().GetMemoryObject()),
      nullptr);
  EXPECT_NE(inserted_memory_object,
            static_cast<mach_port_name_t>(MACH_PORT_NULL));
  SendUInt32(client_port_.get(), inserted_memory_object);

  // Check that no names have been leaked.
  shared_memory.reset();
  EXPECT_EQ(original_name_count, GetActiveNameCount());

  int rv = -1;
  ASSERT_TRUE(child_process_.WaitForExitWithTimeout(
      TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(InsertRightClient) {
  mach_msg_type_number_t original_name_count = 0;
  base::mac::ScopedMachReceiveRight client_port(
      CommonChildProcessSetUp(&original_name_count).release());
  base::mac::ScopedMachReceiveRight inserted_port(
      ReceiveUInt32(client_port.get()));
  base::mac::ScopedMachSendRight memory_object(
      ReceiveMachPort(inserted_port.get()));
  inserted_port.reset();

  // The server should have inserted a right into this process.
  EXPECT_EQ(original_name_count + 1, GetActiveNameCount());

  // Map the memory object and check its contents.
  std::unique_ptr<base::SharedMemory> shared_memory(MapMemoryObject(
      memory_object.release(),
      AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size));
  const char* start = static_cast<const char*>(shared_memory->memory());
  for (int i = 0;
       i < AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size; ++i) {
    DCHECK_EQ(start[i], 'a');
  }

  // Check that no names have been leaked.
  shared_memory.reset();
  EXPECT_EQ(original_name_count, GetActiveNameCount());

  return 0;
}

// The attachment broker inserts the right for a memory object into the
// destination task twice.
TEST_F(AttachmentBrokerPrivilegedMacMultiProcessTest, InsertSameRightTwice) {
  SetUpChild("InsertSameRightTwiceClient");
  mach_msg_type_number_t original_name_count = GetActiveNameCount();
  IPC::AttachmentBrokerPrivilegedMac broker(&port_provider_);

  // Create some shared memory.
  std::unique_ptr<base::SharedMemory> shared_memory =
      CreateAndPopulateSharedMemoryHandle(s_memory_size);
  ASSERT_TRUE(shared_memory->handle().IsValid());

  // Insert the memory object into the destination task, via an intermediate
  // port, twice.
  for (int i = 0; i < 2; ++i) {
    IncrementMachRefCount(shared_memory->handle().GetMemoryObject(),
                          MACH_PORT_RIGHT_SEND);
    mach_port_name_t inserted_memory_object = base::CreateIntermediateMachPort(
        client_task_port_.get(),
        base::mac::ScopedMachSendRight(
            shared_memory->handle().GetMemoryObject()),
        nullptr);
    EXPECT_NE(inserted_memory_object,
              static_cast<mach_port_name_t>(MACH_PORT_NULL));
    SendUInt32(client_port_.get(), inserted_memory_object);
  }

  // Check that no names have been leaked.
  shared_memory.reset();
  EXPECT_EQ(original_name_count, GetActiveNameCount());

  int rv = -1;
  ASSERT_TRUE(child_process_.WaitForExitWithTimeout(
      TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(InsertSameRightTwiceClient) {
  mach_msg_type_number_t original_name_count = 0;
  base::mac::ScopedMachReceiveRight client_port(
      CommonChildProcessSetUp(&original_name_count).release());

  // Receive two memory objects.
  base::mac::ScopedMachReceiveRight inserted_port(
      ReceiveUInt32(client_port.get()));
  base::mac::ScopedMachReceiveRight inserted_port2(
      ReceiveUInt32(client_port.get()));
  base::mac::ScopedMachSendRight memory_object(
      ReceiveMachPort(inserted_port.get()));
  base::mac::ScopedMachSendRight memory_object2(
      ReceiveMachPort(inserted_port2.get()));
  inserted_port.reset();
  inserted_port2.reset();

  // Both rights are for the same Mach port, so only one new name should appear.
  EXPECT_EQ(original_name_count + 1, GetActiveNameCount());

  // Map both memory objects and check their contents.
  std::unique_ptr<base::SharedMemory> shared_memory(MapMemoryObject(
      memory_object.release(),
      AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size));
  char* start = static_cast<char*>(shared_memory->memory());
  for (int i = 0;
       i < AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size; ++i) {
    DCHECK_EQ(start[i], 'a');
  }

  std::unique_ptr<base::SharedMemory> shared_memory2(MapMemoryObject(
      memory_object2.release(),
      AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size));
  char* start2 = static_cast<char*>(shared_memory2->memory());
  for (int i = 0;
       i < AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size; ++i) {
    DCHECK_EQ(start2[i], 'a');
  }

  // Check that the contents of both regions are shared.
  start[0] = 'b';
  DCHECK_EQ(start2[0], 'b');

  // After releasing one shared memory region, the name count shouldn't change,
  // since another reference exists.
  shared_memory.reset();
  EXPECT_EQ(original_name_count + 1, GetActiveNameCount());

  // After releasing the second shared memory region, the name count should be
  // as if no names were ever inserted
  shared_memory2.reset();
  EXPECT_EQ(original_name_count, GetActiveNameCount());

  return 0;
}

// The attachment broker inserts the rights for two memory objects into the
// destination task.
TEST_F(AttachmentBrokerPrivilegedMacMultiProcessTest, InsertTwoRights) {
  SetUpChild("InsertTwoRightsClient");
  mach_msg_type_number_t original_name_count = GetActiveNameCount();
  IPC::AttachmentBrokerPrivilegedMac broker(&port_provider_);

  for (int i = 0; i < 2; ++i) {
    // Create some shared memory.
    std::unique_ptr<base::SharedMemory> shared_memory =
        CreateAndPopulateSharedMemoryHandle(s_memory_size);
    ASSERT_TRUE(shared_memory->handle().IsValid());

    // Insert the memory object into the destination task, via an intermediate
    // port.
    IncrementMachRefCount(shared_memory->handle().GetMemoryObject(),
                          MACH_PORT_RIGHT_SEND);
    mach_port_name_t inserted_memory_object = base::CreateIntermediateMachPort(
        client_task_port_.get(),
        base::mac::ScopedMachSendRight(
            shared_memory->handle().GetMemoryObject()),
        nullptr);
    EXPECT_NE(inserted_memory_object,
              static_cast<mach_port_name_t>(MACH_PORT_NULL));
    SendUInt32(client_port_.get(), inserted_memory_object);
  }

  // Check that no names have been leaked.
  EXPECT_EQ(original_name_count, GetActiveNameCount());

  int rv = -1;
  ASSERT_TRUE(child_process_.WaitForExitWithTimeout(
      TestTimeouts::action_timeout(), &rv));
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(InsertTwoRightsClient) {
  mach_msg_type_number_t original_name_count = 0;
  base::mac::ScopedMachReceiveRight client_port(
      CommonChildProcessSetUp(&original_name_count).release());

  // Receive two memory objects.
  base::mac::ScopedMachReceiveRight inserted_port(
      ReceiveUInt32(client_port.get()));
  base::mac::ScopedMachReceiveRight inserted_port2(
      ReceiveUInt32(client_port.get()));
  base::mac::ScopedMachSendRight memory_object(
      ReceiveMachPort(inserted_port.get()));
  base::mac::ScopedMachSendRight memory_object2(
      ReceiveMachPort(inserted_port2.get()));
  inserted_port.reset();
  inserted_port2.reset();

  // There should be two new names to reflect the two new shared memory regions.
  EXPECT_EQ(original_name_count + 2, GetActiveNameCount());

  // Map both memory objects and check their contents.
  std::unique_ptr<base::SharedMemory> shared_memory(MapMemoryObject(
      memory_object.release(),
      AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size));
  char* start = static_cast<char*>(shared_memory->memory());
  for (int i = 0;
       i < AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size; ++i) {
    DCHECK_EQ(start[i], 'a');
  }

  std::unique_ptr<base::SharedMemory> shared_memory2(MapMemoryObject(
      memory_object2.release(),
      AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size));
  char* start2 = static_cast<char*>(shared_memory2->memory());
  for (int i = 0;
       i < AttachmentBrokerPrivilegedMacMultiProcessTest::s_memory_size; ++i) {
    DCHECK_EQ(start2[i], 'a');
  }

  // Check that the contents of both regions are not shared.
  start[0] = 'b';
  DCHECK_EQ(start2[0], 'a');

  // After releasing one shared memory region, the name count should decrement.
  shared_memory.reset();
  EXPECT_EQ(original_name_count + 1, GetActiveNameCount());
  shared_memory2.reset();
  EXPECT_EQ(original_name_count, GetActiveNameCount());

  return 0;
}

}  //  namespace IPC

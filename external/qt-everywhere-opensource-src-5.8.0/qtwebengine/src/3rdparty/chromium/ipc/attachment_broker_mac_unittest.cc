// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <fcntl.h>
#include <mach/mach_vm.h>
#include <stddef.h>
#include <sys/mman.h>

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/shared_memory.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/spin_wait.h"
#include "base/time/time.h"
#include "ipc/attachment_broker_messages.h"
#include "ipc/attachment_broker_privileged_mac.h"
#include "ipc/attachment_broker_unprivileged_mac.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"
#include "ipc/ipc_test_messages.h"
#include "ipc/test_util_mac.h"

namespace {

const char kDataBuffer1[] = "This is some test data to write to the file.";
const char kDataBuffer2[] = "The lazy dog and a fox.";
const char kDataBuffer3[] = "Two green bears but not a potato.";
const char kDataBuffer4[] = "Red potato is best potato.";
const std::string g_service_switch_name = "service_name";
const size_t g_large_message_size = 8 * 1024 * 1024;
const int g_large_message_count = 1000;
const size_t g_medium_message_size = 512 * 1024;

// Running the message loop is expected to increase the number of resident
// pages. The exact amount is non-deterministic, but for a simple test suite
// like this one, the increase is expected to be less than 1 MB.
const size_t g_expected_memory_increase = 1024 * 1024;

enum TestResult {
  RESULT_UNKNOWN,
  RESULT_SUCCESS,
  RESULT_FAILURE,
};

mach_vm_size_t GetResidentSize() {
  task_basic_info_64 info;
  mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
  kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO_64,
                               reinterpret_cast<task_info_t>(&info), &count);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "Couldn't get resident size.";

  return info.resident_size;
}

base::mac::ScopedMachSendRight GetMachPortFromBrokeredAttachment(
    const scoped_refptr<IPC::BrokerableAttachment>& attachment) {
  if (attachment->GetType() !=
      IPC::BrokerableAttachment::TYPE_BROKERABLE_ATTACHMENT) {
    LOG(INFO) << "Attachment type not TYPE_BROKERABLE_ATTACHMENT.";
    return base::mac::ScopedMachSendRight(MACH_PORT_NULL);
  }

  if (attachment->GetBrokerableType() != IPC::BrokerableAttachment::MACH_PORT) {
    LOG(INFO) << "Brokerable type not MACH_PORT.";
    return base::mac::ScopedMachSendRight(MACH_PORT_NULL);
  }

  IPC::internal::MachPortAttachmentMac* received_mach_port_attachment =
      static_cast<IPC::internal::MachPortAttachmentMac*>(attachment.get());
  base::mac::ScopedMachSendRight send_right(
      received_mach_port_attachment->get_mach_port());
  received_mach_port_attachment->reset_mach_port_ownership();
  return send_right;
}

// Makes a Mach port backed SharedMemory region and fills it with |contents|.
std::unique_ptr<base::SharedMemory> MakeSharedMemory(
    const std::string& contents) {
  base::SharedMemoryHandle shm(contents.size());
  if (!shm.IsValid()) {
    LOG(ERROR) << "Failed to make SharedMemoryHandle.";
    return nullptr;
  }
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(shm, false));
  shared_memory->Map(contents.size());
  memcpy(shared_memory->memory(), contents.c_str(), contents.size());
  return shared_memory;
}

// |message| must be deserializable as a TestSharedMemoryHandleMsg1.
base::SharedMemoryHandle GetSharedMemoryHandleFromMsg1(
    const IPC::Message& message) {
  // Expect a message with a brokered attachment.
  if (!message.HasBrokerableAttachments()) {
    LOG(ERROR) << "Message missing brokerable attachment.";
    return base::SharedMemoryHandle();
  }

  TestSharedMemoryHandleMsg1::Schema::Param p;
  if (!TestSharedMemoryHandleMsg1::Read(&message, &p)) {
    LOG(ERROR) << "Failed to deserialize message.";
    return base::SharedMemoryHandle();
  }

  return std::get<1>(p);
}

// |message| must be deserializable as a TestSharedMemoryHandleMsg2. Returns
// whether deserialization was successful. |handle1| and |handle2| are output
// variables populated on success.
bool GetSharedMemoryHandlesFromMsg2(const IPC::Message& message,
                                    base::SharedMemoryHandle* handle1,
                                    base::SharedMemoryHandle* handle2) {
  // Expect a message with a brokered attachment.
  if (!message.HasBrokerableAttachments()) {
    LOG(ERROR) << "Message missing brokerable attachment.";
    return false;
  }

  TestSharedMemoryHandleMsg2::Schema::Param p;
  if (!TestSharedMemoryHandleMsg2::Read(&message, &p)) {
    LOG(ERROR) << "Failed to deserialize message.";
    return false;
  }

  *handle1 = std::get<0>(p);
  *handle2 = std::get<1>(p);
  return true;
}

// Returns |nullptr| on error.
std::unique_ptr<base::SharedMemory> MapSharedMemoryHandle(
    const base::SharedMemoryHandle& shm,
    bool read_only) {
  if (!shm.IsValid()) {
    LOG(ERROR) << "Invalid SharedMemoryHandle";
    return nullptr;
  }

  size_t size;
  if (!shm.GetSize(&size)) {
    LOG(ERROR) << "Couldn't get size of SharedMemoryHandle";
    return nullptr;
  }

  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(shm, read_only));
  shared_memory->Map(size);
  return shared_memory;
}

// This method maps the SharedMemoryHandle, checks the contents, and then
// consumes a reference to the underlying Mach port.
bool CheckContentsOfSharedMemoryHandle(const base::SharedMemoryHandle& shm,
                                       const std::string& contents) {
  std::unique_ptr<base::SharedMemory> shared_memory(
      MapSharedMemoryHandle(shm, false));

  if (memcmp(shared_memory->memory(), contents.c_str(), contents.size()) != 0) {
    LOG(ERROR) << "Shared Memory contents not equivalent";
    return false;
  }
  return true;
}

// This method mmaps the FileDescriptor, checks the contents, and then munmaps
// the FileDescriptor and closes the underlying fd.
bool CheckContentsOfFileDescriptor(const base::FileDescriptor& file_descriptor,
                                   const std::string& contents) {
  base::ScopedFD fd_closer(file_descriptor.fd);
  lseek(file_descriptor.fd, 0, SEEK_SET);
  std::unique_ptr<char, base::FreeDeleter> buffer(
      static_cast<char*>(malloc(contents.size())));
  if (!base::ReadFromFD(file_descriptor.fd, buffer.get(), contents.size()))
    return false;

  int result = memcmp(buffer.get(), contents.c_str(), contents.size());
  return result == 0;
}

// Open |fp| and populate it with |contents|.
base::FileDescriptor MakeFileDescriptor(const base::FilePath& fp,
                                        const std::string& contents) {
  int fd = open(fp.value().c_str(), O_RDWR, S_IWUSR | S_IRUSR);
  base::ScopedFD fd_closer(fd);
  if (fd <= 0) {
    LOG(ERROR) << "Error opening file at: " << fp.value();
    return base::FileDescriptor();
  }

  if (lseek(fd, 0, SEEK_SET) != 0) {
    LOG(ERROR) << "Error changing offset";
    return base::FileDescriptor();
  }

  if (write(fd, contents.c_str(), contents.size()) !=
      static_cast<ssize_t>(contents.size())) {
    LOG(ERROR) << "Error writing to file";
    return base::FileDescriptor();
  }

  return base::FileDescriptor(fd_closer.release(), true);
}

// Maps both handles, then checks that their contents matches |contents|. Then
// checks that changes to one are reflected in the other. Then consumes
// references to both underlying Mach ports.
bool CheckContentsOfTwoEquivalentSharedMemoryHandles(
    const base::SharedMemoryHandle& handle1,
    const base::SharedMemoryHandle& handle2,
    const std::string& contents) {
  std::unique_ptr<base::SharedMemory> shared_memory1(
      MapSharedMemoryHandle(handle1, false));
  std::unique_ptr<base::SharedMemory> shared_memory2(
      MapSharedMemoryHandle(handle2, false));

  if (memcmp(shared_memory1->memory(), contents.c_str(), contents.size()) !=
      0) {
    LOG(ERROR) << "Incorrect contents in shared_memory1";
    return false;
  }

  if (memcmp(shared_memory1->memory(), shared_memory2->memory(),
             contents.size()) != 0) {
    LOG(ERROR) << "Incorrect contents in shared_memory2";
    return false;
  }

  // Updating shared_memory1 should update shared_memory2.
  const char known_string[] = "string bean";
  if (shared_memory1->mapped_size() < strlen(known_string) ||
      shared_memory2->mapped_size() < strlen(known_string)) {
    LOG(ERROR) << "Shared memory size is too small";
    return false;
  }
  memcpy(shared_memory1->memory(), known_string, strlen(known_string));

  if (memcmp(shared_memory1->memory(), shared_memory2->memory(),
             strlen(known_string)) != 0) {
    LOG(ERROR) << "Incorrect contents in shared_memory2";
    return false;
  }

  return true;
}

// |message| must be deserializable as a TestSharedMemoryHandleMsg1. Returns
// whether the contents of the attached shared memory region matches |contents|.
// Consumes a reference to the underlying Mach port.
bool CheckContentsOfMessage1(const IPC::Message& message,
                             const std::string& contents) {
  base::SharedMemoryHandle shm(GetSharedMemoryHandleFromMsg1(message));
  return CheckContentsOfSharedMemoryHandle(shm, contents);
}

// Once the test is finished, send a control message to the parent process with
// the result. The message may require the runloop to be run before its
// dispatched.
void SendControlMessage(IPC::Sender* sender, bool success) {
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  TestResult result = success ? RESULT_SUCCESS : RESULT_FAILURE;
  message->WriteInt(result);
  sender->Send(message);
}

// Records the most recently received brokerable attachment's id.
class AttachmentBrokerObserver : public IPC::AttachmentBroker::Observer {
 public:
  void ReceivedBrokerableAttachmentWithId(
      const IPC::BrokerableAttachment::AttachmentId& id) override {
    id_ = id;
  }
  IPC::BrokerableAttachment::AttachmentId* get_id() { return &id_; }

 private:
  IPC::BrokerableAttachment::AttachmentId id_;
};

// A broker which always sets the current process as the destination process
// for attachments.
class MockBroker : public IPC::AttachmentBrokerUnprivilegedMac {
 public:
  MockBroker() {}
  ~MockBroker() override {}
  bool SendAttachmentToProcess(
      const scoped_refptr<IPC::BrokerableAttachment>& attachment,
      base::ProcessId destination_process) override {
    return IPC::AttachmentBrokerUnprivilegedMac::SendAttachmentToProcess(
        attachment, base::Process::Current().Pid());
  }
};

// Forwards all messages to |listener_|.  Quits the message loop after a
// message is received, or the channel has an error.
class ProxyListener : public IPC::Listener {
 public:
  ProxyListener() : listener_(nullptr), reason_(MESSAGE_RECEIVED) {}
  ~ProxyListener() override {}

  // The reason for exiting the message loop.
  enum Reason { MESSAGE_RECEIVED, CHANNEL_ERROR };

  bool OnMessageReceived(const IPC::Message& message) override {
    bool result = false;
    if (listener_)
      result = listener_->OnMessageReceived(message);
    reason_ = MESSAGE_RECEIVED;
    messages_.push_back(message);
    base::MessageLoop::current()->QuitNow();
    return result;
  }

  void OnChannelError() override {
    reason_ = CHANNEL_ERROR;
    base::MessageLoop::current()->QuitNow();
  }

  void set_listener(IPC::Listener* listener) { listener_ = listener; }
  Reason get_reason() { return reason_; }
  IPC::Message get_first_message() {
    DCHECK(!messages_.empty());
    return messages_[0];
  }
  void pop_first_message() {
    DCHECK(!messages_.empty());
    messages_.erase(messages_.begin());
  }
  bool has_message() { return !messages_.empty(); }

 private:
  IPC::Listener* listener_;
  Reason reason_;
  std::vector<IPC::Message> messages_;
};

// Waits for a result to be sent over the channel. Quits the message loop
// after a message is received, or the channel has an error.
class ResultListener : public IPC::Listener {
 public:
  ResultListener() : result_(RESULT_UNKNOWN) {}
  ~ResultListener() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);

    int result;
    EXPECT_TRUE(iter.ReadInt(&result));
    result_ = static_cast<TestResult>(result);
    return true;
  }

  TestResult get_result() { return result_; }

 private:
  TestResult result_;
};

class MockPortProvider : public base::PortProvider {
 public:
  mach_port_t TaskForPid(base::ProcessHandle process) const override {
    auto it = port_map_.find(process);
    if (it != port_map_.end())
      return it->second;
    return MACH_PORT_NULL;
  }

  void InsertEntry(base::ProcessHandle process, mach_port_t task_port) {
    port_map_[process] = task_port;
    NotifyObservers(process);
  }

  void ClearPortMap() { port_map_.clear(); }

 private:
  std::map<base::ProcessHandle, mach_port_t> port_map_;
};

// End-to-end tests for the attachment brokering process on Mac.
// The parent process acts as an unprivileged process. The child process acts
// as the privileged process.
class IPCAttachmentBrokerMacTest : public IPCTestBase {
 public:
  IPCAttachmentBrokerMacTest() {}
  ~IPCAttachmentBrokerMacTest() override {}

  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine command_line = IPCTestBase::MakeCmdLine(procname);
    // Pass the service name to the child process.
    command_line.AppendSwitchASCII(g_service_switch_name, service_name_);
    return command_line;
  }

  // Takes ownership of |broker|. Has no effect if called after CommonSetUp().
  void SetBroker(IPC::AttachmentBrokerUnprivilegedMac* broker) {
    broker_.reset(broker);
  }

  // Mach Setup that needs to occur before child processes are forked.
  void MachPreForkSetUp() {
    service_name_ = IPC::CreateRandomServiceName();
    server_port_.reset(IPC::BecomeMachServer(service_name_.c_str()).release());
  }

  // Mach Setup that needs to occur after child processes are forked.
  void MachPostForkSetUp() {
    client_port_.reset(IPC::ReceiveMachPort(server_port_.get()).release());
    IPC::SendMachPort(
        client_port_.get(), mach_task_self(), MACH_MSG_TYPE_COPY_SEND);
  }

  // Setup shared between tests.
  void CommonSetUp(const char* name) {
    PreConnectSetUp(name);
    PostConnectSetUp();
  }

  // All of setup before the channel is connected.
  void PreConnectSetUp(const char* name) {
    Init(name);
    MachPreForkSetUp();

    if (!broker_.get())
      SetBroker(new IPC::AttachmentBrokerUnprivilegedMac);

    broker_->AddObserver(&observer_, task_runner());
    CreateChannel(&proxy_listener_);
    broker_->RegisterBrokerCommunicationChannel(channel());
  }

  // All of setup including the connection and everything after.
  void PostConnectSetUp() {
    ASSERT_TRUE(ConnectChannel());
    ASSERT_TRUE(StartClient());

    MachPostForkSetUp();
    active_names_at_start_ = IPC::GetActiveNameCount();
    get_proxy_listener()->set_listener(&result_listener_);
  }

  void CheckChildResult() {
    ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
              get_proxy_listener()->get_reason());
    ASSERT_EQ(get_result_listener()->get_result(), RESULT_SUCCESS);
  }

  void FinalCleanUp() {
    // There should be no leaked names.
    SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(
        base::TimeDelta::FromSeconds(10),
        active_names_at_start_ == IPC::GetActiveNameCount());
    EXPECT_EQ(active_names_at_start_, IPC::GetActiveNameCount());

    // Close the channel so the client's OnChannelError() gets fired.
    channel()->Close();

    EXPECT_TRUE(WaitForClientShutdown());
    DestroyChannel();
    broker_.reset();
  }

  // Teardown shared between most tests.
  void CommonTearDown() {
    CheckChildResult();
    FinalCleanUp();
  }

  // Makes a SharedMemory region, fills it with |contents|, sends the handle
  // over Chrome IPC, and unmaps the region.
  void SendMessage1(const std::string& contents) {
    std::unique_ptr<base::SharedMemory> shared_memory(
        MakeSharedMemory(contents));
    IPC::Message* message =
        new TestSharedMemoryHandleMsg1(100, shared_memory->handle(), 200);
    sender()->Send(message);
  }

  ProxyListener* get_proxy_listener() { return &proxy_listener_; }
  IPC::AttachmentBrokerUnprivilegedMac* get_broker() { return broker_.get(); }
  AttachmentBrokerObserver* get_observer() { return &observer_; }
  ResultListener* get_result_listener() { return &result_listener_; }

 protected:
  // The number of active names immediately after set up.
  mach_msg_type_number_t active_names_at_start_;

 private:
  ProxyListener proxy_listener_;
  std::unique_ptr<IPC::AttachmentBrokerUnprivilegedMac> broker_;
  AttachmentBrokerObserver observer_;

  // A port on which the main process listens for mach messages from the child
  // process.
  base::mac::ScopedMachReceiveRight server_port_;

  // A port on which the child process listens for mach messages from the main
  // process.
  base::mac::ScopedMachSendRight client_port_;

  std::string service_name_;

  ResultListener result_listener_;
};

// These objects are globally accessible, and are expected to outlive all IPC
// Channels.
struct ChildProcessGlobals {
  MockPortProvider port_provider;

  // The broker must be destroyed before the port_provider, so that the broker
  // gets a chance to unregister itself as an observer. This doesn't matter
  // outside of tests, since neither port_provider nor broker will ever be
  // destroyed.
  std::unique_ptr<IPC::AttachmentBrokerPrivilegedMac> broker;
  base::mac::ScopedMachSendRight server_task_port;

  // Total resident memory before running the message loop.
  mach_vm_size_t initial_resident_size;

  // Whether to emit log statements while processing messages.
  bool message_logging;
};

using OnMessageReceivedCallback = void (*)(IPC::Sender* sender,
                                           const IPC::Message& message,
                                           ChildProcessGlobals* globals);

// Sets up the Mach communication ports with the server. Returns a set of
// globals that must live at least as long as the test.
std::unique_ptr<ChildProcessGlobals> CommonChildProcessSetUp() {
  base::CommandLine cmd_line = *base::CommandLine::ForCurrentProcess();
  std::string service_name =
      cmd_line.GetSwitchValueASCII(g_service_switch_name);
  base::mac::ScopedMachSendRight server_port(
      IPC::LookupServer(service_name.c_str()));
  base::mac::ScopedMachReceiveRight client_port(IPC::MakeReceivingPort());

  // Send the port that this process is listening on to the server.
  IPC::SendMachPort(
      server_port.get(), client_port.get(), MACH_MSG_TYPE_MAKE_SEND);

  // Receive the task port of the server process.
  base::mac::ScopedMachSendRight server_task_port(
      IPC::ReceiveMachPort(client_port.get()));

  std::unique_ptr<ChildProcessGlobals> globals(new ChildProcessGlobals);
  globals->broker.reset(
      new IPC::AttachmentBrokerPrivilegedMac(&globals->port_provider));
  globals->port_provider.InsertEntry(getppid(), server_task_port.get());
  globals->server_task_port.reset(server_task_port.release());
  globals->message_logging = true;
  return globals;
}

int CommonPrivilegedProcessMain(OnMessageReceivedCallback callback,
                                const char* channel_name) {
  LOG(INFO) << "Privileged process start.";
  std::unique_ptr<ChildProcessGlobals> globals(CommonChildProcessSetUp());

  mach_msg_type_number_t active_names_at_start = IPC::GetActiveNameCount();

  base::MessageLoopForIO main_message_loop;
  ProxyListener listener;

  std::unique_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName(channel_name), &listener));
  globals->broker->RegisterCommunicationChannel(channel.get(), nullptr);
  CHECK(channel->Connect());

  globals->initial_resident_size = GetResidentSize();

  while (true) {
    if (globals->message_logging)
      LOG(INFO) << "Privileged process spinning run loop.";
    base::RunLoop().Run();
    ProxyListener::Reason reason = listener.get_reason();
    if (reason == ProxyListener::CHANNEL_ERROR)
      break;

    while (listener.has_message()) {
      if (globals->message_logging)
        LOG(INFO) << "Privileged process running callback.";
      callback(channel.get(), listener.get_first_message(), globals.get());
      if (globals->message_logging)
        LOG(INFO) << "Privileged process finishing callback.";
      listener.pop_first_message();
    }
  }

  if (active_names_at_start != IPC::GetActiveNameCount()) {
    LOG(INFO) << "Memory leak!.";
  }
  LOG(INFO) << "Privileged process end.";
  return 0;
}

// An unprivileged process makes a shared memory region, and writes a string to
// it. The SharedMemoryHandle is sent to the privileged process using Chrome
// IPC. The privileged process checks that it received the same memory region.
TEST_F(IPCAttachmentBrokerMacTest, SendSharedMemoryHandle) {
  CommonSetUp("SendSharedMemoryHandle");

  SendMessage1(kDataBuffer1);
  base::RunLoop().Run();
  CommonTearDown();
}

void SendSharedMemoryHandleCallback(IPC::Sender* sender,
                                    const IPC::Message& message,
                                    ChildProcessGlobals* globals) {
  bool success = CheckContentsOfMessage1(message, kDataBuffer1);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendSharedMemoryHandle) {
  return CommonPrivilegedProcessMain(&SendSharedMemoryHandleCallback,
                                     "SendSharedMemoryHandle");
}

// Similar to SendSharedMemoryHandle, but sends a very long shared memory
// region.
TEST_F(IPCAttachmentBrokerMacTest, SendSharedMemoryHandleLong) {
  CommonSetUp("SendSharedMemoryHandleLong");

  std::string buffer(1 << 23, 'a');
  SendMessage1(buffer);
  base::RunLoop().Run();
  CommonTearDown();
}

void SendSharedMemoryHandleLongCallback(IPC::Sender* sender,
                                        const IPC::Message& message,
                                        ChildProcessGlobals* globals) {
  std::string buffer(1 << 23, 'a');
  bool success = CheckContentsOfMessage1(message, buffer);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendSharedMemoryHandleLong) {
  return CommonPrivilegedProcessMain(&SendSharedMemoryHandleLongCallback,
                                     "SendSharedMemoryHandleLong");
}

// Similar to SendSharedMemoryHandle, but sends two different shared memory
// regions in two messages.
TEST_F(IPCAttachmentBrokerMacTest, SendTwoMessagesDifferentSharedMemoryHandle) {
  CommonSetUp("SendTwoMessagesDifferentSharedMemoryHandle");

  SendMessage1(kDataBuffer1);
  SendMessage1(kDataBuffer2);
  base::RunLoop().Run();
  CommonTearDown();
}

void SendTwoMessagesDifferentSharedMemoryHandleCallback(
    IPC::Sender* sender,
    const IPC::Message& message,
    ChildProcessGlobals* globals) {
  static int count = 0;
  static bool success = true;
  ++count;
  if (count == 1) {
    success &= CheckContentsOfMessage1(message, kDataBuffer1);
  } else if (count == 2) {
    success &= CheckContentsOfMessage1(message, kDataBuffer2);
    SendControlMessage(sender, success);
  }
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendTwoMessagesDifferentSharedMemoryHandle) {
  return CommonPrivilegedProcessMain(
      &SendTwoMessagesDifferentSharedMemoryHandleCallback,
      "SendTwoMessagesDifferentSharedMemoryHandle");
}

// Similar to SendSharedMemoryHandle, but sends the same shared memory region in
// two messages.
TEST_F(IPCAttachmentBrokerMacTest, SendTwoMessagesSameSharedMemoryHandle) {
  CommonSetUp("SendTwoMessagesSameSharedMemoryHandle");

  {
    std::unique_ptr<base::SharedMemory> shared_memory(
        MakeSharedMemory(kDataBuffer1));

    for (int i = 0; i < 2; ++i) {
      IPC::Message* message =
          new TestSharedMemoryHandleMsg1(100, shared_memory->handle(), 200);
      sender()->Send(message);
    }
  }

  base::RunLoop().Run();
  CommonTearDown();
}

void SendTwoMessagesSameSharedMemoryHandleCallback(
    IPC::Sender* sender,
    const IPC::Message& message,
    ChildProcessGlobals* globals) {
  static int count = 0;
  static base::SharedMemoryHandle handle1;
  ++count;

  if (count == 1) {
    handle1 = GetSharedMemoryHandleFromMsg1(message);
  } else if (count == 2) {
    base::SharedMemoryHandle handle2(GetSharedMemoryHandleFromMsg1(message));

    bool success = CheckContentsOfTwoEquivalentSharedMemoryHandles(
        handle1, handle2, kDataBuffer1);
    SendControlMessage(sender, success);
  }
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendTwoMessagesSameSharedMemoryHandle) {
  return CommonPrivilegedProcessMain(
      &SendTwoMessagesSameSharedMemoryHandleCallback,
      "SendTwoMessagesSameSharedMemoryHandle");
}

// Similar to SendSharedMemoryHandle, but sends one message with two different
// memory regions.
TEST_F(IPCAttachmentBrokerMacTest,
       SendOneMessageWithTwoDifferentSharedMemoryHandles) {
  CommonSetUp("SendOneMessageWithTwoDifferentSharedMemoryHandles");

  {
    std::unique_ptr<base::SharedMemory> shared_memory1(
        MakeSharedMemory(kDataBuffer1));
    std::unique_ptr<base::SharedMemory> shared_memory2(
        MakeSharedMemory(kDataBuffer2));
    IPC::Message* message = new TestSharedMemoryHandleMsg2(
        shared_memory1->handle(), shared_memory2->handle());
    sender()->Send(message);
  }
  base::RunLoop().Run();
  CommonTearDown();
}

void SendOneMessageWithTwoDifferentSharedMemoryHandlesCallback(
    IPC::Sender* sender,
    const IPC::Message& message,
    ChildProcessGlobals* globals) {
  base::SharedMemoryHandle handle1;
  base::SharedMemoryHandle handle2;
  if (!GetSharedMemoryHandlesFromMsg2(message, &handle1, &handle2)) {
    LOG(ERROR) << "Failed to deserialize message.";
    SendControlMessage(sender, false);
    return;
  }

  bool success = CheckContentsOfSharedMemoryHandle(handle1, kDataBuffer1) &&
                 CheckContentsOfSharedMemoryHandle(handle2, kDataBuffer2);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(
    SendOneMessageWithTwoDifferentSharedMemoryHandles) {
  return CommonPrivilegedProcessMain(
      &SendOneMessageWithTwoDifferentSharedMemoryHandlesCallback,
      "SendOneMessageWithTwoDifferentSharedMemoryHandles");
}

// Similar to SendSharedMemoryHandle, but sends one message that contains the
// same memory region twice.
TEST_F(IPCAttachmentBrokerMacTest,
       SendOneMessageWithTwoSameSharedMemoryHandles) {
  CommonSetUp("SendOneMessageWithTwoSameSharedMemoryHandles");

  {
    std::unique_ptr<base::SharedMemory> shared_memory(
        MakeSharedMemory(kDataBuffer1));
    IPC::Message* message = new TestSharedMemoryHandleMsg2(
        shared_memory->handle(), shared_memory->handle());
    sender()->Send(message);
  }
  base::RunLoop().Run();
  CommonTearDown();
}

void SendOneMessageWithTwoSameSharedMemoryHandlesCallback(
    IPC::Sender* sender,
    const IPC::Message& message,
    ChildProcessGlobals* globals) {
  base::SharedMemoryHandle handle1;
  base::SharedMemoryHandle handle2;
  if (!GetSharedMemoryHandlesFromMsg2(message, &handle1, &handle2)) {
    LOG(ERROR) << "Failed to deserialize message.";
    SendControlMessage(sender, false);
    return;
  }

  bool success = CheckContentsOfTwoEquivalentSharedMemoryHandles(
      handle1, handle2, kDataBuffer1);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(
    SendOneMessageWithTwoSameSharedMemoryHandles) {
  return CommonPrivilegedProcessMain(
      &SendOneMessageWithTwoSameSharedMemoryHandlesCallback,
      "SendOneMessageWithTwoSameSharedMemoryHandles");
}

// Sends one message with two Posix FDs and two Mach ports.
TEST_F(IPCAttachmentBrokerMacTest, SendPosixFDAndMachPort) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath fp1, fp2;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &fp1));
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &fp2));

  CommonSetUp("SendPosixFDAndMachPort");

  {
    std::unique_ptr<base::SharedMemory> shared_memory1(
        MakeSharedMemory(kDataBuffer1));
    std::unique_ptr<base::SharedMemory> shared_memory2(
        MakeSharedMemory(kDataBuffer2));

    base::FileDescriptor file_descriptor1(
        MakeFileDescriptor(fp1, kDataBuffer3));
    base::FileDescriptor file_descriptor2(
        MakeFileDescriptor(fp2, kDataBuffer4));

    IPC::Message* message = new TestSharedMemoryHandleMsg3(
        file_descriptor1, shared_memory1->handle(), file_descriptor2,
        shared_memory2->handle());
    sender()->Send(message);
  }

  base::RunLoop().Run();
  CommonTearDown();
}

void SendPosixFDAndMachPortCallback(IPC::Sender* sender,
                                    const IPC::Message& message,
                                    ChildProcessGlobals* globals) {
  TestSharedMemoryHandleMsg3::Schema::Param p;
  if (!TestSharedMemoryHandleMsg3::Read(&message, &p)) {
    LOG(ERROR) << "Failed to deserialize message.";
    SendControlMessage(sender, false);
    return;
  }

  base::SharedMemoryHandle handle1 = std::get<1>(p);
  base::SharedMemoryHandle handle2 = std::get<3>(p);
  bool success1 = CheckContentsOfSharedMemoryHandle(handle1, kDataBuffer1) &&
                  CheckContentsOfSharedMemoryHandle(handle2, kDataBuffer2);
  if (!success1)
    LOG(ERROR) << "SharedMemoryHandles have wrong contents.";

  bool success2 =
      CheckContentsOfFileDescriptor(std::get<0>(p), kDataBuffer3) &&
      CheckContentsOfFileDescriptor(std::get<2>(p), kDataBuffer4);
  if (!success2)
    LOG(ERROR) << "FileDescriptors have wrong contents.";

  SendControlMessage(sender, success1 && success2);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendPosixFDAndMachPort) {
  return CommonPrivilegedProcessMain(&SendPosixFDAndMachPortCallback,
                                     "SendPosixFDAndMachPort");
}

// Similar to SendHandle, except the attachment's destination process is this
// process. This is an unrealistic scenario, but simulates an unprivileged
// process sending an attachment to another unprivileged process.
TEST_F(IPCAttachmentBrokerMacTest, SendSharedMemoryHandleToSelf) {
  SetBroker(new MockBroker);
  PreConnectSetUp("SendSharedMemoryHandleToSelf");
  // Technically, the channel is an endpoint, but we need the proxy listener to
  // receive the messages so that it can quit the message loop.
  channel()->SetAttachmentBrokerEndpoint(false);
  PostConnectSetUp();
  get_proxy_listener()->set_listener(get_broker());

  {
    std::unique_ptr<base::SharedMemory> shared_memory(
        MakeSharedMemory(kDataBuffer1));
    mach_port_urefs_t ref_count = IPC::GetMachRefCount(
        shared_memory->handle().GetMemoryObject(), MACH_PORT_RIGHT_SEND);

    IPC::Message* message =
        new TestSharedMemoryHandleMsg1(100, shared_memory->handle(), 200);
    sender()->Send(message);

    // Wait until the child process has sent this process a message.
    base::RunLoop().Run();

    // Wait for any asynchronous activity to complete.
    base::RunLoop().RunUntilIdle();

    // Get the received attachment.
    IPC::BrokerableAttachment::AttachmentId* id = get_observer()->get_id();
    ASSERT_TRUE(id);
    scoped_refptr<IPC::BrokerableAttachment> received_attachment;
    get_broker()->GetAttachmentWithId(*id, &received_attachment);
    ASSERT_NE(received_attachment.get(), nullptr);

    // Check that it's has the same name, but that the ref count has increased.
    base::mac::ScopedMachSendRight memory_object(
        GetMachPortFromBrokeredAttachment(received_attachment));
    ASSERT_EQ(memory_object, shared_memory->handle().GetMemoryObject());
    EXPECT_EQ(ref_count + 1,
              IPC::GetMachRefCount(shared_memory->handle().GetMemoryObject(),
                                   MACH_PORT_RIGHT_SEND));
  }

  FinalCleanUp();
}

void SendSharedMemoryHandleToSelfCallback(IPC::Sender* sender,
                                          const IPC::Message&,
                                          ChildProcessGlobals* globals) {
  // Do nothing special. The default behavior already runs the
  // AttachmentBrokerPrivilegedMac.
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendSharedMemoryHandleToSelf) {
  return CommonPrivilegedProcessMain(&SendSharedMemoryHandleToSelfCallback,
                                     "SendSharedMemoryHandleToSelf");
}

// Similar to SendSharedMemoryHandle, but uses a ChannelProxy instead of a
// Channel.
TEST_F(IPCAttachmentBrokerMacTest, SendSharedMemoryHandleChannelProxy) {
  Init("SendSharedMemoryHandleChannelProxy");
  MachPreForkSetUp();

  SetBroker(new IPC::AttachmentBrokerUnprivilegedMac);
  get_broker()->AddObserver(get_observer(), task_runner());

  std::unique_ptr<base::Thread> thread(
      new base::Thread("ChannelProxyTestServerThread"));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread->StartWithOptions(options);

  set_channel_proxy(std::unique_ptr<IPC::ChannelProxy>(new IPC::ChannelProxy(
      get_proxy_listener(), thread->task_runner().get())));
  get_broker()->RegisterBrokerCommunicationChannel(channel_proxy());
  channel_proxy()->Init(
      CreateChannelFactory(GetTestChannelHandle(), thread->task_runner().get()),
      true);

  ASSERT_TRUE(StartClient());

  MachPostForkSetUp();
  active_names_at_start_ = IPC::GetActiveNameCount();
  get_proxy_listener()->set_listener(get_result_listener());

  SendMessage1(kDataBuffer1);
  base::RunLoop().Run();

  CheckChildResult();

  // There should be no leaked names.
  EXPECT_EQ(active_names_at_start_, IPC::GetActiveNameCount());

  // Close the channel so the client's OnChannelError() gets fired.
  channel_proxy()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannelProxy();
}

void SendSharedMemoryHandleChannelProxyCallback(IPC::Sender* sender,
                                                const IPC::Message& message,
                                                ChildProcessGlobals* globals) {
  bool success = CheckContentsOfMessage1(message, kDataBuffer1);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendSharedMemoryHandleChannelProxy) {
  return CommonPrivilegedProcessMain(
      &SendSharedMemoryHandleChannelProxyCallback,
      "SendSharedMemoryHandleChannelProxy");
}

// Similar to SendSharedMemoryHandle, but first makes a copy of the handle using
// ShareToProcess().
TEST_F(IPCAttachmentBrokerMacTest, ShareToProcess) {
  CommonSetUp("ShareToProcess");

  {
    std::unique_ptr<base::SharedMemory> shared_memory(
        MakeSharedMemory(kDataBuffer1));
    base::SharedMemoryHandle new_handle;
    ASSERT_TRUE(shared_memory->ShareToProcess(0, &new_handle));
    IPC::Message* message =
        new TestSharedMemoryHandleMsg1(100, new_handle, 200);
    sender()->Send(message);
  }

  base::RunLoop().Run();
  CommonTearDown();
}

void ShareToProcessCallback(IPC::Sender* sender,
                            const IPC::Message& message,
                            ChildProcessGlobals* globals) {
  bool success = CheckContentsOfMessage1(message, kDataBuffer1);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(ShareToProcess) {
  return CommonPrivilegedProcessMain(&ShareToProcessCallback, "ShareToProcess");
}

// Similar to ShareToProcess, but instead shares the memory object only with
// read permissions.
TEST_F(IPCAttachmentBrokerMacTest, ShareReadOnlyToProcess) {
  CommonSetUp("ShareReadOnlyToProcess");

  {
    std::unique_ptr<base::SharedMemory> shared_memory(
        MakeSharedMemory(kDataBuffer1));
    base::SharedMemoryHandle new_handle;
    ASSERT_TRUE(shared_memory->ShareReadOnlyToProcess(0, &new_handle));
    IPC::Message* message =
        new TestSharedMemoryHandleMsg1(100, new_handle, 200);
    sender()->Send(message);
  }

  base::RunLoop().Run();
  CommonTearDown();
}

void ShareReadOnlyToProcessCallback(IPC::Sender* sender,
                                    const IPC::Message& message,
                                    ChildProcessGlobals* globals) {
  base::SharedMemoryHandle shm(GetSharedMemoryHandleFromMsg1(message));

  // Try to map the memory as writable.
  std::unique_ptr<base::SharedMemory> shared_memory(
      MapSharedMemoryHandle(shm, false));
  ASSERT_EQ(nullptr, shared_memory->memory());

  // Now try as read-only.
  std::unique_ptr<base::SharedMemory> shared_memory2(
      MapSharedMemoryHandle(shm.Duplicate(), true));
  int current_prot, max_prot;
  ASSERT_TRUE(IPC::GetMachProtections(shared_memory2->memory(),
                                      shared_memory2->mapped_size(),
                                      &current_prot, &max_prot));
  ASSERT_EQ(VM_PROT_READ, current_prot);
  ASSERT_EQ(VM_PROT_READ, max_prot);

  bool success =
      memcmp(shared_memory2->memory(), kDataBuffer1, strlen(kDataBuffer1)) == 0;
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(ShareReadOnlyToProcess) {
  return CommonPrivilegedProcessMain(&ShareReadOnlyToProcessCallback,
                                     "ShareReadOnlyToProcess");
}

// Similar to SendSharedMemoryHandleToSelf, but the child process pretends to
// not have the task port for the parent process.
TEST_F(IPCAttachmentBrokerMacTest, SendSharedMemoryHandleToSelfDelayedPort) {
  SetBroker(new MockBroker);
  PreConnectSetUp("SendSharedMemoryHandleToSelfDelayedPort");
  // Technically, the channel is an endpoint, but we need the proxy listener to
  // receive the messages so that it can quit the message loop.
  channel()->SetAttachmentBrokerEndpoint(false);
  PostConnectSetUp();
  get_proxy_listener()->set_listener(get_broker());

  {
    std::unique_ptr<base::SharedMemory> shared_memory(
        MakeSharedMemory(kDataBuffer1));
    mach_port_urefs_t ref_count = IPC::GetMachRefCount(
        shared_memory->handle().GetMemoryObject(), MACH_PORT_RIGHT_SEND);

    std::vector<IPC::BrokerableAttachment::AttachmentId> ids;
    const int kMessagesToTest = 3;
    for (int i = 0; i < kMessagesToTest; ++i) {
      base::SharedMemoryHandle h = shared_memory->handle().Duplicate();
      ids.push_back(
          IPC::BrokerableAttachment::AttachmentId::CreateIdWithRandomNonce());
      IPC::internal::MachPortAttachmentMac::WireFormat wire_format(
          h.GetMemoryObject(), getpid(), ids[i]);
      sender()->Send(new AttachmentBrokerMsg_DuplicateMachPort(wire_format));

      // Send a dummy message, which will trigger the callback handler in the
      // child process.
      sender()->Send(new TestSharedMemoryHandleMsg4(1));
    }

    int received_message_count = 0;
    while (received_message_count < kMessagesToTest) {
      // Wait until the child process has sent this process a message.
      base::RunLoop().Run();

      // Wait for any asynchronous activity to complete.
      base::RunLoop().RunUntilIdle();

      while (get_proxy_listener()->has_message()) {
        get_proxy_listener()->pop_first_message();
        received_message_count++;
      }
    }

    for (int i = 0; i < kMessagesToTest; ++i) {
      IPC::BrokerableAttachment::AttachmentId* id = &ids[i];
      ASSERT_TRUE(id);
      scoped_refptr<IPC::BrokerableAttachment> received_attachment;
      get_broker()->GetAttachmentWithId(*id, &received_attachment);
      ASSERT_NE(received_attachment.get(), nullptr);

      base::mac::ScopedMachSendRight memory_object(
          GetMachPortFromBrokeredAttachment(received_attachment));
      ASSERT_EQ(shared_memory->handle().GetMemoryObject(), memory_object);
    }

    // Check that the ref count hasn't changed.
    EXPECT_EQ(ref_count,
              IPC::GetMachRefCount(shared_memory->handle().GetMemoryObject(),
                                   MACH_PORT_RIGHT_SEND));
  }

  FinalCleanUp();
}

void SendSharedMemoryHandleToSelfDelayedPortCallback(
    IPC::Sender* sender,
    const IPC::Message& message,
    ChildProcessGlobals* globals) {
  static int i = 0;
  static base::ProcessId pid = message.get_sender_pid();
  static mach_port_t task_port = globals->port_provider.TaskForPid(pid);
  ++i;

  if (i == 1) {
    // Pretend to not have the task port for the parent.
    globals->port_provider.ClearPortMap();
  } else if (i == 2) {
    // Intentionally do nothing.
  } else if (i == 3) {
    // Setting the task port should trigger callbacks, eventually resulting in
    // multiple attachment broker messages.
    globals->port_provider.InsertEntry(pid, task_port);
  }
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendSharedMemoryHandleToSelfDelayedPort) {
  return CommonPrivilegedProcessMain(
      &SendSharedMemoryHandleToSelfDelayedPortCallback,
      "SendSharedMemoryHandleToSelfDelayedPort");
}

// Tests the memory usage characteristics of attachment brokering a single large
// message. This test has the *potential* to be flaky, since it compares
// resident memory at different points in time, and that measurement is
// non-deterministic.
TEST_F(IPCAttachmentBrokerMacTest, MemoryUsageLargeMessage) {
  CommonSetUp("MemoryUsageLargeMessage");

  std::string test_string(g_large_message_size, 'a');
  SendMessage1(test_string);
  base::RunLoop().Run();
  CommonTearDown();
}

void MemoryUsageLargeMessageCallback(IPC::Sender* sender,
                                     const IPC::Message& message,
                                     ChildProcessGlobals* globals) {
  EXPECT_LE(GetResidentSize(),
            globals->initial_resident_size + g_expected_memory_increase);

  base::SharedMemoryHandle shm(GetSharedMemoryHandleFromMsg1(message));
  std::unique_ptr<base::SharedMemory> shared_memory(
      MapSharedMemoryHandle(shm, false));
  EXPECT_LE(GetResidentSize(),
            globals->initial_resident_size + g_expected_memory_increase);

  char* addr = static_cast<char*>(shared_memory->memory());
  for (size_t i = 0; i < g_large_message_size; i += 1024) {
    addr[i] = 'a';
  }
  EXPECT_GE(GetResidentSize(),
            globals->initial_resident_size + g_large_message_size);

  shared_memory.reset();
#if !defined(ADDRESS_SANITIZER) && !defined(LEAK_SANITIZER) &&  \
    !defined(MEMORY_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(UNDEFINED_SANITIZER)
  // Under a sanitizer build, releasing memory does not necessarily reduce the
  // amount of resident memory.
  EXPECT_LE(GetResidentSize(),
            globals->initial_resident_size + g_expected_memory_increase);
#endif

  SendControlMessage(sender, true);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(MemoryUsageLargeMessage) {
  return CommonPrivilegedProcessMain(&MemoryUsageLargeMessageCallback,
                                     "MemoryUsageLargeMessage");
}

// Tests the memory usage characteristics of attachment brokering many small
// messages. This test has the *potential* to be flaky, since it compares
// resident memory at different points in time, and that measurement is
// non-deterministic.
TEST_F(IPCAttachmentBrokerMacTest, MemoryUsageManyMessages) {
  CommonSetUp("MemoryUsageManyMessages");

  for (int i = 0; i < g_large_message_count; ++i) {
    std::string message = base::IntToString(i);
    message += '\0';
    size_t end = message.size();
    message.resize(g_medium_message_size);
    std::fill(message.begin() + end, message.end(), 'a');
    SendMessage1(message);

    base::RunLoop().RunUntilIdle();
  }

  if (get_result_listener()->get_result() == RESULT_UNKNOWN)
    base::RunLoop().Run();

  CommonTearDown();
}

void MemoryUsageManyMessagesCallback(IPC::Sender* sender,
                                     const IPC::Message& message,
                                     ChildProcessGlobals* globals) {
  static int message_index = 0;

  {
    // Map the shared memory, and make sure that its pages are counting towards
    // resident size.
    base::SharedMemoryHandle shm(GetSharedMemoryHandleFromMsg1(message));
    std::unique_ptr<base::SharedMemory> shared_memory(
        MapSharedMemoryHandle(shm, false));

    char* addr = static_cast<char*>(shared_memory->memory());
    std::string message_string(addr);
    int message_int;
    ASSERT_TRUE(base::StringToInt(message_string, &message_int));
    ASSERT_EQ(message_index, message_int);
    for (size_t i = 0; i < g_medium_message_size; i += 1024) {
      addr[i] = 'a';
    }
  }

  ++message_index;

  if (message_index == 1) {
    // Disable message logging, since it significantly contributes towards total
    // memory usage.
    LOG(INFO) << "Disable privileged process message logging.";
    globals->message_logging = false;
  }

  if (message_index == g_large_message_count) {
    size_t memory_increase_kb =
        (GetResidentSize() - globals->initial_resident_size) / 1024;
    LOG(INFO) << "Increase in memory usage in KB: " << memory_increase_kb;

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
    // Under a sanitizer build, releasing memory does not necessarily reduce the
    // amount of resident memory.
    bool success = true;
#else
    // The total increase in resident size should be less than 1MB. The exact
    // amount is not deterministic.
    bool success = memory_increase_kb < 1024;
#endif

    SendControlMessage(sender, success);
  }
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(MemoryUsageManyMessages) {
  return CommonPrivilegedProcessMain(&MemoryUsageManyMessagesCallback,
                                     "MemoryUsageManyMessages");
}

}  // namespace

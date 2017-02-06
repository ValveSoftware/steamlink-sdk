// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <windows.h>

#include <memory>
#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/win/scoped_handle.h"
#include "ipc/attachment_broker_privileged_win.h"
#include "ipc/attachment_broker_unprivileged_win.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/handle_win.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"
#include "ipc/ipc_test_messages.h"

namespace {

using base::win::ScopedHandle;

const char kDataBuffer[] = "This is some test data to write to the file.";
const size_t kSharedMemorySize = 20000;

// Returns the contents of the file represented by |h| as a std::string.
std::string ReadFromFile(HANDLE h) {
  SetFilePointer(h, 0, nullptr, FILE_BEGIN);
  char buffer[100];
  DWORD bytes_read;
  BOOL success = ::ReadFile(h, buffer, static_cast<DWORD>(strlen(kDataBuffer)),
                            &bytes_read, nullptr);
  return success ? std::string(buffer, bytes_read) : std::string();
}

ScopedHandle GetHandleFromBrokeredAttachment(
    const scoped_refptr<IPC::BrokerableAttachment>& attachment) {
  if (attachment->GetType() !=
      IPC::BrokerableAttachment::TYPE_BROKERABLE_ATTACHMENT) {
    LOG(INFO) << "Attachment type not TYPE_BROKERABLE_ATTACHMENT.";
    return ScopedHandle(nullptr);
  }

  if (attachment->GetBrokerableType() !=
      IPC::BrokerableAttachment::WIN_HANDLE) {
    LOG(INFO) << "Brokerable type not WIN_HANDLE.";
    return ScopedHandle(nullptr);
  }

  IPC::internal::HandleAttachmentWin* received_handle_attachment =
      static_cast<IPC::internal::HandleAttachmentWin*>(attachment.get());
  ScopedHandle h(received_handle_attachment->get_handle());
  received_handle_attachment->reset_handle_ownership();
  return h;
}

// |message| must be deserializable as a TestHandleWinMsg. Returns the HANDLE,
// or nullptr if deserialization failed.
ScopedHandle GetHandleFromTestHandleWinMsg(const IPC::Message& message) {
  // Expect a message with a brokered attachment.
  if (!message.HasBrokerableAttachments()) {
    LOG(INFO) << "Message missing brokerable attachment.";
    return ScopedHandle(nullptr);
  }

  TestHandleWinMsg::Schema::Param p;
  bool success = TestHandleWinMsg::Read(&message, &p);
  if (!success) {
    LOG(INFO) << "Failed to deserialize message.";
    return ScopedHandle(nullptr);
  }

  IPC::HandleWin handle_win = std::get<1>(p);
  return ScopedHandle(handle_win.get_handle());
}

// Returns a mapped, shared memory region based on the handle in |message|.
std::unique_ptr<base::SharedMemory> GetSharedMemoryFromSharedMemoryHandleMsg1(
    const IPC::Message& message,
    size_t size) {
  // Expect a message with a brokered attachment.
  if (!message.HasBrokerableAttachments()) {
    LOG(INFO) << "Message missing brokerable attachment.";
    return nullptr;
  }

  TestSharedMemoryHandleMsg1::Schema::Param p;
  bool success = TestSharedMemoryHandleMsg1::Read(&message, &p);
  if (!success) {
    LOG(INFO) << "Failed to deserialize message.";
    return nullptr;
  }

  base::SharedMemoryHandle handle = std::get<0>(p);
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, false));

  shared_memory->Map(size);
  return shared_memory;
}

// |message| must be deserializable as a TestTwoHandleWinMsg. Returns the
// HANDLE, or nullptr if deserialization failed.
bool GetHandleFromTestTwoHandleWinMsg(const IPC::Message& message,
                                      HANDLE* h1,
                                      HANDLE* h2) {
  // Expect a message with a brokered attachment.
  if (!message.HasBrokerableAttachments()) {
    LOG(INFO) << "Message missing brokerable attachment.";
    return false;
  }

  TestTwoHandleWinMsg::Schema::Param p;
  bool success = TestTwoHandleWinMsg::Read(&message, &p);
  if (!success) {
    LOG(INFO) << "Failed to deserialize message.";
    return false;
  }

  IPC::HandleWin handle_win = std::get<0>(p);
  *h1 = handle_win.get_handle();
  handle_win = std::get<1>(p);
  *h2 = handle_win.get_handle();
  return true;
}

// |message| must be deserializable as a TestHandleWinMsg. Returns true if the
// attached file HANDLE has contents |kDataBuffer|.
bool CheckContentsOfTestMessage(const IPC::Message& message) {
  ScopedHandle h(GetHandleFromTestHandleWinMsg(message));
  if (h.Get() == nullptr) {
    LOG(INFO) << "Failed to get handle from TestHandleWinMsg.";
    return false;
  }

  std::string contents = ReadFromFile(h.Get());
  bool success = (contents == std::string(kDataBuffer));
  if (!success) {
    LOG(INFO) << "Expected contents: " << std::string(kDataBuffer);
    LOG(INFO) << "Read contents: " << contents;
  }
  return success;
}

// Returns 0 on error.
DWORD GetCurrentProcessHandleCount() {
  DWORD handle_count = 0;
  BOOL success = ::GetProcessHandleCount(::GetCurrentProcess(), &handle_count);
  return success ? handle_count : 0;
}

enum TestResult {
  RESULT_UNKNOWN,
  RESULT_SUCCESS,
  RESULT_FAILURE,
};

// Once the test is finished, send a control message to the parent process with
// the result. The message may require the runloop to be run before its
// dispatched.
void SendControlMessage(IPC::Sender* sender, bool success) {
  IPC::Message* message = new IPC::Message(0, 2, IPC::Message::PRIORITY_NORMAL);
  TestResult result = success ? RESULT_SUCCESS : RESULT_FAILURE;
  message->WriteInt(result);
  sender->Send(message);
}

class MockObserver : public IPC::AttachmentBroker::Observer {
 public:
  void ReceivedBrokerableAttachmentWithId(
      const IPC::BrokerableAttachment::AttachmentId& id) override {
    id_ = id;
  }
  IPC::BrokerableAttachment::AttachmentId* get_id() { return &id_; }

 private:
  IPC::BrokerableAttachment::AttachmentId id_;
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
  IPC::Message get_first_message() { return messages_[0]; }
  void pop_first_message() { messages_.erase(messages_.begin()); }
  bool has_message() { return !messages_.empty(); }

 private:
  IPC::Listener* listener_;
  Reason reason_;
  std::vector<IPC::Message> messages_;
};

// Waits for a result to be sent over the channel.  Quits the message loop
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

// The parent process acts as an unprivileged process. The forked process acts
// as the privileged process.
class IPCAttachmentBrokerPrivilegedWinTest : public IPCTestBase {
 public:
  IPCAttachmentBrokerPrivilegedWinTest() {}
  ~IPCAttachmentBrokerPrivilegedWinTest() override {}

  void SetUp() override {
    IPCTestBase::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &temp_path_));
  }

  void TearDown() override { IPCTestBase::TearDown(); }

  // Takes ownership of |broker|. Has no effect if called after CommonSetUp().
  void set_broker(IPC::AttachmentBrokerUnprivilegedWin* broker) {
    broker_.reset(broker);
  }

  void CommonSetUp() {
    PreConnectSetUp();
    PostConnectSetUp();
  }

  // All of setup before the channel is connected.
  void PreConnectSetUp() {
    if (!broker_.get())
      set_broker(new IPC::AttachmentBrokerUnprivilegedWin);
    broker_->AddObserver(&observer_, task_runner());
    CreateChannel(&proxy_listener_);
    broker_->RegisterBrokerCommunicationChannel(channel());
  }

  // All of setup including the connection and everything after.
  void PostConnectSetUp() {
    ASSERT_TRUE(ConnectChannel());
    ASSERT_TRUE(StartClient());

    handle_count_ = GetCurrentProcessHandleCount();
    EXPECT_NE(handle_count_, 0u);
  }

  void CommonTearDown() {
    EXPECT_EQ(handle_count_, handle_count_);

    // Close the channel so the client's OnChannelError() gets fired.
    channel()->Close();

    EXPECT_TRUE(WaitForClientShutdown());
    DestroyChannel();
    broker_.reset();
  }

  HANDLE CreateTempFile() {
    EXPECT_NE(-1, WriteFile(temp_path_, kDataBuffer,
                            static_cast<int>(strlen(kDataBuffer))));

    HANDLE h =
        CreateFile(temp_path_.value().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    EXPECT_NE(h, INVALID_HANDLE_VALUE);
    return h;
  }

  void SendMessageWithAttachment(HANDLE h) {
    IPC::HandleWin handle_win(h, IPC::HandleWin::FILE_READ_WRITE);
    IPC::Message* message = new TestHandleWinMsg(100, handle_win, 200);
    sender()->Send(message);
  }

  ProxyListener* get_proxy_listener() { return &proxy_listener_; }
  IPC::AttachmentBrokerUnprivilegedWin* get_broker() { return broker_.get(); }
  MockObserver* get_observer() { return &observer_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_path_;
  ProxyListener proxy_listener_;
  std::unique_ptr<IPC::AttachmentBrokerUnprivilegedWin> broker_;
  MockObserver observer_;
  DWORD handle_count_;
};

// A broker which always sets the current process as the destination process
// for attachments.
class MockBroker : public IPC::AttachmentBrokerUnprivilegedWin {
 public:
  MockBroker() {}
  ~MockBroker() override {}
  bool SendAttachmentToProcess(
      const scoped_refptr<IPC::BrokerableAttachment>& attachment,
      base::ProcessId destination_process) override {
    return IPC::AttachmentBrokerUnprivilegedWin::SendAttachmentToProcess(
        attachment, base::Process::Current().Pid());
  }
};

// An unprivileged process makes a file HANDLE, and writes a string to it. The
// file HANDLE is sent to the privileged process using the attachment broker.
// The privileged process dups the HANDLE into its own HANDLE table. This test
// checks that the file has the same contents in the privileged process.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendHandle) {
  Init("SendHandle");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  HANDLE h = CreateTempFile();
  SendMessageWithAttachment(h);
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

// Similar to SendHandle, except the file HANDLE attached to the message has
// neither read nor write permissions.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest,
       SendHandleWithoutPermissions) {
  Init("SendHandleWithoutPermissions");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  HANDLE h = CreateTempFile();
  HANDLE h2;
  BOOL result = ::DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(),
                                  &h2, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
  ASSERT_TRUE(result);
  IPC::HandleWin handle_win(h2, IPC::HandleWin::DUPLICATE);
  IPC::Message* message = new TestHandleWinMsg(100, handle_win, 200);
  sender()->Send(message);
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

// Similar to SendHandle, except the attachment's destination process is this
// process. This is an unrealistic scenario, but simulates an unprivileged
// process sending an attachment to another unprivileged process.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendHandleToSelf) {
  Init("SendHandleToSelf");

  set_broker(new MockBroker);

  PreConnectSetUp();
  // Technically, the channel is an endpoint, but we need the proxy listener to
  // receive the messages so that it can quit the message loop.
  channel()->SetAttachmentBrokerEndpoint(false);
  PostConnectSetUp();
  get_proxy_listener()->set_listener(get_broker());

  HANDLE h = CreateTempFile();
  SendMessageWithAttachment(h);
  base::MessageLoop::current()->Run();

  // Get the received attachment.
  IPC::BrokerableAttachment::AttachmentId* id = get_observer()->get_id();
  scoped_refptr<IPC::BrokerableAttachment> received_attachment;
  get_broker()->GetAttachmentWithId(*id, &received_attachment);
  ASSERT_NE(received_attachment.get(), nullptr);

  // Check that it's a different entry in the HANDLE table.
  ScopedHandle h2(GetHandleFromBrokeredAttachment(received_attachment));
  EXPECT_NE(h2.Get(), h);

  // But still points to the same file.
  std::string contents = ReadFromFile(h2.Get());
  EXPECT_EQ(contents, std::string(kDataBuffer));

  CommonTearDown();
}

// Similar to SendHandle, but sends a message with two instances of the same
// handle.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendTwoHandles) {
  Init("SendTwoHandles");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  HANDLE h = CreateTempFile();
  HANDLE h2;
  BOOL result = ::DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(),
                                  &h2, 0, FALSE, DUPLICATE_SAME_ACCESS);
  ASSERT_TRUE(result);
  IPC::HandleWin handle_win1(h, IPC::HandleWin::FILE_READ_WRITE);
  IPC::HandleWin handle_win2(h2, IPC::HandleWin::FILE_READ_WRITE);
  IPC::Message* message = new TestTwoHandleWinMsg(handle_win1, handle_win2);
  sender()->Send(message);
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

// Similar to SendHandle, but sends the same message twice.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendHandleTwice) {
  Init("SendHandleTwice");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  HANDLE h = CreateTempFile();
  HANDLE h2;
  BOOL result = ::DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(),
                                  &h2, 0, FALSE, DUPLICATE_SAME_ACCESS);
  ASSERT_TRUE(result);
  SendMessageWithAttachment(h);
  SendMessageWithAttachment(h2);
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

// An unprivileged process makes a shared memory region and sends it to the
// privileged process.
TEST_F(IPCAttachmentBrokerPrivilegedWinTest, SendSharedMemoryHandle) {
  Init("SendSharedMemoryHandle");

  CommonSetUp();
  ResultListener result_listener;
  get_proxy_listener()->set_listener(&result_listener);

  std::unique_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  shared_memory->CreateAndMapAnonymous(kSharedMemorySize);
  memcpy(shared_memory->memory(), kDataBuffer, strlen(kDataBuffer));
  sender()->Send(new TestSharedMemoryHandleMsg1(shared_memory->handle()));
  base::MessageLoop::current()->Run();

  // Check the result.
  ASSERT_EQ(ProxyListener::MESSAGE_RECEIVED,
            get_proxy_listener()->get_reason());
  ASSERT_EQ(result_listener.get_result(), RESULT_SUCCESS);

  CommonTearDown();
}

using OnMessageReceivedCallback = void (*)(IPC::Sender* sender,
                                           const IPC::Message& message);

int CommonPrivilegedProcessMain(OnMessageReceivedCallback callback,
                                const char* channel_name) {
  LOG(INFO) << "Privileged process start.";
  base::MessageLoopForIO main_message_loop;
  ProxyListener listener;

  // Set up IPC channel.
  IPC::AttachmentBrokerPrivilegedWin broker;
  std::unique_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName(channel_name), &listener));
  broker.RegisterCommunicationChannel(channel.get(), nullptr);
  CHECK(channel->Connect());

  while (true) {
    LOG(INFO) << "Privileged process spinning run loop.";
    base::MessageLoop::current()->Run();
    ProxyListener::Reason reason = listener.get_reason();
    if (reason == ProxyListener::CHANNEL_ERROR)
      break;

    while (listener.has_message()) {
      LOG(INFO) << "Privileged process running callback.";
      callback(channel.get(), listener.get_first_message());
      LOG(INFO) << "Privileged process finishing callback.";
      listener.pop_first_message();
    }
  }

  LOG(INFO) << "Privileged process end.";
  return 0;
}

void SendHandleCallback(IPC::Sender* sender, const IPC::Message& message) {
  bool success = CheckContentsOfTestMessage(message);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandle) {
  return CommonPrivilegedProcessMain(&SendHandleCallback, "SendHandle");
}

void SendHandleWithoutPermissionsCallback(IPC::Sender* sender,
                                          const IPC::Message& message) {
  ScopedHandle h(GetHandleFromTestHandleWinMsg(message));
  if (h.Get() != nullptr) {
    SetFilePointer(h.Get(), 0, nullptr, FILE_BEGIN);

    char buffer[100];
    DWORD bytes_read;
    BOOL success =
        ::ReadFile(h.Get(), buffer, static_cast<DWORD>(strlen(kDataBuffer)),
                   &bytes_read, nullptr);
    if (!success && GetLastError() == ERROR_ACCESS_DENIED) {
      SendControlMessage(sender, true);
      return;
    }
  }

  SendControlMessage(sender, false);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandleWithoutPermissions) {
  return CommonPrivilegedProcessMain(&SendHandleWithoutPermissionsCallback,
                                     "SendHandleWithoutPermissions");
}

void SendHandleToSelfCallback(IPC::Sender* sender, const IPC::Message&) {
  // Do nothing special. The default behavior already runs the
  // AttachmentBrokerPrivilegedWin.
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandleToSelf) {
  return CommonPrivilegedProcessMain(&SendHandleToSelfCallback,
                                     "SendHandleToSelf");
}

void SendTwoHandlesCallback(IPC::Sender* sender, const IPC::Message& message) {
  // Check for two handles.
  HANDLE h1, h2;
  EXPECT_TRUE(GetHandleFromTestTwoHandleWinMsg(message, &h1, &h2));
  if (h1 == nullptr || h2 == nullptr) {
    SendControlMessage(sender, false);
    return;
  }

  // Check that their contents are correct.
  std::string contents1 = ReadFromFile(h1);
  std::string contents2 = ReadFromFile(h2);
  if (contents1 != std::string(kDataBuffer) ||
      contents2 != std::string(kDataBuffer)) {
    SendControlMessage(sender, false);
    return;
  }

  // Check that the handles point to the same file.
  const char text[] = "katy perry";
  DWORD bytes_written = 0;
  SetFilePointer(h1, 0, nullptr, FILE_BEGIN);
  BOOL success = ::WriteFile(h1, text, static_cast<DWORD>(strlen(text)),
                             &bytes_written, nullptr);
  if (!success) {
    SendControlMessage(sender, false);
    return;
  }

  SetFilePointer(h2, 0, nullptr, FILE_BEGIN);
  char buffer[100];
  DWORD bytes_read;
  success = ::ReadFile(h2, buffer, static_cast<DWORD>(strlen(text)),
                       &bytes_read, nullptr);
  if (!success) {
    SendControlMessage(sender, false);
    return;
  }
  success = std::string(buffer, bytes_read) == std::string(text);
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendTwoHandles) {
  return CommonPrivilegedProcessMain(&SendTwoHandlesCallback, "SendTwoHandles");
}

void SendHandleTwiceCallback(IPC::Sender* sender, const IPC::Message& message) {
  // We expect the same message twice.
  static int i = 0;
  static bool success = true;
  success &= CheckContentsOfTestMessage(message);
  if (i == 0) {
    LOG(INFO) << "Received first message.";
    ++i;
  } else {
    LOG(INFO) << "Received second message.";
    SendControlMessage(sender, success);
  }
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendHandleTwice) {
  return CommonPrivilegedProcessMain(&SendHandleTwiceCallback,
                                     "SendHandleTwice");
}

void SendSharedMemoryHandleCallback(IPC::Sender* sender,
                                    const IPC::Message& message) {
  std::unique_ptr<base::SharedMemory> shared_memory =
      GetSharedMemoryFromSharedMemoryHandleMsg1(message, kSharedMemorySize);
  bool success =
      memcmp(shared_memory->memory(), kDataBuffer, strlen(kDataBuffer)) == 0;
  SendControlMessage(sender, success);
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendSharedMemoryHandle) {
  return CommonPrivilegedProcessMain(&SendSharedMemoryHandleCallback,
                                     "SendSharedMemoryHandle");
}

}  // namespace

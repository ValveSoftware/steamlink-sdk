// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests are POSIX only.

#include "ipc/ipc_channel_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/unix_domain_socket_util.h"
#include "testing/multiprocess_func_list.h"

namespace {

static const uint32_t kQuitMessage = 47;

class IPCChannelPosixTestListener : public IPC::Listener {
 public:
  enum STATUS {
    DISCONNECTED,
    MESSAGE_RECEIVED,
    CHANNEL_ERROR,
    CONNECTED,
    DENIED,
    LISTEN_ERROR
  };

  IPCChannelPosixTestListener(bool quit_only_on_message)
      : status_(DISCONNECTED),
        quit_only_on_message_(quit_only_on_message) {
  }

  ~IPCChannelPosixTestListener() override {}

  bool OnMessageReceived(const IPC::Message& message) override {
    EXPECT_EQ(message.type(), kQuitMessage);
    status_ = MESSAGE_RECEIVED;
    QuitRunLoop();
    return true;
  }

  void OnChannelConnected(int32_t peer_pid) override {
    status_ = CONNECTED;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  void OnChannelError() override {
    status_ = CHANNEL_ERROR;
    QuitRunLoop();
  }

  void OnChannelDenied() override {
    status_ = DENIED;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  void OnChannelListenError() override {
    status_ = LISTEN_ERROR;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  STATUS status() { return status_; }

  void QuitRunLoop() {
    base::MessageLoopForIO* loop = base::MessageLoopForIO::current();
    if (loop->is_running()) {
      loop->QuitNow();
    } else {
      // Die as soon as Run is called.
      loop->task_runner()->PostTask(FROM_HERE, loop->QuitWhenIdleClosure());
    }
  }

 private:
  // The current status of the listener.
  STATUS status_;
  // If |quit_only_on_message_| then the listener will only break out of
  // the run loop when kQuitMessage is received.
  bool quit_only_on_message_;
};

class IPCChannelPosixTest : public base::MultiProcessTest {
 public:
  static void SetUpSocket(IPC::ChannelHandle *handle,
                          IPC::Channel::Mode mode);
  static void SpinRunLoop(base::TimeDelta delay);
  static const std::string GetConnectionSocketName();
  static const std::string GetChannelDirName();

  bool WaitForExit(base::Process& process, int* exit_code) {
#if defined(OS_ANDROID)
    return AndroidWaitForChildExitWithTimeout(
        process, base::TimeDelta::Max(), exit_code);
#else
    return process.WaitForExit(exit_code);
#endif  // defined(OS_ANDROID)
  }

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<base::MessageLoopForIO> message_loop_;
};

const std::string IPCChannelPosixTest::GetChannelDirName() {
  base::FilePath tmp_dir;
  PathService::Get(base::DIR_TEMP, &tmp_dir);
  return tmp_dir.value();
}

const std::string IPCChannelPosixTest::GetConnectionSocketName() {
  return GetChannelDirName() + "/chrome_IPCChannelPosixTest__ConnectionSocket";
}

void IPCChannelPosixTest::SetUp() {
  MultiProcessTest::SetUp();
  // Construct a fresh IO Message loop for the duration of each test.
  message_loop_.reset(new base::MessageLoopForIO());
}

void IPCChannelPosixTest::TearDown() {
  message_loop_.reset(NULL);
  MultiProcessTest::TearDown();
}

// Create up a socket and bind and listen to it, or connect it
// depending on the |mode|.
void IPCChannelPosixTest::SetUpSocket(IPC::ChannelHandle *handle,
                                      IPC::Channel::Mode mode) {
  const std::string& name = handle->name;

  int socket_fd = 0;
  if (mode == IPC::Channel::MODE_NAMED_SERVER) {
    IPC::CreateServerUnixDomainSocket(base::FilePath(name), &socket_fd);
  } else if (mode == IPC::Channel::MODE_NAMED_CLIENT) {
    IPC::CreateClientUnixDomainSocket(base::FilePath(name), &socket_fd);
  } else {
    FAIL() << "Unknown mode " << mode;
  }
  handle->socket.fd = socket_fd;
}

void IPCChannelPosixTest::SpinRunLoop(base::TimeDelta delay) {
  base::MessageLoopForIO* loop = base::MessageLoopForIO::current();
  // Post a quit task so that this loop eventually ends and we don't hang
  // in the case of a bad test. Usually, the run loop will quit sooner than
  // that because all tests use a IPCChannelPosixTestListener which quits the
  // current run loop on any channel activity.
  loop->task_runner()->PostDelayedTask(FROM_HERE, loop->QuitWhenIdleClosure(),
                                       delay);
  base::RunLoop().Run();
}

TEST_F(IPCChannelPosixTest, BasicListen) {
  const std::string kChannelName =
      GetChannelDirName() + "/IPCChannelPosixTest_BasicListen";

  // Test creating a socket that is listening.
  IPC::ChannelHandle handle(kChannelName);
  SetUpSocket(&handle, IPC::Channel::MODE_NAMED_SERVER);
  unlink(handle.name.c_str());
  std::unique_ptr<IPC::ChannelPosix> channel(
      new IPC::ChannelPosix(handle, IPC::Channel::MODE_NAMED_SERVER, NULL));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());
  channel->ResetToAcceptingConnectionState();
  ASSERT_FALSE(channel->HasAcceptedConnection());
  unlink(handle.name.c_str());
}

TEST_F(IPCChannelPosixTest, BasicConnected) {
  // Test creating a socket that is connected.
  int pipe_fds[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds));
  std::string socket_name("/var/tmp/IPCChannelPosixTest_BasicConnected");
  ASSERT_GE(fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK), 0);

  base::FileDescriptor fd(pipe_fds[0], false);
  IPC::ChannelHandle handle(socket_name, fd);
  std::unique_ptr<IPC::ChannelPosix> channel(
      new IPC::ChannelPosix(handle, IPC::Channel::MODE_SERVER, NULL));
  ASSERT_TRUE(channel->Connect());
  ASSERT_FALSE(channel->AcceptsConnections());
  channel->Close();
  ASSERT_TRUE(IGNORE_EINTR(close(pipe_fds[1])) == 0);

  // Make sure that we can use the socket that is created for us by
  // a standard channel.
  std::unique_ptr<IPC::ChannelPosix> channel2(
      new IPC::ChannelPosix(socket_name, IPC::Channel::MODE_SERVER, NULL));
  ASSERT_TRUE(channel2->Connect());
  ASSERT_FALSE(channel2->AcceptsConnections());
}

// If a connection closes right before a Send() call, we may end up closing
// the connection without notifying the listener, which can cause hangs in
// sync_message_filter and others. Make sure the listener is notified.
TEST_F(IPCChannelPosixTest, SendHangTest) {
  IPCChannelPosixTestListener out_listener(true);
  IPCChannelPosixTestListener in_listener(true);
  IPC::ChannelHandle in_handle("IN");
  std::unique_ptr<IPC::ChannelPosix> in_chan(new IPC::ChannelPosix(
      in_handle, IPC::Channel::MODE_SERVER, &in_listener));
  IPC::ChannelHandle out_handle(
      "OUT", base::FileDescriptor(in_chan->TakeClientFileDescriptor()));
  std::unique_ptr<IPC::ChannelPosix> out_chan(new IPC::ChannelPosix(
      out_handle, IPC::Channel::MODE_CLIENT, &out_listener));
  ASSERT_TRUE(in_chan->Connect());
  ASSERT_TRUE(out_chan->Connect());
  in_chan->Close();  // simulate remote process dying at an unfortunate time.
  // Send will fail, because it cannot write the message.
  ASSERT_FALSE(out_chan->Send(new IPC::Message(
      0,  // routing_id
      kQuitMessage,  // message type
      IPC::Message::PRIORITY_NORMAL)));
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, out_listener.status());
}

// If a connection closes right before a Connect() call, we may end up closing
// the connection without notifying the listener, which can cause hangs in
// sync_message_filter and others. Make sure the listener is notified.
TEST_F(IPCChannelPosixTest, AcceptHangTest) {
  IPCChannelPosixTestListener out_listener(true);
  IPCChannelPosixTestListener in_listener(true);
  IPC::ChannelHandle in_handle("IN");
  std::unique_ptr<IPC::ChannelPosix> in_chan(new IPC::ChannelPosix(
      in_handle, IPC::Channel::MODE_SERVER, &in_listener));
  IPC::ChannelHandle out_handle(
      "OUT", base::FileDescriptor(in_chan->TakeClientFileDescriptor()));
  std::unique_ptr<IPC::ChannelPosix> out_chan(new IPC::ChannelPosix(
      out_handle, IPC::Channel::MODE_CLIENT, &out_listener));
  ASSERT_TRUE(in_chan->Connect());
  in_chan->Close();  // simulate remote process dying at an unfortunate time.
  ASSERT_FALSE(out_chan->Connect());
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, out_listener.status());
}

TEST_F(IPCChannelPosixTest, AdvancedConnected) {
  // Test creating a connection to an external process.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  std::unique_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::Process process = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(process.IsValid());
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0,  // routing_id
                                           kQuitMessage,  // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel->Send(message);
  SpinRunLoop(TestTimeouts::action_timeout());
  int exit_code = 0;
  EXPECT_TRUE(WaitForExit(process, &exit_code));
  EXPECT_EQ(0, exit_code);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel->HasAcceptedConnection());
  unlink(chan_handle.name.c_str());
}

TEST_F(IPCChannelPosixTest, ResetState) {
  // Test creating a connection to an external process. Close the connection,
  // but continue to listen and make sure another external process can connect
  // to us.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  std::unique_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::Process process = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(process.IsValid());
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  channel->ResetToAcceptingConnectionState();
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::Process process2 = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(process2.IsValid());
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0,  // routing_id
                                           kQuitMessage,  // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel->Send(message);
  SpinRunLoop(TestTimeouts::action_timeout());
  EXPECT_TRUE(process.Terminate(0, false));
  int exit_code = 0;
  EXPECT_TRUE(WaitForExit(process2, &exit_code));
  EXPECT_EQ(0, exit_code);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel->HasAcceptedConnection());
  unlink(chan_handle.name.c_str());
}

TEST_F(IPCChannelPosixTest, BadChannelName) {
  // Test empty name
  IPC::ChannelHandle handle("");
  std::unique_ptr<IPC::ChannelPosix> channel(
      new IPC::ChannelPosix(handle, IPC::Channel::MODE_NAMED_SERVER, NULL));
  ASSERT_FALSE(channel->Connect());

  // Test name that is too long.
  const char *kTooLongName = "This_is_a_very_long_name_to_proactively_implement"
                             "client-centered_synergy_through_top-line"
                             "platforms_Phosfluorescently_disintermediate_"
                             "clicks-and-mortar_best_practices_without_"
                             "future-proof_growth_strategies_Continually"
                             "pontificate_proactive_potentialities_before"
                             "leading-edge_processes";
  EXPECT_GE(strlen(kTooLongName), IPC::kMaxSocketNameLength);
  IPC::ChannelHandle handle2(kTooLongName);
  std::unique_ptr<IPC::ChannelPosix> channel2(
      new IPC::ChannelPosix(handle2, IPC::Channel::MODE_NAMED_SERVER, NULL));
  EXPECT_FALSE(channel2->Connect());
}

TEST_F(IPCChannelPosixTest, MultiConnection) {
  // Test setting up a connection to an external process, and then have
  // another external process attempt to connect to us.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  std::unique_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::Process process = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(process.IsValid());
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  base::Process process2 = SpawnChild("IPCChannelPosixFailConnectionProc");
  ASSERT_TRUE(process2.IsValid());
  SpinRunLoop(TestTimeouts::action_max_timeout());
  int exit_code = 0;
  EXPECT_TRUE(WaitForExit(process2, &exit_code));
  EXPECT_EQ(exit_code, 0);
  ASSERT_EQ(IPCChannelPosixTestListener::DENIED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0,  // routing_id
                                           kQuitMessage,  // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel->Send(message);
  SpinRunLoop(TestTimeouts::action_timeout());
  EXPECT_TRUE(WaitForExit(process, &exit_code));
  EXPECT_EQ(exit_code, 0);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel->HasAcceptedConnection());
  unlink(chan_handle.name.c_str());
}

TEST_F(IPCChannelPosixTest, DoubleServer) {
  // Test setting up two servers with the same name.
  IPCChannelPosixTestListener listener(false);
  IPCChannelPosixTestListener listener2(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  std::unique_ptr<IPC::ChannelPosix> channel(
      new IPC::ChannelPosix(chan_handle, IPC::Channel::MODE_SERVER, &listener));
  std::unique_ptr<IPC::ChannelPosix> channel2(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_SERVER, &listener2));
  ASSERT_TRUE(channel->Connect());
  ASSERT_FALSE(channel2->Connect());
}

TEST_F(IPCChannelPosixTest, BadMode) {
  // Test setting up two servers with a bad mode.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  std::unique_ptr<IPC::ChannelPosix> channel(
      new IPC::ChannelPosix(chan_handle, IPC::Channel::MODE_NONE, &listener));
  ASSERT_FALSE(channel->Connect());
}

TEST_F(IPCChannelPosixTest, IsNamedServerInitialized) {
  const std::string& connection_socket_name = GetConnectionSocketName();
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(connection_socket_name);
  ASSERT_TRUE(base::DeleteFile(base::FilePath(connection_socket_name), false));
  ASSERT_FALSE(IPC::Channel::IsNamedServerInitialized(
      connection_socket_name));
  std::unique_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(IPC::Channel::IsNamedServerInitialized(
      connection_socket_name));
  channel->Close();
  ASSERT_FALSE(IPC::Channel::IsNamedServerInitialized(
      connection_socket_name));
  unlink(chan_handle.name.c_str());
}

// A long running process that connects to us
MULTIPROCESS_TEST_MAIN(IPCChannelPosixTestConnectionProc) {
  base::MessageLoopForIO message_loop;
  IPCChannelPosixTestListener listener(true);
  IPC::ChannelHandle handle(IPCChannelPosixTest::GetConnectionSocketName());
  IPCChannelPosixTest::SetUpSocket(&handle, IPC::Channel::MODE_NAMED_CLIENT);
  std::unique_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      handle, IPC::Channel::MODE_NAMED_CLIENT, &listener));
  EXPECT_TRUE(channel->Connect());
  IPCChannelPosixTest::SpinRunLoop(TestTimeouts::action_max_timeout());
  EXPECT_EQ(IPCChannelPosixTestListener::MESSAGE_RECEIVED, listener.status());
  return 0;
}

// Simple external process that shouldn't be able to connect to us.
MULTIPROCESS_TEST_MAIN(IPCChannelPosixFailConnectionProc) {
  base::MessageLoopForIO message_loop;
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle handle(IPCChannelPosixTest::GetConnectionSocketName());
  IPCChannelPosixTest::SetUpSocket(&handle, IPC::Channel::MODE_NAMED_CLIENT);
  std::unique_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      handle, IPC::Channel::MODE_NAMED_CLIENT, &listener));

  // In this case connect may succeed or fail depending on if the packet
  // actually gets sent at sendmsg. Since we never delay on send, we may not
  // see the error. However even if connect succeeds, eventually we will get an
  // error back since the channel will be closed when we attempt to read from
  // it.
  bool connected = channel->Connect();
  if (connected) {
    IPCChannelPosixTest::SpinRunLoop(TestTimeouts::action_max_timeout());
    EXPECT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  } else {
    EXPECT_EQ(IPCChannelPosixTestListener::DISCONNECTED, listener.status());
  }
  return 0;
}

}  // namespace

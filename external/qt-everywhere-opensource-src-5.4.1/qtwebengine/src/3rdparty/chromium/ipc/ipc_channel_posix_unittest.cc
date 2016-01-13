// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests are POSIX only.

#include "ipc/ipc_channel_posix.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "ipc/ipc_listener.h"
#include "ipc/unix_domain_socket_util.h"
#include "testing/multiprocess_func_list.h"

namespace {

static const uint32 kQuitMessage = 47;

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

  virtual ~IPCChannelPosixTestListener() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    EXPECT_EQ(message.type(), kQuitMessage);
    status_ = MESSAGE_RECEIVED;
    QuitRunLoop();
    return true;
  }

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
    status_ = CONNECTED;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  virtual void OnChannelError() OVERRIDE {
    status_ = CHANNEL_ERROR;
    QuitRunLoop();
  }

  virtual void OnChannelDenied() OVERRIDE {
    status_ = DENIED;
    if (!quit_only_on_message_) {
      QuitRunLoop();
    }
  }

  virtual void OnChannelListenError() OVERRIDE {
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
      loop->PostTask(FROM_HERE, loop->QuitClosure());
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

 protected:
  virtual void SetUp();
  virtual void TearDown();

 private:
  scoped_ptr<base::MessageLoopForIO> message_loop_;
};

const std::string IPCChannelPosixTest::GetChannelDirName() {
#if defined(OS_ANDROID)
  base::FilePath tmp_dir;
  PathService::Get(base::DIR_CACHE, &tmp_dir);
  return tmp_dir.value();
#else
  return "/var/tmp";
#endif
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

  int socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  ASSERT_GE(socket_fd, 0) << name;
  ASSERT_GE(fcntl(socket_fd, F_SETFL, O_NONBLOCK), 0);
  struct sockaddr_un server_address = { 0 };
  memset(&server_address, 0, sizeof(server_address));
  server_address.sun_family = AF_UNIX;
  int path_len = snprintf(server_address.sun_path, IPC::kMaxSocketNameLength,
                          "%s", name.c_str());
  DCHECK_EQ(static_cast<int>(name.length()), path_len);
  size_t server_address_len = offsetof(struct sockaddr_un,
                                       sun_path) + path_len + 1;

  if (mode == IPC::Channel::MODE_NAMED_SERVER) {
    // Only one server at a time. Cleanup garbage if it exists.
    unlink(name.c_str());
    // Make sure the path we need exists.
    base::FilePath path(name);
    base::FilePath dir_path = path.DirName();
    ASSERT_TRUE(base::CreateDirectory(dir_path));
    ASSERT_GE(bind(socket_fd,
                   reinterpret_cast<struct sockaddr *>(&server_address),
                   server_address_len), 0) << server_address.sun_path
                                           << ": " << strerror(errno)
                                           << "(" << errno << ")";
    ASSERT_GE(listen(socket_fd, SOMAXCONN), 0) << server_address.sun_path
                                               << ": " << strerror(errno)
                                               << "(" << errno << ")";
  } else if (mode == IPC::Channel::MODE_NAMED_CLIENT) {
    ASSERT_GE(connect(socket_fd,
                      reinterpret_cast<struct sockaddr *>(&server_address),
                      server_address_len), 0) << server_address.sun_path
                                              << ": " << strerror(errno)
                                              << "(" << errno << ")";
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
  loop->PostDelayedTask(FROM_HERE, loop->QuitClosure(), delay);
  loop->Run();
}

TEST_F(IPCChannelPosixTest, BasicListen) {
  const std::string kChannelName =
      GetChannelDirName() + "/IPCChannelPosixTest_BasicListen";

  // Test creating a socket that is listening.
  IPC::ChannelHandle handle(kChannelName);
  SetUpSocket(&handle, IPC::Channel::MODE_NAMED_SERVER);
  unlink(handle.name.c_str());
  scoped_ptr<IPC::ChannelPosix> channel(
      new IPC::ChannelPosix(handle, IPC::Channel::MODE_NAMED_SERVER, NULL));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());
  channel->ResetToAcceptingConnectionState();
  ASSERT_FALSE(channel->HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, BasicConnected) {
  // Test creating a socket that is connected.
  int pipe_fds[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds));
  std::string socket_name("/var/tmp/IPCChannelPosixTest_BasicConnected");
  ASSERT_GE(fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK), 0);

  base::FileDescriptor fd(pipe_fds[0], false);
  IPC::ChannelHandle handle(socket_name, fd);
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      handle, IPC::Channel::MODE_SERVER, NULL));
  ASSERT_TRUE(channel->Connect());
  ASSERT_FALSE(channel->AcceptsConnections());
  channel->Close();
  ASSERT_TRUE(IGNORE_EINTR(close(pipe_fds[1])) == 0);

  // Make sure that we can use the socket that is created for us by
  // a standard channel.
  scoped_ptr<IPC::ChannelPosix> channel2(new IPC::ChannelPosix(
      socket_name, IPC::Channel::MODE_SERVER, NULL));
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
  scoped_ptr<IPC::ChannelPosix> in_chan(new IPC::ChannelPosix(
      in_handle, IPC::Channel::MODE_SERVER, &in_listener));
  base::FileDescriptor out_fd(
      in_chan->TakeClientFileDescriptor(), false);
  IPC::ChannelHandle out_handle("OUT", out_fd);
  scoped_ptr<IPC::ChannelPosix> out_chan(new IPC::ChannelPosix(
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
  scoped_ptr<IPC::ChannelPosix> in_chan(new IPC::ChannelPosix(
      in_handle, IPC::Channel::MODE_SERVER, &in_listener));
  base::FileDescriptor out_fd(
      in_chan->TakeClientFileDescriptor(), false);
  IPC::ChannelHandle out_handle("OUT", out_fd);
  scoped_ptr<IPC::ChannelPosix> out_chan(new IPC::ChannelPosix(
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
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::ProcessHandle handle = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0,  // routing_id
                                           kQuitMessage,  // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel->Send(message);
  SpinRunLoop(TestTimeouts::action_timeout());
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle, &exit_code));
  EXPECT_EQ(0, exit_code);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel->HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, ResetState) {
  // Test creating a connection to an external process. Close the connection,
  // but continue to listen and make sure another external process can connect
  // to us.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::ProcessHandle handle = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  channel->ResetToAcceptingConnectionState();
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::ProcessHandle handle2 = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(handle2);
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0,  // routing_id
                                           kQuitMessage,  // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel->Send(message);
  SpinRunLoop(TestTimeouts::action_timeout());
  EXPECT_TRUE(base::KillProcess(handle, 0, false));
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle2, &exit_code));
  EXPECT_EQ(0, exit_code);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel->HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, BadChannelName) {
  // Test empty name
  IPC::ChannelHandle handle("");
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      handle, IPC::Channel::MODE_NAMED_SERVER, NULL));
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
  scoped_ptr<IPC::ChannelPosix> channel2(new IPC::ChannelPosix(
      handle2, IPC::Channel::MODE_NAMED_SERVER, NULL));
  EXPECT_FALSE(channel2->Connect());
}

TEST_F(IPCChannelPosixTest, MultiConnection) {
  // Test setting up a connection to an external process, and then have
  // another external process attempt to connect to us.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  SetUpSocket(&chan_handle, IPC::Channel::MODE_NAMED_SERVER);
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(channel->Connect());
  ASSERT_TRUE(channel->AcceptsConnections());
  ASSERT_FALSE(channel->HasAcceptedConnection());

  base::ProcessHandle handle = SpawnChild("IPCChannelPosixTestConnectionProc");
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout());
  ASSERT_EQ(IPCChannelPosixTestListener::CONNECTED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  base::ProcessHandle handle2 = SpawnChild("IPCChannelPosixFailConnectionProc");
  ASSERT_TRUE(handle2);
  SpinRunLoop(TestTimeouts::action_max_timeout());
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCode(handle2, &exit_code));
  EXPECT_EQ(exit_code, 0);
  ASSERT_EQ(IPCChannelPosixTestListener::DENIED, listener.status());
  ASSERT_TRUE(channel->HasAcceptedConnection());
  IPC::Message* message = new IPC::Message(0,  // routing_id
                                           kQuitMessage,  // message type
                                           IPC::Message::PRIORITY_NORMAL);
  channel->Send(message);
  SpinRunLoop(TestTimeouts::action_timeout());
  EXPECT_TRUE(base::WaitForExitCode(handle, &exit_code));
  EXPECT_EQ(exit_code, 0);
  ASSERT_EQ(IPCChannelPosixTestListener::CHANNEL_ERROR, listener.status());
  ASSERT_FALSE(channel->HasAcceptedConnection());
}

TEST_F(IPCChannelPosixTest, DoubleServer) {
  // Test setting up two servers with the same name.
  IPCChannelPosixTestListener listener(false);
  IPCChannelPosixTestListener listener2(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_SERVER, &listener));
  scoped_ptr<IPC::ChannelPosix> channel2(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_SERVER, &listener2));
  ASSERT_TRUE(channel->Connect());
  ASSERT_FALSE(channel2->Connect());
}

TEST_F(IPCChannelPosixTest, BadMode) {
  // Test setting up two servers with a bad mode.
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(GetConnectionSocketName());
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NONE, &listener));
  ASSERT_FALSE(channel->Connect());
}

TEST_F(IPCChannelPosixTest, IsNamedServerInitialized) {
  const std::string& connection_socket_name = GetConnectionSocketName();
  IPCChannelPosixTestListener listener(false);
  IPC::ChannelHandle chan_handle(connection_socket_name);
  ASSERT_TRUE(base::DeleteFile(base::FilePath(connection_socket_name), false));
  ASSERT_FALSE(IPC::Channel::IsNamedServerInitialized(
      connection_socket_name));
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
      chan_handle, IPC::Channel::MODE_NAMED_SERVER, &listener));
  ASSERT_TRUE(IPC::Channel::IsNamedServerInitialized(
      connection_socket_name));
  channel->Close();
  ASSERT_FALSE(IPC::Channel::IsNamedServerInitialized(
      connection_socket_name));
}

// A long running process that connects to us
MULTIPROCESS_TEST_MAIN(IPCChannelPosixTestConnectionProc) {
  base::MessageLoopForIO message_loop;
  IPCChannelPosixTestListener listener(true);
  IPC::ChannelHandle handle(IPCChannelPosixTest::GetConnectionSocketName());
  IPCChannelPosixTest::SetUpSocket(&handle, IPC::Channel::MODE_NAMED_CLIENT);
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
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
  scoped_ptr<IPC::ChannelPosix> channel(new IPC::ChannelPosix(
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

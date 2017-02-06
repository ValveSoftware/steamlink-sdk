// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_POSIX)
#if defined(OS_MACOSX)
extern "C" {
#include <sandbox.h>
};
#endif
#include <fcntl.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/file_descriptor_posix.h"
#include "base/location.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_test_base.h"

#if defined(OS_POSIX)
#include "base/macros.h"
#endif

#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt.h"
#endif

namespace {

const unsigned kNumFDsToSend = 7;  // per message
const unsigned kNumMessages = 20;
const char* kDevZeroPath = "/dev/zero";

#if defined(OS_POSIX)
static_assert(kNumFDsToSend ==
                  IPC::MessageAttachmentSet::kMaxDescriptorsPerMessage,
              "The number of FDs to send must be kMaxDescriptorsPerMessage.");
#endif

class MyChannelDescriptorListenerBase : public IPC::Listener {
 public:
  bool OnMessageReceived(const IPC::Message& message) override {
    base::PickleIterator iter(message);
    base::FileDescriptor descriptor;
    while (IPC::ParamTraits<base::FileDescriptor>::Read(
               &message, &iter, &descriptor)) {
      HandleFD(descriptor.fd);
    }
    return true;
  }

 protected:
  virtual void HandleFD(int fd) = 0;
};

class MyChannelDescriptorListener : public MyChannelDescriptorListenerBase {
 public:
  explicit MyChannelDescriptorListener(ino_t expected_inode_num)
      : MyChannelDescriptorListenerBase(),
        expected_inode_num_(expected_inode_num),
        num_fds_received_(0) {
  }

  bool GotExpectedNumberOfDescriptors() const {
    return num_fds_received_ == kNumFDsToSend * kNumMessages;
  }

  void OnChannelError() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 protected:
  void HandleFD(int fd) override {
    ASSERT_GE(fd, 0);
    // Check that we can read from the FD.
    char buf;
    ssize_t amt_read = read(fd, &buf, 1);
    ASSERT_EQ(amt_read, 1);
    ASSERT_EQ(buf, 0);  // /dev/zero always reads 0 bytes.

    struct stat st;
    ASSERT_EQ(fstat(fd, &st), 0);

    ASSERT_EQ(close(fd), 0);

    // Compare inode numbers to check that the file sent over the wire is
    // actually the one expected.
    ASSERT_EQ(expected_inode_num_, st.st_ino);

    ++num_fds_received_;
    if (num_fds_received_ == kNumFDsToSend * kNumMessages)
      base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  ino_t expected_inode_num_;
  unsigned num_fds_received_;
};


class IPCSendFdsTest : public IPCTestBase {
 protected:
  void RunServer() {
    // Set up IPC channel and start client.
    MyChannelDescriptorListener listener(-1);
    CreateChannel(&listener);
    ASSERT_TRUE(ConnectChannel());
    ASSERT_TRUE(StartClient());

    for (unsigned i = 0; i < kNumMessages; ++i) {
      IPC::Message* message =
          new IPC::Message(0, 3, IPC::Message::PRIORITY_NORMAL);
      for (unsigned j = 0; j < kNumFDsToSend; ++j) {
        const int fd = open(kDevZeroPath, O_RDONLY);
        ASSERT_GE(fd, 0);
        base::FileDescriptor descriptor(fd, true);
        IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
      }
      ASSERT_TRUE(sender()->Send(message));
    }

    // Run message loop.
    base::RunLoop().Run();

    // Close the channel so the client's OnChannelError() gets fired.
    channel()->Close();

    EXPECT_TRUE(WaitForClientShutdown());
    DestroyChannel();
  }
};

TEST_F(IPCSendFdsTest, DescriptorTest) {
  Init("SendFdsClient");
  RunServer();
}

int SendFdsClientCommon(const std::string& test_client_name,
                        ino_t expected_inode_num) {
  base::MessageLoopForIO main_message_loop;
  MyChannelDescriptorListener listener(expected_inode_num);

  // Set up IPC channel.
  std::unique_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName(test_client_name), &listener));
  CHECK(channel->Connect());

  // Run message loop.
  base::RunLoop().Run();

  // Verify that the message loop was exited due to getting the correct number
  // of descriptors, and not because of the channel closing unexpectedly.
  CHECK(listener.GotExpectedNumberOfDescriptors());

  return 0;
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendFdsClient) {
  struct stat st;
  int fd = open(kDevZeroPath, O_RDONLY);
  fstat(fd, &st);
  EXPECT_GE(IGNORE_EINTR(close(fd)), 0);
  return SendFdsClientCommon("SendFdsClient", st.st_ino);
}

#if defined(OS_MACOSX)
// Test that FDs are correctly sent to a sandboxed process.
// TODO(port): Make this test cross-platform.
TEST_F(IPCSendFdsTest, DescriptorTestSandboxed) {
  Init("SendFdsSandboxedClient");
  RunServer();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendFdsSandboxedClient) {
  struct stat st;
  const int fd = open(kDevZeroPath, O_RDONLY);
  fstat(fd, &st);
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;

  // Enable the sandbox.
  char* error_buff = NULL;
  int error = sandbox::Seatbelt::Init(kSBXProfilePureComputation, SANDBOX_NAMED,
                                      &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  if (!success)
    return -1;

  sandbox::Seatbelt::FreeError(error_buff);

  // Make sure sandbox is really enabled.
  if (open(kDevZeroPath, O_RDONLY) != -1) {
    LOG(ERROR) << "Sandbox wasn't properly enabled";
    return -1;
  }

  // See if we can receive a file descriptor.
  return SendFdsClientCommon("SendFdsSandboxedClient", st.st_ino);
}
#endif  // defined(OS_MACOSX)


class MyCBListener : public MyChannelDescriptorListenerBase {
 public:
  MyCBListener(base::Callback<void(int)> cb, int fds_to_send)
      : MyChannelDescriptorListenerBase(),
        cb_(cb) {
    }

 protected:
  void HandleFD(int fd) override { cb_.Run(fd); }
 private:
  base::Callback<void(int)> cb_;
};

std::pair<int, int> make_socket_pair() {
  int pipe_fds[2];
  CHECK_EQ(0, HANDLE_EINTR(socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds)));
  return std::pair<int, int>(pipe_fds[0], pipe_fds[1]);
}

static void null_cb(int unused_fd) {
  NOTREACHED();
}

class PipeChannelHelper {
 public:
  PipeChannelHelper(base::Thread* in_thread,
                    base::Thread* out_thread,
                    base::Callback<void(int)> cb,
                    int fds_to_send) :
      in_thread_(in_thread),
      out_thread_(out_thread),
      cb_listener_(cb, fds_to_send),
      null_listener_(base::Bind(&null_cb), 0) {
  }

  void Init() {
    IPC::ChannelHandle in_handle("IN");
    in = IPC::Channel::CreateServer(in_handle, &null_listener_);
    IPC::ChannelHandle out_handle(
        "OUT", base::FileDescriptor(in->TakeClientFileDescriptor()));
    out = IPC::Channel::CreateClient(out_handle, &cb_listener_);
    // PostTask the connect calls to make sure the callbacks happens
    // on the right threads.
    in_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PipeChannelHelper::Connect, in.get()));
    out_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PipeChannelHelper::Connect, out.get()));
  }

  static void DestroyChannel(std::unique_ptr<IPC::Channel>* c,
                             base::WaitableEvent* event) {
    c->reset(0);
    event->Signal();
  }

  ~PipeChannelHelper() {
    base::WaitableEvent a(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::WaitableEvent b(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED);
    in_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PipeChannelHelper::DestroyChannel, &in, &a));
    out_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PipeChannelHelper::DestroyChannel, &out, &b));
    a.Wait();
    b.Wait();
  }

  static void Connect(IPC::Channel *channel) {
    EXPECT_TRUE(channel->Connect());
  }

  void Send(int fd) {
    CHECK_EQ(base::MessageLoop::current(), in_thread_->message_loop());

    ASSERT_GE(fd, 0);
    base::FileDescriptor descriptor(fd, true);

    IPC::Message* message =
        new IPC::Message(0, 3, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
    ASSERT_TRUE(in->Send(message));
  }

 private:
  std::unique_ptr<IPC::Channel> in, out;
  base::Thread* in_thread_;
  base::Thread* out_thread_;
  MyCBListener cb_listener_;
  MyCBListener null_listener_;
};

// This test is meant to provoke a kernel bug on OSX, and to prove
// that the workaround for it is working. It sets up two pipes and three
// threads, the producer thread creates socketpairs and sends one of the fds
// over pipe1 to the middleman thread. The middleman thread simply takes the fd
// sends it over pipe2 to the consumer thread. The consumer thread writes a byte
// to each fd it receives and then closes the pipe. The producer thread reads
// the bytes back from each pair of pipes and make sure that everything worked.
// This feedback mechanism makes sure that not too many file descriptors are
// in flight at the same time. For more info on the bug, see:
// http://crbug.com/298276
class IPCMultiSendingFdsTest : public testing::Test {
 public:
  IPCMultiSendingFdsTest()
      : received_(base::WaitableEvent::ResetPolicy::MANUAL,
                  base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void Producer(PipeChannelHelper* dest,
                base::Thread* t,
                int pipes_to_send) {
    for (int i = 0; i < pipes_to_send; i++) {
      received_.Reset();
      std::pair<int, int> pipe_fds = make_socket_pair();
      t->task_runner()->PostTask(
          FROM_HERE, base::Bind(&PipeChannelHelper::Send,
                                base::Unretained(dest), pipe_fds.second));
      char tmp = 'x';
      CHECK_EQ(1, HANDLE_EINTR(write(pipe_fds.first, &tmp, 1)));
      CHECK_EQ(0, IGNORE_EINTR(close(pipe_fds.first)));
      received_.Wait();
    }
  }

  void ConsumerHandleFD(int fd) {
    char tmp = 'y';
    CHECK_EQ(1, HANDLE_EINTR(read(fd, &tmp, 1)));
    CHECK_EQ(tmp, 'x');
    CHECK_EQ(0, IGNORE_EINTR(close(fd)));
    received_.Signal();
  }

  base::Thread* CreateThread(const char* name) {
    base::Thread* ret = new base::Thread(name);
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    ret->StartWithOptions(options);
    return ret;
  }

  void Run() {
    // On my mac, this test fails roughly 35 times per
    // million sends with low load, but much more with high load.
    // Unless the workaround is in place. With 10000 sends, we
    // should see at least a 3% failure rate.
    const int pipes_to_send = 20000;
    std::unique_ptr<base::Thread> producer(CreateThread("producer"));
    std::unique_ptr<base::Thread> middleman(CreateThread("middleman"));
    std::unique_ptr<base::Thread> consumer(CreateThread("consumer"));
    PipeChannelHelper pipe1(
        middleman.get(),
        consumer.get(),
        base::Bind(&IPCMultiSendingFdsTest::ConsumerHandleFD,
                   base::Unretained(this)),
        pipes_to_send);
    PipeChannelHelper pipe2(
        producer.get(),
        middleman.get(),
        base::Bind(&PipeChannelHelper::Send, base::Unretained(&pipe1)),
        pipes_to_send);
    pipe1.Init();
    pipe2.Init();
    Producer(&pipe2, producer.get(), pipes_to_send);
  }

 private:
  base::WaitableEvent received_;
};

TEST_F(IPCMultiSendingFdsTest, StressTest) {
  Run();
}

}  // namespace

#endif  // defined(OS_POSIX)

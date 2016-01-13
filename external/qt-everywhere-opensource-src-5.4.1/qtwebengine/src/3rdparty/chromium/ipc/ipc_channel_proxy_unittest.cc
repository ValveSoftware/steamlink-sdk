// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/threading/thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_base.h"
#include "ipc/message_filter.h"

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "ipc/ipc_channel_proxy_unittest_messages.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#include "ipc/ipc_channel_proxy_unittest_messages.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#include "ipc/ipc_channel_proxy_unittest_messages.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "ipc/ipc_channel_proxy_unittest_messages.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "ipc/ipc_channel_proxy_unittest_messages.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "ipc/ipc_channel_proxy_unittest_messages.h"
}  // namespace IPC


namespace {

class QuitListener : public IPC::Listener {
 public:
  QuitListener() : bad_message_received_(false) {}
  virtual ~QuitListener() {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    IPC_BEGIN_MESSAGE_MAP(QuitListener, message)
      IPC_MESSAGE_HANDLER(WorkerMsg_Quit, OnQuit)
      IPC_MESSAGE_HANDLER(TestMsg_BadMessage, OnBadMessage)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  virtual void OnBadMessageReceived(const IPC::Message& message) OVERRIDE {
    bad_message_received_ = true;
  }

  void OnQuit() {
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void OnBadMessage(const BadType& bad_type) {
    // Should never be called since IPC wouldn't be deserialized correctly.
    CHECK(false);
  }

  bool bad_message_received_;
};

class ChannelReflectorListener : public IPC::Listener {
 public:
  ChannelReflectorListener() : channel_(NULL) {}
  virtual ~ChannelReflectorListener() {}

  void Init(IPC::Channel* channel) {
    DCHECK(!channel_);
    channel_ = channel;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    IPC_BEGIN_MESSAGE_MAP(ChannelReflectorListener, message)
      IPC_MESSAGE_HANDLER(TestMsg_Bounce, OnTestBounce)
      IPC_MESSAGE_HANDLER(TestMsg_SendBadMessage, OnSendBadMessage)
      IPC_MESSAGE_HANDLER(UtilityMsg_Bounce, OnUtilityBounce)
      IPC_MESSAGE_HANDLER(WorkerMsg_Bounce, OnBounce)
      IPC_MESSAGE_HANDLER(WorkerMsg_Quit, OnQuit)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnTestBounce() {
    channel_->Send(new TestMsg_Bounce());
  }

  void OnSendBadMessage() {
    channel_->Send(new TestMsg_BadMessage(BadType()));
  }

  void OnUtilityBounce() {
    channel_->Send(new UtilityMsg_Bounce());
  }

  void OnBounce() {
    channel_->Send(new WorkerMsg_Bounce());
  }

  void OnQuit() {
    channel_->Send(new WorkerMsg_Quit());
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  IPC::Channel* channel_;
};

class MessageCountFilter : public IPC::MessageFilter {
 public:
  enum FilterEvent {
    NONE,
    FILTER_ADDED,
    CHANNEL_CONNECTED,
    CHANNEL_ERROR,
    CHANNEL_CLOSING,
    FILTER_REMOVED
  };
  MessageCountFilter()
      : messages_received_(0),
        supported_message_class_(0),
        is_global_filter_(true),
        last_filter_event_(NONE),
        message_filtering_enabled_(false) {}

  MessageCountFilter(uint32 supported_message_class)
      : messages_received_(0),
        supported_message_class_(supported_message_class),
        is_global_filter_(false),
        last_filter_event_(NONE),
        message_filtering_enabled_(false) {}

  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE {
    EXPECT_TRUE(sender);
    EXPECT_EQ(NONE, last_filter_event_);
    last_filter_event_ = FILTER_ADDED;
  }

  virtual void OnChannelConnected(int32_t peer_pid) OVERRIDE {
    EXPECT_EQ(FILTER_ADDED, last_filter_event_);
    EXPECT_NE(static_cast<int32_t>(base::kNullProcessId), peer_pid);
    last_filter_event_ = CHANNEL_CONNECTED;
  }

  virtual void OnChannelError() OVERRIDE {
    EXPECT_EQ(CHANNEL_CONNECTED, last_filter_event_);
    last_filter_event_ = CHANNEL_ERROR;
  }

  virtual void OnChannelClosing() OVERRIDE {
    // We may or may not have gotten OnChannelError; if not, the last event has
    // to be OnChannelConnected.
    if (last_filter_event_ != CHANNEL_ERROR)
      EXPECT_EQ(CHANNEL_CONNECTED, last_filter_event_);
    last_filter_event_ = CHANNEL_CLOSING;
  }

  virtual void OnFilterRemoved() OVERRIDE {
    // If the channel didn't get a chance to connect, we might see the
    // OnFilterRemoved event with no other events preceding it. We still want
    // OnFilterRemoved to be called to allow for deleting the Filter.
    if (last_filter_event_ != NONE)
      EXPECT_EQ(CHANNEL_CLOSING, last_filter_event_);
    last_filter_event_ = FILTER_REMOVED;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    // We should always get the OnFilterAdded and OnChannelConnected events
    // prior to any messages.
    EXPECT_EQ(CHANNEL_CONNECTED, last_filter_event_);

    if (!is_global_filter_) {
      EXPECT_EQ(supported_message_class_, IPC_MESSAGE_CLASS(message));
    }
    ++messages_received_;

    if (!message_filtering_enabled_)
      return false;

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MessageCountFilter, message)
      IPC_MESSAGE_HANDLER(TestMsg_BadMessage, OnBadMessage)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnBadMessage(const BadType& bad_type) {
    // Should never be called since IPC wouldn't be deserialized correctly.
    CHECK(false);
  }

  virtual bool GetSupportedMessageClasses(
      std::vector<uint32>* supported_message_classes) const OVERRIDE {
    if (is_global_filter_)
      return false;
    supported_message_classes->push_back(supported_message_class_);
    return true;
  }

  void set_message_filtering_enabled(bool enabled) {
    message_filtering_enabled_ = enabled;
  }

  size_t messages_received() const { return messages_received_; }
  FilterEvent last_filter_event() const { return last_filter_event_; }

 private:
  virtual ~MessageCountFilter() {}

  size_t messages_received_;
  uint32 supported_message_class_;
  bool is_global_filter_;

  FilterEvent last_filter_event_;
  bool message_filtering_enabled_;
};

class IPCChannelProxyTest : public IPCTestBase {
 public:
  IPCChannelProxyTest() {}
  virtual ~IPCChannelProxyTest() {}

  virtual void SetUp() OVERRIDE {
    IPCTestBase::SetUp();

    Init("ChannelProxyClient");

    thread_.reset(new base::Thread("ChannelProxyTestServerThread"));
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    thread_->StartWithOptions(options);

    listener_.reset(new QuitListener());
    CreateChannelProxy(listener_.get(), thread_->message_loop_proxy().get());

    ASSERT_TRUE(StartClient());
  }

  virtual void TearDown() {
    DestroyChannelProxy();
    thread_.reset();
    listener_.reset();
    IPCTestBase::TearDown();
  }

  void SendQuitMessageAndWaitForIdle() {
    sender()->Send(new WorkerMsg_Quit);
    base::MessageLoop::current()->Run();
    EXPECT_TRUE(WaitForClientShutdown());
  }

  bool DidListenerGetBadMessage() {
    return listener_->bad_message_received_;
  }

 private:
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<QuitListener> listener_;
};

TEST_F(IPCChannelProxyTest, MessageClassFilters) {
  // Construct a filter per message class.
  std::vector<scoped_refptr<MessageCountFilter> > class_filters;
  class_filters.push_back(make_scoped_refptr(
      new MessageCountFilter(TestMsgStart)));
  class_filters.push_back(make_scoped_refptr(
      new MessageCountFilter(UtilityMsgStart)));
  for (size_t i = 0; i < class_filters.size(); ++i)
    channel_proxy()->AddFilter(class_filters[i].get());

  // Send a message for each class; each filter should receive just one message.
  sender()->Send(new TestMsg_Bounce());
  sender()->Send(new UtilityMsg_Bounce());

  // Send some messages not assigned to a specific or valid message class.
  sender()->Send(new WorkerMsg_Bounce);

  // Each filter should have received just the one sent message of the
  // corresponding class.
  SendQuitMessageAndWaitForIdle();
  for (size_t i = 0; i < class_filters.size(); ++i)
    EXPECT_EQ(1U, class_filters[i]->messages_received());
}

TEST_F(IPCChannelProxyTest, GlobalAndMessageClassFilters) {
  // Add a class and global filter.
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  class_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(class_filter.get());

  scoped_refptr<MessageCountFilter> global_filter(new MessageCountFilter());
  global_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(global_filter.get());

  // A message  of class Test should be seen by both the global filter and
  // Test-specific filter.
  sender()->Send(new TestMsg_Bounce);

  // A message of a different class should be seen only by the global filter.
  sender()->Send(new UtilityMsg_Bounce);

  // Flush all messages.
  SendQuitMessageAndWaitForIdle();

  // The class filter should have received only the class-specific message.
  EXPECT_EQ(1U, class_filter->messages_received());

  // The global filter should have received both messages, as well as the final
  // QUIT message.
  EXPECT_EQ(3U, global_filter->messages_received());
}

TEST_F(IPCChannelProxyTest, FilterRemoval) {
  // Add a class and global filter.
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  scoped_refptr<MessageCountFilter> global_filter(new MessageCountFilter());

  // Add and remove both types of filters.
  channel_proxy()->AddFilter(class_filter.get());
  channel_proxy()->AddFilter(global_filter.get());
  channel_proxy()->RemoveFilter(global_filter.get());
  channel_proxy()->RemoveFilter(class_filter.get());

  // Send some messages; they should not be seen by either filter.
  sender()->Send(new TestMsg_Bounce);
  sender()->Send(new UtilityMsg_Bounce);

  // Ensure that the filters were removed and did not receive any messages.
  SendQuitMessageAndWaitForIdle();
  EXPECT_EQ(MessageCountFilter::FILTER_REMOVED,
            global_filter->last_filter_event());
  EXPECT_EQ(MessageCountFilter::FILTER_REMOVED,
            class_filter->last_filter_event());
  EXPECT_EQ(0U, class_filter->messages_received());
  EXPECT_EQ(0U, global_filter->messages_received());
}

// The test that follow trigger DCHECKS in debug build.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)

TEST_F(IPCChannelProxyTest, BadMessageOnListenerThread) {
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  class_filter->set_message_filtering_enabled(false);
  channel_proxy()->AddFilter(class_filter.get());

  sender()->Send(new TestMsg_SendBadMessage());

  SendQuitMessageAndWaitForIdle();
  EXPECT_TRUE(DidListenerGetBadMessage());
}

TEST_F(IPCChannelProxyTest, BadMessageOnIPCThread) {
  scoped_refptr<MessageCountFilter> class_filter(
      new MessageCountFilter(TestMsgStart));
  class_filter->set_message_filtering_enabled(true);
  channel_proxy()->AddFilter(class_filter.get());

  sender()->Send(new TestMsg_SendBadMessage());

  SendQuitMessageAndWaitForIdle();
  EXPECT_TRUE(DidListenerGetBadMessage());
}

class IPCChannelBadMessageTest : public IPCTestBase {
 public:
  IPCChannelBadMessageTest() {}
  virtual ~IPCChannelBadMessageTest() {}

  virtual void SetUp() OVERRIDE {
    IPCTestBase::SetUp();

    Init("ChannelProxyClient");

    listener_.reset(new QuitListener());
    CreateChannel(listener_.get());
    ASSERT_TRUE(ConnectChannel());

    ASSERT_TRUE(StartClient());
  }

  virtual void TearDown() {
    listener_.reset();
    IPCTestBase::TearDown();
  }

  void SendQuitMessageAndWaitForIdle() {
    sender()->Send(new WorkerMsg_Quit);
    base::MessageLoop::current()->Run();
    EXPECT_TRUE(WaitForClientShutdown());
  }

  bool DidListenerGetBadMessage() {
    return listener_->bad_message_received_;
  }

 private:
  scoped_ptr<QuitListener> listener_;
};

#if !defined(OS_WIN)
  // TODO(jam): for some reason this is flaky on win buildbots.
TEST_F(IPCChannelBadMessageTest, BadMessage) {
  sender()->Send(new TestMsg_SendBadMessage());
  SendQuitMessageAndWaitForIdle();
  EXPECT_TRUE(DidListenerGetBadMessage());
}
#endif

#endif

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(ChannelProxyClient) {
  base::MessageLoopForIO main_message_loop;
  ChannelReflectorListener listener;
  scoped_ptr<IPC::Channel> channel(IPC::Channel::CreateClient(
      IPCTestBase::GetChannelName("ChannelProxyClient"),
      &listener));
  CHECK(channel->Connect());
  listener.Init(channel.get());

  base::MessageLoop::current()->Run();
  return 0;
}

}  // namespace

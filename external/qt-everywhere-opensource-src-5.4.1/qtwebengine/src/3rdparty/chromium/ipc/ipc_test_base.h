// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_TEST_BASE_H_
#define IPC_IPC_TEST_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_multiprocess_test.h"

namespace base {
class MessageLoopForIO;
}

// A test fixture for multiprocess IPC tests. Such tests include a "client" side
// (running in a separate process). The same client may be shared between
// several different tests.
class IPCTestBase : public base::MultiProcessTest {
 public:
  // The channel name is based on the client's name. This is a public static
  // helper to be used by the client-side code; server-side test code should
  // usually not use this (directly).
  static std::string GetChannelName(const std::string& test_client_name);

 protected:
  IPCTestBase();
  virtual ~IPCTestBase();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Initializes the test to use the given client.
  void Init(const std::string& test_client_name);

  // Creates a channel with the given listener and connects to the channel
  // (returning true if successful), respectively. Use these to use a channel
  // directly. Since the listener must outlive the channel, you must destroy the
  // channel before the listener gets destroyed.
  void CreateChannel(IPC::Listener* listener);
  bool ConnectChannel();
  void DestroyChannel();

  // Use this instead of CreateChannel() if you want to use some different
  // channel specification (then use ConnectChannel() as usual).
  void CreateChannelFromChannelHandle(const IPC::ChannelHandle& channel_handle,
                                      IPC::Listener* listener);

  // Creates a channel proxy with the given listener and task runner. (The
  // channel proxy will automatically create and connect a channel.) You must
  // (manually) destroy the channel proxy before the task runner's thread is
  // destroyed.
  void CreateChannelProxy(IPC::Listener* listener,
                          base::SingleThreadTaskRunner* ipc_task_runner);
  void DestroyChannelProxy();

  // Starts the client process, returning true if successful; this should be
  // done after connecting to the channel.
  bool StartClient();

  // Waits for the client to shut down, returning true if successful. Note that
  // this does not initiate client shutdown; that must be done by the test
  // (somehow). This must be called before the end of the test whenever
  // StartClient() was called successfully.
  bool WaitForClientShutdown();

  // Use this to send IPC messages (when you don't care if you're using a
  // channel or a proxy).
  IPC::Sender* sender() {
    return channel_.get() ? static_cast<IPC::Sender*>(channel_.get()) :
                            static_cast<IPC::Sender*>(channel_proxy_.get());
  }

  IPC::Channel* channel() { return channel_.get(); }
  IPC::ChannelProxy* channel_proxy() { return channel_proxy_.get(); }

  const base::ProcessHandle& client_process() const { return client_process_; }

 private:
  std::string test_client_name_;
  scoped_ptr<base::MessageLoopForIO> message_loop_;

  scoped_ptr<IPC::Channel> channel_;
  scoped_ptr<IPC::ChannelProxy> channel_proxy_;

  base::ProcessHandle client_process_;

  DISALLOW_COPY_AND_ASSIGN(IPCTestBase);
};

// Use this to declare the client side for tests using IPCTestBase.
#define MULTIPROCESS_IPC_TEST_CLIENT_MAIN(test_client_name) \
    MULTIPROCESS_IPC_TEST_MAIN(test_client_name ## TestClientMain)

#endif  // IPC_IPC_TEST_BASE_H_

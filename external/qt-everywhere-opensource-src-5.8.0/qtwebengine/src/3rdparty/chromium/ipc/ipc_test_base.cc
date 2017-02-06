// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_test_base.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/process/kill.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ipc/ipc_descriptors.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#endif

// static
std::string IPCTestBase::GetChannelName(const std::string& test_client_name) {
  DCHECK(!test_client_name.empty());
  return test_client_name + "__Channel";
}

IPCTestBase::IPCTestBase() {
}

IPCTestBase::~IPCTestBase() {
}

void IPCTestBase::TearDown() {
  message_loop_.reset();
  MultiProcessTest::TearDown();
}

void IPCTestBase::Init(const std::string& test_client_name) {
  InitWithCustomMessageLoop(test_client_name,
                            base::WrapUnique(new base::MessageLoopForIO()));
}

void IPCTestBase::InitWithCustomMessageLoop(
    const std::string& test_client_name,
    std::unique_ptr<base::MessageLoop> message_loop) {
  DCHECK(!test_client_name.empty());
  DCHECK(test_client_name_.empty());
  DCHECK(!message_loop_);

  test_client_name_ = test_client_name;
  message_loop_ = std::move(message_loop);
}

void IPCTestBase::CreateChannel(IPC::Listener* listener) {
  CreateChannelFromChannelHandle(GetTestChannelHandle(), listener);
}

bool IPCTestBase::ConnectChannel() {
  CHECK(channel_.get());
  return channel_->Connect();
}

std::unique_ptr<IPC::Channel> IPCTestBase::ReleaseChannel() {
  return std::move(channel_);
}

void IPCTestBase::SetChannel(std::unique_ptr<IPC::Channel> channel) {
  channel_ = std::move(channel);
}


void IPCTestBase::DestroyChannel() {
  DCHECK(channel_.get());
  channel_.reset();
}

void IPCTestBase::CreateChannelFromChannelHandle(
    const IPC::ChannelHandle& channel_handle,
    IPC::Listener* listener) {
  CHECK(!channel_.get());
  CHECK(!channel_proxy_.get());
  channel_ = CreateChannelFactory(
      channel_handle, task_runner().get())->BuildChannel(listener);
}

void IPCTestBase::CreateChannelProxy(
    IPC::Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  CHECK(!channel_.get());
  CHECK(!channel_proxy_.get());
  channel_proxy_ = IPC::ChannelProxy::Create(
      CreateChannelFactory(GetTestChannelHandle(), ipc_task_runner.get()),
      listener, ipc_task_runner);
}

void IPCTestBase::DestroyChannelProxy() {
  CHECK(channel_proxy_.get());
  channel_proxy_.reset();
}

std::string IPCTestBase::GetTestMainName() const {
  return test_client_name_ + "TestClientMain";
}

bool IPCTestBase::DidStartClient() {
  DCHECK(client_process_.IsValid());
  return client_process_.IsValid();
}

#if defined(OS_POSIX)

bool IPCTestBase::StartClient() {
  return StartClientWithFD(channel_
                               ? channel_->GetClientFileDescriptor()
                               : channel_proxy_->GetClientFileDescriptor());
}

bool IPCTestBase::StartClientWithFD(int ipcfd) {
  DCHECK(!client_process_.IsValid());

  base::FileHandleMappingVector fds_to_map;
  if (ipcfd > -1)
    fds_to_map.push_back(std::pair<int, int>(ipcfd,
        kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
  base::LaunchOptions options;
  options.fds_to_remap = &fds_to_map;
  client_process_ = SpawnChildWithOptions(GetTestMainName(), options);

  return DidStartClient();
}

#elif defined(OS_WIN)

bool IPCTestBase::StartClient() {
  DCHECK(!client_process_.IsValid());
  client_process_ = SpawnChild(GetTestMainName());
  return DidStartClient();
}

#endif

bool IPCTestBase::WaitForClientShutdown() {
  DCHECK(client_process_.IsValid());

  int exit_code;
#if defined(OS_ANDROID)
  bool rv = AndroidWaitForChildExitWithTimeout(
      client_process_, base::TimeDelta::FromSeconds(5), &exit_code);
#else
  bool rv = client_process_.WaitForExitWithTimeout(
      base::TimeDelta::FromSeconds(5), &exit_code);
#endif  // defined(OS_ANDROID)
  client_process_.Close();
  return rv;
}

IPC::ChannelHandle IPCTestBase::GetTestChannelHandle() {
  return GetChannelName(test_client_name_);
}

scoped_refptr<base::SequencedTaskRunner> IPCTestBase::task_runner() {
  return message_loop_->task_runner();
}

std::unique_ptr<IPC::ChannelFactory> IPCTestBase::CreateChannelFactory(
    const IPC::ChannelHandle& handle,
    base::SequencedTaskRunner* runner) {
  return IPC::ChannelFactory::Create(handle, IPC::Channel::MODE_SERVER);
}

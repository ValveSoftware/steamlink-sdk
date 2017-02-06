// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"
#include "content/child/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/utility_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/utility/content_utility_client.h"
#include "content/utility/utility_blink_platform_impl.h"
#include "content/utility/utility_process_control_impl.h"
#include "ipc/ipc_sync_channel.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(OS_POSIX) && defined(ENABLE_PLUGINS)
#include "base/files/file_path.h"
#include "content/common/plugin_list.h"
#endif

namespace content {

namespace {

template<typename SRC, typename DEST>
void ConvertVector(const SRC& src, DEST* dest) {
  dest->reserve(src.size());
  for (typename SRC::const_iterator i = src.begin(); i != src.end(); ++i)
    dest->push_back(typename DEST::value_type(*i));
}

}  // namespace

UtilityThreadImpl::UtilityThreadImpl()
    : ChildThreadImpl(ChildThreadImpl::Options::Builder().Build()) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const InProcessChildThreadParams& params)
    : ChildThreadImpl(ChildThreadImpl::Options::Builder()
                          .InBrowserProcess(params)
                          .UseMojoChannel(true)
                          .Build()) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() {
}

void UtilityThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();

  if (blink_platform_impl_)
    blink::Platform::shutdown();
}

void UtilityThreadImpl::ReleaseProcessIfNeeded() {
  if (batch_mode_)
    return;

  if (IsInBrowserProcess()) {
    // Close the channel to cause UtilityProcessHostImpl to be deleted. We need
    // to take a different code path than the multi-process case because that
    // depends on the child process going away to close the channel, but that
    // can't happen when we're in single process mode.
    channel()->Close();
  } else {
    ChildProcess::current()->ReleaseProcess();
  }
}

void UtilityThreadImpl::EnsureBlinkInitialized() {
  if (blink_platform_impl_ || IsInBrowserProcess()) {
    // We can only initialize WebKit on one thread, and in single process mode
    // we run the utility thread on separate thread. This means that if any code
    // needs WebKit initialized in the utility process, they need to have
    // another path to support single process mode.
    return;
  }

  blink_platform_impl_.reset(new UtilityBlinkPlatformImpl);
  blink::Platform::initialize(blink_platform_impl_.get());
}

void UtilityThreadImpl::Init() {
  batch_mode_ = false;
  ChildProcess::current()->AddRefProcess();
  GetContentClient()->utility()->UtilityThreadStarted();

  process_control_.reset(new UtilityProcessControlImpl);
  GetInterfaceRegistry()->AddInterface(base::Bind(
      &UtilityThreadImpl::BindProcessControlRequest, base::Unretained(this)));

  GetContentClient()->utility()->ExposeInterfacesToBrowser(
      GetInterfaceRegistry());
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  if (GetContentClient()->utility()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityThreadImpl, msg)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Started, OnBatchModeStarted)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Finished, OnBatchModeFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UtilityThreadImpl::OnBatchModeStarted() {
  batch_mode_ = true;
}

void UtilityThreadImpl::OnBatchModeFinished() {
  batch_mode_ = false;
  ReleaseProcessIfNeeded();
}

void UtilityThreadImpl::BindProcessControlRequest(
    mojo::InterfaceRequest<mojom::ProcessControl> request) {
  DCHECK(process_control_);
  process_control_bindings_.AddBinding(process_control_.get(),
                                       std::move(request));
}

}  // namespace content

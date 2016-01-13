// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "content/child/blink_platform_impl.h"
#include "content/child/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/plugin_list.h"
#include "content/common/utility_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/utility/content_utility_client.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

namespace {

template<typename SRC, typename DEST>
void ConvertVector(const SRC& src, DEST* dest) {
  dest->reserve(src.size());
  for (typename SRC::const_iterator i = src.begin(); i != src.end(); ++i)
    dest->push_back(typename DEST::value_type(*i));
}

}  // namespace

UtilityThreadImpl::UtilityThreadImpl() : single_process_(false) {
  Init();
}

UtilityThreadImpl::UtilityThreadImpl(const std::string& channel_name)
    : ChildThread(channel_name),
      single_process_(true) {
  Init();
}

UtilityThreadImpl::~UtilityThreadImpl() {
}

void UtilityThreadImpl::Shutdown() {
  ChildThread::Shutdown();

  if (!single_process_)
    blink::shutdown();
}

bool UtilityThreadImpl::Send(IPC::Message* msg) {
  return ChildThread::Send(msg);
}

void UtilityThreadImpl::ReleaseProcessIfNeeded() {
  if (batch_mode_)
    return;

  if (single_process_) {
    // Close the channel to cause UtilityProcessHostImpl to be deleted. We need
    // to take a different code path than the multi-process case because that
    // depends on the child process going away to close the channel, but that
    // can't happen when we're in single process mode.
    channel()->Close();
  } else {
    ChildProcess::current()->ReleaseProcess();
  }
}

#if defined(OS_WIN)

void UtilityThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void UtilityThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}

#endif  // OS_WIN

void UtilityThreadImpl::Init() {
  batch_mode_ = false;
  ChildProcess::current()->AddRefProcess();
  if (!single_process_) {
    // We can only initialize WebKit on one thread, and in single process mode
    // we run the utility thread on separate thread. This means that if any code
    // needs WebKit initialized in the utility process, they need to have
    // another path to support single process mode.
    webkit_platform_support_.reset(new BlinkPlatformImpl);
    blink::initialize(webkit_platform_support_.get());
  }
  GetContentClient()->utility()->UtilityThreadStarted();
}

bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  if (GetContentClient()->utility()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityThreadImpl, msg)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Started, OnBatchModeStarted)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Finished, OnBatchModeFinished)
#if defined(OS_POSIX)
    IPC_MESSAGE_HANDLER(UtilityMsg_LoadPlugins, OnLoadPlugins)
#endif  // OS_POSIX
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

#if defined(OS_POSIX)
void UtilityThreadImpl::OnLoadPlugins(
    const std::vector<base::FilePath>& plugin_paths) {
  PluginList* plugin_list = PluginList::Singleton();

  std::vector<WebPluginInfo> plugins;
  // TODO(bauerb): If we restart loading plug-ins, we might mess up the logic in
  // PluginList::ShouldLoadPlugin due to missing the previously loaded plug-ins
  // in |plugin_groups|.
  for (size_t i = 0; i < plugin_paths.size(); ++i) {
    WebPluginInfo plugin;
    if (!plugin_list->LoadPluginIntoPluginList(
        plugin_paths[i], &plugins, &plugin))
      Send(new UtilityHostMsg_LoadPluginFailed(i, plugin_paths[i]));
    else
      Send(new UtilityHostMsg_LoadedPlugin(i, plugin));
  }

  ReleaseProcessIfNeeded();
}
#endif

}  // namespace content

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_loader_posix.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/plugin_list.h"
#include "content/common/utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"

namespace content {

PluginLoaderPosix::PluginLoaderPosix()
    : next_load_index_(0) {
}

void PluginLoaderPosix::GetPlugins(
    const PluginService::GetPluginsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::vector<WebPluginInfo> cached_plugins;
  if (PluginList::Singleton()->GetPluginsNoRefresh(&cached_plugins)) {
    // Can't assume the caller is reentrant.
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, cached_plugins));
    return;
  }

  if (callbacks_.empty()) {
    callbacks_.push_back(callback);

    PluginList::Singleton()->PrepareForPluginLoading();

    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PluginLoaderPosix::GetPluginsToLoad,
                                       make_scoped_refptr(this)));
  } else {
    // If we are currently loading plugins, the plugin list might have been
    // invalidated in the mean time, or might get invalidated before we finish.
    // We'll wait until we have finished the current run, then try to get them
    // again from the plugin list. If it has indeed been invalidated, it will
    // restart plugin loading, otherwise it will immediately run the callback.
    callbacks_.push_back(base::Bind(&PluginLoaderPosix::GetPluginsWrapper,
                                    make_scoped_refptr(this), callback));
  }
}

bool PluginLoaderPosix::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginLoaderPosix, message)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_LoadedPlugin, OnPluginLoaded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_LoadPluginFailed, OnPluginLoadFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PluginLoaderPosix::OnProcessCrashed(int exit_code) {
  RecordAction(
      base::UserMetricsAction("PluginLoaderPosix.UtilityProcessCrashed"));

  if (next_load_index_ == canonical_list_.size()) {
    // How this case occurs is unknown. See crbug.com/111935.
    canonical_list_.clear();
  } else {
    canonical_list_.erase(canonical_list_.begin(),
                          canonical_list_.begin() + next_load_index_ + 1);
  }

  next_load_index_ = 0;

  LoadPluginsInternal();
}

bool PluginLoaderPosix::Send(IPC::Message* message) {
  if (process_host_.get())
    return process_host_->Send(message);
  return false;
}

PluginLoaderPosix::~PluginLoaderPosix() {
}

void PluginLoaderPosix::GetPluginsToLoad() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::TimeTicks start_time(base::TimeTicks::Now());

  loaded_plugins_.clear();
  next_load_index_ = 0;

  canonical_list_.clear();
  PluginList::Singleton()->GetPluginPathsToLoad(
      &canonical_list_,
      PluginService::GetInstance()->NPAPIPluginsSupported());

  internal_plugins_.clear();
  PluginList::Singleton()->GetInternalPlugins(&internal_plugins_);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PluginLoaderPosix::LoadPluginsInternal,
                 make_scoped_refptr(this)));

  HISTOGRAM_TIMES("PluginLoaderPosix.GetPluginList",
                  (base::TimeTicks::Now() - start_time) *
                      base::Time::kMicrosecondsPerMillisecond);
}

void PluginLoaderPosix::LoadPluginsInternal() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Check if the list is empty or all plugins have already been loaded before
  // forking.
  if (MaybeRunPendingCallbacks())
    return;

  RecordAction(
      base::UserMetricsAction("PluginLoaderPosix.LaunchUtilityProcess"));

  if (load_start_time_.is_null())
    load_start_time_ = base::TimeTicks::Now();

  UtilityProcessHostImpl* host = new UtilityProcessHostImpl(
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get());
  process_host_ = host->AsWeakPtr();
  process_host_->DisableSandbox();
#if defined(OS_MACOSX)
  host->set_child_flags(ChildProcessHost::CHILD_ALLOW_HEAP_EXECUTION);
#endif

  process_host_->Send(new UtilityMsg_LoadPlugins(canonical_list_));
}

void PluginLoaderPosix::GetPluginsWrapper(
    const PluginService::GetPluginsCallback& callback,
    const std::vector<WebPluginInfo>& plugins_unused) {
  // We are being called after plugin loading has finished, but we don't know
  // whether the plugin list has been invalidated in the mean time
  // (and therefore |plugins| might already be stale). So we simply ignore it
  // and call regular GetPlugins() instead.
  GetPlugins(callback);
}

void PluginLoaderPosix::OnPluginLoaded(uint32 index,
                                       const WebPluginInfo& plugin) {
  if (index != next_load_index_) {
    LOG(ERROR) << "Received unexpected plugin load message for "
               << plugin.path.value() << "; index=" << index;
    return;
  }

  if (!MaybeAddInternalPlugin(plugin.path))
    loaded_plugins_.push_back(plugin);

  ++next_load_index_;

  MaybeRunPendingCallbacks();
}

void PluginLoaderPosix::OnPluginLoadFailed(uint32 index,
                                           const base::FilePath& plugin_path) {
  if (index != next_load_index_) {
    LOG(ERROR) << "Received unexpected plugin load failure message for "
               << plugin_path.value() << "; index=" << index;
    return;
  }

  ++next_load_index_;

  MaybeAddInternalPlugin(plugin_path);
  MaybeRunPendingCallbacks();
}

bool PluginLoaderPosix::MaybeAddInternalPlugin(
    const base::FilePath& plugin_path) {
  for (std::vector<WebPluginInfo>::iterator it = internal_plugins_.begin();
       it != internal_plugins_.end();
       ++it) {
    if (it->path == plugin_path) {
      loaded_plugins_.push_back(*it);
      internal_plugins_.erase(it);
      return true;
    }
  }
  return false;
}

bool PluginLoaderPosix::MaybeRunPendingCallbacks() {
  if (next_load_index_ < canonical_list_.size())
    return false;

  PluginList::Singleton()->SetPlugins(loaded_plugins_);

  for (std::vector<PluginService::GetPluginsCallback>::iterator it =
           callbacks_.begin();
       it != callbacks_.end(); ++it) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(*it, loaded_plugins_));
  }
  callbacks_.clear();

  HISTOGRAM_TIMES("PluginLoaderPosix.LoadDone",
                  (base::TimeTicks::Now() - load_start_time_)
                      * base::Time::kMicrosecondsPerMillisecond);
  load_start_time_ = base::TimeTicks();

  return true;
}

}  // namespace content

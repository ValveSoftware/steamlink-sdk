// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_PLUGIN_THREAD_H_
#define CONTENT_PLUGIN_PLUGIN_THREAD_H_

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "content/child/child_thread.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/plugin/plugin_channel.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace content {
class BlinkPlatformImpl;

// The PluginThread class represents a background thread where plugin instances
// live.  Communication occurs between WebPluginDelegateProxy in the renderer
// process and WebPluginDelegateStub in this thread through IPC messages.
class PluginThread : public ChildThread {
 public:
  PluginThread();
  virtual ~PluginThread();
  virtual void Shutdown() OVERRIDE;

  // Returns the one plugin thread.
  static PluginThread* current();

  // Tells the plugin thread to terminate the process forcefully instead of
  // exiting cleanly.
  void SetForcefullyTerminatePluginProcess();

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Callback for when a channel has been created.
  void OnCreateChannel(int renderer_id, bool incognito);
  void OnPluginMessage(const std::vector<uint8> &data);
  void OnNotifyRenderersOfPendingShutdown();
#if defined(OS_MACOSX)
  void OnAppActivated();
  void OnPluginFocusNotify(uint32 instance_id);
#endif

  // The plugin module which is preloaded in Init
  base::NativeLibrary preloaded_plugin_module_;

  bool forcefully_terminate_plugin_process_;

  scoped_ptr<BlinkPlatformImpl> webkit_platform_support_;

  DISALLOW_COPY_AND_ASSIGN(PluginThread);
};

}  // namespace content

#endif  // CONTENT_PLUGIN_PLUGIN_THREAD_H_

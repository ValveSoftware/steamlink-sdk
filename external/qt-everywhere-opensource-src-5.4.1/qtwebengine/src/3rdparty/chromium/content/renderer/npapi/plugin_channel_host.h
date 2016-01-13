// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NPAPI_PLUGIN_CHANNEL_HOST_H_
#define CONTENT_RENDERER_NPAPI_PLUGIN_CHANNEL_HOST_H_

#include "base/containers/hash_tables.h"
#include "content/child/npapi/np_channel_base.h"
#include "ipc/ipc_channel_handle.h"

namespace content {
class NPObjectBase;

// Encapsulates an IPC channel between the renderer and one plugin process.
// On the plugin side there's a corresponding PluginChannel.
class PluginChannelHost : public NPChannelBase {
 public:
#if defined(OS_MACOSX)
  // TODO(shess): Debugging for http://crbug.com/97285 .  See comment
  // in plugin_channel_host.cc.
  static bool* GetRemoveTrackingFlag();
#endif
  static PluginChannelHost* GetPluginChannelHost(
      const IPC::ChannelHandle& channel_handle,
      base::MessageLoopProxy* ipc_message_loop);

  virtual bool Init(base::MessageLoopProxy* ipc_message_loop,
                    bool create_pipe_now,
                    base::WaitableEvent* shutdown_event) OVERRIDE;

  virtual int GenerateRouteID() OVERRIDE;

  void AddRoute(int route_id, IPC::Listener* listener,
                NPObjectBase* npobject);
  void RemoveRoute(int route_id);

  // NPChannelBase override:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener override
  virtual void OnChannelError() OVERRIDE;

  static void Broadcast(IPC::Message* message) {
    NPChannelBase::Broadcast(message);
  }

  bool expecting_shutdown() { return expecting_shutdown_; }

 private:
  // Called on the render thread
  PluginChannelHost();
  virtual ~PluginChannelHost();

  static NPChannelBase* ClassFactory() { return new PluginChannelHost(); }

  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  void OnSetException(const std::string& message);
  void OnPluginShuttingDown();

  // Keep track of all the registered WebPluginDelegeProxies to
  // inform about OnChannelError
  typedef base::hash_map<int, IPC::Listener*> ProxyMap;
  ProxyMap proxies_;

  // True if we are expecting the plugin process to go away - in which case,
  // don't treat it as a crash.
  bool expecting_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(PluginChannelHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NPAPI_PLUGIN_CHANNEL_HOST_H_

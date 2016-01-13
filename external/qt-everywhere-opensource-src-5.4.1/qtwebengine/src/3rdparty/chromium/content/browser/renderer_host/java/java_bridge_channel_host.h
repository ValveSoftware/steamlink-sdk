// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_CHANNEL_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_CHANNEL_HOST_H_

#include "content/child/npapi/np_channel_base.h"

namespace content {

class JavaBridgeChannelHost : public NPChannelBase {
 public:
  static JavaBridgeChannelHost* GetJavaBridgeChannelHost(
      int renderer_id,
      base::MessageLoopProxy* ipc_message_loop);

  // A threadsafe function to generate a unique route ID. Used by the
  // JavaBridgeDispatcherHost on the UI thread and this class on the Java
  // Bridge's background thread.
  static int ThreadsafeGenerateRouteID();

  // NPChannelBase implementation:
  virtual int GenerateRouteID() OVERRIDE;

  // NPChannelBase override:
  virtual bool Init(base::MessageLoopProxy* ipc_message_loop,
                    bool create_pipe_now,
                    base::WaitableEvent* shutdown_event) OVERRIDE;

 protected:
  // NPChannelBase override:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  JavaBridgeChannelHost() {}
  friend class base::RefCountedThreadSafe<JavaBridgeChannelHost>;
  virtual ~JavaBridgeChannelHost();

  static NPChannelBase* ClassFactory() {
    return new JavaBridgeChannelHost();
  }

  // Message handlers
  void OnGenerateRouteID(int* route_id);

  DISALLOW_COPY_AND_ASSIGN(JavaBridgeChannelHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BRIDGE_CHANNEL_HOST_H_

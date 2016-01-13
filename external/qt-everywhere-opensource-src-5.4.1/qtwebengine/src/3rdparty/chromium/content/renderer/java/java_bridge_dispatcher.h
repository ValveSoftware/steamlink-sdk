// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_JAVA_JAVA_BRIDGE_DISPATCHER_H_
#define CONTENT_RENDERER_JAVA_JAVA_BRIDGE_DISPATCHER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/public/renderer/render_frame_observer.h"
#include "ipc/ipc_channel_handle.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace content {
class JavaBridgeChannel;
struct NPVariant_Param;

// This class handles injecting Java objects into the main frame of a
// RenderView. The 'add' and 'remove' messages received from the browser
// process modify the entries in a map of 'pending' objects. These objects are
// bound to the window object of the main frame when that window object is next
// cleared. These objects remain bound until the window object is cleared
// again.
class JavaBridgeDispatcher : public RenderFrameObserver {
 public:
  JavaBridgeDispatcher(RenderFrame* render_frame);
  virtual ~JavaBridgeDispatcher();

 private:
  // RenderViewObserver override:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject() OVERRIDE;

  // Message handlers
  void OnAddNamedObject(const base::string16& name,
                        const NPVariant_Param& variant_param);
  void OnRemoveNamedObject(const base::string16& name);

  void EnsureChannelIsSetUp();

  // Objects that will be bound to the window when the window object is next
  // cleared. We hold a ref to these.
  typedef std::map<base::string16, NPVariant> ObjectMap;
  ObjectMap objects_;
  scoped_refptr<JavaBridgeChannel> channel_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_JAVA_JAVA_BRIDGE_DISPATCHER_H_

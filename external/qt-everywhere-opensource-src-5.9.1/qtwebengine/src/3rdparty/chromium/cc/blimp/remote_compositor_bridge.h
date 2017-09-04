// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_H_
#define CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "cc/base/cc_export.h"

namespace cc {

class CompositorProtoState;
class RemoteCompositorBridgeClient;

// The RemoteCompositorBridge is the remote LTH's communication bridge to the
// compositor on the client that consumes the CompositorProtoState updates and
// performs the actual compositing work. It is responsible for scheduling
// when a state update should be sent to the client and providing back
// mutations made to the state on the client.
class CC_EXPORT RemoteCompositorBridge {
 public:
  RemoteCompositorBridge(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner);

  virtual ~RemoteCompositorBridge();

  // Must be called exactly once before the RemoteCompositorBridge can be
  // used. Once bound, the client is expected to outlive this class.
  virtual void BindToClient(RemoteCompositorBridgeClient* client) = 0;

  // Notifies the bridge that a main frame update is required.
  virtual void ScheduleMainFrame() = 0;

  // If a main frame update results in any mutations to the compositor state,
  // the serialized compositor state is provided to the
  // RemoteCompositorBridge.
  virtual void ProcessCompositorStateUpdate(
      std::unique_ptr<CompositorProtoState> compositor_proto_state) = 0;

 protected:
  // The task runner for the compositor's main thread. The
  // RemoteCompositorBridgeClient must be called on this task runner.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCompositorBridge);
};

}  // namespace cc

#endif  // CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_H_

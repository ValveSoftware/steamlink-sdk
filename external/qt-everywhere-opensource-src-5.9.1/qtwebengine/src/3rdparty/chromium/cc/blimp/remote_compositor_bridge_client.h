// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_
#define CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_

#include <unordered_map>

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace gfx {
class ScrollOffset;
}  // namespace gfx

namespace cc {
namespace proto {
class ClientStateUpdate;
}  // namespace proto

class CC_EXPORT RemoteCompositorBridgeClient {
 public:
  using ScrollOffsetMap = std::unordered_map<int, gfx::ScrollOffset>;

  virtual ~RemoteCompositorBridgeClient() {}

  // Called in response to a ScheduleMainFrame request made on the
  // RemoteCompositorBridge.
  // Note: The method should always be invoked asynchronously after the request
  // is made.
  virtual void BeginMainFrame() = 0;

  // Applied state updates reported from the client onto the main thread state
  // on the engine.
  virtual void ApplyStateUpdateFromClient(
      const proto::ClientStateUpdate& client_state_update) = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_REMOTE_COMPOSITOR_BRIDGE_CLIENT_H_

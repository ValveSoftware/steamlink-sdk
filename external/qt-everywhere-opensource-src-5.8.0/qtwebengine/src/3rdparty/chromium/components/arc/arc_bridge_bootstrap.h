// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_BOOTSTRAP_H_
#define COMPONENTS_ARC_ARC_BRIDGE_BOOTSTRAP_H_

#include <memory>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/arc_bridge.mojom.h"

namespace arc {

// Starts the ARC instance and bootstraps the bridge connection.
// Clients should implement the Delegate to be notified upon communications
// being available.
class ArcBridgeBootstrap {
 public:
  class Delegate {
   public:
    // Called when the connection with ARC instance has been established.
    virtual void OnConnectionEstablished(
        mojom::ArcBridgeInstancePtr instance_ptr) = 0;

    // Called when ARC instance is stopped.
    virtual void OnStopped(ArcBridgeService::StopReason reason) = 0;
  };

  // Creates a default instance of ArcBridgeBootstrap.
  static std::unique_ptr<ArcBridgeBootstrap> Create();
  virtual ~ArcBridgeBootstrap() = default;

  // This must be called before calling Start() or Stop(). |delegate| is owned
  // by the caller and must outlive this instance.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Starts and bootstraps a connection with the instance. The Delegate's
  // OnConnectionEstablished() will be called if the bootstrapping is
  // successful, or OnStopped() if it is not.
  virtual void Start() = 0;

  // Stops the currently-running instance.
  virtual void Stop() = 0;

 protected:
  ArcBridgeBootstrap() = default;

  // Owned by the caller.
  Delegate* delegate_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcBridgeBootstrap);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_BOOTSTRAP_H_

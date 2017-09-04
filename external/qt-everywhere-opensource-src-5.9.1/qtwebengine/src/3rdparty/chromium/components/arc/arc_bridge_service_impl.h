// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_session.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace arc {

// Real IPC based ArcBridgeService that is used in production.
class ArcBridgeServiceImpl : public ArcBridgeService,
                             public ArcSession::Observer {
 public:
  // This is the factory interface to inject ArcSession instance
  // for testing purpose.
  using ArcSessionFactory = base::Callback<std::unique_ptr<ArcSession>()>;

  explicit ArcBridgeServiceImpl(
      const scoped_refptr<base::TaskRunner>& blocking_task_runner);
  ~ArcBridgeServiceImpl() override;

  // ArcBridgeService overrides:
  void RequestStart() override;
  void RequestStop() override;
  void OnShutdown() override;

  // Inject a factory to create ArcSession instance for testing purpose.
  // |factory| must not be null.
  void SetArcSessionFactoryForTesting(const ArcSessionFactory& factory);

  // Returns the current ArcSession instance for testing purpose.
  ArcSession* GetArcSessionForTesting() { return arc_session_.get(); }

  // Normally, reconnecting after connection shutdown happens after a short
  // delay. When testing, however, we'd like it to happen immediately to avoid
  // adding unnecessary delays.
  void DisableReconnectDelayForTesting();

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Restart);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, OnBridgeStopped);

  // If all pre-requisites are true (ARC is available, it has been enabled, and
  // the session has started), and ARC is stopped, start ARC. If ARC is running
  // and the pre-requisites stop being true, stop ARC.
  void PrerequisitesChanged();

  // Stops the running instance.
  void StopInstance();

  // ArcSession::Observer:
  void OnReady() override;
  void OnStopped(StopReason reason) override;

  std::unique_ptr<ArcSession> arc_session_;

  // If the user's session has started.
  bool session_started_;

  // If the instance had already been started but the connection to it was
  // lost. This should make the instance restart.
  bool reconnect_ = false;

  // Delay the reconnection.
  bool use_delay_before_reconnecting_ = true;

  // Factory to inject a fake ArcSession instance for testing.
  ArcSessionFactory factory_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeServiceImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_

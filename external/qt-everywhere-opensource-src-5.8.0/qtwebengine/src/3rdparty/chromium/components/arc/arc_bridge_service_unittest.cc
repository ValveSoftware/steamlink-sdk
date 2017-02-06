// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/test/fake_arc_bridge_bootstrap.h"
#include "components/arc/test/fake_arc_bridge_instance.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class ArcBridgeTest : public testing::Test, public ArcBridgeService::Observer {
 public:
  ArcBridgeTest() : ready_(false) {}
  ~ArcBridgeTest() override {}

  void OnStateChanged(ArcBridgeService::State state) override {
    state_ = state;
    switch (state) {
      case ArcBridgeService::State::READY:
        ready_ = true;
        break;

      case ArcBridgeService::State::STOPPED:
        message_loop_.PostTask(FROM_HERE, message_loop_.QuitWhenIdleClosure());
        break;

      default:
        break;
    }
  }

  void OnBridgeStopped(ArcBridgeService::StopReason stop_reason) override {
    stop_reason_ = stop_reason;
  }

  bool ready() const { return ready_; }
  ArcBridgeService::State state() const { return state_; }

 protected:
  std::unique_ptr<ArcBridgeServiceImpl> service_;
  std::unique_ptr<FakeArcBridgeInstance> instance_;
  ArcBridgeService::StopReason stop_reason_;

 private:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    ready_ = false;
    state_ = ArcBridgeService::State::STOPPED;
    stop_reason_ = ArcBridgeService::StopReason::SHUTDOWN;

    instance_.reset(new FakeArcBridgeInstance());
    service_.reset(new ArcBridgeServiceImpl(
        base::MakeUnique<FakeArcBridgeBootstrap>(instance_.get())));

    service_->AddObserver(this);
  }

  void TearDown() override {
    service_->RemoveObserver(this);
    instance_.reset();
    service_.reset();

    chromeos::DBusThreadManager::Shutdown();
  }

  bool ready_;
  ArcBridgeService::State state_;
  base::MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeTest);
};

class DummyObserver : public ArcBridgeService::Observer {};

}  // namespace

// Exercises the basic functionality of the ARC Bridge Service.  A message from
// within the instance should cause the observer to be notified.
TEST_F(ArcBridgeTest, Basic) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());

  service_->SetAvailable(true);
  service_->HandleStartup();
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If not all pre-requisites are met, the instance is not started.
TEST_F(ArcBridgeTest, Prerequisites) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
  service_->SetAvailable(true);
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
  service_->SetAvailable(false);
  service_->HandleStartup();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If the ArcBridgeService is shut down, it should be stopped, even
// mid-startup.
TEST_F(ArcBridgeTest, ShutdownMidStartup) {
  ASSERT_FALSE(ready());

  service_->SetAvailable(true);
  service_->HandleStartup();
  // WaitForInitCall() omitted.
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// If the instance is stopped, it should be re-started.
TEST_F(ArcBridgeTest, Restart) {
  ASSERT_FALSE(ready());
  ASSERT_EQ(0, instance_->init_calls());

  service_->SetAvailable(true);
  service_->HandleStartup();
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::State::READY, state());
  ASSERT_EQ(1, instance_->init_calls());

  // Simulate a connection loss.
  service_->DisableReconnectDelayForTesting();
  service_->OnChannelClosed();
  instance_->Stop(ArcBridgeService::StopReason::CRASH);
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::State::READY, state());
  ASSERT_EQ(2, instance_->init_calls());

  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// Makes sure OnBridgeStopped is called on stop.
TEST_F(ArcBridgeTest, OnBridgeStopped) {
  ASSERT_FALSE(ready());

  service_->DisableReconnectDelayForTesting();
  service_->SetAvailable(true);
  service_->HandleStartup();
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  // Simulate boot failure.
  service_->OnChannelClosed();
  instance_->Stop(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE);
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::StopReason::GENERIC_BOOT_FAILURE, stop_reason_);
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  // Simulate crash.
  service_->OnChannelClosed();
  instance_->Stop(ArcBridgeService::StopReason::CRASH);
  instance_->WaitForInitCall();
  ASSERT_EQ(ArcBridgeService::StopReason::CRASH, stop_reason_);
  ASSERT_EQ(ArcBridgeService::State::READY, state());

  // Graceful shutdown.
  service_->Shutdown();
  ASSERT_EQ(ArcBridgeService::StopReason::SHUTDOWN, stop_reason_);
  ASSERT_EQ(ArcBridgeService::State::STOPPED, state());
}

// Removing the same observer more than once should be okay.
TEST_F(ArcBridgeTest, RemoveObserverTwice) {
  ASSERT_FALSE(ready());
  service_->RemoveObserver(this);
  // The teardown method will also remove |this|.
}

// Removing an unknown observer should be allowed.
TEST_F(ArcBridgeTest, RemoveUnknownObserver) {
  ASSERT_FALSE(ready());
  auto dummy_observer = base::MakeUnique<DummyObserver>();
  service_->RemoveObserver(dummy_observer.get());
}

}  // namespace arc

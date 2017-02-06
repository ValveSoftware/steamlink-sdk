// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/arc/common/arc_bridge.mojom.h"
#include "components/arc/instance_holder.h"

namespace base {
class CommandLine;
}  // namespace base

namespace arc {

// The Chrome-side service that handles ARC instances and ARC bridge creation.
// This service handles the lifetime of ARC instances and sets up the
// communication channel (the ARC bridge) used to send and receive messages.
class ArcBridgeService : public mojom::ArcBridgeHost {
 public:
  // The possible states of the bridge.  In the normal flow, the state changes
  // in the following sequence:
  //
  // STOPPED
  //   PrerequisitesChanged() ->
  // CONNECTING
  //   OnConnectionEstablished() ->
  // READY
  //
  // The ArcBridgeBootstrap state machine can be thought of being substates of
  // ArcBridgeService's CONNECTING state.
  //
  // *
  //   StopInstance() ->
  // STOPPING
  //   OnStopped() ->
  // STOPPED
  enum class State {
    // ARC is not currently running.
    STOPPED,

    // The request to connect has been sent.
    CONNECTING,

    // The instance has started, and the bridge is fully established.
    CONNECTED,

    // The ARC instance has finished initializing and is now ready for user
    // interaction.
    READY,

    // The ARC instance has started shutting down.
    STOPPING,
  };

  // Describes the reason the bridge is stopped.
  enum class StopReason {
    // ARC instance has been gracefully shut down.
    SHUTDOWN,

    // Errors occurred during the ARC instance boot. This includes any failures
    // before the instance is actually attempted to be started, and also
    // failures on bootstrapping IPC channels with Android.
    GENERIC_BOOT_FAILURE,

    // The device is critically low on disk space.
    LOW_DISK_SPACE,

    // ARC instance has crashed.
    CRASH,
  };

  // Notifies life cycle events of ArcBridgeService.
  class Observer {
   public:
    // Called whenever the state of the bridge has changed.
    // TODO(lchavez): Rename to OnStateChangedForTest
    virtual void OnStateChanged(State state) {}
    virtual void OnBridgeReady() {}
    virtual void OnBridgeStopped(StopReason reason) {}

    // Called whenever ARC's availability has changed for this system.
    virtual void OnAvailableChanged(bool available) {}

   protected:
    virtual ~Observer() {}
  };

  ~ArcBridgeService() override;

  // Gets the global instance of the ARC Bridge Service. This can only be
  // called on the thread that this class was created on.
  static ArcBridgeService* Get();

  // Return true if ARC has been enabled through a commandline
  // switch.
  static bool GetEnabled(const base::CommandLine* command_line);

  // SetDetectedAvailability() should be called once CheckArcAvailability() on
  // the session_manager is called. This can only be called on the thread that
  // this class was created on.
  virtual void SetDetectedAvailability(bool availability) = 0;

  // HandleStartup() should be called upon profile startup.  This will only
  // launch an instance if the instance service is available and it is enabled.
  // This can only be called on the thread that this class was created on.
  virtual void HandleStartup() = 0;

  // Shutdown() should be called when the browser is shutting down. This can
  // only be called on the thread that this class was created on.
  virtual void Shutdown() = 0;

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on. RemoveObserver does nothing if |observer| is not in
  // the list.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  InstanceHolder<mojom::AppInstance>* app() { return &app_; }
  InstanceHolder<mojom::AudioInstance>* audio() { return &audio_; }
  InstanceHolder<mojom::AuthInstance>* auth() { return &auth_; }
  InstanceHolder<mojom::BluetoothInstance>* bluetooth() { return &bluetooth_; }
  InstanceHolder<mojom::ClipboardInstance>* clipboard() { return &clipboard_; }
  InstanceHolder<mojom::CrashCollectorInstance>* crash_collector() {
    return &crash_collector_;
  }
  InstanceHolder<mojom::EnterpriseReportingInstance>* enterprise_reporting() {
    return &enterprise_reporting_;
  }
  InstanceHolder<mojom::FileSystemInstance>* file_system() {
    return &file_system_;
  }
  InstanceHolder<mojom::ImeInstance>* ime() { return &ime_; }
  InstanceHolder<mojom::IntentHelperInstance>* intent_helper() {
    return &intent_helper_;
  }
  InstanceHolder<mojom::MetricsInstance>* metrics() { return &metrics_; }
  InstanceHolder<mojom::NetInstance>* net() { return &net_; }
  InstanceHolder<mojom::NotificationsInstance>* notifications() {
    return &notifications_;
  }
  InstanceHolder<mojom::ObbMounterInstance>* obb_mounter() {
    return &obb_mounter_;
  }
  InstanceHolder<mojom::PolicyInstance>* policy() { return &policy_; }
  InstanceHolder<mojom::PowerInstance>* power() { return &power_; }
  InstanceHolder<mojom::ProcessInstance>* process() { return &process_; }
  InstanceHolder<mojom::StorageManagerInstance>* storage_manager() {
    return &storage_manager_;
  }
  InstanceHolder<mojom::VideoInstance>* video() { return &video_; }
  InstanceHolder<mojom::WindowManagerInstance>* window_manager() {
    return &window_manager_;
  }

  // ArcHost:
  void OnAppInstanceReady(mojom::AppInstancePtr app_ptr) override;
  void OnAudioInstanceReady(mojom::AudioInstancePtr audio_ptr) override;
  void OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) override;
  void OnBluetoothInstanceReady(
      mojom::BluetoothInstancePtr bluetooth_ptr) override;
  void OnClipboardInstanceReady(
      mojom::ClipboardInstancePtr clipboard_ptr) override;
  void OnCrashCollectorInstanceReady(
      mojom::CrashCollectorInstancePtr crash_collector_ptr) override;
  void OnEnterpriseReportingInstanceReady(
      mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) override;
  void OnFileSystemInstanceReady(
      mojom::FileSystemInstancePtr file_system_ptr) override;
  void OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) override;
  void OnIntentHelperInstanceReady(
      mojom::IntentHelperInstancePtr intent_helper_ptr) override;
  void OnMetricsInstanceReady(mojom::MetricsInstancePtr metrics_ptr) override;
  void OnNetInstanceReady(mojom::NetInstancePtr net_ptr) override;
  void OnNotificationsInstanceReady(
      mojom::NotificationsInstancePtr notifications_ptr) override;
  void OnObbMounterInstanceReady(
      mojom::ObbMounterInstancePtr obb_mounter_ptr) override;
  void OnPolicyInstanceReady(mojom::PolicyInstancePtr policy_ptr) override;
  void OnPowerInstanceReady(mojom::PowerInstancePtr power_ptr) override;
  void OnProcessInstanceReady(mojom::ProcessInstancePtr process_ptr) override;
  void OnStorageManagerInstanceReady(
      mojom::StorageManagerInstancePtr storage_manager_ptr) override;
  void OnVideoInstanceReady(mojom::VideoInstancePtr video_ptr) override;
  void OnWindowManagerInstanceReady(
      mojom::WindowManagerInstancePtr window_manager_ptr) override;

  // Gets the current state of the bridge service.
  State state() const { return state_; }

  // Gets if ARC is available in this system.
  bool available() const { return available_; }

 protected:
  ArcBridgeService();

  // Changes the current state and notifies all observers.
  void SetState(State state);

  // Changes the current availability and notifies all observers.
  void SetAvailable(bool availability);

  // Sets the reason the bridge is stopped. This function must be always called
  // before SetState(State::STOPPED) to report a correct reason with
  // Observer::OnBridgeStopped().
  void SetStopReason(StopReason stop_reason);

  base::ObserverList<Observer>& observer_list() { return observer_list_; }

  bool CalledOnValidThread();

  // Closes all Mojo channels.
  void CloseAllChannels();

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Prerequisites);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, ShutdownMidStartup);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Restart);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, OnBridgeStopped);

  // Instance holders.
  InstanceHolder<mojom::AppInstance> app_;
  InstanceHolder<mojom::AudioInstance> audio_;
  InstanceHolder<mojom::AuthInstance> auth_;
  InstanceHolder<mojom::BluetoothInstance> bluetooth_;
  InstanceHolder<mojom::ClipboardInstance> clipboard_;
  InstanceHolder<mojom::CrashCollectorInstance> crash_collector_;
  InstanceHolder<mojom::EnterpriseReportingInstance> enterprise_reporting_;
  InstanceHolder<mojom::FileSystemInstance> file_system_;
  InstanceHolder<mojom::ImeInstance> ime_;
  InstanceHolder<mojom::IntentHelperInstance> intent_helper_;
  InstanceHolder<mojom::MetricsInstance> metrics_;
  InstanceHolder<mojom::NetInstance> net_;
  InstanceHolder<mojom::NotificationsInstance> notifications_;
  InstanceHolder<mojom::ObbMounterInstance> obb_mounter_;
  InstanceHolder<mojom::PolicyInstance> policy_;
  InstanceHolder<mojom::PowerInstance> power_;
  InstanceHolder<mojom::ProcessInstance> process_;
  InstanceHolder<mojom::StorageManagerInstance> storage_manager_;
  InstanceHolder<mojom::VideoInstance> video_;
  InstanceHolder<mojom::WindowManagerInstance> window_manager_;

  base::ObserverList<Observer> observer_list_;

  base::ThreadChecker thread_checker_;

  // If the ARC instance service is available.
  bool available_;

  // The current state of the bridge.
  ArcBridgeService::State state_;

  // The reason the bridge is stopped.
  StopReason stop_reason_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

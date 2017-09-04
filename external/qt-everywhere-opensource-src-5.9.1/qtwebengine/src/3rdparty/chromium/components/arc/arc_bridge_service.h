// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/arc/instance_holder.h"

namespace base {
class CommandLine;
}  // namespace base

namespace arc {

class ArcBridgeTest;

namespace mojom {

// Instead of including components/arc/common/arc_bridge.mojom.h, list all the
// instance classes here for faster build.
class AppInstance;
class AudioInstance;
class AuthInstance;
class BluetoothInstance;
class BootPhaseMonitorInstance;
class ClipboardInstance;
class CrashCollectorInstance;
class EnterpriseReportingInstance;
class FileSystemInstance;
class ImeInstance;
class IntentHelperInstance;
class KioskInstance;
class MetricsInstance;
class NetInstance;
class NotificationsInstance;
class ObbMounterInstance;
class PolicyInstance;
class PowerInstance;
class PrintInstance;
class ProcessInstance;
class StorageManagerInstance;
class TtsInstance;
class VideoInstance;
class WallpaperInstance;

}  // namespace mojom

// The Chrome-side service that handles ARC instances and ARC bridge creation.
// This service handles the lifetime of ARC instances and sets up the
// communication channel (the ARC bridge) used to send and receive messages.
class ArcBridgeService {
 public:
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
    virtual void OnBridgeReady() {}
    virtual void OnBridgeStopped(StopReason reason) {}

   protected:
    virtual ~Observer() {}
  };

  virtual ~ArcBridgeService();

  // Gets the global instance of the ARC Bridge Service. This can only be
  // called on the thread that this class was created on.
  static ArcBridgeService* Get();

  // Return true if ARC has been enabled through a commandline
  // switch.
  static bool GetEnabled(const base::CommandLine* command_line);

  // Return true if ARC is available on the current board.
  static bool GetAvailable(const base::CommandLine* command_line);

  // HandleStartup() should be called upon profile startup.  This will only
  // launch an instance if the instance is enabled.
  // This can only be called on the thread that this class was created on.

  // Starts the ARC service, then it will connect the Mojo channel. When the
  // bridge becomes ready, OnBridgeReady() is called.
  virtual void RequestStart() = 0;

  // Stops the ARC service.
  virtual void RequestStop() = 0;

  // OnShutdown() should be called when the browser is shutting down. This can
  // only be called on the thread that this class was created on. We assume that
  // when this function is called, MessageLoop is no longer exists.
  virtual void OnShutdown() = 0;

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on. RemoveObserver does nothing if |observer| is not in
  // the list.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  InstanceHolder<mojom::AppInstance>* app() { return &app_; }
  InstanceHolder<mojom::AudioInstance>* audio() { return &audio_; }
  InstanceHolder<mojom::AuthInstance>* auth() { return &auth_; }
  InstanceHolder<mojom::BluetoothInstance>* bluetooth() { return &bluetooth_; }
  InstanceHolder<mojom::BootPhaseMonitorInstance>* boot_phase_monitor() {
    return &boot_phase_monitor_;
  }
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
  InstanceHolder<mojom::KioskInstance>* kiosk() { return &kiosk_; }
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
  InstanceHolder<mojom::PrintInstance>* print() { return &print_; }
  InstanceHolder<mojom::ProcessInstance>* process() { return &process_; }
  InstanceHolder<mojom::StorageManagerInstance>* storage_manager() {
    return &storage_manager_;
  }
  InstanceHolder<mojom::TtsInstance>* tts() { return &tts_; }
  InstanceHolder<mojom::VideoInstance>* video() { return &video_; }
  InstanceHolder<mojom::WallpaperInstance>* wallpaper() { return &wallpaper_; }

  // Gets if ARC is currently running.
  bool ready() const { return state() == State::READY; }

  // Gets if ARC is currently stopped. This is not exactly !ready() since there
  // are transient states between ready() and stopped().
  bool stopped() const { return state() == State::STOPPED; }

 protected:
  // The possible states of the bridge.  In the normal flow, the state changes
  // in the following sequence:
  //
  // STOPPED
  //   PrerequisitesChanged() ->
  // CONNECTING
  //   OnConnectionEstablished() ->
  // READY
  //
  // The ArcSession state machine can be thought of being substates of
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

  ArcBridgeService();

  // Instance holders.
  InstanceHolder<mojom::AppInstance> app_;
  InstanceHolder<mojom::AudioInstance> audio_;
  InstanceHolder<mojom::AuthInstance> auth_;
  InstanceHolder<mojom::BluetoothInstance> bluetooth_;
  InstanceHolder<mojom::BootPhaseMonitorInstance> boot_phase_monitor_;
  InstanceHolder<mojom::ClipboardInstance> clipboard_;
  InstanceHolder<mojom::CrashCollectorInstance> crash_collector_;
  InstanceHolder<mojom::EnterpriseReportingInstance> enterprise_reporting_;
  InstanceHolder<mojom::FileSystemInstance> file_system_;
  InstanceHolder<mojom::ImeInstance> ime_;
  InstanceHolder<mojom::IntentHelperInstance> intent_helper_;
  InstanceHolder<mojom::KioskInstance> kiosk_;
  InstanceHolder<mojom::MetricsInstance> metrics_;
  InstanceHolder<mojom::NetInstance> net_;
  InstanceHolder<mojom::NotificationsInstance> notifications_;
  InstanceHolder<mojom::ObbMounterInstance> obb_mounter_;
  InstanceHolder<mojom::PolicyInstance> policy_;
  InstanceHolder<mojom::PowerInstance> power_;
  InstanceHolder<mojom::PrintInstance> print_;
  InstanceHolder<mojom::ProcessInstance> process_;
  InstanceHolder<mojom::StorageManagerInstance> storage_manager_;
  InstanceHolder<mojom::TtsInstance> tts_;
  InstanceHolder<mojom::VideoInstance> video_;
  InstanceHolder<mojom::WallpaperInstance> wallpaper_;

  // Gets the current state of the bridge service.
  State state() const { return state_; }

  // Changes the current state and notifies all observers.
  void SetState(State state);

  // Sets the reason the bridge is stopped. This function must be always called
  // before SetState(State::STOPPED) to report a correct reason with
  // Observer::OnBridgeStopped().
  void SetStopReason(StopReason stop_reason);

  base::ObserverList<Observer>& observer_list() { return observer_list_; }

  bool CalledOnValidThread();

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Prerequisites);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, StopMidStartup);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Restart);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, OnBridgeStopped);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Shutdown);

  base::ObserverList<Observer> observer_list_;

  base::ThreadChecker thread_checker_;

  // The current state of the bridge.
  ArcBridgeService::State state_;

  // The reason the bridge is stopped.
  StopReason stop_reason_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeService);
};

// Defines "<<" operator for LOGging purpose.
std::ostream& operator<<(
    std::ostream& os, ArcBridgeService::StopReason reason);

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_H_

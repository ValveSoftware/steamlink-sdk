// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_HOST_IMPL_H_
#define COMPONENTS_ARC_ARC_BRIDGE_HOST_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/common/arc_bridge.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace arc {

// Implementation of the ArcBridgeHost.
// The lifetime of ArcBridgeHost and ArcBridgeInstance mojo channels are tied
// to this instance. Also, any ARC related Mojo channel will be closed if
// either ArcBridgeHost or ArcBridgeInstance Mojo channels is closed on error.
// When ARC Instance (not Host) Mojo channel gets ready (= passed via
// OnFooInstanceReady(), and the QueryVersion() gets completed), then this sets
// the raw pointer to the ArcBridgeService so that other services can access
// to the pointer, and resets it on channel closing.
// Note that ArcBridgeService must be alive while ArcBridgeHostImpl is alive.
class ArcBridgeHostImpl : public mojom::ArcBridgeHost {
 public:
  // Interface to keep the Mojo channel InterfacePtr.
  class MojoChannel;

  explicit ArcBridgeHostImpl(mojom::ArcBridgeInstancePtr instance);
  ~ArcBridgeHostImpl() override;

  // ArcBridgeHost overrides.
  void OnAppInstanceReady(mojom::AppInstancePtr app_ptr) override;
  void OnAudioInstanceReady(mojom::AudioInstancePtr audio_ptr) override;
  void OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) override;
  void OnBluetoothInstanceReady(
      mojom::BluetoothInstancePtr bluetooth_ptr) override;
  void OnBootPhaseMonitorInstanceReady(
      mojom::BootPhaseMonitorInstancePtr boot_phase_monitor_ptr) override;
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
  void OnKioskInstanceReady(mojom::KioskInstancePtr kiosk_ptr) override;
  void OnMetricsInstanceReady(mojom::MetricsInstancePtr metrics_ptr) override;
  void OnNetInstanceReady(mojom::NetInstancePtr net_ptr) override;
  void OnNotificationsInstanceReady(
      mojom::NotificationsInstancePtr notifications_ptr) override;
  void OnObbMounterInstanceReady(
      mojom::ObbMounterInstancePtr obb_mounter_ptr) override;
  void OnPolicyInstanceReady(mojom::PolicyInstancePtr policy_ptr) override;
  void OnPowerInstanceReady(mojom::PowerInstancePtr power_ptr) override;
  void OnPrintInstanceReady(mojom::PrintInstancePtr print_ptr) override;
  void OnProcessInstanceReady(mojom::ProcessInstancePtr process_ptr) override;
  void OnStorageManagerInstanceReady(
      mojom::StorageManagerInstancePtr storage_manager_ptr) override;
  void OnTtsInstanceReady(mojom::TtsInstancePtr tts_ptr) override;
  void OnVideoInstanceReady(mojom::VideoInstancePtr video_ptr) override;
  void OnWallpaperInstanceReady(
      mojom::WallpaperInstancePtr wallpaper_ptr) override;

 private:
  // Called when the bridge channel is closed. This typically only happens when
  // the ARC instance crashes.
  void OnClosed();

  // The common implementation to handle ArcBridgeHost overrides.
  // |T| is a ARC Mojo Instance type.
  template <typename T>
  void OnInstanceReady(InstanceHolder<T>* holder, mojo::InterfacePtr<T> ptr);

  // Called if one of the established channels is closed.
  void OnChannelClosed(MojoChannel* channel);

  base::ThreadChecker thread_checker_;

  mojo::Binding<mojom::ArcBridgeHost> binding_;
  mojom::ArcBridgeInstancePtr instance_;

  // Put as a last member to ensure that any callback tied to the elements
  // is not invoked.
  std::vector<std::unique_ptr<MojoChannel>> mojo_channels_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeHostImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_HOST_IMPL_H_

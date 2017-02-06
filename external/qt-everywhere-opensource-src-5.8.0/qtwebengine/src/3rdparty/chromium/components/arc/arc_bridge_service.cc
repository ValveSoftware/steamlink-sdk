// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_service.h"

#include <utility>

#include "base/command_line.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceManager.
ArcBridgeService* g_arc_bridge_service = nullptr;

}  // namespace

ArcBridgeService::ArcBridgeService()
    : available_(false),
      state_(State::STOPPED),
      stop_reason_(StopReason::SHUTDOWN),
      weak_factory_(this) {
  DCHECK(!g_arc_bridge_service);
  g_arc_bridge_service = this;
}

ArcBridgeService::~ArcBridgeService() {
  DCHECK(CalledOnValidThread());
  DCHECK(state() == State::STOPPING || state() == State::STOPPED);
  DCHECK(g_arc_bridge_service == this);
  g_arc_bridge_service = nullptr;
}

// static
ArcBridgeService* ArcBridgeService::Get() {
  if (!g_arc_bridge_service) {
    // ArcBridgeService may be indirectly referenced in unit tests where
    // ArcBridgeService is optional.
    LOG(ERROR) << "ArcBridgeService is not ready.";
    return nullptr;
  }
  DCHECK(g_arc_bridge_service->CalledOnValidThread());
  return g_arc_bridge_service;
}

// static
bool ArcBridgeService::GetEnabled(const base::CommandLine* command_line) {
  return command_line->HasSwitch(chromeos::switches::kEnableArc);
}

void ArcBridgeService::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void ArcBridgeService::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void ArcBridgeService::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {
  DCHECK(CalledOnValidThread());
  app_.OnInstanceReady(std::move(app_ptr));
}

void ArcBridgeService::OnAudioInstanceReady(mojom::AudioInstancePtr audio_ptr) {
  DCHECK(CalledOnValidThread());
  audio_.OnInstanceReady(std::move(audio_ptr));
}

void ArcBridgeService::OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) {
  DCHECK(CalledOnValidThread());
  auth_.OnInstanceReady(std::move(auth_ptr));
}

void ArcBridgeService::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {
  DCHECK(CalledOnValidThread());
  bluetooth_.OnInstanceReady(std::move(bluetooth_ptr));
}

void ArcBridgeService::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {
  DCHECK(CalledOnValidThread());
  clipboard_.OnInstanceReady(std::move(clipboard_ptr));
}

void ArcBridgeService::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {
  DCHECK(CalledOnValidThread());
  crash_collector_.OnInstanceReady(std::move(crash_collector_ptr));
}

void ArcBridgeService::OnEnterpriseReportingInstanceReady(
    mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) {
  enterprise_reporting_.OnInstanceReady(std::move(enterprise_reporting_ptr));
}

void ArcBridgeService::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {
  DCHECK(CalledOnValidThread());
  file_system_.OnInstanceReady(std::move(file_system_ptr));
}

void ArcBridgeService::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {
  DCHECK(CalledOnValidThread());
  ime_.OnInstanceReady(std::move(ime_ptr));
}

void ArcBridgeService::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {
  DCHECK(CalledOnValidThread());
  intent_helper_.OnInstanceReady(std::move(intent_helper_ptr));
}

void ArcBridgeService::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {
  DCHECK(CalledOnValidThread());
  metrics_.OnInstanceReady(std::move(metrics_ptr));
}

void ArcBridgeService::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {
  DCHECK(CalledOnValidThread());
  net_.OnInstanceReady(std::move(net_ptr));
}

void ArcBridgeService::OnNotificationsInstanceReady(
    mojom::NotificationsInstancePtr notifications_ptr) {
  DCHECK(CalledOnValidThread());
  notifications_.OnInstanceReady(std::move(notifications_ptr));
}

void ArcBridgeService::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {
  DCHECK(CalledOnValidThread());
  obb_mounter_.OnInstanceReady(std::move(obb_mounter_ptr));
}

void ArcBridgeService::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {
  DCHECK(CalledOnValidThread());
  policy_.OnInstanceReady(std::move(policy_ptr));
}

void ArcBridgeService::OnPowerInstanceReady(mojom::PowerInstancePtr power_ptr) {
  DCHECK(CalledOnValidThread());
  power_.OnInstanceReady(std::move(power_ptr));
}

void ArcBridgeService::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {
  DCHECK(CalledOnValidThread());
  process_.OnInstanceReady(std::move(process_ptr));
}

void ArcBridgeService::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {
  DCHECK(CalledOnValidThread());
  storage_manager_.OnInstanceReady(std::move(storage_manager_ptr));
}

void ArcBridgeService::OnVideoInstanceReady(mojom::VideoInstancePtr video_ptr) {
  DCHECK(CalledOnValidThread());
  video_.OnInstanceReady(std::move(video_ptr));
}

void ArcBridgeService::OnWindowManagerInstanceReady(
    mojom::WindowManagerInstancePtr window_manager_ptr) {
  DCHECK(CalledOnValidThread());
  window_manager_.OnInstanceReady(std::move(window_manager_ptr));
}

void ArcBridgeService::SetState(State state) {
  DCHECK(CalledOnValidThread());
  // DCHECK on enum classes not supported.
  DCHECK(state_ != state);
  state_ = state;
  VLOG(2) << "State: " << static_cast<uint32_t>(state_);
  FOR_EACH_OBSERVER(Observer, observer_list(), OnStateChanged(state_));
  if (state_ == State::READY)
    FOR_EACH_OBSERVER(Observer, observer_list(), OnBridgeReady());
  else if (state == State::STOPPED)
    FOR_EACH_OBSERVER(Observer, observer_list(), OnBridgeStopped(stop_reason_));
}

void ArcBridgeService::SetAvailable(bool available) {
  DCHECK(CalledOnValidThread());
  DCHECK(available_ != available);
  available_ = available;
  FOR_EACH_OBSERVER(Observer, observer_list(), OnAvailableChanged(available_));
}

void ArcBridgeService::SetStopReason(StopReason stop_reason) {
  DCHECK(CalledOnValidThread());
  stop_reason_ = stop_reason;
}

bool ArcBridgeService::CalledOnValidThread() {
  return thread_checker_.CalledOnValidThread();
}

void ArcBridgeService::CloseAllChannels() {
  // Call all the error handlers of all the channels to both close the channel
  // and notify any observers that the channel is closed.
  app_.CloseChannel();
  audio_.CloseChannel();
  auth_.CloseChannel();
  bluetooth_.CloseChannel();
  clipboard_.CloseChannel();
  crash_collector_.CloseChannel();
  enterprise_reporting_.CloseChannel();
  file_system_.CloseChannel();
  ime_.CloseChannel();
  intent_helper_.CloseChannel();
  metrics_.CloseChannel();
  net_.CloseChannel();
  notifications_.CloseChannel();
  obb_mounter_.CloseChannel();
  policy_.CloseChannel();
  power_.CloseChannel();
  process_.CloseChannel();
  storage_manager_.CloseChannel();
  video_.CloseChannel();
  window_manager_.CloseChannel();
}

}  // namespace arc

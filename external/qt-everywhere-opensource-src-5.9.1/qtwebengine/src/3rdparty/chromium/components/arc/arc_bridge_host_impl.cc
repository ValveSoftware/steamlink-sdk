// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_bridge_host_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/instance_holder.h"

namespace arc {

// Thin interface to wrap InterfacePtr<T> with type erasure.
class ArcBridgeHostImpl::MojoChannel {
 public:
  virtual ~MojoChannel() = default;

 protected:
  MojoChannel() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoChannel);
};

namespace {

// The thin wrapper for InterfacePtr<T>, where T is one of ARC mojo Instance
// class.
template <typename T>
class MojoChannelImpl : public ArcBridgeHostImpl::MojoChannel {
 public:
  MojoChannelImpl(InstanceHolder<T>* holder, mojo::InterfacePtr<T> ptr)
      : holder_(holder), ptr_(std::move(ptr)) {
    // Delay registration to the InstanceHolder until the version is ready.
  }

  ~MojoChannelImpl() override { holder_->SetInstance(nullptr, 0); }

  void set_connection_error_handler(const base::Closure& error_handler) {
    ptr_.set_connection_error_handler(error_handler);
  }

  void QueryVersion() {
    // Note: the callback will not be called if |ptr_| is destroyed.
    ptr_.QueryVersion(base::Bind(&MojoChannelImpl<T>::OnVersionReady,
                                 base::Unretained(this)));
  }

 private:
  void OnVersionReady(uint32_t unused_version) {
    holder_->SetInstance(ptr_.get(), ptr_.version());
  }

  // Owned by ArcBridgeService.
  InstanceHolder<T>* const holder_;

  // Put as a last member to ensure that any callback tied to the |ptr_|
  // is not invoked.
  mojo::InterfacePtr<T> ptr_;

  DISALLOW_COPY_AND_ASSIGN(MojoChannelImpl);
};

}  // namespace

ArcBridgeHostImpl::ArcBridgeHostImpl(mojom::ArcBridgeInstancePtr instance)
    : binding_(this), instance_(std::move(instance)) {
  DCHECK(instance_.is_bound());
  instance_.set_connection_error_handler(
      base::Bind(&ArcBridgeHostImpl::OnClosed, base::Unretained(this)));
  instance_->Init(binding_.CreateInterfacePtrAndBind());
}

ArcBridgeHostImpl::~ArcBridgeHostImpl() {
  OnClosed();
}

void ArcBridgeHostImpl::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->app(), std::move(app_ptr));
}

void ArcBridgeHostImpl::OnAudioInstanceReady(
    mojom::AudioInstancePtr audio_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->audio(), std::move(audio_ptr));
}

void ArcBridgeHostImpl::OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->auth(), std::move(auth_ptr));
}

void ArcBridgeHostImpl::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->bluetooth(),
                  std::move(bluetooth_ptr));
}

void ArcBridgeHostImpl::OnBootPhaseMonitorInstanceReady(
    mojom::BootPhaseMonitorInstancePtr boot_phase_monitor_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->boot_phase_monitor(),
                  std::move(boot_phase_monitor_ptr));
}

void ArcBridgeHostImpl::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->clipboard(),
                  std::move(clipboard_ptr));
}

void ArcBridgeHostImpl::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->crash_collector(),
                  std::move(crash_collector_ptr));
}

void ArcBridgeHostImpl::OnEnterpriseReportingInstanceReady(
    mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->enterprise_reporting(),
                  std::move(enterprise_reporting_ptr));
}

void ArcBridgeHostImpl::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->file_system(),
                  std::move(file_system_ptr));
}

void ArcBridgeHostImpl::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->ime(), std::move(ime_ptr));
}

void ArcBridgeHostImpl::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->intent_helper(),
                  std::move(intent_helper_ptr));
}

void ArcBridgeHostImpl::OnKioskInstanceReady(
    mojom::KioskInstancePtr kiosk_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->kiosk(), std::move(kiosk_ptr));
}

void ArcBridgeHostImpl::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->metrics(), std::move(metrics_ptr));
}

void ArcBridgeHostImpl::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->net(), std::move(net_ptr));
}

void ArcBridgeHostImpl::OnNotificationsInstanceReady(
    mojom::NotificationsInstancePtr notifications_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->notifications(),
                  std::move(notifications_ptr));
}

void ArcBridgeHostImpl::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->obb_mounter(),
                  std::move(obb_mounter_ptr));
}

void ArcBridgeHostImpl::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->policy(), std::move(policy_ptr));
}

void ArcBridgeHostImpl::OnPowerInstanceReady(
    mojom::PowerInstancePtr power_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->power(), std::move(power_ptr));
}

void ArcBridgeHostImpl::OnPrintInstanceReady(
    mojom::PrintInstancePtr print_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->print(), std::move(print_ptr));
}

void ArcBridgeHostImpl::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->process(), std::move(process_ptr));
}

void ArcBridgeHostImpl::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->storage_manager(),
                  std::move(storage_manager_ptr));
}

void ArcBridgeHostImpl::OnTtsInstanceReady(mojom::TtsInstancePtr tts_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->tts(), std::move(tts_ptr));
}

void ArcBridgeHostImpl::OnVideoInstanceReady(
    mojom::VideoInstancePtr video_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->video(), std::move(video_ptr));
}

void ArcBridgeHostImpl::OnWallpaperInstanceReady(
    mojom::WallpaperInstancePtr wallpaper_ptr) {
  OnInstanceReady(ArcBridgeService::Get()->wallpaper(),
                  std::move(wallpaper_ptr));
}

void ArcBridgeHostImpl::OnClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Mojo connection lost";

  // Close all mojo channels.
  mojo_channels_.clear();
  instance_.reset();
  if (binding_.is_bound())
    binding_.Close();
}

template <typename T>
void ArcBridgeHostImpl::OnInstanceReady(InstanceHolder<T>* holder,
                                        mojo::InterfacePtr<T> ptr) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(binding_.is_bound());
  DCHECK(ptr.is_bound());

  // Track |channel|'s lifetime via |mojo_channels_| so that it will be
  // closed on ArcBridgeHost/Instance closing or the ArcBridgeHostImpl's
  // destruction.
  auto* channel = new MojoChannelImpl<T>(holder, std::move(ptr));
  mojo_channels_.emplace_back(channel);

  // Since |channel| is managed by |mojo_channels_|, its lifetime is shorter
  // than |this|. Thus, the connection error handler will be invoked only
  // when |this| is alive and base::Unretained is safe here.
  channel->set_connection_error_handler(base::Bind(
      &ArcBridgeHostImpl::OnChannelClosed, base::Unretained(this), channel));

  // Call QueryVersion so that the version info is properly stored in the
  // InterfacePtr<T>.
  channel->QueryVersion();
}

void ArcBridgeHostImpl::OnChannelClosed(MojoChannel* channel) {
  DCHECK(thread_checker_.CalledOnValidThread());
  mojo_channels_.erase(
      std::find_if(mojo_channels_.begin(), mojo_channels_.end(),
                   [channel](std::unique_ptr<MojoChannel>& ptr) {
                     return ptr.get() == channel;
                   }));
}

}  // namespace arc

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/mus_thread_proxy.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

namespace ui {

MusThreadProxy::MusThreadProxy()
    : ws_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      drm_thread_(nullptr),
      weak_ptr_factory_(this) {}

void MusThreadProxy::InitializeOnEvdev() {}

MusThreadProxy::~MusThreadProxy() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  FOR_EACH_OBSERVER(GpuThreadObserver, gpu_thread_observers_,
                    OnGpuThreadRetired());
}

// This is configured on the GPU thread.
void MusThreadProxy::SetDrmThread(DrmThread* thread) {
  base::AutoLock acquire(lock_);
  drm_thread_ = thread;
}

void MusThreadProxy::ProvideManagers(DrmDisplayHostManager* display_manager,
                                     DrmOverlayManager* overlay_manager) {
  display_manager_ = display_manager;
  overlay_manager_ = overlay_manager;
}

void MusThreadProxy::StartDrmThread() {
  DCHECK(drm_thread_);
  drm_thread_->Start();

  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MusThreadProxy::DispatchObserversFromDrmThread,
                            base::Unretained(this)));
}

void MusThreadProxy::DispatchObserversFromDrmThread() {
  ws_task_runner_->PostTask(FROM_HERE, base::Bind(&MusThreadProxy::RunObservers,
                                                  base::Unretained(this)));
}

void MusThreadProxy::RunObservers() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  FOR_EACH_OBSERVER(GpuThreadObserver, gpu_thread_observers_,
                    OnGpuThreadReady());
}

void MusThreadProxy::AddGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());

  gpu_thread_observers_.AddObserver(observer);
  if (IsConnected())
    observer->OnGpuThreadReady();
}

void MusThreadProxy::RemoveGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  gpu_thread_observers_.RemoveObserver(observer);
}

bool MusThreadProxy::IsConnected() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  base::AutoLock acquire(lock_);
  if (drm_thread_)
    return drm_thread_->IsRunning();
  return false;
}

// Services needed for DrmDisplayHostMananger.
void MusThreadProxy::RegisterHandlerForDrmDisplayHostManager(
    DrmDisplayHostManager* handler) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_ = handler;
}

void MusThreadProxy::UnRegisterHandlerForDrmDisplayHostManager() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_ = nullptr;
}

bool MusThreadProxy::GpuCreateWindow(gfx::AcceleratedWidget widget) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::CreateWindow,
                            base::Unretained(drm_thread_), widget));
  return true;
}

bool MusThreadProxy::GpuDestroyWindow(gfx::AcceleratedWidget widget) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::DestroyWindow,
                            base::Unretained(drm_thread_), widget));
  return true;
}

bool MusThreadProxy::GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                            const gfx::Rect& bounds) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::SetWindowBounds,
                            base::Unretained(drm_thread_), widget, bounds));
  return true;
}

void MusThreadProxy::CursorSet(gfx::AcceleratedWidget widget,
                               const std::vector<SkBitmap>& bitmaps,
                               const gfx::Point& location,
                               int frame_delay_ms) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::SetCursor, base::Unretained(drm_thread_), widget,
                 bitmaps, location, frame_delay_ms));
}

void MusThreadProxy::Move(gfx::AcceleratedWidget widget,
                          const gfx::Point& location) {
  // NOTE: Input events skip the main thread to avoid jank.
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::MoveCursor,
                            base::Unretained(drm_thread_), widget, location));
}

// Services needed for DrmOverlayManager.
void MusThreadProxy::RegisterHandlerForDrmOverlayManager(
    DrmOverlayManager* handler) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_ = handler;
}

void MusThreadProxy::UnRegisterHandlerForDrmOverlayManager() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_ = nullptr;
}

bool MusThreadProxy::GpuCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback =
      base::Bind(&MusThreadProxy::GpuCheckOverlayCapabilitiesCallback,
                 weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::CheckOverlayCapabilities,
                            base::Unretained(drm_thread_), widget, overlays,
                            CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuRefreshNativeDisplays() {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback = base::Bind(&MusThreadProxy::GpuRefreshNativeDisplaysCallback,
                             weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::RefreshNativeDisplays,
                 base::Unretained(drm_thread_), CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuConfigureNativeDisplay(int64_t id,
                                               const DisplayMode_Params& mode,
                                               const gfx::Point& origin) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());

  auto callback = base::Bind(&MusThreadProxy::GpuConfigureNativeDisplayCallback,
                             weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::ConfigureNativeDisplay,
                            base::Unretained(drm_thread_), id, mode, origin,
                            CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuDisableNativeDisplay(int64_t id) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback = base::Bind(&MusThreadProxy::GpuDisableNativeDisplayCallback,
                             weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::DisableNativeDisplay,
                            base::Unretained(drm_thread_), id,
                            CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuTakeDisplayControl() {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback = base::Bind(&MusThreadProxy::GpuTakeDisplayControlCallback,
                             weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::TakeDisplayControl, base::Unretained(drm_thread_),
                 CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuRelinquishDisplayControl() {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback =
      base::Bind(&MusThreadProxy::GpuRelinquishDisplayControlCallback,
                 weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::RelinquishDisplayControl,
                 base::Unretained(drm_thread_), CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuAddGraphicsDevice(const base::FilePath& path,
                                          const base::FileDescriptor& fd) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::AddGraphicsDevice,
                            base::Unretained(drm_thread_), path, fd));
  return true;
}

bool MusThreadProxy::GpuRemoveGraphicsDevice(const base::FilePath& path) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::RemoveGraphicsDevice,
                            base::Unretained(drm_thread_), path));
  return true;
}

bool MusThreadProxy::GpuGetHDCPState(int64_t display_id) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback = base::Bind(&MusThreadProxy::GpuGetHDCPStateCallback,
                             weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::GetHDCPState, base::Unretained(drm_thread_),
                 display_id, CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuSetHDCPState(int64_t display_id, HDCPState state) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  DCHECK(drm_thread_->IsRunning());
  auto callback = base::Bind(&MusThreadProxy::GpuSetHDCPStateCallback,
                             weak_ptr_factory_.GetWeakPtr());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::SetHDCPState, base::Unretained(drm_thread_),
                 display_id, state, CreateSafeCallback(callback)));
  return true;
}

bool MusThreadProxy::GpuSetColorCorrection(
    int64_t id,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::SetColorCorrection, base::Unretained(drm_thread_),
                 id, degamma_lut, gamma_lut, correction_matrix));
  return true;
}

void MusThreadProxy::GpuCheckOverlayCapabilitiesCallback(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_->GpuSentOverlayResult(widget, overlays);
}

void MusThreadProxy::GpuConfigureNativeDisplayCallback(int64_t display_id,
                                                       bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

void MusThreadProxy::GpuRefreshNativeDisplaysCallback(
    const std::vector<DisplaySnapshot_Params>& displays) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuHasUpdatedNativeDisplays(displays);
}

void MusThreadProxy::GpuDisableNativeDisplayCallback(int64_t display_id,
                                                     bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

void MusThreadProxy::GpuTakeDisplayControlCallback(bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuTookDisplayControl(success);
}

void MusThreadProxy::GpuRelinquishDisplayControlCallback(bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuRelinquishedDisplayControl(success);
}

void MusThreadProxy::GpuGetHDCPStateCallback(int64_t display_id,
                                             bool success,
                                             HDCPState state) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuReceivedHDCPState(display_id, success, state);
}

void MusThreadProxy::GpuSetHDCPStateCallback(int64_t display_id,
                                             bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuUpdatedHDCPState(display_id, success);
}

}  // namespace ui

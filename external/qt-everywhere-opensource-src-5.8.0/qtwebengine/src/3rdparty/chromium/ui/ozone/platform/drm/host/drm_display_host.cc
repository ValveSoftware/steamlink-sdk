// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_display_host.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/common/display_snapshot_proxy.h"
#include "ui/ozone/common/display_util.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"

namespace ui {

DrmDisplayHost::DrmDisplayHost(GpuThreadAdapter* sender,
                               const DisplaySnapshot_Params& params,
                               bool is_dummy)
    : sender_(sender),
      snapshot_(new DisplaySnapshotProxy(params)),
      is_dummy_(is_dummy) {
  sender_->AddGpuThreadObserver(this);
}

DrmDisplayHost::~DrmDisplayHost() {
  sender_->RemoveGpuThreadObserver(this);
  ClearCallbacks();
}

void DrmDisplayHost::UpdateDisplaySnapshot(
    const DisplaySnapshot_Params& params) {
  snapshot_ = base::WrapUnique(new DisplaySnapshotProxy(params));
}

void DrmDisplayHost::Configure(const DisplayMode* mode,
                               const gfx::Point& origin,
                               const ConfigureCallback& callback) {
  if (is_dummy_) {
    callback.Run(true);
    return;
  }

  configure_callback_ = callback;
  bool status = false;
  if (mode) {
    status = sender_->GpuConfigureNativeDisplay(
        snapshot_->display_id(), GetDisplayModeParams(*mode), origin);
  } else {
    status = sender_->GpuDisableNativeDisplay(snapshot_->display_id());
  }

  if (!status)
    OnDisplayConfigured(false);
}

void DrmDisplayHost::OnDisplayConfigured(bool status) {
  if (!configure_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(configure_callback_, status));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  configure_callback_.Reset();
}

void DrmDisplayHost::GetHDCPState(const GetHDCPStateCallback& callback) {
  get_hdcp_callback_ = callback;
  if (!sender_->GpuGetHDCPState(snapshot_->display_id()))
    OnHDCPStateReceived(false, HDCP_STATE_UNDESIRED);
}

void DrmDisplayHost::OnHDCPStateReceived(bool status, HDCPState state) {
  if (!get_hdcp_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(get_hdcp_callback_, status, state));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  get_hdcp_callback_.Reset();
}

void DrmDisplayHost::SetHDCPState(HDCPState state,
                                  const SetHDCPStateCallback& callback) {
  set_hdcp_callback_ = callback;
  if (!sender_->GpuSetHDCPState(snapshot_->display_id(), state))
    OnHDCPStateUpdated(false);
}

void DrmDisplayHost::OnHDCPStateUpdated(bool status) {
  if (!set_hdcp_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(set_hdcp_callback_, status));
  } else {
    LOG(ERROR) << "Got unexpected event for display "
               << snapshot_->display_id();
  }

  set_hdcp_callback_.Reset();
}

void DrmDisplayHost::SetColorCorrection(
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  sender_->GpuSetColorCorrection(snapshot_->display_id(), degamma_lut,
                                 gamma_lut, correction_matrix);
}

void DrmDisplayHost::OnGpuThreadReady() {
  is_dummy_ = false;

  // Note: These responses are done here since the OnChannelDestroyed() is
  // called after OnChannelEstablished().
  ClearCallbacks();
}

void DrmDisplayHost::OnGpuThreadRetired() {}

void DrmDisplayHost::ClearCallbacks() {
  if (!configure_callback_.is_null())
    OnDisplayConfigured(false);
  if (!get_hdcp_callback_.is_null())
    OnHDCPStateReceived(false, HDCP_STATE_UNDESIRED);
  if (!set_hdcp_callback_.is_null())
    OnHDCPStateUpdated(false);
}

}  // namespace ui

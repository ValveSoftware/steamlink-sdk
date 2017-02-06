// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"

namespace ui {

namespace {

// Helper class that provides DrmCursor with a mechanism to send messages
// to the GPU process.
class CursorIPC : public DrmCursorProxy {
 public:
  CursorIPC(scoped_refptr<base::SingleThreadTaskRunner> send_runner,
            const base::Callback<void(IPC::Message*)>& send_callback);
  ~CursorIPC() override;

  // DrmCursorProxy implementation.
  void CursorSet(gfx::AcceleratedWidget window,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& point,
                 int frame_delay_ms) override;
  void Move(gfx::AcceleratedWidget window, const gfx::Point& point) override;
  void InitializeOnEvdev() override;

 private:
  bool IsConnected();
  void Send(IPC::Message* message);

  scoped_refptr<base::SingleThreadTaskRunner> send_runner_;
  base::Callback<void(IPC::Message*)> send_callback_;

  DISALLOW_COPY_AND_ASSIGN(CursorIPC);
};

CursorIPC::CursorIPC(scoped_refptr<base::SingleThreadTaskRunner> send_runner,
                     const base::Callback<void(IPC::Message*)>& send_callback)
    : send_runner_(send_runner), send_callback_(send_callback) {}

CursorIPC::~CursorIPC() {}

bool CursorIPC::IsConnected() {
  return !send_callback_.is_null();
}

void CursorIPC::CursorSet(gfx::AcceleratedWidget window,
                          const std::vector<SkBitmap>& bitmaps,
                          const gfx::Point& point,
                          int frame_delay_ms) {
  Send(new OzoneGpuMsg_CursorSet(window, bitmaps, point, frame_delay_ms));
}

void CursorIPC::Move(gfx::AcceleratedWidget window, const gfx::Point& point) {
  Send(new OzoneGpuMsg_CursorMove(window, point));
}

void CursorIPC::InitializeOnEvdev() {}

void CursorIPC::Send(IPC::Message* message) {
  if (IsConnected() &&
      send_runner_->PostTask(FROM_HERE, base::Bind(send_callback_, message)))
    return;

  // Drop disconnected updates. DrmWindowHost will call
  // CommitBoundsChange() when we connect to initialize the cursor
  // location.
  delete message;
}

}  // namespace

DrmGpuPlatformSupportHost::DrmGpuPlatformSupportHost(DrmCursor* cursor)
    : cursor_(cursor) {}

DrmGpuPlatformSupportHost::~DrmGpuPlatformSupportHost() {}

void DrmGpuPlatformSupportHost::AddGpuThreadObserver(
    GpuThreadObserver* observer) {
  gpu_thread_observers_.AddObserver(observer);

  if (IsConnected())
    observer->OnGpuThreadReady();
}

void DrmGpuPlatformSupportHost::RemoveGpuThreadObserver(
    GpuThreadObserver* observer) {
  gpu_thread_observers_.RemoveObserver(observer);
}

bool DrmGpuPlatformSupportHost::IsConnected() {
  return host_id_ >= 0;
}

void DrmGpuPlatformSupportHost::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& send_callback) {
  TRACE_EVENT1("drm", "DrmGpuPlatformSupportHost::OnChannelEstablished",
               "host_id", host_id);
  host_id_ = host_id;
  send_runner_ = send_runner;
  send_callback_ = send_callback;

  FOR_EACH_OBSERVER(GpuThreadObserver, gpu_thread_observers_,
                    OnGpuThreadReady());

  // The cursor is special since it will process input events on the IO thread
  // and can by-pass the UI thread. This means that we need to special case it
  // and notify it after all other observers/handlers are notified such that the
  // (windowing) state on the GPU can be initialized before the cursor is
  // allowed to IPC messages (which are targeted to a specific window).
  cursor_->SetDrmCursorProxy(new CursorIPC(send_runner_, send_callback_));
}

void DrmGpuPlatformSupportHost::OnChannelDestroyed(int host_id) {
  TRACE_EVENT1("drm", "DrmGpuPlatformSupportHost::OnChannelDestroyed",
               "host_id", host_id);

  if (host_id_ == host_id) {
    cursor_->ResetDrmCursorProxy();
    host_id_ = -1;
    send_runner_ = nullptr;
    send_callback_.Reset();
    FOR_EACH_OBSERVER(GpuThreadObserver, gpu_thread_observers_,
                      OnGpuThreadRetired());
  }

}

bool DrmGpuPlatformSupportHost::OnMessageReceived(const IPC::Message& message) {
  if (OnMessageReceivedForDrmDisplayHostManager(message))
    return true;
  if (OnMessageReceivedForDrmOverlayManager(message))
    return true;

  return false;
}

bool DrmGpuPlatformSupportHost::Send(IPC::Message* message) {
  if (IsConnected() &&
      send_runner_->PostTask(FROM_HERE, base::Bind(send_callback_, message)))
    return true;

  delete message;
  return false;
}

// DisplayHost
void DrmGpuPlatformSupportHost::RegisterHandlerForDrmDisplayHostManager(
    DrmDisplayHostManager* handler) {
  display_manager_ = handler;
}

void DrmGpuPlatformSupportHost::UnRegisterHandlerForDrmDisplayHostManager() {
  display_manager_ = nullptr;
}

bool DrmGpuPlatformSupportHost::OnMessageReceivedForDrmDisplayHostManager(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupportHost, message)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_UpdateNativeDisplays,
                        OnUpdateNativeDisplays)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayConfigured, OnDisplayConfigured)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateReceived, OnHDCPStateReceived)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateUpdated, OnHDCPStateUpdated)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayControlTaken, OnTakeDisplayControl)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayControlRelinquished,
                        OnRelinquishDisplayControl)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DrmGpuPlatformSupportHost::OnUpdateNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& params) {
  display_manager_->GpuHasUpdatedNativeDisplays(params);
}

void DrmGpuPlatformSupportHost::OnDisplayConfigured(int64_t display_id,
                                                    bool status) {
  display_manager_->GpuConfiguredDisplay(display_id, status);
}

void DrmGpuPlatformSupportHost::OnHDCPStateReceived(int64_t display_id,
                                                    bool status,
                                                    HDCPState state) {
  display_manager_->GpuReceivedHDCPState(display_id, status, state);
}

void DrmGpuPlatformSupportHost::OnHDCPStateUpdated(int64_t display_id,
                                                   bool status) {
  display_manager_->GpuUpdatedHDCPState(display_id, status);
}

void DrmGpuPlatformSupportHost::OnTakeDisplayControl(bool status) {
  display_manager_->GpuTookDisplayControl(status);
}

void DrmGpuPlatformSupportHost::OnRelinquishDisplayControl(bool status) {
  display_manager_->GpuRelinquishedDisplayControl(status);
}

bool DrmGpuPlatformSupportHost::GpuRefreshNativeDisplays() {
  return Send(new OzoneGpuMsg_RefreshNativeDisplays());
}

bool DrmGpuPlatformSupportHost::GpuTakeDisplayControl() {
  return Send(new OzoneGpuMsg_TakeDisplayControl());
}

bool DrmGpuPlatformSupportHost::GpuRelinquishDisplayControl() {
  return Send(new OzoneGpuMsg_RelinquishDisplayControl());
}

bool DrmGpuPlatformSupportHost::GpuAddGraphicsDevice(
    const base::FilePath& path,
    const base::FileDescriptor& fd) {
  return Send(new OzoneGpuMsg_AddGraphicsDevice(path, fd));
}

bool DrmGpuPlatformSupportHost::GpuRemoveGraphicsDevice(
    const base::FilePath& path) {
  return Send(new OzoneGpuMsg_RemoveGraphicsDevice(path));
}

// Overlays
void DrmGpuPlatformSupportHost::RegisterHandlerForDrmOverlayManager(
    DrmOverlayManager* handler) {
  overlay_manager_ = handler;
}

void DrmGpuPlatformSupportHost::UnRegisterHandlerForDrmOverlayManager() {
  overlay_manager_ = nullptr;
}

bool DrmGpuPlatformSupportHost::OnMessageReceivedForDrmOverlayManager(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupportHost, message)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_OverlayCapabilitiesReceived,
                        OnOverlayResult)
    // TODO(rjk): insert the extra
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DrmGpuPlatformSupportHost::OnOverlayResult(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& params) {
  overlay_manager_->GpuSentOverlayResult(widget, params);
}

bool DrmGpuPlatformSupportHost::GpuCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& new_params) {
  return Send(new OzoneGpuMsg_CheckOverlayCapabilities(widget, new_params));
}

// DrmDisplayHost
bool DrmGpuPlatformSupportHost::GpuConfigureNativeDisplay(
    int64_t display_id,
    const ui::DisplayMode_Params& display_mode,
    const gfx::Point& point) {
  return Send(
      new OzoneGpuMsg_ConfigureNativeDisplay(display_id, display_mode, point));
}

bool DrmGpuPlatformSupportHost::GpuDisableNativeDisplay(int64_t display_id) {
  return Send(new OzoneGpuMsg_DisableNativeDisplay(display_id));
}

bool DrmGpuPlatformSupportHost::GpuGetHDCPState(int64_t display_id) {
  return Send(new OzoneGpuMsg_GetHDCPState(display_id));
}

bool DrmGpuPlatformSupportHost::GpuSetHDCPState(int64_t display_id,
                                                ui::HDCPState state) {
  return Send(new OzoneGpuMsg_SetHDCPState(display_id, state));
}

bool DrmGpuPlatformSupportHost::GpuSetColorCorrection(
    int64_t display_id,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  return Send(new OzoneGpuMsg_SetColorCorrection(display_id, degamma_lut,
                                                 gamma_lut, correction_matrix));
}

bool DrmGpuPlatformSupportHost::GpuDestroyWindow(
    gfx::AcceleratedWidget widget) {
  return Send(new OzoneGpuMsg_DestroyWindow(widget));
}

bool DrmGpuPlatformSupportHost::GpuCreateWindow(gfx::AcceleratedWidget widget) {
  return Send(new OzoneGpuMsg_CreateWindow(widget));
}

bool DrmGpuPlatformSupportHost::GpuWindowBoundsChanged(
    gfx::AcceleratedWidget widget,
    const gfx::Rect& bounds) {
  return Send(new OzoneGpuMsg_WindowBoundsChanged(widget, bounds));
}

}  // namespace ui

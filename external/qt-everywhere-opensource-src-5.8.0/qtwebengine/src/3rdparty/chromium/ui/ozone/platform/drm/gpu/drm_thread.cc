// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread.h"

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

class GbmBufferGenerator : public ScanoutBufferGenerator {
 public:
  GbmBufferGenerator() {}
  ~GbmBufferGenerator() override {}

  // ScanoutBufferGenerator:
  scoped_refptr<ScanoutBuffer> Create(const scoped_refptr<DrmDevice>& drm,
                                      gfx::BufferFormat format,
                                      const gfx::Size& size) override {
    scoped_refptr<GbmDevice> gbm(static_cast<GbmDevice*>(drm.get()));
    return GbmBuffer::CreateBuffer(gbm, format, size,
                                   gfx::BufferUsage::SCANOUT);
  }

 protected:
  DISALLOW_COPY_AND_ASSIGN(GbmBufferGenerator);
};

class GbmDeviceGenerator : public DrmDeviceGenerator {
 public:
  GbmDeviceGenerator(bool use_atomic) : use_atomic_(use_atomic) {}
  ~GbmDeviceGenerator() override {}

  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::File file,
                                        bool is_primary_device) override {
    scoped_refptr<DrmDevice> drm =
        new GbmDevice(path, std::move(file), is_primary_device);
    if (drm->Initialize(use_atomic_))
      return drm;

    return nullptr;
  }

 private:
  bool use_atomic_;

  DISALLOW_COPY_AND_ASSIGN(GbmDeviceGenerator);
};

}  // namespace

DrmThread::DrmThread() : base::Thread("DrmThread") {}

DrmThread::~DrmThread() {
  Stop();
}

void DrmThread::Start() {
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_options.priority = base::ThreadPriority::DISPLAY;
  if (!StartWithOptions(thread_options))
    LOG(FATAL) << "Failed to create DRM thread";
}

void DrmThread::Init() {
  bool use_atomic = false;
#if defined(USE_DRM_ATOMIC)
  use_atomic = true;
#endif

  device_manager_.reset(new DrmDeviceManager(
      base::WrapUnique(new GbmDeviceGenerator(use_atomic))));
  buffer_generator_.reset(new GbmBufferGenerator());
  screen_manager_.reset(new ScreenManager(buffer_generator_.get()));

  display_manager_.reset(
      new DrmGpuDisplayManager(screen_manager_.get(), device_manager_.get()));
}

void DrmThread::CreateBuffer(gfx::AcceleratedWidget widget,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             scoped_refptr<GbmBuffer>* buffer) {
  scoped_refptr<GbmDevice> gbm =
      static_cast<GbmDevice*>(device_manager_->GetDrmDevice(widget).get());
  DCHECK(gbm);
  *buffer = GbmBuffer::CreateBuffer(gbm, format, size, usage);
}

void DrmThread::CreateBufferFromFds(gfx::AcceleratedWidget widget,
                                    const gfx::Size& size,
                                    gfx::BufferFormat format,
                                    std::vector<base::ScopedFD>&& fds,
                                    std::vector<int> strides,
                                    std::vector<int> offsets,
                                    scoped_refptr<GbmBuffer>* buffer) {
  scoped_refptr<GbmDevice> gbm =
      static_cast<GbmDevice*>(device_manager_->GetDrmDevice(widget).get());
  DCHECK(gbm);
  *buffer = GbmBuffer::CreateBufferFromFds(gbm, format, size, std::move(fds),
                                           strides, offsets);
}

void DrmThread::GetScanoutFormats(
    gfx::AcceleratedWidget widget,
    std::vector<gfx::BufferFormat>* scanout_formats) {
  display_manager_->GetScanoutFormats(widget, scanout_formats);
}

void DrmThread::SchedulePageFlip(gfx::AcceleratedWidget widget,
                                 const std::vector<OverlayPlane>& planes,
                                 const SwapCompletionCallback& callback) {
  DrmWindow* window = screen_manager_->GetWindow(widget);
  if (window)
    window->SchedulePageFlip(planes, callback);
  else
    callback.Run(gfx::SwapResult::SWAP_ACK);
}

void DrmThread::GetVSyncParameters(
    gfx::AcceleratedWidget widget,
    const gfx::VSyncProvider::UpdateVSyncCallback& callback) {
  DrmWindow* window = screen_manager_->GetWindow(widget);
  // No need to call the callback if there isn't a window since the vsync
  // provider doesn't require the callback to be called if there isn't a vsync
  // data source.
  if (window)
    window->GetVSyncParameters(callback);
}

void DrmThread::CreateWindow(gfx::AcceleratedWidget widget) {
  std::unique_ptr<DrmWindow> window(
      new DrmWindow(widget, device_manager_.get(), screen_manager_.get()));
  window->Initialize(buffer_generator_.get());
  screen_manager_->AddWindow(widget, std::move(window));
}

void DrmThread::DestroyWindow(gfx::AcceleratedWidget widget) {
  std::unique_ptr<DrmWindow> window = screen_manager_->RemoveWindow(widget);
  window->Shutdown();
}

void DrmThread::SetWindowBounds(gfx::AcceleratedWidget widget,
                                const gfx::Rect& bounds) {
  screen_manager_->GetWindow(widget)->SetBounds(bounds);
}

void DrmThread::SetCursor(gfx::AcceleratedWidget widget,
                          const std::vector<SkBitmap>& bitmaps,
                          const gfx::Point& location,
                          int frame_delay_ms) {
  screen_manager_->GetWindow(widget)
      ->SetCursor(bitmaps, location, frame_delay_ms);
}

void DrmThread::MoveCursor(const gfx::AcceleratedWidget& widget,
                           const gfx::Point& location) {
  screen_manager_->GetWindow(widget)->MoveCursor(location);
}

void DrmThread::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays,
    const base::Callback<void(gfx::AcceleratedWidget,
                              const std::vector<OverlayCheck_Params>&)>&
        callback) {
  callback.Run(widget,
               screen_manager_->GetWindow(widget)->TestPageFlip(overlays));
}

void DrmThread::RefreshNativeDisplays(
    const base::Callback<void(const std::vector<DisplaySnapshot_Params>&)>&
        callback) {
  callback.Run(display_manager_->GetDisplays());
}

void DrmThread::ConfigureNativeDisplay(
    int64_t id,
    const DisplayMode_Params& mode,
    const gfx::Point& origin,
    const base::Callback<void(int64_t, bool)>& callback) {
  callback.Run(id, display_manager_->ConfigureDisplay(id, mode, origin));
}

void DrmThread::DisableNativeDisplay(
    int64_t id,
    const base::Callback<void(int64_t, bool)>& callback) {
  callback.Run(id, display_manager_->DisableDisplay(id));
}

void DrmThread::TakeDisplayControl(const base::Callback<void(bool)>& callback) {
  callback.Run(display_manager_->TakeDisplayControl());
}

void DrmThread::RelinquishDisplayControl(
    const base::Callback<void(bool)>& callback) {
  display_manager_->RelinquishDisplayControl();
  callback.Run(true);
}

void DrmThread::AddGraphicsDevice(const base::FilePath& path,
                                  const base::FileDescriptor& fd) {
  device_manager_->AddDrmDevice(path, fd);
}

void DrmThread::RemoveGraphicsDevice(const base::FilePath& path) {
  device_manager_->RemoveDrmDevice(path);
}

void DrmThread::GetHDCPState(
    int64_t display_id,
    const base::Callback<void(int64_t, bool, HDCPState)>& callback) {
  HDCPState state = HDCP_STATE_UNDESIRED;
  bool success = display_manager_->GetHDCPState(display_id, &state);
  callback.Run(display_id, success, state);
}

void DrmThread::SetHDCPState(
    int64_t display_id,
    HDCPState state,
    const base::Callback<void(int64_t, bool)>& callback) {
  callback.Run(display_id, display_manager_->SetHDCPState(display_id, state));
}

void DrmThread::SetColorCorrection(
    int64_t display_id,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  display_manager_->SetColorCorrection(display_id, degamma_lut, gamma_lut,
                                       correction_matrix);
}

}  // namespace ui

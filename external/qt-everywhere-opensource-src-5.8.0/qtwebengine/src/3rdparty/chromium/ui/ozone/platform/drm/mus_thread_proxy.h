// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_MUS_THREAD_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_MUS_THREAD_PROXY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/gpu/inter_thread_messaging_proxy.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ui {

class DrmCursor;
class DrmDisplayHostManager;
class DrmOverlayManager;
class DrmThread;
class GpuThreadObserver;

// In Mus, the window server thread (analogous to Chrome's UI thread), GPU and
// DRM threads coexist in a single Mus process. The |MusThreadProxy| connects
// these threads together via cross-thread calls.
class MusThreadProxy : public GpuThreadAdapter,
                       public InterThreadMessagingProxy,
                       public DrmCursorProxy {
 public:
  MusThreadProxy();
  ~MusThreadProxy() override;

  void StartDrmThread();
  void ProvideManagers(DrmDisplayHostManager* display_manager,
                       DrmOverlayManager* overlay_manager);

  // InterThreadMessagingProxy.
  void SetDrmThread(DrmThread* thread) override;

  // This is the core functionality. They are invoked when we have a main
  // thread, a gpu thread and we have called initialize on both.
  void AddGpuThreadObserver(GpuThreadObserver* observer) override;
  void RemoveGpuThreadObserver(GpuThreadObserver* observer) override;
  bool IsConnected() override;

  // Services needed for DrmDisplayHostMananger.
  void RegisterHandlerForDrmDisplayHostManager(
      DrmDisplayHostManager* handler) override;
  void UnRegisterHandlerForDrmDisplayHostManager() override;

  bool GpuTakeDisplayControl() override;
  bool GpuRefreshNativeDisplays() override;
  bool GpuRelinquishDisplayControl() override;
  bool GpuAddGraphicsDevice(const base::FilePath& path,
                            const base::FileDescriptor& fd) override;
  bool GpuRemoveGraphicsDevice(const base::FilePath& path) override;

  // Services needed for DrmOverlayManager.
  void RegisterHandlerForDrmOverlayManager(DrmOverlayManager* handler) override;
  void UnRegisterHandlerForDrmOverlayManager() override;

  bool GpuCheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlayCheck_Params>& new_params) override;

  // Services needed by DrmDisplayHost
  bool GpuConfigureNativeDisplay(int64_t display_id,
                                 const ui::DisplayMode_Params& display_mode,
                                 const gfx::Point& point) override;
  bool GpuDisableNativeDisplay(int64_t display_id) override;
  bool GpuGetHDCPState(int64_t display_id) override;
  bool GpuSetHDCPState(int64_t display_id, ui::HDCPState state) override;
  bool GpuSetColorCorrection(
      int64_t display_id,
      const std::vector<GammaRampRGBEntry>& degamma_lut,
      const std::vector<GammaRampRGBEntry>& gamma_lut,
      const std::vector<float>& correction_matrix) override;

  // Services needed by DrmWindowHost
  bool GpuDestroyWindow(gfx::AcceleratedWidget widget) override;
  bool GpuCreateWindow(gfx::AcceleratedWidget widget) override;
  bool GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                              const gfx::Rect& bounds) override;

  // DrmCursorProxy.
  void CursorSet(gfx::AcceleratedWidget window,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& point,
                 int frame_delay_ms) override;
  void Move(gfx::AcceleratedWidget window, const gfx::Point& point) override;
  void InitializeOnEvdev() override;

 private:
  void RunObservers();
  void DispatchObserversFromDrmThread();

  void GpuCheckOverlayCapabilitiesCallback(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlayCheck_Params>& overlays) const;

  void GpuConfigureNativeDisplayCallback(int64_t display_id,
                                         bool success) const;

  void GpuRefreshNativeDisplaysCallback(
      const std::vector<DisplaySnapshot_Params>& displays) const;
  void GpuDisableNativeDisplayCallback(int64_t display_id, bool success) const;
  void GpuTakeDisplayControlCallback(bool success) const;
  void GpuRelinquishDisplayControlCallback(bool success) const;
  void GpuGetHDCPStateCallback(int64_t display_id,
                               bool success,
                               HDCPState state) const;
  void GpuSetHDCPStateCallback(int64_t display_id, bool success) const;

  scoped_refptr<base::SingleThreadTaskRunner> ws_task_runner_;

  DrmThread* drm_thread_;  // Not owned.

  // Guards  for multi-theaded access to drm_thread_.
  base::Lock lock_;

  DrmDisplayHostManager* display_manager_;  // Not owned.
  DrmOverlayManager* overlay_manager_;      // Not owned.

  base::ObserverList<GpuThreadObserver> gpu_thread_observers_;

  base::ThreadChecker on_window_server_thread_;

  base::WeakPtrFactory<MusThreadProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MusThreadProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_MUS_THREAD_PROXY_H_

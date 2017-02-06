// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

#include <stdint.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace base {
struct FileDescriptor;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {

class DrmDeviceManager;
class DrmGpuDisplayManager;
class DrmWindow;
class DrmWindowProxy;
class GbmBuffer;
class ScanoutBufferGenerator;
class ScreenManager;

struct GammaRampRGBEntry;
struct OverlayPlane;

// Holds all the DRM related state and performs all DRM related operations.
//
// The DRM thread is used to insulate DRM operations from potential blocking
// behaviour on the GPU main thread in order to reduce the potential for jank
// (for example jank in the cursor if the GPU main thread is performing heavy
// operations). The inverse is also true as blocking operations on the DRM
// thread (such as modesetting) no longer block the GPU main thread.
class DrmThread : public base::Thread {
 public:
  DrmThread();
  ~DrmThread() override;

  void Start();

  // Must be called on the DRM thread.
  void CreateBuffer(gfx::AcceleratedWidget widget,
                    const gfx::Size& size,
                    gfx::BufferFormat format,
                    gfx::BufferUsage usage,
                    scoped_refptr<GbmBuffer>* buffer);
  void CreateBufferFromFds(gfx::AcceleratedWidget widget,
                           const gfx::Size& size,
                           gfx::BufferFormat format,
                           std::vector<base::ScopedFD>&& fds,
                           std::vector<int> strides,
                           std::vector<int> offsets,
                           scoped_refptr<GbmBuffer>* buffer);

  void GetScanoutFormats(gfx::AcceleratedWidget widget,
                         std::vector<gfx::BufferFormat>* scanout_formats);
  void SchedulePageFlip(gfx::AcceleratedWidget widget,
                        const std::vector<OverlayPlane>& planes,
                        const SwapCompletionCallback& callback);
  void GetVSyncParameters(
      gfx::AcceleratedWidget widget,
      const gfx::VSyncProvider::UpdateVSyncCallback& callback);

  void CreateWindow(gfx::AcceleratedWidget widget);
  void DestroyWindow(gfx::AcceleratedWidget widget);
  void SetWindowBounds(gfx::AcceleratedWidget widget, const gfx::Rect& bounds);
  void SetCursor(gfx::AcceleratedWidget widget,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& location,
                 int frame_delay_ms);
  void MoveCursor(const gfx::AcceleratedWidget& widget,
                  const gfx::Point& location);
  void CheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlayCheck_Params>& overlays,
      const base::Callback<void(gfx::AcceleratedWidget,
                                const std::vector<OverlayCheck_Params>&)>&
          callback);
  void RefreshNativeDisplays(
      const base::Callback<void(const std::vector<DisplaySnapshot_Params>&)>&
          callback);
  void ConfigureNativeDisplay(
      int64_t id,
      const DisplayMode_Params& mode,
      const gfx::Point& origin,
      const base::Callback<void(int64_t, bool)>& callback);
  void DisableNativeDisplay(
      int64_t id,
      const base::Callback<void(int64_t, bool)>& callback);
  void TakeDisplayControl(const base::Callback<void(bool)>& callback);
  void RelinquishDisplayControl(const base::Callback<void(bool)>& callback);
  void AddGraphicsDevice(const base::FilePath& path,
                         const base::FileDescriptor& fd);
  void RemoveGraphicsDevice(const base::FilePath& path);
  void GetHDCPState(
      int64_t display_id,
      const base::Callback<void(int64_t, bool, HDCPState)>& callback);
  void SetHDCPState(int64_t display_id,
                    HDCPState state,
                    const base::Callback<void(int64_t, bool)>& callback);
  void SetColorCorrection(int64_t display_id,
                          const std::vector<GammaRampRGBEntry>& degamma_lut,
                          const std::vector<GammaRampRGBEntry>& gamma_lut,
                          const std::vector<float>& correction_matrix);

  // base::Thread:
  void Init() override;

 private:
  std::unique_ptr<DrmDeviceManager> device_manager_;
  std::unique_ptr<ScanoutBufferGenerator> buffer_generator_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmGpuDisplayManager> display_manager_;

  DISALLOW_COPY_AND_ASSIGN(DrmThread);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

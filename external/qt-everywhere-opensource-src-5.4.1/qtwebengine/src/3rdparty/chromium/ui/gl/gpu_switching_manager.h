// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GPU_SWITCHING_MANAGER_H_
#define UI_GL_GPU_SWITCHING_MANAGER_H_

#include <vector>

#include "base/memory/singleton.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_preference.h"

#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#endif  // OS_MACOSX

namespace ui {

class GL_EXPORT GpuSwitchingManager {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuSwitchingManager* GetInstance();

  // Set the switching option to PreferIntegratedGpu.
  void ForceUseOfIntegratedGpu();
  // Set the switching option to PreferDiscreteGpu; switch to discrete GPU
  // immediately on Mac where dual GPU switching is supported.
  void ForceUseOfDiscreteGpu();

  // If no GPU is forced, return the original GpuPreference; otherwise, return
  // the forced GPU.
  gfx::GpuPreference AdjustGpuPreference(gfx::GpuPreference gpu_preference);

  // In the browser process, the value for this flag is computed the first time
  // this function is called.
  // In the GPU process, the value is passed from the browser process using the
  // --supports-dual-gpus commandline switch.
  bool SupportsDualGpus();

  void SetGpuCount(size_t gpu_count);

 private:
  friend struct DefaultSingletonTraits<GpuSwitchingManager>;

  GpuSwitchingManager();
  virtual ~GpuSwitchingManager();

#if defined(OS_MACOSX)
  void SwitchToDiscreteGpuMac();

  CGLPixelFormatObj discrete_pixel_format_;
#endif  // OS_MACOSX

  gfx::GpuPreference gpu_switching_option_;
  bool gpu_switching_option_set_;

  bool supports_dual_gpus_;
  bool supports_dual_gpus_set_;

  size_t gpu_count_;

  DISALLOW_COPY_AND_ASSIGN(GpuSwitchingManager);
};

}  // namespace ui

#endif  // UI_GL_GPU_SWITCHING_MANAGER_H_

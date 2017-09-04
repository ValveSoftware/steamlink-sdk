// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_CHILD_WINDOW_SURFACE_WIN_H_
#define GPU_IPC_SERVICE_CHILD_WINDOW_SURFACE_WIN_H_

#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gl/gl_surface_egl.h"

#include <windows.h>

namespace gpu {

class GpuChannelManager;
struct SharedData;

class ChildWindowSurfaceWin : public gl::NativeViewGLSurfaceEGL {
 public:
  ChildWindowSurfaceWin(base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
                        HWND parent_window);

  // GLSurface implementation.
  EGLConfig GetConfig() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  bool InitializeNativeWindow() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;

 protected:
  ~ChildWindowSurfaceWin() override;

 private:
  void ClearInvalidContents();

  // This member contains all the data that can be accessed from the main or
  // window owner threads.
  std::unique_ptr<SharedData> shared_data_;
  // The eventual parent of the window living in the browser process.
  HWND parent_window_;
  // The window is initially created with this parent window. We need to keep it
  // around so that we can destroy it at the end.
  HWND initial_parent_window_;
  base::WeakPtr<ImageTransportSurfaceDelegate> delegate_;
  bool alpha_;
  bool first_swap_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_CHILD_WINDOW_SURFACE_WIN_H_

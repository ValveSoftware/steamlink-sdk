// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_WIN_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_WIN_H_

#include <stddef.h>
#include <windows.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "cc/output/software_output_device.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace base {
class SharedMemory;
}

namespace ui {
class Compositor;
}

namespace content {
class SoftwareOutputDeviceWin;

class OutputDeviceBacking {
 public:
  OutputDeviceBacking();
  ~OutputDeviceBacking();

  void Resized();
  void RegisterOutputDevice(SoftwareOutputDeviceWin* device);
  void UnregisterOutputDevice(SoftwareOutputDeviceWin* device);
  base::SharedMemory* GetSharedMemory(const gfx::Size& size);

 private:
  size_t GetMaxByteSize();

  std::vector<SoftwareOutputDeviceWin*> devices_;
  std::unique_ptr<base::SharedMemory> backing_;
  size_t created_byte_size_;

  DISALLOW_COPY_AND_ASSIGN(OutputDeviceBacking);
};

class SoftwareOutputDeviceWin : public cc::SoftwareOutputDevice {
 public:
  SoftwareOutputDeviceWin(OutputDeviceBacking* backing,
                          ui::Compositor* compositor);
  ~SoftwareOutputDeviceWin() override;

  void Resize(const gfx::Size& viewport_pixel_size,
              float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;

  gfx::Size viewport_pixel_size() const { return viewport_pixel_size_; }
  void ReleaseContents();

 private:
  HWND hwnd_;
  sk_sp<SkCanvas> contents_;
  bool is_hwnd_composited_;
  OutputDeviceBacking* backing_;
  bool in_paint_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_WIN_H_

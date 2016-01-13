// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_SCANOUT_SURFACE_H_
#define UI_OZONE_PLATFORM_DRI_SCANOUT_SURFACE_H_

#include <stdint.h>

namespace gfx {
class Size;
}

namespace ui {

// ScanoutSurface is an interface for a surface that can be scanned out to a
// monitor using the DRM/KMS API. Implementations will store the internal state
// associated with the drawing surface. Implementations are also required to
// performs all the needed operations to initialize and update the drawing
// surface.
//
// The typical usage pattern is:
// -----------------------------------------------------------------------------
// HardwareDisplayController controller;
// // Initialize controller
//
// ScanoutSurface* surface = new ScanoutSurfaceImpl(size);
// surface.Initialize();
// controller.BindSurfaceToController(surface);
//
// while (true) {
//   DrawIntoSurface(surface);
//   controller.SchedulePageFlip();
//
//   Wait for page flip event. The DRM page flip handler will call
//   surface.SwapBuffers();
// }
//
// delete surface;
// -----------------------------------------------------------------------------
// In the above example the wait consists of reading a DRM pageflip event from
// the graphics card file descriptor. This is done by calling |drmHandleEvent|,
// which will read and process the event. |drmHandleEvent| will call a callback
// registered by |SchedulePageFlip| which will update the internal state.
//
// |SchedulePageFlip| can also be used to limit drawing to the screen's vsync
// since page flips only happen on vsync. In a threaded environment a message
// loop would listen on the graphics card file descriptor for an event and
// |drmHandleEvent| would be called from the message loop. The event handler
// would also be responsible for updating the renderer's state and signal that
// it is OK to start drawing the next frame.
class ScanoutSurface {
 public:
  virtual ~ScanoutSurface() {}

  // Used to allocate all necessary buffers for this surface. If the
  // initialization succeeds, the device is ready to be used for drawing
  // operations.
  // Returns true if the initialization is successful, false otherwise.
  virtual bool Initialize() = 0;

  // Swaps the back buffer with the front buffer.
  virtual void SwapBuffers() = 0;

  // Returns the ID of the current backbuffer.
  virtual uint32_t GetFramebufferId() const = 0;

  // Returns the handle of the current backbuffer.
  virtual uint32_t GetHandle() const = 0;

  // Returns the surface size.
  virtual gfx::Size Size() const = 0;
};

class ScanoutSurfaceGenerator {
 public:
  virtual ~ScanoutSurfaceGenerator() {}

  virtual ScanoutSurface* Create(const gfx::Size& size) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_SCANOUT_SURFACE_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/ozone_base_export.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/native_pixmap.h"

namespace ui {

class NativePixmap;
class SurfaceOzoneCanvas;

// The Ozone interface allows external implementations to hook into Chromium to
// provide a system specific implementation. The Ozone interface supports two
// drawing modes: 1) accelerated drawing using GL and 2) software drawing
// through Skia.
//
// If you want to paint on a window with ozone, you need to create a GLSurface
// or SurfaceOzoneCanvas for that window. The platform can support software, GL,
// or both for painting on the window. The following functionality is specific
// to the drawing mode and may not have any meaningful implementation in the
// other mode. An implementation must provide functionality for at least one
// mode.
//
// 1) Accelerated Drawing (GL path):
//
// The following functions are specific to GL:
//  - GetAllowedGLImplementations
//  - GetGLOzone (along with the associated GLOzone)
//
// 2) Software Drawing (Skia):
//
// The following function is specific to the software path:
//  - CreateCanvasForWidget
//
// The accelerated path can optionally provide support for the software drawing
// path.
//
// The remaining functions are not covered since they are needed in both drawing
// modes (See comments below for descriptions).
class OZONE_BASE_EXPORT SurfaceFactoryOzone {
 public:
  // Returns a list of allowed GL implementations. The default implementation
  // will be the first item.
  virtual std::vector<gl::GLImplementation> GetAllowedGLImplementations();

  // Returns the GLOzone to use for the specified GL implementation, or null if
  // GL implementation doesn't exist.
  virtual GLOzone* GetGLOzone(gl::GLImplementation implementation);

  // DEPRECATED(kylechar): Implement GLOzoneEGL instead.
  virtual intptr_t GetNativeDisplay();

  // DEPRECATED(kylechar): Implement GLOzone instead.
  virtual scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLImplementation implementation,
      gfx::AcceleratedWidget widget);

  // DEPRECATED(kylechar): Implement GLOzone instead.
  virtual scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gl::GLImplementation implementation,
      gfx::AcceleratedWidget widget);

  // DEPRECATED(kylechar): Implement GLOzone instead.
  virtual scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLImplementation implementation,
      const gfx::Size& size);

  // Create SurfaceOzoneCanvas for the specified gfx::AcceleratedWidget.
  //
  // Note: The platform must support creation of SurfaceOzoneCanvas from the
  // Browser Process using only the handle contained in gfx::AcceleratedWidget.
  virtual std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget);

  // DEPRECATED(kylechar): Implement GLOzoneEGL instead.
  virtual bool LoadEGLGLES2Bindings();

  // Returns all scanout formats for |widget| representing a particular display
  // controller or default display controller for kNullAcceleratedWidget.
  virtual std::vector<gfx::BufferFormat> GetScanoutFormats(
      gfx::AcceleratedWidget widget);

  // Create a single native buffer to be used for overlay planes or zero copy
  // for |widget| representing a particular display controller or default
  // display controller for kNullAcceleratedWidget.
  // It can be called on any thread.
  virtual scoped_refptr<NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  // Create a single native buffer from an existing handle. Takes ownership of
  // |handle| and can be called on any thread.
  virtual scoped_refptr<NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      const gfx::NativePixmapHandle& handle);

 protected:
  SurfaceFactoryOzone();
  virtual ~SurfaceFactoryOzone();

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceFactoryOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_

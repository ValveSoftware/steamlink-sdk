// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_SURFACE_OZONE_H_
#define UI_GL_INIT_GL_SURFACE_OZONE_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"

namespace gl {

// TODO(kylechar): These functions, plus the corresponding GLSurface classes
// have been moved into //ui/gl/init as a temporary step to break the dependency
// from //ui/gl to //ui/ozone. Once that dependency is broken the Ozone
// GLSurface classes should be made into base classes for each Ozone platform to
// override and moved back to //ui/gl.

scoped_refptr<GLSurface> CreateViewGLSurfaceOzone(
    gfx::AcceleratedWidget window);

scoped_refptr<GLSurface> CreateViewGLSurfaceOzoneSurfaceless(
    gfx::AcceleratedWidget window);

scoped_refptr<GLSurface> CreateViewGLSurfaceOzoneSurfacelessSurfaceImpl(
    gfx::AcceleratedWidget window);

}  // namespace gl

#endif  // UI_GL_INIT_GL_SURFACE_OZONE_H_

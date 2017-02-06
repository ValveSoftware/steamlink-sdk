// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_FACTORY_H_
#define UI_GL_INIT_GL_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/init/gl_init_export.h"

namespace gl {

class GLContext;
class GLShareGroup;
class GLSurface;

namespace init {

// Initialize GL bindings.
GL_INIT_EXPORT bool InitializeGLOneOff();

// Initialize GL bindings using the provided parameters. This might be required
// for use in tests, otherwise use InitializeGLOneOff() instead.
GL_INIT_EXPORT bool InitializeGLOneOffImplementation(GLImplementation impl,
                                                     bool fallback_to_osmesa,
                                                     bool gpu_service_logging,
                                                     bool disable_gl_drawing);

// Create a GL context that is compatible with the given surface. |share_group|,
// if non-NULL, is a group of contexts which the internally created OpenGL
// context shares textures and other resources.
GL_INIT_EXPORT scoped_refptr<GLContext> CreateGLContext(
    GLShareGroup* share_group,
    GLSurface* compatible_surface,
    GpuPreference gpu_preference);

// Create a GL surface that renders directly to a view.
GL_INIT_EXPORT scoped_refptr<GLSurface> CreateViewGLSurface(
    gfx::AcceleratedWidget window);

#if defined(USE_OZONE)
// Create a GL surface that renders directly into a window with surfaceless
// semantics - there is no default framebuffer and the primary surface must
// be presented as an overlay. If surfaceless mode is not supported or
// enabled it will return a null pointer.
GL_INIT_EXPORT scoped_refptr<GLSurface> CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window);
#endif  // defined(USE_OZONE)

// Create a GL surface used for offscreen rendering.
GL_INIT_EXPORT scoped_refptr<GLSurface> CreateOffscreenGLSurface(
    const gfx::Size& size);

}  // namespace init
}  // namespace gl

#endif  // UI_GL_INIT_GL_FACTORY_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation_osmesa.h"
#include "ui/gl/gl_osmesa_api_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/ozone_util.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {
namespace init {

namespace {

bool InitializeStaticEGLInternal() {
  if (!GetSurfaceFactoryOzone()->LoadEGLGLES2Bindings())
    return false;

  SetGLImplementation(kGLImplementationEGLGLES2);
  InitializeStaticGLBindingsGL();
  InitializeStaticGLBindingsEGL();

  return true;
}

}  // namespace

#if !defined(TOOLKIT_QT)
bool InitializeGLOneOffPlatform() {
  if (HasGLOzone())
    return GetGLOzone()->InitializeGLOneOffPlatform();

  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff(
              GetSurfaceFactoryOzone()->GetNativeDisplay())) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      return true;
    case kGLImplementationOSMesaGL:
    case kGLImplementationMockGL:
      return true;
    default:
      return false;
  }
}
#endif

bool InitializeStaticGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  if (HasGLOzone(implementation)) {
    return GetGLOzone(implementation)
        ->InitializeStaticGLBindings(implementation);
  }

  switch (implementation) {
    case kGLImplementationOSMesaGL:
      return InitializeStaticGLBindingsOSMesaGL();
    case kGLImplementationEGLGLES2:
      return InitializeStaticEGLInternal();
    case kGLImplementationMockGL:
      SetGLImplementation(kGLImplementationMockGL);
      InitializeStaticGLBindingsGL();
      return true;
    default:
      NOTREACHED();
  }

  return false;
}

void InitializeDebugGLBindings() {
  if (HasGLOzone()) {
    GetGLOzone()->InitializeDebugGLBindings();
    return;
  }

  InitializeDebugGLBindingsEGL();
  InitializeDebugGLBindingsGL();
  InitializeDebugGLBindingsOSMESA();
}

void ClearGLBindingsPlatform() {
  if (HasGLOzone()) {
    GetGLOzone()->ClearGLBindings();
    return;
  }

  ClearGLBindingsEGL();
  ClearGLBindingsGL();
  ClearGLBindingsOSMESA();
}

}  // namespace init
}  // namespace gl

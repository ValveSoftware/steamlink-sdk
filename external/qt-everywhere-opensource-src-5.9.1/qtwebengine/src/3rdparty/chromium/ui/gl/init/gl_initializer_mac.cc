// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include <OpenGL/CGLRenderers.h>

#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_osmesa_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_manager.h"

namespace gl {
namespace init {

namespace {

const char kOpenGLFrameworkPath[] =
    "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL";

bool InitializeOneOffForSandbox() {
  static bool initialized = false;
  if (initialized)
    return true;

  // This is called from the sandbox warmup code on Mac OS X.
  // GPU-related stuff is very slow without this, probably because
  // the sandbox prevents loading graphics drivers or some such.
  std::vector<CGLPixelFormatAttribute> attribs;
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    // Avoid switching to the discrete GPU just for this pixel
    // format selection.
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
  }
  if (GetGLImplementation() == kGLImplementationAppleGL) {
    attribs.push_back(kCGLPFARendererID);
    attribs.push_back(
        static_cast<CGLPixelFormatAttribute>(kCGLRendererGenericFloatID));
  }
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));

  CGLPixelFormatObj format;
  GLint num_pixel_formats;
  if (CGLChoosePixelFormat(&attribs.front(), &format, &num_pixel_formats) !=
      kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return false;
  }
  if (!format) {
    LOG(ERROR) << "format == 0.";
    return false;
  }
  CGLReleasePixelFormat(format);
  DCHECK_NE(num_pixel_formats, 0);
  initialized = true;
  return true;
}

bool InitializeStaticOSMesaInternal() {
  // osmesa.so is located in the build directory. This code path is only
  // valid in a developer build environment.
  base::FilePath exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(ERROR) << "PathService::Get failed.";
    return false;
  }
  base::FilePath bundle_path = base::mac::GetAppBundlePath(exe_path);
  // Some unit test targets depend on osmesa but aren't built as app
  // bundles. In that case, the .so is next to the executable.
  if (bundle_path.empty())
    bundle_path = exe_path;
  base::FilePath build_dir_path = bundle_path.DirName();
  base::FilePath osmesa_path = build_dir_path.Append("osmesa.so");

  // When using OSMesa, just use OSMesaGetProcAddress to find entry points.
  base::NativeLibrary library = base::LoadNativeLibrary(osmesa_path, NULL);
  if (!library) {
    LOG(ERROR) << "osmesa.so not found at " << osmesa_path.value();
    return false;
  }

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "OSMesaGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "OSMesaGetProcAddress not found.";
    base::UnloadNativeLibrary(library);
    return false;
  }

  SetGLGetProcAddressProc(get_proc_address);
  AddGLNativeLibrary(library);
  SetGLImplementation(kGLImplementationOSMesaGL);

  InitializeStaticGLBindingsGL();
  InitializeStaticGLBindingsOSMESA();

  return true;
}

bool InitializeStaticCGLInternal(GLImplementation implementation) {
  base::NativeLibrary library =
      base::LoadNativeLibrary(base::FilePath(kOpenGLFrameworkPath), nullptr);
  if (!library) {
    LOG(ERROR) << "OpenGL framework not found";
    return false;
  }

  AddGLNativeLibrary(library);
  SetGLImplementation(implementation);

  InitializeStaticGLBindingsGL();
  return true;
}

}  // namespace

bool InitializeGLOneOffPlatform() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      if (!InitializeOneOffForSandbox()) {
        LOG(ERROR) << "GLSurfaceCGL::InitializeOneOff failed.";
        return false;
      }
      return true;
    default:
      return true;
  }
}

bool InitializeStaticGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  // Allow the main thread or another to initialize these bindings
  // after instituting restrictions on I/O. Going forward they will
  // likely be used in the browser process on most platforms. The
  // one-time initialization cost is small, between 2 and 5 ms.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  switch (implementation) {
    case kGLImplementationOSMesaGL:
      return InitializeStaticOSMesaInternal();
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      return InitializeStaticCGLInternal(implementation);
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
  InitializeDebugGLBindingsGL();
  InitializeDebugGLBindingsOSMESA();
}

void ClearGLBindingsPlatform() {
  ClearGLBindingsGL();
  ClearGLBindingsOSMESA();
}

}  // namespace init
}  // namespace gl
